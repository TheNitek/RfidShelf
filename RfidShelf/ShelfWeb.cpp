#include "ShelfWeb.h"

// This sucks - Maybe refactor ShelfWeb to singleton
ShelfWeb* ShelfWeb::_instance;

ShelfWeb::ShelfWeb(ShelfPlayback &playback, ShelfRfid &rfid, SdFat &sd, NTPClient &timeClient) : _playback(playback), _rfid(rfid), _SD(sd), _timeClient(timeClient) {
  _instance = this;
}

void ShelfWeb::defaultCallback() {
  _instance->handleDefault();
}

void ShelfWeb::fileUploadCallback() {
  _instance->handleFileUpload();
}

void ShelfWeb::begin() {
  _server.on("/", HTTP_POST, defaultCallback, fileUploadCallback);
  _server.onNotFound(defaultCallback);

  _server.begin();
  
  MDNS.begin(_dnsname);
  MDNS.addService("http", "tcp", 80);
}

void ShelfWeb::returnOK() {
  _server.send_P(200, "text/plain", NULL);
}

void ShelfWeb::returnHttpStatus(uint16_t statusCode, const char *msg) {
  _server.send_P(statusCode, "text/plain", msg);
}

void ShelfWeb::sendHTML() {
  _server.send(200, "text/html", ShelfHtml::INDEX);
}


