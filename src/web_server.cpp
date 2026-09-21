#include "web_server.h"

#include <Update.h>
#include <WiFi.h>
#include <cerrno>
#include <cctype>
#include <cstdlib>

#include "app_state.h"
#include "device_control.h"
#include "display.h"
#include "media.h"
#include "settings.h"
#include "ui_web_assets.h"

namespace {

constexpr size_t MAX_STATION_NAME_LENGTH = 80;
constexpr size_t MAX_URL_LENGTH = 512;
constexpr size_t MAX_M3U_LINE_LENGTH = 640;

bool otaUploadStarted = false;
bool otaUploadSucceeded = false;

String htmlEscape(const String& value) {
    String escaped;
    escaped.reserve(value.length());
    for (size_t i = 0; i < value.length(); ++i) {
        switch (value[i]) {
            case '&': escaped += F("&amp;"); break;
            case '<': escaped += F("&lt;"); break;
            case '>': escaped += F("&gt;"); break;
            case '\"': escaped += F("&quot;"); break;
            case '\'': escaped += F("&#39;"); break;
            default: escaped += value[i]; break;
        }
    }
    return escaped;
}

String jsonEscape(const String& value) {
    static constexpr char HEX_DIGITS[] = "0123456789ABCDEF";
    String escaped;
    escaped.reserve(value.length() + 8);
    for (size_t i = 0; i < value.length(); ++i) {
        const uint8_t character = static_cast<uint8_t>(value[i]);
        switch (character) {
            case '\"': escaped += F("\\\""); break;
            case '\\': escaped += F("\\\\"); break;
            case '\b': escaped += F("\\b"); break;
            case '\f': escaped += F("\\f"); break;
            case '\n': escaped += F("\\n"); break;
            case '\r': escaped += F("\\r"); break;
            case '\t': escaped += F("\\t"); break;
            default:
                if (character < 0x20) {
                    escaped += F("\\u00");
                    escaped += HEX_DIGITS[character >> 4];
                    escaped += HEX_DIGITS[character & 0x0F];
                } else {
                    escaped += static_cast<char>(character);
                }
                break;
        }
    }
    return escaped;
}

bool isHttpUrl(const String& value) {
    return value.length() <= MAX_URL_LENGTH &&
        (value.startsWith("http://") || value.startsWith("https://"));
}

bool isHexColor(const String& value) {
    if (value.length() != 7 || value[0] != '#') {
        return false;
    }
    for (size_t i = 1; i < value.length(); ++i) {
        if (!isxdigit(static_cast<unsigned char>(value[i]))) {
            return false;
        }
    }
    return true;
}

bool parseIntegerArg(const char* name, int minimum, int maximum, int& result) {
    if (!server.hasArg(name)) {
        return false;
    }
    const String value = server.arg(name);
    if (value.isEmpty()) {
        return false;
    }

    char* end = nullptr;
    errno = 0;
    const long parsed = strtol(value.c_str(), &end, 10);
    if (errno == ERANGE || end == value.c_str() || *end != '\0' ||
        parsed < minimum || parsed > maximum) {
        return false;
    }
    result = static_cast<int>(parsed);
    return true;
}

void sendBadRequest(const char* message) {
    server.send(400, "text/plain", message);
}

void redirectTo(const char* location) {
    server.sendHeader("Location", location);
    server.send(303);
}

void redirectTo(const String& location) {
    redirectTo(location.c_str());
}

String pageStart(const String& title, const String& accent = "orange") {
    return "<html><head><meta charset='utf-8'>"
           "<meta name='viewport' content='width=device-width,initial-scale=1'>"
           "<style>body{background:#121212;color:" + accent +
           ";font-family:sans-serif;text-align:center;padding:10px}"
           ".btn{background:#333;border:2px solid " + accent +
           ";color:#fff;padding:10px;margin:5px;display:inline-block;text-decoration:none;border-radius:8px}"
           ".card{background:#222;padding:15px;margin:10px auto;max-width:420px;border-radius:12px;border:1px solid " +
           accent + "}input,select{background:#222;color:#fff;padding:7px;border:1px solid #555}"
           "table{width:100%;max-width:600px;margin:auto;border-collapse:collapse}"
           "td{padding:8px;border-bottom:1px solid #444}</style></head><body><h1>" +
           htmlEscape(title) + "</h1>";
}

enum class WebSection : uint8_t { Stations, Network, Weather, Appearance, Device };

const char* sectionPath(WebSection section) {
    switch (section) {
    case WebSection::Stations: return "/";
    case WebSection::Network: return "/network";
    case WebSection::Weather: return "/weather";
    case WebSection::Appearance: return "/appearance";
    case WebSection::Device: return "/device";
    }
    return "/";
}

const char* sectionDesktopLabel(WebSection section) {
    switch (section) {
    case WebSection::Stations: return "Stations";
    case WebSection::Network: return "Network";
    case WebSection::Weather: return "Weather &amp; time";
    case WebSection::Appearance: return "Appearance";
    case WebSection::Device: return "Device &amp; maintenance";
    }
    return "";
}

const char* sectionMobileLabel(WebSection section) {
    switch (section) {
    case WebSection::Stations: return "Stations";
    case WebSection::Network: return "Network";
    case WebSection::Weather: return "Weather &amp; time";
    case WebSection::Appearance: return "Appearance";
    case WebSection::Device: return "Device";
    }
    return "";
}

const char* sectionIcon(WebSection section) {
    switch (section) {
    case WebSection::Stations: return "<svg viewBox='0 0 24 24' aria-hidden='true'><circle cx='12' cy='12' r='2'/><path d='M4.9 4.9a10 10 0 0 0 0 14.2M19.1 4.9a10 10 0 0 1 0 14.2M8 8a5.7 5.7 0 0 0 0 8M16 8a5.7 5.7 0 0 1 0 8'/></svg>";
    case WebSection::Network: return "<svg viewBox='0 0 24 24' aria-hidden='true'><path d='M5 12.5a11 11 0 0 1 14 0M8 16a6.5 6.5 0 0 1 8 0M11.3 19.4a1 1 0 1 1 1.4 0'/></svg>";
    case WebSection::Weather: return "<svg viewBox='0 0 24 24' aria-hidden='true'><circle cx='8' cy='8' r='3'/><path d='M8 1v2m0 10v2m7-7h-2M3 8H1m12 7a4 4 0 0 1 7.7 1.5A3.5 3.5 0 0 1 20 23H9.5a4.5 4.5 0 0 1 3.5-8Z'/></svg>";
    case WebSection::Appearance: return "<svg viewBox='0 0 24 24' aria-hidden='true'><rect x='3' y='4' width='18' height='16' rx='2'/><circle cx='8' cy='9' r='1.4'/><path d='m4 18 5.4-5.4 3.3 3.3 2.3-2.3L20 18'/></svg>";
    case WebSection::Device: return "<svg viewBox='0 0 24 24' aria-hidden='true'><path d='M12 2v4m0 12v4M4.9 4.9l2.8 2.8m8.6 8.6 2.8 2.8M2 12h4m12 0h4M4.9 19.1l2.8-2.8m8.6-8.6 2.8-2.8'/><circle cx='12' cy='12' r='4'/></svg>";
    }
    return "";
}

String connectionState() {
    if (isAP) return "Setup mode";
    if (WiFi.status() == WL_CONNECTED) return "Connected";
    return st_ssid.isEmpty() ? "Not configured" : "Disconnected";
}

String localAddress() {
    return isAP ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
}

String currentPlaybackName() {
    if (podcastMode) {
        const PodcastEpisode* episode = podcastActiveEpisode();
        if (episode != nullptr && !episode->title.isEmpty()) return episode->title;
    }
    if (currentStationIdx >= 0 && currentStationIdx < STATION_COUNT &&
        !stations[currentStationIdx].name.isEmpty()) {
        return stations[currentStationIdx].name;
    }
    return "Nothing playing";
}

String playbackLabel() {
    switch (mediaPlaybackState()) {
    case PlaybackState::Connecting: return "Connecting…";
    case PlaybackState::Playing: return isStationMuted() ? "Muted" : "Now playing";
    case PlaybackState::Failed: return "Stream error";
    case PlaybackState::Stopped: return "Nothing playing";
    }
    return "Nothing playing";
}

uint32_t playerRevision() {
    // This is a deterministic optimistic-concurrency token for the small,
    // shared player state.  It changes for encoder/TFT as well as web edits,
    // without persisting an artificial revision counter.
    uint32_t revision = 2166136261UL;
    const uint8_t values[] = {
        static_cast<uint8_t>(mainVal),
        static_cast<uint8_t>(radioMuted),
        static_cast<uint8_t>(gB + 15),
        static_cast<uint8_t>(gM + 15),
        static_cast<uint8_t>(gT + 15),
    };
    for (const uint8_t value : values) {
        revision ^= value;
        revision *= 16777619UL;
    }
    return revision & 0x7fffffffUL;
}

const char* playbackStateName() {
    switch (mediaPlaybackState()) {
    case PlaybackState::Connecting: return "connecting";
    case PlaybackState::Playing: return radioMuted ? "muted" : "playing";
    case PlaybackState::Failed: return "failed";
    case PlaybackState::Stopped: return "stopped";
    }
    return "stopped";
}

String playerStateJson() {
    String json;
    json.reserve(240);
    json = "{\"revision\":" + String(playerRevision()) +
        ",\"volume\":" + String(mainVal) +
        ",\"muted\":" + String(radioMuted ? "true" : "false") +
        ",\"bass\":" + String(gB) +
        ",\"mid\":" + String(gM) +
        ",\"treble\":" + String(gT) +
        ",\"state\":\"" + String(playbackStateName()) +
        "\",\"name\":\"" + jsonEscape(currentPlaybackName()) + "\"}";
    return json;
}

void sendPlayerState(int statusCode = 200) {
    server.send(statusCode, "application/json; charset=utf-8", playerStateJson());
}

bool hasCurrentPlayerRevision() {
    int requestRevision = 0;
    if (!parseIntegerArg("revision", 0, 2147483647, requestRevision)) {
        server.send(400, "application/json; charset=utf-8", "{\"error\":\"Invalid player revision\"}");
        return false;
    }
    if (static_cast<uint32_t>(requestRevision) != playerRevision()) {
        sendPlayerState(409);
        return false;
    }
    return true;
}

void appendNavigation(String& html, WebSection selected) {
    constexpr WebSection sections[] = {
        WebSection::Stations, WebSection::Network, WebSection::Weather,
        WebSection::Appearance, WebSection::Device,
    };
    html += "<nav class='rh-nav' aria-label='Settings sections'>";
    for (const WebSection section : sections) {
        html += "<a href='" + String(sectionPath(section)) + "' class='rh-navlink'";
        if (section == selected) html += " aria-current='page'";
        html += ">" + String(sectionIcon(section)) + "<span class='rh-desktoplabel'>" +
            String(sectionDesktopLabel(section)) + "</span><span class='rh-mobiletitle'>" +
            String(sectionMobileLabel(section)) + "</span></a>";
    }
    html += "</nav>";
}

void appendSectionContent(String& html, WebSection section) {
    switch (section) {
    case WebSection::Stations: {
        int savedCount = 0;
        for (int slot = 0; slot < STATION_COUNT; ++slot) {
            if (!stations[slot].name.isEmpty() || !stations[slot].url.isEmpty()) ++savedCount;
        }
        html += "<div class='rh-pagehead'><div><h1>Stations</h1><p>A familiar collection. A new discovery.</p></div></div>"
            "<section class='rh-list' aria-labelledby='saved-stations'><div class='rh-listhead'><h2 id='saved-stations'>Saved on your radio</h2><span>" +
            String(savedCount) + " / " + String(STATION_COUNT) + " stations</span></div>";
        if (savedCount == 0) {
            html += "<p class='rh-empty'>No saved stations. Add your first station above.</p>";
        } else {
            for (int slot = 0; slot < STATION_COUNT; ++slot) {
                const RadioStation& station = stations[slot];
                if (station.name.isEmpty() && station.url.isEmpty()) continue;
                const String name = station.name.isEmpty() ? station.url : station.name;
                html += "<div class='rh-station'><div class='rh-art' aria-hidden='true'>" +
                    htmlEscape(name.substring(0, 1)) + "</div><div><div class='rh-stationname'><bdi>" +
                    htmlEscape(name) + "</bdi>";
                if (slot == mediaPlayingStation()) html += "<span class='rh-playing'>Playing</span>";
                html += "</div><p class='rh-stationdesc' dir='ltr'>" + htmlEscape(station.url) +
                    "</p></div></div>";
            }
        }
        html += "</section><p class='rh-footnote'>Favorites appear on your radio, too.</p>";
        break;
    }
    case WebSection::Network:
        html += "<div class='rh-pagehead'><div><h1>Network</h1><p>Keep your radio connected.</p></div></div>"
            "<section class='rh-box' aria-labelledby='network-status'><h2 id='network-status'>Connection</h2><dl class='rh-data'>"
            "<dt>Status</dt><dd>" + htmlEscape(connectionState()) + "</dd><dt>Network</dt><dd><bdi>" +
            htmlEscape(WiFi.status() == WL_CONNECTED ? WiFi.SSID() : st_ssid) +
            "</bdi></dd><dt>Address</dt><dd dir='ltr'>" + htmlEscape(localAddress()) +
            "</dd></dl></section>";
        break;
    case WebSection::Weather:
        html += "<div class='rh-pagehead'><div><h1>Weather &amp; time</h1><p>A little local context for your radio.</p></div></div>"
            "<section class='rh-box' aria-labelledby='weather-status'><h2 id='weather-status'>Weather on your radio</h2><dl class='rh-data'>"
            "<dt>Location</dt><dd><bdi>" + htmlEscape(owmCity) + "</bdi></dd><dt>Units</dt><dd>" +
            String(useCelsius ? "Celsius" : "Fahrenheit") + "</dd></dl></section>";
        break;
    case WebSection::Appearance:
        html += "<div class='rh-pagehead'><div><h1>Appearance</h1><p>Keep the radio readable, day and night.</p></div></div>"
            "<section class='rh-box'><h2>On your radio</h2><p class='rh-muted'>Automatic dimming, supported themes and touch calibration are set on the radio itself.</p></section>";
        break;
    case WebSection::Device:
        html += "<div class='rh-pagehead'><div><h1>Device &amp; maintenance</h1><p>Device information and safe maintenance.</p></div></div>"
            "<section class='rh-box'><h2>Radio</h2><dl class='rh-data'><dt>Connection</dt><dd>" +
            htmlEscape(connectionState()) + "</dd><dt>Address</dt><dd dir='ltr'>" +
            htmlEscape(localAddress()) + "</dd></dl></section>";
        break;
    }
}

void handleConfigShell(WebSection section) {
    String html;
    html.reserve(9400);
    html = "<!doctype html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<meta name='theme-color' content='#0b1321'><title>radiohead / settings</title>"
        "<link rel='stylesheet' href='/ui/radiohead.css'></head><body class='radiohead-page'><div id='rh-config'>"
        "<header class='rh-top'><div class='rh-brand'><span class='rh-brand-mark' aria-hidden='true'>⌁</span>radiohead <span class='rh-muted'>/ settings</span></div>"
        "<div class='rh-topmeta'><span>" + htmlEscape(connectionState()) + "</span><span dir='ltr'>" +
        htmlEscape(localAddress()) + "</span></div></header><div class='rh-layout'><aside class='rh-sidebar'>";
    appendNavigation(html, section);
    html += "<div class='rh-device'><span class='rh-online' aria-hidden='true'></span>" +
        htmlEscape(connectionState()) + "<strong>radiohead</strong><span dir='ltr'>" + htmlEscape(localAddress()) +
        "</span></div></aside><main class='rh-main'><section class='rh-hero' aria-label='Current playback'><div><span class='rh-eyebrow'>" +
        playbackLabel() + "</span><h2 class='rh-playingname'><bdi>" + htmlEscape(currentPlaybackName()) +
        "</bdi></h2>";
    if (!songTitle.isEmpty() && mediaPlaybackState() == PlaybackState::Playing && !podcastMode) {
        html += "<p class='rh-muted'><bdi>" + htmlEscape(songTitle) + "</bdi></p>";
    }
    html += "</div><div class='rh-volume'><button class='rh-iconbutton' type='button' id='mute-button' data-role='mute' aria-label='" +
        String(radioMuted ? "Unmute radio" : "Mute radio") + "' aria-pressed='" +
        String(radioMuted ? "true" : "false") + "'><svg viewBox='0 0 24 24' aria-hidden='true'><path d='M4 10v4h4l5 4V6l-5 4H4Z'/><path d='M16 9a4 4 0 0 1 0 6M18.5 6.5a7.5 7.5 0 0 1 0 11'/></svg></button>"
        "<label for='volume-input'>Volume</label><input id='volume-input' type='range' min='0' max='21' value='" +
        String(mainVal) + "' aria-describedby='player-status'><output id='volume-value' for='volume-input'>" +
        String(mainVal) + " / 21</output></div></section><p class='rh-sr-only' id='player-status' aria-live='polite'></p>";
    appendSectionContent(html, section);
    html += "</main></div><section class='rh-eq'><details><summary>Sound · custom tone</summary><div class='rh-eqgrid'>"
        "<label class='rh-field' for='tone-bass'><span>Bass <output id='tone-bass-value'>" + String(gB) +
        "</output></span><input id='tone-bass' type='range' min='-15' max='15' value='" + String(gB) + "'></label>"
        "<label class='rh-field' for='tone-mid'><span>Mid <output id='tone-mid-value'>" + String(gM) +
        "</output></span><input id='tone-mid' type='range' min='-15' max='15' value='" + String(gM) + "'></label>"
        "<label class='rh-field' for='tone-treble'><span>Treble <output id='tone-treble-value'>" + String(gT) +
        "</output></span><input id='tone-treble' type='range' min='-15' max='15' value='" + String(gT) + "'></label>"
        "</div></details></section></div><script>"
        "(()=>{const volume=document.getElementById('volume-input'),value=document.getElementById('volume-value'),mute=document.getElementById('mute-button'),"
        "status=document.getElementById('player-status'),bass=document.getElementById('tone-bass'),mid=document.getElementById('tone-mid'),treble=document.getElementById('tone-treble');"
        "const tones=[bass,mid,treble],toneValues=['tone-bass-value','tone-mid-value','tone-treble-value'].map(id=>document.getElementById(id));let revision=" + String(playerRevision()) + ",volumeTimer=0,toneTimer=0,writing=false,pendingRequest=null;"
        "const show=(message)=>{status.textContent=message};const reflect=(state)=>{revision=state.revision;if(document.activeElement!==volume)volume.value=state.volume;value.textContent=state.volume+' / 21';"
        "mute.setAttribute('aria-pressed',state.muted);mute.setAttribute('aria-label',state.muted?'Unmute radio':'Mute radio');tones.forEach((tone,index)=>{if(document.activeElement!==tone)tone.value=[state.bass,state.mid,state.treble][index];toneValues[index].textContent=[state.bass,state.mid,state.treble][index]})};"
        "const request=(path,body)=>{if(writing){pendingRequest={path,body};return}writing=true;body.set('revision',revision);fetch(path,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body}).then(async response=>{const state=await response.json();reflect(state);if(!response.ok)throw Error(response.status===409?'The radio changed elsewhere. Values were refreshed.':'The radio did not accept that change.');show('Changes saved after you stop adjusting.')}).catch(error=>show(error.message)).finally(()=>{writing=false;const next=pendingRequest;pendingRequest=null;if(next)request(next.path,next.body)})};"
        "volume.addEventListener('input',()=>{value.textContent=volume.value+' / 21';clearTimeout(volumeTimer);volumeTimer=setTimeout(()=>request('/api/player/volume',new URLSearchParams({volume:volume.value})),140)});"
        "mute.addEventListener('click',()=>request('/api/player/mute',new URLSearchParams()));"
        "tones.forEach((tone,index)=>tone.addEventListener('input',()=>{toneValues[index].textContent=tone.value;clearTimeout(toneTimer);toneTimer=setTimeout(()=>request('/api/player/tone',new URLSearchParams({bass:bass.value,mid:mid.value,treble:treble.value})),180)}));"
        "setInterval(async()=>{if(writing||document.hidden)return;try{const response=await fetch('/api/player',{cache:'no-store'});if(response.ok)reflect(await response.json())}catch(_){show('Radio connection lost.')}} ,3000)})();</script></body></html>";
    server.send(200, "text/html; charset=utf-8", html);
}

void handleRoot() { handleConfigShell(WebSection::Stations); }

void handleUpdateUpload() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        otaUploadSucceeded = false;
        otaUploadStarted = Update.begin(UPDATE_SIZE_UNKNOWN);
        if (!otaUploadStarted) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (!otaUploadStarted) {
            return;
        }
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
            Update.abort();
            otaUploadStarted = false;
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (otaUploadStarted) {
            otaUploadSucceeded = Update.end(true);
            otaUploadStarted = false;
            if (!otaUploadSucceeded) {
                Update.printError(Serial);
            }
        }
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        if (otaUploadStarted) {
            Update.abort();
        }
        otaUploadStarted = false;
        otaUploadSucceeded = false;
    }
}

