#pragma once

#include <Arduino.h>

// Playback is intentionally separate from the selected catalog slot. A station
// can be requested without being confirmed as playing yet.
enum class PlaybackState : uint8_t {
    Stopped,
    Connecting,
    Playing,
    Failed,
};

enum class MediaSource : uint8_t { None, Radio, Podcast, Spotify };
enum class MediaStatus : uint8_t { Stopped, Connecting, Playing, Paused, Failed };

struct MediaSnapshot {
    MediaSource source = MediaSource::None;
    MediaStatus status = MediaStatus::Stopped;
    String name;
    String title;
    String artist;
    String album;
    String artworkIdentity;
    uint32_t revision = 0;
    bool canPause = false;
    bool canPrevious = false;
    bool canNext = false;
};

// Read on the Arduino loop, after mediaTick consumes worker events.
const MediaSnapshot& mediaSnapshot();

enum class PodcastLoadState : uint8_t {
    Idle,
    Loading,
    Ready,
    Failed,
};

struct PodcastPlaybackSnapshot {
    bool active = false;
    bool paused = false;
    int showIndex = -1;
    int episodeIndex = -1;
    uint32_t elapsedSeconds = 0;
    uint32_t durationSeconds = 0;
    bool canPause = false;
    bool canSeek = false;
    bool controlError = false;
};

struct PodcastEpisode;

void mediaBegin();
void mediaTick(unsigned long now);
bool mediaLocalAudioAvailable();
String parseM3U(const String& url);
// Starts a single bounded HTTPS job. Completion is published by mediaTick();
// callers must not mutate the fetched collection or call Audio from a worker.
bool requestPodcastEpisodes(int showIndex);
PodcastLoadState podcastLoadState();
int podcastRequestedShow();
bool podcastEpisodesReadyFor(int showIndex);
bool playPodcastEpisode(int showIndex, int episodeIndex);
bool togglePodcastPause();
bool seekPodcastBySeconds(int seconds);
PodcastPlaybackSnapshot podcastPlaybackSnapshot();
// Playback keeps its own copy because the browse cache is replaced whenever a
// different show is opened.
const PodcastEpisode* podcastActiveEpisode();
void playStation(int stationIndex);
// Explicit web test playback is intentionally separate from the saved catalog.
bool startStationTest(const String& name, const String& url);
bool mediaTestActive();
String mediaTestName();
void stopStationPlayback();
void setRadioVolumeIndex(int volumeIndex);
void toggleRadioMute();
// Apply/preview tone without changing committed settings. Published to PCM tasks.
void mediaSetTone(int bass, int mid, int treble);
// PCM-task bridge for the pinned local EQ; not a main-loop settings API.
void mediaApplyLocalTone(int bass, int mid, int treble);
bool isStationMuted();
// v4 delivers VU data through the audio callback; this is the latest combined
// channel level normalized to the 0..255 range used by the renderer.
int playableStationCount();
int playableStationSlotAt(int visibleIndex);
int selectedPlayableStationIndex();
int adjacentPlayableStationSlot(int stationIndex, int direction);
PlaybackState mediaPlaybackState();
int mediaRequestedStation();
int mediaPlayingStation();

// Startup-noise experiment: quiet the I2S pins before other setup work.
// Call only before I2S owns the pins; this does not control amplifier SD_MODE.
void mediaPrepareBootOutput();
// Configure local I2S before boot audio, keeping experiment output at zero.
void mediaConfigureOutput();
// Main-loop settings publication; PCM tasks keep independent filter histories.
void mediaSetNotchFilter(bool enabled, int frequencyHz, int endHz = 0);
// Local output uses the source rate; called once per PCM block by its task.
uint32_t mediaLocalSampleRate();
// Link the application PCM hooks rather than the library's weak no-op defaults.
void mediaInitializeAudioFilters();
// Set boot volume to 10 once the sound has opened, preserving saved playback volume.
void mediaEnableBootOutput();
// Initialize media state after boot audio and start the saved station outside AP mode.
void mediaStartSavedPlayback();
