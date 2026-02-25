#include <Arduino.h>
#include "PlaylistManager.h"
#include <algorithm>
#include <random>
#include <SD.h> // Ensure SD access

PlaylistManager::PlaylistManager() : _currentModeIndex(-1), _currentSongIndex(-1) {}

void PlaylistManager::addMode(String path) {
    _modes.push_back(path);
}

void PlaylistManager::setMode(int index) {
    if (_modes.empty()) return;
    
    // Validate index
    if (index < 0) index = _modes.size() - 1;
    if (index >= (int)_modes.size()) index = 0;
    
    if (_currentModeIndex != index) {
        _currentModeIndex = index;
        
        // Save to NVS
        _prefs.begin("playlist", false);
        _prefs.putInt("mode", _currentModeIndex);
        _prefs.end();
    }
    
    // Clear playlist
    _playlist.clear();
    // Pre-reserve for large dirs
    _playlist.reserve(1000);
    
    Serial.printf("Switching to mode: %s\n", _modes[_currentModeIndex].c_str());
    
    // Try to load cache first
    if (!loadCache(_currentModeIndex)) {
        Serial.println("Cache miss, full scanning SD...");
        
        // Full scan
        // Use stored path (now includes slash from config.h)
        scan(SD, _modes[_currentModeIndex].c_str(), 2);
        
        // Save cache immediately
        saveCache(_currentModeIndex);
    } else {
        Serial.println("Cache hit!");
    }
    
    // Shuffle
    shuffle();
    printList();
}



void PlaylistManager::loadMode() {
    _prefs.begin("playlist", true); // Read-only
    int savedIndex = _prefs.getInt("mode", 0); // Default to 0
    _prefs.end();
    
    Serial.printf("Loading saved mode index: %d\n", savedIndex);
    setMode(savedIndex);
}

void PlaylistManager::nextMode() {
    setMode(_currentModeIndex + 1);
}

void PlaylistManager::prevMode() {
    setMode(_currentModeIndex - 1);
}

String PlaylistManager::getCurrentModeName() {
    if (_currentModeIndex >= 0 && _currentModeIndex < (int)_modes.size()) {
        String name = _modes[_currentModeIndex];
        if (name.startsWith("/")) {
            return name.substring(1);
        }
        return name;
    }
    return "Unknown";
}

void PlaylistManager::saveCache(int modeIndex) {
    if (_playlist.empty()) return;
    
    String cacheFile = "/.playlist_cache_" + String(modeIndex) + ".txt";
    File f = SD.open(cacheFile.c_str(), FILE_WRITE);
    if (!f) {
        Serial.println("Failed to save cache");
        return;
    }
    
    for (const auto& song : _playlist) {
        f.println(song);
    }
    f.close();
    Serial.println("Cache saved.");
}

bool PlaylistManager::loadCache(int modeIndex) {
    String cacheFile = "/.playlist_cache_" + String(modeIndex) + ".txt";
    if (!SD.exists(cacheFile.c_str())) return false;
    
    File f = SD.open(cacheFile.c_str());
    if (!f) return false;
    
    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
            _playlist.push_back(line);
        }
    }
    f.close();
    
    if (_playlist.empty()) return false;
    return true;
}

void PlaylistManager::clearCache() {
    // Helper to delete all cache files
    for (int i = 0; i < (int)_modes.size(); i++) {
        String cacheFile = "/.playlist_cache_" + String(i) + ".txt";
        if (SD.exists(cacheFile.c_str())) {
            SD.remove(cacheFile.c_str());
        }
    }
    Serial.println("Cache cleared!");
}

void PlaylistManager::scan(fs::FS &fs, const char *dirname, uint8_t levels) {
    // Reserve memory to avoid reallocations
    if (_playlist.capacity() < 1000) {
        _playlist.reserve(1000);
    }

    File root = fs.open(dirname);
    if (!root) {
        Serial.println("Failed to open directory");
        return;
    }
    if (!root.isDirectory()) {
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            if (levels) {
                scan(fs, file.path(), levels - 1);
            }
        } else {
            String filename = String(file.name());
            
            // Filter hidden files (start with .)
            if (filename.startsWith(".") || filename.indexOf("/.") != -1) {
                file = root.openNextFile();
                continue;
            }

            // Some FS implementations return full path, others just name.
            // Using file.path() is safer on ESP32 Arduino 2.0+
            String path = String(file.path());
            
            if (isAudioFile(path)) {
                _playlist.push_back(path);
                // Serial.printf("Found: %s\n", path.c_str());
            }
        }
        file = root.openNextFile();
        
        // Feed watchdog
        yield();
    }
}

bool PlaylistManager::isAudioFile(String filename) {
    filename.toLowerCase();
    return filename.endsWith(".mp3") || 
           filename.endsWith(".aac") || 
           filename.endsWith(".m4a") || 
           filename.endsWith(".flac") || 
           filename.endsWith(".ogg") ||
           filename.endsWith(".wav");
}

void PlaylistManager::shuffle() {
    if (_playlist.empty()) return;
    
    // Use ESP32 hardware random number generator
    std::random_device rd;
    std::mt19937 g(esp_random());
    std::shuffle(_playlist.begin(), _playlist.end(), g);
    
    _currentSongIndex = -1;
    Serial.println("Playlist shuffled");
}

String PlaylistManager::next() {
    if (_playlist.empty()) return "";
    
    _currentSongIndex++;
    if (_currentSongIndex >= (int)_playlist.size()) {
        // Option 1: Loop back to 0 without shuffle
        // _currentSongIndex = 0;
        
        // Option 2: Reshuffle and start from 0
        shuffle(); 
        _currentSongIndex = 0; 
    }
    
    return _playlist[_currentSongIndex];
}

String PlaylistManager::prev() {
    if (_playlist.empty()) return "";
    
    if (_currentSongIndex == 0) {
        _currentSongIndex = _playlist.size() - 1;
    } else {
        _currentSongIndex--;
    }
    
    return _playlist[_currentSongIndex];
}

void PlaylistManager::remove(String path) {
    for (size_t i = 0; i < _playlist.size(); i++) {
        if (_playlist[i] == path) {
            _playlist.erase(_playlist.begin() + i);
            
            // Adjust index if we removed an element before the current index
            if (i < _currentSongIndex) {
                _currentSongIndex--;
            }
            // If we removed the element AT the current index, the next element shifts down.
            // The index stays the same, but now points to the new element.
            // We only need to clamp if we are now out of bounds (which happens if we removed the last element)
            if (_currentSongIndex >= _playlist.size() && _currentSongIndex > 0) {
                 _currentSongIndex = 0; // Wrap around or reset
            }
            
            Serial.printf("Removed missing file from playlist: %s\n", path.c_str());
            return; // Found and removed, exit
        }
    }
}

size_t PlaylistManager::count() const {
    return _playlist.size();
}

void PlaylistManager::printList() {
    Serial.printf("Total songs: %d\n", _playlist.size());
    // for (const auto& song : _playlist) {
    //     Serial.println(song);
    // }
}
