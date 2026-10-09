#pragma once

#include <cstdint>

// Stable persisted IDs. Both artworks ship in every firmware image.
enum class BootMode : uint8_t {
    Original = 0,
    NewArtwork = 1,
};

// Preserve the former RADIOHEAD_NEW_BOOT_SCREEN=1 production default.
constexpr BootMode DEFAULT_BOOT_MODE = BootMode::NewArtwork;

constexpr bool isSupportedBootMode(BootMode mode) {
    return mode == BootMode::Original || mode == BootMode::NewArtwork;
}
