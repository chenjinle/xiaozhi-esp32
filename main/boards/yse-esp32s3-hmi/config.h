#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>
#include <driver/i2c_types.h>
#include <driver/spi_master.h>
#include <esp_lcd_panel_dev.h>

#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_I2S_MIC_GPIO_WS    GPIO_NUM_40
#define AUDIO_I2S_MIC_GPIO_SCK   GPIO_NUM_42
#define AUDIO_I2S_MIC_GPIO_DIN   GPIO_NUM_41
#define AUDIO_I2S_SPK_GPIO_DOUT  GPIO_NUM_21
#define AUDIO_I2S_SPK_GPIO_BCLK  GPIO_NUM_47
#define AUDIO_I2S_SPK_GPIO_LRCK  GPIO_NUM_48
#define AUDIO_I2S_SPK_GPIO_EN    GPIO_NUM_NC

#define BUILTIN_LED_GPIO         GPIO_NUM_NC
#define BOOT_BUTTON_GPIO         GPIO_NUM_0
#define VOLUME_UP_BUTTON_GPIO    GPIO_NUM_NC
#define VOLUME_DOWN_BUTTON_GPIO  GPIO_NUM_NC

#define I2C_SDA_PIN              GPIO_NUM_1
#define I2C_SCL_PIN              GPIO_NUM_2
#define I2C_PORT                 I2C_NUM_1

#define DISPLAY_SPI_HOST         SPI3_HOST
#define DISPLAY_SPI_MODE         0
#define DISPLAY_CS_PIN           GPIO_NUM_10
#define DISPLAY_MOSI_PIN         GPIO_NUM_11
#define DISPLAY_MISO_PIN         GPIO_NUM_13
#define DISPLAY_CLK_PIN          GPIO_NUM_12
#define DISPLAY_DC_PIN           GPIO_NUM_14
#define DISPLAY_RST_PIN          GPIO_NUM_39
#define DISPLAY_BACKLIGHT_PIN    GPIO_NUM_38
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

#define DISPLAY_WIDTH            480
#define DISPLAY_HEIGHT           320
#define DISPLAY_MIRROR_X         false
#define DISPLAY_MIRROR_Y         false
#define DISPLAY_SWAP_XY          true
#define DISPLAY_RGB_ORDER        LCD_RGB_ELEMENT_ORDER_BGR
#define DISPLAY_INVERT_COLOR     true
#define DISPLAY_OFFSET_X         0
#define DISPLAY_OFFSET_Y         0

#define TOUCH_RST_PIN            GPIO_NUM_43
#define TOUCH_INT_PIN            GPIO_NUM_44
#define TOUCH_SWAP_XY            1
#define TOUCH_MIRROR_X           1
#define TOUCH_MIRROR_Y           1

#define CAM_PIN_PWDN             GPIO_NUM_NC
#define CAM_PIN_RESET            GPIO_NUM_45
#define CAM_PIN_VSYNC            GPIO_NUM_3
#define CAM_PIN_HREF             GPIO_NUM_46
#define CAM_PIN_PCLK             GPIO_NUM_7
#define CAM_PIN_XCLK             GPIO_NUM_5
#define CAM_PIN_D0               GPIO_NUM_16
#define CAM_PIN_D1               GPIO_NUM_18
#define CAM_PIN_D2               GPIO_NUM_8
#define CAM_PIN_D3               GPIO_NUM_17
#define CAM_PIN_D4               GPIO_NUM_15
#define CAM_PIN_D5               GPIO_NUM_6
#define CAM_PIN_D6               GPIO_NUM_4
#define CAM_PIN_D7               GPIO_NUM_9
#define CAM_XCLK_FREQ_HZ         24000000

#define SDCARD_SDMMC_ENABLED      0
#define SDCARD_SDMMC_BUS_WIDTH    1
#define SDCARD_MOUNT_POINT        "/sdcard"
#define SDCARD_SDMMC_CLK_PIN      GPIO_NUM_12
#define SDCARD_SDMMC_CMD_PIN      GPIO_NUM_13
#define SDCARD_SDMMC_D0_PIN       GPIO_NUM_11

// External peripherals controlled via MCP (voice control)
#define PERIPHERAL_RELAY_GPIO     GPIO_NUM_3
#define PERIPHERAL_FAN_GPIO       GPIO_NUM_7
#define PERIPHERAL_LIGHT_GPIO     GPIO_NUM_8
#define PERIPHERAL_LIGHT_LED_COUNT 4
#define PERIPHERAL_BUZZER_GPIO    GPIO_NUM_9

// Car motor driver: 2 channels, 2 IOs each (IN1/IN2), 4 IOs total.
// Reuses the idle camera pins (camera is disabled on this board).
// Left side: CH1, right side: CH3 (differential drive).
// NOTE: GPIO3/GPIO46 are strapping pins (sampled at reset only); keep the
// driver/relay inputs high-impedance at power-on. GPIO45 (VDD_SPI strap) is NOT used.
#define MOTOR_CH1_IN1_GPIO        GPIO_NUM_15
#define MOTOR_CH1_IN2_GPIO        GPIO_NUM_4
#define MOTOR_CH3_IN1_GPIO        GPIO_NUM_16
#define MOTOR_CH3_IN2_GPIO        GPIO_NUM_17

// ML307 Cat.1 4G module UART (dual network: Wi-Fi + 4G)
// ESP32 TX (GPIO5) -> ML307 RX; ESP32 RX (GPIO6) <- ML307 TX
#define ML307_TX_PIN              GPIO_NUM_5
#define ML307_RX_PIN              GPIO_NUM_6

#endif // _BOARD_CONFIG_H_
