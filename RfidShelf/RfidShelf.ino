#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <SPI.h>
#include <MFRC522.h>
#include <SdFat.h>
#include <Adafruit_VS1053.h>
#include <RingBufCPP.h>

#define RC522_CS        D8
#define SD_CS           D2

#define BREAKOUT_RESET  -1     // VS1053 reset pin (output)
#define BREAKOUT_CS     D3     // VS1053 chip select pin (output)
#define BREAKOUT_DCS    D0    // VS1053 Data/command select pin (output)
#define DREQ            D1    // VS1053 Data request (input)

#define AMP_POWER       D4

Adafruit_VS1053_FilePlayer musicPlayer =
  Adafruit_VS1053_FilePlayer(BREAKOUT_RESET, BREAKOUT_CS, BREAKOUT_DCS, DREQ, SD_CS);

WiFiManager wifiManager;

ESP8266WebServer server(80);

MFRC522 mfrc522(RC522_CS, UINT8_MAX);   // Create MFRC522 instance.

MFRC522::MIFARE_Key key;

#define MAJOR_VERSION 1
#define MINOR_VERSION 3

// Lower value means louder!
#define DEFAULT_VOLUME 10

#define PLAYING_NO 0
#define PLAYING_FILE 1
#define PLAYING_HTTP 2

// Number off consecutive http reconnects before giving up stream
#define MAX_RECONNECTS 10

// Init array that will store new card uid
byte lastCardUid[4];
uint8_t playing = PLAYING_NO;
bool playingByCard = true;
bool pairing = false;
String currentFile = F("");
String currentStreamUrl = F("");
uint8_t reconnectCount = 0;
uint8_t volume = DEFAULT_VOLUME;
unsigned long lastRfidCheck;

SdFat SD;
SdFile uploadFile;

HTTPClient http;
WiFiClient * stream;

void setup() {
  // Seems to make flashing more reliable
  delay(100);

  Serial.begin(115200);
  Serial.println();
  Serial.println("Starting ...");

  if (AMP_POWER > 0) {
    // Disable amp
    pinMode(AMP_POWER, OUTPUT);
    digitalWrite(AMP_POWER, LOW);
    Serial.println(F("Amp powered down"));
  }

  // Init SPI SS pins
  pinMode(RC522_CS, OUTPUT);
  digitalWrite(RC522_CS, HIGH);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  pinMode(BREAKOUT_CS, OUTPUT);
  digitalWrite(BREAKOUT_CS, HIGH);
  pinMode(BREAKOUT_DCS, OUTPUT);
  digitalWrite(BREAKOUT_DCS, HIGH);

  SPI.begin();        // Init SPI bus
  mfrc522.PCD_Init(); // Init MFRC522 card

  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  Serial.print(F("Using rfid key (for A and B): "));
  print_byte_array(key.keyByte, MFRC522::MF_KEY_SIZE);
  Serial.println();

  //Initialize the SdCard.
  if (!SD.begin(SD_CS)) {
    Serial.println(F("Could not initialize SD card"));
    SD.initErrorHalt();
  }
  Serial.println(F("SD initialized"));

  // initialise the music player
  if (!musicPlayer.begin()) { // initialise the music player
    Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
    while (1) delay(500);
  }
  Serial.println(F("VS1053 found"));

  /* Fix for the design fuckup of the cheap LC Technology MP3 shield
    see http://www.bajdi.com/lcsoft-vs1053-mp3-module/#comment-33773
    Doesn't hurt for other shields
  */
  musicPlayer.sciWrite(VS1053_REG_WRAMADDR, VS1053_GPIO_DDR);
  musicPlayer.sciWrite(VS1053_REG_WRAM, 0x0003);
  musicPlayer.GPIO_digitalWrite(0x0000);
  musicPlayer.softReset();

  Serial.println(F("VS1053 soft reset done"));

  if (patchVS1053()) {
    // Enable Mono Output
    musicPlayer.sciWrite(VS1053_REG_WRAMADDR, 0x1e09);
    musicPlayer.sciWrite(VS1053_REG_WRAM, 0x0001);

    // Enable differential output
    /*uint16_t mode = VS1053_MODE_SM_DIFF | VS1053_MODE_SM_SDINEW;
      musicPlayer.sciWrite(VS1053_REG_MODE, mode); */
      Serial.println(F("VS1053 patch installed"));
  } else {
    Serial.println(F("Could not load patch"));
  }

  Serial.print(F("SampleRate "));
  Serial.println(musicPlayer.sciRead(VS1053_REG_AUDATA));

  musicPlayer.setVolume(volume, volume);

  musicPlayer.dumpRegs();

  Serial.println(F("VS1053 found"));

  wifiManager.setConfigPortalTimeout(3 * 60);
  if (!wifiManager.autoConnect("MP3-SHELF-SETUP", "lacklack")) {
    Serial.println(F("Setup timed out, starting AP"));
    WiFi.mode(WIFI_AP);
    WiFi.softAP("MP3-SHELF", "lacklack");
  }

  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_POST, handleNotFound, handleFileUpload);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println(F("Wifi initialized"));

  Serial.println(F("Init done"));
}

