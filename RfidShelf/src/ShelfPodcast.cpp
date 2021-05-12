#include "ShelfPodcast.h"

bool ShelfPodcast::_nextPodcast(char *folder) {
  sdfat::SdFile root;
  if(!root.open("/")) {
    Sprintln(F("Could not open root"));
    return false;
  }

  bool lastFolder = false;
  
  sdfat::SdFile entry;
  while(entry.openNext(&root, sdfat::O_READ)) {
    if(!entry.isDir()) {
      entry.close();
      continue;
    }

    char newFolder[13] = {0};
    entry.getSFN(newFolder);

    if(!entry.exists(".podcast")) {
      Sprintf("No .podcast in %s\n", newFolder);
      entry.close();
      continue;
    }

    if(strlen(folder) == 0 || lastFolder) {
      strncpy(folder, newFolder, sizeof(newFolder));
      entry.close();
      root.close();
      return true;
    }
    
    if(strcmp(folder, newFolder) == 0) {
      lastFolder = true;
    }

    entry.close();
  }

  root.close();
  return false;
};

bool ShelfPodcast::_readPodcastFile(PodcastInfo &info, const char* podFilename) {
  sdfat::SdFile podfile(podFilename, sdfat::O_READ);
  if(!podfile.isOpen()) {
    Sprintf("Could not open %s\n", podFilename);
    return false;;
  }

  char buffer[201] = {0};

  if(podfile.fgets(buffer, sizeof(buffer)-1) < 8) {
    Sprintln(F("Could not read podcast URL"));
    podfile.close();
    return false;;
  }
  buffer[strcspn(buffer, "\n\r")] = '\0';
  info.feedUrl = buffer;

  memset(buffer, 0, sizeof(buffer));
  if(podfile.fgets(buffer, sizeof(buffer)-1) < 1) {
    Sprintln(F("Could not read max episode count"));
    podfile.close();
    return false;;
  }
  info.maxEpisodes = atoi(buffer);

  memset(buffer, 0, sizeof(buffer));
  if(podfile.fgets(buffer, sizeof(buffer)-1) < 1) {
    Sprintln(F("Could not read last guid"));
    podfile.close();
    return false;;
  }
  buffer[strcspn(buffer, "\n\r")] = '\0';
  info.lastGuid = buffer;

  memset(buffer, 0, sizeof(buffer));
  if(podfile.fgets(buffer, sizeof(buffer)-1) < 1) {
    Sprintln(F("Could not read last file no"));
    podfile.close();
    return false;;
  }
  info.lastFileNo = atoi(buffer);

  podfile.close();
  return true;
}

void ShelfPodcast::_loadFeed(_HTTPClient &httpClient, const String &feedUrl, PodcastState &state) {
  Podcatcher catcher;
  EpisodeCallback episodeCallback = std::bind(&PodcastState::episodeCallback, &state, std::placeholders::_1, std::placeholders::_2);
  catcher.begin(episodeCallback);

  std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
  client->setInsecure();
  httpClient.begin(*client, feedUrl);
  httpClient.setReuse(false);
  httpClient.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  int httpCode = httpClient.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("HTTP Error: %d\n", httpCode);
    httpClient.end();
    return;
  }

  int len = httpClient.getSize();
  WiFiClient* stream = httpClient.getStreamPtr();

  if(httpClient.getTransferEncoding() == HTTPC_TE_CHUNKED) {
    Serial.println(F("Chunked"));
  }

  while(len > 0 || httpClient.getTransferEncoding() == HTTPC_TE_CHUNKED){ 
    if(httpClient.getTransferEncoding() == HTTPC_TE_CHUNKED) {

      char chunkSizeBuffer[20] = "";

      size_t headerLength = stream->readBytesUntil('\n', chunkSizeBuffer, sizeof(chunkSizeBuffer));

      if(headerLength == 0) {
        Serial.println(F("Last Chunk"));
        break;
      } else {
        // Terminate string and get rid of \r
        chunkSizeBuffer[headerLength - 1] = '\0';

        // read size of chunk
        len = (uint32_t) strtol((const char *) chunkSizeBuffer, NULL, 16);
      }
    }

    while(len > 0) {
      if(stream->available()) {
        if(!state.done) {
          catcher.processChar(stream->read());
        } else {
          httpClient.end();
          Serial.println(F("Fetched podcast"));
          return;
        }
        if (len > 0) {
          len--;
        }
      }
    }

    if(httpClient.getTransferEncoding() == HTTPC_TE_CHUNKED) {
      char buf[2];
      stream->readBytes((uint8_t*)buf, 2);
    }
  }
  httpClient.end();
  Serial.println(F("Fetched NEW podcast"));
}

