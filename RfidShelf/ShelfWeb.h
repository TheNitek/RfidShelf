#ifndef ShelfWeb_h
#define ShelfWeb_h

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266httpUpdate.h>
#include <Adafruit_VS1053.h>
#include <SdFat.h>
#include <NTPClient.h>
#include "ShelfConfig.h"
#include "ShelfPlayback.h"
#include "ShelfRfid.h"
#include "ShelfVersion.h"
#include "ShelfHtml.h"

class ShelfWeb {
  public:
    ShelfWeb(ShelfPlayback &playback, ShelfRfid &rfid, SdFat &sd, NTPClient &timeClient);
    void begin();
    void work();
    static void defaultCallback();
    static void fileUploadCallback();
  private:
    static ShelfWeb *_instance;
    ShelfPlayback &_playback;
    ShelfRfid &_rfid;
    SdFat &_SD;
    NTPClient &_timeClient;
    ESP8266WebServer _server;
    SdFile _uploadFile;
    uint32_t _uploadStart;
    void _returnOK();
    void _returnHttpStatus(uint16_t statusCode, const char *msg);
    void _sendHTML();
    void _sendJsonStatus();
    void _sendJsonFS(const char *path);
    bool _loadFromSdCard(const char *path);
    void _handleWriteRfid(const char *folder);
    void _handleFileUpload();
    void _handleDefault();
    void _downloadPatch();
    void _updateOTA();
};

#endif // ShelfWeb_h
