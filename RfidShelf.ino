/**
 * Put this file https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WebServer/examples/SDWebServer/SdRoot/edit/index.htm
 * in the root folder of the SD card for webinterface
*/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_VS1053.h>
#include <SD.h>

#define RC522_CS_PIN          D0
#define SD_CS_PIN             D2

#define BREAKOUT_RESET  D4     // VS1053 reset pin (output)
#define BREAKOUT_CS     D8     // VS1053 chip select pin (output)
#define BREAKOUT_DCS    D3    // VS1053 Data/command select pin (output)
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

File currentFolder;

File uploadFile;


void setup() {
  Serial.begin(115200);

  SPI.begin();        // Init SPI bus
  mfrc522.PCD_Init(); // Init MFRC522 card

  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  Serial.print(F("Using rfid key (for A and B):"));
  dump_byte_array(key.keyByte, MFRC522::MF_KEY_SIZE);
  Serial.println();

  //Initialize the SdCard.
  if (!SD.begin(SD_CS_PIN)) Serial.println(F("SD.begin failed"));
  if (!(currentFolder = SD.open("/"))) Serial.println(F("sd.chdir"));
  while (File file = currentFolder.openNextFile())
  {
    if(file.isDirectory()){
      switchFolder(file.name());
      file.close();
    }
  }


  // initialise the music player
  if (! musicPlayer.begin()) { // initialise the music player
    Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
    while(1) delay(500);
  }
  Serial.println(F("VS1053 found"));

  // Fix for the design fuckup of the cheap LC Technology MP3 shield
  // see http://www.bajdi.com/lcsoft-vs1053-mp3-module/#comment-33773
  musicPlayer.sciWrite(VS1053_REG_WRAMADDR, VS1053_GPIO_DDR);
  musicPlayer.sciWrite(VS1053_REG_WRAM, 0x0003);
  musicPlayer.GPIO_digitalWrite(0x0000);
  musicPlayer.softReset();

  Serial.print(F("SampleRate "));
  Serial.println(musicPlayer.sciRead(VS1053_REG_AUDATA));

  musicPlayer.setVolume(40, 40);

  wifiManager.autoConnect("MP3-SHELF", "ikealack");
  
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/playMp3", HTTP_GET, handlePlayMp3);
  server.on("/stopMp3", HTTP_GET, handleStopMp3);
  server.on("/list", HTTP_GET, printDirectory);
  server.on("/edit", HTTP_DELETE, handleDelete);
  server.on("/edit", HTTP_PUT, handleCreate);
  server.on("/edit", HTTP_POST, [](){ returnOK(); }, handleFileUpload);
  server.onNotFound(handleNotFound);

  server.begin();

  Serial.println(F("Init done"));
}

