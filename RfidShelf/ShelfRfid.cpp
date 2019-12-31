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
  if ((_playback.playbackState() == PLAYBACK_FILE) && _playback.playingByCard && myCard.stopOnRemove) {

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

  // Check for compatibility
  if (piccType != MFRC522::PICC_TYPE_MIFARE_1K && piccType != MFRC522::PICC_TYPE_MIFARE_UL ) {
    Sprintln(F("Unsupported card."));
    return;
  }

  if (hasActivePairing) {
    writeRfidBlock(1, 0, (uint8_t*) myCard.folder, strlen(myCard.folder)+1);
    writeConfigBlock();
    hasActivePairing = false;
  }

  // Reset watchdog timer
  ESP.wdtFeed();
  handleRfidData();

  // Halt PICC
  _mfrc522.PICC_HaltA();
  // Stop encryption on PCD
  _mfrc522.PCD_StopCrypto1();
}

void ShelfRfid::handleRfidData() {
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
  if (!readRfidBlock(1, 0, readBuffer, sizeof(readBuffer))) {
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

  strcpy(myCard.folder, readFolder);

  char currentFolder[101];
  currentFolder[0] = '/';
  _playback.currentFolder(currentFolder+1, sizeof(currentFolder)-1);
  if ((_playback.playbackState() != PLAYBACK_NO) && (strcmp(myCard.folder, currentFolder) == 0)) {
    Sprint("Resuming "); Sprintln(currentFolder);
    _playback.resumePlayback();
    _playback.playingByCard = true;
  } else if (_playback.switchFolder(myCard.folder)) {
    handleRfidConfig();
    _playback.setPlaybackOption(myCard.repeat, myCard.shuffle);
    _playback.startPlayback();
    _playback.playingByCard = true;
  }

  dumpCurrentCard();
}

void ShelfRfid::handleRfidConfig() {
  uint8_t configBuffer[18];
  if (!readRfidBlock(2, 0, configBuffer, sizeof(configBuffer))) {
    // we could not read card config, skip handling
    return;
  }

    if(configBuffer[0] < RFID_CONFIG_VERSION) {
      // "Upgrade" card
      Sprintln(F("Found old version, upgrading card..."));
      myCard.volume = DEFAULT_VOLUME;
      myCard.repeat = false;
      myCard.shuffle = false;
      writeConfigBlock();
    } else {
      if(configBuffer[1] > 0) {
        myCard.volume = configBuffer[2];
        // check for newer config version
        if(configBuffer[1] == 2) {
          // we use same bit for some flags
          myCard.repeat = (configBuffer[3] & B00000001) > 0;
          myCard.shuffle = (configBuffer[3] & B00000010) > 0;
          myCard.stopOnRemove = (configBuffer[3] & B00000100) > 0;
        } else {
          myCard.repeat = true;
          myCard.shuffle = false;
          myCard.stopOnRemove = true;
        }
      }
    }

    Sprint(F("Setting volume: ")); Sprintln(myCard.volume);
    _playback.volume(myCard.volume);
}

nfcTagObject ShelfRfid::getPairingConfig() {
  return myCard;
}

bool ShelfRfid::startPairing(const char *folder, uint8_t volume, bool repeat, bool shuffle, bool stopOnRemove) {
  if(strlen(folder) > 16)
    return false;
  
  strcpy(myCard.folder, folder);
  myCard.volume = volume;
  myCard.repeat = repeat;
  myCard.shuffle = shuffle;
  myCard.stopOnRemove = stopOnRemove;

  Sprintln(F("Pairing mode enabled"));
  dumpCurrentCard();

  hasActivePairing = true;
  return true;
}

void ShelfRfid::dumpCurrentCard() {
  Sprintln(F("Current card data: "));
  Sprint(F("  folder: ")); Sprintln(myCard.folder);
  Sprint(F("  volume: ")); Sprintln(myCard.volume);
  Sprint(F("  repeat: ")); Sprintln(myCard.repeat);
  Sprint(F("  shuffle: ")); Sprintln(myCard.shuffle);
  Sprint(F("  stopOnRemove: ")); Sprintln(myCard.stopOnRemove);
}

void ShelfRfid::writeConfigBlock() {
  uint8_t configLength = 4;

  // Store config (like volume)
  uint8_t configBuffer[configLength];
  // Magic number to mark config block and distinguish legacy cards without it
  configBuffer[0] = RFID_CONFIG_VERSION;
  // Length of config entry without header
  configBuffer[1] = configLength-2;
  configBuffer[2] = _playback.volume();
  configBuffer[3] = 0;
  if(myCard.repeat) {
    configBuffer[3] = configBuffer[3] | B00000001;
  }
  if(myCard.shuffle) {
    configBuffer[3] = configBuffer[3] | B00000010;
  }
  if(myCard.stopOnRemove) {
    configBuffer[3] = configBuffer[3] | B00000100;
  }
  writeRfidBlock(2, 0, configBuffer, configLength);
}

bool ShelfRfid::writeRfidBlock(const uint8_t sector, const uint8_t relativeBlock, const uint8_t *content, const uint8_t contentSize) {
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
  Sprint(F("Writing data: ")); print_byte_array(content, contentSize);
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
bool ShelfRfid::readRfidBlock(uint8_t sector, uint8_t relativeBlock, uint8_t *outputBuffer, uint8_t bufferSize) {
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
  print_byte_array(outputBuffer, 16);
  Sprintln();
  Sprintln();

  return true;
}


void ShelfRfid::print_byte_array(const uint8_t *buffer, const uint8_t  bufferSize) {
#ifdef DEBUG_ENABLE
  for (uint8_t i = 0; i < bufferSize; i++) {
    Sprint(buffer[i] < 0x10 ? " 0" : " ");
    // Use Serial.print here because of HEX parameter and ifdef around this
    Serial.print(buffer[i], HEX);
  }
  Sprintln();
#endif
}
