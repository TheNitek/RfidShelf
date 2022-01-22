#pragma once

#include "ShelfConfig.h"
#include "ShelfPins.h"
#include "ShelfPlayback.h"
#include <SPI.h>
#include <MFRC522.h>

#define RFID_CONFIG_MARKER 137

class ShelfRfid {
  public:
    ShelfRfid(ShelfConfig::GlobalConfig &config, ShelfPlayback &playback) :
      _config(config),
      _playback(playback),
      _mfrc522(RC522_CS, UINT8_MAX)
      {};
    struct CardConfig {
      uint8_t magicByte = RFID_CONFIG_MARKER;
      uint8_t size;
      uint8_t volume;
      uint8_t repeat : 2;       // 2 = current setting, 0 = no, 1 = yes
      uint8_t shuffle : 2;      // 2 = current setting, 0 = no, 1 = yes
      uint8_t stopOnRemove : 2; // 2 = current setting, 0 = no, 1 = yes
    };
    // this object stores nfc tag data
    struct NFCTagObject {
      char folder[17] = {'\0'};
      CardConfig config;
    };
    void begin();
    void handleRfid(const bool ignoreTagData = false);
    bool startPairing(const char *folder, const uint8_t volume, const uint8_t repeat, const uint8_t shuffle, const uint8_t stopOnRemove);
    NFCTagObject getPairingConfig();
    bool hasActivePairing = false;
  private:
    ShelfConfig::GlobalConfig &_config;
    ShelfPlayback &_playback;
    MFRC522 _mfrc522; // Create MFRC522 instance.
    MFRC522::MIFARE_Key _key = {.keyByte={0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
    byte _lastCardUid[4]; // Init array that will store new card uid
    unsigned long _lastRfidCheck = 0L;
    NFCTagObject _currentCard;
    NFCTagObject _pairingCard;
    void _handleRfidData();
    bool _handleRfidConfig();
    static void _print_byte_array(const uint8_t *buffer, const uint8_t  bufferSize);
    void _writeConfigBlock(const CardConfig *config);
    bool _writeRfidBlock(const uint8_t sector, const uint8_t relativeBlock, const uint8_t *content, const uint8_t contentSize) ;
    bool _readRfidBlock(const uint8_t sector, const uint8_t relativeBlock, uint8_t *outputBuffer, uint8_t bufferSize);
    void _setPlaybackOptions(const uint8_t repeat, const uint8_t shuffle);
    void _dumpCurrentCard(const NFCTagObject* card);
};