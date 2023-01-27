#include "ShelfConfig.h"

bool ShelfConfig::GlobalConfig::load() {
  sprintf_P(hostname, PSTR("SHELF_%06X"), ESP.getChipId());
  defaultRepeat = true;
  defaultShuffle = false;
  defaultStopOnRemove = true;
  strncpy_P(timezone, TZ_Europe_Berlin, sizeof(timezone));
  strncpy_P(ntpServer, PSTR("pool.ntp.org"), sizeof(ntpServer));

  Sprintln(F("Loaded config"));
  return true;
}


bool ShelfConfig::PodcastConfig::load(const char* podFilename) {
  File32 podfile = _SD.open(podFilename, O_READ);
  if(!podfile.isOpen()) {
    Sprintf("Could not open %s\n", podFilename);
    return false;
  }

  char buffer[201] = {0};

  if(podfile.fgets(buffer, sizeof(buffer)-1) < 8) {
    Sprintln(F("Could not read podcast URL"));
    podfile.close();
    return false;
  }
  buffer[strcspn(buffer, "\n\r")] = '\0';
  feedUrl = buffer;

  memset(buffer, 0, sizeof(buffer));
  if(podfile.fgets(buffer, sizeof(buffer)-1) < 1) {
    Sprintln(F("Could not read max episode count"));
    podfile.close();
    return false;
  }
  maxEpisodes = atoi(buffer);

  memset(buffer, 0, sizeof(buffer));
  if(podfile.fgets(buffer, sizeof(buffer)-1) < 1) {
    Sprintln(F("Could not read last guid"));
    podfile.close();
    return false;
  }
  buffer[strcspn(buffer, "\n\r")] = '\0';
  lastGuid = buffer;

  memset(buffer, 0, sizeof(buffer));
  if(podfile.fgets(buffer, sizeof(buffer)-1) < 1) {
    Sprintln(F("Could not read last file no"));
    podfile.close();
    return false;
  }
  lastFileNo = atoi(buffer);

  memset(buffer, 0, sizeof(buffer));
  if(podfile.fgets(buffer, sizeof(buffer)-1) < 1) {
    Sprintln(F("Could not read enabled flag. Keeping default"));
  } else {
    enabled = (buffer[0] == '1');
  }

  podfile.close();
  return true;
}

bool ShelfConfig::PodcastConfig::save(const char* podFilename) {
  // This writes to the SD card more often then needed, but prevents re-downloading of episodes in case of a crash.
  Sprintln(F("Updating .podcast"));
  File32 podfile = _SD.open(podFilename, O_WRITE | O_TRUNC | O_CREAT);
  if(!podfile.isOpen()) {
    Sprintf("Could not open %s\n", podFilename);
    return false;
  }
  podfile.println(feedUrl);
  podfile.println(maxEpisodes);
  podfile.println(lastGuid);
  podfile.println(lastFileNo);
  podfile.println(enabled ? "1" : "0");
  podfile.close();
  return true;
}
