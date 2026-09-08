#pragma once

#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_rom_sys.h>
#include <freertos/FreeRTOS.h>

// Minimal bit-bang DHT11 driver.
// The data pin must have an external 4.7k~10k pull-up to 3.3V
// (many DHT11 breakout boards already include it).
class Dht11Sensor {
private:
    gpio_num_t pin_;
    portMUX_TYPE spinlock_ = portMUX_INITIALIZER_UNLOCKED;

    static int WaitLevel(gpio_num_t pin, int level, int timeout_us) {
        int waited = 0;
        while (gpio_get_level(pin) != level) {
            if (waited >= timeout_us) return -1;
            esp_rom_delay_us(1);
            waited++;
        }
        return waited;
    }

public:
    explicit Dht11Sensor(gpio_num_t pin) : pin_(pin) {
        gpio_config_t cfg = {};
        cfg.pin_bit_mask = 1ULL << pin_;
        cfg.mode = GPIO_MODE_INPUT_OUTPUT_OD;  // open-drain, 释放总线由外部上拉
        cfg.pull_up_en = GPIO_PULLUP_ENABLE;
        cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
        cfg.intr_type = GPIO_INTR_DISABLE;
        ESP_ERROR_CHECK(gpio_config(&cfg));
        gpio_set_level(pin_, 1);
        ESP_LOGI("Dht11Sensor", "DHT11 initialized on GPIO %d", (int)pin_);
    }

    // Returns true on success, fills temperature (Celsius) and humidity (percent).
    bool Read(float& temperature, float& humidity) {
        uint8_t data[5] = {0};

        // 起始信号：主机拉低 18ms 再释放
        gpio_set_level(pin_, 0);
        esp_rom_delay_us(18000);
        gpio_set_level(pin_, 1);
        esp_rom_delay_us(30);

        // 数据阶段对时序敏感，关中断读取（约 4~5ms）
        portENTER_CRITICAL(&spinlock_);

        // DHT11 响应：拉低 80us，再拉高 80us
        if (WaitLevel(pin_, 0, 100) < 0) goto fail;
        if (WaitLevel(pin_, 1, 100) < 0) goto fail;
        // 等待响应高电平结束
        if (WaitLevel(pin_, 0, 100) < 0) goto fail;

        // 读 40 位数据：每 bit 以 50us 低电平开始，
        // 高电平 26~28us = 0，70us = 1
        for (int i = 0; i < 40; i++) {
            if (WaitLevel(pin_, 1, 100) < 0) goto fail;
            int high_time = 0;
            while (gpio_get_level(pin_) == 1) {
                if (high_time >= 100) goto fail;
                esp_rom_delay_us(1);
                high_time++;
            }
            data[i / 8] <<= 1;
            if (high_time > 40) data[i / 8] |= 1;
        }

        portEXIT_CRITICAL(&spinlock_);

        // 校验和：前四个字节相加等于第五个字节
        if ((uint8_t)(data[0] + data[1] + data[2] + data[3]) != data[4]) {
            ESP_LOGW("Dht11Sensor", "Checksum mismatch");
            return false;
        }

        humidity = data[0] + data[1] * 0.1f;      // 湿度 %RH
        temperature = data[2] + data[3] * 0.1f;   // 温度 ℃
        return true;

fail:
        portEXIT_CRITICAL(&spinlock_);
        ESP_LOGW("Dht11Sensor", "Read timeout");
        return false;
    }
};
