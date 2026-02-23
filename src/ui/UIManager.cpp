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
    // Draw static elements
    _lcd.drawRect(10, 200, 220, 10, _currentTheme.textColor);
}

void UIManager::drawStatusBar() {
    _lcd.setTextSize(1);
    _lcd.fillRect(0, 0, 240, 24, _currentTheme.statusBgColor);
    // Don't print static text that will be partially overwritten
}

void UIManager::drawMainArea() {
    // Rectangular Spectrum
    // No static elements needed, just clear the area
    // Area: Y=40 to Y=150. X=20 to X=220.
    // _lcd.fillRect(20, 40, 200, 110, _currentTheme.bgColor);
}

void UIManager::updateVisualizer() {
    // Handle Scrolling Text here too
    updateScrollingText();

    if (!_lastIsPlaying) return;
    
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate < 30) return; // ~33fps for smoother animation
    lastUpdate = millis();
    
    int startX = 20;
    int startY = 140; 
    int maxH = 80;
    int bars = 16;
    int barW = 8;
    int gap = 4;
    
    static int currentHeights[16] = {0};
    
    for (int i = 0; i < bars; i++) {
        // Generate a target height
        // To make it less random, we can mix in some coherent noise or just smooth the random
        int target = random(5, maxH);
        
        // Physics simulation:
        // 1. Fast rise (kick)
        // 2. Slow decay (gravity)
        
        if (target > currentHeights[i]) {
            // Rise: Move 40% towards target (smooth rise) or instant?
            // Instant rise looks more energetic for beats
            // Smooth rise looks more "flowy"
            currentHeights[i] = (currentHeights[i] * 2 + target) / 3;
        } else {
            // Decay: Drop by fixed amount (gravity)
            currentHeights[i] -= 3;
        }
        
        // Clamp
        if (currentHeights[i] < 2) currentHeights[i] = 2;
        if (currentHeights[i] > maxH) currentHeights[i] = maxH;
        
        int h = currentHeights[i];
        int x = startX + i * (barW + gap);
        
        // Redraw bar
        // Clear full slot first to avoid artifacts (simplest way, though slightly more flicker risk)
        // Or smart clear
        _lcd.fillRect(x, startY - maxH, barW, maxH, _currentTheme.bgColor);
        _lcd.fillRect(x, startY - h, barW, h, _currentTheme.highlightColor);
    }
}

void UIManager::updateSongInfo(String filename, int index, int total) {
    _lcd.setTextSize(1);
    
    // Clean filename
    int slashIdx = filename.lastIndexOf('/');
    if (slashIdx >= 0) filename = filename.substring(slashIdx + 1);
    
    _lastSongName = filename;
    _lcd.setTextSize(1);
    _lcd.setTextColor(_currentTheme.textColor, _currentTheme.bgColor);
    
    // Calculate width
    _songNameWidth = _lcd.textWidth(filename);
    
    // Reset Scroll State
    _scrollState = 0;
    _scrollWaitStart = millis();
    
    if (_songNameWidth <= 240) {
        // Center static
        _scrollX = (240 - _songNameWidth) / 2;
        if (_scrollX < 0) _scrollX = 0;
    } else {
        // Prepare scroll
        _scrollX = 10; // Start with some padding
    }
    
    // Draw initial state
    // Clear song name area
    _lcd.fillRect(0, 160, 240, 30, _currentTheme.bgColor);
    _lcd.setCursor(_scrollX, 170);
    _lcd.print(filename);
    
    // Index
    if (total > 0) {
        String idxStr = String(index) + "/" + String(total);
        int w = _lcd.textWidth(idxStr);
        // Right align at 235
        int idxX = 235 - w;
        
        // Clear index area (170 to 240) to be safe from Volume overdraw
        _lcd.fillRect(170, 5, 70, 14, _currentTheme.statusBgColor); 
        
        _lcd.setCursor(idxX, 5); // Update in status bar
        _lcd.setTextColor(_currentTheme.textColor, _currentTheme.statusBgColor);
        _lcd.print(idxStr);
    }
}

void UIManager::updateScrollingText() {
    if (_songNameWidth <= 240) return; // No need to scroll
    
    unsigned long now = millis();
    if (now - _lastScrollUpdate < 50) return; // 20fps
    _lastScrollUpdate = now;
    
    bool needRedraw = false;
    
    switch (_scrollState) {
        case 0: // Wait Start
            if (now - _scrollWaitStart > 2000) {
                _scrollState = 1;
            }
            break;
            
        case 1: // Scrolling
            _scrollX -= 2; // Speed
            needRedraw = true;
            
            // Check if end reached
            // Visible width 240. Text ends at _scrollX + _songNameWidth
            // We want to scroll until text is fully gone or just end visible?
            // "Marquee" usually scrolls until end is visible + padding
            // Let's scroll until end is at 230 (right padding 10)
            if (_scrollX + _songNameWidth < 230) {
                _scrollState = 2;
                _scrollWaitStart = now;
            }
            break;
            
        case 2: // Wait End
            if (now - _scrollWaitStart > 2000) {
                _scrollState = 3;
            }
            break;
            
        case 3: // Reset
            _scrollX = 10;
            _scrollState = 0;
            _scrollWaitStart = now;
            needRedraw = true;
            break;
    }
    
    if (needRedraw) {
        // Redraw text line
        // We must clip or clear carefully
        // Y=170, H=20
        _lcd.fillRect(0, 160, 240, 30, _currentTheme.bgColor);
        _lcd.setCursor(_scrollX, 170);
        _lcd.setTextColor(_currentTheme.textColor, _currentTheme.bgColor);
        _lcd.print(_lastSongName);
    }
}

