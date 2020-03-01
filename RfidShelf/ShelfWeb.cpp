#include "ShelfWeb.h"

void ShelfWeb::begin() {
  ArRequestHandlerFunction defaultCallback = std::bind(&ShelfWeb::_handleDefault, this,std::placeholders::_1);
  _server.on(
    "/",
    HTTP_POST,
    defaultCallback,
    std::bind(&ShelfWeb::_handleFileUpload, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));
  AsyncStaticWebHandler &staticHandler = _server.serveStatic("/", _SD, "/");
  staticHandler.setFilter(
    [] (AsyncWebServerRequest *request) {
      return request->args() == 0;
    }
  );
  _server.onNotFound(defaultCallback);
  _server.begin();

  MDNS.addService("http", "tcp", 80);
}

void ShelfWeb::_returnOK(AsyncWebServerRequest *request) {
  request->send(200);
}

void ShelfWeb::_returnHttpStatus(AsyncWebServerRequest *request, uint16_t statusCode, const char *msg) {
  request->send_P(statusCode, "text/plain", msg);
}

void ShelfWeb::_sendHTML(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", ShelfHtml::INDEX, ShelfHtml::INDEX_SIZE);
  response->addHeader("Content-Encoding", "gzip");
  request->send(response);
}

void ShelfWeb::_sendJsonStatus(AsyncWebServerRequest *request) {
  char output[768] = "{\"playback\":\"";
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
    _playback.currentFolder(buffer, sizeof(buffer));
    strcat(output, "\",\"currentFolder\":\"");
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

  if(_playback.isShuffle()) {
    strcat(output, ",\"shuffle\":true");
  } else {
    strcat(output, ",\"shuffle\":false");
  }

  strcat(output, ",\"time\":");
  snprintf(buffer, sizeof(buffer), "%lu", time(nullptr));
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

  request->send_P(200, "application/json", output);
}

void ShelfWeb::_sendJsonConfig(AsyncWebServerRequest *request) {
  using namespace ShelfConfig;
  char output[768] = "{\"host\":\"";
  char buffer[101];

  strcat(output, config.hostname);
  strcat(output, "\",\"ntp\":\"");
  strcat(output, config.ntpServer);
  strcat(output, "\",\"tz\":\"");
  strcat(output, config.timezone);
  strcat(output, "\",\"volume\":");
  snprintf(buffer, sizeof(buffer), "%d", config.defaultVolumne);
  strcat(output, buffer);
  strcat(output, ",\"repeat\":");
  strcat(output, config.defaultRepeat ? "true" : "false");
  strcat(output, ",\"shuffle\":");
  strcat(output, config.defaultShuffle ? "true" : "false");
  strcat(output, ",\"stopOnRemove\":");
  strcat(output, config.defaultStopOnRemove ? "true" : "false");
  strcat(output, ",\"nightMode\":[");
  for(uint8_t i = 0; i < sizeof(config.nightModeTimes)/sizeof(Timeslot_t); i++) {
    strcat(output, "{");
    strcat(output, "\"days\":[");
    strcat(output, config.nightModeTimes[i].monday ? "true," : "false,");
    strcat(output, config.nightModeTimes[i].tuesday ? "true," : "false,");
    strcat(output, config.nightModeTimes[i].wednesday ? "true," : "false,");
    strcat(output, config.nightModeTimes[i].thursday ? "true," : "false,");
    strcat(output, config.nightModeTimes[i].friday ? "true," : "false,");
    strcat(output, config.nightModeTimes[i].saturday ? "true," : "false,");
    strcat(output, config.nightModeTimes[i].sunday ? "true" : "false");
    strcat(output, "],\"start\":\"");
    snprintf(buffer, sizeof(buffer), "%02d:%02d", config.nightModeTimes[i].startHour, config.nightModeTimes[i].startMinutes);
    strcat(output, buffer);
    strcat(output, "\",\"end\":\"");
    snprintf(buffer, sizeof(buffer), "%02d:%02d", config.nightModeTimes[i].endHour, config.nightModeTimes[i].endMinutes);
    strcat(output, buffer);
    strcat(output, "\"");

    strcat(output, "}");
    if(i < sizeof(config.nightModeTimes)/sizeof(Timeslot_t) - 1) {
      strcat(output, ",");
    }
  }
  strcat(output, "]}");

  request->send_P(200, "application/json", output);
}

