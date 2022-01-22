#pragma once

#include <EasyButton.h>
#include "ShelfConfig.h"
#include "ShelfPins.h"
#include "ShelfPlayback.h"

class ShelfButtons {
  public:
    ShelfButtons(ShelfConfig::GlobalConfig &config, ShelfPlayback &playback);
    void begin();
    void work();
    static void handlePause();
    static void handleSkip();
  private:
    static ShelfButtons *_instance;
    ShelfConfig::GlobalConfig &_config;
    ShelfPlayback &_playback;
    unsigned long _lastAnalogCheck = 0L;
    EasyButton _pauseButton;
    EasyButton _skipButton;
    uint8_t _lastAnalogVolume;
    void _handleVolume();
};