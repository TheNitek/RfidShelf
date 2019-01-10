#ifndef ShelfRfid_h
#define ShelfRfid_h

#include "ShelfPins.h"
#include "ShelfPlayback.h"
#include <MFRC522.h>
#include <SdFat.h>

class ShelfRfid {
  public:
    ShelfRfid(ShelfPlayback &playback) :
      _playback(playback),
      _mfrc522(RC522_CS, UINT8_MAX)
      {};
    void begin();
    void handleRfid();
    bool pairing = false;
  private:
    ShelfPlayback &_playback;
    MFRC522 _mfrc522;   // Create MFRC522 instance.
    MFRC522::MIFARE_Key _key;
    byte _lastCardUid[4]; // Init array that will store new card uid
    static void print_byte_array(const uint8_t *buffer, const uint8_t  bufferSize);
    bool writeRfidBlock(uint8_t sector, uint8_t relativeBlock, const uint8_t *newContent, uint8_t contentSize) ;
    bool readRfidBlock(uint8_t sector, uint8_t relativeBlock, uint8_t *outputBuffer, uint8_t bufferSize);
};

#endif // ShelfRfid_h
