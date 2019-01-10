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
    // Enable Mono Output
    _musicPlayer.sciWrite(VS1053_REG_WRAMADDR, 0x1e09);
    _musicPlayer.sciWrite(VS1053_REG_WRAM, 0x0001);

    // Enable differential output
    /*uint16_t mode = VS1053_MODE_SM_DIFF | VS1053_MODE_SM_SDINEW;
      _musicPlayer.sciWrite(VS1053_REG_MODE, mode); */
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

  if (!SD.exists(folder)) {
    Serial.println(F("Folder does not exist"));
    return false;
  }
  SD.chdir(folder);
  SD.vwd()->rewind();
  _currentFile = "";
  return true;
}


void ShelfPlayback::stopPlayback() {
  Serial.println(F("Stopping playback"));
  if (AMP_POWER > 0) {
    digitalWrite(AMP_POWER, LOW);
  }
  if(_playing == PLAYBACK_FILE) {
    _musicPlayer.stopPlaying();
  } else if (_playing == PLAYBACK_HTTP) {
    _http.end();
  }
  _playing = PLAYBACK_NO;
}

void ShelfPlayback::playFile() {
  // IO takes time, reset watchdog timer so it does not kill us
  ESP.wdtFeed();
  SdFile file;
  SD.vwd()->rewind();

  char filenameChar[100];
  SD.vwd()->getName(filenameChar, 100);
  String dirname = "/" + String(filenameChar) + "/";

  String nextFile = "";

  while (file.openNext(SD.vwd(), O_READ))
  {

    file.getName(filenameChar, 100);
    file.close();

    if (file.isDir() || !_musicPlayer.isMP3File(filenameChar)) {
      Serial.print(F("Ignoring "));
      Serial.println(filenameChar);
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
    playFile();
    return;
  }

  // No _currentFile && no nextFile => Nothing to play!
  if (nextFile == "") {
    Serial.print(F("No mp3 files in "));
    Serial.println(dirname);
    stopPlayback();
    return;
  }

  String fullpath = dirname + nextFile;

  Serial.print(F("Playing "));
  Serial.println(fullpath);

  _playing = PLAYBACK_FILE;
  _currentFile = nextFile;
  _musicPlayer.startPlayingFile((char *)fullpath.c_str());

  if (AMP_POWER > 0) {
    digitalWrite(AMP_POWER, HIGH);
  }
}

void ShelfPlayback::volume(uint8_t volume) {
  _volume = volume;
  _musicPlayer.setVolume(_volume, _volume);
}

void ShelfPlayback::volumeUp() {
  _volume -= 5;
  if(_volume < 0) {
    _volume = 0;
  }
  _musicPlayer.setVolume(_volume, _volume);
}

void ShelfPlayback::volumeDown() {
  _volume += 5;
  if(_volume > 50) {
    _volume = 50;
  }
  _musicPlayer.setVolume(_volume, _volume);
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


/* Since there isn't much RAM to use we don't use any buffering here.
 * Using a small ring buffer here made things worse
 */
void ShelfPlayback::feedPlaybackFromHttp() {
  if(_http.connected()) {
    _reconnectCount = 0;
    // get available data size
    size_t available = _stream->available();

    // VS1053 accepts at least 32 bytes when "ready" so we can batch the data transfer
    while((available >= 32) && _musicPlayer.readyForData()) {
      uint8_t buff[32] = { 0 };
      int c = _stream->readBytes(buff, ((available > sizeof(buff)) ? sizeof(buff) : available));

      _musicPlayer.playData(buff, c);
    }
  } else {
    Serial.println(F("Reconnecting http"));
    stopPlayback();
    if(_reconnectCount < MAX_RECONNECTS) {
      playHttp();
      _reconnectCount++;
    }
  }
}


void ShelfPlayback::playHttp() {
  if(!currentStreamUrl) {
    Serial.println(F("StreamUrl not set"));
    return;
  }
  _http.begin(_wifiClient, currentStreamUrl);
  int httpCode = _http.GET();
  int len = _http.getSize();
  if ((httpCode != HTTP_CODE_OK) || !(len > 0 || len == -1)) {
    Serial.println(F("Webradio request failed"));
    return;
  }

  _stream = _http.getStreamPtr();

  _playing = PLAYBACK_HTTP;
  Serial.println(F("Initialized HTTP stream"));
}

void ShelfPlayback::work() {
  if (_playing == PLAYBACK_FILE) {
    if (_musicPlayer.playingMusic) {
      _musicPlayer.feedBuffer();
    } else {
      playFile();
    }
  }

  if (_playing == PLAYBACK_HTTP) {
    feedPlaybackFromHttp();
  }
}
