#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include <vector>
#include <Preferences.h>
#include "Audio.h"
#include "config.h"
#include "PlaylistManager.h"
#include "InputManager.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "ui/UIManager.h"

// Globals
Audio audio;
PlaylistManager playlist;
InputManager input;
Preferences prefs;

// Async mode-switch flags (set in button callbacks, processed in loop)
static volatile bool g_nextModeRequest = false;
static volatile bool g_prevModeRequest = false;

// Volume state
int currentVolume = 5; // Default 5
bool isLedEnabled = true;
uint8_t ledHue = 0;
unsigned long lastLedUpdate = 0;

// Helper for RGB LED
void blinkLED(int times, uint8_t r, uint8_t g, uint8_t b) {
    if (!isLedEnabled) return;
    for (int i = 0; i < times; i++) {
        neopixelWrite(BUILTIN_LED_GPIO, r, g, b);
        delay(100);
        neopixelWrite(BUILTIN_LED_GPIO, 0, 0, 0);
        delay(100);
    }
}

// Simple HSV to RGB (Hue 0-255, Sat 255, Val 255)
void setRainbowColor(uint8_t wheelPos) {
    wheelPos = 255 - wheelPos;
    if (wheelPos < 85) {
        neopixelWrite(BUILTIN_LED_GPIO, 255 - wheelPos * 3, 0, wheelPos * 3);
    } else if (wheelPos < 170) {
        wheelPos -= 85;
        neopixelWrite(BUILTIN_LED_GPIO, 0, wheelPos * 3, 255 - wheelPos * 3);
    } else {
        wheelPos -= 170;
        neopixelWrite(BUILTIN_LED_GPIO, wheelPos * 3, 255 - wheelPos * 3, 0);
    }
}

void updateLED() {
    if (!isLedEnabled) {
        neopixelWrite(BUILTIN_LED_GPIO, 0, 0, 0);
        return;
    }
    
    if (audio.isRunning()) {
        unsigned long now = millis();
        if (now - lastLedUpdate > 50) { // Update every 50ms
            lastLedUpdate = now;
            setRainbowColor(ledHue++);
        }
    } else {
        neopixelWrite(BUILTIN_LED_GPIO, 0, 0, 0); // Off when paused
    }
}

void toggleLed() {
    isLedEnabled = !isLedEnabled;
    Serial.printf("LED Enabled: %d\n", isLedEnabled);
    
    prefs.begin("settings", false);
    prefs.putBool("led", isLedEnabled);
    prefs.end();
    
    if (isLedEnabled) {
        blinkLED(1, 0, 255, 0); // Blink Green once
    } else {
        blinkLED(1, 255, 0, 0); // Blink Red once
        neopixelWrite(BUILTIN_LED_GPIO, 0, 0, 0); // Force off
    }
}

void playNext() {
    // Safety check to prevent infinite loop if all files are missing
    static int skipCount = 0;
    if (skipCount > 10) {
        Serial.println("Too many missing files, stopping playback.");
        skipCount = 0;
        return;
    }

    String nextFile = playlist.next();
    if (nextFile.length() > 0) {
        if (SD.exists(nextFile)) {
            Serial.printf("Playing: %s\n", nextFile.c_str());
            
            #ifdef ENABLE_DISPLAY
            ui.updateSongInfo(nextFile, playlist.getCurrentIndex() + 1, playlist.count());
            ui.updateStatus(playlist.getCurrentModeName(), currentVolume, true);
            #endif
            
            audio.connecttoFS(SD, nextFile.c_str());
            skipCount = 0; // Reset counter on success
        } else {
            Serial.printf("File missing: %s, removing from playlist...\n", nextFile.c_str());
            playlist.remove(nextFile);
            skipCount++;
            playNext(); // Recursive skip
        }
    } else {
        Serial.println("Playlist empty! Auto-switching to next mode...");
        // 空列表时自动切换到下一个模式，避免系统无响应
        static int emptyModeCount = 0;
        if (emptyModeCount < (int)playlist.getModeCount()) {
            emptyModeCount++;
            playlist.nextMode();
            skipCount = 0;
            playNext();
        } else {
            // 所有模式都是空的
            emptyModeCount = 0;
            Serial.println("All playlists empty!");
            #ifdef ENABLE_DISPLAY
            ui.updateSongInfo("No Music Found", 0, 0);
            ui.updateStatus(playlist.getCurrentModeName(), currentVolume, false);
            #endif
        }
    }
}

