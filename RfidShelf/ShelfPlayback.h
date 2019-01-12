#ifndef ShelfPlayback_h
#define ShelfPlayback_h

#include "ShelfPins.h"
#include <Adafruit_VS1053.h>
#include <ESP8266HTTPClient.h>
#include <SdFat.h>

// Lower value means louder!
#define DEFAULT_VOLUME 10

// Number off consecutive http reconnects before giving up stream
#define MAX_RECONNECTS 10

// bass enhancer settings
// treble amplitude in 1.5 dB steps (-8..7, 0 = off)
#define TREBLE_AMPLITUDE 0
// treble lower limit frequency in 1000 Hz steps (1..15)
#define TREBLE_FREQLIMIT 0
// bass enhancement in 1 dB steps (0..15, 0 = off)
#define BASS_AMPLITUDE 10
// bass lower limit frequency in 10 Hz steps (2..15)
#define BASS_FREQLIMIT 15

enum PlaybackState {PLAYBACK_NO, PLAYBACK_FILE, PLAYBACK_HTTP};

class ShelfPlayback {
  public:
    ShelfPlayback(SdFat &sd) :
      _musicPlayer(Adafruit_VS1053_FilePlayer(BREAKOUT_RESET, BREAKOUT_CS, BREAKOUT_DCS, DREQ, SD_CS)),
      _SD(sd)
      {};
    void begin();
    bool switchFolder(const char *folder);
    void switchStreamUrl(const String);
    void playFile();
    void playHttp();
    void stopPlayback();
    uint8_t volume() {return _volume;};
    void volume(uint8_t volume);
    void volumeDown();
    void volumeUp();
    void setBassAndTreble(uint8_t trebleAmplitude, uint8_t trebleFreqLimit, uint8_t bassAmplitude, uint8_t bassFreqLimit);
    PlaybackState playbackState() {return _playing;};
    String currentFile() {return _currentFile;};
    void work();
    String currentStreamUrl;
    bool playingByCard = true;
  private:
    uint8_t _volume = DEFAULT_VOLUME;
    PlaybackState _playing = PLAYBACK_NO;
    Adafruit_VS1053_FilePlayer _musicPlayer;
    SdFat &_SD;
    String _currentFile;
    uint8_t _reconnectCount = 0;
    HTTPClient _http;
    WiFiClient * _stream;
    bool patchVS1053();
    void feedPlaybackFromHttp();
};

#endif // ShelfPlayback_h
