#pragma once
#include <Arduino.h>

struct Theme {
    const char* name;
    uint16_t bgColor;
    uint16_t textColor;
    uint16_t highlightColor;
    uint16_t progressBgColor;
    uint16_t progressFillColor;
    uint16_t statusBgColor;
};

// 预定义主题
namespace Themes {
    const Theme Classic = {
        "Classic",
        0x0000, // Black
        0xFFFF, // White
        0x07E0, // Green
        0x4208, // Dark Grey
        0x07E0, // Green
        0x2104  // Dark Grey
    };

    const Theme Blue = {
        "Blue",
        0x001F, // Blue
        0xFFFF, // White
        0xFFE0, // Yellow
        0x0010, // Dark Blue
        0xFFE0, // Yellow
        0x0015  // Mid Blue
    };
    
    const Theme Light = {
        "Light",
        0xFFFF, // White
        0x0000, // Black
        0xF800, // Red
        0xCE79, // Light Grey
        0xF800, // Red
        0xE71C  // Light Grey
    };
}