void loop() {
  if (musicPlayer.playingMusic) {
    musicPlayer.feedBuffer();
  } else if(playing) {
    playMp3();
  }

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
  dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
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
  dump_byte_array(buffer, 16);
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

/**
   Helper routine to dump a byte array as hex values to Serial.
*/
void dump_byte_array(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

void switchFolder(const char *folder) {
  // TODO do SD.exists(folder) check here, but it keeps crashing the ESP
  Serial.print(F("Switching folder to "));
  Serial.println(folder);
  currentFolder.close();
  currentFolder = SD.open(folder);
}

void stopMp3() {
    playing = false;
    musicPlayer.stopPlaying();
}

void playMp3() {
  while (File file = currentFolder.openNextFile())
  {

    char filename[30] = "";
    strcat(filename, "/");
    strcat(filename, currentFolder.name());
    strcat(filename, "/");
    strcat(filename, file.name());
    file.close();

    if(!file.isDirectory() && isMP3File(filename)){ 
      Serial.print(F("Playing "));
      Serial.println(filename);

      playing = true;
      musicPlayer.startPlayingFile(filename);
      return;
    } else {
      Serial.print(F("Ignoring "));
      Serial.println(filename);
    }
  }
  // Start again
  currentFolder.rewindDirectory();
}

bool isMP3File(const char* filename) {
  int   numChars;
  char  dotMP3[] = ".MP3";

  if (filename) {
    numChars = strlen(filename);
    if (numChars > 4) {
      byte index = 0;
      for (byte i = numChars - 4; i < numChars; i++) {
        if (!(toupper(filename[i]) == dotMP3[index])) {
          return false;
        }
        index++;
      }
      return true;
    }
  }
  return false;
}

void returnOK() {
  server.send(200, "text/plain", "");
}

void returnFail(String msg) {
  server.send(500, "text/plain", msg + "\r\n");
}

bool loadFromSdCard(String path){
  String dataType = "text/plain";
  if(path.endsWith("/")) path += "index.htm";

  if(path.endsWith(".htm")) dataType = "text/html";
  else if(path.endsWith(".css")) dataType = "text/css";
  else if(path.endsWith(".js")) dataType = "application/javascript";

  File dataFile = SD.open(path.c_str());
  if(dataFile.isDirectory()){
    path += "/index.htm";
    dataType = "text/html";
    dataFile = SD.open(path.c_str());
  }

  if (!dataFile)
    return false;

  if (server.hasArg("download")) dataType = "application/octet-stream";

  if (server.streamFile(dataFile, dataType) != dataFile.size()) {
    Serial.println("Sent less data than expected!");
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
void handleFileUpload(){
  if(server.uri() != "/edit") return;
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    if(SD.exists((char *)upload.filename.c_str())) SD.remove((char *)upload.filename.c_str());
    uploadFile = SD.open(upload.filename.c_str(), FILE_WRITE);
    Serial.print("Upload: START, filename: "); Serial.println(upload.filename);
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(uploadFile) uploadFile.write(upload.buf, upload.currentSize);
    Serial.print("Upload: WRITE, Bytes: "); Serial.println(upload.currentSize);
  } else if(upload.status == UPLOAD_FILE_END){
    if(uploadFile) uploadFile.close();
    Serial.print("Upload: END, Size: "); Serial.println(upload.totalSize);
  }
}

void deleteRecursive(String path){
  File file = SD.open((char *)path.c_str());
  if(!file.isDirectory()){
    file.close();
    SD.remove((char *)path.c_str());
    return;
  }

  file.rewindDirectory();
  while(true) {
    File entry = file.openNextFile();
    if (!entry) break;
    String entryPath = path + "/" +entry.name();
    if(entry.isDirectory()){
      entry.close();
      deleteRecursive(entryPath);
    } else {
      entry.close();
      SD.remove((char *)entryPath.c_str());
    }
    yield();
  }

  SD.rmdir((char *)path.c_str());
  file.close();
}

void handleDelete(){
  if(server.args() == 0) return returnFail("BAD ARGS");
  String path = server.arg(0);
  if(path == "/" || !SD.exists((char *)path.c_str())) {
    returnFail("BAD PATH");
    return;
  }
  deleteRecursive(path);
  returnOK();
}

void handleCreate(){
  if(server.args() == 0) return returnFail("BAD ARGS");
  String path = server.arg(0);
  if(path == "/" || SD.exists((char *)path.c_str())) {
    returnFail("BAD PATH");
    return;
  }

  if(path.indexOf('.') > 0){
    File file = SD.open((char *)path.c_str(), FILE_WRITE);
    if(file){
      file.write((const char *)0);
      file.close();
    }
  } else {
    SD.mkdir((char *)path.c_str());
  }
  returnOK();
}

void printDirectory() {
  if(!server.hasArg("dir")) return returnFail("BAD ARGS");
  String path = server.arg("dir");
  if(path != "/" && !SD.exists((char *)path.c_str())) return returnFail("BAD PATH");
  File dir = SD.open((char *)path.c_str());
  path = String();
  if(!dir.isDirectory()){
    dir.close();
    return returnFail("NOT DIR");
  }
  dir.rewindDirectory();
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/json", "");
  WiFiClient client = server.client();

  server.sendContent("[");
  for (int cnt = 0; true; ++cnt) {
    File entry = dir.openNextFile();
    if (!entry)
    break;

    String output;
    if (cnt > 0)
      output = ',';

    output += "{\"type\":\"";
    output += (entry.isDirectory()) ? "dir" : "file";
    output += "\",\"name\":\"";
    output += entry.name();
    output += "\"";
    output += "}";
    server.sendContent(output);
    entry.close();
 }
 server.sendContent("]");
 dir.close();
}

void handleNotFound(){
  if(loadFromSdCard(server.uri())) return;
  String message = "SDCARD Not Detected\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " NAME:"+server.argName(i) + "\n VALUE:" + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  Serial.print(message);
}
