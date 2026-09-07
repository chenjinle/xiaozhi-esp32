#pragma once

#include "config.h"
#include "mcp_server.h"
#include <driver/gpio.h>
#include <led_strip.h>
#include <esp_log.h>
#include <mutex>
#include <stdexcept>

#define TAG "YsePeripherals"

// Voice-controllable external peripherals exposed through MCP.
//   relay:  GPIO on/off output (drives an external relay module)
//   fan:    GPIO on/off output (drives a DC fan through a MOSFET)
//   light:  WS2812B smart LED driven through RMT (on/off + RGB color)
//   buzzer: GPIO on/off output (drives an active buzzer; use PWM if passive)
class YsePeripheralControls {
    std::mutex mutex_;
    bool relay_on_ = false;
    bool fan_on_ = false;
    bool light_on_ = false;
    bool buzzer_on_ = false;
    led_strip_handle_t light_strip_ = nullptr;

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

    void SetLight(bool on, uint8_t r, uint8_t g, uint8_t b) {
        if (light_strip_ == nullptr) return;
        led_strip_set_pixel(light_strip_, 0, on ? r : 0, on ? g : 0, on ? b : 0);
        led_strip_refresh(light_strip_);
    }

public:
    void Initialize() {
        InitOutput(PERIPHERAL_RELAY_GPIO);
        InitOutput(PERIPHERAL_FAN_GPIO);
        InitOutput(PERIPHERAL_BUZZER_GPIO);

        led_strip_config_t strip_cfg = {};
        strip_cfg.strip_gpio_num = PERIPHERAL_LIGHT_GPIO;
        strip_cfg.max_leds = 1;
        strip_cfg.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
        strip_cfg.led_model = LED_MODEL_WS2812;
        led_strip_rmt_config_t rmt_cfg = {};
        rmt_cfg.resolution_hz = 10 * 1000 * 1000;  // 10MHz
        Check(led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &light_strip_));
        led_strip_clear(light_strip_);

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
            "Turn the WS2812B smart light on or off and optionally set its RGB color (0-255 each, default white).",
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
                SetLight(on, r, g, b);
                light_on_ = on;
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
            "Get the current state of the relay, fan, light and buzzer.",
            PropertyList(), [this](const PropertyList&) -> ReturnValue {
                std::lock_guard<std::mutex> lock(mutex_);
                cJSON* result = cJSON_CreateObject();
                cJSON_AddBoolToObject(result, "relay_on", relay_on_);
                cJSON_AddBoolToObject(result, "fan_on", fan_on_);
                cJSON_AddBoolToObject(result, "light_on", light_on_);
                cJSON_AddBoolToObject(result, "buzzer_on", buzzer_on_);
                return result;
            });

        ESP_LOGI(TAG, "Voice-controllable peripherals registered");
    }
};