void ShelfWeb::_sendJsonFS(AsyncWebServerRequest *request, const char *path) {
  AsyncResponseStream *response = request->beginResponseStream("application/json");
  response->print("{\"fs\":[");

  sdfat::SdFile dir;
  dir.open(path, sdfat::O_READ);
  dir.rewind();

  sdfat::SdFile entry;

  char buffer[101];
  char output[256];

  bool first = true;

  while (entry.openNext(&dir, sdfat::O_READ)) {
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
    response->print(output);
  }
  response->print("],");
  response->printf("\"path\":\"%s\"", path);
  response->print("}");

  request->send(response);

  dir.close();
}

bool ShelfWeb::_loadFromSdCard(AsyncWebServerRequest *request, const char *path) {
  File dataFile = _SD.open(path, "r");

  if (!dataFile) {
    Sprintln("File not open");
    _returnHttpStatus(request, 404, "Not found");
    return false;
  }

  if (dataFile.isDirectory()) {
    _sendJsonFS(request, path);
  } else {
    _returnHttpStatus(request, 400, "Not a directory");
  }
  dataFile.close();
  return true;
}

void ShelfWeb::_downloadPatch(AsyncWebServerRequest *request) {
  Sprintln("Starting patch download");
  std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
  // do not validate certificate
  client->setInsecure();
  HTTPClient httpClient;
  httpClient.begin(*client, VS1053_PATCH_URL);
  int httpCode = httpClient.GET();
  if (httpCode < 0) {
    _returnHttpStatus(request, 500, httpClient.errorToString(httpCode).c_str());
    return;
  }
  if (httpCode != HTTP_CODE_OK) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "invalid response code: %d", httpCode);
    _returnHttpStatus(request, 500, buffer);
    return;
  }
  int len = httpClient.getSize();
  WiFiClient* stream = httpClient.getStreamPtr();
  uint8_t buffer[128] = { 0 };
  sdfat::SdFile patchFile;
  patchFile.open("/patches.053",sdfat::O_WRITE | sdfat::O_CREAT);
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
  _returnOK(request);
  request->client()->close(true);
  ESP.restart();
}

void ShelfWeb::_handleFileUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  // Upload always happens on /
  if (request->url() != "/") {
    Sprintln("Invalid upload URI");
    return;
  }


  if (!index) {
    if (!Adafruit_VS1053_FilePlayer::isMP3File((char *)filename.c_str())) {
      Sprint("Not a MP3: "); Sprintln(filename);
      return;
    }

    if (!filename.startsWith("/")) {
      Sprintln("Invalid upload target");
      return;
    }

    if (_SD.exists(filename.c_str())) {
      Sprintln("File " + filename + " already exists. Skipping");
      return;
    }

    if(_playback.playbackState() == PLAYBACK_FILE) {
      Sprintln("Pausing playback during for upload");
      _playback.pausePlayback();
    }

    _uploadFile.open(filename.c_str(), sdfat::O_WRITE | sdfat::O_CREAT);
    _uploadStart = millis();
    Sprint("Upload start: ");
    Sprintln(filename);
  }
  
  if (_uploadFile.isOpen()) {
    _uploadFile.write(data, len);
    //Sprint("Upload write: "));
    //Sprintln(upload.currentSize);
  }

  if (final && _uploadFile.isOpen()) {
    _uploadFile.close();
    Sprint("Upload took: "); Sprintln(((millis()-_uploadStart)/1000));
  }
}

