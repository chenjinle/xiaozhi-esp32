#include "uvc_camera.h"

#include "config.h"
#include "display/lcd_display.h"

#include <esp_heap_caps.h>
#include <esp_jpeg_dec.h>
#include <esp_log.h>
#include <cstring>

#define TAG "YseUvcCamera"

static void usb_lib_task(void* arg) {
    while (true) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            usb_host_device_free_all();
        }
    }
}

YseUvcCamera::YseUvcCamera(LcdDisplay* display) : display_(display) {
    frame_queue_ = xQueueCreate(2, sizeof(uvc_host_frame_t*));
}

YseUvcCamera::~YseUvcCamera() {
    Stop();
    if (frame_queue_ != nullptr) {
        vQueueDelete(frame_queue_);
        frame_queue_ = nullptr;
    }
    if (decode_buf_ != nullptr) {
        jpeg_free_align(decode_buf_);
        decode_buf_ = nullptr;
    }
}

esp_err_t YseUvcCamera::Start() {
    if (running_) return ESP_OK;
    if (display_ == nullptr || frame_queue_ == nullptr) return ESP_ERR_INVALID_STATE;

    // USB Host 只能安装一次
    if (!host_installed_) {
        const usb_host_config_t host_config = {
            .skip_phy_setup = false,
            .intr_flags = ESP_INTR_FLAG_LEVEL1,
        };
        esp_err_t err = usb_host_install(&host_config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "usb_host_install failed: %s", esp_err_to_name(err));
            return err;
        }
        host_installed_ = true;
        xTaskCreate(usb_lib_task, "usb_lib", 4096, nullptr, 15, nullptr);
        ESP_LOGW(TAG, "USB Host installed, USB-Serial/JTAG console unavailable until reboot");
    }

    if (!uvc_installed_) {
        const uvc_host_driver_config_t uvc_config = {
            .driver_task_stack_size = 4 * 1024,
            .driver_task_priority = 16,
            .xCoreID = tskNO_AFFINITY,
            .create_background_task = true,
        };
        esp_err_t err = uvc_host_install(&uvc_config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "uvc_host_install failed: %s", esp_err_to_name(err));
            return err;
        }
        uvc_installed_ = true;
    }

    uvc_host_stream_config_t stream_config = {};
    stream_config.event_cb = OnStreamEvent;
    stream_config.frame_cb = OnFrame;
    stream_config.user_ctx = this;
    stream_config.usb.vid = UVC_HOST_ANY_VID;
    stream_config.usb.pid = UVC_HOST_ANY_PID;
    stream_config.usb.uvc_stream_index = 0;
    stream_config.vs_format.format = UVC_VS_FORMAT_MJPEG;
    stream_config.advanced.frame_size = 0;       // 自动按协商结果分配
    stream_config.advanced.number_of_frame_buffers = 2;
    stream_config.advanced.number_of_urbs = 2;
    stream_config.advanced.urb_size = 10 * 1024;
    stream_config.advanced.frame_heap_caps = MALLOC_CAP_SPIRAM;

    // 优先按液晶分辨率请求，摄像头不支持时自动回退 320x240
    struct {
        uint16_t width;
        uint16_t height;
        uint16_t fps;
    } modes[] = {
        {UVC_CAMERA_WIDTH, UVC_CAMERA_HEIGHT, UVC_CAMERA_FPS},
        {320, 240, 15},
    };

    esp_err_t err = ESP_ERR_NOT_FOUND;
    for (auto& mode : modes) {
        stream_config.vs_format.h_res = mode.width;
        stream_config.vs_format.v_res = mode.height;
        stream_config.vs_format.fps = mode.fps;
        err = uvc_host_stream_open(&stream_config, pdMS_TO_TICKS(5000), &stream_);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "UVC camera opened %dx%d@%d", mode.width, mode.height, mode.fps);
            break;
        }
        ESP_LOGW(TAG, "UVC open %dx%d@%d failed: %s", mode.width, mode.height, mode.fps, esp_err_to_name(err));
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "No supported UVC mode found");
        return err;
    }

    uvc_host_stream_start(stream_);
    running_ = true;
    preview_started_ = false;
    xTaskCreate([](void* arg) {
        static_cast<YseUvcCamera*>(arg)->DecodeTask();
    }, "uvc_decode", 4096, this, 5, &decode_task_);
    return ESP_OK;
}

