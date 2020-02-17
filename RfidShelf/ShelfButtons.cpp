#include "ShelfButtons.h"

// This sucks - Maybe refactor ShelfButtons to singleton
ShelfButtons *ShelfButtons::_instance;

ShelfButtons::ShelfButtons(ShelfPlayback &playback) : _playback(playback), _pauseButton(PAUSE_BTN, 50, true, false), _skipButton(SKIP_BTN, 50, true, false) {
    _instance = this;
};

void ShelfButtons::begin() {
  _lastAnalogVolume = ShelfConfig::config.defaultVolumne;
  _pauseButton.begin();
  _pauseButton.onPressed(handlePause);
  _skipButton.begin();
  _skipButton.onPressed(handleSkip);
}

void ShelfButtons::work() {
  _pauseButton.read();
  _skipButton.read();
  _handleVolume();
}

void ShelfButtons::handlePause() {
  _instance->_playback.togglePause();
}

void ShelfButtons::handleSkip() {
  _instance->_playback.skipFile();
}

void ShelfButtons::_handleVolume() {
  // Note: Analog volume always wins! Volume from cards/web will be ignored/overwritten

  if (millis() - _lastAnalogCheck < 500) {
    return;
  }
  _lastAnalogCheck = millis();

  uint8_t volume_new = analogRead(VOLUME) / 20; // Map 1024 roughly into our 0-50 volume
  if(volume_new != _lastAnalogVolume  || _lastAnalogVolume != _playback.volume()) {
    _playback.volume(volume_new);
    _lastAnalogVolume = _playback.volume();
  }
}