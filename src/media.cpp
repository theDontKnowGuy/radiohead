#include "media.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "app_state.h"

namespace {

bool isHttpUrl(const String& url) {
    return url.length() <= 512 &&
        (url.startsWith("http://") || url.startsWith("https://"));
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
        return;
    }

    podcastMode = false;
    podcastShowTft = "";
    songTitle = "";
    if (!isHttpUrl(stations[stationIndex].url)) {
        audio.stopSong();
        forceRedraw = true;
        return;
    }
    const String targetUrl = parseM3U(stations[stationIndex].url);
    if (isHttpUrl(targetUrl)) {
        audio.connecttohost(targetUrl.c_str());
    }
}

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
    }
}
