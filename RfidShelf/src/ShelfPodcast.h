#pragma once

#include <SdFat.h>
#include <ESP8266WiFi.h>
#include <Podcatcher.h>
#include <Adafruit_VS1053.h>
#include <time.h>
#include "ShelfConfig.h"
#include "ShelfPlayback.h"
#include "ShelfWeb.h"

class ShelfPodcast {
  public:
    ShelfPodcast(ShelfConfig::GlobalConfig &config, ShelfPlayback &playback, ShelfWeb &web, SdFat &sd): _config(config), _playback(playback), _web(web), _SD(sd) {}
    void work();
  private:
    class PodcastState {
      public: 
        PodcastState(const ShelfConfig::PodcastConfig &info) : _info(info) {}
        PodcastState(const ShelfConfig::PodcastConfig &&) = delete;
        String episodeUrl;
        String episodeGuid;
        uint16_t episodeCount = 0;
        bool done = false;
        void episodeCallback(const char *url, const char *guid) {
          if(_info.lastGuid.equals(guid) || (++(episodeCount) > _info.maxEpisodes)) {
            done = true;
            return;
          }

          episodeUrl = url;
          episodeGuid = guid;
        #ifdef DEBUG_ENABLE
          Serial.printf_P(PSTR("%s %s\n"), url, guid);
        #endif
        };
      private:
        const ShelfConfig::PodcastConfig &_info;
    };
    class _HTTPClient: public HTTPClient {
      public:
        transferEncoding_t getTransferEncoding() {
          return _transferEncoding;
        }
    };
    ShelfConfig::GlobalConfig &_config;
    ShelfPlayback &_playback;
    ShelfWeb &_web;
    SdFat &_SD;
    unsigned long _lastUpdate = 0;
    bool _isPodcastTime();
    bool _nextPodcast(char *folder);
    void _loadFeed(_HTTPClient &httpClient, const String &feedUrl, PodcastState &state);
    bool _downloadNextEpisode(ShelfConfig::PodcastConfig &info, const char *folder);
    void _cleanupEpisodes(uint16_t maxEpisodes, const char *folder);
};