void ShelfWeb::sendJsonStatus() {
  char output[512] = "{\"playback\":\"";
  char buffer[16];

  if(_playback.playbackState() == PLAYBACK_FILE) {
    strcat(output, "FILE\"");
  } else if(_playback.playbackState() == PLAYBACK_PAUSED) {
    strcat(output, "PAUSED\"");
  } else {
    strcat(output, "NO\"");
  }

  if(_rfid.pairingFolder[0] != '\0') {
    strcat(output, ",\"pairing\":\"");
    strcat(output, _rfid.pairingFolder);
    strcat(output, "\"");
  }

  if(_playback.playbackState() != PLAYBACK_NO) {
    strcat(output, ",\"currentFile\":\"");
    strcat(output, _playback.currentFile().c_str());
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

  strcat(output, ",\"time\":");
  snprintf(buffer, sizeof(buffer), "%lu", _timeClient.getEpochTime());
  strcat(output, buffer);

  strcat(output, ",\"version\":");
  snprintf(buffer, sizeof(buffer), "\"%d.%d\"", MAJOR_VERSION, MINOR_VERSION);
  strcat(output, buffer);
  strcat(output, "}");

  _server.send_P(200, "application/json", output);
}

void ShelfWeb::sendJsonFS(const char *path) {
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

bool ShelfWeb::loadFromSdCard(const char *path) {
  File dataFile = _SD.open(path);

  if (!dataFile) {
    Sprintln(F("File not open"));
    returnHttpStatus(404, "Not found");
    return false;
  }

  if (dataFile.isDir()) {
    sendJsonFS(path);
  } else {
    String dataType = "application/octet-stream";
    if (_server.streamFile(dataFile, dataType) != dataFile.size()) {
      Sprintln(F("Sent less data than expected!"));
    }
  }
  dataFile.close();
  return true;
}

void ShelfWeb::downloadPatch() {
  Sprintln(F("Starting patch download"));
  std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
  // do not validate certificate
  client->setInsecure();
  HTTPClient httpClient;
  httpClient.begin(*client, VS1053_PATCH_URL);
  int httpCode = httpClient.GET();
  if (httpCode < 0) {
    returnHttpStatus(500, httpClient.errorToString(httpCode).c_str());
    return;
  }
  if (httpCode != HTTP_CODE_OK) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "invalid response code: %d", httpCode);
    returnHttpStatus(500, buffer);
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
  returnOK();
  _server.client().flush();
  ESP.restart();
}

void ShelfWeb::handleFileUpload() {
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
    Sprint(F("UPLOAD_FILE_START: "));
    Sprintln(filename);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (_uploadFile.isOpen()) {
      _uploadFile.write(upload.buf, upload.currentSize);
      Sprint(F("UPLOAD_FILE_WRITE: "));
      Sprintln(upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (_uploadFile.isOpen()) {
      _uploadFile.close();
      Sprint(F("UPLOAD_FILE_END: ")); Sprintln(upload.totalSize);
      Sprint(F("Took: ")); Sprintln(((millis()-_uploadStart)/1000));
    }
  }
}

void ShelfWeb::handleDefault() {
  String path = _server.urlDecode(_server.uri());
  Sprintf(F("Request to: %s\n"), path.c_str());
  if (_server.method() == HTTP_GET) {
    if (_server.hasArg("status")) {
      sendJsonStatus();
      return;
    } else if(path == "/" && !_server.hasArg("fs")) {
      sendHTML();
      return;
    } else {
      loadFromSdCard(path.c_str());
      return;
    }
  } else if (_server.method() == HTTP_DELETE) {
    if (path == "/" || !_SD.exists(path.c_str())) {
      returnHttpStatus(400, "Bad path");
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
    returnOK();
    return;
  } else if (_server.method() == HTTP_POST) {
    if (_server.hasArg("newFolder")) {
      Sprint(F("Creating folder ")); Sprintln(_server.arg("newFolder"));
      _SD.mkdir(_server.arg("newFolder").c_str());
      returnOK();
      return;
    } else if (_server.hasArg("ota")) {
      Sprintln(F("Starting OTA"));
      std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
      // do not validate certificate
      client->setInsecure();
      t_httpUpdate_return ret = ESPhttpUpdate.update(*client, UPDATE_URL);
      switch (ret) {
        case HTTP_UPDATE_FAILED:
          Sprintf("HTTP_UPDATE_FAILD Error (%d): ", ESPhttpUpdate.getLastError());
          Sprintln(ESPhttpUpdate.getLastErrorString().c_str());
          returnHttpStatus(500, "Update failed, please try again");
          return;
        case HTTP_UPDATE_NO_UPDATES:
          Sprintln(F("HTTP_UPDATE_NO_UPDATES"));
          break;
        case HTTP_UPDATE_OK:
          Sprintln(F("HTTP_UPDATE_OK"));
          break;
      }
      returnOK();
      return;
    } else if (_server.hasArg("downloadpatch")) {
      downloadPatch();
      return;
    } else if (_server.hasArg("stop")) {
      _playback.stopPlayback();
      sendJsonStatus();
      return;
    } else if (_server.hasArg("pause")) {
      _playback.pausePlayback();
      sendJsonStatus();
      return;
    } else if (_server.hasArg("resume")) {
      _playback.playingByCard = false;
      _playback.resumePlayback();
      sendJsonStatus();
      return;
    } else if (_server.hasArg("skip")) {
      _playback.playingByCard = false;
      _playback.skipFile();
      sendJsonStatus();
      return;
    } else if (_server.hasArg("volumeUp")) {
      _playback.volumeUp();
      sendJsonStatus();
      return;
    } else if (_server.hasArg("volumeDown")) {
      _playback.volumeDown();
      sendJsonStatus();
      return;
    } else if (_server.hasArg("toggleNight")) {
      if(_playback.isNight()) {
        _playback.stopNight();
      } else {
        _playback.startNight();
      }
      sendJsonStatus();
      return;
    } else if (_server.uri() == "/") {
      Sprintln(F("Probably got an upload request"));
      returnOK();
      return;
    } else if (_SD.exists(path.c_str())) {
      // <= 17 here because leading "/"" is included
      if (_server.hasArg("write") && path.length() <= 17) {
        const char *target = path.c_str();
        // Remove leading "/""
        target++;
        if (_rfid.startPairing(target)) {
          sendJsonStatus();
          return;
        }
      } else if (_server.hasArg("play") && _playback.switchFolder(path.c_str())) {
        _playback.startPlayback();
        _playback.playingByCard = false;
        sendJsonStatus();
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
            sendJsonStatus();
            return;
          }
        }
      }
    }
  }

  // 404 otherwise
  returnHttpStatus(404, "Not found");
  Sprintln("404: " + path);
}

void ShelfWeb::work() {
  _server.handleClient();
}