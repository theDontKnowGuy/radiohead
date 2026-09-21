#include "media.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <cstring>
#include <esp_heap_caps.h>
#include <new>

#include "app_state.h"
#include "podcast_json_reader.h"

namespace {

bool isHttpUrl(const String& url) {
    return url.length() <= 512 &&
        (url.startsWith("http://") || url.startsWith("https://"));
}

bool isPlayableStation(const RadioStation& station) {
    return !station.name.isEmpty() && isHttpUrl(station.url);
}

constexpr unsigned long kConnectionTimeoutMs = 12000;
constexpr unsigned long kWifiRecoveryPollMs = 5000;
constexpr unsigned long kRecoveryDelaysMs[] = {2000, 5000, 15000, 30000};
constexpr size_t kPodcastTitleBytes = 160;
constexpr size_t kPodcastIdBytes = 48;
constexpr size_t kPodcastDateBytes = 32;
constexpr size_t kPodcastUrlBytes = 512;
PlaybackState playbackState = PlaybackState::Stopped;
int requestedStation = -1;
int playingStation = -1;
unsigned long requestStartedAt = 0;
bool stationRecoveryScheduled = false;
bool stationRecoveryAttempt = false;
bool stationTestPlayback = false;
String stationTestName;
bool ignoreEofUntilPlaybackReady = false;
uint8_t stationRecoveryAttempts = 0;
unsigned long stationRecoveryDueAt = 0;

PodcastLoadState podcastState = PodcastLoadState::Idle;
int requestedPodcastShow = -1;
uint32_t podcastRequestGeneration = 0;
// Main-loop owned: stays busy until the published result has been consumed,
// including when the worker has finished but mediaTick() has not run yet.
bool podcastFetchBusy = false;
portMUX_TYPE podcastFetchMux = portMUX_INITIALIZER_UNLOCKED;
PodcastEpisode fetchedPodcastEpisodes[MAX_EPISODES];
int fetchedPodcastEpisodeCount = 0;
int fetchedPodcastShow = -1;
uint32_t fetchedPodcastGeneration = 0;
bool podcastFetchResultReady = false;
bool podcastFetchSucceeded = false;
int activePodcastShow = -1;
int activePodcastEpisode = -1;
PodcastEpisode activePodcastEpisodeData;
bool hasActivePodcastEpisode = false;
bool podcastPaused = false;
bool podcastControlError = false;

uint32_t activePodcastDurationSeconds() {
    const uint32_t decodedDuration = audio.getAudioFileDuration();
    if (decodedDuration > 0) return decodedDuration;
    if (hasActivePodcastEpisode) {
        return activePodcastEpisodeData.durationSeconds;
    }
    return 0;
}

void truncate(String& value, size_t limit) {
    if (value.length() > limit) value.remove(limit);
}

bool hasRecoverableStation() {
    return !podcastMode && requestedStation >= 0 && requestedStation < STATION_COUNT &&
        isPlayableStation(stations[requestedStation]);
}

void cancelStationRecovery() {
    stationRecoveryScheduled = false;
    stationRecoveryAttempts = 0;
}

void logStreamDiagnostic(const char* event) {
    Serial.printf(
        "[stream] event=%s requested=%d playing=%d state=%d running=%d wifi=%d rssi=%ld buffer=%lu/%lu\n",
        event, requestedStation, playingStation, static_cast<int>(playbackState), audio.isRunning(),
        static_cast<int>(WiFi.status()), static_cast<long>(WiFi.RSSI()),
        static_cast<unsigned long>(audio.inBufferFilled()),
        static_cast<unsigned long>(audio.getInBufferSize()));
}

void scheduleStationRecovery(const char* reason) {
    if (!hasRecoverableStation() || stationRecoveryScheduled) return;

    const size_t delayIndex = std::min<size_t>(
        stationRecoveryAttempts, sizeof(kRecoveryDelaysMs) / sizeof(kRecoveryDelaysMs[0]) - 1);
    const unsigned long delayMs = kRecoveryDelaysMs[delayIndex];
    ++stationRecoveryAttempts;
    stationRecoveryDueAt = millis() + delayMs;
    stationRecoveryScheduled = true;
    playbackState = PlaybackState::Connecting;
    playingStation = -1;
    forceRedraw = true;
    Serial.printf("[stream] recovery scheduled station=%d reason=%s attempt=%u delay_ms=%lu\n",
        requestedStation, reason, static_cast<unsigned>(stationRecoveryAttempts), delayMs);
}

void updatePlaybackFromAudioInfo(Audio::msg_t message) {
    if (message.e == Audio::evt_info && message.msg != nullptr &&
        (strcmp(message.msg, "slow stream") == 0 || strcmp(message.msg, "Stream lost") == 0)) {
        // The audio library has its own stream-loss reconnect path.  Record the
        // evidence but do not compete with it by opening another connection.
        logStreamDiagnostic(message.msg);
    }
    // `stream ready` is emitted by the installed audio library after a decoder
    // is initialized. A successful TCP connect alone is not shown as Playing.
    if (message.e == Audio::evt_info && message.msg != nullptr &&
        strcmp(message.msg, "stream ready") == 0 && podcastMode) {
        ignoreEofUntilPlaybackReady = false;
        playbackState = PlaybackState::Playing;
        forceRedraw = true;
    } else if (message.e == Audio::evt_info && message.msg != nullptr &&
        strcmp(message.msg, "stream ready") == 0 && (requestedStation >= 0 || stationTestPlayback)) {
        ignoreEofUntilPlaybackReady = false;
        stationRecoveryScheduled = false;
        stationRecoveryAttempts = 0;
        playbackState = PlaybackState::Playing;
        playingStation = requestedStation;
        forceRedraw = true;
    } else if (message.e == Audio::evt_eof && playbackState != PlaybackState::Stopped) {
        // Replacing a station queues an EOF from the old connection.  Let the
        // new request reach `stream ready` (or its bounded timeout) before an
        // EOF can be treated as a failure of that request.
        if (!ignoreEofUntilPlaybackReady) {
            logStreamDiagnostic("eof");
            playbackState = PlaybackState::Failed;
            playingStation = -1;
            forceRedraw = true;
            scheduleStationRecovery("eof");
        }
    }
}

bool fetchPodcastEpisodes(
    int showIndex,
    uint32_t generation,
    PodcastEpisode (&result)[MAX_EPISODES],
    int& resultCount) {
    resultCount = 0;
    const unsigned long startedAt = millis();
    Serial.printf("[podcast] request=%lu show=%d start wifi=%d rssi=%ld heap=%u internal=%u largest_internal=%u\n",
        static_cast<unsigned long>(generation), showIndex, static_cast<int>(WiFi.status()),
        static_cast<long>(WiFi.RSSI()), ESP.getFreeHeap(),
        static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
        static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
    if (showIndex < 0 || showIndex >= PODCAST_SHOW_COUNT || WiFi.status() != WL_CONNECTED) {
        Serial.printf("[podcast] request=%lu rejected: invalid show or Wi-Fi disconnected\n",
            static_cast<unsigned long>(generation));
        return false;
    }

    const PodcastShow& show = podcastShows[showIndex];
    const String url = "https://api.omny.fm/programs/" + String(show.program) +
        "/playlists/" + String(show.playlist) + "/clips?pageSize=" + String(MAX_EPISODES);
    WiFiClientSecure client;
    client.setInsecure();
    client.setHandshakeTimeout(10);  // Seconds; avoid the library's 120-second default.
    HTTPClient http;
    http.useHTTP10(true);
    http.setConnectTimeout(10000);
    http.setTimeout(10000);
    if (!http.begin(client, url)) {
        Serial.printf("[podcast] request=%lu HTTP begin failed\n", static_cast<unsigned long>(generation));
        return false;
    }
    const char* headerKeys[] = {"Content-Type", "Transfer-Encoding", "Content-Encoding"};
    http.collectHeaders(headerKeys, 3);
    const int code = http.GET();
    Serial.printf("[podcast] request=%lu HTTP=%d headers_ms=%lu length=%d json=%d chunked=%d encoded=%d\n",
        static_cast<unsigned long>(generation), code, millis() - startedAt, http.getSize(),
        http.header("Content-Type").startsWith("application/json"),
        http.header("Transfer-Encoding").equalsIgnoreCase("chunked"),
        !http.header("Content-Encoding").isEmpty() && !http.header("Content-Encoding").equalsIgnoreCase("identity"));
    if (code != HTTP_CODE_OK) {
        char tlsError[128] = {};
        const int tlsCode = client.lastError(tlsError, sizeof(tlsError));
        Serial.printf("[podcast] request=%lu HTTP failed: %s tls=%d (%s) wifi=%d\n",
            static_cast<unsigned long>(generation),
            code < 0 ? HTTPClient::errorToString(code).c_str() : "server status",
            tlsCode, tlsCode == 0 ? "none" : tlsError, static_cast<int>(WiFi.status()));
        http.end();
        return false;
    }

    // Omny's documented consumer Clip model supplies Id, Title, PublishedUtc,
    // AudioUrl/MediaUrls.AudioUrl, ImageUrl and DurationSeconds. Only the
    // bounded fields below are retained; artwork is not downloaded on-device.
    JsonDocument filter;
    filter["Clips"][0]["Id"] = true;
    filter["Clips"][0]["Title"] = true;
    filter["Clips"][0]["PublishedUtc"] = true;
    filter["Clips"][0]["AudioUrl"] = true;
    filter["Clips"][0]["MediaUrls"]["AudioUrl"] = true;
    filter["Clips"][0]["DurationSeconds"] = true;
    JsonDocument document;
    const unsigned long parseStartedAt = millis();
    PodcastJsonReader<WiFiClientSecure> reader(client);
    const DeserializationError error = deserializeJson(
        document, reader, DeserializationOption::Filter(filter));
    Serial.printf("[podcast] request=%lu JSON=%s parse_ms=%lu clips_array=%d clips=%u overflow=%d bytes=%u stop=%s\n",
        static_cast<unsigned long>(generation), error.c_str(), millis() - parseStartedAt,
        document["Clips"].is<JsonArray>(), static_cast<unsigned>(document["Clips"].size()), document.overflowed(),
        static_cast<unsigned>(reader.bytesRead()), reader.stopReason());
    http.end();
    if (error || !document["Clips"].is<JsonArray>()) return false;

    int skipped = 0;
    for (JsonObject clip : document["Clips"].as<JsonArray>()) {
        if (resultCount >= MAX_EPISODES) break;
        PodcastEpisode episode;
        episode.id = String(clip["Id"] | "");
        episode.title = String(clip["Title"] | "");
        episode.publishedUtc = String(clip["PublishedUtc"] | "");
        episode.audioUrl = String(clip["AudioUrl"] | "");
        if (episode.audioUrl.isEmpty()) episode.audioUrl = String(clip["MediaUrls"]["AudioUrl"] | "");
        const JsonVariantConst duration = clip["DurationSeconds"];
        episode.durationSeconds = duration.is<uint32_t>() ? duration.as<uint32_t>() : 0;
        truncate(episode.id, kPodcastIdBytes);
        truncate(episode.title, kPodcastTitleBytes);
        truncate(episode.publishedUtc, kPodcastDateBytes);
        truncate(episode.audioUrl, kPodcastUrlBytes);
        // A temporary audio URL or array index is never used as identity.
        if (episode.id.isEmpty() || !isHttpUrl(episode.audioUrl)) {
            ++skipped;
            continue;
        }
        result[resultCount++] = episode;
    }
    Serial.printf("[podcast] request=%lu playable=%d skipped=%d total_ms=%lu\n",
        static_cast<unsigned long>(generation), resultCount, skipped, millis() - startedAt);
    return resultCount > 0;
}

struct PodcastFetchRequest {
    int showIndex;
    uint32_t generation;
};

void podcastFetchWorker(void* parameter) {
    const PodcastFetchRequest request = *static_cast<PodcastFetchRequest*>(parameter);
    delete static_cast<PodcastFetchRequest*>(parameter);
    int count = 0;
    const unsigned long startedAt = millis();
    bool succeeded = false;
    {
        PodcastEpisode result[MAX_EPISODES];
        succeeded = fetchPodcastEpisodes(request.showIndex, request.generation, result, count);
        // This worker is the only writer. Keep the result slot reserved until
        // mediaTick() consumes it, even after the worker exits.
        for (int index = 0; index < count; ++index) fetchedPodcastEpisodes[index] = result[index];
    }  // Destroy the temporary Strings: vTaskDelete() does not unwind C++ locals.
    Serial.printf("[podcast] request=%lu show=%d complete=%s count=%d elapsed_ms=%lu heap=%u stack_free=%u\n",
        static_cast<unsigned long>(request.generation), request.showIndex, succeeded ? "ready" : "failed",
        count, millis() - startedAt, ESP.getFreeHeap(), static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
    // Publish completion only after the bounded values and metadata are written.
    fetchedPodcastEpisodeCount = count;
    fetchedPodcastShow = request.showIndex;
    fetchedPodcastGeneration = request.generation;
    podcastFetchSucceeded = succeeded;
    portENTER_CRITICAL(&podcastFetchMux);
    podcastFetchResultReady = true;
    portEXIT_CRITICAL(&podcastFetchMux);
    vTaskDelete(nullptr);
}

void startPendingPodcastFetch() {
    if (podcastFetchBusy || requestedPodcastShow < 0) return;
    auto* request = new (std::nothrow) PodcastFetchRequest{requestedPodcastShow, podcastRequestGeneration};
    podcastFetchBusy = true;
    if (request == nullptr || xTaskCreate(podcastFetchWorker, "podcast-fetch", 6144, request, 1, nullptr) != pdPASS) {
        delete request;
        podcastFetchBusy = false;
        Serial.printf("[podcast] request=%lu show=%d worker allocation failed heap=%u\n",
            static_cast<unsigned long>(podcastRequestGeneration), requestedPodcastShow, ESP.getFreeHeap());
        podcastState = PodcastLoadState::Failed;
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
    bool hasResult = false;
    int resultShow = -1;
    int resultCount = 0;
    uint32_t resultGeneration = 0;
    bool resultSucceeded = false;
    portENTER_CRITICAL(&podcastFetchMux);
    if (podcastFetchResultReady) {
        podcastFetchResultReady = false;
        hasResult = true;
        resultShow = fetchedPodcastShow;
        resultCount = fetchedPodcastEpisodeCount;
        resultGeneration = fetchedPodcastGeneration;
        resultSucceeded = podcastFetchSucceeded;
    }
    portEXIT_CRITICAL(&podcastFetchMux);
    if (hasResult && resultGeneration == podcastRequestGeneration && resultShow == requestedPodcastShow) {
        if (resultSucceeded) {
            for (int index = 0; index < resultCount; ++index) podcastEpisodes[index] = fetchedPodcastEpisodes[index];
            podcastEpisodeCount = resultCount;
            loadedPodcastShow = resultShow;
            podcastState = PodcastLoadState::Ready;
        } else {
            podcastEpisodeCount = 0;
            loadedPodcastShow = -1;
            podcastState = PodcastLoadState::Failed;
        }
        forceRedraw = true;
    }
    if (hasResult) {
        Serial.printf("[podcast] request=%lu show=%d result=%s current_request=%lu\n",
            static_cast<unsigned long>(resultGeneration), resultShow,
            resultGeneration == podcastRequestGeneration && resultShow == requestedPodcastShow
                ? (resultSucceeded ? "applied" : "failed") : "discarded-stale",
            static_cast<unsigned long>(podcastRequestGeneration));
        podcastFetchBusy = false;
    }
    if (podcastState == PodcastLoadState::Loading && !podcastFetchBusy) startPendingPodcastFetch();

    if (stationRecoveryScheduled && static_cast<long>(now - stationRecoveryDueAt) >= 0) {
        if (WiFi.status() != WL_CONNECTED) {
            stationRecoveryDueAt = now + kWifiRecoveryPollMs;
            Serial.printf("[stream] recovery waiting for Wi-Fi station=%d retry_ms=%lu\n",
                requestedStation, kWifiRecoveryPollMs);
        } else {
            const int station = requestedStation;
            stationRecoveryScheduled = false;
            stationRecoveryAttempt = true;
            playStation(station);
            stationRecoveryAttempt = false;
        }
    }

    if (playbackState == PlaybackState::Connecting && !stationRecoveryScheduled &&
        now - requestStartedAt >= kConnectionTimeoutMs) {
        logStreamDiagnostic("connection timeout");
        playbackState = PlaybackState::Failed;
        forceRedraw = true;
        scheduleStationRecovery("connection timeout");
    } else if (playbackState == PlaybackState::Playing && !podcastPaused && !audio.isRunning()) {
        logStreamDiagnostic("player stopped");
        playbackState = PlaybackState::Failed;
        playingStation = -1;
        forceRedraw = true;
        scheduleStationRecovery("player stopped");
    } else if (playbackState == PlaybackState::Failed && !stationRecoveryScheduled) {
        scheduleStationRecovery("connection failed");
    }
}

void setRadioVolumeIndex(int volumeIndex) {
    mainVal = constrain(volumeIndex, 0, 21);
    // Adjusting a muted radio changes the saved/restored level but does not
    // resume audio.  This keeps web and encoder behavior consistent.
    if (!radioMuted) {
        audio.setVolume(volCurve[mainVal]);
    }
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

bool requestPodcastEpisodes(int showIndex) {
    if (showIndex < 0 || showIndex >= PODCAST_SHOW_COUNT) return false;
    // Reopening the current show must not invalidate its cache or enqueue a
    // duplicate fetch while the user is returning from the player screen.
    if (podcastEpisodesReadyFor(showIndex) ||
        (podcastState == PodcastLoadState::Loading && requestedPodcastShow == showIndex)) {
        return true;
    }
    ++podcastRequestGeneration;
    requestedPodcastShow = showIndex;
    podcastState = PodcastLoadState::Loading;
    Serial.printf("[podcast] request=%lu show=%d queued busy=%d\n",
        static_cast<unsigned long>(podcastRequestGeneration), showIndex, podcastFetchBusy);
    if (!podcastFetchBusy) startPendingPodcastFetch();
    forceRedraw = true;
    return true;
}

PodcastLoadState podcastLoadState() { return podcastState; }
int podcastRequestedShow() { return requestedPodcastShow; }
bool podcastEpisodesReadyFor(int showIndex) {
    return podcastState == PodcastLoadState::Ready && loadedPodcastShow == showIndex;
}

bool playPodcastEpisode(int showIndex, int episodeIndex) {
    if (!podcastEpisodesReadyFor(showIndex) || episodeIndex < 0 || episodeIndex >= podcastEpisodeCount) return false;

    const PodcastShow& show = podcastShows[showIndex];
    PodcastEpisode& episode = podcastEpisodes[episodeIndex];
    if (!isHttpUrl(episode.audioUrl)) {
        return false;
    }
    Serial.println("Playing podcast");

    cancelStationRecovery();
    audio.stopSong();
    podcastMode = true;
    radioMuted = false;
    podcastShowTft = show.tftName;
    activePodcastShow = showIndex;
    activePodcastEpisode = episodeIndex;
    activePodcastEpisodeData = episode;
    hasActivePodcastEpisode = true;
    podcastPaused = false;
    podcastControlError = false;
    requestedStation = -1;
    playingStation = -1;
    playbackState = PlaybackState::Connecting;
    requestStartedAt = millis();
    ignoreEofUntilPlaybackReady = true;

    String date = episode.publishedUtc;
    if (date.length() >= 10) {
        date = date.substring(0, 10);
    }
    songTitle = "Recorded: " + date;
    forceRedraw = true;
    if (!audio.connecttohost(episode.audioUrl.c_str())) {
        playbackState = PlaybackState::Failed;
        forceRedraw = true;
        return false;
    }
    return true;
}

bool togglePodcastPause() {
    if (!podcastMode || activePodcastEpisode < 0 || playbackState != PlaybackState::Playing) {
        podcastControlError = true;
        forceRedraw = true;
        return false;
    }
    if (!audio.pauseResume()) {
        podcastControlError = true;
        forceRedraw = true;
        return false;
    }
    podcastPaused = !podcastPaused;
    podcastControlError = false;
    forceRedraw = true;
    return true;
}

bool seekPodcastBySeconds(int seconds) {
    if (!podcastMode || podcastPaused || playbackState != PlaybackState::Playing || seconds == 0) {
        podcastControlError = true;
        forceRedraw = true;
        return false;
    }
    const uint32_t duration = activePodcastDurationSeconds();
    if (duration == 0) {
        podcastControlError = true;
        forceRedraw = true;
        return false;
    }
    const int64_t target = static_cast<int64_t>(audio.getAudioCurrentTime()) + seconds;
    if (target < 0 || target >= static_cast<int64_t>(duration)) {
        podcastControlError = true;
        forceRedraw = true;
        return false;
    }
    // setTimeOffset() only checks that the source is a web file.  Seeking by
    // byte position additionally checks the library's observed Accept-Ranges
    // response, so a CDN that ignores Range cannot leave the player pretending
    // that it skipped.
    const uint32_t currentByte = audio.getAudioFilePosition();
    const uint32_t bitRate = audio.getBitRate();
    const int64_t byteDelta = (static_cast<int64_t>(bitRate) * seconds) / 8;
    const int64_t targetByte = static_cast<int64_t>(currentByte) + byteDelta;
    const bool success = currentByte > 0 && bitRate > 0 && targetByte > 0 &&
        audio.setAudioFilePosition(static_cast<uint32_t>(targetByte));
    podcastControlError = !success;
    forceRedraw = true;
    return success;
}

PodcastPlaybackSnapshot podcastPlaybackSnapshot() {
    PodcastPlaybackSnapshot snapshot;
    snapshot.active = podcastMode && hasActivePodcastEpisode && activePodcastShow >= 0 && activePodcastEpisode >= 0;
    snapshot.paused = podcastPaused;
    snapshot.showIndex = activePodcastShow;
    snapshot.episodeIndex = activePodcastEpisode;
    snapshot.elapsedSeconds = snapshot.active ? audio.getAudioCurrentTime() : 0;
    snapshot.durationSeconds = snapshot.active ? activePodcastDurationSeconds() : 0;
    snapshot.canPause = snapshot.active && playbackState == PlaybackState::Playing;
    snapshot.canSeek = snapshot.canPause && !podcastPaused && snapshot.durationSeconds > 0 &&
        audio.getBitRate() > 0 && audio.getAudioFilePosition() > 0 && !podcastControlError;
    snapshot.controlError = podcastControlError;
    return snapshot;
}

const PodcastEpisode* podcastActiveEpisode() {
    return hasActivePodcastEpisode ? &activePodcastEpisodeData : nullptr;
}

void playStation(int stationIndex) {
    stationTestPlayback = false;
    stationTestName = "";
    if (!stationRecoveryAttempt) {
        cancelStationRecovery();
    }
    if (isAP || stationIndex < 0 || stationIndex >= STATION_COUNT) {
        playbackState = PlaybackState::Failed;
        requestedStation = -1;
        playingStation = -1;
        forceRedraw = true;
        return;
    }

    podcastMode = false;
    hasActivePodcastEpisode = false;
    activePodcastShow = -1;
    activePodcastEpisode = -1;
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
    ignoreEofUntilPlaybackReady = true;
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

bool startStationTest(const String& name, const String& url) {
    if (isAP || name.isEmpty() || !isHttpUrl(url)) return false;
    cancelStationRecovery();
    podcastMode = false;
    hasActivePodcastEpisode = false;
    activePodcastShow = -1;
    activePodcastEpisode = -1;
    radioMuted = false;
    audio.setVolume(volCurve[mainVal]);
    podcastShowTft = "";
    songTitle = "";
    stationTestPlayback = true;
    stationTestName = name;
    requestedStation = -1;
    playingStation = -1;
    playbackState = PlaybackState::Connecting;
    requestStartedAt = millis();
    ignoreEofUntilPlaybackReady = true;
    forceRedraw = true;
    const String targetUrl = parseM3U(url);
    if (!isHttpUrl(targetUrl) || !audio.connecttohost(targetUrl.c_str())) {
        playbackState = PlaybackState::Failed;
        forceRedraw = true;
    }
    return true;
}

void stopStationPlayback() {
    cancelStationRecovery();
    audio.stopSong();
    podcastMode = false;
    hasActivePodcastEpisode = false;
    activePodcastShow = -1;
    activePodcastEpisode = -1;
    podcastPaused = false;
    playbackState = PlaybackState::Stopped;
    ignoreEofUntilPlaybackReady = false;
    requestedStation = -1;
    playingStation = -1;
    stationTestPlayback = false;
    stationTestName = "";
    forceRedraw = true;
}

PlaybackState mediaPlaybackState() { return playbackState; }
bool mediaTestActive() { return stationTestPlayback; }
String mediaTestName() { return stationTestName; }
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
