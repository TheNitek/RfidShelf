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

  _alexaDevice = new EspalexaDevice(_config.hostname, std::bind(&ShelfWeb::_deviceCallback, this, std::placeholders::_1), EspalexaDeviceType::dimmable, 50);
  _alexaDevice->setPercent(2*(50-_playback.volume()));
  _espalexa.addDevice(_alexaDevice);
  _espalexa.begin(&_server);

  _playback.callback = std::bind(&ShelfWeb::_playbackCallback, this, std::placeholders::_1, std::placeholders::_2);

  MDNS.addService("http", "tcp", 80);
}

void ShelfWeb::_deviceCallback(EspalexaDevice* device) {
  Sprintf("Got device callback type: %d\n", device->getLastChangedProperty());

  switch(device->getLastChangedProperty()) {
    case EspalexaDeviceProperty::off:
      if(_playback.playbackState() == ShelfPlayback::PLAYBACK_FILE) {
        _playback.pausePlayback();
      }
      break;
    case EspalexaDeviceProperty::on: 
      if(_playback.playbackState() == ShelfPlayback::PLAYBACK_PAUSED) {
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

void ShelfWeb::_playbackCallback(ShelfPlayback::PlaybackState state, uint8_t volume) {
  if(state == ShelfPlayback::PLAYBACK_FILE) {
    _alexaDevice->setPercent(2*(50-_playback.volume()));
  } else {
    _alexaDevice->setValue(0);
  }
}

void ShelfWeb::_returnOK() {
  _server.send_P(200, PSTR("text/plain"), "");
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

  switch(_playback.playbackState()) {
    case ShelfPlayback::PLAYBACK_FILE:
      strcat_P(output, PSTR("FILE\""));
      break;
    case ShelfPlayback::PLAYBACK_PAUSED:
      strcat_P(output, PSTR("PAUSED\""));
      break;
    case ShelfPlayback::PLAYBACK_NO:
      strcat_P(output, PSTR("NO\""));
      break;
  }

  if(_rfid.hasActivePairing) {
    ShelfRfid::NFCTagObject pairingConfig = _rfid.getPairingConfig();
    strcat_P(output, PSTR(",\"pairing\":\""));
    strcat(output, pairingConfig.folder);
    strcat(output, "\"");
  }

  if(_playback.playbackState() != ShelfPlayback::PLAYBACK_NO) {
    strcat_P(output, PSTR(",\"currentFile\":\""));
    _playback.currentFile(buffer, sizeof(buffer));
    strcat(output, buffer);
    strcat_P(output, PSTR("\",\"currentFolder\":\""));
    _playback.currentFolder(buffer, sizeof(buffer));
    strcat(output, buffer);
    strcat(output, "\"");
  }

  strcat_P(output, PSTR(",\"volume\":"));
  snprintf(buffer, sizeof(buffer), "%d", 50 - _playback.volume());
  strcat(output, buffer);

  _SD.exists("/patches.053") ? strcat_P(output, PSTR(",\"patch\":true")) : strcat_P(output, PSTR(",\"patch\":false"));

  _playback.isNight()? strcat_P(output, PSTR(",\"night\":true")) : strcat_P(output, PSTR(",\"night\":false"));

  _playback.isShuffle() ? strcat_P(output, PSTR(",\"shuffle\":true")) : strcat_P(output, PSTR(",\"shuffle\":false"));

  snprintf_P(buffer, sizeof(buffer), PSTR(",\"time\":%llu"), time(nullptr));
  strcat(output, buffer);

  snprintf_P(buffer, sizeof(buffer), PSTR(",\"uptime\":%lu"), (millis()/(1000*60)));
  strcat(output, buffer);

  snprintf_P(buffer, sizeof(buffer), PSTR(",\"version\":\"%d.%d\"}"), MAJOR_VERSION, MINOR_VERSION);
  strcat(output, buffer);

  _server.send_P(200, PSTR("application/json"), output);
}

void ShelfWeb::_sendJsonFSUsage() {
  sdfat::csd_t m_csd;
  if(!_SD.card()->readCSD(&m_csd)) {
    Sprintln(PSTR("Could not read card info"));
    _returnHttpStatus(500, PSTR("Could not read card info"));
    return;
  }
  uint32_t free = (uint32_t)(0.000512*_SD.vol()->freeClusterCount()*_SD.vol()->sectorsPerCluster());
  uint32_t total = (uint32_t)(0.000512 * sdCardCapacity(&m_csd));

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

  sdfat::File32 dir = _SD.open(path, sdfat::O_READ);
  sdfat::File32 entry;

  char buffer[101];
  char output[256];

  bool first = true;

  while (entry.openNext(&dir, sdfat::O_READ)) {
    strcpy_P(output, PSTR(",{\"name\":\""));
    entry.getName(buffer, sizeof(buffer));
    // TODO encode special characters
    strcat(output, buffer);
    if (entry.isDir()) {
      strcat(output, "\"");
    } else {
      snprintf_P(buffer, sizeof(buffer), PSTR("\",\"size\":%lu"), entry.fileSize());
      strcat(output, buffer);
    }
    entry.close();
    strcat(output, "}");
    if(first) {
      first = false;
      _server.sendContent_P(output+1);
    } else {
      _server.sendContent_P(output);
    }
  }
  snprintf_P(output, sizeof(output), PSTR("],\"path\":\"%s\"}"), path);
  _server.sendContent_P(output);
  _server.sendContent_P("");

  dir.close();
}

bool ShelfWeb::_loadFromSdCard(const char *path) {
  sdfat::File32 dataFile = _SD.open(path);

  if (!dataFile.isOpen()) {
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

  sdfat::File32 patchFile = _SD.open("/patches.053", sdfat::O_WRITE | sdfat::O_CREAT);
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

void ShelfWeb::_handleDelete(const char *path) {
  if((strcmp(path, "/") == 0) || !_SD.exists(path)) {
    _returnHttpStatus(400, PSTR("Bad path"));
    return;
  }

  sdfat::File32 file = _SD.open(path);
  if(file.isDir()) {
    // Check if deleting the currently playing folder
    if(_playback.playbackState() != ShelfPlayback::PLAYBACK_NO) {
      char sfn[13];
      char playSfn[13];
      file.getSFN(sfn);
      _playback.currentFolderSFN(playSfn);
      if(strcmp(sfn, playSfn) == 0) {
        _playback.stopPlayback();
      }
    }
    if(!file.rmRfStar()) {
      Sprintln(F("Could not delete folder"));
    }
  } else {
    // Maybe Check if deleting the currently playing file?
    if(!_SD.remove(path)) {
      Sprintln(F("Could not delete file"));
    }
  }
  file.close();
  _returnOK();
  return;
}

void ShelfWeb::_handleFileUpload() {
  // Upload always happens on /
  if (_server.uri() != "/") {
    Sprintln(F("Invalid upload URI"));
    return;
  }

  HTTPUpload& upload = _server.upload();

  switch(upload.status) {
    case UPLOAD_FILE_START:
      if (!upload.filename.startsWith("/")) {
        Sprintln(F("Invalid upload target"));
        return;
      }

      if (_SD.exists(upload.filename.c_str())) {
        Sprintf("File %s already exists. Skipping", upload.filename.c_str());
        return;
      }

      if(_playback.playbackState() == ShelfPlayback::PLAYBACK_FILE) {
        Sprintln(F("Pausing playback for upload"));
        _playback.pausePlayback();
      }

      _uploadFile = _SD.open(upload.filename.c_str(), sdfat::O_WRITE | sdfat::O_CREAT);
      _uploadStart = millis();
      Sprint(F("Upload start: "));
      Sprintln(upload.filename);
      break;
    case UPLOAD_FILE_WRITE:
      if (_uploadFile.isOpen()) {
        _uploadFile.write(upload.buf, upload.currentSize);
        //Sprint("Upload write: "));
        //Sprintln(upload.currentSize);
      }
      break;
    case UPLOAD_FILE_END:
      if (_uploadFile.isOpen()) {
        _uploadFile.close();
        Sprint(F("Upload end: ")); Sprintln(upload.totalSize);
        Sprint(F("Took: ")); Sprintln(((millis()-_uploadStart)/1000));
      }
      break;
    case UPLOAD_FILE_ABORTED:
      Sprintln(F("Upload aborted"));
      if (_uploadFile.isOpen()) {
        _uploadFile.remove();
        _uploadFile.close();
      }
      break;
  }
}

void ShelfWeb::_handleDefault() {
  String path = _server.urlDecode(_server.uri());
  Sprintf("Request to: %s\n", path.c_str());
  if(_espalexa.handleAlexaApiCall(_server.uri(), _server.arg(F("plain")))) {
    return;
  }
  switch(_server.method()) {
    case HTTP_GET:
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
    case HTTP_DELETE:
      _handleDelete(path.c_str());
      return;
    case HTTP_POST:
      if(_server.hasArg(F("newFolder"))) {
        Sprint(F("Creating folder ")); Sprintln(_server.arg(F("newFolder")));
        if(_SD.mkdir(_server.arg(F("newFolder")).c_str())) {
          _returnOK();
        } else {
          Sprintln(F("Folder creation failed"));
          _returnHttpStatus(500, PSTR("Folder creation failed"));
        }
        return;
      } else if(_server.hasArg(F("ota"))) {
        Sprint(F("Starting OTA from ")); Sprintln(_server.arg(F("ota")));
        _playback.stopPlayback();
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
        _playback.isNight() ? _playback.stopNight() : _playback.startNight();
        _sendJsonStatus();
        return;
      } else if(_server.hasArg(F("toggleShuffle"))) {
        _playback.isShuffle() ? _playback.stopShuffle() : _playback.startShuffle();
        _sendJsonStatus();
        return;
      } else if(path == "/") {
        Sprintln(F("Probably got an upload request"));
        _returnOK();
        return;
      } else if(_SD.exists(path.c_str())) {
        // <= 17 here because leading "/"" is included
        if(_server.hasArg(F("write")) && path.length() <= 17) {
          const char *target = path.c_str();
          const uint8_t volume = 50-(uint8_t)_server.arg(F("volume")).toInt();
          const uint8_t repeat = _server.arg(F("repeat")).toInt();
          const uint8_t shuffle = _server.arg(F("shuffle")).toInt();
          const uint8_t stopOnRemove = _server.arg(F("stopOnRemove")).toInt();
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
          char* pathCStr = strdup(path.c_str());
          char* folder = strtok(pathCStr, "/");
          char* file = strtok(NULL, "/");

          if((folder != NULL) && (file != NULL) && (strlen(folder) < 90) && _playback.switchFolder(folder)) {
            _playback.startFilePlayback(folder, file);
            _playback.playingByCard = false;
            _sendJsonStatus();
            free(pathCStr);
            return;
          }
          free(pathCStr);
        }
      }
      break;
    default:
      Sprintln(PSTR("Unsupported method"));
      break;
  }

  // 404 otherwise
  _returnHttpStatus(404, PSTR("Not found"));
  Sprint(F("404: ")); Sprintln(path);
}

bool ShelfWeb::isFileUploading() {
  return _uploadFile.isOpen();
}

void ShelfWeb::pause() {
  if(_paused) {
    return;
  }
  _paused = true;
  _server.stop();
}

void ShelfWeb::unpause() {
  if(!_paused) {
    return;
  }
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
