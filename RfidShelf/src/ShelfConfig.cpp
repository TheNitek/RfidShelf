#include "ShelfConfig.h"

void ShelfConfig::init() {
  sprintf_P(hostname, PSTR("SHELF_%06X"), ESP.getChipId());
  defaultRepeat = true;
  defaultShuffle = false;
  defaultStopOnRemove = true;
  strncpy_P(timezone, TZ_Europe_Berlin, sizeof(timezone));
  strncpy_P(ntpServer, PSTR("pool.ntp.org"), sizeof(ntpServer));

  Sprintln(F("Loaded config"));
}