void handlePrograms() {
    String html = pageStart("תוכניות גל״צ", "#00FFFF");
    html += "<div dir='rtl'><a class='btn' href='/'>חזרה לרדיו</a>";
    for (int i = 0; i < PODCAST_SHOW_COUNT; ++i) {
        html += "<a class='btn' style='display:block;max-width:400px;margin:8px auto' href='/episodes?show=" +
            String(i) + "'>" + podcastShows[i].webName + "</a>";
    }
    html += "</div></body></html>";
    server.send(200, "text/html; charset=utf-8", html);
}

void handleEpisodes() {
    int showIndex = 0;
    if (!parseIntegerArg("show", 0, PODCAST_SHOW_COUNT - 1, showIndex)) {
        sendBadRequest("Invalid show");
        return;
    }

    String html = pageStart(podcastShows[showIndex].webName, "orange");
    html += "<div dir='rtl'><a class='btn' href='/programs'>חזרה לתוכניות</a>";
    const bool ready = podcastEpisodesReadyFor(showIndex);
    if (!ready && (podcastRequestedShow() != showIndex ||
                   server.hasArg("retry") || podcastLoadState() == PodcastLoadState::Idle)) {
        requestPodcastEpisodes(showIndex);
    }
    if (podcastLoadState() == PodcastLoadState::Loading && podcastRequestedShow() == showIndex) {
        html += "<h3>טוען פרקים…</h3><meta http-equiv='refresh' content='2'>";
    } else if (!podcastEpisodesReadyFor(showIndex)) {
        html += "<h3>לא ניתן לטעון פרקים</h3><a class='btn' href='/episodes?show=" +
            String(showIndex) + "&retry=1'>נסה שוב</a>";
    } else {
        for (int i = 0; i < podcastEpisodeCount; ++i) {
            PodcastEpisode& episode = podcastEpisodes[i];
            String date = episode.publishedUtc;
            if (date.length() >= 10) {
                date = date.substring(0, 10);
            }
            html += "<a class='btn' style='display:block;max-width:500px;margin:8px auto' href='/playepisode?show=" +
                String(showIndex) + "&ep=" + String(i) + "'>" + htmlEscape(episode.title) +
                "<br><small>" + htmlEscape(date) + "</small></a>";
        }
    }
    html += "</div></body></html>";
    server.send(200, "text/html; charset=utf-8", html);
}

