#include "UIManager.h"

#ifdef ENABLE_DISPLAY

UIManager ui;

const Theme* availableThemes[] = { &Themes::Classic, &Themes::Blue, &Themes::Light };
const int themeCount = 3;

UIManager::UIManager() : _themeIndex(0), _currentTheme(Themes::Classic) {
    _lastVolume = -1;
    _lastIsPlaying = false;
}

void UIManager::begin() {
    Serial.println("UIManager: Initializing display...");
    
    if (!_lcd.init()) {
        Serial.println("UIManager: Display init failed!");
        return;
    }
    Serial.println("UIManager: Display init success");

    _lcd.setRotation(3);
    _lcd.setBrightness(128);
    _lcd.setFont(&fonts::efontCN_16); // Support Chinese characters
    _lcd.fillScreen(_currentTheme.bgColor);
    
    drawUI();
    Serial.println("UIManager: UI drawn");
}

void UIManager::setTheme(const Theme& theme) {
    _currentTheme = theme;
    _lcd.fillScreen(_currentTheme.bgColor);
    drawUI();
    // Re-draw current state
    updateStatus(_lastMode, _lastVolume, _lastIsPlaying);
    // Force redraw song info if we have it
    if (_lastSongName.length() > 0) {
        updateSongInfo(_lastSongName, 0, 0); // Todo: cache index
    }
}

void UIManager::nextTheme() {
    _themeIndex = (_themeIndex + 1) % themeCount;
    setTheme(*availableThemes[_themeIndex]);
}

void UIManager::drawUI() {
    drawStatusBar();
    drawMainArea();
    // Static elements moved to dynamic updates or MainArea
}

void UIManager::drawStatusBar() {
    _lcd.setTextSize(1);
    _lcd.fillRect(0, 0, 240, 24, _currentTheme.statusBgColor);
    // Index will be drawn here (Left)
    // Volume will be drawn here (Right)
    // Mode? Maybe small in center or moved to bottom info area
}

void UIManager::drawMainArea() {
    // Clear main area
    _lcd.fillRect(0, 24, 240, 216, _currentTheme.bgColor);
    
    // Static labels if any
    // Bitrate label area: Y=140
    // Song Name area: Y=160
    // Progress Bar: Y=190
    // Time/Controls: Y=210
}

void UIManager::updateVisualizer() {
    // Handle Scrolling Text
    updateScrollingText();

    if (!_lastIsPlaying) return;
    
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate < 30) return; // ~33fps
    lastUpdate = millis();
    
    int startX = 20;
    int startY = 120; // Spectrum bottom at 120 (Moved up)
    int maxH = 80;    // Height up to Y=40
    int bars = 16;
    int barW = 8;
    int gap = 4;
    
    static int currentHeights[16] = {0};
    static int peakHeights[16] = {0}; // Peak hold
    
    for (int i = 0; i < bars; i++) {
        int target = random(5, maxH);
        
        // Physics: Rise/Decay
        if (target > currentHeights[i]) {
            currentHeights[i] = (currentHeights[i] * 2 + target) / 3;
        } else {
            currentHeights[i] -= 3;
        }
        
        // Clamp
        if (currentHeights[i] < 2) currentHeights[i] = 2;
        if (currentHeights[i] > maxH) currentHeights[i] = maxH;
        
        // Peak Logic
        if (currentHeights[i] > peakHeights[i]) {
            peakHeights[i] = currentHeights[i];
        } else {
            peakHeights[i] -= 1; // Slow decay for peak
            if (peakHeights[i] < currentHeights[i]) peakHeights[i] = currentHeights[i];
        }
        
        int h = currentHeights[i];
        int p = peakHeights[i];
        int x = startX + i * (barW + gap);
        
        // Redraw bar
        // Clear slot
        _lcd.fillRect(x, startY - maxH - 2, barW, maxH + 2, _currentTheme.bgColor); // +2 for peak space
        
        // Draw Bar
        _lcd.fillRect(x, startY - h, barW, h, _currentTheme.highlightColor);
        
        // Draw Peak Dot
        if (p > h + 1) {
             _lcd.fillRect(x, startY - p, barW, 2, _currentTheme.textColor); // White/Text color for peak
        }
    }
}

