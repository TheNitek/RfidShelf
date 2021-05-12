#pragma once

#include <Arduino.h>
#include <TZ.h>


// -------------------------
// AUDIO SETTINGS
// -------------------------
// bass enhancer settings
// treble amplitude in 1.5 dB steps (-8..7, 0 = off)
#define TREBLE_AMPLITUDE 0
// treble lower limit frequency in 1000 Hz steps (1..15)
#define TREBLE_FREQLIMIT 0
// bass enhancement in 1 dB steps (0..15, 0 = off)
#define BASS_AMPLITUDE 10
// bass lower limit frequency in 10 Hz steps (2..15)
#define BASS_FREQLIMIT 15

// enable differential output, if disabled mono output is used
//#define USE_DIFFERENTIAL_OUTPUT

#define NIGHT_FACTOR 0.25f
#define NIGHT_TIMEOUT 300000

// -------------------------
// MISC SETTINGS
// -------------------------

//#define ARDUINO_OTA_ENABLE 1

// the update url for the OTA update
#define UPDATE_URL "https://download.naeveke.de/board/latest.bin"

// the url for the patch download
#define VS1053_PATCH_URL "https://raw.githubusercontent.com/madsci1016/Sparkfun-MP3-Player-Shield-Arduino-Library/master/plugins/patches.053"

// -------------------------
// HARDWARE BUTTONS
// -------------------------
// Make sure to disable DEBUG_ENABLE if you enable this
// Right now this enables volume control via A0 input.
// Changing volume via cards or web interface will not work anymore!
//#define BUTTONS_ENABLE


// -------------------------
// DEBUG OUTPUT
// -------------------------
#define DEBUG_ENABLE
#ifdef DEBUG_ENABLE
    #define Sprintln(a) (Serial.println(a))
    #define Sprint(a) (Serial.print(a))
    #define Sprintf(a, b) (Serial.printf_P(PSTR(a), b))
#else
    #define Sprintln(a)
    #define Sprint(a)
    #define Sprintf(a, b)
#endif

#if defined(BUTTONS_ENABLE) && defined(DEBUG_ENABLE)
  #error "BUTTONS_ENABLE cannot be used with DEBUG_ENABLE"
#endif

struct Timeslot_t{
    Timeslot_t() : monday(false), tuesday(false), wednesday(false), thursday(false), friday(false), saturday(false), sunday(false) {}
    bool monday : 1;
    bool tuesday : 1;
    bool wednesday : 1;
    bool thursday : 1;
    bool friday : 1;
    bool saturday : 1;
    bool sunday : 1;
    uint8_t startHour = 0;
    uint8_t startMinutes = 0;
    uint8_t endHour = 0;
    uint8_t endMinutes = 0;
};

class ShelfConfig {
    public:
        ShelfConfig() {};
        void init();
        uint8_t version = 1;
        char hostname[20];
        char ntpServer[100];
        char timezone[50];
        uint8_t podcastHour = 2;
        uint8_t defaultVolumne = 10;
        bool defaultRepeat : 1;
        bool defaultShuffle : 1;
        bool defaultStopOnRemove : 1;
        Timeslot_t nightModeTimes[5];
};