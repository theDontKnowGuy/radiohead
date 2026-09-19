#include "app_state.h"

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
