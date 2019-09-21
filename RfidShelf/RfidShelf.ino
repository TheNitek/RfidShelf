#include <Arduino.h>
#include "ShelfPins.h"
#include "ShelfConfig.h"
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <SdFat.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include "ShelfPlayback.h"
#include "ShelfRfid.h"
#include "ShelfWeb.h"

#ifdef PUSHOVER_ENABLE
#include "ShelfPushover.h"
#endif

WiFiManager wifiManager;

SdFat SD;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_SERVER, NTP_OFFSET, NTP_UPDATE_TIME);

ShelfPlayback playback(SD);
ShelfRfid rfid(playback);
ShelfWeb webInterface(playback, rfid, SD, timeClient);
#ifdef PUSHOVER_ENABLE
ShelfPushover pushover;
#endif

void setup() {
  // Seems to make flashing more reliable
  delay(100);

  Serial.begin(115200);
  Serial.println();
  Serial.println(F("Starting ..."));

  // Init SPI SS pins
  pinMode(RC522_CS, OUTPUT);
  digitalWrite(RC522_CS, HIGH);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  pinMode(BREAKOUT_CS, OUTPUT);
  digitalWrite(BREAKOUT_CS, HIGH);
  pinMode(BREAKOUT_DCS, OUTPUT);
  digitalWrite(BREAKOUT_DCS, HIGH);

  rfid.begin();
    
  //Initialize the SdCard.
  if (!SD.begin(SD_CS)) {
    Serial.println(F("Could not initialize SD card"));
    SD.initErrorHalt();
  }
  Serial.println(F("SD ready"));
  
  playback.begin();

  wifiManager.setConfigPortalTimeout(3 * 60);
  if (!wifiManager.autoConnect("MP3-SHELF-SETUP", "lacklack")) {
    Serial.println(F("Setup timed out, starting AP"));
    WiFi.mode(WIFI_AP);
    WiFi.softAP("MP3-SHELF", "lacklack");
  }

  Serial.print(F("Connected! IP address: "));
  Serial.println(WiFi.localIP());

  if(NTP_ENABLE == 1) {
    timeClient.begin();
  }

  webInterface.begin();

  Serial.println(F("Init done"));
}

void loop() {
  playback.work();

  rfid.handleRfid();

#ifdef PUSHOVER_ENABLE
  pushover.sendPoweredNotification();
#endif

  if(NTP_ENABLE == 1) {
    timeClient.update();
  }

  webInterface.work();
}
