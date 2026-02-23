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
        Serial.println("Playlist empty!");
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
    }
}

void nextMode() {
    blinkLED(2, 0, 0, 16); // Blink Blue
    #ifdef ENABLE_DISPLAY
    ui.showLoading("Loading...");
    #endif
    playlist.nextMode();
    playNext();
}

void prevMode() {
    blinkLED(2, 0, 0, 16); // Blink Blue
    #ifdef ENABLE_DISPLAY
    ui.showLoading("Loading...");
    #endif
    playlist.prevMode();
    playNext();
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
    } else {
        Serial.println("PSRAM init failed!");
    }

    // SPI & SD Setup
    SPI.begin(SD_CLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    // Increase SPI frequency to 20MHz for faster scanning and reading
    if (!SD.begin(SD_CS_PIN, SPI, 20000000)) {
        Serial.println("SD Mount Failed");
        return;
    }
    
    // Setup Modes
    playlist.addMode("/儿歌");
    playlist.addMode("/古诗");
    playlist.addMode("/故事");
    playlist.addMode("/音乐");
    
    // Load last mode
    #ifdef ENABLE_DISPLAY
    ui.showLoading("Loading...");
    #endif
    playlist.loadMode();

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
    input.onNextMode(nextMode);
    input.onPrevMode(prevMode);
    
    input.onModeDoubleClick(toggleLed);
    input.onFunctionLongPress(switch_to_other_app);

    input.begin();

    // Start Playback
    blinkLED(3, 0, 16, 0); // Blink Green (Success)
    
    #ifdef ENABLE_DISPLAY
    ui.updateStatus(playlist.getCurrentModeName(), currentVolume, true);
    #endif
    
    playNext();
}

void loop() {
    audio.loop();
    input.loop();
    // playlist.loop(); // Removed background scan
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
