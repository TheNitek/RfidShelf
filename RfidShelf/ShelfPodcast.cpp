#include "ShelfPodcast.h"

void ShelfPodcast::_episodeCallback(PodcastState *state, const char *url, const char *guid) {
  if((strcmp(state->lastGuid, guid) == 0) || (++(state->episodeCount) > state->maxEpisodes)) {
    state->done = true;
    return;
  }

  strncpy(state->episodeUrl, url, sizeof(state->episodeUrl)-1);
  strncpy(state->episodeGuid, guid, sizeof(state->episodeGuid)-1);
#ifdef DEBUG_ENABLE
  Serial.printf("%s %s\n", url, guid);
#endif
}

bool ShelfPodcast::_nextPodcast(char *folder) {
  sdfat::SdFile root;
  if(!root.open("/")) {
    Sprintln("Could not open root");
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

bool ShelfPodcast::_readPodcastFile(PodcastState &state, const char* podFilename) {
      sdfat::SdFile podfile(podFilename, sdfat::O_READ);
      if(!podfile.isOpen()) {
        Sprintf("Could not open %s\n", podFilename);
        return false;;
      }

      if(podfile.fgets(state.feedUrl, sizeof(state.feedUrl)-1) < 8) {
        Sprintln("Could not read podcast URL");
        podfile.close();
        return false;;
      }
      state.feedUrl[strcspn(state.feedUrl, "\n\r")] = '\0';

      char buffer[5] = {0};
      if(podfile.fgets(buffer, sizeof(buffer)-1) < 1) {
        Sprintln("Could not read max episode count");
        podfile.close();
        return false;;
      }
      state.maxEpisodes = atoi(buffer);

      if(podfile.fgets(state.lastGuid, sizeof(state.lastGuid)-1) < 1) {
        Sprintln("Could not read last guid");
        podfile.close();
        return false;;
      }
      state.lastGuid[strcspn(state.lastGuid, "\n\r")] = '\0';

      if(podfile.fgets(buffer, sizeof(buffer)-1) < 1) {
        Sprintln("Could not read last file no");
        podfile.close();
        return false;;
      }
      state.lastFileNo = atoi(buffer);

      podfile.close();
      return true;
}

void ShelfPodcast::_loadFeed(PodcastState *state, const char *podcastUrl) {
  Podcatcher catcher;
  EpisodeCallback episodeCallback = std::bind(&ShelfPodcast::_episodeCallback, this, state, std::placeholders::_1, std::placeholders::_2);
  catcher.begin(episodeCallback);

  std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
  client->setInsecure();
  class: public HTTPClient {
    public:
      transferEncoding_t getTransferEncoding() {
        return _transferEncoding;
      }
  } httpClient;
  httpClient.begin(*client, podcastUrl);
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
    Serial.println("Chunked");
  }

  while(len > 0 || httpClient.getTransferEncoding() == HTTPC_TE_CHUNKED){ 
    if(httpClient.getTransferEncoding() == HTTPC_TE_CHUNKED) {

      char chunkSizeBuffer[20] = "";

      size_t headerLength = stream->readBytesUntil('\n', chunkSizeBuffer, sizeof(chunkSizeBuffer));

      if(headerLength == 0) {
        Serial.println("Last Chunk");
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
        if(!state->done) {
          catcher.processChar(stream->read());
        } else {
          httpClient.end();
          Serial.println("Fetched podcast");
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
  Serial.println("Fetched NEW podcast");
}

boolean ShelfPodcast::_downloadEpisodes(PodcastState *state, const char *folder) {
  if(strlen(state->episodeUrl) == 0) {
    Sprintln("Episode URL empty");
    return false;
  }

  char tmpFilename[50] = "";
  snprintf(tmpFilename, sizeof(tmpFilename), "/%s/download.tmp", folder);

  Sprint("File "); Sprintln(tmpFilename);
  Sprint("Downloading "); Sprintln(state->episodeUrl);

  std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
  // do not validate certificate
  client->setInsecure();

  HTTPClient httpClient;
  httpClient.begin(*client, state->episodeUrl);
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
    Sprintln("Failed to open target file");
    httpClient.end();
    return false;
  }

  httpClient.writeToStream(&episodeFile);
  httpClient.end();

  episodeFile.close();

  char filename[50] = "";
  do {
    state->lastFileNo++;
    snprintf(filename, sizeof(filename), "/%s/%05d.mp3", folder, state->lastFileNo);
  } while(_SD.exists(filename));

  Sprintf("Renaming to %s\n", filename);
  if(!_SD.rename(tmpFilename, filename)) {
    Sprintln("Failed to rename tmp file");
    return false;
  }

  strncpy(state->lastGuid, state->episodeGuid, sizeof(state->lastGuid)-1);
  Sprintln("Download done");
  return true;
}

void ShelfPodcast::_cleanupEpisodes(PodcastState &state, const char *folderName) {
  Sprintln("Cleaning up old episodes");
  sdfat::SdFile dir;
  if(!dir.open(folderName, sdfat::O_READ)) {
    Sprintf("Could not open folder /%s/\n", folderName);
    return;
  }

  uint16_t mp3Count = 0;
  do {
    char filename[50] = "";
    dir.rewind();
    uint16_t oldestDate = UINT16_MAX;
    uint16_t oldestTime = UINT16_MAX;
    mp3Count = 0;
    sdfat::SdFile file;
    while(file.openNext(&dir, sdfat::O_READ)) {
      char sfn[13] = "";
      file.getSFN(sfn);
      if(Adafruit_VS1053_FilePlayer::isMP3File(sfn)) {
        mp3Count++;
        sdfat::dir_t d;
        file.dirEntry(&d);
        if((d.creationDate < oldestDate) || ((d.creationDate == oldestDate) && (d.creationTime < oldestTime))) {
          snprintf(filename, sizeof(filename), "/%s/%s", folderName, sfn);
        }
      }
      file.close();
    }

    if((strlen(filename) > 0) && (mp3Count > state.maxEpisodes)) {
      if(_SD.remove(filename)) {
        Sprintf("Deleted %s\n", filename);
      } else {
        Sprintf("Failed to delete %s\n", filename);
      }
    }
  } while(mp3Count > state.maxEpisodes);
}

void ShelfPodcast::work() {
  if(_lastUpdate != 0) {
    return;
  }
  _lastUpdate = millis();

  char folder[13] = {0};

  while(true) {
    if(!_nextPodcast(folder)) {
      break;
    }

    char podFilename[25] = {0};
    snprintf(podFilename, sizeof(podFilename), "/%s/.podcast", folder);

    if(!_readPodcastFile(state, podFilename)) {
      continue;
    }

    Sprintf("Podcast: %s\n", state.feedUrl);

    _web.pause();

    do {
      state.done = false;
      state.episodeUrl[0] = '\0';
      state.episodeGuid[0] = '\0';
      state.episodeCount = 0;

      unsigned long start = millis();
      _loadFeed(&state, state.feedUrl);

      if(_downloadEpisodes(&state, folder)) {
        Sprintln("Updating .podcast");
        sdfat::SdFile podfile(podFilename,  sdfat::O_WRITE | sdfat::O_TRUNC);
        if(!podfile.isOpen()) {
          Sprintf("Could not open %s\n", podFilename);
          return;
        }
        podfile.println(state.feedUrl);
        podfile.println(state.maxEpisodes);
        podfile.println(state.lastGuid);
        podfile.println(state.lastFileNo);
        podfile.close();
      }

      Sprint("Episode took: "); Sprintln((millis()-start)/(1000*60));
    } while(strlen(state.episodeUrl) > 0);

    _cleanupEpisodes(state, folder);

    _web.unpause();
  }

  Sprintln("Done with podcasts");
}