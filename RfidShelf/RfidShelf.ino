#include <Arduino.h>
#include "ShelfPins.h"
#include "ShelfConfig.h"
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <SdFat.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include <NTPClient.h>
#include "ShelfPlayback.h"
#include "ShelfRfid.h"
#include "ShelfWeb.h"

#ifdef ARDUINO_OTA_ENABLE
#include <ArduinoOTA.h>
#endif

#ifdef BUTTONS_ENABLE
#include "ShelfButtons.h"
#endif

#ifdef PUSHOVER_ENABLE
#include "ShelfPushover.h"
#endif


WiFiManager wifiManager;

SdFat SD;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_SERVER, NTP_OFFSET, NTP_UPDATE_TIME);

char hostString[16] = {0};

ShelfPlayback playback(SD);
ShelfRfid rfid(playback);
ShelfWeb webInterface(playback, rfid, SD, timeClient);
#ifdef BUTTONS_ENABLE
ShelfButtons buttons(playback);
#endif
#ifdef PUSHOVER_ENABLE
ShelfPushover pushover;
#endif

void setup() {
  // Seems to make flashing more reliable
  delay(100);

#ifdef DEBUG_ENABLE
  Serial.begin(115200);
#endif
  Sprintln();
  Sprintln(F("Starting ..."));

  // Init SPI SS pins
  pinMode(RC522_CS, OUTPUT);
  digitalWrite(RC522_CS, HIGH);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  pinMode(BREAKOUT_CS, OUTPUT);
  digitalWrite(BREAKOUT_CS, HIGH);
  pinMode(BREAKOUT_DCS, OUTPUT);
  digitalWrite(BREAKOUT_DCS, HIGH);

  randomSeed(analogRead(A0));

  rfid.begin();
    
  //Initialize the SdCard.
  if (!SD.begin(SD_CS) || !SD.chdir("/")) {
    Sprintln(F("Could not initialize SD card"));
    SD.initErrorHalt();
  }
  Sprintln(F("SD ready"));

  playback.begin();
  playback.startFilePlayback("", "ready_before_wifi.mp3");

  sprintf(hostString, "SHELF_%06X", ESP.getChipId());
  Sprint("Hostname: "); Sprintln(hostString);
  WiFi.hostname(hostString);

  wifiManager.setConfigPortalTimeout(3 * 60);
  if (!wifiManager.autoConnect("MP3-SHELF-SETUP", "lacklack")) {
    Sprintln(F("Setup timed out, starting AP"));
    WiFi.mode(WIFI_AP);
    Sprintln(WiFi.softAP("MP3-SHELF", "lacklack") ? "Soft-AP is set up" : "Soft-AP setup failed");
  }

  if(WiFi.isConnected()) {
    Sprint(F("Connected! IP address: ")); Sprintln(WiFi.localIP());
  }

  if (!MDNS.begin(hostString)) {
    Sprintln("Error setting up MDNS responder!");
  }

#ifdef ARDUINO_OTA_ENABLE
  // TODO move into EEPROM config
  ArduinoOTA.setPassword((const char *)"shelfshelf");
  ArduinoOTA.begin();
#endif

  if(NTP_ENABLE == 1) {
    timeClient.begin();
  }

  webInterface.begin();

#ifdef BUTTONS_ENABLE
  buttons.begin();
#endif

  playback.startFilePlayback("", "ready.mp3");

  Sprintln(F("Init done"));
}

void loop() {
#ifdef BUTTONS_ENABLE
  buttons.work();
#endif

#ifdef ARDUINO_OTA_ENABLE
  ArduinoOTA.handle();
#endif

  MDNS.update();

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
