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

// the default play volume (0-50)
// lower value means louder!
#define DEFAULT_VOLUME 10

#define NIGHT_FACTOR 0.25f
#define NIGHT_TIMEOUT 300000


// -------------------------
// MISC SETTINGS
// -------------------------
// number off consecutive http reconnects before giving up stream
#define MAX_RECONNECTS 10

// the update url for the OTA update
#define UPDATE_URL "http://download.naeveke.de/board/latest.bin"

// the url for the patch download
#define VS1053_PATCH_URL "https://raw.githubusercontent.com/madsci1016/Sparkfun-MP3-Player-Shield-Arduino-Library/master/plugins/patches.053"

// -------------------------
// NOTIFICTATION_SETTINGS
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
