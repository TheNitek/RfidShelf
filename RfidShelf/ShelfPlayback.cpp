#include "ShelfPlayback.h"

void ShelfPlayback::begin() {

  if (AMP_POWER > 0) {
    // Disable amp
    pinMode(AMP_POWER, OUTPUT);
    digitalWrite(AMP_POWER, LOW);
    Serial.println(F("Amp powered down"));
  }

  // initialise the music player
  if (!_musicPlayer.begin()) { // initialise the music player
    Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
    while (1) delay(500);
  }
  Serial.println(F("VS1053 found"));

  /* Fix for the design fuckup of the cheap LC Technology MP3 shield
    see http://www.bajdi.com/lcsoft-vs1053-mp3-module/#comment-33773
    Doesn't hurt for other shields
  */
  _musicPlayer.sciWrite(VS1053_REG_WRAMADDR, VS1053_GPIO_DDR);
  _musicPlayer.sciWrite(VS1053_REG_WRAM, 0x0003);
  _musicPlayer.GPIO_digitalWrite(0x0000);
  _musicPlayer.softReset();

  Serial.println(F("VS1053 soft reset done"));

  if (patchVS1053()) {
#ifdef USE_DIFFERENTIAL_OUTPUT
    // Enable differential output
    uint16_t mode = VS1053_MODE_SM_DIFF | VS1053_MODE_SM_SDINEW;
    _musicPlayer.sciWrite(VS1053_REG_MODE, mode);
#else
    // Enable Mono Output
    _musicPlayer.sciWrite(VS1053_REG_WRAMADDR, 0x1e09);
    _musicPlayer.sciWrite(VS1053_REG_WRAM, 0x0001);
#endif
    Serial.println(F("VS1053 patch installed"));
  } else {
    Serial.println(F("Could not load patch"));
  }

  Serial.print(F("SampleRate "));
  Serial.println(_musicPlayer.sciRead(VS1053_REG_AUDATA));

  _musicPlayer.setVolume(_volume, _volume);

  setBassAndTreble(TREBLE_AMPLITUDE, TREBLE_FREQLIMIT, BASS_AMPLITUDE, BASS_FREQLIMIT);

  _musicPlayer.dumpRegs();

  Serial.println(F("VS1053 found"));
}


bool ShelfPlayback::switchFolder(const char *folder) {
  Serial.print(F("Switching folder to "));
  Serial.println(folder);

  if (!_SD.exists(folder)) {
    Serial.println(F("Folder does not exist"));
    return false;
  }
  stopPlayback();
  _SD.chdir(folder);
  _SD.vwd()->rewind();
  _currentFile = "";
  return true;
}


void ShelfPlayback::stopPlayback() {
  Serial.println(F("Stopping playback"));
  if(_playing == PLAYBACK_NO) {
    Serial.println(F("Already stopped"));
    return;
  }
  
  if (AMP_POWER > 0) {
    digitalWrite(AMP_POWER, LOW);
  }
  if(_playing == PLAYBACK_FILE) {
    _musicPlayer.stopPlaying();
  }
  _playing = PLAYBACK_NO;
  if(isNight()) {
    _lastNightActivity = millis();
  }
}

void ShelfPlayback::startPlayback() {
  // IO takes time, reset watchdog timer so it does not kill us
  ESP.wdtFeed();
  SdFile file;
  _SD.vwd()->rewind();

  char filenameChar[100];
  _SD.vwd()->getName(filenameChar, 100);
  String dirname = "/" + String(filenameChar) + "/";

  String nextFile = "";

  while (file.openNext(_SD.vwd(), O_READ))
  {

    file.getName(filenameChar, 100);
    file.close();

    if (file.isDir() || !_musicPlayer.isMP3File(filenameChar)) {
      Serial.print(F("Ignoring ")); Serial.println(filenameChar);
      continue;
    }

    String tmpFile = String(filenameChar);
    if (_currentFile < tmpFile && (tmpFile < nextFile || nextFile == "")) {
      nextFile = tmpFile;
    }
  }

  // Start folder from the beginning
  if (nextFile == "" && _currentFile != "") {
    _currentFile = "";
    startPlayback();
    return;
  }

  // No _currentFile && no nextFile => Nothing to play!
  if (nextFile == "") {
    Serial.print(F("No mp3 files in ")); Serial.println(dirname);
    stopPlayback();
    return;
  }

  startFilePlayback((char *)dirname.c_str(), (char *)nextFile.c_str());
}

