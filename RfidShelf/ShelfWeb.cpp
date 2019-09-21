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
  _server.send(200, "text/plain", "");
}

void ShelfWeb::returnHttpStatus(uint16_t statusCode, String msg) {
  _server.send(statusCode, "text/plain", msg);
}

void ShelfWeb::renderDirectory(String path) {
  _server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  _server.send(200, "text/html", "");

  _server.sendContent(
    F("<html><head><meta charset=\"utf-8\"><script>"
      "function _(el) {return document.getElementById(el);}\n"
      "function b(c) { document.body.innerHTML=c}\n"
      "function ajax(m, url, cb, p) {var xhr = new XMLHttpRequest(); xhr.onreadystatechange = function() { if (xhr.readyState == 4) { cb(xhr.responseText); }}; xhr.open(m, url, true); xhr.send(p);}\n"
      "function deleteUrl(url){ if(confirm(\"Really delete?\"))ajax('DELETE', url, reload);}\n"
      "function upload(folder){ var fileInput = _('fileInput'); if(fileInput.files.length === 0){ alert('Choose a file first'); return; } var fileTooLong = false; Array.prototype.forEach.call(fileInput.files, function(file) { if (file.name.length > 100) { fileTooLong = true; }}); if (fileTooLong) { alert(\"File name too long. Files can be max. 100 characters long.\"); return; } xhr = new XMLHttpRequest(); xhr.onreadystatechange = function() { if (xhr.readyState === 4) { location.reload(); }};\n"
      "var d = new FormData(); for(var i = 0; i < fileInput.files.length; i++) { d.append('data', fileInput.files[i], folder.concat(fileInput.files[i].name)); }; xhr.open('POST', '/');\n"
      "xhr.upload.addEventListener('progress', progressHandler, false); xhr.addEventListener('load', completeHandler, false); xhr.addEventListener('error', errorHandler, false); xhr.addEventListener('abort', abortHandler, false); _('ulDiv').style.display = 'block'; xhr.send(d); }\n"
      "function progressHandler(event) { var percentage = Math.round((event.loaded / event.total) * 100); _('progressBar').value = percentage; _('ulStatus').innerHTML = percentage + '% (' + Math.ceil((event.loaded/(1024*1024)) * 10)/10 + '/' + Math.ceil((event.total/(1024*1024)) * 10)/10 + 'MB) uploaded'; }\n"
      "function completeHandler(event) { _('ulStatus').innerHTML = event.target.responseText; location.reload(); }\n"
      "function errorHandler(event) { _('ulStatus').innerHTML = 'Upload Failed'; }\n"
      "function abortHandler(event) { _('ulStatus').innerHTML = 'Upload Aborted'; }\n"
      "function reload(){ location.reload(); }\n"
      "function loadFS(url){ajax('GET', url+'?fs=1', function(r){ _('fs').innerHTML=r; sortDivs('fs');});}\n"
      "function writeRfid(url){var d = new FormData(); d.append('write', 1); ajax('POST', url, reload, d);}\n"
      "function play(url){var d = new FormData(); d.append('play', 1); ajax('POST', url, reload, d);}\n"
      "function playFile(url){var d = new FormData(); d.append('playfile', 1); ajax('POST', url, reload, d);}\n"
      "function rootAction(action){var d = new FormData(); d.append(action, 1); ajax('POST', '/', reload, d);}\n"
      "function volume(action){var d = new FormData(); d.append(action, 1); ajax('POST', '/', function(v){_('volumeBar').value=v;}, d);}\n"
      "function mkdir(){var f = _('folder').value; if(f != ''){var d = new FormData(); d.append('newFolder', f); ajax('POST', '/', reload, d);}}\n"
      "function ota(){var d = new FormData(); d.append('ota', 1); ajax('POST', '/', function(){ b('Please wait and do NOT turn off the power!'); location.reload();}, d);}\n"
      "function downloadpatch(){ var xhr = new XMLHttpRequest(); xhr.onreadystatechange = function() { if (xhr.readyState === 1) { document.write('Please wait while downloading patch! When the download was successful the system is automatically restarting.'); } else if (xhr.readyState === 4) { location.reload(); }}; xhr.open('POST', '/'); var formData = new FormData(); formData.append('downloadpatch', 1); xhr.send(formData);}\n"
      "function formatNumbers(){ var numbers = document.getElementsByClassName('number'); for (var i = 0; i < numbers.length; i++) { numbers[i].innerText = Number(numbers[i].innerText).toLocaleString(); }}\n"
      "function sortDivs(id) { var parent = _(id); var toSort = parent.childNodes; toSort = Array.prototype.slice.call(toSort, 0); toSort.sort(function (a, b) { if(a.className == b.className) return ('' + a.id).localeCompare(b.id); else if(a.className == 'folder') return -1; else return 1;}); parent.innerHTML = ''; for(var i = 0, l = toSort.length; i < l; i++) { parent.appendChild(toSort[i]); }}\n"
      "</script><link rel=\"icon\" href=\"data:;base64,iVBORw0KGgo=\"><title>RfidShelf</title><style>body { font-family: Arial, Helvetica; font-size: 150%} #fs {vertical-align: middle;} #fs div:nth-child(even) { background: LightGray; } a { color: #0000EE; text-decoration: none; }</style></head><body>\n"));

  String output;

  if(_rfid.pairing) {
    char currentFolder[50];
    _SD.vwd()->getName(currentFolder, sizeof(currentFolder));

    output = F("<p style=\"font-weight: bold\">Pairing mode active. Place card on shelf to write current configuration for <span style=\"color:red\">/{currentFolder}</span> onto it</p>");
    output.replace("{currentFolder}", currentFolder);
    _server.sendContent(output);
  }

  // Playback state
  if(_playback.playbackState() != PLAYBACK_NO) {
    output = F("<p>Currently playing: <strong>{currentFile}</strong>");
    output.replace("{currentFile}", _playback.currentFile());
    _server.sendContent(output);
    if(_playback.playbackState() == PLAYBACK_PAUSED) {
      _server.sendContent(F(" <a href=\"#\" onclick=\"rootAction('resume'); return false;\">&#x25b6;</a>"));
    } else {
      _server.sendContent(F(" <a href=\"#\" onclick=\"rootAction('pause'); return false;\">&#x23f8;</a>"));
    }
    _server.sendContent(F(" <a href=\"#\" onclick=\"rootAction('stop'); return false;\">&#x23f9;</a>"));
    _server.sendContent(F(" <a href=\"#\" onclick=\"rootAction('skip'); return false;\">&#x23ed;</a></p>"));
  }

  _server.sendContent(F("<p>"));
  // Volume
  output = F("Volume:&nbsp;<meter id=\"volumeBar\" value=\"{volume}\" max=\"50\" style=\"width:300px;\">{volume}</meter><br>");
  output.replace("{volume}", String(50 - _playback.volume()));
  _server.sendContent(output);
  // Night Mode
  output = F(
    "<a title=\"decrease\" href=\"#\" onclick=\"volume('volumeDown'); return false;\">&#x1f509;</a> / "
    "<a title=\"increase\" href=\"#\" onclick=\"volume('volumeUp'); return false;\">&#x1f50a;</a> "
    "<a href=\"#\" title=\"{action} night mode\" onclick=\"rootAction('toggleNight'); return false;\">{symbol}</a>"
    );
  output.replace("{action}", (_playback.isNight() ? F("deactivate") : F("activate")));
  output.replace("{symbol}", (_playback.isNight() ? F("&#x1f31b;") : F("&#x1f31e;")));
  _server.sendContent(output);
  _server.sendContent(F("</p>"));

  // Upload
  if (path != "/") {
    output = F("<form><p>Upload MP3 file: <input type=\"file\" multiple=\"true\" name=\"data\" accept=\".mp3\" id=\"fileInput\"><input type=\"button\" value=\"upload\" onclick=\"upload('{folder}'); return false;\"></p>"
      "<div id=\"ulDiv\" style=\"display:none;\"><progress id=\"progressBar\" value=\"0\" max=\"100\" style=\"width:300px;\"></progress>"
      "<p id=\"ulStatus\"></p></div></form>");
    output.replace("{folder}", path);
    _server.sendContent(output);
    _server.sendContent(F("<div><a href=\"..\">Back to top</a></div><br />"));
  }

  // create folder
  if (path == "/") {
    _server.sendContent(F("<form onsubmit=\"mkdir(); return false;\">Create new folder: <input type=\"text\" name=\"folder\" id=\"folder\"><input type=\"button\" value=\"Create\" onclick=\"mkdir(); return false;\"></form>"));
  }

  // Load file system entries
  output = F("<div id=\"fs\"><a title=\"Show file system\" href=\"#\" onclick=\"loadFS('{folder}'); return false;\"><span style=\"font-size: 300%\">&#x1f5d8;</span> Show file system</a></div>");
  output.replace("{folder}", path);
  _server.sendContent(output);

  if (path == "/") {
    if (!_SD.exists("/patches.053")) {
      output = F("<form><p><b>MP3 decoder patch missing</b> (might reduce sound quality) <input type=\"button\" value=\"Download + Install VS1053 patch\" onclick=\"downloadpatch(); return false;\"></p><form>");
      _server.sendContent(output);
    }
    
    output = F("<form><p>Version {major}.{minor} <input type=\"button\" value=\"Update Firmware\" onclick=\"ota(); return false;\"></p></form>");
    output.replace("{major}", String(MAJOR_VERSION));
    output.replace("{minor}", String(MINOR_VERSION));
    _server.sendContent(output);

    int hours = _timeClient.getHours();
    int minutes = _timeClient.getMinutes();
    int seconds = _timeClient.getSeconds();
    char buffer[9];
    sprintf(buffer, "%02d:%02d:%02d", hours, minutes, seconds);

    output = F("<p>Time: {time}</p>");
    output.replace("{time}", buffer);
    _server.sendContent(output);
  } else {
    output = F("<script>loadFS('{folder}')</script>");
    output.replace("{folder}", path);
    _server.sendContent(output);
  }

  // Dirty client side UI stuff that's too complicated (for me) in C
  _server.sendContent(F("<script>"
    "sortDivs('fs');"
    "formatNumbers();"
    "</script></body></html>"));

  _server.sendContent("");
  _server.client().stop();
}