void playPrev() {
    // Similar safety check for prev
    static int skipCount = 0;
    if (skipCount > 10) {
        Serial.println("Too many missing files, stopping playback.");
        skipCount = 0;
        return;
    }

    String prevFile = playlist.prev();
    if (prevFile.length() > 0) {
        if (SD.exists(prevFile)) {
            Serial.printf("Playing: %s\n", prevFile.c_str());
            
            #ifdef ENABLE_DISPLAY
            ui.updateSongInfo(prevFile, playlist.getCurrentIndex() + 1, playlist.count());
            ui.updateStatus(playlist.getCurrentModeName(), currentVolume, true);
            #endif
            
            audio.connecttoFS(SD, prevFile.c_str());
            skipCount = 0;
        } else {
            Serial.printf("File missing: %s, removing from playlist...\n", prevFile.c_str());
            playlist.remove(prevFile);
            skipCount++;
            playPrev();
        }
    } else {
        #ifdef ENABLE_DISPLAY
        ui.updateSongInfo("No Music Found", 0, 0);
        #endif
    }
}

void nextMode() {
    // 不在此处 blinkLED（含 delay），避免阻塞 input.loop()
    #ifdef ENABLE_DISPLAY
    ui.showLoading("Loading...");
    #endif
    playlist.nextMode();
    playNext();
    blinkLED(2, 0, 0, 16); // 切换完成后再闪灯
}

void prevMode() {
    #ifdef ENABLE_DISPLAY
    ui.showLoading("Loading...");
    #endif
    playlist.prevMode();
    playNext();
    blinkLED(2, 0, 0, 16);
}

void switch_to_other_app() {
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *target = NULL;
    
    // Find partition at specific address 0x20000
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
    while (it != NULL) {
        const esp_partition_t *p = esp_partition_get(it);
        if (p->address == 0x20000) {
            target = p;
            esp_partition_iterator_release(it); // Don't forget to release iterator
            break;
        }
        it = esp_partition_next(it);
    }
    
    // Safety check: Don't switch to self if we are already running at 0x20000
    if (target != NULL && running->address == target->address) {
        Serial.println("Already running at 0x20000 partition!");
        return;
    }

    if (target != NULL) {
        Serial.printf("Switching to partition at 0x%X...\n", target->address);
        delay(100);

        esp_err_t err = esp_ota_set_boot_partition(target);
        if (err == ESP_OK) {
            Serial.println("Partition switch success, restarting...");
            delay(1000);
            ESP.restart();
        } else {
            Serial.printf("Failed to set boot partition: %s\n", esp_err_to_name(err));
        }
    } else {
        Serial.println("No switchable partition found!");
    }
}

