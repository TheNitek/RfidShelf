#pragma once

#include <SdFat.h>
#include <ESP8266WiFi.h>
#include <Podcatcher.h>
#include <Adafruit_VS1053.h>
#include "ShelfConfig.h"
#include "ShelfPlayback.h"
#include "ShelfWeb.h"

class ShelfPodcast {
  public:
    ShelfPodcast(ShelfConfig &config, ShelfPlayback &playback, ShelfWeb &web, sdfat::SdFat &sd) : _config(config), _playback(playback), _web(web), _SD(sd) {}
    void work();
  private:
    ShelfConfig &_config;
    ShelfPlayback &_playback;
    ShelfWeb &_web;
    sdfat::SdFat &_SD;
    unsigned long _lastUpdate = 0;
    struct PodcastState {
      char episodeUrl[151] = {0};
      char episodeGuid[151] = {0};
      uint16_t episodeCount = 0;
      bool done = false;
      char lastGuid[151] = {0};
      uint16_t lastFileNo = 0;
      char feedUrl[151] = {0};
      uint16_t maxEpisodes = 0;
    } state;
    void _episodeCallback(PodcastState *state, const char *url, const char *guid);
    bool _nextPodcast(char *folder);
    bool _readPodcastFile(PodcastState &state, const char* podFilename);
    void _loadFeed(PodcastState *state, const char *podcastUrl);
    bool _downloadEpisodes(PodcastState *state, const char *folder);
    void _cleanupEpisodes(PodcastState &state, const char *folder);
};