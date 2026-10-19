#pragma once

// Shared by local playback and Spotify, which take turns owning the I2S bus.
// These outputs occupy external JTAG pins; USB remains on GPIO19/20.
constexpr int I2S_BCK = 39;
constexpr int I2S_DIN = 40;
constexpr int I2S_LRC = 41;
