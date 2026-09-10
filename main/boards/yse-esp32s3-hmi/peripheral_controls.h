#pragma once

#include "application.h"
#include "config.h"
#include "device_state.h"
#include "dht11_sensor.h"
#include "mcp_server.h"
#include <driver/gpio.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <led_strip.h>
#include <mutex>
#include <stdexcept>

#define TAG "YsePeripherals"

// Voice-controllable external peripherals exposed through MCP.
//   relay:  GPIO on/off output (drives an external relay module)
//   fan:    GPIO on/off output (drives a DC fan through a MOSFET)
//   light:  WS2812B smart LED strip driven through RMT
//           (on/off + RGB color + chase/marquee animation, all LEDs light together)
//   buzzer: GPIO on/off output (drives an active buzzer; use PWM if passive)
//   dht11:  temperature & humidity sensor (single-wire)
//   pir:    PIR motion sensor (analog output, ADC read), auto light + greeting
//   mq2:    smoke/gas sensor DO input (active low), auto sound & light alarm
class YsePeripheralControls {
    std::mutex mutex_;
    bool relay_on_ = false;
    bool fan_on_ = false;
    bool light_on_ = false;
    bool buzzer_on_ = false;
    led_strip_handle_t light_strip_ = nullptr;
    Dht11Sensor dht11_{PERIPHERAL_DHT11_GPIO};

    // 走马灯（chase）状态，由 MCP 工具设置、后台任务执行
    bool chase_on_ = false;
    uint8_t chase_r_ = 255;
    uint8_t chase_g_ = 255;
    uint8_t chase_b_ = 255;
    int chase_speed_ms_ = 120;
    TaskHandle_t chase_task_ = nullptr;

    // 传感器后台监测任务
    TaskHandle_t sensor_task_ = nullptr;
    int64_t pir_cooldown_until_ = 0;   // 毫秒时间戳，防止持续检测到人时反复问候
    int64_t pir_last_active_ms_ = 0;   // 最后一次检测到人的时间
    bool pir_light_on_ = false;        // 灯光是否由 PIR 点亮（用于自动关灯）
    int pir_threshold_ = PERIPHERAL_PIR_ADC_THRESHOLD;  // 运行时灵敏度阈值
    bool gas_alarm_on_ = false;
    adc_oneshot_unit_handle_t pir_adc_handle_ = nullptr;

    static void Check(esp_err_t err) {
        if (err != ESP_OK) throw std::runtime_error(esp_err_to_name(err));
    }

    static void InitOutput(gpio_num_t pin) {
        gpio_config_t cfg = {};
        cfg.pin_bit_mask = 1ULL << pin;
        cfg.mode = GPIO_MODE_OUTPUT;
        cfg.pull_up_en = GPIO_PULLUP_DISABLE;
        cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
        cfg.intr_type = GPIO_INTR_DISABLE;
        Check(gpio_config(&cfg));
        Check(gpio_set_level(pin, 0));
    }

    // 以下操作 LED 的方法默认调用者已持有 mutex_

    void SetLight(bool on, uint8_t r, uint8_t g, uint8_t b) {
        if (light_strip_ == nullptr) return;
        for (int i = 0; i < PERIPHERAL_LIGHT_LED_COUNT; i++) {
            led_strip_set_pixel(light_strip_, i, on ? r : 0, on ? g : 0, on ? b : 0);
        }
        led_strip_refresh(light_strip_);
    }

    void ClearLight() {
        if (light_strip_ == nullptr) return;
        led_strip_clear(light_strip_);
    }