void ShelfPlayback::startFilePlayback(const char* folder, const char* nextFile) {
  Serial.print(F("Playing ")); Serial.print(folder); Serial.println(nextFile);

  _playing = PLAYBACK_FILE;
  _currentFile = nextFile;
  
  char fullPath[116] = "";
  strcat(fullPath, folder);
  strcat(fullPath, nextFile);
  _musicPlayer.startPlayingFile(fullPath);

  if (AMP_POWER > 0) {
    digitalWrite(AMP_POWER, HIGH);
  }
}

void ShelfPlayback::skipFile() {
  _musicPlayer.stopPlaying();
  startPlayback();
}

void ShelfPlayback::volume(uint8_t volume) {
  if(volume > 50) {
    _volume = 50;
  } else {
    _volume = volume;
  }

  uint8_t calcVolume = volume;
  
  if(isNight()) {
    calcVolume = 50 - (NIGHT_FACTOR * (50 - _volume));
  }
  _musicPlayer.setVolume(calcVolume, calcVolume);
}

void ShelfPlayback::volumeUp() {
  if(_volume < 5) {
    volume(0);
  } else {
    volume(_volume - 5);
  }
}

void ShelfPlayback::volumeDown() {
  volume(_volume + 5);
}


void ShelfPlayback::setBassAndTreble(uint8_t trebleAmplitude, uint8_t trebleFreqLimit, uint8_t bassAmplitude, uint8_t bassFreqLimit) {
  uint16_t bassReg = 0;
  bassReg |= trebleAmplitude;
  bassReg <<= 4;
  bassReg |= trebleFreqLimit;
  bassReg <<= 4;
  bassReg |= bassAmplitude;
  bassReg <<= 4;
  bassReg |= bassFreqLimit;
  Serial.printf(F("bass value: %04x\n"), bassReg);
  _musicPlayer.sciWrite(VS1053_REG_BASS, bassReg);
}

bool ShelfPlayback::patchVS1053() {
  Serial.println(F("Installing patch to VS1053"));

  SdFile file;
  if (!file.open("patches.053", O_READ)) return false;

  uint16_t addr, n, val, i = 0;

  while (file.read(&addr, 2) && file.read(&n, 2)) {
    i += 2;
    if (n & 0x8000U) {
      n &= 0x7FFF;
      if (!file.read(&val, 2)) {
        file.close();
        return false;
      }
      while (n--) {
        _musicPlayer.sciWrite(addr, val);
      }
    } else {
      while (n--) {
        if (!file.read(&val, 2)) {
          file.close();
          return false;
        }
        i++;
        _musicPlayer.sciWrite(addr, val);
      }
    }
  }
  file.close();

  Serial.print(F("Number of bytes: ")); Serial.println(i);
  return true;
}

void ShelfPlayback::work() {
  if (_playing == PLAYBACK_FILE) {
    if (_musicPlayer.playingMusic) {
      _musicPlayer.feedBuffer();
    } else {
      startPlayback();
    }
    return;
  }

  if ((_playing == PLAYBACK_NO) && isNight() && (millis() - _lastNightActivity > NIGHT_TIMEOUT)) {
    stopNight();
  }
}

void ShelfPlayback::startNight() {
  _lastNightActivity = millis();
  _nightMode = true;
  volume(_volume);
}

bool ShelfPlayback::isNight() {
  return _nightMode;
}

void ShelfPlayback::stopNight() {
  _nightMode = false;
  volume(_volume);
}
