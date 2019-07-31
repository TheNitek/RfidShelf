#ifndef ShelfPlayback_h
#define ShelfPlayback_h

#include "ShelfPins.h"
#include "ShelfConfig.h"
#include <Adafruit_VS1053.h>
#include <ESP8266HTTPClient.h>
#include <SdFat.h>

enum PlaybackState {PLAYBACK_NO, PLAYBACK_FILE, PLAYBACK_PAUSED};

class ShelfPlayback {
  public:
    ShelfPlayback(SdFat &sd) :
      _musicPlayer(Adafruit_VS1053_FilePlayer(BREAKOUT_RESET, BREAKOUT_CS, BREAKOUT_DCS, DREQ, SD_CS)),
      _SD(sd)
      {};
    void begin();
    bool switchFolder(const char *folder);
    void switchStreamUrl(const String);
    void startPlayback();
    void startFilePlayback(const char* folder, const char* fullpath);
    void skipFile();
    void pausePlayback();
    void resumePlayback();
    void stopPlayback();
    uint8_t volume() {return _volume;};
    void volume(uint8_t volume);
    void volumeDown();
    void volumeUp();
    void startNight();
    bool isNight();
    void stopNight();
    void setBassAndTreble(uint8_t trebleAmplitude, uint8_t trebleFreqLimit, uint8_t bassAmplitude, uint8_t bassFreqLimit);
    PlaybackState playbackState() {return _playing;};
    String currentFile() {return _currentFile;};
    void work();
    bool playingByCard = true;
  private:
    uint8_t _volume = DEFAULT_VOLUME;
    PlaybackState _playing = PLAYBACK_NO;
    Adafruit_VS1053_FilePlayer _musicPlayer;
    SdFat &_SD;
    String _currentFile;
    bool patchVS1053();
    bool _nightMode = false;
    // Night mode timeouts NIGHT_TIMEOUT minutes after playback ends, so we need to keep count
    unsigned long _lastNightActivity;
};

#endif // ShelfPlayback_h
