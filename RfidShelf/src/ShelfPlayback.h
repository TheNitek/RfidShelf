#pragma once

#include "ShelfPins.h"
#include "ShelfConfig.h"
#include <Adafruit_VS1053.h>
#include <ESP8266HTTPClient.h>
#include <google-tts.h>
#include <SdFat.h>
#include <BoolArray.h>

class ShelfPlayback {
  public:
    ShelfPlayback(ShelfConfig::GlobalConfig &config, SdFat &sd) :
      _config(config),
      _musicPlayer(BREAKOUT_RESET, BREAKOUT_CS, BREAKOUT_DCS, DREQ, sd),
      _SD(sd)
      {};
    enum PlaybackState {PLAYBACK_NO, PLAYBACK_FILE, PLAYBACK_PAUSED};
    typedef std::function<void(PlaybackState state, uint8_t volume)> PlaybackCallbackFunction;
    void begin();
    const bool switchFolder(const char *folder);
    void startPlayback();
    void startFilePlayback(const char* folder, const char* file);
    void say(const char *text);
    void skipFile();
    void pausePlayback();
    void resumePlayback();
    void togglePause();
    void stopPlayback();
    const uint8_t volume() {return _volume;};
    void volume(const uint8_t volume, bool notifyCallback=true);
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
    void setBassAndTreble(const uint8_t trebleAmplitude, const uint8_t trebleFreqLimit, const uint8_t bassAmplitude, const uint8_t bassFreqLimit);
    const PlaybackState playbackState() {return _playing;};
    void currentFile(char *name, size_t size);
    void currentFolder(char *name, size_t size);
    void currentFolderSFN(char *name, size_t size);
    void work();
    bool playingByCard = true;
    PlaybackCallbackFunction callback = nullptr;
    TTS tts;
  private:
    const bool _patchVS1053();
    void _flushVS1053();
    ShelfConfig::GlobalConfig &_config;
    Adafruit_VS1053_FilePlayer _musicPlayer;
    uint8_t _volume;
    PlaybackState _playing = PLAYBACK_NO;
    SdFat &_SD;
    File32 _currentFolder;
    uint16_t _currentFolderFileCount;
    char _currentFile[100];
    bool _nightMode = false;
    // Night mode timeouts NIGHT_TIMEOUT minutes after playback ends, so we need to keep count
    unsigned long _lastNightActivity;
    bool _repeatMode = true;
    bool _shuffleMode = false;
    uint16_t _shufflePlaybackCount;
    // Store playback history for shuffle playback to prevent repititions
    BoolArray _shuffleHistory;
};