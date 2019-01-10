#ifndef ShelfPlayback_h
#define ShelfPlayback_h

#include "ShelfPins.h"
#include <Adafruit_VS1053.h>
#include <ESP8266HTTPClient.h>


// Lower value means louder!
#define DEFAULT_VOLUME 10

// Number off consecutive http reconnects before giving up stream
#define MAX_RECONNECTS 10

enum PlaybackState {PLAYBACK_NO, PLAYBACK_FILE, PLAYBACK_HTTP};

class ShelfPlayback {
  public:
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
    PlaybackState playbackState() {return _playing;};
    String currentFile() {return _currentFile;};
    void work();
    String currentStreamUrl;
    bool playingByCard = true;
  private:
    uint8_t _volume = DEFAULT_VOLUME;
    PlaybackState _playing = PLAYBACK_NO;
    String _currentFile;
    uint8_t _reconnectCount = 0;
    HTTPClient _http;
    WiFiClient * _stream;
    Adafruit_VS1053_FilePlayer _musicPlayer = Adafruit_VS1053_FilePlayer(BREAKOUT_RESET, BREAKOUT_CS, BREAKOUT_DCS, DREQ, SD_CS);
    bool patchVS1053(void);
    void feedPlaybackFromHttp(void);
};

#endif // ShelfPlayback_h
