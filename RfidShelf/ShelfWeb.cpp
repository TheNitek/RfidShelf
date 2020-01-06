#include "ShelfWeb.h"

// This sucks - Maybe refactor ShelfWeb to singleton
ShelfWeb *ShelfWeb::_instance;

ShelfWeb::ShelfWeb(ShelfPlayback &playback, ShelfRfid &rfid, SdFat &sd, NTPClient &timeClient) : _playback(playback), _rfid(rfid), _SD(sd), _timeClient(timeClient) {
  _instance = this;
}

void ShelfWeb::defaultCallback() {
  _instance->_handleDefault();
}

void ShelfWeb::fileUploadCallback() {
  _instance->_handleFileUpload();
}

void ShelfWeb::begin() {
  _server.on("/", HTTP_POST, defaultCallback, fileUploadCallback);
  _server.onNotFound(defaultCallback);

  _server.begin();

  MDNS.addService("http", "tcp", 80);
}

void ShelfWeb::_returnOK() {
  _server.send_P(200, "text/plain", NULL);
}

void ShelfWeb::_returnHttpStatus(uint16_t statusCode, const char *msg) {
  _server.send_P(statusCode, "text/plain", msg);
}

void ShelfWeb::_sendHTML() {
  _server.sendHeader("Content-Encoding", "gzip");
  _server.send_P(200, "text/html", ShelfHtml::INDEX, ShelfHtml::INDEX_SIZE);
}


void ShelfWeb::_sendJsonStatus() {
  char output[512] = "{\"playback\":\"";
  char buffer[101];

  if(_playback.playbackState() == PLAYBACK_FILE) {
    strcat(output, "FILE\"");
  } else if(_playback.playbackState() == PLAYBACK_PAUSED) {
    strcat(output, "PAUSED\"");
  } else {
    strcat(output, "NO\"");
  }

  if(_rfid.hasActivePairing) {
    nfcTagObject pairingConfig = _rfid.getPairingConfig();
    strcat(output, ",\"pairing\":\"");
    strcat(output, pairingConfig.folder);
    strcat(output, "\"");
  }

  if(_playback.playbackState() != PLAYBACK_NO) {
    _playback.currentFile(buffer, sizeof(buffer));
    strcat(output, ",\"currentFile\":\"");
    strcat(output, buffer);
    strcat(output, "\"");
  }

  strcat(output, ",\"volume\":");
  snprintf(buffer, sizeof(buffer), "%d", 50 - _playback.volume());
  strcat(output, buffer);

  if (_SD.exists("/patches.053")) {
    strcat(output, ",\"patch\":true");
  } else {
    strcat(output, ",\"patch\":false");
  }

  if(_playback.isNight()) {
    strcat(output, ",\"night\":true");
  } else {
    strcat(output, ",\"night\":false");
  }

  if(_playback.defaultShuffleMode) {
    strcat(output, ",\"shuffle\":true");
  } else {
    strcat(output, ",\"shuffle\":false");
  }

  if(_playback.isShuffle()) {
    strcat(output, ",\"currentShuffle\":true");
  } else {
    strcat(output, ",\"currentShuffle\":false");
  }

  strcat(output, ",\"time\":");
  snprintf(buffer, sizeof(buffer), "%lu", _timeClient.getEpochTime());
  strcat(output, buffer);

  // This is too slow on bigger cards, so it needs to be moved somewhere else
  /*strcat(output, ",\"sdfree\":");
  snprintf(buffer, sizeof(buffer), "%u", (uint32_t)(0.000512*_SD.vol()->freeClusterCount()*_SD.vol()->blocksPerCluster()));
  strcat(output, buffer);

  strcat(output, ",\"sdsize\":");
  snprintf(buffer, sizeof(buffer), "%u", (uint32_t)(0.000512*_SD.card()->cardCapacity()));
  strcat(output, buffer);*/


  strcat(output, ",\"version\":");
  snprintf(buffer, sizeof(buffer), "\"%d.%d\"", MAJOR_VERSION, MINOR_VERSION);
  strcat(output, buffer);
  strcat(output, "}");

  _server.send_P(200, "application/json", output);
}

void ShelfWeb::_sendJsonFS(const char *path) {
  _server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  _server.send_P(200, "application/json", "{\"fs\":[");

  SdFile dir;
  dir.open(path, O_READ);
  dir.rewind();

  SdFile entry;

  char buffer[101];
  char output[256];

  bool first = true;

  while (entry.openNext(&dir, O_READ)) {
    if(first) {
      first = false;
      strcpy(output, "{\"name\":\"");
    } else {
      strcpy(output, ",{\"name\":\"");
    }
    entry.getName(buffer, sizeof(buffer));
    // TODO encode special characters
    strcat(output, buffer);
    if (entry.isDir()) {
      strcat(output, "\"");
    } else {
      strcat(output, "\",\"size\":");
      snprintf(buffer, sizeof(buffer), "%lu", (unsigned long) entry.fileSize());
      strcat(output, buffer);
    }
    entry.close();
    strcat(output, "}");
    _server.sendContent_P(output);
  }
  _server.sendContent_P("],");
  snprintf(output, sizeof(output), "\"path\":\"%s\"", path);
  _server.sendContent_P(output);
  _server.sendContent_P("}");
  _server.sendContent_P("");

  dir.close();
}