void loop() {

  if (playing == PLAYING_FILE) {
    if (musicPlayer.playingMusic) {
      musicPlayer.feedBuffer();
    } else {
      playFile();
    }
  }

  if (playing == PLAYING_HTTP) {
    feedPlaybackFromHttp();
  }

  if ((playing == PLAYING_NO) || (millis() - lastRfidCheck > 500)) {
    handleRfid();
    lastRfidCheck = millis();
  }

  server.handleClient();
}

void handleRfid() {

  // While playing check if the tag is still present
  if ((playing != PLAYING_NO) && playingByCard) {

    // Since wireless communication is voodoo we'll give it a few retrys before killing the music
    for (int i = 0; i < 3; i++) {
      // Detect Tag without looking for collisions
      byte bufferATQA[2];
      byte bufferSize = sizeof(bufferATQA);

      MFRC522::StatusCode result = mfrc522.PICC_WakeupA(bufferATQA, &bufferSize);

      if (result == mfrc522.STATUS_OK && mfrc522.PICC_ReadCardSerial() && (
            mfrc522.uid.uidByte[0] == lastCardUid[0] &&
            mfrc522.uid.uidByte[1] == lastCardUid[1] &&
            mfrc522.uid.uidByte[2] == lastCardUid[2] &&
            mfrc522.uid.uidByte[3] == lastCardUid[3] )) {
        mfrc522.PICC_HaltA();
        return;
      }
    }

    stopPlayback();
  }

  // Look for new cards and select one if it exists
  if (!mfrc522.PICC_IsNewCardPresent() ||  !mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  // Show some details of the PICC (that is: the tag/card)
  Serial.print(F("Card UID:"));
  print_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
  Serial.print(F("PICC type: "));
  MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
  Serial.println(mfrc522.PICC_GetTypeName(piccType));

  // Check for compatibility
  if (piccType != MFRC522::PICC_TYPE_MIFARE_1K ) {
    Serial.println(F("Unsupported card."));
    return;
  }

  uint8_t configLength = 3;
  if (pairing) {
    char writeFolder[17];
    SD.vwd()->getName(writeFolder, 17);
    writeRfidBlock(1, 0, (uint8_t*) writeFolder, 17);
    // Store config (like volume)
    uint8_t configBuffer[configLength];
    // Magic number to mark config block and distinguish legacy cards without it
    configBuffer[0] = 137;
    // Length of config entry without header
    configBuffer[1] = configLength-2;
    configBuffer[2] = volume;
    writeRfidBlock(2, 0, configBuffer, configLength);
    pairing = false;
  }

  // Reset watchdog timer
  yield();
  uint8_t readBuffer[18];
  if (readRfidBlock(1, 0, readBuffer, sizeof(readBuffer))) {
    // Store uid
    memcpy(lastCardUid, mfrc522.uid.uidByte, 4 * sizeof(uint8_t) );

    if(readBuffer[0] != '\0') {
      char readFolder[18];
      readFolder[0] = '/';
      // allthough sizeof(readBuffer) == 18, we only get 16 byte of data
      memcpy(readFolder + 1 , readBuffer, 16 * sizeof(uint8_t) );
      // readBuffer will already contain \0 if the folder name is < 16 chars, but otherwise we need to add it
      readFolder[17] = '\0';
  
      if (playing != PLAYING_NO) {
        stopPlayback();
      }
      
      if (switchFolder(readFolder)) {
        uint8_t configBuffer[18];
        if (readRfidBlock(2, 0, configBuffer, sizeof(configBuffer)) && (configBuffer[0] == 137)) {
          if(configBuffer[1] > 0) {
            Serial.print(F("Setting volume: ")); Serial.println(configBuffer[2]);
            volume = configBuffer[2];
            musicPlayer.setVolume(volume, volume);
          }
        } else {
          volume = DEFAULT_VOLUME;
          musicPlayer.setVolume(volume, volume);
        }
  
        playFile();
        playingByCard = true;
      }
    }
  }

  // Halt PICC
  mfrc522.PICC_HaltA();
  // Stop encryption on PCD
  mfrc522.PCD_StopCrypto1();

}

bool writeRfidBlock(uint8_t sector, uint8_t relativeBlock, const uint8_t *newContent, uint8_t contentSize) {
  uint8_t absoluteBlock = (sector * 4) + relativeBlock;

  // Authenticate using key A
  Serial.println(F("Authenticating using key A..."));
  MFRC522::StatusCode status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, absoluteBlock, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("PCD_Authenticate() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }

  uint8_t bufferSize = 16;
  byte buffer[bufferSize];
  if (contentSize < bufferSize) {
    bufferSize = contentSize;
  }
  memcpy(buffer, newContent, bufferSize * sizeof(byte) );

  // Write block
  Serial.print(F("Writing data to block: ")); print_byte_array(newContent, contentSize);
  status = mfrc522.MIFARE_Write(absoluteBlock, buffer, 16);
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("MIFARE_Write() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }
  return true;
}

/**
   read a block from a rfid card into outputBuffer which needs to be >= 18 bytes long
*/
bool readRfidBlock(uint8_t sector, uint8_t relativeBlock, uint8_t *outputBuffer, uint8_t bufferSize) {
  if (relativeBlock > 3) {
    Serial.println(F("Invalid block number"));
    return false;
  }

  uint8_t absoluteBlock = (sector * 4) + relativeBlock;

  // Authenticate using key A
  Serial.println(F("Authenticating using key A..."));
  MFRC522::StatusCode status = (MFRC522::StatusCode) mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, absoluteBlock, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("PCD_Authenticate() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }

  Serial.print(F("Reading block "));
  Serial.println(absoluteBlock);
  status = mfrc522.MIFARE_Read(absoluteBlock, outputBuffer, &bufferSize);
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("MIFARE_Read() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }
  Serial.print(F("Data in block ")); Serial.print(absoluteBlock); Serial.println(F(":"));
  print_byte_array(outputBuffer, 16);
  Serial.println();
  Serial.println();

  return true;
}


void print_byte_array(const uint8_t *buffer, const uint8_t  bufferSize) {
  for (uint8_t i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
  Serial.println();
}

bool switchFolder(const char *folder) {
  Serial.print(F("Switching folder to "));
  Serial.println(folder);

  if (!SD.exists(folder)) {
    Serial.println(F("Folder does not exist"));
    return false;
  }
  SD.chdir(folder);
  SD.vwd()->rewind();
  currentFile = "";
  return true;
}

void stopPlayback() {
  Serial.println(F("Stopping playback"));
  if (AMP_POWER > 0) {
    digitalWrite(AMP_POWER, LOW);
  }
  if(playing == PLAYING_FILE) {
    musicPlayer.stopPlaying();
  } else if (playing == PLAYING_HTTP) {
    http.end();
  }
  playing = PLAYING_NO;
}

void playFile() {
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

    String tmpFile = String(filenameChar);
    if (file.isDir() || !isMP3File(tmpFile)) {
      Serial.print(F("Ignoring "));
      Serial.println(tmpFile);
      continue;
    }

    if (currentFile < tmpFile && (tmpFile < nextFile || nextFile == "")) {
      nextFile = tmpFile;
    }
  }

  // Start folder from the beginning
  if (nextFile == "" && currentFile != "") {
    currentFile = "";
    playFile();
    return;
  }

  // No currentFile && no nextFile => Nothing to play!
  if (nextFile == "") {
    Serial.print(F("No mp3 files in "));
    Serial.println(dirname);
    stopPlayback();
    return;
  }

  String fullpath = dirname + nextFile;

  Serial.print(F("Playing "));
  Serial.println(fullpath);

  playing = PLAYING_FILE;
  currentFile = nextFile;
  musicPlayer.startPlayingFile((char *)fullpath.c_str());

  if (AMP_POWER > 0) {
    digitalWrite(AMP_POWER, HIGH);
  }
}

void playHttp() {
  if(!currentStreamUrl) {
    Serial.println(F("StreamUrl not set"));
    return;
  }
  http.begin(currentStreamUrl);
  int httpCode = http.GET();
  int len = http.getSize();
  if ((httpCode != HTTP_CODE_OK) || !(len > 0 || len == -1)) {
    Serial.println(F("Webradio request failed"));
    return;
  }

  stream = http.getStreamPtr();
  playing = PLAYING_HTTP;
  Serial.println(F("Initialized HTTP stream"));
}

/* Since there isn't much RAM to use we don't use any buffering here.
 * Using a small ring buffer here made things worse
 */
void feedPlaybackFromHttp() {
  if(http.connected()) {
    reconnectCount = 0;
    // get available data size
    size_t available = stream->available();

    // VS1053 accepts at least 32 bytes when "ready" so we can batch the data transfer
    while((available >= 32) && musicPlayer.readyForData()) {
      uint8_t buff[32] = { 0 };
      int c = stream->readBytes(buff, ((available > sizeof(buff)) ? sizeof(buff) : available));

      musicPlayer.playData(buff, c);
    }
  } else {
    Serial.println(F("Reconnecting http"));
    stopPlayback();
    if(reconnectCount < MAX_RECONNECTS) {
      playHttp();
      reconnectCount++;
    }
  }
}

void volumeUp() {
  volume -= 5;
  if(volume > 50) {
    volume = 0;
  }
  musicPlayer.setVolume(volume, volume);
}

void volumeDown() {
  volume += 5;
  if(volume > 50) {
    volume = 50;
  }
  musicPlayer.setVolume(volume, volume);
}

bool isMP3File(String &filename) {
  return filename.endsWith(".mp3");
}

void returnOK() {
  server.send(200, "text/plain", "");
}

void returnHttpStatus(uint8_t statusCode, String msg) {
  server.send(statusCode, "text/plain", msg);
}

void renderDirectory(String &path) {
  SdFile dir;
  dir.open(path.c_str(), O_READ);

  dir.rewind();
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");

  server.sendContent(
    F("<html><head><meta charset=\"utf-8\"/><script>"
      "function deleteUrl(url){ var xhr = new XMLHttpRequest(); xhr.onreadystatechange = function() { if (xhr.readyState === 4) { location.reload(); }}; xhr.open('DELETE', url); xhr.send();}"
      "function upload(folder){ var fileInput = document.getElementById('fileInput'); if(fileInput.files.length === 0){ alert('Choose a file first'); return; } xhr = new XMLHttpRequest(); xhr.onreadystatechange = function() { if (xhr.readyState === 4) { location.reload(); }};"
      "var formData = new FormData(); for(var i = 0; i < fileInput.files.length; i++) { formData.append('data', fileInput.files[i], folder.concat(fileInput.files[i].name)); }; xhr.open('POST', '/'); xhr.send(formData); }"
      "function writeRfid(url){ var xhr = new XMLHttpRequest(); xhr.onreadystatechange = function() { if (xhr.readyState === 4) { location.reload(); }}; xhr.open('POST', url); var formData = new FormData(); formData.append('write', 1); xhr.send(formData);}"
      "function play(url){ var xhr = new XMLHttpRequest(); xhr.onreadystatechange = function() { if (xhr.readyState === 4) { location.reload(); }}; xhr.open('POST', url); var formData = new FormData(); formData.append('play', 1); xhr.send(formData);}"
      "function playHttp(){ var streamUrl = document.getElementById('streamUrl'); if(streamUrl != ''){ var xhr = new XMLHttpRequest(); xhr.onreadystatechange = function() { if (xhr.readyState === 4) { location.reload(); }}; xhr.open('POST', '/'); var formData = new FormData(); formData.append('streamUrl', streamUrl.value); xhr.send(formData);}}"
      "function rootAction(action){ var xhr = new XMLHttpRequest(); xhr.onreadystatechange = function() { if (xhr.readyState === 4) { location.reload(); }}; xhr.open('POST', '/'); var formData = new FormData(); formData.append(action, 1); xhr.send(formData);}"
      "function mkdir() { var folder = document.getElementById('folder'); if(folder != ''){ var xhr = new XMLHttpRequest(); xhr.onreadystatechange = function() { if (xhr.readyState === 4) { location.reload(); }}; xhr.open('POST', '/'); var formData = new FormData(); formData.append('newFolder', folder.value); xhr.send(formData);}}"
      "function ota() { var xhr = new XMLHttpRequest(); xhr.onreadystatechange = function() { if (xhr.readyState === 4) { document.write('Please wait and do NOT turn off the power!'); location.reload(); }}; xhr.open('POST', '/'); var formData = new FormData(); formData.append('ota', 1); xhr.send(formData);}"
      "</script><link rel=\"icon\" href=\"data:;base64,iVBORw0KGgo=\"></head><body>"));

  String output;
  
  if(pairing) {
    char currentFolder[50];
    SD.vwd()->getName(currentFolder, sizeof(currentFolder));

    output = F("<p style=\"font-weight: bold\">Pairing mode active. Place card on shelf to write current configuration for <span style=\"color:red\">/{currentFolder}</span> onto it</p>");
    output.replace("{currentFolder}", currentFolder);
    server.sendContent(output);
  }
  
  if (path == "/") {
    if(playing != PLAYING_NO) {
      output = F("<div>Currently playing: <strong>{currentFile}</strong> (<a href=\"#\" onclick=\"rootAction('stop'); return false;\">&#x25a0;</a>)</div>");
      output.replace("{currentFile}", currentFile);
      server.sendContent(output);
    }
    output = F("<div>Volume: {volume} <a href=\"#\" onclick=\"rootAction('volumeUp'); return false;\">+</a> / <a href=\"#\" onclick=\"rootAction('volumeDown'); return false;\">-</a></div>"
      "<form onsubmit=\"playHttp(); return false;\"><input type=\"text\" name=\"streamUrl\" id=\"streamUrl\"><input type=\"button\" value=\"stream\" onclick=\"playHttp(); return false;\"></form>"
      "<form onsubmit=\"mkdir(); return false;\"><input type=\"text\" name=\"folder\" id=\"folder\"><input type=\"button\" value=\"mkdir\" onclick=\"mkdir(); return false;\"></form>");
    output.replace("{volume}", String(50-volume));
    output.replace("{folder}", path);
    server.sendContent(output);
  } else {
    output = F("<form><input type=\"file\" multiple=\"true\" name=\"data\" id=\"fileInput\"><input type=\"button\" value=\"upload\" onclick=\"upload('{folder}'); return false;\"></form>");
    output.replace("{folder}", path);
    server.sendContent(output);
    server.sendContent(F("<div><a href=\"..\">..</a></div>"));
  }

  SdFile entry;
  while (entry.openNext(&dir, O_READ)) {
    if (!entry.isDir()) {
      entry.close();
      continue;
    }

    char filenameChar[100];
    entry.getName(filenameChar, 100);
    // TODO encode special characters
    String filename = String(filenameChar);

    output = F("<div id=\"{name}\">{icon}<a href=\"{path}\">{name}</a> <a href=\"#\" onclick=\"deleteUrl('{name}'); return false;\" title=\"delete\">&#x2718;</a>");
    // Currently only foldernames <= 16 characters can be written onto the rfid
    if (filename.length() <= 16) {
      output += F("<a href=\"#\" onclick=\"writeRfid('{name}');\" title=\"write to card\">&#x1f4be;</a> "
        "<a href=\"#\" onclick=\"play('{name}'); return false;\" title=\"play folder\">&#9654;</a>");
    }
    output.replace("{icon}", F("&#x1f4c2; "));
    output.replace("{path}", filename + "/");
    output += F("</div>");
    output.replace("{name}", filename);
    server.sendContent(output);
    entry.close();
  }
  dir.rewind();
  while (entry.openNext(&dir, O_READ)) {
    if (entry.isDir()) {
      entry.close();
      continue;
    }
    char filenameChar[100];
    entry.getName(filenameChar, 100);
    Serial.println(filenameChar);
    // TODO encode special characters
    String filename = String(filenameChar);

    output = F("<div id=\"{name}\">{icon}<a href=\"{path}\">{name}</a> <a href=\"#\" onclick=\"deleteUrl('{name}'); return false;\" title=\"delete\">&#x2718;</a>");
    if (isMP3File(filename)) {
      output.replace("{icon}", F("&#x266b; "));
    } else {
      output.replace("{icon}", F(""));
    }
    output.replace("{path}", filename);
    output += F("</div>");
    output.replace("{name}", filename);
    server.sendContent(output);
    entry.close();
  }
  if (path == "/") {
    output = F("<br><br><form>Version {major}.{minor} <input type=\"button\" value=\"Update Firmware\" onclick=\"ota(); return false;\"</form>");
    output.replace("{major}", String(MAJOR_VERSION));
    output.replace("{minor}", String(MINOR_VERSION));
    server.sendContent(output);
  }
  server.sendContent(F("</body></html>"));
}

bool loadFromSdCard(String &path) {
  String dataType = "application/octet-stream";

  if (path.endsWith(".HTM")) dataType = "text/html";
  else if (path.endsWith(".CSS")) dataType = "text/css";
  else if (path.endsWith(".JS")) dataType = "application/javascript";

  File dataFile = SD.open(path.c_str());

  if (!dataFile) {
    Serial.println("File not open");
    return false;
  }

  if (dataFile.isDir()) {
    // dataFile.name() will always be "/" for directorys, so we cannot know if we are in the root directory without handing it over
    renderDirectory(path);
  } else {
    if (server.streamFile(dataFile, dataType) != dataFile.size()) {
      Serial.println("Sent less data than expected!");
    }
  }
  dataFile.close();
  return true;
}

void handleWriteRfid(String &folder) {
  if (switchFolder((char *)folder.c_str())) {
    stopPlayback();
    pairing = true;
    returnOK();
  } else {
    returnHttpStatus((uint8_t)404, "Not found");
  }
}

void handleFileUpload() {
  // Upload always happens on /
  if (server.uri() != "/") {
    Serial.println(F("Invalid upload URI"));
    return;
  }

  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;

    if (!isMP3File(filename)) {
      Serial.print(F("Not a MP3: "));
      Serial.println(filename);
      return;
    }

    if (!filename.startsWith("/")) {
      Serial.println(F("Invalid upload target"));
      return;
    }

    if (SD.exists((char *)filename.c_str())) {
      Serial.println("File " + filename + " already exists. Skipping");
      return;
    }

    uploadFile.open((char *)filename.c_str(), FILE_WRITE);
    Serial.print(F("UPLOAD_FILE_START: "));
    Serial.println(filename);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile.isOpen()) {
      uploadFile.write(upload.buf, upload.currentSize);
      Serial.print(F("UPLOAD_FILE_WRITE: "));
      Serial.println(upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile.isOpen()) {
      uploadFile.close();
      Serial.print(F("UPLOAD_FILE_END: "));
      Serial.println(upload.totalSize);
    }
  }
}

void handleNotFound() {
  String path = server.urlDecode(server.uri());
  Serial.println(F("Request to: ") + path);
  if (server.method() == HTTP_GET) {
    if (loadFromSdCard(path)) return;
  } else if (server.method() == HTTP_DELETE) {
    if (server.uri() == "/" || !SD.exists(path.c_str())) {
      returnHttpStatus((uint8_t)500, "BAD PATH: " + server.uri());
      return;
    }

    SdFile file;
    file.open(path.c_str());
    if (file.isDir()) {
      if(!file.rmRfStar()) {
        Serial.println(F("Could not delete folder"));
      }
    } else {
      if(!SD.remove(path.c_str())) {
        Serial.println(F("Could not delete file"));
      }
    }
    returnOK();
    return;
  } else if (server.method() == HTTP_POST) {
    if (server.hasArg("newFolder")) {
      Serial.print(F("Creating folder "));
      Serial.println(server.arg("newFolder"));
      stopPlayback();
      yield();
      switchFolder("/");
      SD.mkdir((char *)server.arg("newFolder").c_str());
      returnOK();
      return;
    } else if (server.hasArg("ota")) {
      Serial.println(F("Starting OTA"));
      t_httpUpdate_return ret = ESPhttpUpdate.update("http://download.naeveke.de/board/latest.bin");

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
    } else if (server.hasArg("stop")) {
      stopPlayback();
      returnOK();
      return;
    } else if (server.hasArg("volumeUp")) {
      volumeUp();
      returnOK();
      return;
    } else if (server.hasArg("volumeDown")) {
      volumeDown();
      returnOK();
      return;
    } else if (server.hasArg("streamUrl")) {
      stopPlayback();
      currentStreamUrl = server.arg("streamUrl");
      playHttp();
      playingByCard = false;
      returnOK();
      return;
    } else if (server.uri() == "/") {
      Serial.println(F("Probably got an upload request"));
      returnOK();
      return;
    } else if (SD.exists((char *)path.c_str())) {
      if (server.hasArg("write") && path.length() <= 16) {
        handleWriteRfid(path);
        returnOK();
        return;
      } else if (server.hasArg("play") && switchFolder((char *)path.c_str())) {
        stopPlayback();
        playFile();
        playingByCard = false;
        returnOK();
        return;
      }
    }
  }

  // 404 otherwise
  returnHttpStatus((uint8_t)404, "Not found");
  Serial.println("404: " + path);
}

bool patchVS1053() {
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
        musicPlayer.sciWrite(addr, val);
      }
    } else {
      while (n--) {
        if (!file.read(&val, 2)) {
          file.close();
          return false;
        }
        i++;
        musicPlayer.sciWrite(addr, val);
      }
    }
  }
  file.close();

  Serial.print(F("Number of bytes: ")); Serial.println(i);
  return true;
}
