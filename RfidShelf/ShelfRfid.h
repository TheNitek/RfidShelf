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
  uint8_t repeat;       // 0 = current setting, 2 = no, 3 = yes
  uint8_t shuffle;      // 0 = current setting, 2 = no, 3 = yes
  uint8_t stopOnRemove; // 0 = current setting, 2 = no, 3 = yes
};

class ShelfRfid {
  public:
    ShelfRfid(ShelfPlayback &playback) :
      _playback(playback),
      _mfrc522(RC522_CS, UINT8_MAX)
      {};
    void begin();
    void handleRfid();
    bool startPairing(const char *folder, const uint8_t volume, const uint8_t repeat, const uint8_t shuffle, const uint8_t stopOnRemove);
    nfcTagObject getPairingConfig();
    bool hasActivePairing = false;
  private:
    ShelfPlayback &_playback;
    MFRC522 _mfrc522; // Create MFRC522 instance.
    MFRC522::MIFARE_Key _key;
    byte _lastCardUid[4]; // Init array that will store new card uid
    unsigned long _lastRfidCheck = 0L;
    nfcTagObject _currentCard;
    void _handleRfidData();
    void _handleRfidConfig();
    static void _print_byte_array(const uint8_t *buffer, const uint8_t  bufferSize);
    void _writeConfigBlock();
    bool _writeRfidBlock(uint8_t sector, uint8_t relativeBlock, const uint8_t *content, uint8_t contentSize) ;
    bool _readRfidBlock(uint8_t sector, uint8_t relativeBlock, uint8_t *outputBuffer, uint8_t bufferSize);
    void _setPlaybackOptions(uint8_t repeat, uint8_t shuffle);
    void _dumpCurrentCard();
};

#endif // ShelfRfid_h
