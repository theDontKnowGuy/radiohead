#include "media.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <cstring>

#include "app_state.h"

namespace {

bool isHttpUrl(const String& url) {
    return url.length() <= 512 &&
        (url.startsWith("http://") || url.startsWith("https://"));
}

bool isPlayableStation(const RadioStation& station) {
    return !station.name.isEmpty() && isHttpUrl(station.url);
}

constexpr unsigned long kConnectionTimeoutMs = 12000;
PlaybackState playbackState = PlaybackState::Stopped;
int requestedStation = -1;
int playingStation = -1;
unsigned long requestStartedAt = 0;

void updatePlaybackFromAudioInfo(Audio::msg_t message) {
    // `stream ready` is emitted by the installed audio library after a decoder
    // is initialized. A successful TCP connect alone is not shown as Playing.
    if (message.e == Audio::evt_info && message.msg != nullptr &&
        strcmp(message.msg, "stream ready") == 0 && requestedStation >= 0) {
        playbackState = PlaybackState::Playing;
        playingStation = requestedStation;
        forceRedraw = true;
    } else if (message.e == Audio::evt_eof && playbackState != PlaybackState::Stopped) {
        playbackState = PlaybackState::Failed;
        playingStation = -1;
        forceRedraw = true;
    }
}

template <typename LineHandler>
void readHttpLines(HTTPClient& http, LineHandler handleLine) {
    WiFiClient* stream = http.getStreamPtr();
    String line;
    line.reserve(160);
    bool lineTooLong = false;
    unsigned long lastData = millis();

    while (http.connected() || stream->available()) {
        const int value = stream->read();
        if (value < 0) {
            audio.loop();
            if (millis() - lastData >= 3000) {
                break;
            }
            delay(1);
            continue;
        }
        lastData = millis();
        const char character = static_cast<char>(value);
        if (character == '\n') {
            if (!lineTooLong) {
                line.trim();
                if (handleLine(line)) {
                    return;
                }
            }
            line = "";
            lineTooLong = false;
        } else if (character != '\r' && !lineTooLong) {
            if (line.length() < 640) {
                line += character;
            } else {
                lineTooLong = true;
            }
        }
        audio.loop();
    }

    if (!line.isEmpty() && !lineTooLong) {
        line.trim();
        handleLine(line);
    }
}

}  // namespace

void mediaBegin() {
    Audio::audio_info_callback = updatePlaybackFromAudioInfo;
}

void mediaTick(unsigned long now) {
    if (playbackState == PlaybackState::Connecting && now - requestStartedAt >= kConnectionTimeoutMs) {
        playbackState = PlaybackState::Failed;
        forceRedraw = true;
    } else if (playbackState == PlaybackState::Playing && !audio.isRunning()) {
        playbackState = PlaybackState::Failed;
        playingStation = -1;
        forceRedraw = true;
    }
}

void setRadioVolumeIndex(int volumeIndex) {
    mainVal = constrain(volumeIndex, 0, 21);
    radioMuted = false;
    audio.setVolume(volCurve[mainVal]);
    lastVolChange = millis();
    forceRedraw = true;
}

void toggleRadioMute() {
    radioMuted = !radioMuted;
    audio.setVolume(radioMuted ? 0 : volCurve[mainVal]);
    forceRedraw = true;
}

bool isStationMuted() {
    return radioMuted;
}

int playableStationCount() {
    int count = 0;
    for (int slot = 0; slot < STATION_COUNT; ++slot) {
        if (isPlayableStation(stations[slot])) {
            ++count;
        }
    }
    return count;
}

int playableStationSlotAt(int visibleIndex) {
    if (visibleIndex < 0) {
        return -1;
    }
    for (int slot = 0; slot < STATION_COUNT; ++slot) {
        if (!isPlayableStation(stations[slot])) {
            continue;
        }
        if (visibleIndex-- == 0) {
            return slot;
        }
    }
    return -1;
}

