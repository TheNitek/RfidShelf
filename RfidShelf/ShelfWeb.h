#ifndef ShelfWeb_h
#define ShelfWeb_h

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ESP8266httpUpdate.h>
#include <Adafruit_VS1053.h>
#include <SdFat.h>
#include "ShelfPlayback.h"
#include "ShelfRfid.h"
#include "ShelfVersion.h"

class ShelfWeb {
  public:
    ShelfWeb(ShelfPlayback &playback, ShelfRfid &rfid);
    void begin();
    void work();
    static void notFoundCallback();
    static void fileUploadCallback();
  private:
    static ShelfWeb* _instance;
    ShelfPlayback &_playback;
    ShelfRfid &_rfid;
    ESP8266WebServer _server;
    SdFat _SD;
    SdFile _uploadFile;
    uint32_t _uploadStart;
    void returnOK();
    void returnHttpStatus(uint8_t statusCode, String msg);
    void renderDirectory(String &path);
    bool loadFromSdCard(String &path);
    void handleWriteRfid(String &folder);
    void handleFileUpload();
    void handleNotFound();
};

#endif // ShelfWeb_h
