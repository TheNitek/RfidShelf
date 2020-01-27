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
  if ((_playback.playbackState() == PLAYBACK_FILE) && _playback.playingByCard && (_currentCard.stopOnRemove > 0)) {

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
  _print_byte_array(_mfrc522.uid.uidByte, _mfrc522.uid.size);
  Sprint(F("PICC type: "));
  MFRC522::PICC_Type piccType = _mfrc522.PICC_GetType(_mfrc522.uid.sak);
  Sprintln(_mfrc522.PICC_GetTypeName(piccType));

  // Check for compatibility
  if (piccType != MFRC522::PICC_TYPE_MIFARE_1K && piccType != MFRC522::PICC_TYPE_MIFARE_UL ) {
    Sprintln(F("Unsupported card."));
    return;
  }

  if (hasActivePairing) {
    _writeRfidBlock(1, 0, (uint8_t*) _currentCard.folder, strlen(_currentCard.folder)+1);
    _writeConfigBlock();
    hasActivePairing = false;
  }

  // Reset watchdog timer
  ESP.wdtFeed();
  _handleRfidData();

  // Halt PICC
  _mfrc522.PICC_HaltA();
  // Stop encryption on PCD
  _mfrc522.PCD_StopCrypto1();
}

void ShelfRfid::_handleRfidData() {
  uint8_t readBuffer[18];
  /* RFID Layout:
   * Mifare Classic:
   * Sector 1, Block 0: Folder
   * Sector 2, Block 0: Config
   * 
   * Mifare Ultra:
   * Page 4-7: Folder
   * Page 8-11: Config
  */
  if (!_readRfidBlock(1, 0, readBuffer, sizeof(readBuffer))) {
    // we could not read card, skip handling
    return;
  }

  // Store uid
  memcpy(_lastCardUid, _mfrc522.uid.uidByte, 4 * sizeof(uint8_t) );

  if(readBuffer[0] == '\0') {
    if(_playback.switchFolder("/")) {
      _playback.startFilePlayback("", "unknown_card.mp3");
    }
    return;
  }

  char readFolder[18];
  readFolder[0] = '/';
  // allthough sizeof(readBuffer) == 18, we only get 16 byte of data (2 bytes are CRC)
  memcpy(readFolder + 1 , readBuffer, 16 * sizeof(uint8_t) );
  // readBuffer will already contain \0 if the folder name is < 16 chars, but otherwise we need to add it
  readFolder[17] = '\0';

  strcpy(_currentCard.folder, readFolder);

  char currentFolder[101];
  currentFolder[0] = '/';
  _playback.currentFolder(currentFolder+1, sizeof(currentFolder)-1);
  if ((_playback.playbackState() != PLAYBACK_NO) && (strcmp(_currentCard.folder, currentFolder) == 0)) {
    Sprint(F("Resuming ")); Sprintln(currentFolder);
    _playback.resumePlayback();
    _playback.playingByCard = true;
  } else if (_playback.switchFolder(_currentCard.folder)) {
    _handleRfidConfig();
    _setPlaybackOptions(_currentCard.repeat, _currentCard.shuffle);
    _playback.startPlayback();
    _playback.playingByCard = true;
  }

  _dumpCurrentCard();
}

void ShelfRfid::_handleRfidConfig() {
  uint8_t configBuffer[18];
  if (!_readRfidBlock(2, 0, configBuffer, sizeof(configBuffer))) {
    // we could not read card config, skip handling
    return;
  }

  _currentCard.repeat = 0;
  _currentCard.shuffle = 0;
  _currentCard.stopOnRemove = 0;

  if(configBuffer[0] != RFID_CONFIG_VERSION) {
    // "Upgrade" card
    Sprintln(F("Found old version, upgrading card..."));
    _currentCard.volume = DEFAULT_VOLUME;
    _writeConfigBlock();
  } else {
    if(configBuffer[1] > 0) {
      _currentCard.volume = configBuffer[2];
      // check for newer config version
      if(configBuffer[1] == 2) {
        // we use same bit for some flags
        _currentCard.repeat = configBuffer[3] & B00000011;
        _currentCard.shuffle = configBuffer[3] >> 2 & B00000011;
        _currentCard.stopOnRemove = configBuffer[3] >> 4 & B00000011;
      }
    }
  }

  Sprint(F("Setting volume: ")); Sprintln(_currentCard.volume);
  _playback.volume(_currentCard.volume);
}


void ShelfRfid::_setPlaybackOptions(uint8_t repeat, uint8_t shuffle) {
  if(repeat == 1) {
    _playback.startRepeat();
  } else if(repeat == 0) {
    _playback.stopRepeat();
  }
  
  if(shuffle == 1) {
    _playback.startShuffle();
  } else if(shuffle == 0) {
    _playback.stopShuffle();
  }
}

nfcTagObject ShelfRfid::getPairingConfig() {
  return _currentCard;
}

bool ShelfRfid::startPairing(const char *folder, const uint8_t volume, const uint8_t repeat, const uint8_t shuffle, const uint8_t stopOnRemove) {
  if(strlen(folder) > 16)
    return false;
  
  strcpy(_currentCard.folder, folder);
  _currentCard.volume = volume;
  _currentCard.repeat = repeat;
  _currentCard.shuffle = shuffle;
  _currentCard.stopOnRemove = stopOnRemove;

  Sprintln(F("Pairing mode enabled"));
  _dumpCurrentCard();

  hasActivePairing = true;
  return true;
}