int selectedPlayableStationIndex() {
    int visibleIndex = 0;
    for (int slot = 0; slot < STATION_COUNT; ++slot) {
        if (!isPlayableStation(stations[slot])) {
            continue;
        }
        if (slot == currentStationIdx) {
            return visibleIndex;
        }
        ++visibleIndex;
    }
    return -1;
}

int adjacentPlayableStationSlot(int stationIndex, int direction) {
    const int count = playableStationCount();
    if (count == 0 || direction == 0) {
        return -1;
    }
    int visibleIndex = -1;
    for (int index = 0; index < count; ++index) {
        if (playableStationSlotAt(index) == stationIndex) {
            visibleIndex = index;
            break;
        }
    }
    if (visibleIndex < 0) {
        return playableStationSlotAt(direction > 0 ? 0 : count - 1);
    }
    return playableStationSlotAt((visibleIndex + (direction > 0 ? 1 : count - 1)) % count);
}

void parseM3UPro(const String& playlistUrl) {
    m3uTempList.clear();
    if (!isHttpUrl(playlistUrl)) {
        return;
    }

    HTTPClient http;
    if (!http.begin(playlistUrl)) {
        return;
    }
    http.setTimeout(3000);
    const int httpCode = http.GET();

    String currentExtName = "Unknown Station";
    if (httpCode == HTTP_CODE_OK) {
        readHttpLines(http, [&](const String& line) {
            if (line.startsWith("#EXTINF:")) {
                const int commaIndex = line.indexOf(',');
                if (commaIndex != -1) {
                    currentExtName = line.substring(commaIndex + 1);
                    if (currentExtName.length() > 80) {
                        currentExtName.remove(80);
                    }
                }
            } else if (isHttpUrl(line)) {
                m3uTempList.push_back({currentExtName, line});
                currentExtName = "Unknown Station";
            }
            return m3uTempList.size() >= 30;
        });
    }
    http.end();
}

String parseM3U(const String& url) {
    if (!isHttpUrl(url) || (!url.endsWith(".m3u") && !url.endsWith(".m3u8"))) {
        return url;
    }

    HTTPClient http;
    if (!http.begin(url)) {
        return url;
    }
    http.setTimeout(3000);
    const int httpCode = http.GET();
    String streamUrl;

    if (httpCode == HTTP_CODE_OK) {
        readHttpLines(http, [&](const String& line) {
            if (isHttpUrl(line)) {
                streamUrl = line;
                return true;
            }
            return false;
        });
    }

    http.end();
    return streamUrl.isEmpty() ? url : streamUrl;
}

bool loadPodcastEpisodes(int showIndex) {
    if (showIndex < 0 || showIndex >= PODCAST_SHOW_COUNT || WiFi.status() != WL_CONNECTED) {
        return false;
    }

    podcastEpisodeCount = 0;
    loadedPodcastShow = -1;
    const PodcastShow& show = podcastShows[showIndex];
    const String url =
        "https://api.omny.fm/programs/" + String(show.program) + "/playlists/" +
        String(show.playlist) + "/clips?pageSize=" + String(MAX_EPISODES);

    Serial.print("Omny API: ");
    Serial.println(url);

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.useHTTP10(true);
    http.setTimeout(10000);

    if (!http.begin(client, url)) {
        Serial.println("Omny: http.begin failed");
        return false;
    }

    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("Omny HTTP error: %d\n", code);
        http.end();
        return false;
    }

    JsonDocument filter;
    filter["Clips"][0]["Title"] = true;
    filter["Clips"][0]["PublishedUtc"] = true;
    filter["Clips"][0]["AudioUrl"] = true;
    filter["Clips"][0]["MediaUrls"]["AudioUrl"] = true;

    JsonDocument document;
    const DeserializationError error = deserializeJson(
        document, http.getStream(), DeserializationOption::Filter(filter));

    if (error) {
        Serial.print("Omny JSON error: ");
        Serial.println(error.c_str());
        http.end();
        return false;
    }

    for (JsonObject clip : document["Clips"].as<JsonArray>()) {
        if (podcastEpisodeCount >= MAX_EPISODES) {
            break;
        }

        String title = clip["Title"] | "";
        String published = clip["PublishedUtc"] | "";
        String audioUrl = clip["AudioUrl"] | "";
        if (audioUrl.isEmpty()) {
            audioUrl = clip["MediaUrls"]["AudioUrl"] | "";
        }
        if (!isHttpUrl(audioUrl)) {
            continue;
        }
        if (title.length() > 160) {
            title.remove(160);
        }
        if (published.length() > 32) {
            published.remove(32);
        }

        PodcastEpisode& episode = podcastEpisodes[podcastEpisodeCount++];
        episode.title = title;
        episode.publishedUtc = published;
        episode.audioUrl = audioUrl;
    }

    http.end();
    Serial.printf("Loaded %d podcast episodes\n", podcastEpisodeCount);
    if (podcastEpisodeCount > 0) {
        loadedPodcastShow = showIndex;
    }
    return podcastEpisodeCount > 0;
}

