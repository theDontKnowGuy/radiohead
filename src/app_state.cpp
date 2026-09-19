#include "app_state.h"

#include "xpt2046_sampling.h"

uint_fast8_t LightTouchXPT2046::getTouchRaw(lgfx::touch_point_t* point, uint_fast8_t count) {
    if (!point || count == 0 || !_inited) return 0;
    point->size = 0;

    uint8_t data[xpt2046_sampling::kFrameBytes];
    xpt2046_sampling::prepare(data);

    lgfx::spi::beginTransaction(_cfg.spi_host, _cfg.freq, 0);
    if (_cfg.pin_cs >= 0) lgfx::gpio_lo(_cfg.pin_cs);
    lgfx::spi::readBytes(_cfg.spi_host, data, sizeof(data));
    if (_cfg.pin_cs >= 0) lgfx::gpio_hi(_cfg.pin_cs);
    lgfx::spi::endTransaction(_cfg.spi_host);

    uint16_t x = 0;
    uint16_t y = 0;
    if (!xpt2046_sampling::readAxis(data, y) ||
        !xpt2046_sampling::readAxis(data + xpt2046_sampling::kAxisBytes, x)) return 0;
    point->x = x;
    point->y = y;
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
    {"arba/podcast", 0, "ארבע אחרי הצהריים", "Arba Aharei Hatzohorayim", "arba", "podcast"},
    {"ilanadayan/podcast", 1, "אילנה דיין", "Ilana Dayan", "ilanadayan", "podcast"},
    {"beseder/podcast", 2, "יהיה בסדר", "Yihye Beseder", "beseder", "podcast"},
    {"hamesh/playlist-1", 3, "חמש בערב", "Hamesh Baerev", "hamesh", "playlist-1"},
    {"rinozror/podcast", 4, "רינו צרור", "Rino Zror", "rinozror", "podcast"},
    {"goodmorningisrael/podcast", 5, "בוקר טוב ישראל", "Boker Tov Israel", "goodmorningisrael", "podcast"},
    {"yoman/podcast", 6, "יומן הצהריים", "Yoman Hatzohorayim", "yoman", "podcast"},
    {"galivri/podcast", 7, "גל עברי ירוק", "Gal Ivri Yarok", "galivri", "podcast"},
    {"israelinight/podcast", 8, "לילה ישראלי", "Laila Israeli", "israelinight", "podcast"},
    {"giluydaat/podcast", 9, "גילוי דעת", "Giluy Daat", "giluydaat", "podcast"},
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
    // More acquisition time for light/high-resistance contact. The bounded
    // 41-byte burst takes ~1.31 ms at 250 kHz; the display clock is unchanged.
    touchConfig.freq = 250000;
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
uint16_t podcastShowFavoriteMask = 0;
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
