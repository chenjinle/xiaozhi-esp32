#include "dual_network_board.h"
#include "codecs/no_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "peripheral_controls.h"
#include "led/single_led.h"
#include "esp32_camera.h"
#include "mcp_server.h"
#include "uvc_camera.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

#include <cJSON.h>
#include <driver/i2c_master.h>
#include <driver/spi_master.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_st7796.h>
#include <esp_lcd_touch_ft5x06.h>
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <esp_timer.h>
#include <esp_vfs_fat.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mbedtls/base64.h>
#include <mbedtls/md.h>
#include <sdmmc_cmd.h>
#include <driver/sdmmc_host.h>

#define TAG "YseEsp32s3Hmi"

extern const uint8_t _binary_idle_gif_start[];
extern const uint8_t _binary_idle_gif_end[];

class YseEsp32s3Hmi : public DualNetworkBoard {
private:
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    Button boot_button_;
    YsePeripheralControls peripheral_controls_;
    LcdDisplay* display_ = nullptr;
    Esp32Camera* camera_ = nullptr;
    YseUvcCamera* uvc_camera_ = nullptr;

    // 顶部信息栏状态
    TaskHandle_t info_panel_task_ = nullptr;
    float info_temp_ = 0.0f;
    float info_humidity_ = 0.0f;
    bool info_dht_valid_ = false;
    std::string weather_text_;
    bool weather_ok_ = false;

