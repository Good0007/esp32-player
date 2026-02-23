#pragma once

#include <Arduino.h>

// 麦克风
#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_41
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_39
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_40

// 音频输出 (I2S)
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_5
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_6
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_7

#define BUILTIN_LED_GPIO        GPIO_NUM_48

// 功能控制键
#define MODEL_BUTTON_GPIO       GPIO_NUM_14
// 音量键
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_9
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_21

// 定义 SD 卡的 GPIO 引脚 (SPI Mode)
#define SD_CS_PIN   GPIO_NUM_10
#define SD_MOSI_PIN GPIO_NUM_11
#define SD_CLK_PIN  GPIO_NUM_12
#define SD_MISO_PIN GPIO_NUM_13

// 挂载点 (SPIFFS/SD)
#define MOUNT_POINT "/sdcard"

// ---- 屏幕-预留 -----
#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_15
#define DISPLAY_MOSI_PIN      GPIO_NUM_18
#define DISPLAY_CLK_PIN       GPIO_NUM_8
#define DISPLAY_DC_PIN        GPIO_NUM_16
#define DISPLAY_RST_PIN       GPIO_NUM_17
#define DISPLAY_CS_PIN        -1

// Screen Config
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  240
#define DISPLAY_SPI_MODE 3
