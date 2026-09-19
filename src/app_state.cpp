#include "app_state.h"

#include <climits>
#include <cstring>

namespace {

constexpr int kMinimumLightTouchSamples = 2;
constexpr int kMaximumLightTouchPairDelta = 300;

struct RawTouchSample {
    uint16_t x;
    uint16_t y;
};

}  // namespace

uint_fast8_t LightTouchXPT2046::getTouchRaw(lgfx::touch_point_t* point, uint_fast8_t count) {
    if (!point || count == 0 || !_inited) return 0;
    point->size = 0;

    uint8_t data[57];
    memset(data, 0, 8);
    data[0] = 0x91;
    data[2] = 0xB1;
    data[4] = 0xD1;
    data[6] = 0xC1;
    data[56] = 0x80;
    memcpy(&data[8], data, 8);
    memcpy(&data[16], data, 16);
    memcpy(&data[32], data, 24);

    lgfx::spi::beginTransaction(_cfg.spi_host, _cfg.freq, 0);
    if (_cfg.pin_cs >= 0) lgfx::gpio_lo(_cfg.pin_cs);
    lgfx::spi::readBytes(_cfg.spi_host, data, sizeof(data));
    if (_cfg.pin_cs >= 0) lgfx::gpio_hi(_cfg.pin_cs);
    lgfx::spi::endTransaction(_cfg.spi_host);

    RawTouchSample samples[7];
    size_t sampleCount = 0;
    for (size_t sample = 0; sample < 7; ++sample) {
        const uint8_t* values = &data[sample * 8];
        const int x = (values[5] << 8 | values[6]) >> 3;
        const int y = (values[1] << 8 | values[2]) >> 3;
        if (x > 128 && x <= 3968 && y > 128 && y <= 3968) {
            samples[sampleCount++] = {
                static_cast<uint16_t>(x),
                static_cast<uint16_t>(y),
            };
        }
    }
    // A light finger may produce only two valid readings.  Accept that pair
    // only if both coordinates agree closely, which preserves a guard against
    // floating-MISO noise while removing the stock pressure requirement.
    if (sampleCount < kMinimumLightTouchSamples) return 0;

    int bestPairDelta = INT_MAX;
    size_t first = 0;
    size_t second = 0;
    for (size_t left = 0; left + 1 < sampleCount; ++left) {
        for (size_t right = left + 1; right < sampleCount; ++right) {
            const int delta = abs(static_cast<int>(samples[left].x) - samples[right].x) +
                abs(static_cast<int>(samples[left].y) - samples[right].y);
            if (delta < bestPairDelta) {
                bestPairDelta = delta;
                first = left;
                second = right;
            }
        }
    }
    if (bestPairDelta > kMaximumLightTouchPairDelta) return 0;

    point->x = (samples[first].x + samples[second].x) / 2;
    point->y = (samples[first].y + samples[second].y) / 2;
    point->size = 1;
    return 1;
}

const char* ntpServer = "pool.ntp.org";

RadioStation stations[STATION_COUNT] = {
    {"Dance Wave Retro", "http://dancewave.online/retrodance.mp3"},
    {"Empty Slot 2", ""},
    {"Empty Slot 3", ""},
    {"Empty Slot 4", ""},
    {"Empty Slot 5", ""},
    {"Empty Slot 6", ""},
    {"Empty Slot 7", ""},
    {"Empty Slot 8", ""},
    {"Empty Slot 9", ""},
    {"Empty Slot 10", ""},
};

std::vector<RadioStation> m3uTempList;
Skin currentSkin;

const PodcastShow podcastShows[PODCAST_SHOW_COUNT] = {
    {"ארבע אחרי הצהריים", "Arba Aharei Hatzohorayim", "arba", "podcast"},
    {"אילנה דיין", "Ilana Dayan", "ilanadayan", "podcast"},
    {"יהיה בסדר", "Yihye Beseder", "beseder", "podcast"},
    {"חמש בערב", "Hamesh Baerev", "hamesh", "playlist-1"},
    {"רינו צרור", "Rino Zror", "rinozror", "podcast"},
    {"בוקר טוב ישראל", "Boker Tov Israel", "goodmorningisrael", "podcast"},
    {"יומן הצהריים", "Yoman Hatzohorayim", "yoman", "podcast"},
    {"גל עברי ירוק", "Gal Ivri Yarok", "galivri", "podcast"},
    {"לילה ישראלי", "Laila Israeli", "israelinight", "podcast"},
    {"גילוי דעת", "Giluy Daat", "giluydaat", "podcast"},
};

PodcastEpisode podcastEpisodes[MAX_EPISODES];
const uint8_t volCurve[22] = {
    0, 1, 2, 3, 4, 5, 6, 8, 10, 12, 14, 17, 20, 23, 26, 30, 34, 38, 42, 46, 50, 55,
};

LGFX_Config::LGFX_Config() {
    auto busConfig = bus_.config();
    busConfig.spi_host = SPI2_HOST;
    busConfig.pin_sclk = TFT_SCLK;
    busConfig.pin_mosi = TFT_MOSI;
    busConfig.pin_miso = TFT_MISO;
    busConfig.pin_dc = TFT_DC;
    bus_.config(busConfig);
    panel_.setBus(&bus_);

    auto panelConfig = panel_.config();
    panelConfig.pin_cs = TFT_CS;
    panelConfig.pin_rst = TFT_RST;
    panelConfig.panel_width = 240;
    panelConfig.panel_height = 320;
    panelConfig.bus_shared = true;
    panel_.config(panelConfig);

    auto touchConfig = touch_.config();
    touchConfig.spi_host = SPI2_HOST;
    touchConfig.pin_sclk = TFT_SCLK;
    touchConfig.pin_mosi = TFT_MOSI;
    touchConfig.pin_miso = TFT_MISO;
    touchConfig.pin_cs = TOUCH_CS;
    touchConfig.pin_int = -1;
    touchConfig.freq = 1000000;
    touchConfig.x_min = 300;
    touchConfig.x_max = 3900;
    touchConfig.y_min = 400;
    touchConfig.y_max = 3900;
    touchConfig.bus_shared = true;
    touchConfig.offset_rotation = 0;
    touch_.config(touchConfig);
    panel_.setTouch(&touch_);

    setPanel(&panel_);
}

LGFX_Config tft;
Audio audio;
WebServer server(80);
Preferences pref;

int visualMode = 3;
String owmKey;
String owmCity;
float tempC = 0.0F;
int weatherID = 800;
bool useCelsius = true;
int currentStationIdx = 0;
int tempStationIdx = 0;
int mainVal = 5;
bool radioMuted = false;
uint16_t stationFavoriteMask = 0;
int podcastEpisodeCount = 0;
int loadedPodcastShow = -1;
bool podcastMode = false;
String podcastShowTft;
int gB = 0;
int gM = 0;
int gT = 0;
bool showSpectrum = true;
bool isDimmed = false;
volatile bool forceRedraw = false;
volatile unsigned long lastInteraction = 0;
unsigned long lastVolChange = 0;
volatile int encoderPos = 0;
String songTitle;
String lastDrawnSong = "INIT";
String lastDrawnStationName = "INIT";
int lastMain = -999;
int lastDrawnMode = -1;
int lastDrawnStationIdx = -1;
int barHeights[2] = {0, 0};
int alarmH = 7;
int alarmM = 0;
bool alarmActive = false;
bool isAlarming = false;
String lastDrawnTime;
int alarmVolume = 0;
unsigned long lastAlarmStep = 0;
unsigned long alarmStartMillis = 0;
String st_ssid;
String st_pass;
bool isAP = false;
