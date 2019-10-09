#ifndef ShelfPins_h
#define ShelfPins_h

#define RC522_CS        D8
#define SD_CS           D2

#define BREAKOUT_RESET  -1    // VS1053 reset pin (output)
#define BREAKOUT_CS     D3    // VS1053 chip select pin (output)
#define BREAKOUT_DCS    D0    // VS1053 Data/command select pin (output)
#define DREQ            D1    // VS1053 Data request (input)

#define AMP_POWER       D4

#define PAUSE_BTN       D10   // GPIO1 = TX
#define SKIP_BTN        D9    // GPIO3 = RX
#define VOLUME          A0

#endif // ShelfPins_h