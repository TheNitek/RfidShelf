#include "ShelfWeb.h"

void ShelfWeb::begin() {
  std::function<void(void)> defaultCallback = std::bind(&ShelfWeb::_handleDefault, this);
  _server.on(
    "/",
    HTTP_POST,
    defaultCallback,
    std::bind(&ShelfWeb::_handleFileUpload, this)
  );
  _server.onNotFound(defaultCallback);
  // Done by espalexa
  //_server.begin();

  _alexaDevice.setPercent(2*(50-_playback.volume()));
  _espalexa.addDevice(&_alexaDevice);
  _espalexa.begin(&_server);

  _playback.callback = std::bind(&ShelfWeb::_playbackCallback, this, std::placeholders::_1, std::placeholders::_2);

  MDNS.addService("http", "tcp", 80);
}

void ShelfWeb::_deviceCallback(EspalexaDevice* device) {
  Sprintf("Got device callback type: %d\n", device->getLastChangedProperty());

  switch(device->getLastChangedProperty()) {
    case EspalexaDeviceProperty::off:
      if(_playback.playbackState() == PLAYBACK_FILE) {
        _playback.pausePlayback();
      }
      break;
    case EspalexaDeviceProperty::on: 
      if(_playback.playbackState() == PLAYBACK_PAUSED) {
        _playback.playingByCard = false;
        _playback.resumePlayback();
        device->setPercent(2*(50-_playback.volume()));
      }
      break;
    case EspalexaDeviceProperty::bri:
      _playback.volume(50-device->getPercent()/2, false);
      break;
    default:
      Sprintln(F("Callback ignored"));
  }
}

void ShelfWeb::_playbackCallback(PlaybackState state, uint8_t volume) {
  if(state == PLAYBACK_FILE) {
    _alexaDevice.setPercent(2*(50-_playback.volume()));
  } else {
    _alexaDevice.setValue(0);
  }
}

void ShelfWeb::_returnOK() {
  _server.send_P(200, PSTR("text/plain"), NULL);
}

void ShelfWeb::_returnHttpStatus(const uint16_t statusCode, const char *msg) {
  _server.send_P(statusCode, PSTR("text/plain"), msg);
}

void ShelfWeb::_sendHTML() {
  _server.sendHeader(F("Content-Encoding"), F("gzip"));
  _server.send_P(200, PSTR("text/html"), ShelfHtml::INDEX, ShelfHtml::INDEX_SIZE);
}

void ShelfWeb::_sendJsonStatus() {
  char output[768] = "{\"playback\":\"";
  char buffer[101];

  if(_playback.playbackState() == PLAYBACK_FILE) {
    strcat_P(output, PSTR("FILE\""));
  } else if(_playback.playbackState() == PLAYBACK_PAUSED) {
    strcat_P(output, PSTR("PAUSED\""));
  } else {
    strcat_P(output, PSTR("NO\""));
  }

  if(_rfid.hasActivePairing) {
    NFCTagObject pairingConfig = _rfid.getPairingConfig();
    strcat_P(output, PSTR(",\"pairing\":\""));
    strcat(output, pairingConfig.folder);
    strcat(output, "\"");
  }

  if(_playback.playbackState() != PLAYBACK_NO) {
    _playback.currentFile(buffer, sizeof(buffer));
    strcat_P(output, PSTR(",\"currentFile\":\""));
    strcat(output, buffer);
    _playback.currentFolder(buffer, sizeof(buffer));
    strcat_P(output, PSTR("\",\"currentFolder\":\""));
    strcat(output, buffer);
    strcat(output, "\"");
  }

  strcat_P(output, PSTR(",\"volume\":"));
  snprintf(buffer, sizeof(buffer), "%d", 50 - _playback.volume());
  strcat(output, buffer);

  if (_SD.exists("/patches.053")) {
    strcat_P(output, PSTR(",\"patch\":true"));
  } else {
    strcat_P(output, PSTR(",\"patch\":false"));
  }

  if(_playback.isNight()) {
    strcat_P(output, PSTR(",\"night\":true"));
  } else {
    strcat_P(output, PSTR(",\"night\":false"));
  }

  if(_playback.isShuffle()) {
    strcat_P(output, PSTR(",\"shuffle\":true"));
  } else {
    strcat_P(output, PSTR(",\"shuffle\":false"));
  }

  strcat_P(output, PSTR(",\"time\":"));
  snprintf(buffer, sizeof(buffer), "%lu", time(nullptr));
  strcat(output, buffer);

  strcat_P(output, PSTR(",\"uptime\":"));
  snprintf(buffer, sizeof(buffer), "%lu", (millis()/(1000*60)));
  strcat(output, buffer);

  strcat_P(output, PSTR(",\"version\":"));
  snprintf_P(buffer, sizeof(buffer), PSTR("\"%d.%d\""), MAJOR_VERSION, MINOR_VERSION);
  strcat(output, buffer);
  strcat(output, "}");

  _server.send_P(200, PSTR("application/json"), output);
}

