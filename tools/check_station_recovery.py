"""Exercise production radio recovery with simulated PCM, Wi-Fi and a clock."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / '.pio/station_recovery_native'
out.mkdir(parents=True, exist_ok=True)
media = (root / 'src/media.cpp').read_text()

def section(start, end):
    return media[media.index(start):media.index(end, media.index(start))]

recovery_tick = section('#if defined(RADIO_SPOTIFY_EXPERIMENT)\n    // A station retry', 'void setRadioVolumeIndex(')
fixture = r'''
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>
#include "media.h"
#include "radio_stream_health.h"
#include "spotify_adapter.h"
PlaybackState playbackState;
constexpr unsigned long kConnectionTimeoutMs = 12000, kWifiRecoveryPollMs = 5000;
constexpr unsigned long kRecoveryDelaysMs[] = {2000, 5000, 15000, 30000};
unsigned long clockMs, requestStartedAt, stationRecoveryDueAt;
bool stationRecoveryScheduled, stationRecoveryAttempt;
int requestedStation = 1, playingStation = -1, attempts;
bool forceRedraw = false, podcastPaused = false, podcastMode = false;
bool stationTestPlayback = false, ignoreEofUntilPlaybackReady = false;
bool eligible = true, logging = true;
bool isAP = false, radioMuted = false;
int currentStationIdx = 1, tempStationIdx = 1, mainVal = 10, gB = 0, gM = 0, gT = 0;
constexpr int STATION_COUNT = 3, PODCAST_SHOW_COUNT = 1;
struct RadioStation { String name, url; } stations[STATION_COUNT] = {
    {"First", "http://first.example/stream"}, {"Second", "http://second.example/list.m3u"},
    {"Dead", "http://dead.example/stream"},
};
String stationTestName, podcastShowTft, songTitle, activePodcastShowName;
int activePodcastShow = -1, activePodcastEpisode = -1;
bool hasActivePodcastEpisode = false;
struct PodcastEpisode { String title; } activePodcastEpisodeData;
uint8_t stationRecoveryAttempts = 0;
constexpr int WL_CONNECTED = 3;
constexpr unsigned RadioPcmStall = 512, RadioRecovery = 16, RadioReady = 8, RadioOutputTimeout = 8192;
struct { int value = WL_CONNECTED; int status() { return value; } } WiFi;
struct { template<class... T> void printf(const char*, T...) {} } Serial;
struct Audio { enum { evt_info, evt_log, evt_eof }; struct msg_t { int e; const char* msg; }; };
void updatePlaybackFromAudioInfo(Audio::msg_t message);
struct {
    bool running = true, queuedReady = false, restoreWorks = true;
    uint32_t buffered = 256000, bitRate = 128000;
    bool isRunning() { return running; }
    uint32_t inBufferFilled() { return buffered; }
    uint32_t getBitRate() { return bitRate; }
    void setVolume(int) {}
    void stopSong() { running = false; }
    void loop() {
        if (queuedReady) {
            queuedReady = false;
            updatePlaybackFromAudioInfo({Audio::evt_info, "stream ready"});
            updatePlaybackFromAudioInfo({Audio::evt_eof, "old stream"});
        }
    }
    bool connecttohost(const char* url) {
        ++attempts;
        clockMs += 20;
        return running = std::strcmp(url, "http://dead.example/stream") != 0;
    }
    bool releaseOutputForHandoff(unsigned) { return true; }
    bool restoreOutputAfterHandoff() { return restoreWorks; }
} audio;
#if !defined(RADIO_SPOTIFY_EXPERIMENT)
enum class OutputOwner { Local, Spotify, Failed };
std::atomic<OutputOwner> outputOwner{OutputOwner::Local};
#endif
std::atomic<uint32_t> localPcmBlocks{0};
RadioStreamHealth stationHealth;
bool wifiHandoffPending = false;
uint32_t wifiHandoffBlocks = 0, wifiHandoffProgressAt = 0, wifiHandoffBuffered = 0;
void mediaUpdateSpeakerMute(bool) {}
void mediaSilenceSpeakers() {}
void mediaSetTone(int, int, int) {}
String parseM3U(const String& url) {
    assert(!audio.isRunning());
    audio.loop();
    clockMs += 1;
    return url;
}
unsigned signals = 0;
unsigned long millis() { return clockMs; }
enum class RadioMediaOperation { SpotifyRestore, Connect, AwaitPlayback };
void radioDiagnosticsMediaBegin(RadioMediaOperation, unsigned) {}
void radioDiagnosticsMediaEnd(int) {}
void radioDiagnosticsMediaRetryWait(unsigned) {}
void radioDiagnosticsSignal(unsigned flag) { if (logging) signals |= flag; }
void logStreamDiagnostic(const char*) {}
void validationEvent(const char*) {}
bool hasRecoverableStation() {
    return eligible && !podcastMode && requestedStation >= 0 && requestedStation < STATION_COUNT;
}
#if defined(RADIO_SPOTIFY_EXPERIMENT)
bool spotifyReleases = true;
SpotifySignal pendingSpotifySignal;
SpotifySignal spotifyAdapterTakeSignal() {
    const auto signal = pendingSpotifySignal;
    pendingSpotifySignal = {};
    return signal;
}
void spotifyAdapterDiscardPending() { pendingSpotifySignal = {}; }
bool spotifyAdapterReleaseOutput(uint32_t) { return spotifyReleases; }
bool spotifyAdapterAcquireOutput(uint8_t, uint32_t) { return true; }
void spotifyAdapterSetOutputVolume(uint8_t) {}
void spotifyAdapterSetTone(int8_t, int8_t, int8_t) {}
uint8_t radioVolumeGain8(int value) { return value; }
int constrain(int value, int low, int high) { return std::clamp(value, low, high); }
#endif
'''
fixture += section('bool isHttpUrl(', 'bool isPlayableStation(')
fixture += section('void cancelStationRecovery();', 'uint32_t activePodcastDurationSeconds()')
fixture += section('void cancelStationRecovery() {', 'void logStreamDiagnostic(')
fixture += section('void scheduleStationRecovery(', 'bool fetchPodcastEpisodes(')
fixture += section('void mediaLocalPcmBlock() {', 'void mediaBegin()')
fixture += section('bool mediaCanRoamWifi() {', 'void mediaTick(')
fixture += section('void playStation(int stationIndex) {', 'bool mediaTestActive()')
fixture += 'void tick(unsigned long now) {\n'
fixture += '#if defined(RADIO_SPOTIFY_EXPERIMENT)\n    serviceSpotifySignals();\n#endif\n'
fixture += recovery_tick + r'''
void at(unsigned long now) { clockMs = now; tick(now); }
void reset(unsigned long now) {
    clockMs = requestStartedAt = now; attempts = 0; signals = 0;
    stationRecoveryAttempt = false;
    cancelStationRecovery();
    playbackState = PlaybackState::Connecting; WiFi.value = WL_CONNECTED;
    eligible = true; requestedStation = 1; podcastMode = podcastPaused = false;
    logging = true; audio.running = true; audio.buffered = 256000; audio.bitRate = 128000; outputOwner = OutputOwner::Local;
    audio.queuedReady = false; audio.restoreWorks = true;
    stationTestPlayback = false;
#if defined(RADIO_SPOTIFY_EXPERIMENT)
    spotifyStatus = MediaStatus::Stopped; spotifyReleases = true; pendingSpotifySignal = {};
#endif
}
void playing(unsigned long now) {
    reset(now); playbackState = PlaybackState::Playing; tick(now);
}
int main() {
    // A retry or UI connection started after the caller sampled now must not
    // underflow its connection timeout (existing regression).
    reset(100); stationRecoveryScheduled = true; stationRecoveryDueAt = 100;
    tick(100);
    assert(attempts == 1 && !stationRecoveryScheduled && stationRecoveryAttempts == 0);
    at(requestStartedAt + kConnectionTimeoutMs - 1);
    assert(!stationRecoveryScheduled);
    at(clockMs + 1);
    assert(stationRecoveryScheduled && stationRecoveryAttempts == 1);
    reset(200); requestStartedAt = clockMs = 210; tick(200);
    assert(!stationRecoveryScheduled);
    reset(std::numeric_limits<unsigned long>::max() - 10);
    const auto beforeWrap = requestStartedAt;
    at(clockMs + 20); assert(!stationRecoveryScheduled);
    at(beforeWrap + kConnectionTimeoutMs); assert(stationRecoveryScheduled);

    // Reproduce today's failure: Running stays true, but PCM freezes.
    playing(100); at(15099); assert(!stationRecoveryScheduled);
    at(15100);
    assert(audio.running && stationRecoveryScheduled && stationRecoveryAttempts == 1);
    assert(playbackState == PlaybackState::Connecting && playingStation == -1);
    assert((signals & (RadioPcmStall | RadioRecovery)) == (RadioPcmStall | RadioRecovery));
    const auto due = stationRecoveryDueAt;
    at(due - 1); assert(attempts == 0);
    at(due); assert(attempts == 1 && !stationRecoveryScheduled);

    // Stream-ready without sustained output must not erase the backoff.
    updatePlaybackFromAudioInfo({Audio::evt_info, "stream ready"});
    assert(stationRecoveryAttempts == 1);
    tick(clockMs); at(clockMs + 15000);
    assert(stationRecoveryAttempts == 2 && stationRecoveryDueAt == clockMs + 5000);

    // Wi-Fi outage: no connection attempts until Wi-Fi is available.
    WiFi.value = 0; at(stationRecoveryDueAt);
    assert(attempts == 1 && stationRecoveryScheduled);
    assert(stationRecoveryDueAt == clockMs + kWifiRecoveryPollMs);
    WiFi.value = WL_CONNECTED; at(stationRecoveryDueAt);
    assert(attempts == 2 && !stationRecoveryScheduled);

    // Existing stopped/failed/EOF paths remain active.
    playing(0); audio.running = false; at(1); assert(stationRecoveryScheduled);
    reset(0); playbackState = PlaybackState::Failed; at(1); assert(stationRecoveryScheduled);
    playing(0); ignoreEofUntilPlaybackReady = false;
    updatePlaybackFromAudioInfo({Audio::evt_eof, "eof"}); assert(stationRecoveryScheduled);

    // Recovery remains active with diagnostics switched off.
    playing(0); logging = false; at(15000);
    assert(stationRecoveryScheduled && signals == 0);

    // Brief stalls/resumption do not reconnect. Stable PCM eventually resets
    // backoff, unlike a decoder-ready notification alone.
    playing(0); stationRecoveryAttempts = 3;
    for (unsigned t = 1000; t <= 30000; t += 1000) {
        mediaLocalPcmBlock(); at(t); assert(!stationRecoveryScheduled);
        if (t < 30000) assert(stationRecoveryAttempts == 3);
    }
    assert(stationRecoveryAttempts == 0);
    at(44999); assert(!stationRecoveryScheduled);
    mediaLocalPcmBlock(); at(45000); assert(!stationRecoveryScheduled);
    at(60000); assert(stationRecoveryScheduled);

    // A fresh selection or stop cancels the pending retry and stale timer.
    playing(0); at(15000); cancelStationRecovery();
    tick(clockMs); at(29999); assert(!stationRecoveryScheduled);
    cancelStationRecovery(); playbackState = PlaybackState::Stopped;
    at(90000); assert(!stationRecoveryScheduled);
    // Other sources and unsaved preview streams must not restart a saved station.
    for (int mode = 0; mode < 3; ++mode) {
        playing(0);
        if (mode == 0) podcastMode = true;
        if (mode == 1) requestedStation = -1;
        if (mode == 2) eligible = false;
        at(30000); assert(!stationRecoveryScheduled);
    }
#if defined(RADIO_SPOTIFY_EXPERIMENT)
    for (auto owner : {OutputOwner::Spotify, OutputOwner::Failed}) {
        playing(0); outputOwner = owner; at(30000);
        assert(!stationRecoveryScheduled);
    }
#endif
    // A stale podcast pause flag must not suppress a resumed radio's guard.
    playing(0); podcastPaused = true; at(15000); assert(stationRecoveryScheduled);

    // Long outages keep the capped delay without uint8_t wraparound.
    reset(0);
    for (unsigned i = 0; i < 300; ++i) {
        stationRecoveryScheduled = false;
        scheduleStationRecovery("failure");
        assert(stationRecoveryDueAt - clockMs == kRecoveryDelaysMs[std::min(i, 3U)]);
    }
    assert(stationRecoveryAttempts == UINT8_MAX);

    // A failed station must not keep its retry or queued events when the user
    // selects another station, including when that station uses a playlist.
    playing(0); playStation(2); assert(playbackState == PlaybackState::Failed);
    tick(clockMs); assert(stationRecoveryScheduled);
    const auto deadRetry = stationRecoveryDueAt;
    audio.queuedReady = true;
    playStation(1);
    assert(!stationRecoveryScheduled && !audio.queuedReady && playbackState == PlaybackState::Connecting);
    assert(mediaSnapshot().name == "Second" && mediaSnapshot().status == MediaStatus::Connecting);
    updatePlaybackFromAudioInfo({Audio::evt_info, "stream ready"});
    at(deadRetry); assert(attempts == 2 && playingStation == 1);
    assert(playbackState == PlaybackState::Playing);

#if defined(RADIO_SPOTIFY_EXPERIMENT)
    // A live or paused Spotify session can miss the release deadline. Retain
    // the station, retry behind the deadline and restore output before tuning.
    reset(0); requestedStation = -1; audio.running = false; outputOwner = OutputOwner::Spotify;
    spotifyStatus = MediaStatus::Paused; spotifyReleases = false;
    playStation(1);
    assert(outputOwner == OutputOwner::Failed && stationRecoveryScheduled && attempts == 0);
    assert(mediaSnapshot().source == MediaSource::Radio && mediaSnapshot().status == MediaStatus::Connecting);
    spotifyReleases = true;
    at(stationRecoveryDueAt - 1); assert(attempts == 0);
    at(stationRecoveryDueAt);
    assert(outputOwner == OutputOwner::Local && !stationRecoveryScheduled && attempts == 1);
    updatePlaybackFromAudioInfo({Audio::evt_info, "stream ready"});
    assert(mediaSnapshot().status == MediaStatus::Playing && playingStation == 1);

    // A Spotify transfer-away can fail to create local output before the web
    // selection arrives. Choosing another station then supersedes that retry.
    reset(0); requestedStation = -1; audio.running = false; outputOwner = OutputOwner::Spotify;
    audio.restoreWorks = false; pendingSpotifySignal.type = SpotifySignalType::Stop;
    tick(clockMs); assert(mediaSnapshot().source == MediaSource::Spotify);
    assert(mediaSnapshot().status == MediaStatus::Failed);
    playStation(1); assert(stationRecoveryScheduled);
    const auto supersededRetry = stationRecoveryDueAt;
    audio.restoreWorks = true; playStation(0);
    assert(outputOwner == OutputOwner::Local && !stationRecoveryScheduled && attempts == 1);
    updatePlaybackFromAudioInfo({Audio::evt_info, "stream ready"});
    at(supersededRetry); assert(attempts == 1 && playingStation == 0);

    // Stopping or removing the station cancels the retry even when I2S restore
    // is still failing. A later release must not start the discarded selection.
    reset(0); requestedStation = -1; audio.running = false; outputOwner = OutputOwner::Spotify;
    spotifyReleases = false; playStation(1); assert(stationRecoveryScheduled);
    const auto stoppedRetry = stationRecoveryDueAt;
    stopStationPlayback(); assert(requestedStation == -1 && !stationRecoveryScheduled);
    spotifyReleases = true; at(stoppedRetry); assert(attempts == 0);
#endif

    // Voluntary roaming uses actual data, not nominal buffer capacity.
    playing(0); assert(mediaCanRoamWifi());
    audio.buffered = 191999; assert(!mediaCanRoamWifi());
    audio.buffered = 192000; assert(mediaCanRoamWifi());
    audio.bitRate = 0; assert(!mediaCanRoamWifi());
    audio.bitRate = UINT32_MAX; assert(!mediaCanRoamWifi());
    audio.bitRate = 128000; podcastMode = true; assert(!mediaCanRoamWifi());
    podcastMode = false; playbackState = PlaybackState::Connecting; assert(!mediaCanRoamWifi());
    playbackState = PlaybackState::Stopped; assert(mediaCanRoamWifi());

    // Keep buffered audio across the disconnect. Reconnect as it runs low,
    // without waiting for the normal 10-second library or 15-second PCM guard.
    playing(100); mediaWifiHandoffStarted(); assert(!mediaCanRoamWifi());
    WiFi.value = 0;
    for (unsigned t = 200; t <= 1200; t += 100) {
        audio.buffered -= 1600; mediaLocalPcmBlock(); at(t);
        assert(attempts == 0 && playbackState == PlaybackState::Playing);
    }
    WiFi.value = WL_CONNECTED; mediaLocalPcmBlock(); at(1300); assert(attempts == 0);
    audio.buffered = 8000; mediaLocalPcmBlock(); at(1400);
    assert(attempts == 1 && !wifiHandoffPending);
    // A resumed socket must not be restarted or inhibit future roaming.
    playing(0); mediaWifiHandoffStarted(); audio.buffered -= 100; at(1);
    audio.buffered += 200; mediaLocalPcmBlock(); at(2);
    assert(!wifiHandoffPending && attempts == 0);
    // Stuck PCM with a stale full buffer has a short bounded grace period.
    playing(0); mediaWifiHandoffStarted(); at(749); assert(attempts == 0);
    at(750); assert(attempts == 1);
    playing(UINT32_MAX - 100); mediaWifiHandoffStarted();
    at(uint32_t(UINT32_MAX - 100 + 750)); assert(attempts == 1);
    // Never restart a stopped or newly selected source from a stale handoff.
    playing(0); mediaWifiHandoffStarted(); cancelStationRecovery();
    playbackState = PlaybackState::Stopped; at(2000); assert(attempts == 0);
    playing(0); mediaWifiHandoffStarted(); podcastMode = true;
    at(2000); assert(!wifiHandoffPending && attempts == 0);
#if defined(RADIO_SPOTIFY_EXPERIMENT)
    playing(0); outputOwner = OutputOwner::Spotify;
    assert(!mediaCanRoamWifi()); mediaWifiHandoffStarted(); assert(!wifiHandoffPending);
#endif

    // True 32-bit millis/PCM wraparound for the production progress tracker.
    RadioStreamHealth health;
    using Result = RadioStreamHealth::Result;
    const uint32_t start = UINT32_MAX - 5000;
    assert(health.update(true, start, UINT32_MAX) == Result::Waiting);
    assert(health.update(true, uint32_t(start + 14999), UINT32_MAX) == Result::Waiting);
    assert(health.update(true, uint32_t(start + 15000), UINT32_MAX) == Result::Stalled);
    assert(health.update(true, uint32_t(start + 15001), 0) == Result::Waiting);
    assert(health.update(false, uint32_t(start + 50000), 0) == Result::Waiting);
    assert(health.update(true, uint32_t(start + 60000), 0) == Result::Waiting);
    puts("Station recovery: playback timeout, PCM stall, stale events, failed-station replacement, backoff, Wi-Fi and rollover passed.");
#if defined(RADIO_SPOTIFY_EXPERIMENT)
    puts("Spotify handoff: release timeout, output restore failure, replacement and stop passed.");
#endif
}
'''
(out / 'recovery.cpp').write_text(fixture)
for spotify in (False, True):
    subprocess.run([
        'clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-g',
        *(['-DRADIO_SPOTIFY_EXPERIMENT=1'] if spotify else []),
        '-fsanitize=address,undefined', '-I', str(root / 'include'), '-I', str(root / 'tools/native_programs'),
        str(out / 'recovery.cpp'), '-o', str(out / 'recovery')
    ], check=True)
    subprocess.run([str(out / 'recovery')], check=True)
