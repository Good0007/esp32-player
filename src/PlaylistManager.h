#pragma once

#include <Arduino.h>
#include <vector>
#include <FS.h>
#include <SD.h>
#include <Preferences.h>

class PlaylistManager {
public:
    PlaylistManager();
    
    // Add directory to scan
    void addMode(String path);
    void setMode(int index);
    void loadMode(); // Load from NVS
    void nextMode();
    void prevMode();
    String getCurrentModeName();
    
    // Cache Management
    void saveCache(int modeIndex);
    bool loadCache(int modeIndex);
    void clearCache(); // Force rescan helper

    // Playback
    void shuffle();
    String next();
    String prev(); // Add previous song support
    void remove(String path);
    size_t count() const;
    size_t getCurrentIndex() const { return _currentSongIndex; }
    size_t getModeCount() const { return _modes.size(); }
    void printList();

private:
    void scan(fs::FS &fs, const char *dirname, uint8_t levels);
    bool isAudioFile(String filename);

    std::vector<String> _playlist;
    std::vector<String> _modes; // Paths like "/music", "/story"
    int _currentModeIndex;
    size_t _currentSongIndex;
    Preferences _prefs;
};