void handleM3UUpload() {
    HTTPUpload& upload = server.upload();
    static String line;
    static String pendingName;
    static bool lineTooLong = false;

    const auto processLine = [&] {
        if (lineTooLong) {
            line = "";
            lineTooLong = false;
            return;
        }
        line.trim();
        if (line.startsWith("#EXTINF:")) {
            const int commaIndex = line.indexOf(',');
            pendingName = commaIndex >= 0 ? line.substring(commaIndex + 1) : "Unknown Station";
            if (pendingName.length() > MAX_STATION_NAME_LENGTH) {
                pendingName.remove(MAX_STATION_NAME_LENGTH);
            }
        } else if (isHttpUrl(line) && m3uTempList.size() < 30) {
            if (pendingName.isEmpty()) {
                pendingName = "Unknown Station";
            }
            m3uTempList.push_back({pendingName, line});
            pendingName = "";
        }
        line = "";
    };

    if (upload.status == UPLOAD_FILE_START) {
        m3uTempList.clear();
        line = "";
        line.reserve(160);
        pendingName = "";
        lineTooLong = false;
        return;
    }
    if (upload.status == UPLOAD_FILE_END) {
        if (!line.isEmpty() || lineTooLong) {
            processLine();
        }
        return;
    }
    if (upload.status == UPLOAD_FILE_ABORTED) {
        line = "";
        pendingName = "";
        lineTooLong = false;
        m3uTempList.clear();
        return;
    }
    if (upload.status != UPLOAD_FILE_WRITE || m3uTempList.size() >= 30) {
        return;
    }

    for (size_t i = 0; i < upload.currentSize; ++i) {
        const char character = upload.buf[i];
        if (character == '\n') {
            processLine();
            if (m3uTempList.size() >= 30) {
                return;
            }
            continue;
        }
        if (character == '\r' || lineTooLong) {
            continue;
        }
        if (line.length() >= MAX_M3U_LINE_LENGTH) {
            lineTooLong = true;
        } else {
            line += character;
        }
    }
}

}  // namespace

