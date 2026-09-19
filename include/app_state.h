#pragma once

#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <Preferences.h>
#include <WebServer.h>
#include <vector>

#include "Audio.h"

constexpr int I2S_BCK = 15;
constexpr int I2S_DIN = 16;
constexpr int I2S_LRC = 17;
constexpr int PIN_A = 4;
constexpr int PIN_B = 5;
constexpr int PIN_K0 = 6;
constexpr int PIN_SW = 7;
constexpr int TFT_BLK = 3;
constexpr int TFT_SCLK = 12;
constexpr int TFT_MOSI = 11;
constexpr int TFT_MISO = 13;
constexpr int TFT_DC = 9;
constexpr int TFT_CS = 8;
constexpr int TFT_RST = 10;
constexpr int TOUCH_CS = 14;

constexpr int STATION_COUNT = 10;
constexpr int PODCAST_SHOW_COUNT = 10;
constexpr int MAX_EPISODES = 8;

struct RadioStation {
    String name;
    String url;
};

struct Skin {
    uint16_t bgTop;
    uint16_t bgBottom;
    uint16_t textMain;
    uint16_t textAccent;
    uint16_t wifiSig;
    uint16_t selMode;
    uint16_t clk;
    uint16_t hInfo;
    uint16_t barLow;
    uint16_t barMid;
    uint16_t barHigh;
    uint16_t volBar;
    uint16_t almWarn;
    String hexTop;
    String hexBottom;
    String hexMain;
    String hexAccent;
    String hexWifi;
    String hexSel;
    String hexClk;
    String hexHInfo;
    String hexBarL;
    String hexBarM;
    String hexBarH;
    String hexVol;
    String hexAlm;
};

struct PodcastShow {
    const char* webName;
    const char* tftName;
    const char* program;
    const char* playlist;
};

struct PodcastEpisode {
    String title;
    String publishedUtc;
    String audioUrl;
};

// The XPT2046 panel on this device can provide stable coordinates for a light
// finger contact while its calculated resistance value remains zero.  Keep the
// controller's multi-sample coordinate validation but do not use that derived
// pressure value as a binary gate.
class LightTouchXPT2046 final : public lgfx::Touch_XPT2046 {
public:
    uint_fast8_t getTouchRaw(lgfx::touch_point_t* point, uint_fast8_t count) override;
};

class LGFX_Config : public lgfx::LGFX_Device {
public:
    LGFX_Config();

private:
    lgfx::Panel_ILI9341 panel_;
    lgfx::Bus_SPI bus_;
    LightTouchXPT2046 touch_;
};

extern const char* ntpServer;
extern RadioStation stations[STATION_COUNT];
extern std::vector<RadioStation> m3uTempList;
extern Skin currentSkin;
extern const PodcastShow podcastShows[PODCAST_SHOW_COUNT];
extern PodcastEpisode podcastEpisodes[MAX_EPISODES];
extern const uint8_t volCurve[22];

extern LGFX_Config tft;
extern Audio audio;
extern WebServer server;
extern Preferences pref;

extern int visualMode;
extern String owmKey;
extern String owmCity;
extern float tempC;
extern int weatherID;
extern bool useCelsius;
extern int currentStationIdx;
extern int tempStationIdx;
extern int mainVal;
extern bool radioMuted;
// A station favorite is identified by its persistent catalog slot, never by a
// filtered list row.  Only the low STATION_COUNT bits are meaningful.
extern uint16_t stationFavoriteMask;
extern int podcastEpisodeCount;
extern int loadedPodcastShow;
extern bool podcastMode;
extern String podcastShowTft;
extern int gB;
extern int gM;
extern int gT;
extern bool showSpectrum;
extern bool isDimmed;
extern volatile bool forceRedraw;
extern volatile unsigned long lastInteraction;
extern unsigned long lastVolChange;
extern volatile int encoderPos;
extern String songTitle;
extern String lastDrawnSong;
extern String lastDrawnStationName;
extern int lastMain;
extern int lastDrawnMode;
extern int lastDrawnStationIdx;
extern int barHeights[2];
extern int alarmH;
extern int alarmM;
extern bool alarmActive;
extern bool isAlarming;
extern String lastDrawnTime;
extern int alarmVolume;
extern unsigned long lastAlarmStep;
extern unsigned long alarmStartMillis;
extern String st_ssid;
extern String st_pass;
extern bool isAP;
