#include "ShelfPodcast.h"

void ShelfPodcast::_episodeCallback(PodcastState *state, const char *url, const char *guid) {
  if(state->lastGuid.equals(guid) || (++(state->episodeCount) > state->maxEpisodes)) {
    state->done = true;
    return;
  }

  state->episodeUrl = String(url);
  state->episodeGuid = String(guid);
#ifdef DEBUG_ENABLE
  Serial.printf("%s %s\n", url, guid);
#endif
}

void ShelfPodcast::work() {
  if(_lastUpdate != 0) {
    return;
  }
  _lastUpdate = millis();

  PodcastState state;

  const char folder[] = "/test/";

  char filename[50] = "";
  snprintf(filename, sizeof(filename), "/%s/.podcast", folder);

  if(!_SD.exists(filename)) {
    Sprintf("No .podcast in %s\n", folder);
    return;
  }

  sdfat::SdFile podfile(filename, sdfat::O_READ);
  if(!podfile.isOpen()) {
    Sprintf("Could not open %s\n", filename);
    return;
  }

  char feedUrl[151] = {0};
  if(podfile.fgets(feedUrl, sizeof(feedUrl)-1) < 8) {
    Sprintln("Could not read podcast URL");
    return;
  }
  feedUrl[strcspn(feedUrl, "\n\r")] = '\0';

  char buffer[151] = {0};
  if(podfile.fgets(buffer, sizeof(buffer)-1) < 1) {
    Sprintln("Could not read max episode count");
    return;
  }
  state.maxEpisodes = atoi(buffer);

  if(podfile.fgets(buffer, sizeof(buffer)-1) < 1) {
    Sprintln("Could not read last guid");
    return;
  }
  buffer[strcspn(buffer, "\n\r")] = '\0';
  state.lastGuid = String(buffer);

  podfile.close();

  do {
    state.done = false;
    unsigned long start = millis();
    _loadFeed(&state, feedUrl);
    _downloadEpisodes(&state, folder);
    Sprint("Episode took: "); Sprintln((millis()-start)/(1000*60));
  } while(state.done);
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
  state->done = true;
  Serial.println("Fetched NEW podcast");
}

void ShelfPodcast::_downloadEpisodes(PodcastState *state, const char *folder) {
  if(state->episodeUrl.isEmpty()) {
    return;
  }

  uint16_t fileCount = 0;
  char filename[50] = "";
  do {
    fileCount++;
    snprintf(filename, sizeof(filename), "/test/%05d.mp3", fileCount);
  } while(_SD.exists(filename));

  Sprint("File "); Sprintln(filename);
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
    Sprintf("invalid response code: %d", httpCode);
    httpClient.end();
    return;
  }

  snprintf(filename, sizeof(filename), "/test/%05d.mp3", fileCount);
  sdfat::File episodeFile(filename,sdfat::O_WRITE | sdfat::O_CREAT);
  if(!episodeFile.isOpen()) {
    Sprintln("Failed to open target file");
    httpClient.end();
    return;
  }

  httpClient.writeToStream(&episodeFile);
  httpClient.end();

  episodeFile.close();
  state->lastGuid = state->episodeGuid;
  Sprintln("Download done");
}