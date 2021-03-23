# Firmware Installation
You can flash the software using Arduino IDE or PlatformIO.

## Using Arduino IDE

1. Install [Arduino IDE](https://www.arduino.cc/en/Main/Software)
1. Install [ESP8266 Support](https://github.com/esp8266/Arduino#installing-with-boards-manager)
1. Install the following libraries using the library manager:
 * MFRC522
 * WifiManager
 * BoolArray
 * Adafruit_VS1053_Library
 * EasyButton
1. Install [Espalexa](https://github.com/Aircoookie/Espalexa)
1. Open the RfidShelf.ino sketch and flash it into the ESP8266
1. Watch the serial console during boot up


## Using PlatformIO
1. Install [PlatformIO Core](https://platformio.org/install/cli)
1. Run "platformio run -t upload" in the RfidShelf folder

## Install VS1053 patches (optional)
* Download [VS1053 patches](https://github.com/sparkfun/LilyPad_MP3_Player/blob/master/Arduino/libraries/SFEMP3Shield/plugins/patches.053) and put it in the root folder of your SD card. It allows the MP3 decoder to do mono playback.

## Setup
After the firmware is flashed, the shelf will create a WiFi hotspot called "MP3-SHELF-SETUP". You can connect using the password "lacklack". In the captive portal (or on http://192.168.4.1/) you can configure your WiFi.

*Note*: Every time the shelf cannot find the configured hotspot during boot up, it will reopen this configuration WiFi.

Now you can open the shelf's web interface. For every card, you need to create a folder. Put as many MP3 files in each folder as you want. There is no need to create/upload through the web interface, just fill the SD card up before installing it into the shelf. Hit the floppy disc icon next to the folder to start the pairing mode. Place a card on the shelf afterwards. It will start playing this folder to let you know it is successfully paired. Once this happens this card will play this folder, every time it is placed on the shelf again. The current playback volume will also be stored on the card during pairing (and only then).

## Notes
* The name of the folder (and the volume) are stored on the card itself. So you can use the same card on different shelfs if the folder names are the same
* Pairing mode will overwrite cards that are already paired
* Non-MP3 files on the SD card will be ignored
* You can start/stop playback and control the volume via the web interface. The next card placed on the shelf will overwrite your commands