void UIManager::updateBitrate(int bitrate) {
    if (bitrate == _lastBitrate) return;
    _lastBitrate = bitrate;
    
    // Draw Bitrate at Y=130 Left (Moved up)
    _lcd.setTextSize(1);
    _lcd.fillRect(10, 130, 100, 16, _currentTheme.bgColor);
    _lcd.setCursor(10, 130);
    _lcd.setTextColor(_currentTheme.textColor, _currentTheme.bgColor);
    _lcd.printf("%d kbps", bitrate / 1000);
    
    // Draw "MP3" or "NOR" on Right
    _lcd.setCursor(180, 130);
    _lcd.print("MP3");
}

void UIManager::updateSongInfo(String filename, int index, int total) {
    // Clear the main area (remove Loading text, old spectrum artifacts)
    // Area: Top Status (24) to Song Name (160)
    _lcd.fillRect(0, 24, 240, 136, _currentTheme.bgColor);
    
    _lcd.setTextSize(1);
    _lcd.setTextWrap(false); // Disable wrap for scrolling
    
    // Clean filename
    int slashIdx = filename.lastIndexOf('/');
    if (slashIdx >= 0) filename = filename.substring(slashIdx + 1);
    
    _lastSongName = filename;
    _lcd.setTextSize(1); // Ensure size 1 (16px)
    _lcd.setTextColor(_currentTheme.textColor, _currentTheme.bgColor);
    
    // Width Calc
    _songNameWidth = _lcd.textWidth(filename);
    _scrollState = 0;
    _scrollWaitStart = millis();
    
    if (_songNameWidth <= 240) {
        _scrollX = (240 - _songNameWidth) / 2;
        if (_scrollX < 0) _scrollX = 0;
    } else {
        _scrollX = 10;
    }
    
    // Y Position for Song Name: 160
    _lcd.fillRect(0, 160, 240, 24, _currentTheme.bgColor); // Increase clear height
    _lcd.setCursor(_scrollX, 160);
    _lcd.print(filename);
    
    // Index moved to Top Left (Status Bar)
    if (total > 0) {
        String idxStr = String(index) + "/" + String(total);
        // Draw at Top Left: 5, 5
        _lcd.fillRect(0, 0, 100, 24, _currentTheme.statusBgColor); // Clear Left
        _lcd.setCursor(5, 5);
        _lcd.setTextColor(_currentTheme.textColor, _currentTheme.statusBgColor);
        _lcd.print(idxStr);
    }
}

void UIManager::updateScrollingText() {
    if (_songNameWidth <= 240) return;
    
    unsigned long now = millis();
    if (now - _lastScrollUpdate < 50) return;
    _lastScrollUpdate = now;
    
    bool needRedraw = false;
    
    switch (_scrollState) {
        case 0: // Wait Start
            if (now - _scrollWaitStart > 2000) _scrollState = 1;
            break;
        case 1: // Scrolling
            _scrollX -= 2;
            needRedraw = true;
            if (_scrollX + _songNameWidth < 230) {
                _scrollState = 2;
                _scrollWaitStart = now;
            }
            break;
        case 2: // Wait End
            if (now - _scrollWaitStart > 2000) _scrollState = 3;
            break;
        case 3: // Reset
            _scrollX = 10;
            _scrollState = 0;
            _scrollWaitStart = now;
            needRedraw = true;
            break;
    }
    
    if (needRedraw) {
        _lcd.setTextWrap(false);
        _lcd.fillRect(0, 160, 240, 24, _currentTheme.bgColor);
        _lcd.setCursor(_scrollX, 160);
        _lcd.setTextColor(_currentTheme.textColor, _currentTheme.bgColor);
        _lcd.print(_lastSongName);
    }
}

