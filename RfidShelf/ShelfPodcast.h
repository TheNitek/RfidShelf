#pragma once

#include <SdFat.h>
#include <sdios.h>
#include <ESP8266WiFi.h>
#include <Podcatcher.h>
#include "ShelfConfig.h"
#include "ShelfPlayback.h"

class ShelfPodcast {
  public:
    ShelfPodcast(ShelfConfig &config, ShelfPlayback &playback, sdfat::SdFat &sd) : _config(config), _playback(playback), _SD(sd) {}
    void work();
  private:
    ShelfConfig &_config;
    ShelfPlayback &_playback;
    sdfat::SdFat &_SD;
    struct PodcastState {
      String episodeUrl = "";
      String episodeGuid = "";
      uint16_t episodeCount = 0;
      boolean done = false;
      String lastGuid = "";
      uint16_t maxEpisodes = 0;
    };
    void _episodeCallback(PodcastState *state, const char *url, const char *guid);
    void _loadFeed(PodcastState *state, const char *podcastUrl);
    void _downloadEpisodes(PodcastState *state, const char *folder);
    unsigned long _lastUpdate = 0;
};