#pragma once

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
    ShelfWeb(ShelfConfig &config, ShelfPlayback &playback, ShelfRfid &rfid, sdfat::SdFat &sd) : _config(config), _playback(playback), _rfid(rfid), _SD(sd), _alexaDevice(_config.hostname, std::bind(&ShelfWeb::_deviceCallback, this, std::placeholders::_1), EspalexaDeviceType::dimmable, 50) {}
    void begin();
    void work();
    bool isFileUploading();
    void pause();
    void unpause();
  private:
    ShelfConfig &_config;
    ShelfPlayback &_playback;
    ShelfRfid &_rfid;
    sdfat::SdFat &_SD;
    Espalexa _espalexa;
    EspalexaDevice _alexaDevice;
    ESP8266WebServer _server;
    sdfat::SdFile _uploadFile;
    uint32_t _uploadStart;
    bool _paused = false;
    void _returnOK();
    void _returnHttpStatus(const uint16_t statusCode, const char *msg);
    void _sendHTML();
    void _sendJsonStatus();
    void _sendJsonConfig();
    void _sendJsonFSUsage();
    void _sendJsonFS(const char *path);
    bool _loadFromSdCard(const char *path);
    void _handleWriteRfid(const char *folder);
    void _handleFileUpload();
    void _handleDefault();
    void _deviceCallback(EspalexaDevice* device);
    void _downloadPatch();
    void _updateOTA();
    void _playbackCallback(PlaybackState state, uint8_t volume);
};