void startWebServer() {
    server.on("/", handleRoot);
    server.on("/stations", [] { handleConfigShell(WebSection::Stations); });
    server.on("/network", [] { handleConfigShell(WebSection::Network); });
    server.on("/weather", [] { handleConfigShell(WebSection::Weather); });
    server.on("/appearance", [] { handleConfigShell(WebSection::Appearance); });
    server.on("/device", [] { handleConfigShell(WebSection::Device); });
    server.on("/audio", [] { redirectTo("/"); });
    server.on("/skin", [] { redirectTo("/appearance"); });
    server.on("/update_ui", [] { redirectTo("/device"); });
    server.on("/ui/radiohead.css", HTTP_GET, [] {
        server.sendHeader("Cache-Control", "public, max-age=86400");
        server.send_P(200, "text/css; charset=utf-8",
            reinterpret_cast<const char*>(ui_web_configuration_css), ui_web_configuration_css_len);
    });
    server.on("/ui/assets/coast.jpg", HTTP_GET, [] {
        server.sendHeader("Cache-Control", "public, max-age=86400");
        server.send_P(200, "image/jpeg",
            reinterpret_cast<const char*>(ui_web_configuration_coast_jpg), ui_web_configuration_coast_jpg_len);
    });
    server.on("/api/player", HTTP_GET, [] { sendPlayerState(); });
    server.on("/api/player/volume", HTTP_POST, [] {
        int volume = 0;
        if (!hasCurrentPlayerRevision()) return;
        if (!parseIntegerArg("volume", 0, 21, volume)) {
            server.send(400, "application/json; charset=utf-8", "{\"error\":\"Invalid volume\"}");
            return;
        }
        setRadioVolumeIndex(volume);
        queueSettingsSave();
        sendPlayerState();
    });
    server.on("/api/player/mute", HTTP_POST, [] {
        if (!hasCurrentPlayerRevision()) return;
        toggleRadioMute();
        sendPlayerState();
    });
    server.on("/api/player/tone", HTTP_POST, [] {
        int bass = 0;
        int mid = 0;
        int treble = 0;
        if (!hasCurrentPlayerRevision()) return;
        if (!parseIntegerArg("bass", -15, 15, bass) ||
            !parseIntegerArg("mid", -15, 15, mid) ||
            !parseIntegerArg("treble", -15, 15, treble)) {
            server.send(400, "application/json; charset=utf-8", "{\"error\":\"Invalid tone\"}");
            return;
        }
        gB = bass;
        gM = mid;
        gT = treble;
        audio.setTone(gB, gM, gT);
        forceRedraw = true;
        queueSettingsSave();
        sendPlayerState();
    });
    server.on("/programs", handlePrograms);
    server.on("/episodes", handleEpisodes);

    server.on("/togglespec", [] {
        showSpectrum = !showSpectrum;
        tft.fillRect(0, 160, 320, 80, TFT_BLACK);
        forceRedraw = true;
        saveSettings();
        server.send(200, "text/plain", "OK");
    });
    server.on("/setVisual", HTTP_GET, [] {
        int mode = 0;
        if (!parseIntegerArg("mode", 1, 3, mode)) {
            sendBadRequest("Invalid mode");
            return;
        }
        visualMode = mode;
        tft.fillRect(0, 160, 320, 80, TFT_BLACK);
        server.send(200, "text/plain", "OK");
    });
    server.on("/setvol", [] {
        int volume = 0;
        if (!parseIntegerArg("v", 0, 21, volume)) {
            sendBadRequest("Invalid volume");
            return;
        }
        setRadioVolumeIndex(volume);
        saveSettings();
        redirectTo("/");
    });
    server.on("/seteq", [] {
        int bass = 0;
        int mid = 0;
        int treble = 0;
        if (!parseIntegerArg("b", -15, 15, bass) ||
            !parseIntegerArg("m", -15, 15, mid) ||
            !parseIntegerArg("t", -15, 15, treble)) {
            sendBadRequest("Invalid equalizer value");
            return;
        }
        gB = bass;
        gM = mid;
        gT = treble;
        audio.setTone(gB, gM, gT);
        saveSettings();
        redirectTo("/audio");
    });
    server.on("/setpreset", [] {
        const String preset = server.arg("p");
        if (preset == "rock") {
            gB = 6; gM = -1; gT = 5;
        } else if (preset == "pop") {
            gB = 3; gM = 2; gT = 2;
        } else if (preset == "jazz") {
            gB = 4; gM = 0; gT = 3;
        } else if (preset == "flat") {
            gB = 0; gM = 0; gT = 0;
        } else {
            sendBadRequest("Invalid preset");
            return;
        }
        audio.setTone(gB, gM, gT);
        saveSettings();
        redirectTo("/audio");
    });
    server.on("/setweather", HTTP_POST, [] {
        const String city = server.arg("city");
        const String key = server.arg("key");
        const String unit = server.arg("u");
        if (city.isEmpty() || city.length() > 80 || key.length() > 128 ||
            (unit != "C" && unit != "F")) {
            sendBadRequest("Invalid weather settings");
            return;
        }
        owmCity = city;
        if (server.hasArg("clear_key")) {
            owmKey = "";
        } else if (!key.isEmpty()) {
            owmKey = key;
        }
        useCelsius = unit == "C";
        saveSettings();
        server.send(200, "text/html", "<html><head><meta http-equiv='refresh' content='5;url=/'></head>"
            "<body><h1>Settings saved!</h1><p>Restarting...</p></body></html>");
        delay(1000);
        ESP.restart();
    });
    server.on("/setwifi", HTTP_POST, [] {
        const String newSsid = server.arg("s");
        const String newPassword = server.arg("p");
        if (newSsid.isEmpty() || newSsid.length() > 32 || newPassword.length() > 63) {
            sendBadRequest("Invalid Wi-Fi settings");
            return;
        }
        const bool networkChanged = newSsid != st_ssid;
        st_ssid = newSsid;
        if (server.hasArg("clear_pass") || (networkChanged && newPassword.isEmpty())) {
            st_pass = "";
        } else if (!newPassword.isEmpty()) {
            st_pass = newPassword;
        }
        pref.begin("radio", false);
        pref.putString("ssid", st_ssid);
        pref.putString("pass", st_pass);
        pref.end();
        server.send(200, "text/html", "Restarting...");
        delay(2000);
        ESP.restart();
    });
    server.on("/scan_m3u", [] {
        const String url = server.arg("m3u_url");
        if (!isHttpUrl(url)) {
            sendBadRequest("Invalid playlist URL");
            return;
        }
        parseM3UPro(url);
        redirectTo("/stations");
    });
    server.on("/apply_manual", [] {
        int stationIndex = 0;
        const String name = server.arg("n");
        const String url = server.arg("u");
        if (!parseIntegerArg("s", 0, STATION_COUNT - 1, stationIndex) ||
            name.isEmpty() || name.length() > MAX_STATION_NAME_LENGTH || !isHttpUrl(url)) {
            sendBadRequest("Invalid station");
            return;
        }
        stations[stationIndex].name = name;
        stations[stationIndex].url = url;
        if (clearStationFavorite(stationIndex)) {
            saveFavorites();
        }
        saveSettings();
        forceRedraw = true;
        redirectTo("/stations");
    });
    server.on("/clear_list", [] {
        m3uTempList.clear();
        redirectTo("/stations");
    });
    server.on("/edit", [] {
        int stationIndex = 0;
        const String name = server.arg("name");
        const String url = server.arg("url");
        if (!parseIntegerArg("id", 0, STATION_COUNT - 1, stationIndex) ||
            name.isEmpty() || name.length() > MAX_STATION_NAME_LENGTH ||
            (!url.isEmpty() && !isHttpUrl(url))) {
            sendBadRequest("Invalid station");
            return;
        }
        const bool replaced = stations[stationIndex].url != url;
        stations[stationIndex].url = url;
        stations[stationIndex].name = name;
        if (replaced && clearStationFavorite(stationIndex)) {
            saveFavorites();
        }
        saveSettings();
        forceRedraw = true;
        redirectTo("/stations");
    });
    server.on("/scan", [] { redirectTo("/?scan=1"); });
    server.on("/scan_data", [] {
        const int networkCount = WiFi.scanNetworks();
        String json = "[";
        for (int i = 0; i < networkCount; ++i) {
            json += "\"" + jsonEscape(WiFi.SSID(i)) + "\"" + (i == networkCount - 1 ? "" : ",");
        }
        json += "]";
        server.send(200, "application/json", json);
    });
    server.on("/setalarm", [] {
        int hour = 0;
        int minute = 0;
        if (!parseIntegerArg("h", 0, 23, hour) || !parseIntegerArg("m", 0, 59, minute)) {
            sendBadRequest("Invalid alarm time");
            return;
        }
        alarmH = hour;
        alarmM = minute;
        alarmActive = server.hasArg("active");
        saveSettings();
        forceRedraw = true;
        redirectTo("/");
    });
    server.on("/off", [] {
        server.send(200);
        delay(500);
        goToSleep();
    });
    server.on("/prev", [] {
        const int station = adjacentPlayableStationSlot(currentStationIdx, -1);
        if (station >= 0) {
            playStation(station);
            saveSettings();
        }
        redirectTo("/");
    });
    server.on("/next", [] {
        const int station = adjacentPlayableStationSlot(currentStationIdx, 1);
        if (station >= 0) {
            playStation(station);
            saveSettings();
        }
        redirectTo("/");
    });
    server.on("/favorite", [] {
        int stationIndex = 0;
        if (!parseIntegerArg("station", 0, STATION_COUNT - 1, stationIndex) ||
            stations[stationIndex].url.isEmpty()) {
            sendBadRequest("Invalid station");
            return;
        }
        const bool wasFavorite = isStationFavorite(stationIndex);
        if (toggleStationFavorite(stationIndex) && !saveFavorites()) {
            setStationFavorite(stationIndex, wasFavorite);
            server.send(500, "text/plain", "Unable to save favorite");
            return;
        }
        forceRedraw = true;
        server.send(200, "text/plain", isStationFavorite(stationIndex) ? "favorited" : "unfavorited");
    });
    server.on("/update", HTTP_POST, [] {
        if (!otaUploadSucceeded || Update.hasError()) {
            otaUploadSucceeded = false;
            server.send(500, "text/plain", "Firmware update failed");
            return;
        }
        otaUploadSucceeded = false;
        server.send(200, "text/plain", "OK");
        delay(1000);
        ESP.restart();
    }, handleUpdateUpload);
    server.on("/setskin", [] {
        const String top = server.arg("top");
        const String bottom = server.arg("bot");
        const String mainText = server.arg("txt");
        const String accent = server.arg("acc");
        const String wifi = server.arg("wifi");
        const String selector = server.arg("sel");
        const String clock = server.arg("clk");
        const String headerInfo = server.arg("hinf");
        if (!isHexColor(top) || !isHexColor(bottom) || !isHexColor(mainText) ||
            !isHexColor(accent) || !isHexColor(wifi) || !isHexColor(selector) ||
            !isHexColor(clock) || !isHexColor(headerInfo)) {
            sendBadRequest("Invalid color");
            return;
        }
        currentSkin.hexTop = top;
        currentSkin.hexBottom = bottom;
        currentSkin.hexMain = mainText;
        currentSkin.hexAccent = accent;
        currentSkin.hexWifi = wifi;
        currentSkin.hexSel = selector;
        currentSkin.hexClk = clock;
        currentSkin.hexHInfo = headerInfo;
        updateColors();
        saveSettings();
        redirectTo("/skin");
    });
    server.on("/defaultskin", [] {
        currentSkin.hexTop = "#000000";
        currentSkin.hexBottom = "#000000";
        currentSkin.hexMain = "#FFFFFF";
        currentSkin.hexAccent = "#00FFFF";
        currentSkin.hexWifi = "#00FF00";
        currentSkin.hexSel = "#0000FF";
        currentSkin.hexClk = "#FFFFFF";
        currentSkin.hexHInfo = "#FFFFFF";
        currentSkin.hexBarL = "#00FF00";
        currentSkin.hexBarM = "#FFFF00";
        currentSkin.hexBarH = "#FF0000";
        currentSkin.hexVol = "#00FFFF";
        currentSkin.hexAlm = "#FF0000";
        updateColors();
        saveSettings();
        redirectTo("/skin");
    });
    server.on("/upload_m3u", HTTP_POST, [] { redirectTo("/stations"); }, handleM3UUpload);
    server.on("/playepisode", [] {
        int showIndex = 0;
        int episodeIndex = 0;
        if (!parseIntegerArg("show", 0, PODCAST_SHOW_COUNT - 1, showIndex) ||
            !parseIntegerArg("ep", 0, MAX_EPISODES - 1, episodeIndex)) {
            sendBadRequest("Invalid episode");
            return;
        }
        if (!podcastEpisodesReadyFor(showIndex)) {
            requestPodcastEpisodes(showIndex);
            redirectTo("/episodes?show=" + String(showIndex));
            return;
        }
        if (episodeIndex >= podcastEpisodeCount) {
            sendBadRequest("Invalid episode");
            return;
        }
        if (!playPodcastEpisode(showIndex, episodeIndex)) {
            server.send(409, "text/plain", "Episode is no longer playable");
            return;
        }
        redirectTo("/");
    });

    server.begin();
}