boolean ShelfPodcast::_downloadNextEpisode(PodcastInfo &info, const char *folder) {
  PodcastState state(info);

  _HTTPClient httpClient;

  _loadFeed(httpClient, info.feedUrl, state);

  if(state.episodeUrl.isEmpty()) {
    Sprintln(F("Episode URL empty"));
    return false;
  }

  char tmpFilename[50] = "";
  snprintf_P(tmpFilename, sizeof(tmpFilename)-1, PSTR("/%s/download.tmp"), folder);

  Sprint(F("File ")); Sprintln(tmpFilename);
  Sprint(F("Downloading ")); Sprintln(state.episodeUrl);

  std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
  // do not validate certificate
  client->setInsecure();

  httpClient.begin(*client, state.episodeUrl);
  httpClient.setReuse(false);
  httpClient.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  int httpCode = httpClient.GET();
  if (httpCode != HTTP_CODE_OK) {
    Sprintf("invalid response code: %d\n", httpCode);
    httpClient.end();
    return false;
  }

  sdfat::File episodeFile(tmpFilename, sdfat::O_WRITE | sdfat::O_CREAT | sdfat::O_TRUNC);
  if(!episodeFile.isOpen()) {
    Sprintln(F("Failed to open target file"));
    httpClient.end();
    return false;
  }

  httpClient.writeToStream(&episodeFile);
  httpClient.end();

  episodeFile.close();

  char filename[50] = "";
  do {
    info.lastFileNo++;
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%05d.mp3"), folder, info.lastFileNo);
  } while(_SD.exists(filename));

  Sprintf("Renaming to %s\n", filename);
  if(!_SD.rename(tmpFilename, filename)) {
    Sprintln(F("Failed to rename tmp file"));
    return false;
  }

  info.lastGuid = state.episodeGuid;
  Sprintln(F("Download done"));
  return true;
}

void ShelfPodcast::_cleanupEpisodes(uint16_t maxEpisodes, const char *folderName) {
  Sprintln(F("Cleaning up old episodes"));
  sdfat::SdFile dir;
  if(!dir.open(folderName, sdfat::O_READ)) {
    Sprintf("Could not open folder /%s/\n", folderName);
    return;
  }

  uint16_t mp3Count = 0;
  do {
    char filename[50] = {0};
    uint16_t oldestDate = UINT16_MAX;
    uint16_t oldestTime = UINT16_MAX;
    mp3Count = 0;
    sdfat::SdFile file;
    dir.rewind();
    while(file.openNext(&dir, sdfat::O_READ)) {
      char sfn[13] = {0};
      file.getSFN(sfn);
      if(Adafruit_VS1053_FilePlayer::isMP3File(sfn)) {
        mp3Count++;
        sdfat::dir_t d;
        file.dirEntry(&d);
        if((d.creationDate < oldestDate) || ((d.creationDate == oldestDate) && (d.creationTime < oldestTime))) {
          snprintf_P(filename, sizeof(filename), PSTR("/%s/%s"), folderName, sfn);
        }
      }
      file.close();
    }

    if((strlen(filename) > 0) && (mp3Count > maxEpisodes)) {
      if(_SD.remove(filename)) {
        Sprintf("Deleted %s\n", filename);
      } else {
        Sprintf("Failed to delete %s\n", filename);
      }
    }
  } while(mp3Count > maxEpisodes);
}

bool ShelfPodcast::_isPodcastTime() {
  // Only check once an hour (and thereby make sure podcast are only fetech once at podcastHour)
  if((_lastUpdate != 0) && ((millis()-_lastUpdate) < 60*60*1000)) {
    return false;
  }

  _lastUpdate = millis();

  time_t now;
  tm tm;
  time(&now);
  localtime_r(&now, &tm);

  return tm.tm_hour == _config.podcastHour;
}

void ShelfPodcast::work() {
  if(!_isPodcastTime()) {
    return;
  }

  _resumePlayback = (_playback.playbackState() == PLAYBACK_FILE);

  if(_resumePlayback) {
    _playback.pausePlayback();
  }

  char folder[13] = {0};

  while(true) {
    if(!_nextPodcast(folder)) {
      break;
    }

    PodcastInfo info;

    char podFilename[25] = {0};
    snprintf_P(podFilename, sizeof(podFilename), PSTR("/%s/.podcast"), folder);

    if(!_readPodcastFile(info, podFilename)) {
      continue;
    }

    Sprintf("Podcast: %s\n", info.feedUrl.c_str());

    _web.pause();

    do {
      unsigned long start = millis();

      if(!_downloadNextEpisode(info, folder)) {
        break;
      }

      // This writes to the SD card more often then needed, but prevents re-downloading of episodes in case of a crash.
      Sprintln(F("Updating .podcast"));
      sdfat::SdFile podfile;
      if(!podfile.open(podFilename, sdfat::O_WRITE | sdfat::O_TRUNC)) {
        Sprintf("Could not open %s\n", podFilename);
        return;
      }
      podfile.println(info.feedUrl);
      podfile.println(info.maxEpisodes);
      podfile.println(info.lastGuid);
      podfile.println(info.lastFileNo);
      podfile.close();

      Sprint(F("Episode took: ")); Sprintln((millis()-start)/(1000*60));

    } while(true);

    _cleanupEpisodes(info.maxEpisodes, folder);

    _web.unpause();
  }

  if(_resumePlayback) {
    _playback.resumePlayback();
  }

  Sprintln(F("Done with podcasts"));
}