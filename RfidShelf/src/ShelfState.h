#pragma once

#include <Arduino.h>
#include "ShelfPlayback.h"

struct ShelfState_t {
  uint8_t volume;
  char currentFolder[100];
  bool repeat;
  bool shuffle;
  bool stopOnRemove;
  bool playingByCard;
};

struct ShelfStateEnvelope_t {
  uint32_t crc32;
  ShelfState_t state;
};

class ShelfState {
  public:
    void storeState(ShelfState_t state);
    bool loadState(ShelfState_t &state);
  private:
    uint32_t _calculateCRC32(const uint8_t *data, size_t length);
};