void ShelfWeb::renderFS(String path) {
  _server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  _server.send(200, "text/html", "");

  String output;
  SdFile entry;

  SdFile dir;
  dir.open(path.c_str(), O_READ);

  dir.rewind();

  while (entry.openNext(&dir, O_READ)) {
    // filename can be only 100 characters long
    char filenameChar[101] = { 0 };
    entry.getName(filenameChar, 100);
    // TODO encode special characters
    String filename = String(filenameChar);

    if (entry.isDir()) {
      output = F("<div class=\"folder\" id=\"{name}\">&#x1f4c2; <a href=\"{name}/\">{name}</a> <a href=\"#\" onclick=\"deleteUrl('{name}'); return false;\" title=\"delete\">&#x2718;</a>");
      // Currently only foldernames <= 16 characters can be written onto the rfid
      if (filename.length() <= 16) {
        output += F("<a href=\"#\" onclick=\"writeRfid('{name}');\" title=\"write to card\">&#x1f4be;</a> ");
      } else {
        output += F("<span title=\"write to card (only possible for folder names with <= 16 characters!)\" style=\"opacity: 0.5;\">&#x1f4be;</span> ");
      }
      output += F("<a href=\"#\" onclick=\"play('{name}'); return false;\" title=\"play folder\">&#x25b6;</a></div>");
      output.replace("{name}", filename);
    } else {
      output = F("<div class=\"file\" id=\"{name}\">{icon}<a href=\"{name}\">{name}</a> (<span class=\"number\">{size}</span> bytes) <a href=\"#\" onclick=\"deleteUrl('{name}'); return false;\" title=\"delete\">&#x2718;</a>");
      if(Adafruit_VS1053_FilePlayer::isMP3File(filenameChar) && (path != "/")) {
        output += F("<a href=\"#\" onclick=\"playFile('{name}'); return false;\" title=\"play\">&#x25b6;</a>");
      }
      output += F("</div>");

      if (Adafruit_VS1053_FilePlayer::isMP3File(filenameChar)) {
        output.replace("{icon}", F("&#x266b; "));
      } else {
        output.replace("{icon}", "");
      }
      output.replace("{name}", filename);
      output.replace("{size}", String(entry.fileSize()));
    }
    _server.sendContent(output);

    entry.close();
  }

  _server.sendContent("");
  _server.client().stop();
}