void setup() {
    Serial.begin(115200);
    
    #ifdef ENABLE_DISPLAY
    // Init UI first to show boot status
    ui.begin();
    ui.updateStatus("Booting...", 0, false);
    #endif
    
    // PSRAM Check
    if (psramInit()) {
        Serial.printf("PSRAM initialized. Free: %d bytes\n", ESP.getFreePsram());
        // Do NOT set custom buffer size as it causes AAC decoder crashes and overflows
        // audio.setBufsize(30000, 4096); 
    } else {
        Serial.println("PSRAM init failed!");
    }

    // SPI & SD Setup
    SPI.begin(SD_CLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    // Increase SPI frequency to 20MHz for faster scanning and reading
    bool sdSuccess = SD.begin(SD_CS_PIN, SPI, 20000000);
    if (!sdSuccess) {
        Serial.println("SD Mount Failed");
        #ifdef ENABLE_DISPLAY
        ui.showLoading("请插入SD卡");
        #endif
    } else {
        // Setup Modes
        playlist.addMode(PLAYLIST_DIR_CHILDREN);
        playlist.addMode(PLAYLIST_DIR_MUSIC);
        playlist.addMode(PLAYLIST_DIR_POEM);
        playlist.addMode(PLAYLIST_DIR_STORY);
        
        // Load last mode
        #ifdef ENABLE_DISPLAY
        ui.showLoading("Loading...");
        #endif
        playlist.loadMode();
    }

    // Load Volume & LED
    prefs.begin("settings", false);
    currentVolume = prefs.getInt("volume", 10);
    isLedEnabled = prefs.getBool("led", true);
    prefs.end();
    
    // Audio Setup
    audio.setPinout(AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT);
    audio.setVolume(currentVolume);

    // Input Setup
    input.onPlayPause([]() {
        // Always try to toggle pause/resume regardless of state
        // The library handles internal state checks
        audio.pauseResume();
        Serial.println("Pause/Resume");
        #ifdef ENABLE_DISPLAY
        ui.updateStatus(playlist.getCurrentModeName(), currentVolume, audio.isRunning());
        #endif
    });

    input.onVolumeUp([]() {
        if (currentVolume < 21) {
            currentVolume++;
            audio.setVolume(currentVolume);
            Serial.printf("Volume: %d\n", currentVolume);
            
            prefs.begin("settings", false);
            prefs.putInt("volume", currentVolume);
            prefs.end();
            
            #ifdef ENABLE_DISPLAY
            ui.updateVolume(currentVolume);
            #endif
        }
    });

    input.onVolumeDown([]() {
        if (currentVolume > 0) {
            currentVolume--;
            audio.setVolume(currentVolume);
            Serial.printf("Volume: %d\n", currentVolume);
            
            prefs.begin("settings", false);
            prefs.putInt("volume", currentVolume);
            prefs.end();
            
            #ifdef ENABLE_DISPLAY
            ui.updateVolume(currentVolume);
            #endif
        }
    });

    input.onNextSong(playNext);
    input.onPrevSong(playPrev);
    // 使用标志位异步触发，避免在回调中直接扫描 SD 卡导致 input.loop() 长时间阻塞
    input.onNextMode([]() { g_nextModeRequest = true; });
    input.onPrevMode([]() { g_prevModeRequest = true; });
    
    input.onModeDoubleClick(toggleLed);
    input.onFunctionLongPress(switch_to_other_app);

    input.begin();

    // Start Playback only if SD is OK
    if (sdSuccess) {
        blinkLED(3, 0, 16, 0); // Blink Green (Success)
        
        #ifdef ENABLE_DISPLAY
        ui.updateStatus(playlist.getCurrentModeName(), currentVolume, true);
        #endif
        
        playNext();
    } else {
        blinkLED(3, 16, 0, 0); // Blink Red (Failure)
        
        #ifdef ENABLE_DISPLAY
        ui.updateStatus("", currentVolume, false);
        ui.showLoading("请插入SD卡");
        #endif
    }
}

void loop() {
    // input.loop() 必须最优先，保证 OneButton 时序不受 audio.loop() 耗时影响
    input.loop();

    // 异步处理模式切换（扫描 SD 会阻塞数百毫秒，不能在回调中直接做）
    if (g_nextModeRequest) {
        g_nextModeRequest = false;
        nextMode();
    }
    if (g_prevModeRequest) {
        g_prevModeRequest = false;
        prevMode();
    }

    audio.loop();
    updateLED();

    #ifdef ENABLE_DISPLAY
    ui.updateVisualizer();
    static unsigned long lastUIUpdate = 0;
    if (millis() - lastUIUpdate > 500) {
        lastUIUpdate = millis();
        if (audio.isRunning()) {
            ui.updateProgress(audio.getAudioCurrentTime(), audio.getAudioFileDuration());
            ui.updateBitrate(audio.getBitRate());
        }
    }
    #endif

    // Check if song finished (requires Audio library callback or polling)
    // The Audio library handles EOF by calling audio_eof_mp3 or generic eof.
    // We can use the callback mechanism.
}

// Audio Library Callbacks
void audio_eof_mp3(const char *info) {
    Serial.print("EOF: "); Serial.println(info);
    playNext();
}

void audio_eof_aac(const char *info) {
    Serial.print("EOF: "); Serial.println(info);
    playNext();
}

void audio_eof_stream(const char *info) {
    Serial.print("EOF: "); Serial.println(info);
    playNext();
}

void audio_eof_flac(const char *info) {
    Serial.print("EOF: "); Serial.println(info);
    playNext();
}

void audio_eof_speech(const char *info) {
    Serial.print("EOF: "); Serial.println(info);
    playNext();
}
