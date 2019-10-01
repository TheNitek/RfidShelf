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
  _server.send_P(200, "text/plain", "");
}

void ShelfWeb::returnHttpStatus(uint16_t statusCode, const char *msg) {
  _server.send_P(statusCode, "text/plain", msg);
}

void ShelfWeb::sendHTML() {
  _server.send(200, "text/html", 
    F("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">"
      "<html><head><meta charset=\"utf-8\"><script>"
      "function _(e){return document.getElementById(e);}"
      "function $(q){return Array.from(document.querySelectorAll(q))}"
      "function b(c){ document.body.innerHTML=c}"
      "function t(c,v){$(c).forEach(function(e){e.innerText = v})}"
      "function ajax(m, url, cb, p) {var xhr = new XMLHttpRequest(); xhr.onreadystatechange = function() { if (xhr.readyState == 4) { cb(xhr.responseText); }}; xhr.open(m, url, true); xhr.send(p);}\n"
      "function deleteUrl(url){ if(confirm(\"Delete?\"))ajax('DELETE', url, reload);}\n"
      "function upload(folder){ var fileInput = _('fileInput'); if(fileInput.files.length === 0){ alert('Choose a file first'); return; } var fileTooLong = false; Array.prototype.forEach.call(fileInput.files, function(file) { if (file.name.length > 100) { fileTooLong = true; }}); if (fileTooLong) { alert(\"File name too long. Files can be max. 100 characters long.\"); return; } xhr = new XMLHttpRequest(); xhr.onreadystatechange = function() { if (xhr.readyState === 4) { location.reload(); }};\n"
      "var d = new FormData(); for(var i = 0; i < fileInput.files.length; i++) { d.append('data', fileInput.files[i], folder.concat(fileInput.files[i].name)); }; xhr.open('POST', '/');\n"
      "xhr.upload.addEventListener('progress', progressHandler, false); xhr.addEventListener('load', completeHandler, false); xhr.addEventListener('error', errorHandler, false); xhr.addEventListener('abort', abortHandler, false); _('ulDiv').style.display = 'block'; xhr.send(d); }\n"
      "function progressHandler(event) { var percentage = Math.round((event.loaded / event.total) * 100); _('progressBar').value = percentage; _('ulStatus').innerHTML = percentage + '% (' + Math.ceil((event.loaded/(1024*1024)) * 10)/10 + '/' + Math.ceil((event.total/(1024*1024)) * 10)/10 + 'MB) uploaded'; }\n"
      "function completeHandler(event) { _('ulStatus').innerHTML = event.target.responseText; location.reload(); }\n"
      "function errorHandler(event) { _('ulStatus').innerHTML = 'Upload Failed'; }\n"
      "function abortHandler(event) { _('ulStatus').innerHTML = 'Upload Aborted'; }\n"
      "function reload(){ location.reload(); }\n"
      "function loadStatus(){ajax('GET', '/?status=1', renderStatus)}\n"
      "function loadFS(url){ajax('GET', url+'?fs=1', renderFS)}\n"
      "function writeRfid(url){ if(url.length > 16) {alert('Folder name cannot have more than 16 characters'); return;} var d = new FormData(); d.append('write', 1); ajax('POST', url, renderStatus, d);}\n"
      "function play(url){var d = new FormData(); d.append('play', 1); ajax('POST', url, renderStatus, d);}\n"
      "function playFile(url){var d = new FormData(); d.append('playfile', 1); ajax('POST', url, renderStatus, d);}\n"
      "function rootAction(action){var d = new FormData(); d.append(action, 1); ajax('POST', '/', renderStatus, d);}\n"
      "function volume(action){var d = new FormData(); d.append(action, 1); ajax('POST', '/', renderStatus, d);}\n"
      "function mkdir(){var f = _('folder').value; if(f != ''){var d = new FormData(); d.append('newFolder', f); ajax('POST', '/', reload, d);}}\n"
      "function ota(){var d = new FormData(); d.append('ota', 1); ajax('POST', '/', function(){ b('Please wait and do NOT turn off the power!'); location.reload();}, d);}\n"
      "function downloadpatch(){ var xhr = new XMLHttpRequest(); xhr.onreadystatechange = function() { if (xhr.readyState === 1) { document.write('Please wait while downloading patch! When the download was successful the system is automatically restarting.'); } else if (xhr.readyState === 4) { location.reload(); }}; xhr.open('POST', '/'); var formData = new FormData(); formData.append('downloadpatch', 1); xhr.send(formData);}\n"
      "function formatBytes(a){if(0==a)return\"0 Bytes\";var c=1024,e=[\"Bytes\",\"KB\",\"MB\",\"GB\"],f=Math.floor(Math.log(a)/Math.log(c));return parseFloat((a/Math.pow(c,f)).toFixed(2))+\" \"+e[f]}\n"
      "function formatNumbers(){ $('.number').forEach(function(n){n.innerText = formatBytes(n.innerText); })}\n"
      "function renderFS(r){"
        "var p = JSON.parse(r);"
        "_('fs').innerHTML = '';"
        "_('uploadForm').innerHTML=_('uploadForm').innerHTML.replace(/{path}/g, p.path);"
        "if(p.path == '/'){$('.hiddenNoRoot').forEach(function(e){e.style.display='initial'}); $('.hiddenRoot').forEach(function(e){e.style.display='none'});}"
        "else{$('.hiddenRoot').forEach(function(e){e.style.display='initial'}); $('.hiddenNoRoot').forEach(function(e){e.style.display='none'});}"
        "p.fs.sort(sortFS); p.fs.forEach(renderFSentry); formatNumbers();}\n"
      "function sortFS(a, b){ if(('size' in a) == ('size' in b)) return ('' + a.name).localeCompare(b.name); else if(!('size' in a)) return -1; else return 1;}"
      "function renderFSentry(e){ "
        "if('size' in e){ let t = document.importNode(_('fileT').content.querySelector('div'), true); t.innerHTML = t.innerHTML.replace(/\\{filename\\}/g, e.name).replace(/\\{filesize\\}/g, e.size); if(e.name.endsWith('.mp3')){t.classList.add('mp3')} _('fs').appendChild(t);}"
        "else{ let t = document.importNode(_('folderT').content.querySelector('div'), true); t.innerHTML = t.innerHTML.replace(/\\{foldername\\}/g, e.name); _('fs').appendChild(t);}"
      " }\n"
      "function renderStatus(r){ "
        "var s = JSON.parse(r);"
        "if('pairing' in s){t('.pairingFile', '/' + s.pairing); _('pairing').style.display='initial'}"
        "else{_('pairing').style.display='none'}"
        "var pb=_('playback');"
        "if('currentFile' in s){t('.currentFile', s.currentFile)}"
        "if(s.playback == 'FILE'){pb.className='playing';}"
        "if(s.playback == 'PAUSED'){pb.className='paused';}"
        "var v=_('volume');"
        "if(s.night){v.className='night';}"
        "else{v.className='noNight';}"
        "_('volumeBar').value=s.volume; _('volumeBar').innerHTML=s.volume;"
        "if(!s.patch){_('patchForm').style.display='initial'}"
        "t('.firmwareVersion', s.version);"
        "t('.shelfTime', (new Date(s.time * 1000)).toUTCString());"
      "}\n"
      "</script>"
      "<link rel=\"icon\" href=\"data:;base64,iVBORw0KGgo=\">"
      "<title>RfidShelf</title>"
      "<style>"
      "body { font-family: Arial, Helvetica; font-size: 150%} "
      "#fs {vertical-align: middle;} #fs div:nth-child(even) { background: LightGray;} "
      "a { color: #0000EE; text-decoration: none;} "
      "a.del { color: red;}"
      ".mp3only, .hiddenPlaying, .hiddenPaused, .hidden, .hiddenNight, .hiddenNoNight, .hiddenNoRoot, .hiddenRoot { display: none;}"
      ".mp3 .mp3only, .playing .hiddenPaused, .paused .hiddenPlaying, .night .hiddenNoNight, .noNight .hiddenNight { display: initial;}"
      "</style>"
      "</head><body>\n"
      "<template id=\"folderT\">"
      "<div class=\"folder\">&#x1f4c2; <a href=\"{foldername}/\">{foldername}</a> "
      "<a href=\"#\" class=\"del\" onclick=\"deleteUrl('{foldername}'); return false;\" title=\"delete\">&#x2718;</a> "
      "<a href=\"#\" onclick=\"writeRfid('{foldername}');\" title=\"write to card\">&#x1f4be;</a> "
      "<a href=\"#\" onclick=\"play('{foldername}'); return false;\" title=\"play folder\">&#x25b6;</a></div>"
      "</template>"
      "<template id=\"fileT\">"
      "<div class=\"file\">"
      "<span class=\"mp3only\">&#x266b; </span>"
      "<a href=\"{filename}\">{filename}</a> (<span class=\"number\">{filesize}</span>) "
      "<a href=\"#\" class=\"del\" onclick=\"deleteUrl('{filename}'); return false;\" title=\"delete\">&#x2718;</a> "
      "<a href=\"#\" class=\"mp3only\" onclick=\"playFile('{filename}'); return false;\" title=\"play\">&#x25b6;</a></div>"
      "</template>"
      "<p style=\"font-weight: bold; display: none;\" id=\"pairing\">Pairing mode active. Place card on shelf to write current configuration for <span class=\"pairingFile\" style=\"color:red\"> </span> onto it</p>"
      "<p id=\"playback\" class=\"hidden\">Currently playing: <strong class=\"currentFile\"> </strong><br>"
      " <a class=\"hiddenPlaying\" href=\"#\" onclick=\"rootAction('resume'); return false;\">&#x25b6;</a>"
      " <a class=\"hiddenPaused\" href=\"#\" onclick=\"rootAction('pause'); return false;\">&#x23f8;</a>"
      " <a href=\"#\" onclick=\"rootAction('stop'); return false;\">&#x23f9;</a>"
      " <a href=\"#\" onclick=\"rootAction('skip'); return false;\">&#x23ed;</a></p>"
      "<p id=\"volume\">"
        "Volume:&nbsp;<meter id=\"volumeBar\" value=\"0\" max=\"50\" style=\"width:300px;\">0</meter><br>"
        "<a title=\"decrease volume\" href=\"#\" onclick=\"volume('volumeDown'); return false;\">&#x1f509;</a> / "
        "<a title=\"increase volume\" href=\"#\" onclick=\"volume('volumeUp'); return false;\">&#x1f50a;</a> "
        "<a class=\"hiddenNoNight\" href=\"#\" title=\"deactivate night mode\" onclick=\"rootAction('toggleNight'); return false;\">&#x1f31b;</a>"
        "<a class=\"hiddenNight\" href=\"#\" title=\"activate night mode\" onclick=\"rootAction('toggleNight'); return false;\">&#x1f31e;</a>"
      "</p>"
      "<form class=\"hiddenNoRoot\" onsubmit=\"mkdir(); return false;\">Create new folder: "
        "<input type=\"text\" name=\"folder\" id=\"folder\">"
        "<input type=\"button\" value=\"Create\" onclick=\"mkdir(); return false;\"></form>"
      "<form class=\"hiddenRoot\" id=\"uploadForm\"><p>Upload MP3 file: <input type=\"file\" multiple=\"true\" name=\"data\" accept=\".mp3\" id=\"fileInput\">"   // Upload
        "<input type=\"button\" value=\"upload\" onclick=\"upload('{path}'); return false;\"></p>"
        "<div id=\"ulDiv\" style=\"display:none;\"><progress id=\"progressBar\" value=\"0\" max=\"100\" style=\"width:300px;\"></progress>"
        "<p id=\"ulStatus\"></p></div></form>"
      "<p class=\"hiddenRoot\"><a href=\"..\">Back to top</a></p>"
      "<div id=\"fs\">Loading ...</div>"
      "<form id=\"patchForm\" class=\"hidden\"><p><b>MP3 decoder patch missing</b> (might reduce sound quality) <input type=\"button\" value=\"Download + Install VS1053 patch\" onclick=\"downloadpatch(); return false;\"></p></form>"
      "<form id=\"firmwareForm\"><p>Version <span class=\"firmwareVersion\"> </span> <input type=\"button\" value=\"Update Firmware\" onclick=\"ota(); return false;\"></p></form>"
      "<p id=\"time\">Shelf Time: <span class=\"shelfTime\"> </span></p>"
      "<script>loadStatus(); loadFS('.')</script>"
      "</body></html>"
      ));
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

bool ShelfWeb::loadFromSdCard(const char *path, bool fs) {
  File dataFile = _SD.open(path);

  if (!dataFile) {
    Sprintln(F("File not open"));
    returnHttpStatus(404, "Not found");
    return false;
  }

  if (dataFile.isDir()) {
    if(fs) {
      sendJsonFS(path);
    } else {
      sendHTML();
    }
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
    if(_server.hasArg("status")){
      sendJsonStatus();
    }else{
      loadFromSdCard(path.c_str(), _server.hasArg("fs"));
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