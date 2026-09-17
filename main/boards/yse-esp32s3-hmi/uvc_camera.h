#pragma once

#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <usb/usb_host.h>
#include <usb/uvc_host.h>

class LcdDisplay;

// UVC USB 摄像头（MJPEG）驱动：
// - 摄像头默认不启动，语音调用 Start() 时才安装 USB Host + UVC 驱动并拉流
// - 抓到的 MJPEG 帧由后台任务解码成 RGB565，再推送到 LCD 实时预览
// - 注意：USB Host 一旦安装，USB-Serial/JTAG 串口将不可用，重启后恢复
class YseUvcCamera {
public:
    explicit YseUvcCamera(LcdDisplay* display);
    ~YseUvcCamera();

    esp_err_t Start();
    esp_err_t Stop();
    bool IsRunning() const { return running_; }

private:
    static bool OnFrame(const uvc_host_frame_t* frame, void* user_ctx);
    static void OnStreamEvent(const uvc_host_stream_event_data_t* event, void* user_ctx);
    void DecodeTask();

    LcdDisplay* display_ = nullptr;
    QueueHandle_t frame_queue_ = nullptr;
    TaskHandle_t decode_task_ = nullptr;
    uvc_host_stream_hdl_t stream_ = nullptr;
    volatile bool running_ = false;
    bool host_installed_ = false;
    bool uvc_installed_ = false;
    bool preview_started_ = false;
    uint8_t* decode_buf_ = nullptr;
    int decode_buf_size_ = 0;
};
