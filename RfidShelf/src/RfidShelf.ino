#include <Arduino.h>
#include "ShelfPins.h"
#include "ShelfConfig.h"
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <SdFat.h>
#include <ESP8266mDNS.h>
#include <time.h>
#include <coredecls.h>        // settimeofday_cb()
#include "ShelfPlayback.h"
#include "ShelfRfid.h"
#include "ShelfWeb.h"
#include "ShelfPodcast.h"

#ifdef ARDUINO_OTA_ENABLE
#include <ArduinoOTA.h>
#endif

#ifdef BUTTONS_ENABLE
#include "ShelfButtons.h"
#endif


SdFat sdCard;

ShelfConfig::GlobalConfig config(sdCard);
ShelfPlayback playback(config, sdCard);
ShelfRfid rfid(config, playback);
ShelfWeb web(config, playback, rfid, sdCard);
ShelfPodcast podcast(config, playback, web, sdCard);
#ifdef BUTTONS_ENABLE
ShelfButtons buttons(config, playback);
#endif

void timeCallback() {
  Sprint(F("Time updated: "));
  time_t now = time(nullptr);
  Sprintln(ctime(&now));
}

void initCS(uint8_t pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, HIGH);
}

void setup() {
#ifdef DEBUG_ENABLE
  Serial.begin(115200);
#endif
  Sprintln();
  Sprintln(F("Starting ..."));

  // Init SPI CS pins
  initCS(RC522_CS);
  initCS(SD_CS);
  initCS(BREAKOUT_CS);
  initCS(BREAKOUT_DCS);

  randomSeed(analogRead(A0));

  SPI.begin();

  config.load();

  rfid.begin();
    
  //Initialize the SdCard.
  if (!sdCard.begin(SD_CS) || !sdCard.chdir("/")) {
    Sprintln(F("Could not initialize SD card width SdFat"));
    sdCard.initErrorHalt();
  }
  Sprintln(F("SDFat ready"));

  playback.begin();

  if(config.ntpServer[0] != '\0') {
    settimeofday_cb(timeCallback);
    configTime(config.timezone, config.ntpServer);
  }


  Sprint(F("Hostname: ")); Sprintln(config.hostname);
  WiFi.hostname(config.hostname);

  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(3 * 60);
  if (!wifiManager.autoConnect("MP3-SHELF-SETUP", "lacklack")) {
    Sprintln(F("Setup timed out, starting AP"));
    WiFi.mode(WIFI_AP);
    if (WiFi.softAP("MP3-SHELF", "lacklack")) {
      Sprintln(F("Soft-AP is set up"));
    } else {
      Sprintln(F("Soft-AP setup failed"));
    }
  }

  if(WiFi.isConnected()) {
    Sprint(F("Connected! http://")); Sprintln(WiFi.localIP());
  }

  if (!MDNS.begin(config.hostname)) {
    Sprintln(F("Error setting up MDNS responder!"));
  }

#ifdef ARDUINO_OTA_ENABLE
  // TODO move into EEPROM config
  ArduinoOTA.setPassword((const char *)"shelfshelf");
  ArduinoOTA.begin();
#endif

  web.begin();

#ifdef BUTTONS_ENABLE
  buttons.begin();
#endif

  // To prevent playback after reboot if a tag is still placed on the reader
  rfid.handleRfid(true);

  Sprintln(F("Init done"));
}

void loop() {
  MDNS.update();

  web.work();

  // Skip the rest while file is uploading to improve performance (playback won't work anyway)
  if(web.isFileUploading()) {
    return;
  }

#ifdef BUTTONS_ENABLE
  buttons.work();
#endif

#ifdef ARDUINO_OTA_ENABLE
  ArduinoOTA.handle();
#endif

  playback.work();

  rfid.handleRfid();

  podcast.work();
}