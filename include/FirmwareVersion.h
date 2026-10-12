#pragma once

// The release script reads these values before it builds and publishes a
// release. Keep this as the single source of truth for both the compiled image
// and GitHub's manifest.
#define FIRMWARE_VERSION "0.2.1"
#define FIRMWARE_RELEASED "2026-10-12"
#define FIRMWARE_NOTES "Increase encoder sleep hold to 3 seconds. Enable encoder and K0 wake using light sleep, then restore the radio through normal startup. Physical sleep/wake validation is pending."

// A release manifest can contain images for several boards. This target's
// build key is declared in platformio.ini and must match its manifest entry.
#ifndef FIRMWARE_BUILD
#define FIRMWARE_BUILD "unknown"
#endif
