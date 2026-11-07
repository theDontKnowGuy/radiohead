#pragma once
#include <cstdint>
#include <cstring>
// Stable route table v1. Append new paths; never reorder existing IDs.
// Unknown paths export 0; no URL/query/body/user data is retained.
inline constexpr const char* RadioWebRoutes[] = {
    "/api/programs", // 1
    "/api/programs/save", // 2
    "/api/programs/remove", // 3
    "/api/programs/favorite", // 4
    "/api/programs/preview", // 5
    "/api/radio/crash", // 6
    "/api/radio/crash/clear", // 7
    "/api/radio/crash/test", // 8
    "/api/radio/diagnostics/roaming", // 9
    "/api/radio/diagnostics/config", // 10
    "/api/radio/diagnostics", // 11
    "/api/settings/backup", // 12
    "/api/settings/restore", // 13
    "/api/spotify/resources", // 14
    "/api/spotify/diagnostics", // 15
    "/spotify_info", // 16
    "/", // 17
    "/stations", // 18
    "/audio", // 19
    "/spotify", // 20
    "/network", // 21
    "/weather", // 22
    "/appearance", // 23
    "/device", // 24
    "/skin", // 25
    "/update_ui", // 26
    "/favicon.png", // 27
    "/ui/radiohead.css", // 28
    "/ui/assets/coast.jpg", // 29
    "/api/network", // 30
    "/api/network/scan/start", // 31
    "/api/network/scan", // 32
    "/api/network/connect", // 33
    "/api/network/retry", // 34
    "/api/network/forget", // 35
    "/api/weather", // 36
    "/api/device", // 37
    "/api/device/encoder-debug", // 38
    "/api/device/boot-mode", // 39
    "/api/device/spotify-credentials", // 40
    "/api/device/spotify-name", // 41
    "/api/update/status", // 42
    "/api/update/check", // 43
    "/api/update/install", // 44
    "/api/update/auto", // 45
    "/api/device/restart", // 46
    "/api/device/touch-calibration", // 47
    "/api/device/touch-debug", // 48
    "/api/device/touch-recover", // 49
    "/api/device/factory-reset", // 50
    "/api/device/ota", // 51
    "/api/weather/save", // 52
    "/api/player", // 53
    "/api/player/volume", // 54
    "/api/player/mute", // 55
    "/api/player/tone", // 56
    "/api/audio", // 57
    "/api/stations", // 58
    "/api/stations/artwork", // 59
    "/api/stations/play", // 60
    "/api/stations/save", // 61
    "/api/stations/favorite", // 62
    "/api/stations/remove", // 63
    "/api/stations/test", // 64
    "/programs", // 65
    "/episodes", // 66
    "/togglespec", // 67
    "/setVisual", // 68
    "/setvol", // 69
    "/seteq", // 70
    "/setpreset", // 71
    "/setweather", // 72
    "/setwifi", // 73
    "/apply_manual", // 74
    "/edit", // 75
    "/scan", // 76
    "/scan_data", // 77
    "/setalarm", // 78
    "/off", // 79
    "/prev", // 80
    "/next", // 81
    "/favorite", // 82
    "/update", // 83
    "/setskin", // 84
    "/defaultskin", // 85
    "/playepisode", // 86
    "/api/radio/crash/test-partition", // 87 (bench-only, no raw data in normal firmware)
};
inline uint32_t radioWebRouteId(const char* path) {
    for (uint32_t i = 0; i < sizeof(RadioWebRoutes) / sizeof(RadioWebRoutes[0]); ++i)
        if (strcmp(path, RadioWebRoutes[i]) == 0) return i + 1;
    return 0;
}
