# RfidShelf
Make your shelf play MP3s!

Use some cheap hardware (Esp8266/NodeMcu, RC522, VS1053 MP3 breakout board) to create an RFID controlled MP3 (and more) shelf. Impression of the first prototype: [![RfidShelf Prototype](http://img.youtube.com/vi/39uuoB3o7k8/0.jpg)](http://www.youtube.com/watch?v=39uuoB3o7k8 "RfidShelf Prototype")

This is how the first "test subject" reacted:
[![RfidShelf Test](http://img.youtube.com/vi/FcALmyrhR3w/0.jpg)](http://www.youtube.com/watch?v=FcALmyrhR3w "RfidShelf Test")

## The idea
When I was a little child I loved my tapes, vinyls and later CDs. Choosing my own music or listening to Pumuckl all day. Nowadays everything is available as MP3 which makes it hard for little kids to choose their own music. Since I wanted to give my son the same possibilities I decided to start this project:

Rfid-cards can the be assigned a folder containing MP3 files on a SD card, played when the card is put on the shelf. Using some creativity (and/or the Internet and a printer) those cards can be customized to represent their content, giving the kid the ability to distinguish them. A WiFi web interface gives you the ability to manage the files on the shelf and to program the cards.

## Features
* Easy to use: kids proof!
* Safe: No wiring on the outside, just a USB cable going in
* Cheap: ~ 30€ depending on the parts you choose
* Easy to managed: Web interface to configure everything needed

## Hardware
I tried a variety of hardware combinations both focusing on being cheap and easy to assemble. Based on that I can recommend a few setups, depending on your budget and your ability/willingness to solder. Most items can be bought on Aliexpress for a few bucks, the links are only meant to be an example.

### Common for all:
* [Shelf (All Ikea Lack Shelfs and even Tables should work, but I only tried the one linked) ~6€](http://www.ikea.com/de/de/catalog/products/50282177/)
* [NodeMcu (make sure to get correct version, because the vary in size!) ~2,50€](https://www.aliexpress.com/item/V3-Wireless-module-NodeMcu-4M-bytes-Lua-WIFI-Internet-of-Things-development-board-based-ESP8266-for/32554198757.html)
* [MFRC-522 Rfid Reader ~1,50€](https://www.aliexpress.com/item/Free-shipping-MFRC-522-RC522-RFID-RF-IC-card-sensor-module-to-send-S50-Fudan-card/1623810751.html)
* [3W 8Ohm In-Ceiling Speaker ~6,50€](http://www.ebay.de/itm/112275116606?_trksid=p2057872.m2749.l2649&ssPageName=STRK%3AMEBIDX%3AIT)
* [USB Connector ~1,50€](https://www.aliexpress.com/item/D-type-aluminum-USB-3-0-female-to-female-connector/32608847792.html)
* [Rfid Cards ~0,25€ each](http://www.ebay.de/itm/10pcs-NFC-thin-smart-card-tag-1k-S50-IC-13-56MHz-Read-Write-RFID-/172309355607?hash=item281e701c57)
* Micro USB Cable + USB Power Supply (Just use one from your old smart phone)
* Jumper Wires

### Cheap (~30€)
Disclaimer: I had some noise issues with this version, probably due to all the wires flying around
![RfidBoardBaseplate](images/baseplate.jpg)
* [Base plate for NodeMcu (also available as a kit including the NodeMcu) ~1,50](https://www.aliexpress.com/item/Nodemcu-base-plate-Lua-WIFI-NodeMcu-development-board-ESP8266-serial-port/32678372845.html)
* [VS1053 MP3 + SD Board ~6,50€](https://www.aliexpress.com/item/VS1053-VS1053B-MP3-Module-Breakout-Board-With-SD-Card-Slot-VS1053B-Ogg-Real-time-Recording-For/32809994212.html)
* [PAM8302 Amplifier ~1,00€](https://www.aliexpress.com/item/CJMCU-832-PAM8302-2-5W-single-channel-Class-D-Audio-power-amplifier-module-PAM8302A-development-board/32708571731.html)
* SD Card (NOT a micro SD but a big one!)

### ShelfBoard based (~40€)
Same as the "cheap" version but instead of the base plate use the custom [ShelfBoard (~10,00€)](https://PCBs.io/share/z7aNg).
![RfidBoardTop](images/top.jpg)
![RfidBoardBottom](images/bottom.jpg)

### Easy (~45€)
* [Base plate for NodeMcu (also available as a kit including the NodeMcu) ~1,50](https://www.aliexpress.com/item/Nodemcu-base-plate-Lua-WIFI-NodeMcu-development-board-ESP8266-serial-port/32678372845.html)
* [Music Maker FeatherWing w/ Amp ~30,00€](https://www.adafruit.com/product/3436)
* A MicroSD Card
![RfidBoardAdafruit](images/adafruit.jpg)


## More Pictures
![Shelf](images/shelf.jpg)
![Shelf with speaker](images/shelfspeaker.jpg)

## Disclaimer
Everything in this project I did up to my best knowledge. Nevertheless this comes without guarantee. Do not hold me responsible in case something unexpected/undesired happens.
