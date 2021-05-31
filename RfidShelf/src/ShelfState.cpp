#include "ShelfState.h"

void ShelfState::storeState(ShelfState_t state) {
  ShelfStateEnvelope_t env;
  env.state = state;
  // Update CRC32 of data
  env.crc32 = _calculateCRC32((uint8_t*) &(env.state), sizeof(env.state));
  // Write struct to RTC memory
  if(!ESP.rtcUserMemoryWrite(0, (uint32_t*) &env, sizeof(env))) {
    Sprintln(F("Failed to write state to RTC memory"));
  }
}

bool ShelfState::loadState(ShelfState_t &state) {
  ShelfStateEnvelope_t env;
  if (ESP.rtcUserMemoryRead(0, (uint32_t *) &env, sizeof(env))) {
    uint32_t crcOfData = _calculateCRC32((uint8_t*) &(env.state), sizeof(env.state));
  #ifdef DEBUG_ENABLE
    Serial.print(F("CRC32 of data: "));
    Serial.println(crcOfData, HEX);
    Serial.print(F("CRC32 read from RTC: "));
    Serial.println(env.crc32, HEX);
  #endif
    if (crcOfData != env.crc32) {
      Sprintln(F("CRC32 mismatch. State invalid!"));
      return false;
    } else {
      Sprintln(F("CRC32 match"));
      memcpy(&state, &(env.state), sizeof(env.state));
      return true;
    }
  }
  return false;
}

uint32_t ShelfState::_calculateCRC32(const uint8_t *data, size_t length) {
  uint32_t crc = 0xffffffff;
  while (length--) {
    uint8_t c = *data++;
    for (uint32_t i = 0x80; i > 0; i >>= 1) {
      bool bit = crc & 0x80000000;
      if (c & i) {
        bit = !bit;
      }
      crc <<= 1;
      if (bit) {
        crc ^= 0x04c11db7;
      }
    }
  }
  return crc;
}