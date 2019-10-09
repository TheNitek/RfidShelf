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
	  const char* _dnsname = DNS_NAME;
    ESP8266WebServer _server;
    SdFile _uploadFile;
    uint32_t _uploadStart;
    void returnOK();
    void returnHttpStatus(uint16_t statusCode, const char *msg);
    void sendHTML();
    void sendJsonStatus();
    void sendJsonFS(const char *path);
    bool loadFromSdCard(const char *path);
    void handleWriteRfid(const char *folder);
    void handleFileUpload();
    void handleDefault();
    void downloadPatch();
};

#endif // ShelfWeb_h