void playPodcastEpisode(int showIndex, int episodeIndex) {
    if (showIndex < 0 || showIndex >= PODCAST_SHOW_COUNT) {
        return;
    }
    if (loadedPodcastShow != showIndex && !loadPodcastEpisodes(showIndex)) {
        return;
    }
    if (episodeIndex < 0 || episodeIndex >= podcastEpisodeCount) {
        return;
    }

    const PodcastShow& show = podcastShows[showIndex];
    PodcastEpisode& episode = podcastEpisodes[episodeIndex];
    if (!isHttpUrl(episode.audioUrl)) {
        return;
    }
    Serial.println("Playing podcast");

    audio.stopSong();
    podcastMode = true;
    podcastShowTft = show.tftName;

    String date = episode.publishedUtc;
    if (date.length() >= 10) {
        date = date.substring(0, 10);
    }
    songTitle = "Recorded: " + date;
    forceRedraw = true;
    audio.connecttohost(episode.audioUrl.c_str());
}

void playStation(int stationIndex) {
    if (isAP || stationIndex < 0 || stationIndex >= STATION_COUNT) {
        playbackState = PlaybackState::Failed;
        requestedStation = -1;
        playingStation = -1;
        forceRedraw = true;
        return;
    }

    podcastMode = false;
    radioMuted = false;
    audio.setVolume(volCurve[mainVal]);
    podcastShowTft = "";
    songTitle = "";
    if (!isHttpUrl(stations[stationIndex].url)) {
        audio.stopSong();
        playbackState = PlaybackState::Failed;
        requestedStation = stationIndex;
        playingStation = -1;
        forceRedraw = true;
        return;
    }
    currentStationIdx = stationIndex;
    tempStationIdx = stationIndex;
    requestedStation = stationIndex;
    playingStation = -1;
    playbackState = PlaybackState::Connecting;
    requestStartedAt = millis();
    forceRedraw = true;
    const String targetUrl = parseM3U(stations[stationIndex].url);
    if (isHttpUrl(targetUrl)) {
        if (!audio.connecttohost(targetUrl.c_str())) {
            playbackState = PlaybackState::Failed;
            forceRedraw = true;
        }
    } else {
        playbackState = PlaybackState::Failed;
        forceRedraw = true;
    }
}

void stopStationPlayback() {
    audio.stopSong();
    playbackState = PlaybackState::Stopped;
    requestedStation = -1;
    playingStation = -1;
    forceRedraw = true;
}

PlaybackState mediaPlaybackState() { return playbackState; }
int mediaRequestedStation() { return requestedStation; }
int mediaPlayingStation() { return playingStation; }

void audio_showstreamtitle(const char* info) {
    if (podcastMode || info == nullptr) {
        return;
    }

    String title(info);
    title.trim();
    if (title.length() > 160) {
        title.remove(160);
    }
    if (!title.isEmpty()) {
        songTitle = title;
        forceRedraw = true;
    }
}