void ShelfRfid::_dumpCurrentCard() {
  Sprintln(F("Current card data: "));
  Sprint(F("  folder: ")); Sprintln(_currentCard.folder);
  Sprint(F("  volume: ")); Sprintln(_currentCard.volume);
  Sprint(F("  repeat: ")); Sprintln(_currentCard.repeat);
  Sprint(F("  shuffle: ")); Sprintln(_currentCard.shuffle);
  Sprint(F("  stopOnRemove: ")); Sprintln(_currentCard.stopOnRemove);
}

void ShelfRfid::_writeConfigBlock() {
  uint8_t configLength = 4;

  // Store config (like volume)
  uint8_t configBuffer[configLength];
  // Magic number to mark config block and distinguish legacy cards without it
  configBuffer[0] = RFID_CONFIG_VERSION;
  // Length of config entry without header
  configBuffer[1] = configLength-2;
  configBuffer[2] = _currentCard.volume;
  configBuffer[3] = (_currentCard.repeat << 0) | (_currentCard.shuffle << 2) | (_currentCard.stopOnRemove << 4);
  _writeRfidBlock(2, 0, configBuffer, configLength);
}

bool ShelfRfid::_writeRfidBlock(const uint8_t sector, const uint8_t relativeBlock, const uint8_t *content, const uint8_t contentSize) {
  uint8_t absoluteBlock = (sector * 4) + relativeBlock;
  MFRC522::StatusCode status;
  MFRC522::PICC_Type piccType = _mfrc522.PICC_GetType(_mfrc522.uid.sak);

  if(piccType == MFRC522::PICC_TYPE_MIFARE_1K) {
      // Authenticate using key A
    Sprintln(F("Authenticating using key A..."));
    status = _mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, absoluteBlock, &_key, &(_mfrc522.uid));
    if (status != MFRC522::STATUS_OK) {
      Sprint(F("PCD_Authenticate() failed: "));
      Sprintln(_mfrc522.GetStatusCodeName(status));
      return false;
    }
  }

  const uint8_t bufferSize = 16;
  byte buffer[bufferSize];

  uint8_t blockSize = 16;
  if(piccType == MFRC522::PICC_TYPE_MIFARE_UL) {
    blockSize = 4;
  }

  // While reading works basically the same for Mifare Classic and Ultra, Ultras need to be written in chunks of 4 bytes instead of 16
  Sprint(F("Writing data: ")); _print_byte_array(content, contentSize);
  for(uint8_t i = 0; (i < 16/blockSize) && (i*blockSize < contentSize); i++) {
    // Write block
    memset(buffer, 0, bufferSize * sizeof(uint8_t));
    memcpy(buffer, content+(i*blockSize), (((i+1)*blockSize > contentSize) ? (contentSize % blockSize) : blockSize) * sizeof(uint8_t) );

    status = _mfrc522.MIFARE_Write(absoluteBlock + i, buffer, bufferSize);
    if (status != MFRC522::STATUS_OK) {
      Sprint(F("MIFARE_Write() failed: "));
      Sprintln(_mfrc522.GetStatusCodeName(status));
      return false;
    }
  }
  return true;
}

/**
   read a block from a rfid card into outputBuffer which needs to be >= 18 bytes long
*/
bool ShelfRfid::_readRfidBlock(uint8_t sector, uint8_t relativeBlock, uint8_t *outputBuffer, uint8_t bufferSize) {
  if (relativeBlock > 3) {
    Sprintln(F("Invalid block number"));
    return false;
  }

  uint8_t absoluteBlock = (sector * 4) + relativeBlock;
  MFRC522::StatusCode status;
  MFRC522::PICC_Type piccType = _mfrc522.PICC_GetType(_mfrc522.uid.sak);

  if(piccType == MFRC522::PICC_TYPE_MIFARE_1K) {
    // Authenticate using key A
    Sprintln(F("Authenticating using key A..."));
    status = _mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, absoluteBlock, &_key, &(_mfrc522.uid));
    if (status != MFRC522::STATUS_OK) {
      Sprint(F("PCD_Authenticate() failed: "));
      Sprintln(_mfrc522.GetStatusCodeName(status));
      return false;
    }
  }

  Sprint(F("Reading block ")); Sprint(absoluteBlock); Sprintln(F(":"));
  status = _mfrc522.MIFARE_Read(absoluteBlock, outputBuffer, &bufferSize);
  if (status != MFRC522::STATUS_OK) {
    Sprint(F("MIFARE_Read() failed: ")); Sprintln(_mfrc522.GetStatusCodeName(status));
    return false;
  }
  _print_byte_array(outputBuffer, 16);
  Sprintln();
  Sprintln();

  return true;
}


void ShelfRfid::_print_byte_array(const uint8_t *buffer, const uint8_t  bufferSize) {
#ifdef DEBUG_ENABLE
  for (uint8_t i = 0; i < bufferSize; i++) {
    Sprint(buffer[i] < 0x10 ? " 0" : " ");
    // Use Serial.print here because of HEX parameter and ifdef around this
    Serial.print(buffer[i], HEX);
  }
  Sprintln();
#endif
}
