#pragma once
// Device state substitutes for compiling the real controller on the host.
#include <algorithm>
#include <cstdlib>
using std::max;
#define constrain(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))
constexpr int STATION_COUNT = 10;
constexpr int PODCAST_SHOW_COUNT = 10;
constexpr int MAX_EPISODES = 8;
extern int currentStationIdx;
extern int podcastEpisodeCount;
inline unsigned long millis() { return 0; }
