#pragma once

#include "config.h"
#include "mcp_server.h"
#include <driver/gpio.h>
#include <led_strip.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mutex>
#include <stdexcept>

#define TAG "YsePeripherals"

// Voice-controllable external peripherals exposed through MCP.
//   relay:  GPIO on/off output (drives an external relay module)
//   fan:    GPIO on/off output (drives a DC fan through a MOSFET)
//   light:  WS2812B smart LED strip driven through RMT
//           (on/off + RGB color + chase/marquee animation, all LEDs light together)
//   buzzer: GPIO on/off output (drives an active buzzer; use PWM if passive)
//   car:    4-channel motor driver (IN1/IN2 per channel, 8 GPIOs), differential drive
class YsePeripheralControls {
    struct MotorChannel {
        gpio_num_t in1;
        gpio_num_t in2;
    };

    std::mutex mutex_;
    bool relay_on_ = false;
    bool fan_on_ = false;
    bool light_on_ = false;
    bool buzzer_on_ = false;
    led_strip_handle_t light_strip_ = nullptr;

    // 走马灯（chase）状态，由 MCP 工具设置、后台任务执行
    bool chase_on_ = false;
    uint8_t chase_r_ = 255;
    uint8_t chase_g_ = 255;
    uint8_t chase_b_ = 255;
    int chase_speed_ms_ = 120;
    TaskHandle_t chase_task_ = nullptr;

    MotorChannel motor_channels_[2] = {
        {MOTOR_CH1_IN1_GPIO, MOTOR_CH1_IN2_GPIO},
        {MOTOR_CH3_IN1_GPIO, MOTOR_CH3_IN2_GPIO},
    };

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

    // All methods below that touch LEDs/motors assume mutex_ is already held,
    // unless explicitly stated otherwise.

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

    // ---- 电机 ----

    void MotorSetChannel(const MotorChannel& ch, bool forward) {
        gpio_set_level(ch.in1, forward ? 1 : 0);
        gpio_set_level(ch.in2, forward ? 0 : 1);
    }

    void MotorStopChannel(const MotorChannel& ch) {
        gpio_set_level(ch.in1, 0);
        gpio_set_level(ch.in2, 0);
    }

    void MotorStopAll() {
        for (auto& ch : motor_channels_) MotorStopChannel(ch);
    }

    void InitializeMotors() {
        for (auto& ch : motor_channels_) {
            gpio_config_t cfg = {};
            cfg.pin_bit_mask = (1ULL << ch.in1) | (1ULL << ch.in2);
            cfg.mode = GPIO_MODE_OUTPUT;
            cfg.pull_up_en = GPIO_PULLUP_DISABLE;
            cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
            cfg.intr_type = GPIO_INTR_DISABLE;
            Check(gpio_config(&cfg));
            MotorStopChannel(ch);
        }

        auto& mcp = McpServer::GetInstance();
        mcp.AddTool("self.car.move",
            "Control the car movement. action must be one of:\n"
            "forward: move forward\nbackward: move backward\n"
            "left: turn left in place\nright: turn right in place\nstop: stop immediately",
            PropertyList({Property("action", kPropertyTypeString)}),
            [this](const PropertyList& p) -> ReturnValue {
                std::lock_guard<std::mutex> lock(mutex_);
                const std::string action = p["action"].value<std::string>();
                if (action == "forward") {
                    for (auto& ch : motor_channels_) MotorSetChannel(ch, true);
                } else if (action == "backward") {
                    for (auto& ch : motor_channels_) MotorSetChannel(ch, false);
                } else if (action == "left") {
                    // 左轮反转、右轮正转，原地左转
                    MotorSetChannel(motor_channels_[0], false);
                    MotorSetChannel(motor_channels_[1], true);
                } else if (action == "right") {
                    // 左轮正转、右轮反转，原地右转
                    MotorSetChannel(motor_channels_[0], true);
                    MotorSetChannel(motor_channels_[1], false);
                } else if (action == "stop") {
                    MotorStopAll();
                } else {
                    return false;
                }
                return true;
            });
        ESP_LOGI(TAG, "Car motor controls registered");
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

        InitializeMotors();

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

        mcp.AddTool("self.peripherals.get_status",
            "Get the current state of the relay, fan, light, buzzer and the chase animation.",
            PropertyList(), [this](const PropertyList&) -> ReturnValue {
                std::lock_guard<std::mutex> lock(mutex_);
                cJSON* result = cJSON_CreateObject();
                cJSON_AddBoolToObject(result, "relay_on", relay_on_);
                cJSON_AddBoolToObject(result, "fan_on", fan_on_);
                cJSON_AddBoolToObject(result, "light_on", light_on_);
                cJSON_AddBoolToObject(result, "chase_on", chase_on_);
                cJSON_AddBoolToObject(result, "buzzer_on", buzzer_on_);
                return result;
            });

        ESP_LOGI(TAG, "Voice-controllable peripherals registered");
    }
};

#undef TAG
