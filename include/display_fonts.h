#pragma once

#include <lgfx/v1/LGFXBase.hpp>

// Loaded once; shared by the production memory canvas and native font fixture.
namespace display_fonts {
bool init();
const lgfx::IFont* label();
const lgfx::IFont* caption();
const lgfx::IFont* homeTitle();
const lgfx::IFont* smooth(const lgfx::IFont* bitmap);
}
