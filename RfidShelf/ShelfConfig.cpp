#include "ShelfConfig.h"

void ShelfConfig::init() {
  sprintf(hostname, "SHELF_%06X", ESP.getChipId());
  defaultRepeat = true;
  defaultShuffle = false;
  defaultStopOnRemove = true;
  strncpy(timezone, TZ_Europe_Berlin, sizeof(timezone));
  strncpy(ntpServer, "pool.ntp.org", sizeof(ntpServer));

  Sprintln("Loaded config");
}
