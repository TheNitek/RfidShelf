#ifndef ShelfConfig_h
#define ShelfConfig_h

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
    #define Sprintf(a, b) (Serial.printf(a, b))
#else
    #define Sprintln(a)
    #define Sprint(a)
    #define Sprintf(a, b)
#endif

// -------------------------
// NOTIFICTATION SETTINGS
// -------------------------
// enable sending of notifications using pushover
// #define PUSHOVER_ENABLE
#ifdef PUSHOVER_ENABLE
    // your pushover user key
    #define PUSHOVER_USER "user"
    // your pushover application token
    #define PUSHOVER_TOKEN "key"
    // receiving device(s), leave empty to send notifications to all devices
    #define PUSHOVER_DEVICE ""
    // name of this RfidShelf (in case you have multiple)
    #define PUSHOVER_RFIDSHELD_NAME "RfidShelf"
    // timeout for periodic sending of the powered notification in ms (defaults to 30 min)
    #define PUSHOVER_POWERED_NOTIFICATION_TIME 30 * 60 * 1000
    // sound to play for powered notification
    #define PUSHOVER_POWERED_SOUND "climb"
#endif

#if defined(BUTTONS_ENABLE) && defined(DEBUG_ENABLE)
  #error "BUTTONS_ENABLE cannot be used with DEBUG_ENABLE"
#endif

struct Timeslot_t
{
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

struct ShelfConfig_t {
    ShelfConfig_t() {
        sprintf(hostname, "SHELF_%06X", ESP.getChipId());
        defaultRepeat = true;
        defaultShuffle = false;
        defaultStopOnRemove = true;
        strncpy(timezone, TZ_Europe_Berlin, sizeof(timezone));
        strncpy(ntpServer, "pool.ntp.org", sizeof(ntpServer));
    };
    uint8_t version = 1;
    char hostname[20];
    char ntpServer[100];
    char timezone[50];
    uint8_t defaultVolumne = 10;
    bool defaultRepeat : 1;
    bool defaultShuffle : 1;
    bool defaultStopOnRemove : 1;
    Timeslot_t nightModeTimes[5];
};

namespace ShelfConfig {
    extern ShelfConfig_t config;
    void init();
};

#endif // ShelfConfig_h