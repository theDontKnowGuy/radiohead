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
void parseM3UPro(const String& playlistUrl);
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
bool isStationMuted();
// v4 delivers VU data through the audio callback; this is the latest combined
// channel level normalized to the 0..255 range used by the renderer.
uint8_t mediaVuLevel();
int playableStationCount();
int playableStationSlotAt(int visibleIndex);
int selectedPlayableStationIndex();
int adjacentPlayableStationSlot(int stationIndex, int direction);
PlaybackState mediaPlaybackState();
int mediaRequestedStation();
int mediaPlayingStation();
