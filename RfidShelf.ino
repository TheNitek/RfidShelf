/**
   Put this file https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WebServer/examples/SDWebServer/SdRoot/edit/index.htm
   in the root folder of the SD card for webinterface
*/

#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_VS1053.h>
#include <SdFat.h>

#define RC522_CS_PIN          D3
#define SD_CS_PIN             D2

#define BREAKOUT_RESET  D4     // VS1053 reset pin (output)
#define BREAKOUT_CS     D8     // VS1053 chip select pin (output)
#define BREAKOUT_DCS    D0    // VS1053 Data/command select pin (output)
#define DREQ            D1    // VS1053 Data request, ideally an Interrupt pin

Adafruit_VS1053_FilePlayer musicPlayer =
  Adafruit_VS1053_FilePlayer(BREAKOUT_RESET, BREAKOUT_CS, BREAKOUT_DCS, DREQ, SD_CS_PIN);

WiFiManager wifiManager;

ESP8266WebServer server(80);

MFRC522 mfrc522(RC522_CS_PIN, UINT8_MAX);   // Create MFRC522 instance.

MFRC522::MIFARE_Key key;


// Init array that will store new NUID
byte nuidPICC[4];

boolean playing = false;

SdFat SD;

SdFile uploadFile;

void setup() {
  Serial.begin(115200);
  Serial.println();

  // Init SPI SS pins
  pinMode(RC522_CS_PIN, OUTPUT);
  digitalWrite(RC522_CS_PIN, HIGH);
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  pinMode(BREAKOUT_CS, OUTPUT);
  digitalWrite(BREAKOUT_CS, HIGH);
  pinMode(BREAKOUT_DCS, OUTPUT);
  digitalWrite(BREAKOUT_DCS, HIGH);

  SPI.begin();        // Init SPI bus
  mfrc522.PCD_Init(); // Init MFRC522 card

  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  Serial.print(F("Using rfid key (for A and B):"));
  print_byte_array(key.keyByte, MFRC522::MF_KEY_SIZE);
  Serial.println();

  //Initialize the SdCard.
  if (!SD.begin(SD_CS_PIN)) SD.initErrorHalt();

  // initialise the music player
  if (! musicPlayer.begin()) { // initialise the music player
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

  Serial.print(F("SampleRate "));
  Serial.println(musicPlayer.sciRead(VS1053_REG_AUDATA));

  musicPlayer.setVolume(40, 40);

  wifiManager.autoConnect("MP3-SHELF", "lacklack");

  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/playMp3", HTTP_GET, handlePlayMp3);
  server.on("/stopMp3", HTTP_GET, handleStopMp3);
  server.on("/", HTTP_POST, handleNotFound, handleFileUpload);
  server.onNotFound(handleNotFound);

  server.begin();

  Serial.println(F("Init done"));
}

void loop() {

  if (playing && musicPlayer.playingMusic) {
    musicPlayer.feedBuffer();
  } else if (playing) {
    playMp3();
  }

  handleRfid();
  server.handleClient();
}

void handleRfid() {
  // Look for new cards
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  if (mfrc522.uid.uidByte[0] == nuidPICC[0] &&
      mfrc522.uid.uidByte[1] == nuidPICC[1] &&
      mfrc522.uid.uidByte[2] == nuidPICC[2] &&
      mfrc522.uid.uidByte[3] == nuidPICC[3] ) {
    mfrc522.PICC_HaltA();
    Serial.println(F("Card already read"));
    return;
  }

  // Show some details of the PICC (that is: the tag/card)
  Serial.print(F("Card UID:"));
  print_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
  Serial.println();
  Serial.print(F("PICC type: "));
  MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
  Serial.println(mfrc522.PICC_GetTypeName(piccType));

  // Check for compatibility
  if ( piccType != MFRC522::PICC_TYPE_MIFARE_1K ) {
    Serial.println(F("Unsupported card."));
    return;
  }

  char folder[17];
  if (readRfidBlock(1, 0, folder)) {

    // Store NUID into nuidPICC array
    for (byte i = 0; i < 4; i++) {
      nuidPICC[i] = mfrc522.uid.uidByte[i];
    }

    switchFolder(folder);
    stopMp3();
    yield();
    playMp3();
  }

}

boolean readRfidBlock(uint8_t sector, uint8_t relativeBlock, char *outputBuffer) {
  if (relativeBlock > 3) {
    Serial.println(F("Invalid block number"));
    return false;
  }

  // Block 4 is trailer block
  byte trailerBlock   = (sector * 4) + 3;
  byte absoluteBlock = (sector * 4) + relativeBlock;
  MFRC522::StatusCode status;

  // Authenticate using key A
  Serial.println(F("Authenticating using key A..."));
  status = (MFRC522::StatusCode) mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("PCD_Authenticate() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }

  Serial.print(F("Reading block "));
  Serial.println(absoluteBlock);
  byte buffer[18];
  byte bufferSize = sizeof(buffer);
  status = (MFRC522::StatusCode) mfrc522.MIFARE_Read(absoluteBlock, buffer, &bufferSize);
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("MIFARE_Read() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }
  Serial.print(F("Data in block ")); Serial.print(absoluteBlock); Serial.println(F(":"));
  print_byte_array(buffer, 16);
  Serial.println();
  Serial.println();


  // Halt PICC
  mfrc522.PICC_HaltA();
  // Stop encryption on PCD
  mfrc522.PCD_StopCrypto1();

  for (byte i = 0; i < 16; i++) {
    outputBuffer[i] = buffer[i];
  }
  outputBuffer[bufferSize] = '\0';

  return true;
}


void print_byte_array(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

void switchFolder(const char *folder) {
  Serial.print(F("Switching folder to "));
  Serial.println(folder);

  if (!SD.exists(folder)) {
    Serial.println(F("Folder does not exist"));
    return;
  }
  SD.chdir(folder);
  SD.vwd()->rewind();
}

void stopMp3() {
  playing = false;
  musicPlayer.stopPlaying();
}

void playMp3() {
  SdFile file;
  while (file.openNext(SD.vwd(), O_READ))
  {
    char filenameChar[100];
    String filename = "/";

    SD.vwd()->getName(filenameChar, 100);
    filename += filenameChar;

    filename += "/";
    file.getName(filenameChar, 100);
    filename += filenameChar;

    file.close();

    if (!file.isDir() && isMP3File(filename)) {
      Serial.print(F("Playing "));
      Serial.println(filename);

      playing = true;
      musicPlayer.startPlayingFile((char *)filename.c_str());
      return;
    } else {
      Serial.print(F("Ignoring "));
      Serial.println(filename);
    }
  }
  // Start again
  SD.vwd()->rewind();
}

bool isMP3File(String filename) {
  return filename.endsWith(".mp3");
}

void returnOK() {
  server.send(200, "text/plain", "");
}

void returnServerError(String msg) {
  server.send(500, "text/plain", msg);
}

void renderDirectory(String path) {
  SdFile dir;
  dir.open(path.c_str(), O_READ);

  dir.rewind();
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  WiFiClient client = server.client();

  server.sendContent(
    F("<html><head><script>"
      "function deleteUrl(url){ var xhr = new XMLHttpRequest(); xhr.onreadystatechange = function() { if (xhr.readyState === 4) { location.reload(); }}; xhr.open('DELETE', url); xhr.send();}"
      "function upload(folder){ var fileInput = document.getElementById('fileInput'); if(fileInput.files.length === 0){ alert('Choose a file first'); return; } xhr = new XMLHttpRequest(); xhr.onreadystatechange = function() { if (xhr.readyState === 4) { location.reload(); }};"
      "var formData = new FormData(); for(var i = 0; i < fileInput.files.length; i++) { formData.append('data', fileInput.files[i], folder.concat(fileInput.files[i].name)); }; xhr.open('POST', '/'); xhr.send(formData); }"
      "function mkdir() { var folder = document.getElementById('folder'); if(folder != ''){ var xhr = new XMLHttpRequest(); xhr.onreadystatechange = function() { if (xhr.readyState === 4) { location.reload(); }}; var formData = new FormData(); formData.append('folder', folder.value); xhr.open('POST', '/'); xhr.send(formData);}}"
      "</script></head><body>"));

  String output;
  if (path == "/") {
    output = F("<form><input type=\"text\" name=\"folder\" id=\"folder\"><input type=\"button\" value=\"mkdir\" onclick=\"mkdir()\"></form>");
    output.replace("{folder}", path);
    server.sendContent(output);
  } else {
    output = F("<form><input type=\"file\" multiple=\"true\" name=\"data\" id=\"fileInput\"><input type=\"button\" value=\"upload\" onclick=\"upload('{folder}')\"></form>");
    output.replace("{folder}", path);
    server.sendContent(output);
    server.sendContent(F("<div><a href=\"..\">..</a></div>"));
  }

  SdFile entry;
  while (entry.openNext(&dir, O_READ)) {
    char filenameChar[100];
    entry.getName(filenameChar, 100);
    // TODO encode special characters
    String filename = String(filenameChar);

    String output = F("<div id=\"{name}\">{icon}<a href=\"{path}\">{name}</a> <a href=\"#\" onclick=\"deleteUrl('{name}'); return false;\">&#x2718;</a>");
    if (entry.isDir()) {
      //output += F("<a href=\"#\" onclick=\"\">&#x1f4be;</a>");
      output.replace("{icon}", F("&#x1f4c2; "));
      output.replace("{path}", filename + "/");
    } else {
      if (isMP3File(filename)) {
        output.replace("{icon}", F("&#x266b; "));
      } else {
        output.replace("{icon}", F(""));
      }
      output.replace("{path}", filename);
    }
    output += F("</div>");
    output.replace("{name}", filename);
    server.sendContent(output);
    entry.close();
  }
  server.sendContent(F("</body></html>"));
  server.client().stop();
}

bool loadFromSdCard(String path) {
  String dataType = "text/plain";

  if (path.endsWith(".HTM")) dataType = "text/html";
  else if (path.endsWith(".CSS")) dataType = "text/css";
  else if (path.endsWith(".JS")) dataType = "application/javascript";
  else dataType = "application/octet-stream";

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

void handlePlayMp3() {
  playMp3();
  returnOK();
}

void handleStopMp3() {
  stopMp3();
  returnOK();
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
  if (server.method() == HTTP_GET) {
    if (loadFromSdCard(path)) return;
  } else if (server.method() == HTTP_DELETE) {
    if (server.uri() == "/" || !SD.exists((char *)path.c_str())) {
      returnServerError("BAD PATH: " + server.uri());
      return;
    }

    SdFile file;
    file.open(path.c_str());
    if (file.isDir()) {
      file.rmRfStar();
    } else {
      file.remove();
    }
    returnOK();
    return;
  } else if (server.method() == HTTP_POST) {
    if (server.hasArg("folder")) {
      Serial.println(F("Creating folder"));
      SD.mkdir((char *)server.arg("folder").c_str());
      returnOK();
      return;
    } else if ((server.uri() == "/") || SD.exists((char *)path.c_str())) {
      Serial.println(F("Probably got an upload request"));
      returnOK();
      return;
    }
    // 404 otherwise
  }

  server.send(404, "text/plain", F("Not found"));
  Serial.println("404: " + path);
}
