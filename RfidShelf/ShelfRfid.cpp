#include "ShelfRfid.h"

void ShelfRfid::begin() {
  SPI.begin();        // Init SPI bus

  _mfrc522.PCD_Init(); // Init MFRC522 card

  for (byte i = 0; i < 6; i++) {
    _key.keyByte[i] = 0xFF;
  }

  Sprintln(F("RFID initialized"));
}

void ShelfRfid::handleRfid() {
  if ((_playback.playbackState() != PLAYBACK_NO) && (millis() - _lastRfidCheck < 500)) {
    return;
  }
  _lastRfidCheck = millis();

  // While playing check if the tag is still present
  if ((_playback.playbackState() == PLAYBACK_FILE) && _playback.playingByCard) {

    // Since wireless communication is voodoo we'll give it a few retrys before killing the music
    for (int i = 0; i < 3; i++) {
      // Detect Tag without looking for collisions
      byte bufferATQA[2];
      byte bufferSize = sizeof(bufferATQA);

      MFRC522::StatusCode result = _mfrc522.PICC_WakeupA(bufferATQA, &bufferSize);

      if (result == _mfrc522.STATUS_OK && _mfrc522.PICC_ReadCardSerial() && (
            _mfrc522.uid.uidByte[0] == _lastCardUid[0] &&
            _mfrc522.uid.uidByte[1] == _lastCardUid[1] &&
            _mfrc522.uid.uidByte[2] == _lastCardUid[2] &&
            _mfrc522.uid.uidByte[3] == _lastCardUid[3] )) {
        _mfrc522.PICC_HaltA();
        return;
      }
    }

    _playback.pausePlayback();
  }

  // Look for new cards and select one if it exists
  if (!_mfrc522.PICC_IsNewCardPresent() ||  !_mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  // Show some details of the PICC (that is: the tag/card)
  Sprint(F("Card UID:"));
  print_byte_array(_mfrc522.uid.uidByte, _mfrc522.uid.size);
  Sprint(F("PICC type: "));
  MFRC522::PICC_Type piccType = _mfrc522.PICC_GetType(_mfrc522.uid.sak);
  Sprintln(_mfrc522.PICC_GetTypeName(piccType));

  if (_playback.playbackState() == PLAYBACK_PAUSED &&
      _mfrc522.uid.uidByte[0] == _lastCardUid[0] &&
      _mfrc522.uid.uidByte[1] == _lastCardUid[1] &&
      _mfrc522.uid.uidByte[2] == _lastCardUid[2] &&
      _mfrc522.uid.uidByte[3] == _lastCardUid[3] ) {
  _playback.resumePlayback();
  _mfrc522.PICC_HaltA();
  return;
}


  // Check for compatibility
  if (piccType != MFRC522::PICC_TYPE_MIFARE_1K ) {
    Sprintln(F("Unsupported card."));
    return;
  }

  if (pairing) {
    char writeFolder[17];
    SD.vwd()->getName(writeFolder, 17);
    writeRfidBlock(1, 0, (uint8_t*) writeFolder, 17);
    writeConfigBlock();
    pairing = false;
  }

  // Reset watchdog timer
  ESP.wdtFeed();
  uint8_t readBuffer[18];
  if (readRfidBlock(1, 0, readBuffer, sizeof(readBuffer))) {
    // Store uid
    memcpy(_lastCardUid, _mfrc522.uid.uidByte, 4 * sizeof(uint8_t) );

    if(readBuffer[0] != '\0') {
      char readFolder[18];
      readFolder[0] = '/';
      // allthough sizeof(readBuffer) == 18, we only get 16 byte of data
      memcpy(readFolder + 1 , readBuffer, 16 * sizeof(uint8_t) );
      // readBuffer will already contain \0 if the folder name is < 16 chars, but otherwise we need to add it
      readFolder[17] = '\0';

      if (_playback.switchFolder(readFolder)) {
        uint8_t configBuffer[18];
        if (readRfidBlock(2, 0, configBuffer, sizeof(configBuffer)) && (configBuffer[0] == 137)) {
          if(configBuffer[1] > 0) {
            Sprint(F("Setting volume: ")); Sprintln(configBuffer[2]);
            _playback.volume(configBuffer[2]);
          }
        } else {
          _playback.volume(DEFAULT_VOLUME);
          // "Upgrade" card
          writeConfigBlock();
        }

        _playback.startPlayback();
        _playback.playingByCard = true;
      }
    }
  }

  // Halt PICC
  _mfrc522.PICC_HaltA();
  // Stop encryption on PCD
  _mfrc522.PCD_StopCrypto1();

}

void ShelfRfid::writeConfigBlock() {
  uint8_t configLength = 3;

  // Store config (like volume)
  uint8_t configBuffer[configLength];
  // Magic number to mark config block and distinguish legacy cards without it
  configBuffer[0] = 137;
  // Length of config entry without header
  configBuffer[1] = configLength-2;
  configBuffer[2] = _playback.volume();
  writeRfidBlock(2, 0, configBuffer, configLength);
}

bool ShelfRfid::writeRfidBlock(uint8_t sector, uint8_t relativeBlock, const uint8_t *newContent, uint8_t contentSize) {
  uint8_t absoluteBlock = (sector * 4) + relativeBlock;

  // Authenticate using key A
  Sprintln(F("Authenticating using key A..."));
  MFRC522::StatusCode status = _mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, absoluteBlock, &_key, &(_mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Sprint(F("PCD_Authenticate() failed: "));
    Sprintln(_mfrc522.GetStatusCodeName(status));
    return false;
  }

  uint8_t bufferSize = 16;
  byte buffer[bufferSize];
  if (contentSize < bufferSize) {
    bufferSize = contentSize;
  }
  memcpy(buffer, newContent, bufferSize * sizeof(byte) );

  // Write block
  Sprint(F("Writing data to block: ")); print_byte_array(newContent, contentSize);
  status = _mfrc522.MIFARE_Write(absoluteBlock, buffer, 16);
  if (status != MFRC522::STATUS_OK) {
    Sprint(F("MIFARE_Write() failed: "));
    Sprintln(_mfrc522.GetStatusCodeName(status));
    return false;
  }
  return true;
}

/**
   read a block from a rfid card into outputBuffer which needs to be >= 18 bytes long
*/
bool ShelfRfid::readRfidBlock(uint8_t sector, uint8_t relativeBlock, uint8_t *outputBuffer, uint8_t bufferSize) {
  if (relativeBlock > 3) {
    Sprintln(F("Invalid block number"));
    return false;
  }

  uint8_t absoluteBlock = (sector * 4) + relativeBlock;

  // Authenticate using key A
  Sprintln(F("Authenticating using key A..."));
  MFRC522::StatusCode status = (MFRC522::StatusCode) _mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, absoluteBlock, &_key, &(_mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Sprint(F("PCD_Authenticate() failed: "));
    Sprintln(_mfrc522.GetStatusCodeName(status));
    return false;
  }

  Sprint(F("Reading block "));
  Sprintln(absoluteBlock);
  status = _mfrc522.MIFARE_Read(absoluteBlock, outputBuffer, &bufferSize);
  if (status != MFRC522::STATUS_OK) {
    Sprint(F("MIFARE_Read() failed: "));
    Sprintln(_mfrc522.GetStatusCodeName(status));
    return false;
  }
  Sprint(F("Data in block ")); Sprint(absoluteBlock); Sprintln(F(":"));
  print_byte_array(outputBuffer, 16);
  Sprintln();
  Sprintln();

  return true;
}


void ShelfRfid::print_byte_array(const uint8_t *buffer, const uint8_t  bufferSize) {
#ifdef DEBUG_OUTPUT
  for (uint8_t i = 0; i < bufferSize; i++) {
    Sprint(buffer[i] < 0x10 ? " 0" : " ");
    // Use Serial.print here because of HEX parameter and ifdef around this
    Serial.print(buffer[i], HEX);
  }
  Sprintln();
#endif
}
