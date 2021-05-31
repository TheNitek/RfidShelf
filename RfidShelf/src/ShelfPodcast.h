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
    ShelfPodcast(ShelfConfig &config, ShelfPlayback &playback, ShelfWeb &web, sdfat::SdFat &sd) : _config(config), _playback(playback), _web(web), _SD(sd) {}
    struct PodcastInfo {
      public:
        PodcastInfo(sdfat::SdFat &sd) : _SD(sd) {}
        String feedUrl;
        uint16_t maxEpisodes = 0;
        String lastGuid;
        uint16_t lastFileNo = 0;
        bool load(const char* podFilename);
        bool save(const char* podFilename);
      private:
        sdfat::SdFat &_SD;
    };
    void work();
  private:
    class PodcastState {
      public: 
        PodcastState(const PodcastInfo &info) : _info(info) {}
        PodcastState(const PodcastInfo &&) = delete;
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
        const PodcastInfo &_info;
    };
    class _HTTPClient: public HTTPClient {
      public:
        transferEncoding_t getTransferEncoding() {
          return _transferEncoding;
        }
    };
    ShelfConfig &_config;
    ShelfPlayback &_playback;
    ShelfWeb &_web;
    sdfat::SdFat &_SD;
    unsigned long _lastUpdate = 0;
    bool _resumePlayback = false;
    bool _isPodcastTime();
    bool _nextPodcast(char *folder);
    void _loadFeed(_HTTPClient &httpClient, const String &feedUrl, PodcastState &state);
    bool _downloadNextEpisode(PodcastInfo &info, const char *folder);
    void _cleanupEpisodes(uint16_t maxEpisodes, const char *folder);
};