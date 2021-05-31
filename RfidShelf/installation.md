# Firmware Installation
You can flash the software using Arduino IDE or PlatformIO.

## Using PlatformIO
1. Install [PlatformIO Core](https://platformio.org/install/cli)
1. Run "platformio run -t upload" in the RfidShelf folder

## Setup
After the firmware is flashed, the shelf will create a WiFi hotspot called "MP3-SHELF-SETUP". You can connect using the password "lacklack". In the captive portal (or on http://192.168.4.1/) you can configure your WiFi.

*Note*: Every time the shelf cannot find the configured hotspot during boot up, it will reopen this configuration WiFi.

Now you can open the shelf's web interface. For every card, you need to create a folder. Put as many MP3 files in each folder as you want. There is no need to create/upload through the web interface, just fill the SD card up before installing it into the shelf. Hit the floppy disc icon next to the folder to start the pairing mode. Place a card on the shelf afterwards. It will start playing this folder to let you know it is successfully paired. Once this happens this card will play this folder, every time it is placed on the shelf again. The current playback volume will also be stored on the card during pairing (and only then).

## Notes
* The name of the folder (and the volume) are stored on the card itself. So you can use the same card on different shelfs if the folder names are the same
* Pairing mode will overwrite cards that are already paired
* Non-MP3 files on the SD card will be ignored
* You can start/stop playback and control the volume via the web interface. The next card placed on the shelf will overwrite your commands
