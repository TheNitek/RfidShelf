#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266httpUpdate.h>
#include <Espalexa.h>
#include <Adafruit_VS1053.h>
#include <SdFat.h>
#include <time.h>
#include "ShelfConfig.h"
#include "ShelfPlayback.h"
#include "ShelfRfid.h"
#include "ShelfVersion.h"
#include "ShelfHtml.h"

class ShelfWeb {
  public:
    ShelfWeb(ShelfConfig::GlobalConfig &config, ShelfPlayback &playback, ShelfRfid &rfid, SdFat &sd) : _config(config), _playback(playback), _rfid(rfid), _SD(sd) {}
    void begin();
    void work();
    bool isFileUploading();
    void pause();
    void unpause();
    bool updatePodcastsRequested = false;
    // Dirty hack to work around https://github.com/esp8266/Arduino/issues/8055
    class StreamFile: public File32 {
      int availableForWrite() override {
        return 512;
      }
    };

  private:
    ShelfConfig::GlobalConfig &_config;
    ShelfPlayback &_playback;
    ShelfRfid &_rfid;
    SdFat &_SD;
    Espalexa _espalexa;
    EspalexaDevice *_alexaDevice;
    ESP8266WebServer _server;
    File32 _uploadFile;
    uint32_t _uploadStart;
    bool _paused = false;
    void _returnOK();
    void _returnHttpStatus(const uint16_t statusCode, const char *msg);
    void _sendHTML();
    void _sendJsonStatus();
    void _sendJsonConfig();
    void _sendJsonFSUsage();
    void _sendJsonFS(const char *path);
    void _sendJsonPodcast(const char *path);
    bool _loadFromSdCard(const char *path);
    void _handleDelete(const char *file);
    void _handleWriteRfid(const char *folder);
    void _handleWritePodcast(const char *folder);
    void _handleFileUpload();
    void _handleDefault();
    void _deviceCallback(EspalexaDevice* device);
    void _downloadPatch();
    void _updateOTA();
    void _playbackCallback(ShelfPlayback::PlaybackState state, uint8_t volume);
};