bool ShelfWeb::_loadFromSdCard(const char *path) {
  File dataFile = _SD.open(path);

  if (!dataFile) {
    Sprintln(F("File not open"));
    _returnHttpStatus(404, "Not found");
    return false;
  }

  if (dataFile.isDir()) {
    _sendJsonFS(path);
  } else {
    if (_server.streamFile(dataFile, "application/octet-stream") != dataFile.size()) {
      Sprintln(F("Sent less data than expected!"));
    }
  }
  dataFile.close();
  return true;
}

void ShelfWeb::_downloadPatch() {
  Sprintln(F("Starting patch download"));
  std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
  // do not validate certificate
  client->setInsecure();
  HTTPClient httpClient;
  httpClient.begin(*client, VS1053_PATCH_URL);
  int httpCode = httpClient.GET();
  if (httpCode < 0) {
    _returnHttpStatus(500, httpClient.errorToString(httpCode).c_str());
    return;
  }
  if (httpCode != HTTP_CODE_OK) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "invalid response code: %d", httpCode);
    _returnHttpStatus(500, buffer);
    return;
  }
  int len = httpClient.getSize();
  WiFiClient* stream = httpClient.getStreamPtr();
  uint8_t buffer[128] = { 0 };
  SdFile patchFile;
  patchFile.open("/patches.053", O_WRITE | O_CREAT);
  while (httpClient.connected() && (len > 0 || len == -1)) {
    // get available data size
    size_t size = stream->available();
    if (size) {
      // read til buffer is full
      int c = stream->readBytes(buffer, ((size > sizeof(buffer)) ? sizeof(buffer) : size));
      // write the buffer to our patch file
      if (patchFile.isOpen()) {
        patchFile.write(buffer, c);
      }
      // reduce len until we the end (= zero) if len not -1
      if (len > 0) {
        len -= c;
      }
    }
    delay(1);
  }
  if (patchFile.isOpen()) {
    patchFile.close();
  }
  _returnOK();
  _server.client().flush();
  ESP.restart();
}

void ShelfWeb::_handleFileUpload() {
  // Upload always happens on /
  if (_server.uri() != "/") {
    Sprintln(F("Invalid upload URI"));
    return;
  }

  HTTPUpload& upload = _server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;

    if (!Adafruit_VS1053_FilePlayer::isMP3File((char *)filename.c_str())) {
      Sprint(F("Not a MP3: ")); Sprintln(filename);
      return;
    }

    if (!filename.startsWith("/")) {
      Sprintln(F("Invalid upload target"));
      return;
    }

    if (_SD.exists(filename.c_str())) {
      Sprintln("File " + filename + " already exists. Skipping");
      return;
    }

    _uploadFile.open(filename.c_str(), O_WRITE | O_CREAT);
    _uploadStart = millis();
    Sprint(F("Upload start: "));
    Sprintln(filename);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (_uploadFile.isOpen()) {
      _uploadFile.write(upload.buf, upload.currentSize);
      //Sprint(F("Upload write: "));
      //Sprintln(upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (_uploadFile.isOpen()) {
      _uploadFile.close();
      Sprint(F("Upload end: ")); Sprintln(upload.totalSize);
      Sprint(F("Took: ")); Sprintln(((millis()-_uploadStart)/1000));
    }
  }
}

