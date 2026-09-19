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

void mediaBegin();
void mediaTick(unsigned long now);
void parseM3UPro(const String& playlistUrl);
String parseM3U(const String& url);
bool loadPodcastEpisodes(int showIndex);
void playPodcastEpisode(int showIndex, int episodeIndex);
void playStation(int stationIndex);
void stopStationPlayback();
void setRadioVolumeIndex(int volumeIndex);
void toggleRadioMute();
bool isStationMuted();
int playableStationCount();
int playableStationSlotAt(int visibleIndex);
int selectedPlayableStationIndex();
int adjacentPlayableStationSlot(int stationIndex, int direction);
PlaybackState mediaPlaybackState();
int mediaRequestedStation();
int mediaPlayingStation();