bool ShelfWeb::loadFromSdCard(String path, bool fs) {
  File dataFile = _SD.open(path.c_str());

  if (!dataFile) {
    Serial.println(F("File not open"));
    returnHttpStatus(404, "Not found");
    return false;
  }

  if (dataFile.isDir()) {
    if(fs) {
      renderFS(path);
    } else {
      // dataFile.name() will always be "/" for directorys, so we cannot know if we are in the root directory without handing it over
      renderDirectory(path);
    }
  } else {
    String dataType = "application/octet-stream";

    if (path.endsWith(".HTM")) dataType = "text/html";
    else if (path.endsWith(".CSS")) dataType = "text/css";
    else if (path.endsWith(".JS")) dataType = "application/javascript";

    if (_server.streamFile(dataFile, dataType) != dataFile.size()) {
      Serial.println(F("Sent less data than expected!"));
    }
  }
  dataFile.close();
  return true;
}

void ShelfWeb::handleFileUpload() {
  // Upload always happens on /
  if (_server.uri() != "/") {
    Serial.println(F("Invalid upload URI"));
    return;
  }

  HTTPUpload& upload = _server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;

    if (!Adafruit_VS1053_FilePlayer::isMP3File((char *)filename.c_str())) {
      Serial.print(F("Not a MP3: "));
      Serial.println(filename);
      return;
    }

    if (!filename.startsWith("/")) {
      Serial.println(F("Invalid upload target"));
      return;
    }

    if (_SD.exists((char *)filename.c_str())) {
      Serial.println("File " + filename + " already exists. Skipping");
      return;
    }

    _uploadFile.open((char *)filename.c_str(), O_WRITE | O_CREAT);
    _uploadStart = millis();
    Serial.print(F("UPLOAD_FILE_START: "));
    Serial.println(filename);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (_uploadFile.isOpen()) {
      _uploadFile.write(upload.buf, upload.currentSize);
#ifdef SHELFDEBUG
      Serial.print(F("UPLOAD_FILE_WRITE: "));
      Serial.println(upload.currentSize);
#endif
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (_uploadFile.isOpen()) {
      _uploadFile.close();
      Serial.print(F("UPLOAD_FILE_END: "));
      Serial.println(upload.totalSize);
      Serial.print(F("Took: "));
      Serial.println(((millis()-_uploadStart)/1000));
    }
  }
}

