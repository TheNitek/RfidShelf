# 1 "c:\\users\\nitek\\appdata\\local\\temp\\tmphfgiel"
#include <Arduino.h>
# 1 "C:/Users/Nitek/Documents/Arduino/RfidShelf/RfidShelf/RfidShelf.ino"
#include <Arduino.h>
#include "ShelfPins.h"
#include "ShelfConfig.h"
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <SdFat.h>
#include "ShelfPlayback.h"
#include "ShelfRfid.h"
#include "ShelfWeb.h"

#ifdef PUSHOVER_ENABLE
#include "ShelfPushover.h"
#endif

WiFiManager wifiManager;

SdFat SD;

ShelfPlayback playback(SD);
ShelfRfid rfid(playback);
ShelfWeb webInterface(playback, rfid, SD);
#ifdef PUSHOVER_ENABLE
ShelfPushover pushover;
#endif
void setup();
void loop();
#line 26 "C:/Users/Nitek/Documents/Arduino/RfidShelf/RfidShelf/RfidShelf.ino"
void setup() {

  delay(100);

  Serial.begin(115200);
  Serial.println();
  Serial.println("Starting ...");


  pinMode(RC522_CS, OUTPUT);
  digitalWrite(RC522_CS, HIGH);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  pinMode(BREAKOUT_CS, OUTPUT);
  digitalWrite(BREAKOUT_CS, HIGH);
  pinMode(BREAKOUT_DCS, OUTPUT);
  digitalWrite(BREAKOUT_DCS, HIGH);

  rfid.begin();


  if (!SD.begin(SD_CS)) {
    Serial.println(F("Could not initialize SD card"));
    SD.initErrorHalt();
  }
  Serial.println(F("SD initialized"));

  playback.begin();

  wifiManager.setConfigPortalTimeout(3 * 60);
  if (!wifiManager.autoConnect("MP3-SHELF-SETUP", "lacklack")) {
    Serial.println(F("Setup timed out, starting AP"));
    WiFi.mode(WIFI_AP);
    WiFi.softAP("MP3-SHELF", "lacklack");
  }

  Serial.print(F("Connected! IP address: "));
  Serial.println(WiFi.localIP());

  webInterface.begin();

  Serial.println(F("Init done"));
}

void loop() {
  playback.work();

  rfid.handleRfid();

#ifdef PUSHOVER_ENABLE
  pushover.sendPoweredNotification();
#endif

  webInterface.work();
}