void ShelfWeb::_sendJsonFSUsage() {
  uint32_t free = (uint32_t)(0.000512*_SD.vol()->freeClusterCount()*_SD.vol()->blocksPerCluster());
  uint32_t total = (uint32_t)(0.000512*_SD.card()->cardCapacity());

  // This is too slow on bigger cards, so it needs to be moved somewhere else
  char output[51] = "";
  snprintf_P(output, sizeof(output), PSTR("{\"free\":%u,\"total\":%u}"), free, total);
  _server.send_P(200, PSTR("application/json"), output);
}

void ShelfWeb::_sendJsonConfig() {
  char output[768] = "{\"host\":\"";
  char buffer[101];

  PGM_P t = PSTR("true");
  PGM_P f = PSTR("false");

  PGM_P tk = PSTR("true,");
  PGM_P fk = PSTR("false,");


  strcat(output, _config.hostname);
  strcat_P(output, PSTR("\",\"ntp\":\""));
  strcat(output, _config.ntpServer);
  strcat_P(output, PSTR("\",\"tz\":\""));
  strcat(output, _config.timezone);
  strcat_P(output, PSTR("\",\"volume\":"));
  snprintf(buffer, sizeof(buffer), "%d", _config.defaultVolumne);
  strcat(output, buffer);
  strcat_P(output, PSTR(",\"repeat\":"));
  strcat_P(output, _config.defaultRepeat ? t : f);
  strcat_P(output, PSTR(",\"shuffle\":"));
  strcat_P(output, _config.defaultShuffle ? t : f);
  strcat_P(output, PSTR(",\"stopOnRemove\":"));
  strcat_P(output, _config.defaultStopOnRemove ? t : f);
  strcat_P(output, PSTR(",\"nightMode\":["));
  for(uint8_t i = 0; i < sizeof(_config.nightModeTimes)/sizeof(Timeslot_t); i++) {
    strcat_P(output, PSTR("{\"days\":["));
    strcat_P(output, _config.nightModeTimes[i].monday ? tk : fk);
    strcat_P(output, _config.nightModeTimes[i].tuesday ? tk : fk);
    strcat_P(output, _config.nightModeTimes[i].wednesday ? tk : fk);
    strcat_P(output, _config.nightModeTimes[i].thursday ? tk : fk);
    strcat_P(output, _config.nightModeTimes[i].friday ? tk : fk);
    strcat_P(output, _config.nightModeTimes[i].saturday ? tk : fk);
    strcat_P(output, _config.nightModeTimes[i].sunday ? t : f);
    strcat_P(output, PSTR("],\"start\":\""));
    snprintf_P(buffer, sizeof(buffer), PSTR("%02d:%02d"), _config.nightModeTimes[i].startHour, _config.nightModeTimes[i].startMinutes);
    strcat(output, buffer);
    strcat_P(output, PSTR("\",\"end\":\""));
    snprintf_P(buffer, sizeof(buffer), PSTR("%02d:%02d"), _config.nightModeTimes[i].endHour, _config.nightModeTimes[i].endMinutes);
    strcat(output, buffer);
    strcat(output, "\"}");

    if(i < sizeof(_config.nightModeTimes)/sizeof(Timeslot_t) - 1) {
      strcat(output, ",");
    }
  }
  strcat(output, "]}");

  _server.send_P(200, PSTR("application/json"), output);
}

