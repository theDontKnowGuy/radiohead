#pragma once

// The release script reads these values before it builds and publishes a
// release. Keep this as the single source of truth for both the compiled image
// and GitHub's manifest.
#define FIRMWARE_VERSION "0.2.4"
#define FIRMWARE_RELEASED "2026-10-22"
#define FIRMWARE_NOTES "OTA uploads are now supported. The firmware can be updated over the air using the web interface. This release also includes several bug fixes and performance improvements."

// A release manifest can contain images for several boards. This target's
// build key is declared in platformio.ini and must match its manifest entry.
#ifndef FIRMWARE_BUILD
#define FIRMWARE_BUILD "unknown"
#endif