void UIManager::updateStatus(String modeName, int volume, bool isPlaying) {
    _lcd.setTextSize(1);
    _lastMode = modeName;
    _lastVolume = volume;
    _lastIsPlaying = isPlaying;
    
    // Mode Name - Top Center
    // Clear center area: 80 to 160 (80px width) to avoid overlap with Index(Left) and Volume(Right)
    _lcd.fillRect(80, 0, 80, 24, _currentTheme.statusBgColor);
    
    _lcd.setTextColor(_currentTheme.textColor, _currentTheme.statusBgColor);
    int modeW = _lcd.textWidth(modeName);
    // Center alignment
    int modeX = (240 - modeW) / 2;
    // Vertical center: (24 - 16) / 2 = 4
    _lcd.setCursor(modeX, 4);
    _lcd.print(modeName);
    
    // Play Icon - Bottom Middle
    int iconX = 114;
    int iconY = 212;
    _lcd.fillRect(iconX, iconY, 12, 12, _currentTheme.bgColor);
    
    if (isPlaying) {
        // Playing -> Show Pause Bars (White)
        _lcd.fillRect(iconX, iconY, 4, 12, _currentTheme.textColor);
        _lcd.fillRect(iconX+8, iconY, 4, 12, _currentTheme.textColor);
    } else {
        // Paused -> Show Play Triangle (White)
        _lcd.fillTriangle(iconX, iconY, iconX, iconY+12, iconX+12, iconY+6, _currentTheme.textColor);
    }
    
    // Volume - Top Right
    updateVolume(volume);
}

void UIManager::updateVolume(int volume) {
    _lcd.setTextSize(1);
    
    String volStr = String(volume);
    int textW = _lcd.textWidth(volStr);
    int iconW = 14; 
    int totalW = iconW + 4 + textW; // Gap 4
    
    // Align Right: End at 235 (5px padding)
    int startX = 235 - totalW;
    
    // Clear area: From 180 to 240
    _lcd.fillRect(180, 0, 60, 24, _currentTheme.statusBgColor); // Clear full height
    
    int volX = startX;
    
    // Icon
    int icoY = 6;
    _lcd.fillRect(volX, icoY + 3, 2, 6, _currentTheme.textColor);
    _lcd.fillTriangle(volX + 2, icoY + 6, volX + 7, icoY + 1, volX + 7, icoY + 11, _currentTheme.textColor);
    if (volume > 0) _lcd.drawLine(volX + 9, icoY + 4, volX + 9, icoY + 8, _currentTheme.textColor);
    if (volume > 5) _lcd.drawLine(volX + 11, icoY + 2, volX + 11, icoY + 10, _currentTheme.textColor);
    
    // Text
    _lcd.setCursor(volX + iconW, 3); // Moved up from 5 to 3
    _lcd.setTextColor(_currentTheme.textColor, _currentTheme.statusBgColor);
    _lcd.print(volStr);
}

void UIManager::showLoading(String message) {
    // Clear main area
    _lcd.fillRect(0, 24, 240, 216, _currentTheme.bgColor);
    
    // Draw loading text in center
    _lcd.setTextSize(1);
    _lcd.setTextColor(_currentTheme.textColor, _currentTheme.bgColor);
    int w = _lcd.textWidth(message);
    _lcd.setCursor((240 - w) / 2, 110);
    _lcd.print(message);
}

void UIManager::updateProgress(int current, int total) {
    if (total <= 0) return;
    
    // Progress Bar Y=190
    float pct = (float)current / total;
    if (pct > 1.0) pct = 1.0;
    
    int w = 218 * pct;
    // Thinner progress bar (6px)
    _lcd.fillRect(11, 191, w, 6, _currentTheme.progressFillColor);
    _lcd.fillRect(11 + w, 191, 218 - w, 6, _currentTheme.progressBgColor);
    
    // Time text Y=210
    _lcd.setTextSize(1);
    _lcd.setTextColor(_currentTheme.textColor, _currentTheme.bgColor);
    
    char currBuf[10];
    char totalBuf[10];
    sprintf(currBuf, "%02d:%02d", current/60, current%60);
    sprintf(totalBuf, "%02d:%02d", total/60, total%60);
    
    _lcd.setCursor(10, 210);
    _lcd.print(currBuf);
    
    int totalW = _lcd.textWidth(totalBuf);
    _lcd.setCursor(240 - 10 - totalW, 210);
    _lcd.print(totalBuf);
}

#endif
