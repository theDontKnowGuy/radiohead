#pragma once

#include <Arduino.h>

constexpr int PODCAST_SHOW_COUNT = 10;
constexpr int MAX_EPISODES = 8;
constexpr size_t PODCAST_NAME_BYTES = 160;
constexpr size_t PODCAST_SOURCE_BYTES = 512;

struct PodcastShow {
    String favoriteId; // Stable identity; never a filtered row or reusable slot.
    uint8_t favoriteSlot = 0;
    String webName;
    String tftName;
    String program;    // Omny program/playlist slugs, retained for existing shows.
    String playlist;
    String feedUrl;    // Nonempty for RSS 2.0 sources.
};

bool isSavedPodcastShow(int slot);
bool validPodcastShow(const PodcastShow& show);
bool validPodcastSourceUrl(const String& url);
int savedPodcastShowCount();
uint32_t podcastCatalogRevision();
void loadPodcastCatalog(); // After loading legacy favorites.
bool savePodcastCatalog();
// Commit storage before publishing RAM. Source changes replace identity; renames
// retain it. Favorite is an explicit choice for the resulting identity. Callers
// invalidate media caches on success.
bool savePodcastShow(int slot, const PodcastShow& draft, bool favorite);
bool removePodcastShow(int slot);
