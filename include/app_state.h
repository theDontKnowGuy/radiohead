#pragma once

#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <Preferences.h>
#include <WebServer.h>
#include <vector>

#include "Audio.h"
#include "boot_mode.h"

constexpr int I2S_BCK = 15;
constexpr int I2S_DIN = 16;
constexpr int I2S_LRC = 17;
// Keep the encoder off JTAG GPIO39/40 and the possible RGB LED pin GPIO38.
// GPIO35-37 remain reserved for octal PSRAM on the N16R8 target.
constexpr int PIN_A = 4;
constexpr int PIN_B = 7;
// Separate power/wake button; retained alongside encoder light-sleep wake.
constexpr int PIN_K0 = 6;
// Encoder switch/light-sleep wake; preserve the existing sleep behavior.
constexpr int PIN_SW = 13;
// Backlight PWM uses a non-strapping RTC GPIO for the sleep hold.
constexpr int TFT_BLK = 1;
constexpr int TFT_SCLK = 12;
constexpr int TFT_MOSI = 11;
// Shared SPI2 input comes from touch T_DO; display SDO stays disconnected.
constexpr int TFT_MISO = 5;
constexpr int TFT_DC = 9;
constexpr int TFT_CS = 8;
constexpr int TFT_RST = 10;
constexpr int TOUCH_SCLK = TFT_SCLK;
constexpr int TOUCH_MOSI = TFT_MOSI;
constexpr int TOUCH_MISO = TFT_MISO;
constexpr int TOUCH_CS = 14;

constexpr int STATION_COUNT = 10;
constexpr int PODCAST_SHOW_COUNT = 10;
constexpr int MAX_EPISODES = 8;

// Network identity is intentionally fixed: the QR badges are generated from
// these values and the mDNS responder must use the same hostname.
constexpr const char* kRadioMdnsHostname = "radio";
constexpr const char* kRadioMdnsAddress = "radio.local";
constexpr const char* kSetupAccessPointSsid = "Radio_Setup";

enum class SetupAccessReason : uint8_t {
    Requested,
    NoCredentials,
    ConnectionFailed,
};

struct RadioStation {
    String name;
    String url;
};

struct PodcastShow {
    // This is a stable catalog identity, not a current list position. It is
    // deliberately separate from the display name used by the UI.
    const char* favoriteId;
    uint8_t favoriteSlot;
    const char* webName;
    const char* tftName;
    const char* program;
    const char* playlist;
};

struct PodcastEpisode {
    String id;
    String title;
    String publishedUtc;
    String audioUrl;
    uint32_t durationSeconds = 0;
};

// The XPT2046 panel on this device can provide stable coordinates for a light
// finger contact while its calculated resistance value remains zero.  Keep the
// drivers on for consecutive same-axis readings, discard settling conversions,
// and validate coordinates without using derived pressure as a binary gate.
class LightTouchXPT2046 final : public lgfx::Touch_XPT2046 {
public:
    uint_fast8_t getTouchRaw(lgfx::touch_point_t* point, uint_fast8_t count) override;
};

// Stock acquisition with loop-owned diagnostics; no sensitivity tuning.
class DiagnosticTouchXPT2046 final : public lgfx::Touch_XPT2046 {
public:
    bool init() override;
    bool recover();
    uint_fast8_t getTouchRaw(lgfx::touch_point_t* point, uint_fast8_t count) override;
};

class LGFX_Config : public lgfx::LGFX_Device {
public:
    LGFX_Config();
    bool recoverTouch();

private:
    lgfx::Panel_ILI9341 panel_;
    lgfx::Bus_SPI bus_;
    // Preserve the stock reader's pressure gate and report its actual frames.
    // The custom sampler above is retained for comparison but is inactive.
    DiagnosticTouchXPT2046 touch_;
};

extern const char* ntpServer;
extern RadioStation stations[STATION_COUNT];
extern const PodcastShow podcastShows[PODCAST_SHOW_COUNT];
extern PodcastEpisode podcastEpisodes[MAX_EPISODES];
extern const uint8_t volCurve[22];

extern LGFX_Config tft;
extern Audio audio;
extern WebServer server;
extern Preferences pref;

// Active for this boot; shared by rendering and future mode-specific branding.
extern BootMode bootMode;
// Durable selection for the next boot. Saving never changes the active mode.
extern BootMode configuredBootMode;

extern String owmKey;
extern String owmCity;
extern float tempC;
extern int weatherID;
extern bool useCelsius;
// Weather and clock settings are shared by the web and TFT renderers.  The
// timezone ID is resolved through the bounded rule table in settings.cpp.
extern bool showWeatherOnHome;
extern bool use24HourClock;
extern String timeZoneId;
extern String spotifyConnectName;
extern int currentStationIdx;
extern int tempStationIdx;
extern int mainVal;
extern bool radioMuted;
// A station favorite is identified by its persistent catalog slot, never by a
// filtered list row.  Only the low STATION_COUNT bits are meaningful.
extern uint16_t stationFavoriteMask;
extern uint16_t podcastShowFavoriteMask;
extern int podcastEpisodeCount;
extern int loadedPodcastShow;
extern bool podcastMode;
extern String podcastShowTft;
extern int gB;
extern int gM;
extern int gT;
// Automatic dimming is a small, device-local preference.  It is expressed in
// seconds so both the settings UI and the main-loop deadline avoid hidden
// presentation-unit conversions.
extern uint16_t autoDimSeconds;
extern bool isDimmed;
extern volatile bool forceRedraw;
extern volatile unsigned long lastInteraction;
extern unsigned long lastVolChange;
extern volatile int encoderPos;
extern String songTitle;
extern String st_ssid;
extern String st_pass;
extern bool isAP;
// GitHub release updates default to automatic installation. The owner can
// switch to ask-first mode from Device & maintenance.
extern bool firmwareAutoUpdate;
extern SetupAccessReason setupAccessReason;