    // 走马灯任务：每次点亮一颗灯，其余熄灭，循环移动
    void ChaseTask() {
        int step = 0;
        while (true) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (!chase_on_) break;
                for (int i = 0; i < PERIPHERAL_LIGHT_LED_COUNT; i++) {
                    led_strip_set_pixel(light_strip_, i, 0, 0, 0);
                }
                int index = step % PERIPHERAL_LIGHT_LED_COUNT;
                led_strip_set_pixel(light_strip_, index, chase_r_, chase_g_, chase_b_);
                led_strip_refresh(light_strip_);
            }
            vTaskDelay(pdMS_TO_TICKS(chase_speed_ms_));
            step++;
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            chase_task_ = nullptr;
        }
        vTaskDelete(nullptr);
    }

    void StartChaseLocked(uint8_t r, uint8_t g, uint8_t b, int speed_ms) {
        chase_r_ = r;
        chase_g_ = g;
        chase_b_ = b;
        chase_speed_ms_ = speed_ms;
        chase_on_ = true;
        light_on_ = true;
        if (chase_task_ == nullptr) {
            xTaskCreate([](void* arg) {
                static_cast<YsePeripheralControls*>(arg)->ChaseTask();
            }, "light_chase", 4096, this, 5, &chase_task_);
        }
    }

    void StopChaseLocked() {
        chase_on_ = false;
    }

    // ---- 传感器输入初始化 ----

    void InitSensorInputs() {
        // PIR：模拟量输出，走 ADC1 通道读取（GPIO5 = ADC1_CH4）
        adc_oneshot_unit_init_cfg_t adc_init = {};
        adc_init.unit_id = ADC_UNIT_1;
        adc_init.ulp_mode = ADC_ULP_MODE_DISABLE;
        Check(adc_oneshot_new_unit(&adc_init, &pir_adc_handle_));

        adc_oneshot_chan_cfg_t adc_chan = {};
        adc_chan.atten = ADC_ATTEN_DB_12;   // 最大量程，约 0~3.1V
        adc_chan.bitwidth = ADC_BITWIDTH_12;
        Check(adc_oneshot_config_channel(pir_adc_handle_, ADC_CHANNEL_4, &adc_chan));

        // MQ2 DO：报警极性由 PERIPHERAL_MQ2_DO_ACTIVE_HIGH 决定；
        // 未接模块时固定为"不报警"电平
        gpio_config_t mq2_cfg = {};
        mq2_cfg.pin_bit_mask = 1ULL << PERIPHERAL_MQ2_DO_GPIO;
        mq2_cfg.mode = GPIO_MODE_INPUT;
#if PERIPHERAL_MQ2_DO_ACTIVE_HIGH
        mq2_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
        mq2_cfg.pull_down_en = GPIO_PULLDOWN_ENABLE;   // 未接 = 低 = 不报警
#else
        mq2_cfg.pull_up_en = GPIO_PULLUP_ENABLE;       // 未接 = 高 = 不报警
        mq2_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
#endif
        mq2_cfg.intr_type = GPIO_INTR_DISABLE;
        Check(gpio_config(&mq2_cfg));

        ESP_LOGI(TAG, "Sensor inputs initialized (PIR ADC1_CH4 on GPIO%d, MQ2 DO GPIO%d)",
                 (int)PERIPHERAL_PIR_GPIO, (int)PERIPHERAL_MQ2_DO_GPIO);
    }

    // ---- 传感器后台监测任务 ----

    void HandlePirActive() {
        int64_t now = esp_timer_get_time() / 1000;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pir_last_active_ms_ = now;
            if (!pir_light_on_) {
                pir_light_on_ = true;
                StopChaseLocked();
                SetLight(true, 255, 200, 100);      // 暖白灯（氛围灯）
                light_on_ = true;
#if PERIPHERAL_PIR_CONTROL_RELAY
                Check(gpio_set_level(PERIPHERAL_RELAY_GPIO, 1));
                relay_on_ = true;
#endif
            }
        }

        // 语音问候"主人你好"（服务器 TTS，空闲状态才触发，30 秒冷却）
        if (now >= pir_cooldown_until_) {
            pir_cooldown_until_ = now + 30000;
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateIdle) {
                app.WakeWordInvoke("主人你好");
            }
        }
        ESP_LOGD(TAG, "PIR active");
    }

    void HandlePirLeave() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (pir_light_on_) {
            pir_light_on_ = false;
            ClearLight();
            light_on_ = false;
#if PERIPHERAL_PIR_CONTROL_RELAY
            Check(gpio_set_level(PERIPHERAL_RELAY_GPIO, 0));
            relay_on_ = false;
#endif
            ESP_LOGI(TAG, "PIR leave: light off");
        }
    }

    void HandleGasAlarmStart() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!gas_alarm_on_) {
            gas_alarm_on_ = true;
            Check(gpio_set_level(PERIPHERAL_BUZZER_GPIO, 1));
            buzzer_on_ = true;
            StopChaseLocked();
            SetLight(true, 255, 0, 0);  // 红灯
            ESP_LOGW(TAG, "Gas alarm triggered!");
        }
    }

    void HandleGasAlarmStop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (gas_alarm_on_) {
            gas_alarm_on_ = false;
            Check(gpio_set_level(PERIPHERAL_BUZZER_GPIO, 0));
            buzzer_on_ = false;
            ClearLight();
            light_on_ = false;
            ESP_LOGI(TAG, "Gas alarm cleared");
        }
    }

    void SensorMonitorTask() {
        int pir_debounce = 0;
        int mq2_debounce = 0;
        int flash_counter = 0;
        int log_counter = 0;
        bool flash_on = false;

        ESP_LOGI(TAG, "Sensor monitor task started");

        while (true) {
            // PIR：模拟量输出，ADC 读取（12 位原始值左移 4 位，对齐 MicroPython read_u16）
            int pir_raw = 0;
            if (pir_adc_handle_ != nullptr &&
                adc_oneshot_read(pir_adc_handle_, ADC_CHANNEL_4, &pir_raw) == ESP_OK) {
                int pir_raw16 = pir_raw << 4;
                if (pir_raw16 > pir_threshold_) {
                    if (++pir_debounce >= 3) {
                        pir_debounce = 3;
                        HandlePirActive();
                    }
                } else if (pir_raw16 < PERIPHERAL_PIR_ADC_RELEASE) {
                    pir_debounce = 0;
                }
            }

            // 人体离开超过延时后自动关灯
            if (pir_light_on_) {
                int64_t now = esp_timer_get_time() / 1000;
                if (now - pir_last_active_ms_ >= PERIPHERAL_PIR_OFF_DELAY_MS) {
                    HandlePirLeave();
                }
            }

            // MQ2 DO：报警极性由 PERIPHERAL_MQ2_DO_ACTIVE_HIGH 决定，需连续 3 次（300ms）确认
            int mq2_alarm_level = PERIPHERAL_MQ2_DO_ACTIVE_HIGH ? 1 : 0;
            if (gpio_get_level(PERIPHERAL_MQ2_DO_GPIO) == mq2_alarm_level) {
                if (++mq2_debounce >= 3) {
                    mq2_debounce = 3;
                    HandleGasAlarmStart();
                }
            } else {
                if (mq2_debounce >= 3) HandleGasAlarmStop();
                mq2_debounce = 0;
            }

            // 报警期间红灯闪烁（每 500ms 翻转）
            if (gas_alarm_on_) {
                if (++flash_counter >= 5) {
                    flash_counter = 0;
                    flash_on = !flash_on;
                    std::lock_guard<std::mutex> lock(mutex_);
                    SetLight(flash_on, 255, 0, 0);
                }
            }

            // 每 5 秒打印一次 PIR ADC 值，方便调阈值
            if (++log_counter >= 50) {
                log_counter = 0;
                int raw = 0;
                if (pir_adc_handle_ != nullptr &&
                    adc_oneshot_read(pir_adc_handle_, ADC_CHANNEL_4, &raw) == ESP_OK) {
                    ESP_LOGI(TAG, "PIR ADC raw16=%d (threshold=%d)", raw << 4, pir_threshold_);
                }
            }

            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    void StartSensorMonitorTask() {
        xTaskCreate([](void* arg) {
            static_cast<YsePeripheralControls*>(arg)->SensorMonitorTask();
        }, "sensor_monitor", 4096, this, 5, &sensor_task_);
    }

public:
    void Initialize() {
        InitOutput(PERIPHERAL_RELAY_GPIO);
        InitOutput(PERIPHERAL_FAN_GPIO);
        InitOutput(PERIPHERAL_BUZZER_GPIO);

        led_strip_config_t strip_cfg = {};
        strip_cfg.strip_gpio_num = PERIPHERAL_LIGHT_GPIO;
        strip_cfg.max_leds = PERIPHERAL_LIGHT_LED_COUNT;
        strip_cfg.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
        strip_cfg.led_model = LED_MODEL_WS2812;
        led_strip_rmt_config_t rmt_cfg = {};
        rmt_cfg.resolution_hz = 10 * 1000 * 1000;  // 10MHz
        Check(led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &light_strip_));
        led_strip_clear(light_strip_);

        InitSensorInputs();
        StartSensorMonitorTask();

        auto& mcp = McpServer::GetInstance();

        mcp.AddTool("self.relay.set", "Turn the external relay on or off.",
            PropertyList({Property("on", kPropertyTypeBoolean)}),
            [this](const PropertyList& p) -> ReturnValue {
                std::lock_guard<std::mutex> lock(mutex_);
                bool on = p["on"].value<bool>();
                Check(gpio_set_level(PERIPHERAL_RELAY_GPIO, on ? 1 : 0));
                relay_on_ = on;
                return true;
            });

        mcp.AddTool("self.fan.set", "Turn the cooling fan on or off.",
            PropertyList({Property("on", kPropertyTypeBoolean)}),
            [this](const PropertyList& p) -> ReturnValue {
                std::lock_guard<std::mutex> lock(mutex_);
                bool on = p["on"].value<bool>();
                Check(gpio_set_level(PERIPHERAL_FAN_GPIO, on ? 1 : 0));
                fan_on_ = on;
                return true;
            });

        mcp.AddTool("self.light.set",
            "Turn the WS2812B light strip on or off and optionally set its RGB color (0-255 each, default white). "
            "All LEDs of the strip light up together with the same color. This stops the chase animation if it is running.",
            PropertyList({
                Property("on", kPropertyTypeBoolean),
                Property("red", kPropertyTypeInteger, 255, 0, 255),
                Property("green", kPropertyTypeInteger, 255, 0, 255),
                Property("blue", kPropertyTypeInteger, 255, 0, 255)
            }),
            [this](const PropertyList& p) -> ReturnValue {
                std::lock_guard<std::mutex> lock(mutex_);
                bool on = p["on"].value<bool>();
                uint8_t r = static_cast<uint8_t>(p["red"].value<int>());
                uint8_t g = static_cast<uint8_t>(p["green"].value<int>());
                uint8_t b = static_cast<uint8_t>(p["blue"].value<int>());
                StopChaseLocked();
                SetLight(on, r, g, b);
                light_on_ = on;
                return true;
            });

        mcp.AddTool("self.light.set_chase",
            "Start or stop the chase (marquee) animation of the WS2812B light strip. "
            "When started, a single LED lights up and moves along the strip repeatedly. "
            "red/green/blue set the color (0-255, default white), speed_ms sets the moving interval (default 120ms).",
            PropertyList({
                Property("on", kPropertyTypeBoolean),
                Property("red", kPropertyTypeInteger, 255, 0, 255),
                Property("green", kPropertyTypeInteger, 255, 0, 255),
                Property("blue", kPropertyTypeInteger, 255, 0, 255),
                Property("speed_ms", kPropertyTypeInteger, 120, 20, 1000)
            }),
            [this](const PropertyList& p) -> ReturnValue {
                std::lock_guard<std::mutex> lock(mutex_);
                bool on = p["on"].value<bool>();
                if (on) {
                    uint8_t r = static_cast<uint8_t>(p["red"].value<int>());
                    uint8_t g = static_cast<uint8_t>(p["green"].value<int>());
                    uint8_t b = static_cast<uint8_t>(p["blue"].value<int>());
                    int speed_ms = p["speed_ms"].value<int>();
                    StartChaseLocked(r, g, b, speed_ms);
                } else {
                    StopChaseLocked();
                    ClearLight();
                    light_on_ = false;
                }
                return true;
            });

        mcp.AddTool("self.buzzer.set", "Turn the buzzer on or off.",
            PropertyList({Property("on", kPropertyTypeBoolean)}),
            [this](const PropertyList& p) -> ReturnValue {
                std::lock_guard<std::mutex> lock(mutex_);
                bool on = p["on"].value<bool>();
                Check(gpio_set_level(PERIPHERAL_BUZZER_GPIO, on ? 1 : 0));
                buzzer_on_ = on;
                return true;
            });

        mcp.AddTool("self.pir.set_sensitivity",
            "Adjust the PIR motion sensor sensitivity threshold (0-65535, default 500). "
            "Lower value = more sensitive, higher value = less sensitive.",
            PropertyList({Property("threshold", kPropertyTypeInteger, 500, 0, 65535)}),
            [this](const PropertyList& p) -> ReturnValue {
                std::lock_guard<std::mutex> lock(mutex_);
                pir_threshold_ = p["threshold"].value<int>();
                ESP_LOGI(TAG, "PIR threshold set to %d", pir_threshold_);
                return true;
            });

        mcp.AddTool("self.sensor.read_temperature_humidity",
            "Read the ambient temperature and humidity from the DHT11 sensor. "
            "Returns a JSON object with \"temperature\" in Celsius and \"humidity\" in percent.",
            PropertyList(), [this](const PropertyList&) -> ReturnValue {
                float temperature = 0.0f;
                float humidity = 0.0f;
                cJSON* result = cJSON_CreateObject();
                if (dht11_.Read(temperature, humidity)) {
                    cJSON_AddNumberToObject(result, "temperature", temperature);
                    cJSON_AddNumberToObject(result, "humidity", humidity);
                } else {
                    cJSON_AddBoolToObject(result, "error", true);
                }
                return result;
            });

        mcp.AddTool("self.peripherals.get_status",
            "Get the current state of the relay, fan, light, buzzer, chase animation, PIR light and gas alarm.",
            PropertyList(), [this](const PropertyList&) -> ReturnValue {
                std::lock_guard<std::mutex> lock(mutex_);
                cJSON* result = cJSON_CreateObject();
                cJSON_AddBoolToObject(result, "relay_on", relay_on_);
                cJSON_AddBoolToObject(result, "fan_on", fan_on_);
                cJSON_AddBoolToObject(result, "light_on", light_on_);
                cJSON_AddBoolToObject(result, "chase_on", chase_on_);
                cJSON_AddBoolToObject(result, "buzzer_on", buzzer_on_);
                cJSON_AddBoolToObject(result, "gas_alarm_on", gas_alarm_on_);
                cJSON_AddBoolToObject(result, "pir_light_on", pir_light_on_);
                return result;
            });

        ESP_LOGI(TAG, "Voice-controllable peripherals registered");
    }
};

#undef TAG