void ShelfWeb::_sendJsonFS(const char *path) {
  _server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  _server.send_P(200, PSTR("application/json"), PSTR("{\"fs\":["));

  sdfat::SdFile dir;
  dir.open(path, sdfat::O_READ);

  sdfat::SdFile entry;

  char buffer[101];
  char output[256];

  bool first = true;

  while (entry.openNext(&dir, sdfat::O_READ)) {
    if(first) {
      first = false;
      strcpy_P(output, PSTR("{\"name\":\""));
    } else {
      strcpy_P(output, PSTR(",{\"name\":\""));
    }
    entry.getName(buffer, sizeof(buffer));
    // TODO encode special characters
    strcat(output, buffer);
    if (entry.isDir()) {
      strcat(output, "\"");
    } else {
      strcat_P(output, PSTR("\",\"size\":"));
      snprintf(buffer, sizeof(buffer), "%lu", (unsigned long) entry.fileSize());
      strcat(output, buffer);
    }
    entry.close();
    strcat(output, "}");
    _server.sendContent_P(output);
  }
  _server.sendContent_P("],");
  snprintf_P(output, sizeof(output), PSTR("\"path\":\"%s\""), path);
  _server.sendContent_P(output);
  _server.sendContent_P("}");
  _server.sendContent_P("");

  dir.close();
}