void ShelfWeb::handleDefault() {
  String path = _server.urlDecode(_server.uri());
  Serial.printf(F("Request to: %s\n"), path.c_str());
  if (_server.method() == HTTP_GET) {
    loadFromSdCard(path, _server.hasArg("fs"));
  } else if (_server.method() == HTTP_DELETE) {
    if (_server.uri() == "/" || !_SD.exists(path.c_str())) {
      returnHttpStatus(400, "Bad path: " + _server.uri());
      return;
    }

    SdFile file;
    file.open(path.c_str());
    if (file.isDir()) {
      if(!file.rmRfStar()) {
        Serial.println(F("Could not delete folder"));
      }
    } else {
      if(!_SD.remove(path.c_str())) {
        Serial.println(F("Could not delete file"));
      }
    }
    returnOK();
    return;
  } else if (_server.method() == HTTP_POST) {
    if (_server.hasArg("newFolder")) {
      Serial.print(F("Creating folder "));
      Serial.println(_server.arg("newFolder"));
      _playback.switchFolder("/");
      _SD.mkdir((char *)_server.arg("newFolder").c_str());
      returnOK();
      return;
    } else if (_server.hasArg("ota")) {
      Serial.println(F("Starting OTA"));
      std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
      // do not validate certificate
      client->setInsecure();
      t_httpUpdate_return ret = ESPhttpUpdate.update(*client, UPDATE_URL);
      switch (ret) {
        case HTTP_UPDATE_FAILED:
          Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
          returnHttpStatus((uint8_t)500, "Update failed, please try again");
          return;
        case HTTP_UPDATE_NO_UPDATES:
          Serial.println(F("HTTP_UPDATE_NO_UPDATES"));
          break;
        case HTTP_UPDATE_OK:
          Serial.println(F("HTTP_UPDATE_OK"));
          break;
      }
      returnOK();
      return;
    } else if (_server.hasArg("downloadpatch")) {
      Serial.println(F("Starting patch download"));
      std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
      // do not validate certificate
      client->setInsecure();
      HTTPClient httpClient;
      httpClient.begin(*client, VS1053_PATCH_URL);
      int httpCode = httpClient.GET();
      if (httpCode < 0) {
        returnHttpStatus(500, httpClient.errorToString(httpCode));
        return;
      }
      if (httpCode != HTTP_CODE_OK) {
        returnHttpStatus(500, String("http code not 200: ") + httpCode);
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
    } else if (_server.hasArg("stop")) {
      _playback.stopPlayback();
      returnOK();
      return;
    } else if (_server.hasArg("pause")) {
      _playback.pausePlayback();
      returnOK();
      return;
    } else if (_server.hasArg("resume")) {
      _playback.playingByCard = false;
      _playback.resumePlayback();
      returnOK();
      return;
    } else if (_server.hasArg("skip")) {
      _playback.playingByCard = false;
      _playback.skipFile();
      returnOK();
      return;
    } else if (_server.hasArg("volumeUp")) {
      _playback.volumeUp();
      returnHttpStatus(200, String(50 - _playback.volume()));
      return;
    } else if (_server.hasArg("volumeDown")) {
      _playback.volumeDown();
      returnHttpStatus(200, String(50 - _playback.volume()));
      return;
    } else if (_server.hasArg("toggleNight")) {
      if(_playback.isNight()) {
        _playback.stopNight();
        returnHttpStatus(200, "0");
      } else {
        _playback.startNight();
        returnHttpStatus(200, "1");
      }
      returnOK();
      return;
    } else if (_server.uri() == "/") {
      Serial.println(F("Probably got an upload request"));
      returnOK();
      return;
    } else if (_SD.exists((char *)path.c_str())) {
      if (_server.hasArg("write") && path.length() <= 16) {
        handleWriteRfid(path);
        returnOK();
        return;
      } else if (_server.hasArg("play") && _playback.switchFolder((char *)path.c_str())) {
        _playback.startPlayback();
        _playback.playingByCard = false;
        returnOK();
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
            _playback.startFilePlayback(folder, file);
            _playback.playingByCard = false;
            returnOK();
            return;
          }
        }
      }
    }
  }

  // 404 otherwise
  returnHttpStatus(404, "Not found");
  Serial.println("404: " + path);
}

void ShelfWeb::handleWriteRfid(String folder) {
  if (_playback.switchFolder((char *)folder.c_str())) {
    _rfid.pairing = true;
    returnOK();
  } else {
    returnHttpStatus(404, "Not found");
  }
}

void ShelfWeb::work() {
  _server.handleClient();
}