void UIManager::updateStatus(String modeName, int volume, bool isPlaying) {
    _lcd.setTextSize(1);
    _lastMode = modeName;
    _lastVolume = volume;
    _lastIsPlaying = isPlaying;
    
    // Mode in Status Bar
    // CLEAN EVERYTHING from 0 to 120 (Mode + Middle Gap)
    // This ensures no artifacts like ".8" or "." remain
    _lcd.fillRect(0, 0, 120, 24, _currentTheme.statusBgColor); 
    
    _lcd.setCursor(5, 5);
    _lcd.setTextColor(_currentTheme.textColor, _currentTheme.statusBgColor);
    _lcd.print(modeName);
    
    // Play Icon - Moved to Bottom Middle
    // Draw at bottom middle (between times)
    // Y=215 is text top. Icon can be Y=212.
    int iconX = 114;
    int iconY = 212;
    // Clear bottom icon area
    _lcd.fillRect(iconX, iconY, 12, 12, _currentTheme.bgColor);
    
    if (isPlaying) {
        // Play Triangle
        _lcd.fillTriangle(iconX, iconY, iconX, iconY+12, iconX+12, iconY+6, _currentTheme.highlightColor);
    } else {
        // Pause Bars
        _lcd.fillRect(iconX, iconY, 4, 12, _currentTheme.textColor);
        _lcd.fillRect(iconX+8, iconY, 4, 12, _currentTheme.textColor);
    }
    
    // Volume
    updateVolume(volume);
}

void UIManager::updateVolume(int volume) {
    _lcd.setTextSize(1);
    int volX = 120;
    // Reduce clear width to avoid clearing Index
    _lcd.fillRect(volX, 5, 50, 14, _currentTheme.statusBgColor); // Clear middle-right (120-170)
    
    // Draw Speaker Icon at volX
    // Small rect + Triangle
    // Rect: 120, 8, 3, 6 (Center Y=11)
    // Triangle: 123, 8, 123, 14, 128, 5, 128, 17? Too big.
    // Fit in 12px height.
    
    int icoY = 6;
    _lcd.fillRect(volX, icoY + 3, 2, 6, _currentTheme.textColor); // Box part
    _lcd.fillTriangle(volX + 2, icoY + 3, volX + 2, icoY + 9, volX + 8, icoY, _currentTheme.textColor); // Triangle part
    _lcd.fillTriangle(volX + 2, icoY + 9, volX + 8, icoY + 12, volX + 8, icoY, _currentTheme.textColor); // Bottom half of triangle?
    // Wait, fillTriangle takes 3 points.
    // Top triangle: (x+2, y+3), (x+2, y+9) <- this is vertical line.
    // Speaker shape:
    //   | /
    // []|/
    //   | \
    
    // Let's use a simpler polygon or 2 triangles.
    // Point 1: (x+2, y+3)
    // Point 2: (x+2, y+9)
    // Point 3: (x+7, y+1) -> Top right
    // Point 4: (x+7, y+11) -> Bottom right
    
    _lcd.fillTriangle(volX + 2, icoY + 6, volX + 7, icoY + 1, volX + 7, icoY + 11, _currentTheme.textColor);
    // Overlap the rect
    
    // Sound waves if vol > 0
    if (volume > 0) {
        _lcd.drawLine(volX + 9, icoY + 4, volX + 9, icoY + 8, _currentTheme.textColor);
    }
    if (volume > 5) {
         _lcd.drawLine(volX + 11, icoY + 2, volX + 11, icoY + 10, _currentTheme.textColor);
    }
    
    // Text
    _lcd.setCursor(volX + 15, 5);
    _lcd.setTextColor(_currentTheme.textColor, _currentTheme.statusBgColor);
    _lcd.print(volume);
}

void UIManager::updateProgress(int current, int total) {
    if (total <= 0) return;
    
    float pct = (float)current / total;
    if (pct > 1.0) pct = 1.0;
    
    int w = 218 * pct;
    _lcd.fillRect(11, 201, w, 8, _currentTheme.progressFillColor);
    _lcd.fillRect(11 + w, 201, 218 - w, 8, _currentTheme.progressBgColor);
    
    // Time text
    _lcd.setTextSize(1);
    _lcd.setTextColor(_currentTheme.textColor, _currentTheme.bgColor);
    
    char currBuf[10];
    char totalBuf[10];
    sprintf(currBuf, "%02d:%02d", current/60, current%60);
    sprintf(totalBuf, "%02d:%02d", total/60, total%60);
    
    // Clear the time area first (optional, but good for anti-flicker if background matches)
    // Actually fillRect might flicker if done every second, but text overwrite is safer with background color
    
    // Current time on left
    _lcd.setCursor(10, 215);
    _lcd.print(currBuf);
    
    // Total time on right
    int totalW = _lcd.textWidth(totalBuf);
    _lcd.setCursor(240 - 10 - totalW, 215);
    _lcd.print(totalBuf);
}

#endif
