#pragma once

#include <cstdint>

// Stable persisted IDs. Both artworks ship in every firmware image.
enum class BootMode : uint8_t {
    Original = 0,
    NewArtwork = 1,
};

// New installs and factory resets start with the original screen (mode 0).
constexpr BootMode DEFAULT_BOOT_MODE = BootMode::Original;

constexpr bool isSupportedBootMode(BootMode mode) {
    return mode == BootMode::Original || mode == BootMode::NewArtwork;
}
