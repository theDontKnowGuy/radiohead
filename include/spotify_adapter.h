#pragma once

#include <stdint.h>

// Only the experimental ESP-IDF image links the native Connect service.
enum class SpotifySignalType : uint8_t { None, Activate, Pause, Stop, Volume };
struct SpotifySignal {
    SpotifySignalType type = SpotifySignalType::None;
    uint32_t sequence = 0;
    uint16_t volume = 0;
};

#if defined(RADIO_SPOTIFY_EXPERIMENT)
#include <map>
#include <string>
#include <Arduino.h>
void spotifyAdapterBegin();
String spotifyAdapterInfoJson();
bool spotifyAdapterPairingSubmit(const std::map<std::string, std::string>& fields);
SpotifySignal spotifyAdapterTakeSignal();
bool spotifyAdapterAcquireOutput(uint8_t volume, uint32_t timeoutMs);
bool spotifyAdapterReleaseOutput(uint32_t timeoutMs);
void spotifyAdapterSetOutputVolume(uint8_t volume);
void spotifyAdapterSetTone(int8_t bassDb, int8_t middleDb, int8_t trebleDb);
void spotifyAdapterDiscardPending();
bool spotifyAdapterReady();
#endif
