#include "ShelfWeb.h"

// This sucks - Maybe refactor ShelfWeb to singleton
ShelfWeb* ShelfWeb::_instance;

ShelfWeb::ShelfWeb(ShelfPlayback &playback, ShelfRfid &rfid, SdFat &sd) : _playback(playback), _rfid(rfid), _SD(sd) {
  _instance = this;
}

void ShelfWeb::notFoundCallback() {
  _instance->handleNotFound();
}

void ShelfWeb::fileUploadCallback() {
  _instance->handleFileUpload();
}

void ShelfWeb::begin() {
  _server.on("/", HTTP_POST, notFoundCallback, fileUploadCallback);
  _server.onNotFound(notFoundCallback);

  _server.begin();
  
  MDNS.begin(_dnsname);
  MDNS.addService("http", "tcp", 80);
}

void ShelfWeb::returnOK() {
  _server.send(200, "text/plain", "");
}

void ShelfWeb::returnHttpStatus(uint8_t statusCode, String msg) {
  _server.send(statusCode, "text/plain", msg);
}

void ShelfWeb::renderDirectory(String &path) {
  SdFile dir;
  dir.open(path.c_str(), O_READ);

  dir.rewind();
  _server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  _server.send(200, "text/html", "");

  _server.sendContent(
    F("<html><head><meta charset=\"utf-8\"/><script>"
      "function _(el) {return document.getElementById(el);}"
      "function deleteUrl(url){ var xhr = new XMLHttpRequest(); xhr.onreadystatechange = function() { if (xhr.readyState === 4) { location.reload(); }}; xhr.open('DELETE', url); xhr.send();}\n"
      "function upload(folder){ var fileInput = _('fileInput'); if(fileInput.files.length === 0){ alert('Choose a file first'); return; } xhr = new XMLHttpRequest(); xhr.onreadystatechange = function() { if (xhr.readyState === 4) { location.reload(); }};\n"
      "var formData = new FormData(); for(var i = 0; i < fileInput.files.length; i++) { formData.append('data', fileInput.files[i], folder.concat(fileInput.files[i].name)); }; xhr.open('POST', '/');\n"
      "xhr.upload.addEventListener('progress', progressHandler, false); xhr.addEventListener('load', completeHandler, false); xhr.addEventListener('error', errorHandler, false); xhr.addEventListener('abort', abortHandler, false); _('ulDiv').style.display = 'block'; xhr.send(formData); }\n"
      "function progressHandler(event) { var percentage = Math.round((event.loaded / event.total) * 100); _('progressBar').value = percentage; _('ulStatus').innerHTML = percentage + '% (' + Math.ceil((event.loaded/(1024*1024)) * 10)/10 + '/' + Math.ceil((event.total/(1024*1024)) * 10)/10 + 'MB) uploaded'; }\n"
      "function completeHandler(event) { _('ulStatus').innerHTML = event.target.responseText; location.reload(); }\n"
      "function errorHandler(event) { _('ulStatus').innerHTML = 'Upload Failed'; }\n"
      "function abortHandler(event) { _('ulStatus').innerHTML = 'Upload Aborted'; }\n"
      "function writeRfid(url){ var xhr = new XMLHttpRequest(); xhr.onreadystatechange = function() { if (xhr.readyState === 4) { location.reload(); }}; xhr.open('POST', url); var formData = new FormData(); formData.append('write', 1); xhr.send(formData);}\n"
      "function play(url){ var xhr = new XMLHttpRequest(); xhr.onreadystatechange = function() { if (xhr.readyState === 4) { location.reload(); }}; xhr.open('POST', url); var formData = new FormData(); formData.append('play', 1); xhr.send(formData);}\n"
      "function playFile(url){ var xhr = new XMLHttpRequest(); xhr.open('POST', url); var formData = new FormData(); formData.append('playfile', 1); xhr.send(formData);}\n"
      "function playHttp(){ var streamUrl = _('streamUrl'); if(streamUrl != ''){ var xhr = new XMLHttpRequest(); xhr.onreadystatechange = function() { if (xhr.readyState === 4) { location.reload(); }}; xhr.open('POST', '/'); var formData = new FormData(); formData.append('streamUrl', streamUrl.value); xhr.send(formData);}}\n"
      "function rootAction(action){ var xhr = new XMLHttpRequest(); xhr.onreadystatechange = function() { if (xhr.readyState === 4) { location.reload(); }}; xhr.open('POST', '/'); var formData = new FormData(); formData.append(action, 1); xhr.send(formData);}\n"
      "function mkdir() { var folder = _('folder'); if(folder != ''){ var xhr = new XMLHttpRequest(); xhr.onreadystatechange = function() { if (xhr.readyState === 4) { location.reload(); }}; xhr.open('POST', '/'); var formData = new FormData(); formData.append('newFolder', folder.value); xhr.send(formData);}}\n"
      "function ota() { var xhr = new XMLHttpRequest(); xhr.onreadystatechange = function() { if (xhr.readyState === 4) { document.write('Please wait and do NOT turn off the power!'); location.reload(); }}; xhr.open('POST', '/'); var formData = new FormData(); formData.append('ota', 1); xhr.send(formData);}\n"
      "function formatNumbers() { var numbers = document.getElementsByClassName('number'); for (var i = 0; i < numbers.length; i++) { numbers[i].innerText = Number(numbers[i].innerText).toLocaleString(); }}\n"
      "function sortDivs(id) { var parent = _(id); var toSort = parent.childNodes; toSort = Array.prototype.slice.call(toSort, 0); toSort.sort(function (a, b) { return ('' + a.id).localeCompare(b.id); }); parent.innerHTML = ''; for(var i = 0, l = toSort.length; i < l; i++) { parent.appendChild(toSort[i]); }}\n"
      "</script><link rel=\"icon\" href=\"data:;base64,iVBORw0KGgo=\"></head><body>\n"));

  String output;

  if(_rfid.pairing) {
    char currentFolder[50];
    _SD.vwd()->getName(currentFolder, sizeof(currentFolder));

    output = F("<p style=\"font-weight: bold\">Pairing mode active. Place card on shelf to write current configuration for <span style=\"color:red\">/{currentFolder}</span> onto it</p>");
    output.replace("{currentFolder}", currentFolder);
    _server.sendContent(output);
  }

  if (path == "/") {
    if(_playback.playbackState() != PLAYBACK_NO) {
      output = F("<div>Currently playing: <strong>{currentFile}</strong> <a href=\"#\" onclick=\"rootAction('stop'); return false;\">&#x25a0;</a> <a href=\"#\" onclick=\"rootAction('skip'); return false;\">&#10097;&#10097;</a></div>");
      output.replace("{currentFile}", _playback.currentFile());
      _server.sendContent(output);
    }
    output = F("<div>Volume: {volume} <a href=\"#\" onclick=\"rootAction('volumeUp'); return false;\">+</a> / <a href=\"#\" onclick=\"rootAction('volumeDown'); return false;\">-</a></div>"
      "<form onsubmit=\"playHttp(); return false;\"><input type=\"text\" name=\"streamUrl\" id=\"streamUrl\"><input type=\"button\" value=\"stream\" onclick=\"playHttp(); return false;\"></form>"
      "<form onsubmit=\"mkdir(); return false;\"><input type=\"text\" name=\"folder\" id=\"folder\"><input type=\"button\" value=\"mkdir\" onclick=\"mkdir(); return false;\"></form>");
    output.replace("{volume}", String(50-_playback.volume()));
    output.replace("{folder}", path);
    _server.sendContent(output);
  } else {
    output = F("<form><p><input type=\"file\" multiple=\"true\" name=\"data\" id=\"fileInput\"><input type=\"button\" value=\"upload\" onclick=\"upload('{folder}'); return false;\"></p>"
      "<div id=\"ulDiv\" style=\"display:none;\"><progress id=\"progressBar\" value=\"0\" max=\"100\" style=\"width:300px;\"></progress>"
      "<p id=\"ulStatus\"></p></div></form>");
    output.replace("{folder}", path);
    _server.sendContent(output);
    _server.sendContent(F("<div><a href=\"..\">..</a></div>"));
  }

  SdFile entry;
  _server.sendContent(F("<div id=\"folders\">"));
  while (entry.openNext(&dir, O_READ)) {
    if (!entry.isDir()) {
      entry.close();
      continue;
    }

    char filenameChar[100];
    entry.getName(filenameChar, 100);
    // TODO encode special characters
    String filename = String(filenameChar);

    output = F("<div id=\"{name}\">&#x1f4c2; <a href=\"{name}/\">{name}</a> <a href=\"#\" onclick=\"deleteUrl('{name}'); return false;\" title=\"delete\">&#x2718;</a>");
    // Currently only foldernames <= 16 characters can be written onto the rfid
    if (filename.length() <= 16) {
      output += F("<a href=\"#\" onclick=\"writeRfid('{name}');\" title=\"write to card\">&#x1f4be;</a> "
        "<a href=\"#\" onclick=\"play('{name}'); return false;\" title=\"play folder\">&#9654;</a>");
    }
    output.replace("{name}", filename);
    _server.sendContent(output);
    _server.sendContent(F("</div>"));
    entry.close();
  }
  dir.rewind();
  _server.sendContent(F("</div><div id=\"files\">"));
  while (entry.openNext(&dir, O_READ)) {
    if (entry.isDir()) {
      entry.close();
      continue;
    }
    char fileNameChar[100];
    entry.getName(fileNameChar, 100);
    Serial.println(fileNameChar);
    // TODO encode special characters
    String fileName = String(fileNameChar);
    String fileSize = String(entry.fileSize());

    output = F("<div id=\"{name}\">{icon}<a href=\"{name}\">{name}</a> (<span class=\"number\">{size}</span> bytes) <a href=\"#\" onclick=\"deleteUrl('{name}'); return false;\" title=\"delete\">&#x2718;</a>{playfilebutton}</div>");
    if (Adafruit_VS1053_FilePlayer::isMP3File(fileNameChar)) {
      output.replace("{icon}", F("&#x266b; "));
    } else {
      output.replace("{icon}", "");
    }
    if(Adafruit_VS1053_FilePlayer::isMP3File(fileNameChar) && (path != "/")) {
      output.replace("{playfilebutton}", " <a href=\"#\" onclick=\"playFile('{name}'); return false;\" title=\"play\">&#9654;</a>");
    } else {
      output.replace("{playfilebutton}", "");
    }
    output.replace("{name}", fileName);
    output.replace("{size}", fileSize);
    _server.sendContent(output);
    entry.close();
  }
  _server.sendContent(F("</div>"));
  if (path == "/") {
    output = F("<br><br><form>Version {major}.{minor} <input type=\"button\" value=\"Update Firmware\" onclick=\"ota(); return false;\"</form>");
    output.replace("{major}", String(MAJOR_VERSION));
    output.replace("{minor}", String(MINOR_VERSION));
    _server.sendContent(output);
  }

  // Dirty client side UI stuff that's too complicated (for me) in C
  _server.sendContent(F("<script>"
    "formatNumbers();"
    "sortDivs('folders');"
    "sortDivs('files');"
    "</script></body></html>"));
}