esp_err_t YseUvcCamera::Stop() {
    if (running_) {
        running_ = false;
        if (stream_ != nullptr) {
            uvc_host_stream_stop(stream_);
            uvc_host_stream_close(stream_);
            stream_ = nullptr;
        }
        if (decode_task_ != nullptr) {
            vTaskDelete(decode_task_);
            decode_task_ = nullptr;
        }
    }
    preview_started_ = false;
    if (display_ != nullptr) {
        display_->StopCameraPreview();
    }
    ESP_LOGI(TAG, "UVC camera stopped");
    return ESP_OK;
}

bool YseUvcCamera::OnFrame(const uvc_host_frame_t* frame, void* user_ctx) {
    auto* self = static_cast<YseUvcCamera*>(user_ctx);
    if (!self->running_ || self->frame_queue_ == nullptr) {
        return true;  // 直接归还
    }
    if (xQueueSendToBack(self->frame_queue_, &frame, 0) != pdPASS) {
        return true;  // 队列满，丢弃并归还
    }
    return false;  // 由解码任务处理完后归还
}

void YseUvcCamera::OnStreamEvent(const uvc_host_stream_event_data_t* event, void* user_ctx) {
    auto* self = static_cast<YseUvcCamera*>(user_ctx);
    switch (event->type) {
    case UVC_HOST_DEVICE_DISCONNECTED:
        ESP_LOGW(TAG, "UVC camera disconnected");
        self->running_ = false;
        uvc_host_stream_close(event->device_disconnected.stream_hdl);
        self->stream_ = nullptr;
        if (self->display_ != nullptr) {
            self->display_->StopCameraPreview();
        }
        break;
    case UVC_HOST_TRANSFER_ERROR:
        ESP_LOGE(TAG, "USB transfer error: %d", event->transfer_error.error);
        break;
    case UVC_HOST_FRAME_BUFFER_OVERFLOW:
        ESP_LOGW(TAG, "Frame buffer overflow");
        break;
    case UVC_HOST_FRAME_BUFFER_UNDERFLOW:
        ESP_LOGW(TAG, "Frame buffer underflow");
        break;
    default:
        break;
    }
}

void YseUvcCamera::DecodeTask() {
    while (running_) {
        uvc_host_frame_t* frame = nullptr;
        if (xQueueReceive(frame_queue_, &frame, pdMS_TO_TICKS(500)) != pdPASS) {
            continue;
        }

        jpeg_dec_config_t dec_cfg = DEFAULT_JPEG_DEC_CONFIG();
        dec_cfg.output_type = JPEG_PIXEL_FORMAT_RGB565_LE;

        jpeg_dec_handle_t dec = nullptr;
        if (jpeg_dec_open(&dec_cfg, &dec) != JPEG_ERR_OK) {
            uvc_host_frame_return(stream_, frame);
            continue;
        }

        jpeg_dec_io_t io = {};
        io.inbuf = frame->data;
        io.inbuf_len = frame->data_len;

        jpeg_dec_header_info_t info = {};
        if (jpeg_dec_parse_header(dec, &io, &info) != JPEG_ERR_OK) {
            jpeg_dec_close(dec);
            uvc_host_frame_return(stream_, frame);
            continue;
        }

        int out_len = 0;
        jpeg_dec_get_outbuf_len(dec, &out_len);
        if (decode_buf_size_ < out_len) {
            if (decode_buf_ != nullptr) {
                jpeg_free_align(decode_buf_);
                decode_buf_ = nullptr;
            }
            decode_buf_ = static_cast<uint8_t*>(jpeg_calloc_align(out_len, 16));
            decode_buf_size_ = (decode_buf_ != nullptr) ? out_len : 0;
        }
        if (decode_buf_ == nullptr) {
            jpeg_dec_close(dec);
            uvc_host_frame_return(stream_, frame);
            continue;
        }

        io.outbuf = decode_buf_;
        io.out_size = out_len;
        if (jpeg_dec_process(dec, &io) == JPEG_ERR_OK) {
            if (display_ != nullptr) {
                if (!preview_started_) {
                    display_->StartCameraPreview(info.width, info.height);
                    preview_started_ = true;
                }
                display_->UpdateCameraPreview(decode_buf_, info.width, info.height);
            }
        }

        jpeg_dec_close(dec);
        uvc_host_frame_return(stream_, frame);
    }
    vTaskDelete(nullptr);
}
