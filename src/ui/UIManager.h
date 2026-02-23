#pragma once

#ifdef ENABLE_DISPLAY

#include "../display/LGFX_Setup.h"
#include "Theme.h"

class UIManager {
public:
    UIManager();
    void begin();
    
    // Core updates
    void updateSongInfo(String filename, int index, int total);
    void updateProgress(int current, int total); // Seconds
    void updateVisualizer(); // New method for spectrum animation
    void updateStatus(String modeName, int volume, bool isPlaying);
    void updateVolume(int volume);
    void updateBitrate(int bitrate); // New method
    void showLoading(String message); // New method
    
    // Theme
    void nextTheme();
    void setTheme(const Theme& theme);

private:
    void updateScrollingText();
    
    LGFX_ST7789 _lcd;
    Theme _currentTheme;
    int _themeIndex;
    
    // Cache to avoid flickering// State cache
    String _lastSongName;
    String _lastMode;
    int _lastVolume;
    bool _lastIsPlaying;
    int _lastBitrate = 0; // Cache bitrate
    
    // Scrolling state
    int _songNameWidth = 0;
    int _scrollX = 0;
    int _scrollState = 0; // 0:WaitStart, 1:Scrolling, 2:WaitEnd
    unsigned long _lastScrollUpdate = 0;
    unsigned long _scrollWaitStart = 0;
    
    void drawUI();
    void drawStatusBar();
    void drawMainArea();
    void drawProgressBar(float percentage);
};

extern UIManager ui;

#endif