void ShelfWeb::_handleDefault(AsyncWebServerRequest *request) {
  String path = request->urlDecode(request->url());
  Sprintf("Request to: %s\n", path.c_str());
  if (request->method() == HTTP_GET) {
    if (request->hasArg("status")) {
      _sendJsonStatus(request);
      return;
    }
    if (request->hasArg("config")) {
      _sendJsonConfig(request);
      return;
    }
    if(path == "/" && !request->hasArg("fs")) {
      _sendHTML(request);
      return;
    }

    // start file download
    _loadFromSdCard(request, path.c_str());
    return;
  } else if (request->method() == HTTP_DELETE) {
    if (path == "/" || !_SD.exists(path.c_str())) {
      _returnHttpStatus(request, 400, "Bad path");
      return;
    }

    sdfat::SdFile file;
    file.open(path.c_str());
    if (file.isDir()) {
      if(!file.rmRfStar()) {
        Sprintln("Could not delete folder");
      }
    } else {
      if(!_SD.remove(path.c_str())) {
        Sprintln("Could not delete file");
      }
    }
    file.close();
    _returnOK(request);
    return;
  } else if (request->method() == HTTP_POST) {
    if (request->hasArg("newFolder")) {
      Sprint("Creating folder "); Sprintln(request->arg("newFolder"));
      _SD.mkdir(request->arg("newFolder").c_str());
      _returnOK(request);
      return;
    } else if (request->hasArg("ota")) {
      Sprint("Starting OTA from "); Sprintln(request->arg("ota"));
      std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
      // do not validate certificate
      client->setInsecure();
      t_httpUpdate_return ret = ESPhttpUpdate.update(*client, request->arg("ota"));
      switch (ret) {
        case HTTP_UPDATE_FAILED:
          Sprintf("HTTP_UPDATE_FAILD Error (%d): ", ESPhttpUpdate.getLastError());
          Sprintln(ESPhttpUpdate.getLastErrorString().c_str());
          _returnHttpStatus(request, 500, "Update failed, please try again");
          return;
        case HTTP_UPDATE_NO_UPDATES:
          Sprintln("HTTP_UPDATE_NO_UPDATES");
          break;
        case HTTP_UPDATE_OK:
          Sprintln("HTTP_UPDATE_OK");
          break;
      }
      _returnOK(request);
      return;
    } else if (request->hasArg("downloadpatch")) {
      _downloadPatch(request);
      return;
    } else if (request->hasArg("stop")) {
      _playback.stopPlayback();
      _sendJsonStatus(request);
      return;
    } else if (request->hasArg("pause")) {
      _playback.pausePlayback();
      _sendJsonStatus(request);
      return;
    } else if (request->hasArg("resume")) {
      _playback.playingByCard = false;
      _playback.resumePlayback();
      _sendJsonStatus(request);
      return;
    } else if (request->hasArg("skip")) {
      _playback.playingByCard = false;
      _playback.skipFile();
      _sendJsonStatus(request);
      return;
    } else if (request->hasArg("volumeUp")) {
      _playback.volumeUp();
      _sendJsonStatus(request);
      return;
    } else if (request->hasArg("volumeDown")) {
      _playback.volumeDown();
      _sendJsonStatus(request);
      return;
    } else if (request->hasArg("toggleNight")) {
      if(_playback.isNight()) {
        _playback.stopNight();
      } else {
        _playback.startNight();
      }
      _sendJsonStatus(request);
      return;
    } else if (request->hasArg("toggleShuffle")) {
      if(_playback.isShuffle()) {
        _playback.stopShuffle();
      } else {
        _playback.startShuffle();
      }
      _sendJsonStatus(request);
      return;
    } else if (request->url() == "/") {
      Sprintln("Probably got an upload request");
      _returnOK(request);
      return;
    } else if (_SD.exists(path.c_str())) {
      // <= 17 here because leading "/"" is included
      if (request->hasArg("write") && path.length() <= 17) {
        const char *target = path.c_str();
        const uint8_t volume = 50-(uint8_t)request->arg("volume").toInt();
        uint8_t repeat = request->arg("repeat").toInt();
        uint8_t shuffle = request->arg("shuffle").toInt();
        uint8_t stopOnRemove = request->arg("stopOnRemove").toInt();
        // Remove leading "/""
        target++;
        if (_rfid.startPairing(target, volume, repeat, shuffle, stopOnRemove)) {
          _sendJsonStatus(request);
          return;
        }
      } else if (request->hasArg("play") && _playback.switchFolder(path.c_str())) {
        _playback.startPlayback();
        _playback.playingByCard = false;
        _sendJsonStatus(request);
        return;
      } else if (request->hasArg("playfile")) {
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
            _sendJsonStatus(request);
            return;
          }
        }
      }
    }
  }

  // 404 otherwise
  _returnHttpStatus(request, 404, "Not found");
  Sprintln("404: " + path);
}

void ShelfWeb::work() {
}
