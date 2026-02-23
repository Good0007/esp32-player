#include "InputManager.h"
#include "config.h"

InputManager::InputManager() {
    btnMode = new OneButton(MODEL_BUTTON_GPIO, true, true);
    btnVolUp = new OneButton(VOLUME_UP_BUTTON_GPIO, true, true);
    btnVolDown = new OneButton(VOLUME_DOWN_BUTTON_GPIO, true, true);
}

void InputManager::begin() {
    // Mode Button
    btnMode->attachClick([](void *scope) {
        InputManager *self = (InputManager *)scope;
        if (self->_playPauseCb) self->_playPauseCb();
    }, this);

    btnMode->attachDoubleClick([](void *scope) {
        InputManager *self = (InputManager *)scope;
        if (self->_modeDoubleCb) self->_modeDoubleCb();
    }, this);

    btnMode->attachLongPressStart([](void *scope) {
        InputManager *self = (InputManager *)scope;
        if (self->_funcLongPressCb) self->_funcLongPressCb();
    }, this);

    // Volume Up
    btnVolUp->attachClick([](void *scope) {
        InputManager *self = (InputManager *)scope;
        if (self->_volUpCb) self->_volUpCb();
    }, this);
    
    btnVolUp->attachDoubleClick([](void *scope) {
        InputManager *self = (InputManager *)scope;
        if (self->_nextSongCb) self->_nextSongCb();
    }, this);

    btnVolUp->attachLongPressStart([](void *scope) {
        InputManager *self = (InputManager *)scope;
        if (self->_nextModeCb) self->_nextModeCb();
    }, this);

    // Volume Down
    btnVolDown->attachClick([](void *scope) {
        InputManager *self = (InputManager *)scope;
        if (self->_volDownCb) self->_volDownCb();
    }, this);
    
    btnVolDown->attachDoubleClick([](void *scope) {
        InputManager *self = (InputManager *)scope;
        if (self->_prevSongCb) self->_prevSongCb();
    }, this);

    btnVolDown->attachLongPressStart([](void *scope) {
        InputManager *self = (InputManager *)scope;
        if (self->_prevModeCb) self->_prevModeCb();
    }, this);
}

void InputManager::loop() {
    btnMode->tick();
    btnVolUp->tick();
    btnVolDown->tick();
}

void InputManager::onPlayPause(Callback cb) { _playPauseCb = cb; }
void InputManager::onModeDoubleClick(Callback cb) { _modeDoubleCb = cb; }
void InputManager::onFunctionLongPress(Callback cb) { _funcLongPressCb = cb; }
void InputManager::onVolumeUp(Callback cb) { _volUpCb = cb; }
void InputManager::onVolumeDown(Callback cb) { _volDownCb = cb; }
void InputManager::onNextSong(Callback cb) { _nextSongCb = cb; }
void InputManager::onPrevSong(Callback cb) { _prevSongCb = cb; }
void InputManager::onNextMode(Callback cb) { _nextModeCb = cb; }
void InputManager::onPrevMode(Callback cb) { _prevModeCb = cb; }
