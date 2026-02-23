#pragma once

#include <Arduino.h>
#include <OneButton.h>
#include <functional>

class InputManager {
public:
    using Callback = std::function<void()>;

    InputManager();
    void begin();
    void loop();

    void onPlayPause(Callback cb);
    void onModeDoubleClick(Callback cb); // Double Click Mode
    void onFunctionLongPress(Callback cb); // Long press Mode button
    void onVolumeUp(Callback cb);
    void onVolumeDown(Callback cb);
    void onNextSong(Callback cb); // Double Click
    void onPrevSong(Callback cb); // Double Click
    void onNextMode(Callback cb); // Long Press
    void onPrevMode(Callback cb); // Long Press

private:
    OneButton *btnMode;
    OneButton *btnVolUp;
    OneButton *btnVolDown;

    Callback _playPauseCb;
    Callback _modeDoubleCb;
    Callback _funcLongPressCb;
    Callback _volUpCb;
    Callback _volDownCb;
    Callback _nextSongCb;
    Callback _prevSongCb;
    Callback _nextModeCb;
    Callback _prevModeCb;
};