    // 请求指定 location 的天气，成功返回 true 并更新 weather_text_
    bool FetchWeatherOnce(const char* location) {
        time_t ts = time(nullptr);
        if (ts < 1600000000) return false;  // 尚未校时

        char params[256];
        snprintf(params, sizeof(params),
                 "language=zh-Hans&location=%s&ts=%lld&uid=%s&unit=c",
                 location, (long long)ts, WEATHER_PUBLIC_KEY);

        // HMAC-SHA1(私钥, params) -> Base64
        uint8_t digest[20];
        mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA1),
                        reinterpret_cast<const uint8_t*>(WEATHER_PRIVATE_KEY), strlen(WEATHER_PRIVATE_KEY),
                        reinterpret_cast<const uint8_t*>(params), strlen(params),
                        digest);
        size_t b64_len = 0;
        uint8_t b64[64] = {};
        mbedtls_base64_encode(b64, sizeof(b64), &b64_len, digest, sizeof(digest));
        std::string sig(reinterpret_cast<char*>(b64), b64_len);

        // URL 编码 sig
        static const char* hex = "0123456789ABCDEF";
        std::string sig_encoded;
        for (char c : sig) {
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                c == '-' || c == '_' || c == '.' || c == '~') {
                sig_encoded += c;
            } else {
                sig_encoded += '%';
                sig_encoded += hex[(c >> 4) & 0xF];
                sig_encoded += hex[c & 0xF];
            }
        }

        char url[512];
        snprintf(url, sizeof(url), "https://api.seniverse.com/v3/weather/now.json?%s&sig=%s",
                 params, sig_encoded.c_str());

        esp_http_client_config_t cfg = {};
        cfg.url = url;
        cfg.timeout_ms = 10000;
        cfg.buffer_size = 1024;
        cfg.crt_bundle_attach = esp_crt_bundle_attach;

        esp_http_client_handle_t client = esp_http_client_init(&cfg);
        if (client == nullptr) return false;

        bool ok = false;
        esp_err_t err = esp_http_client_open(client, 0);
        if (err == ESP_OK && esp_http_client_fetch_headers(client) >= 0) {
            std::string response;
            char buf[256];
            int read;
            while ((read = esp_http_client_read(client, buf, sizeof(buf) - 1)) > 0) {
                buf[read] = '\0';
                response += buf;
                if (response.size() > 4096) break;
            }

            cJSON* root = cJSON_Parse(response.c_str());
            if (root != nullptr) {
                cJSON* results = cJSON_GetObjectItem(root, "results");
                if (results != nullptr && cJSON_GetArraySize(results) > 0) {
                    cJSON* now = cJSON_GetObjectItem(cJSON_GetArrayItem(results, 0), "now");
                    if (now != nullptr) {
                        cJSON* text = cJSON_GetObjectItem(now, "text");
                        cJSON* temp = cJSON_GetObjectItem(now, "temperature");
                        if (cJSON_IsString(text) && cJSON_IsString(temp)) {
                            weather_text_ = std::string(text->valuestring) + " " + temp->valuestring + "C";
                            ok = true;
                        }
                    }
                }
                cJSON_Delete(root);
            }
        }
        esp_http_client_close(client);
        esp_http_client_cleanup(client);

        if (ok) {
            ESP_LOGI(TAG, "Weather updated (location=%s): %s", location, weather_text_.c_str());
        } else {
            ESP_LOGW(TAG, "Weather query failed (location=%s)", location);
        }
        return ok;
    }

    void FetchWeather() {
#if WEATHER_ENABLED
        // 优先按设备出口 IP 自动定位；失败则用默认城市兜底
        bool ok = FetchWeatherOnce(WEATHER_CITY);
        if (!ok) {
            ok = FetchWeatherOnce(WEATHER_FALLBACK_CITY);
        }
        weather_ok_ = ok;
#endif
    }

    void InfoPanelTask() {
        int dht_counter = 29;  // 首次立即读温湿度
        int64_t last_weather_ms = 0;

        while (true) {
            time_t now = time(nullptr);
            struct tm tm_info;
            localtime_r(&now, &tm_info);

            char time_str[64];
            if (now < 1600000000) {  // 服务器尚未校时
                snprintf(time_str, sizeof(time_str), "--:--");
            } else {
                snprintf(time_str, sizeof(time_str), "%02d-%02d %02d:%02d",
                         tm_info.tm_mon + 1, tm_info.tm_mday, tm_info.tm_hour, tm_info.tm_min);
            }

            if (++dht_counter >= 30) {  // 每 30 秒读一次 DHT11
                dht_counter = 0;
                float t = 0.0f, h = 0.0f;
                info_dht_valid_ = peripheral_controls_.ReadTemperatureHumidity(t, h);
                if (info_dht_valid_) {
                    info_temp_ = t;
                    info_humidity_ = h;
                }
            }

            int64_t now_ms = esp_timer_get_time() / 1000;
            // 天气查询失败/未校时时每分钟重试一次，成功后按 30 分钟刷新
            int64_t weather_interval = weather_ok_ ? (int64_t)WEATHER_UPDATE_INTERVAL_MS : 60000;
            if (now_ms - last_weather_ms >= weather_interval) {
                last_weather_ms = now_ms;
                FetchWeather();
            }

            char th_str[32] = "";
            if (info_dht_valid_) {
                snprintf(th_str, sizeof(th_str), "%.1fC %.0f%%", info_temp_, info_humidity_);
            }

            if (display_ != nullptr) {
                display_->SetInfoPanel(time_str, th_str, weather_text_.c_str());
            }

            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    void RegisterUvcCameraTools() {
        auto& mcp = McpServer::GetInstance();

        mcp.AddTool("self.camera.start_preview",
            "Turn on the USB UVC camera and show the live view on the LCD screen. "
            "Note: while the camera is on, the USB console is unavailable until reboot.",
            PropertyList(), [this](const PropertyList&) -> ReturnValue {
                if (uvc_camera_ == nullptr) {
                    return std::string("camera not initialized");
                }
                esp_err_t err = uvc_camera_->Start();
                if (err != ESP_OK) {
                    cJSON* result = cJSON_CreateObject();
                    cJSON_AddBoolToObject(result, "success", false);
                    cJSON_AddStringToObject(result, "error", esp_err_to_name(err));
                    if (err == ESP_ERR_NOT_FOUND) {
                        cJSON_AddStringToObject(result, "hint",
                            "USB上没有检测到摄像头。请检查：1 摄像头供电是否足够，建议外部5V供电；"
                            "2 OTG转接线是否接好；3 摄像头是否支持UVC协议。");
                    }
                    return result;
                }
                cJSON* result = cJSON_CreateObject();
                cJSON_AddBoolToObject(result, "success", true);
                return result;
            });

        mcp.AddTool("self.camera.stop_preview",
            "Turn off the USB UVC camera and stop the live view on the LCD screen.",
            PropertyList(), [this](const PropertyList&) -> ReturnValue {
                if (uvc_camera_ != nullptr) {
                    uvc_camera_->Stop();
                }
                return true;
            });
    }

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_PORT,
            .sda_io_num = I2C_SDA_PIN,
            .scl_io_num = I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void InitializeDisplay() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_CLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(DISPLAY_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(DISPLAY_SPI_HOST, &io_config, &panel_io));

        esp_lcd_panel_handle_t panel = nullptr;
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7796(panel_io, &panel_config, &panel));

        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

        display_ = new SpiLcdDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                     DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
                                     DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
        display_->SetIdleAnimation(_binary_idle_gif_start,
                                   static_cast<size_t>(_binary_idle_gif_end - _binary_idle_gif_start));
    }

    void InitializeTouch() {
        esp_lcd_touch_config_t tp_cfg = {
            .x_max = DISPLAY_WIDTH,
            .y_max = DISPLAY_HEIGHT,
            .rst_gpio_num = TOUCH_RST_PIN,
            .int_gpio_num = TOUCH_INT_PIN,
            .levels = {
                .reset = 0,
                .interrupt = 0,
            },
            .flags = {
                .swap_xy = TOUCH_SWAP_XY,
                .mirror_x = TOUCH_MIRROR_X,
                .mirror_y = TOUCH_MIRROR_Y,
            },
        };

        esp_lcd_panel_io_handle_t tp_io_handle = nullptr;
        esp_lcd_panel_io_i2c_config_t tp_io_config = {};
        tp_io_config.dev_addr = ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS;
        tp_io_config.control_phase_bytes = 1;
        tp_io_config.dc_bit_offset = 0;
        tp_io_config.lcd_cmd_bits = 8;
        tp_io_config.flags.disable_control_phase = 1;
        tp_io_config.scl_speed_hz = 400 * 1000;

        esp_err_t ret = esp_lcd_new_panel_io_i2c(i2c_bus_, &tp_io_config, &tp_io_handle);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to create touch I2C IO: %s", esp_err_to_name(ret));
            return;
        }

        esp_lcd_touch_handle_t tp = nullptr;
        ret = esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, &tp);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to initialize FT5x06 touch: %s", esp_err_to_name(ret));
            return;
        }

        lvgl_port_touch_cfg_t touch_cfg = {};
        touch_cfg.disp = lv_display_get_default();
        touch_cfg.handle = tp;
        lvgl_port_add_touch(&touch_cfg);
        ESP_LOGI(TAG, "Touch panel initialized");
    }

    void InitializeSdCard() {
#if SDCARD_SDMMC_ENABLED
        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
        slot_config.clk = SDCARD_SDMMC_CLK_PIN;
        slot_config.cmd = SDCARD_SDMMC_CMD_PIN;
        slot_config.d0 = SDCARD_SDMMC_D0_PIN;
        slot_config.width = SDCARD_SDMMC_BUS_WIDTH;
        slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

        esp_vfs_fat_sdmmc_mount_config_t mount_config = {
            .format_if_mount_failed = false,
            .max_files = 5,
            .allocation_unit_size = 0,
            .disk_status_check_enable = true,
        };

        sdmmc_card_t* card = nullptr;
        esp_err_t ret = esp_vfs_fat_sdmmc_mount(SDCARD_MOUNT_POINT, &host, &slot_config, &mount_config, &card);
        if (ret == ESP_OK) {
            sdmmc_card_print_info(stdout, card);
            ESP_LOGI(TAG, "SD card mounted at %s (SDMMC 1-bit)", SDCARD_MOUNT_POINT);
        } else {
            ESP_LOGW(TAG, "Failed to mount SD card (SDMMC 1-bit): %s", esp_err_to_name(ret));
        }
#endif
    }

    void InitializeCamera() {
        camera_config_t config = {};
        config.pin_d0 = CAM_PIN_D0;
        config.pin_d1 = CAM_PIN_D1;
        config.pin_d2 = CAM_PIN_D2;
        config.pin_d3 = CAM_PIN_D3;
        config.pin_d4 = CAM_PIN_D4;
        config.pin_d5 = CAM_PIN_D5;
        config.pin_d6 = CAM_PIN_D6;
        config.pin_d7 = CAM_PIN_D7;
        config.pin_xclk = CAM_PIN_XCLK;
        config.pin_pclk = CAM_PIN_PCLK;
        config.pin_vsync = CAM_PIN_VSYNC;
        config.pin_href = CAM_PIN_HREF;
        config.pin_sccb_sda = -1;
        config.pin_sccb_scl = -1;
        config.sccb_i2c_port = I2C_PORT;
        config.pin_pwdn = CAM_PIN_PWDN;
        config.pin_reset = CAM_PIN_RESET;
        config.xclk_freq_hz = CAM_XCLK_FREQ_HZ;
        config.pixel_format = PIXFORMAT_RGB565;
        config.frame_size = FRAMESIZE_QVGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_PSRAM;
        config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

        camera_ = new Esp32Camera(config);
        camera_->SetHMirror(false);
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (GetNetworkType() == NetworkType::WIFI) {
                if (app.GetDeviceState() == kDeviceStateStarting) {
                    auto& wifi_board = static_cast<WifiBoard&>(GetCurrentBoard());
                    wifi_board.EnterWifiConfigMode();
                    return;
                }
            }
            app.ToggleChatState();
        });
        boot_button_.OnDoubleClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting || app.GetDeviceState() == kDeviceStateWifiConfiguring) {
                SwitchNetworkType();
            }
        });
    }

