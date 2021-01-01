#pragma once

#include "ShelfPins.h"
#include "ShelfConfig.h"
#include <Adafruit_VS1053.h>
#include <ESP8266HTTPClient.h>
#include <SdFat.h>
#include <BoolArray.h>

enum PlaybackState {PLAYBACK_NO, PLAYBACK_FILE, PLAYBACK_PAUSED};
typedef std::function<void(PlaybackState state, uint8_t volume)> PlaybackCallbackFunction;

class ShelfPlayback {
  public:
    ShelfPlayback(ShelfConfig &config, sdfat::SdFat &sd) :
      _config(config),
      _musicPlayer(Adafruit_VS1053_FilePlayer(BREAKOUT_RESET, BREAKOUT_CS, BREAKOUT_DCS, DREQ, SD_CS)),
      _SD(sd)
      {};
    void begin();
    const bool switchFolder(const char *folder);
    void startPlayback();
    void startFilePlayback(const char* folder, const char* file);
    void skipFile();
    void pausePlayback();
    void resumePlayback();
    void togglePause();
    void stopPlayback();
    const uint8_t volume() {return _volume;};
    void volume(uint8_t volume, bool notifyCallback=true);
    void volumeDown();
    void volumeUp();
    void startNight();
    const bool isNight();
    void stopNight();
    void startShuffle();
    const bool isShuffle();
    void stopShuffle();
    void startRepeat();
    const bool isRepeat();
    void stopRepeat();
    void setBassAndTreble(uint8_t trebleAmplitude, uint8_t trebleFreqLimit, uint8_t bassAmplitude, uint8_t bassFreqLimit);
    const PlaybackState playbackState() {return _playing;};
    void currentFile(char *name, size_t size);
    void currentFolder(char *name, size_t size);
    void work();
    bool playingByCard = true;
    PlaybackCallbackFunction callback = nullptr;
  private:
    ShelfConfig _config;
    uint8_t _volume;
    PlaybackState _playing = PLAYBACK_NO;
    Adafruit_VS1053_FilePlayer _musicPlayer;
    sdfat::SdFat &_SD;
    sdfat::SdFile _currentFolder;
    uint16_t _currentFolderFileCount;
    char _currentFile[100];
    const bool _patchVS1053();
    bool _nightMode = false;
    // Night mode timeouts NIGHT_TIMEOUT minutes after playback ends, so we need to keep count
    unsigned long _lastNightActivity;
    bool _repeatMode = true;
    bool _shuffleMode = false;
    uint16_t _shufflePlaybackCount;
    // Store playback history for shuffle playback to prevent repititions
    BoolArray _shuffleHistory;
};