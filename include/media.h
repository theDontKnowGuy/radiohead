#pragma once

#include <Arduino.h>

void parseM3UPro(const String& playlistUrl);
String parseM3U(const String& url);
bool loadPodcastEpisodes(int showIndex);
void playPodcastEpisode(int showIndex, int episodeIndex);
void playStation(int stationIndex);