public:
    YseEsp32s3Hmi() : DualNetworkBoard(ML307_TX_PIN, ML307_RX_PIN, GPIO_NUM_NC, 0), boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeDisplay();
        InitializeTouch();
        InitializeSdCard();
        InitializeButtons();
        peripheral_controls_.Initialize();
        uvc_camera_ = new YseUvcCamera(display_);
        RegisterUvcCameraTools();
        xTaskCreate([](void* arg) {
            static_cast<YseEsp32s3Hmi*>(arg)->InfoPanelTask();
        }, "info_panel", 10240, this, 5, &info_panel_task_);  // 栈加大：HTTPS/TLS 握手需要较大栈
        // InitializeCamera();  // 暂时注释：SCCB与触摸共用I2C_NUM_1导致冲突
        if (GetBacklight() != nullptr) {
            GetBacklight()->RestoreBrightness();
        }
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK,
            AUDIO_I2S_SPK_GPIO_LRCK,
            AUDIO_I2S_SPK_GPIO_DOUT,
            // 两颗 NS4168 共用同一组 I2S：SEL=GND 播放左槽，SEL=VDD 播放右槽
            (i2s_std_slot_mask_t)(I2S_STD_SLOT_LEFT | I2S_STD_SLOT_RIGHT),
            AUDIO_I2S_MIC_GPIO_SCK,
            AUDIO_I2S_MIC_GPIO_WS,
            AUDIO_I2S_MIC_GPIO_DIN,
            I2S_STD_SLOT_LEFT);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        if (DISPLAY_BACKLIGHT_PIN == GPIO_NUM_NC) {
            return nullptr;
        }
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual Camera* GetCamera() override {
        return camera_;
    }
};

DECLARE_BOARD(YseEsp32s3Hmi);
