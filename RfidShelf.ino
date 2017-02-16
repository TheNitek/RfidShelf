/**

*/

#include <SPI.h>
#include <MFRC522.h>

//Add the SdFat Libraries
#include <SdFat.h>
#include <SdFatUtil.h>

/*
   For the RFID to work while MP3 plays, you have to change
   #define USE_MP3_REFILL_MEANS USE_MP3_INTx
   to
   #define USE_MP3_REFILL_MEANS USE_MP3_Polled
   in SFEMP3Shield.h
*/
#include <SFEMP3Shield.h>

#define RST_PIN         5           // Configurable, see typical pin layout above
#define SS_PIN          10          // Configurable, see typical pin layout above

MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance.

MFRC522::MIFARE_Key key;

// Init array that will store new NUID
byte nuidPICC[4];


// Below is not needed if interrupt driven. Safe to remove if not using.
#if defined(USE_MP3_REFILL_MEANS) && USE_MP3_REFILL_MEANS == USE_MP3_Timer1
#include <TimerOne.h>
#elif defined(USE_MP3_REFILL_MEANS) && USE_MP3_REFILL_MEANS == USE_MP3_SimpleTimer
#include <SimpleTimer.h>
#endif

SdFat sd;

SFEMP3Shield MP3player;

void setup() {
  Serial.begin(115200);
  while (!Serial);    // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)

  //Initialize the SdCard.
  if (!sd.begin(SD_SEL, SPI_FULL_SPEED)) sd.initErrorHalt();
  // depending upon your SdCard environment, SPI_HAVE_SPEED may work better.
  if (!sd.chdir("/")) sd.errorHalt("sd.chdir");

  //Initialize the MP3 Player Shield
  uint8_t result = MP3player.begin();
  //check result, see readme for error codes.
  if (result != 0) {
    Serial.print(F("Error code: "));
    Serial.print(result);
    Serial.println(F(" when trying to start MP3 player"));
    if ( result == 6 ) {
      Serial.println(F("Warning: patch file not found, skipping.")); // can be removed for space, if needed.
      Serial.println(F("Use the \"d\" command to verify SdCard can be read")); // can be removed for space, if needed.
    }
  }

  SPI.begin();        // Init SPI bus
  mfrc522.PCD_Init(); // Init MFRC522 card

  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  Serial.print(F("Using rfid key (for A and B):"));
  dump_byte_array(key.keyByte, MFRC522::MF_KEY_SIZE);
  Serial.println();
}

/**
   Main loop.
*/
void loop() {
#if defined(USE_MP3_REFILL_MEANS) \
      && ( (USE_MP3_REFILL_MEANS == USE_MP3_SimpleTimer) \
      ||   (USE_MP3_REFILL_MEANS == USE_MP3_Polled)      )

  MP3player.available();
#endif

  // Look for new cards
  if ( ! mfrc522.PICC_IsNewCardPresent())
    return;

  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial())
    return;

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

  if (mfrc522.uid.uidByte[0] == nuidPICC[0] &&
      mfrc522.uid.uidByte[1] == nuidPICC[1] &&
      mfrc522.uid.uidByte[2] == nuidPICC[2] &&
      mfrc522.uid.uidByte[3] == nuidPICC[3] ) {
    Serial.println(F("Card already read"));
    return;
  }

  // Store NUID into nuidPICC array
  for (byte i = 0; i < 4; i++) {
    nuidPICC[i] = mfrc522.uid.uidByte[i];
  }

  char folder[17];
  readBlock(1, 0, folder);
  Serial.print(F("Folder on rfid: "));
  Serial.println(folder);
  playFolder(folder);

  delay(100);
}

void readBlock(uint8_t sector, uint8_t relativeBlock, char *outputBuffer) {
  if (relativeBlock > 3) {
    Serial.println(F("Invalid block number"));
    return;
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
    return;
  }

  // Read data from the block
  Serial.print(F("Reading data from block "));
  Serial.print(absoluteBlock);
  Serial.println(F(" ..."));
  byte buffer[18];
  byte bufferSize = sizeof(buffer);
  status = (MFRC522::StatusCode) mfrc522.MIFARE_Read(absoluteBlock, buffer, &bufferSize);
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("MIFARE_Read() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
  }
  Serial.print(F("Data in block ")); Serial.print(absoluteBlock); Serial.println(F(":"));
  dump_byte_array(buffer, 16);
  Serial.println();
  Serial.println();


  // Halt PICC
  mfrc522.PICC_HaltA();
  // Stop encryption on PCD
  mfrc522.PCD_StopCrypto1();

  for (byte i = 0; i < bufferSize; i++) {
    outputBuffer[i] = buffer[i];
  }
  outputBuffer[bufferSize] = '\0';

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

void playFolder(char *folder) {
  Serial.println(F("Music Files found:"));
  SdFile file;
  char filename[20];
  sd.chdir(folder, true);
  while (file.openNext(sd.vwd(), O_READ))
  {
    file.getFilename(filename);
    if ( isFnMusic(filename) ) {
      MP3player.stopTrack();
      uint8_t result = MP3player.playMP3(filename);

      //check result, see readme for error codes.
      if (result != 0) {
        Serial.print(F("Error code: "));
        Serial.print(result);
        Serial.println(F(" when trying to play track"));
      } else {

        Serial.println(F("Playing:"));

        char title[30]; // buffer to contain the extract the Title from the current filehandles
        char artist[30]; // buffer to contain the extract the artist name from the current filehandles
        char album[30]; // buffer to contain the extract the album name from the current filehandles

        //we can get track info by using the following functions and arguments
        //the functions will extract the requested information, and put it in the array we pass in
        MP3player.trackTitle((char*)&title);
        MP3player.trackArtist((char*)&artist);
        MP3player.trackAlbum((char*)&album);

        //print out the arrays of track information
        Serial.write((byte*)&title, 30);
        Serial.println();
        Serial.print(F("by:  "));
        Serial.write((byte*)&artist, 30);
        Serial.println();
        Serial.print(F("Album:  "));
        Serial.write((byte*)&album, 30);
        Serial.println();
      }
      file.close();
      return;
    } else {
      Serial.print(F("Not a music file: "));
      Serial.println(filename);
    }
    file.close();
  }

}

