#include "ShelfPodcast.h"

bool ShelfPodcast::_nextPodcast(char *folder) {
  File32 root = _SD.open("/");
  if(!root.isOpen()) {
    Sprintln(F("Could not open root"));
    return false;
  }

  bool lastFolder = false;

  // This safes one File32 in global scope at the expense of performance. Might or might not be a good idea.
  File32 entry;
  while(entry.openNext(&root, O_READ)) {
    if(!entry.isDir()) {
      entry.close();
      continue;
    }

    char newFolder[13] = {0};
    entry.getSFN(newFolder, sizeof(newFolder));

    if(!entry.exists(".podcast")) {
      Sprintf("No .podcast in %s\n", newFolder);
      entry.close();
      continue;
    }

    if(!strlen(folder) || lastFolder) {
      strncpy(folder, newFolder, 13);
      entry.close();
      root.close();
      return true;
    }
    
    if(!strcmp(folder, newFolder)) {
      lastFolder = true;
    }

    entry.close();
  }

  root.close();
  return false;
};

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

boolean ShelfPodcast::_downloadNextEpisode(ShelfConfig::PodcastConfig &info, const char *folder) {
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
  httpClient.setTimeout(10*1000);
  int httpCode = httpClient.GET();
  if (httpCode != HTTP_CODE_OK) {
    Sprintf("invalid response code: %d\n", httpCode);
    httpClient.end();
    return false;
  }

  ShelfWeb::StreamFile episodeFile;
  episodeFile.open(&_SD, tmpFilename, O_WRITE | O_CREAT | O_TRUNC);
  if(!episodeFile.isOpen()) {
    Sprintln(F("Failed to open target file"));
    httpClient.end();
    return false;
  }

  if(httpClient.getSize() > 0) {
    Sprintln("PreAllocating file");
    episodeFile.preAllocate(httpClient.getSize());
  }

  int byteCount = httpClient.writeToPrint(&episodeFile);
  Sprintf("Downloaded %d bytes\n", byteCount);
  httpClient.end();

  episodeFile.close();

  if(byteCount <= 0 || ((httpClient.getSize() > 0) && (byteCount < httpClient.getSize()))) {
    return false;
  }

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
  File32 dir = _SD.open(folderName, O_READ);
  if(!dir.isOpen()) {
    Sprintf("Could not open folder /%s/\n", folderName);
    return;
  }

  uint16_t mp3Count = 0;
  do {
    char filename[50] = {0};
    uint16_t oldestDate = UINT16_MAX;
    uint16_t oldestTime = UINT16_MAX;
    mp3Count = 0;
    File32 file;
    dir.rewind();
    while(file.openNext(&dir, O_READ)) {
      char sfn[13] = {0};
      file.getSFN(sfn, sizeof(sfn));
      if(Adafruit_VS1053_FilePlayer::isMP3File(sfn)) {
        mp3Count++;
        uint16_t createDate;
        uint16_t createTime;
        file.getCreateDateTime(&createDate, &createTime);
        if((createDate < oldestDate) || ((createDate == oldestDate) && (createTime < oldestTime))) {
          snprintf_P(filename, sizeof(filename), PSTR("/%s/%s"), folderName, sfn);
          oldestDate = createDate;
          oldestTime = createTime;
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
  // Only check once an hour (and thereby make sure podcast are only fetch once at podcastHour)
  if((_lastUpdate != 0) && ((millis()-_lastUpdate) < 60*60*1000)) {
    return false;
  }

  _lastUpdate = millis();

  //return true;

  time_t now;
  tm tm;
  time(&now);
  localtime_r(&now, &tm);

  return tm.tm_hour == _config.podcastHour;
}

void ShelfPodcast::work() {
  if(!_isPodcastTime() && !_web.updatePodcastsRequested) {
    return;
  }

  _web.updatePodcastsRequested=false;

  bool resumePlayback = (_playback.playbackState() == ShelfPlayback::PLAYBACK_FILE);

  if(_playback.playbackState() == ShelfPlayback::PLAYBACK_FILE) {
    _playback.pausePlayback();
  }

  char folder[13] = {0};

  while(true) {
    if(!_nextPodcast(folder)) {
      break;
    }

    ShelfConfig::PodcastConfig info(_SD);

    char podFilename[25] = {0};
    snprintf_P(podFilename, sizeof(podFilename), PSTR("/%s/.podcast"), folder);

    if(!info.load(podFilename)) {
      continue;
    }

    if(!info.enabled) {
      Sprintln(F("Podcast disabled - Skipping"));
      continue;
    }

    char playbackFolder[13] = {0};
    _playback.currentFolderSFN(playbackFolder, sizeof(playbackFolder));

    // If current folder is currently playing we better stop the player to not delete anything currently played
    if(strcmp(folder, playbackFolder) == 0) {
      _playback.stopPlayback();
    }

    Sprintf("Podcast: %s\n", info.feedUrl.c_str());

    _web.pause();

    do {
      unsigned long start = millis();

      if(!_downloadNextEpisode(info, folder)) {
        break;
      }

      info.save(podFilename);

      Sprint(F("Episode took: ")); Sprintln((millis()-start)/(1000*60));

    } while(true);

    _cleanupEpisodes(info.maxEpisodes, folder);

    _web.unpause();
  }

  if(resumePlayback) {
    if(_playback.playbackState() == ShelfPlayback::PLAYBACK_PAUSED) {
      _playback.resumePlayback();
    } else {
      _playback.startPlayback();
    }
  }

  Sprintln(F("Done with podcasts"));
}