bool ShelfWeb::_loadFromSdCard(const char *path) {
  sdfat::File dataFile = _SD.open(path);

  if (!dataFile) {
    Sprintln(F("File not open"));
    _returnHttpStatus(404, PSTR("Not found"));
    return false;
  }

  if (dataFile.isDir()) {
    _sendJsonFS(path);
  } else {
    if (_server.streamFile(dataFile, F("application/octet-stream")) != dataFile.size()) {
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
    httpClient.end();
    _returnHttpStatus(500, httpClient.errorToString(httpCode).c_str());
    return;
  }
  if (httpCode != HTTP_CODE_OK) {
    httpClient.end();
    char buffer[32];
    snprintf_P(buffer, sizeof(buffer), PSTR("invalid response code: %d"), httpCode);
    _returnHttpStatus(500, buffer);
    return;
  }

  sdfat::File patchFile("/patches.053", sdfat::O_WRITE | sdfat::O_CREAT);
  if(!patchFile.isOpen()) {
    httpClient.end();
    _returnHttpStatus(500, PSTR("Could not open patch file"));
    return;
  }
  patchFile.close();
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

    if (!filename.startsWith("/")) {
      Sprintln(F("Invalid upload target"));
      return;
    }

    if (_SD.exists(filename.c_str())) {
      Sprintf("File %s already exists. Skipping", filename.c_str());
      return;
    }

    if(_playback.playbackState() == PLAYBACK_FILE) {
      Sprintln(F("Pausing playback for upload"));
      _playback.pausePlayback();
    }

    _uploadFile.open(filename.c_str(), sdfat::O_WRITE | sdfat::O_CREAT);
    _uploadStart = millis();
    Sprint(F("Upload start: "));
    Sprintln(filename);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (_uploadFile.isOpen()) {
      _uploadFile.write(upload.buf, upload.currentSize);
      //Sprint("Upload write: "));
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
  Sprintf("Request to: %s\n", path.c_str());
  if(_espalexa.handleAlexaApiCall(_server.uri(), _server.arg(F("plain")))) {
    return;
  }
  if(_server.method() == HTTP_GET) {
    if(_server.hasArg(F("status"))) {
      _sendJsonStatus();
      return;
    }
    if(_server.hasArg(F("config"))) {
      _sendJsonConfig();
      return;
    }
    if(_server.hasArg(F("usage"))) {
      _sendJsonFSUsage();
      return;
    }
    if(path == "/" && !_server.hasArg("fs")) {
      _sendHTML();
      return;
    }

    // start file download
    _loadFromSdCard(path.c_str());
    return;
  } else if(_server.method() == HTTP_DELETE) {
    if(path == "/" || !_SD.exists(path.c_str())) {
      _returnHttpStatus(400, PSTR("Bad path"));
      return;
    }

    sdfat::SdFile file;
    file.open(path.c_str());
    if(file.isDir()) {
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
  } else if(_server.method() == HTTP_POST) {
    if(_server.hasArg(F("newFolder"))) {
      Sprint(F("Creating folder ")); Sprintln(_server.arg(F("newFolder")));
      if(_SD.mkdir(_server.arg(F("newFolder")).c_str())) {
        _SD.cacheClear();
        _returnOK();
      } else {
        Sprintln(F("Folder creation failed"));
        _returnHttpStatus(500, PSTR("Folder creation failed"));
      }
      return;
    } else if(_server.hasArg(F("ota"))) {
      Sprint(F("Starting OTA from ")); Sprintln(_server.arg(F("ota")));
      std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
      // do not validate certificate
      client->setInsecure();
      t_httpUpdate_return ret = ESPhttpUpdate.update(*client, _server.arg(F("ota")));
      switch (ret) {
        case HTTP_UPDATE_FAILED:
          Sprintf("HTTP_UPDATE_FAILD Error (%d): ", ESPhttpUpdate.getLastError());
          Sprintln(ESPhttpUpdate.getLastErrorString().c_str());
          _returnHttpStatus(500, PSTR("Update failed, please try again"));
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
    } else if(_server.hasArg(F("downloadpatch"))) {
      _downloadPatch();
      return;
    } else if(_server.hasArg(F("stop"))) {
      _playback.stopPlayback();
      _sendJsonStatus();
      return;
    } else if(_server.hasArg(F("pause"))) {
      _playback.pausePlayback();
      _sendJsonStatus();
      return;
    } else if(_server.hasArg(F("resume"))) {
      _playback.playingByCard = false;
      _playback.resumePlayback();
      _sendJsonStatus();
      return;
    } else if(_server.hasArg(F("skip"))) {
      _playback.playingByCard = false;
      _playback.skipFile();
      _sendJsonStatus();
      return;
    } else if(_server.hasArg(F("volumeUp"))) {
      _playback.volumeUp();
      _sendJsonStatus();
      return;
    } else if(_server.hasArg(F("volumeDown"))) {
      _playback.volumeDown();
      _sendJsonStatus();
      return;
    } else if(_server.hasArg(F("toggleNight"))) {
      if(_playback.isNight()) {
        _playback.stopNight();
      } else {
        _playback.startNight();
      }
      _sendJsonStatus();
      return;
    } else if(_server.hasArg(F("toggleShuffle"))) {
      if(_playback.isShuffle()) {
        _playback.stopShuffle();
      } else {
        _playback.startShuffle();
      }
      _sendJsonStatus();
      return;
    } else if(_server.uri() == "/") {
      Sprintln(F("Probably got an upload request"));
      _returnOK();
      return;
    } else if(_SD.exists(path.c_str())) {
      // <= 17 here because leading "/"" is included
      if(_server.hasArg(F("write")) && path.length() <= 17) {
        const char *target = path.c_str();
        const uint8_t volume = 50-(uint8_t)_server.arg(F("volume")).toInt();
        uint8_t repeat = _server.arg(F("repeat")).toInt();
        uint8_t shuffle = _server.arg(F("shuffle")).toInt();
        uint8_t stopOnRemove = _server.arg(F("stopOnRemove")).toInt();
        // Remove leading "/""
        target++;
        if(_rfid.startPairing(target, volume, repeat, shuffle, stopOnRemove)) {
          _sendJsonStatus();
          return;
        }
      } else if(_server.hasArg(F("play")) && _playback.switchFolder(path.c_str())) {
        _playback.startPlayback();
        _playback.playingByCard = false;
        _sendJsonStatus();
        return;
      } else if(_server.hasArg(F("playfile"))) {
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
  _returnHttpStatus(404, PSTR("Not found"));
  Sprint(F("404: ")); Sprintln(path);
}

bool ShelfWeb::isFileUploading() {
  return _uploadFile.isOpen();
}

void ShelfWeb::pause() {
  _paused = true;
  _server.stop();
}

void ShelfWeb::unpause() {
  _paused = false;
  _server.begin();
}

void ShelfWeb::work() {
  if(_paused) {
    return;
  }
  // Not needed because espalexa does it
  //_server.handleClient();
  _espalexa.loop();
}