void ShelfWeb::_handleDefault() {
  String path = _server.urlDecode(_server.uri());
  Sprintf(F("Request to: %s\n"), path.c_str());
  if (_server.method() == HTTP_GET) {
    if (_server.hasArg("status")) {
      _sendJsonStatus();
      return;
    } else if(path == "/" && !_server.hasArg("fs")) {
      _sendHTML();
      return;
    } else {
      _loadFromSdCard(path.c_str());
      return;
    }
  } else if (_server.method() == HTTP_DELETE) {
    if (path == "/" || !_SD.exists(path.c_str())) {
      _returnHttpStatus(400, "Bad path");
      return;
    }

    SdFile file;
    file.open(path.c_str());
    if (file.isDir()) {
      if(!file.rmRfStar()) {
        Sprintln(F("Could not delete folder"));
      }
    } else {
      if(!_SD.remove(path.c_str())) {
        Sprintln(F("Could not delete file"));
      }
    }
    file.close();
    _returnOK();
    return;
  } else if (_server.method() == HTTP_POST) {
    if (_server.hasArg("newFolder")) {
      Sprint(F("Creating folder ")); Sprintln(_server.arg("newFolder"));
      _SD.mkdir(_server.arg("newFolder").c_str());
      _returnOK();
      return;
    } else if (_server.hasArg("ota")) {
      Sprint(F("Starting OTA from ")); Sprintln(_server.arg("ota"));
      std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
      // do not validate certificate
      client->setInsecure();
      t_httpUpdate_return ret = ESPhttpUpdate.update(*client, _server.arg("ota"));
      switch (ret) {
        case HTTP_UPDATE_FAILED:
          Sprintf("HTTP_UPDATE_FAILD Error (%d): ", ESPhttpUpdate.getLastError());
          Sprintln(ESPhttpUpdate.getLastErrorString().c_str());
          _returnHttpStatus(500, "Update failed, please try again");
          return;
        case HTTP_UPDATE_NO_UPDATES:
          Sprintln(F("HTTP_UPDATE_NO_UPDATES"));
          break;
        case HTTP_UPDATE_OK:
          Sprintln(F("HTTP_UPDATE_OK"));
          break;
      }
      _returnOK();
      return;
    } else if (_server.hasArg("downloadpatch")) {
      _downloadPatch();
      return;
    } else if (_server.hasArg("stop")) {
      _playback.stopPlayback();
      _sendJsonStatus();
      return;
    } else if (_server.hasArg("pause")) {
      _playback.pausePlayback();
      _sendJsonStatus();
      return;
    } else if (_server.hasArg("resume")) {
      _playback.playingByCard = false;
      _playback.resumePlayback();
      _sendJsonStatus();
      return;
    } else if (_server.hasArg("skip")) {
      _playback.playingByCard = false;
      _playback.skipFile();
      _sendJsonStatus();
      return;
    } else if (_server.hasArg("volumeUp")) {
      _playback.volumeUp();
      _sendJsonStatus();
      return;
    } else if (_server.hasArg("volumeDown")) {
      _playback.volumeDown();
      _sendJsonStatus();
      return;
    } else if (_server.hasArg("toggleNight")) {
      if(_playback.isNight()) {
        _playback.stopNight();
      } else {
        _playback.startNight();
      }
      _sendJsonStatus();
      return;
    } else if (_server.hasArg("toggleShuffle")) {
      if(_playback.defaultShuffleMode) {
        _playback.defaultShuffleMode = false;
        _playback.stopShuffle();
      } else {
        _playback.defaultShuffleMode = true;
        _playback.startShuffle();
      }
      _sendJsonStatus();
      return;
    } else if (_server.uri() == "/") {
      Sprintln(F("Probably got an upload request"));
      _returnOK();
      return;
    } else if (_SD.exists(path.c_str())) {
      // <= 17 here because leading "/"" is included
      if (_server.hasArg("write") && path.length() <= 17) {
        const char *target = path.c_str();
        const uint8_t volume = (uint8_t)_server.arg("volume").toInt();
        uint8_t repeat = 0;       // keep configured setting
        uint8_t shuffle = 0;      // keep configured setting
        uint8_t stopOnRemove = 0; // keep configured setting
        if(_server.arg("repeat").equals("1") || _server.arg("repeat").equals("0")) {
          repeat = 2 + _server.arg("repeat").toInt();
        }
        if(_server.arg("shuffle").equals("1") || _server.arg("shuffle").equals("0")) {
          repeat = 2 + _server.arg("shuffle").toInt();
        }
        if(_server.arg("stopOnRemove").equals("1") || _server.arg("stopOnRemove").equals("0")) {
          repeat = 2 + _server.arg("stopOnRemove").toInt();
        }
        // Remove leading "/""
        target++;
        if (_rfid.startPairing(target, volume, repeat, shuffle, stopOnRemove)) {
          _sendJsonStatus();
          return;
        }
      } else if (_server.hasArg("play") && _playback.switchFolder(path.c_str())) {
        _playback.startPlayback();
        _playback.playingByCard = false;
        _sendJsonStatus();
        return;
      } else if (_server.hasArg("playfile")) {
        char* pathCStr = (char *)path.c_str();
        char* folderRaw = strtok(pathCStr, "/");
        char* file = strtok(NULL, "/");

        if((folderRaw != NULL) && (file != NULL) && strlen(folderRaw) < 90) {

          char folder[100] = "/";
          strcat(folder, folderRaw);
          strcat(folder, "/");

          if(_playback.switchFolder(folder)) {
            _playback.startFilePlayback(folderRaw, file);
            _playback.playingByCard = false;
            _sendJsonStatus();
            return;
          }
        }
      }
    }
  }

  // 404 otherwise
  _returnHttpStatus(404, "Not found");
  Sprintln("404: " + path);
}

void ShelfWeb::work() {
  _server.handleClient();
}
