#ifndef ShelfWeb_h
#define ShelfWeb_h

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266httpUpdate.h>
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
    ShelfWeb(AsyncWebServer &webserver, ShelfPlayback &playback, ShelfRfid &rfid, FS &sd) : _server(webserver), _playback(playback), _rfid(rfid), _SD(sd) {}
    void begin();
    void work();
  private:
    AsyncWebServer &_server;
    ShelfPlayback &_playback;
    ShelfRfid &_rfid;
    FS &_SD;
    sdfat::SdFile _uploadFile;
    uint32_t _uploadStart;
    void _returnOK(AsyncWebServerRequest *request);
    void _returnHttpStatus(AsyncWebServerRequest *request, uint16_t statusCode, const char *msg);
    void _sendHTML(AsyncWebServerRequest *request);
    void _sendJsonStatus(AsyncWebServerRequest *request);
    void _sendJsonConfig(AsyncWebServerRequest *request);
    void _sendJsonFS(AsyncWebServerRequest *request, const char *path);
    bool _loadFromSdCard(AsyncWebServerRequest *request, const char *path);
    void _handleWriteRfid(const char *folder);
    void _handleFileUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final);
    void _handleDefault(AsyncWebServerRequest *request);
    void _downloadPatch(AsyncWebServerRequest *request);
    void _updateOTA();
};

#endif // ShelfWeb_h
