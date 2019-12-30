#ifndef ShelfRfid_h
#define ShelfRfid_h

#include "ShelfConfig.h"
#include "ShelfPins.h"
#include "ShelfPlayback.h"
#include <SPI.h>
#include <MFRC522.h>
#include <SdFat.h>

// this object stores nfc tag data
struct nfcTagObject {
  char folder[17] = {'\0'};
  uint8_t volume;
  bool shuffle;
  bool repeat;
};

class ShelfRfid {
  public:
    ShelfRfid(ShelfPlayback &playback) :
      _playback(playback),
      _mfrc522(RC522_CS, UINT8_MAX)
      {};
    void begin();
    void handleRfid();
    bool startPairing(const char *folder, uint8_t volume, bool repeat, bool shuffle);
    nfcTagObject getPairingConfig();
    bool hasActivePairing = false;
  private:
    ShelfPlayback &_playback;
    MFRC522 _mfrc522; // Create MFRC522 instance.
    MFRC522::MIFARE_Key _key;
    byte _lastCardUid[4]; // Init array that will store new card uid
    unsigned long _lastRfidCheck = 0L;
    nfcTagObject myCard;
    void handleRfidData();
    void handleRfidConfig();
    static void print_byte_array(const uint8_t *buffer, const uint8_t  bufferSize);
    void writeConfigBlock();
    bool writeRfidBlock(uint8_t sector, uint8_t relativeBlock, const uint8_t *content, uint8_t contentSize) ;
    bool readRfidBlock(uint8_t sector, uint8_t relativeBlock, uint8_t *outputBuffer, uint8_t bufferSize);
    void dumpCurrentCard();
};

#endif // ShelfRfid_h
