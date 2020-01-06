#ifndef ShelfButtons_h
#define ShelfButtons_h

#include <EasyButton.h>
#include "ShelfConfig.h"
#include "ShelfPins.h"
#include "ShelfPlayback.h"

class ShelfButtons {
  public:
    ShelfButtons(ShelfPlayback &playback);
    void begin();
    void work();
    static void handlePause();
    static void handleSkip();
  private:
    static ShelfButtons *_instance;
    ShelfPlayback &_playback;
    unsigned long _lastAnalogCheck = 0L;
    EasyButton _pauseButton;
    EasyButton _skipButton;
    uint8_t _lastAnalogVolume = DEFAULT_VOLUME;
    void _handleVolume();
};

#endif // ShelfButtons_h