bool ShelfWeb::loadFromSdCard(String &path) {
  String dataType = "application/octet-stream";

  if (path.endsWith(".HTM")) dataType = "text/html";
  else if (path.endsWith(".CSS")) dataType = "text/css";
  else if (path.endsWith(".JS")) dataType = "application/javascript";

  File dataFile = _SD.open(path.c_str());

  if (!dataFile) {
    Serial.println("File not open");
    return false;
  }

  if (dataFile.isDir()) {
    // dataFile.name() will always be "/" for directorys, so we cannot know if we are in the root directory without handing it over
    renderDirectory(path);
  } else {
    if (_server.streamFile(dataFile, dataType) != dataFile.size()) {
      Serial.println("Sent less data than expected!");
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

void ShelfWeb::handleNotFound() {
  String path = _server.urlDecode(_server.uri());
  Serial.println(F("Request to: ") + path);
  if (_server.method() == HTTP_GET) {
    if (loadFromSdCard(path)) return;
  } else if (_server.method() == HTTP_DELETE) {
    if (_server.uri() == "/" || !_SD.exists(path.c_str())) {
      returnHttpStatus((uint8_t)500, "BAD PATH: " + _server.uri());
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
      
      WiFiClient client;
      t_httpUpdate_return ret = ESPhttpUpdate.update(client, UPDATE_URL);
      switch (ret) {
        case HTTP_UPDATE_FAILED:
          Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
          returnHttpStatus((uint8_t)500, "Update failed, please try again");
          return;
        case HTTP_UPDATE_NO_UPDATES:
          Serial.println(F("HTTP_UPDATE_NO_UPDATES"));
          returnHttpStatus(200, "No update available");
          return;
        case HTTP_UPDATE_OK:
          Serial.println(F("HTTP_UPDATE_OK"));
          break;
      }
      returnOK();
      return;
    } else if (_server.hasArg("stop")) {
      _playback.stopPlayback();
      returnOK();
      return;
    } else if (_server.hasArg("skip")) {
      _playback.skipFile();
      returnOK();
      return;
    } else if (_server.hasArg("volumeUp")) {
      _playback.volumeUp();
      returnOK();
      return;
    } else if (_server.hasArg("volumeDown")) {
      _playback.volumeDown();
      returnOK();
      return;
    } else if (_server.hasArg("streamUrl")) {
      _playback.stopPlayback();
      _playback.currentStreamUrl = _server.arg("streamUrl");
      _playback.playHttp();
      _playback.playingByCard = false;
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
        _playback.playFile();
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
            _playback.playFile(folder, file);
            _playback.playingByCard = false;
            returnOK();
            return;
          }
        }
      }
    }
  }

  // 404 otherwise
  returnHttpStatus((uint8_t)404, "Not found");
  Serial.println("404: " + path);
}

void ShelfWeb::handleWriteRfid(String &folder) {
  if (_playback.switchFolder((char *)folder.c_str())) {
    _rfid.pairing = true;
    returnOK();
  } else {
    returnHttpStatus((uint8_t)404, "Not found");
  }
}

void ShelfWeb::work() {
  _server.handleClient();
}
