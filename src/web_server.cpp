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
// Deliberately generous during M3U import stress testing.  The request is
// still bounded, but its heap/service impact must be measured on hardware.
constexpr size_t MAX_M3U_TEXT_BYTES = 4 * 1024 * 1024;

bool otaUploadStarted = false;
bool otaUploadSucceeded = false;
bool artworkUploadAccepted = false;
bool artworkUploadSucceeded = false;

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
    if (mediaTestActive()) return mediaTestName();
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

uint32_t fnv1aString(const String& value, uint32_t hash = 2166136261UL) {
    for (size_t i = 0; i < value.length(); ++i) {
        hash ^= static_cast<uint8_t>(value[i]);
        hash *= 16777619UL;
    }
    return hash;
}

uint32_t stationCatalogRevision() {
    uint32_t hash = 2166136261UL;
    for (int slot = 0; slot < STATION_COUNT; ++slot) {
        hash = fnv1aString(stations[slot].name, hash);
        hash = fnv1aString(stations[slot].url, hash);
        hash ^= isStationFavorite(slot) ? 1U : 0U;
        hash *= 16777619UL;
    }
    return hash & 0x7fffffffUL;
}

bool hasCurrentStationRevision() {
    int revision = 0;
    if (!parseIntegerArg("revision", 0, 2147483647, revision)) {
        server.send(400, "application/json; charset=utf-8", "{\"error\":\"Invalid station revision\"}");
        return false;
    }
    if (static_cast<uint32_t>(revision) != stationCatalogRevision()) {
        server.send(409, "application/json; charset=utf-8",
                    "{\"error\":\"Station list changed. Refresh the draft before saving.\",\"revision\":" +
                    String(stationCatalogRevision()) + "}");
        return false;
    }
    return true;
}

int savedStationCount() {
    int count = 0;
    for (int slot = 0; slot < STATION_COUNT; ++slot) {
        if (!stations[slot].url.isEmpty()) ++count;
    }
    return count;
}

String stationListJson() {
    String json;
    json.reserve(2300);
    json = "{\"revision\":" + String(stationCatalogRevision()) + ",\"capacity\":" +
        String(STATION_COUNT) + ",\"saved\":" + String(savedStationCount()) + ",\"stations\":[";
    bool first = true;
    for (int slot = 0; slot < STATION_COUNT; ++slot) {
        const RadioStation& station = stations[slot];
        if (station.url.isEmpty()) continue;
        if (!first) json += ',';
        first = false;
        json += "{\"slot\":" + String(slot) + ",\"name\":\"" + jsonEscape(station.name) +
            "\",\"url\":\"" + jsonEscape(station.url) + "\",\"favorite\":" +
            String(isStationFavorite(slot) ? "true" : "false") + ",\"playing\":" +
            String(slot == mediaPlayingStation() ? "true" : "false") + ",\"artwork\":" +
            String(stationArtworkExists(slot) ? "true" : "false") + ",\"artworkRevision\":" +
            String(stationArtworkRevision(slot)) + "}";
    }
    return json + "]}";
}

void sendStationList(int status = 200) {
    server.send(status, "application/json; charset=utf-8", stationListJson());
}

void appendM3UEntries(const String& text, std::vector<RadioStation>& entries) {
    String line;
    String pendingName;
    line.reserve(160);
    const auto process = [&] {
        line.trim();
        if (line.startsWith("#EXTINF:")) {
            const int comma = line.indexOf(',');
            pendingName = comma >= 0 ? line.substring(comma + 1) : "Unknown station";
            if (pendingName.length() > MAX_STATION_NAME_LENGTH) pendingName.remove(MAX_STATION_NAME_LENGTH);
        } else if (isHttpUrl(line) && entries.size() < STATION_COUNT) {
            entries.push_back({pendingName.isEmpty() ? String("Unknown station") : pendingName, line});
            pendingName = "";
        }
        line = "";
    };
    for (size_t i = 0; i <= text.length(); ++i) {
        const char character = i == text.length() ? '\n' : text[i];
        if (character == '\n') {
            process();
        } else if (character != '\r' && line.length() < MAX_M3U_LINE_LENGTH) {
            line += character;
        }
    }
}

String m3uPreviewJson(const String& text, bool& valid) {
    std::vector<RadioStation> entries;
    valid = text.length() <= MAX_M3U_TEXT_BYTES;
    if (valid) appendM3UEntries(text, entries);
    String json = "{\"revision\":" + String(stationCatalogRevision()) + ",\"free\":" +
        String(STATION_COUNT - savedStationCount()) + ",\"entries\":[";
    for (size_t i = 0; i < entries.size(); ++i) {
        if (i) json += ',';
        json += "{\"name\":\"" + jsonEscape(entries[i].name) + "\",\"url\":\"" +
            jsonEscape(entries[i].url) + "\"}";
    }
    return json + "]}";
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
        const int savedCount = savedStationCount();
        html += "<div class='rh-pagehead'><div><h1>Stations</h1><p>A familiar collection. A new discovery.</p></div></div>"
            "<div class='rh-row rh-spread'><button class='rh-button rh-primary' id='station-add' type='button'" +
            String(savedCount >= STATION_COUNT ? " disabled" : "") + ">Add station</button><button class='rh-button' id='m3u-import' type='button'>Import M3U</button></div>"
            "<section class='rh-list' aria-labelledby='saved-stations'><div class='rh-listhead'><h2 id='saved-stations'>Saved on your radio</h2><span id='station-count'>" +
            String(savedCount) + " / " + String(STATION_COUNT) + " stations</span></div><div id='station-list'>";
        if (savedCount == 0) {
            html += "<p class='rh-empty'>No saved stations. Add your first station above.</p>";
        } else {
            for (int slot = 0; slot < STATION_COUNT; ++slot) {
                const RadioStation& station = stations[slot];
                if (station.url.isEmpty()) continue;
                const String name = station.name.isEmpty() ? station.url : station.name;
                html += "<div class='rh-station'><div class='rh-art' aria-hidden='true'>" +
                    String(stationArtworkExists(slot) ? "<img alt='' src='/api/stations/artwork?slot=" + String(slot) + "'>" : htmlEscape(name.substring(0, 1))) + "</div><div><div class='rh-stationname'><bdi>" +
                    htmlEscape(name) + "</bdi>";
                if (slot == mediaPlayingStation()) html += "<span class='rh-playing'>Playing</span>";
                html += "</div><p class='rh-stationdesc' dir='ltr'>" + htmlEscape(station.url) +
                    "</p></div><div class='rh-actions'><button class='rh-iconbutton' type='button' data-favorite='" + String(slot) +
                    "' aria-label='Toggle favorite for " + htmlEscape(name) + "' aria-pressed='" +
                    String(isStationFavorite(slot) ? "true" : "false") + "'>★</button><button class='rh-iconbutton' type='button' data-edit='" +
                    String(slot) + "' aria-label='Edit " + htmlEscape(name) + "'>✎</button></div></div>";
            }
        }
        html += "</div></section><p class='rh-importline'>Favorites appear on your radio, too. <span id='station-feedback' class='rh-muted' aria-live='polite'></span></p>"
            "<section class='rh-box' id='station-editor' hidden><button class='rh-back' type='button' id='station-cancel'>← Back to stations</button>"
            "<h2 id='station-editor-title'>Make it yours</h2><p>Use a direct audio stream, not a station’s website.</p>"
            "<div class='rh-steps'><b>1 · Find</b><span>→</span><span>2 · Review &amp; test</span><span>→</span><span>3 · Save</span></div>"
            "<div class='rh-tabs'><button type='button' data-mode='search' aria-pressed='true'>Search directory</button><button type='button' data-mode='manual' aria-pressed='false'>Enter manually</button></div>"
            "<div id='directory-search'><label class='rh-field'><span>Station name</span><input id='directory-query' maxlength='80' autocomplete='off'></label>"
            "<div class='rh-grid2'><label class='rh-field'><span>Country <small>optional</small></span><input id='directory-country' maxlength='80'></label><label class='rh-field'><span>Language <small>optional</small></span><input id='directory-language' maxlength='80'></label></div>"
            "<div class='rh-footer'><span class='rh-formstatus' id='directory-status'></span><button class='rh-button' type='button' id='directory-submit'>Search</button></div><div id='directory-results'></div></div>"
            "<label class='rh-field'><span>Station name</span><input id='station-name' maxlength='80' required></label><label class='rh-field'><span>Stream URL</span><input id='station-url' type='url' maxlength='512' inputmode='url' required></label>"
            "<label class='rh-field'><span>Logo URL <small>optional; local upload is the dependable fallback</small></span><input id='station-logo-url' type='url' maxlength='512'></label>"
            "<label class='rh-field'><span>Image framing</span><select id='station-art-fit'><option value='fit'>Fit with padding</option><option value='crop'>Center crop</option></select></label>"
            "<label class='rh-upload' for='station-logo-file'><input id='station-logo-file' type='file' accept='image/png,image/jpeg,image/webp,image/svg+xml'>Upload a station logo <small id='station-logo-status'></small></label>"
            "<div class='rh-art' id='station-art-preview' aria-label='Artwork preview'>R</div><label class='rh-inlinecheck'><input id='station-favorite' type='checkbox'>Add to favorites</label>"
            "<p class='rh-note rh-warning'>Test on radio changes playback. Browsing, editing and saving keep your current station playing.</p>"
            "<div class='rh-footer'><span class='rh-formstatus' id='station-form-status'>Not tested</span><button class='rh-button' type='button' id='station-test'>Test on radio</button><button class='rh-button rh-danger' type='button' id='station-remove' hidden>Remove station</button><button class='rh-button rh-primary' type='button' id='station-save'>Save station</button></div></section>"
            "<section class='rh-box' id='m3u-editor' hidden><button class='rh-back' type='button' id='m3u-cancel'>← Back to stations</button><h2>Import stations</h2><p>Preview your playlist before adding anything.</p>"
            "<label class='rh-upload' for='m3u-file'><input id='m3u-file' type='file' accept='.m3u,.m3u8,audio/x-mpegurl'>Choose an M3U file</label><label class='rh-field'><span>Or paste M3U content</span><textarea id='m3u-text' maxlength='4194304'></textarea></label>"
            "<div class='rh-footer'><span class='rh-formstatus' id='m3u-status'></span><button class='rh-button' type='button' id='m3u-preview'>Preview import</button><button class='rh-button rh-primary' type='button' id='m3u-commit' disabled>Import stations</button></div><div id='m3u-results'></div></section>"
            "<script>";
        html += R"JS((()=>{
const $=id=>document.getElementById(id), api=(path,body)=>fetch(path,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(body)}).then(async r=>{const data=await r.json().catch(()=>({error:'The radio returned an invalid response.'}));if(!r.ok)throw Object.assign(Error(data.error||'Request failed.'),{data});return data});
let state=null,draftSlot=null,draftArtwork=null,draftArtworkDirty=false,previewRevision=null,testPoll=0;
const feedback=message=>$('station-feedback').textContent=message;
const esc=value=>String(value).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const showList=()=>{$('station-editor').hidden=true;$('m3u-editor').hidden=true;window.scrollTo({top:0,behavior:'smooth'});};
function render(data){state=data;$('station-count').textContent=data.saved+' / '+data.capacity+' stations';$('station-add').disabled=data.saved>=data.capacity;const list=$('station-list');list.innerHTML=data.stations.length?data.stations.map(s=>`<div class="rh-station"><div class="rh-art" aria-hidden="true">${s.artwork?`<img alt="" src="/api/stations/artwork?slot=${s.slot}&r=${s.artworkRevision}">`:esc((s.name||s.url).slice(0,1))}</div><div><div class="rh-stationname"><bdi>${esc(s.name||s.url)}</bdi>${s.playing?'<span class="rh-live">Playing</span>':''}</div><p class="rh-stationdesc" dir="ltr">${esc(s.url)}</p></div><div class="rh-actions"><button class="rh-iconbutton" data-favorite="${s.slot}" aria-label="Toggle favorite for ${esc(s.name)}" aria-pressed="${s.favorite}">★</button><button class="rh-iconbutton" data-edit="${s.slot}" aria-label="Edit ${esc(s.name)}">✎</button></div></div>`).join(''):'<p class="rh-empty">No saved stations. Add your first station above.</p>';list.querySelectorAll('[data-edit]').forEach(b=>b.onclick=()=>openEditor(Number(b.dataset.edit)));list.querySelectorAll('[data-favorite]').forEach(b=>b.onclick=()=>favorite(Number(b.dataset.favorite)));}
async function refresh(){try{render(await fetch('/api/stations',{cache:'no-store'}).then(r=>r.json()))}catch(_){feedback('Radio connection lost.')}}
function openEditor(slot){const existing=slot===null?null:state.stations.find(s=>s.slot===slot);if(!existing&&state.saved>=state.capacity){feedback('All 10 slots are full. Edit or remove a saved station to make room.');return;}draftSlot=slot;draftArtwork=null;draftArtworkDirty=false;clearInterval(testPoll);$('station-editor-title').textContent=existing?'Edit station':'Make it yours';$('station-name').value=existing?existing.name:'';$('station-url').value=existing?existing.url:'';$('station-logo-url').value='';$('station-favorite').checked=!!(existing&&existing.favorite);$('station-remove').hidden=!existing;$('station-form-status').textContent='Not tested';$('station-art-preview').textContent=(existing?existing.name:'R').slice(0,1)||'R';$('station-editor').hidden=false;$('m3u-editor').hidden=true;$('station-editor').scrollIntoView({behavior:'smooth'});}
async function favorite(slot){try{const data=await api('/api/stations/favorite',{slot,revision:state.revision});render(data);feedback('Favorite updated.')}catch(error){feedback(error.message);await refresh();}}
async function search(){const query=$('directory-query').value.trim();if(!query){$('directory-status').textContent='Enter a station name.';return;}$('directory-status').textContent='Searching directory…';const aliases={nyt:'New York Times'};const term=aliases[query.toLowerCase()]||query;const params=new URLSearchParams({name:term,limit:'20',hidebroken:'true'});const country=$('directory-country').value.trim(),language=$('directory-language').value.trim();if(country)params.set('country',country);if(language)params.set('language',language);const mirrors=['https://de1.api.radio-browser.info','https://at1.api.radio-browser.info'];let results;for(const mirror of mirrors){try{const ctl=new AbortController(),timer=setTimeout(()=>ctl.abort(),7000);const response=await fetch(mirror+'/json/stations/search?'+params,{signal:ctl.signal});clearTimeout(timer);if(response.ok){results=await response.json();break;}}catch(_){}}if(!results){$('directory-status').textContent='The station directory is unavailable. Try again when internet returns, or enter a stream URL manually.';return;}const norm=s=>s.normalize('NFKC').toLocaleLowerCase().trim().replace(/\s+/g,' ');const target=norm(term);results=results.filter(r=>r&&r.name&&(r.url_resolved||r.url)).sort((a,b)=>{const score=r=>{const n=norm(r.name);return n===target?3:n.split(' ').includes(target)?2:n.includes(target)?1:0};return score(b)-score(a)||(Number(b.votes)||0)-(Number(a.votes)||0)}).slice(0,5);$('directory-status').textContent=results.length?(term!==query?'Searching for '+term+'.':'Suggested matches'):'No matching stations found. Try another name or enter a stream URL manually.';$('directory-results').innerHTML=results.map((r,i)=>`<button class="rh-result" type="button" data-result="${i}"><span class="rh-art">${esc(r.name.slice(0,1))}</span><span><bdi>${esc(r.name)}</bdi><small>${esc([r.country,r.language,r.codec].filter(Boolean).join(' · ')||'Untested')}</small></span></button>`).join('');$('directory-results').querySelectorAll('[data-result]').forEach(b=>b.onclick=()=>{const r=results[Number(b.dataset.result)];$('station-name').value=r.name;$('station-url').value=r.url_resolved||r.url;$('station-logo-url').value=r.favicon||'';$('station-form-status').textContent='Not tested';$('directory-results').innerHTML='';$('directory-status').textContent='Suggestion copied to your editable draft.';});}
async function loadImage(fileOrUrl){try{let blob=fileOrUrl;if(typeof fileOrUrl==='string'){const response=await fetch(fileOrUrl,{mode:'cors'});if(!response.ok)throw Error();blob=await response.blob();}if(blob.size>512*1024)throw Error('Image exceeds the 512 KiB limit.');const image=await createImageBitmap(blob);if(image.width>2048||image.height>2048)throw Error('Image dimensions exceed 2048 × 2048.');draftArtwork={image,blob};draftArtworkDirty=true;$('station-logo-status').textContent='Ready to prepare after station save.';const canvas=document.createElement('canvas');canvas.width=88;canvas.height=88;const c=canvas.getContext('2d');c.fillStyle='#26364f';c.fillRect(0,0,88,88);const scale=$('station-art-fit').value==='crop'?Math.max(88/image.width,88/image.height):Math.min(88/image.width,88/image.height);c.drawImage(image,(88-image.width*scale)/2,(88-image.height*scale)/2,image.width*scale,image.height*scale);$('station-art-preview').innerHTML='';$('station-art-preview').append(c); }catch(error){draftArtwork=null;draftArtworkDirty=false;$('station-logo-status').textContent=(error.message||'Could not prepare that logo. Use a local PNG, JPEG or WebP image.');}}
function le32(out,n){out.push(n&255,(n>>>8)&255,(n>>>16)&255,(n>>>24)&255)}function fnv(bytes){let h=2166136261;for(const b of bytes){h^=b;h=Math.imul(h,16777619)}return h>>>0}function artPackage(image,revision){const payload=[];for(const size of [32,64,88]){const canvas=document.createElement('canvas');canvas.width=canvas.height=size;const c=canvas.getContext('2d');c.fillStyle='#26364f';c.fillRect(0,0,size,size);const scale=$('station-art-fit').value==='crop'?Math.max(size/image.width,size/image.height):Math.min(size/image.width,size/image.height);c.drawImage(image,(size-image.width*scale)/2,(size-image.height*scale)/2,image.width*scale,image.height*scale);const p=c.getImageData(0,0,size,size).data;for(let i=0;i<p.length;i+=4){const rgb=((p[i]&248)<<8)|((p[i+1]&252)<<3)|(p[i+2]>>3);payload.push(rgb>>8,rgb&255);}}const out=[82,72,65,49];le32(out,revision);le32(out,fnv(payload));out.push(0,0,0,0,...payload);return new Blob([new Uint8Array(out)],{type:'application/octet-stream'});}
async function uploadArtwork(slot,revision){if(!draftArtworkDirty||!draftArtwork)return 'No artwork selected.';const form=new FormData();form.set('package',artPackage(draftArtwork.image,revision),'station-artwork.rha');const response=await fetch('/api/stations/artwork?slot='+encodeURIComponent(slot)+'&revision='+encodeURIComponent(revision),{method:'POST',body:form});const data=await response.json().catch(()=>({error:'Artwork response was invalid.'}));if(!response.ok)throw Error(data.error||'Artwork upload failed.');return 'Artwork saved.';}
async function save(){const name=$('station-name').value.trim(),url=$('station-url').value.trim();if(!name||!/^https?:\/\//i.test(url)){ $('station-form-status').textContent='Enter a station name and direct HTTP(S) stream URL.';return;}try{const data=await api('/api/stations/save',{slot:draftSlot===null?'':draftSlot,name,url,favorite:$('station-favorite').checked?'1':'0',revision:state.revision});render(data);const saved=data.stations.find(s=>s.url===url&&s.name===name);let result='Station saved.';if(draftArtworkDirty&&saved){try{result+=' '+await uploadArtwork(saved.slot,saved.artworkRevision);await refresh();}catch(error){result+=' Station saved, but artwork failed: '+error.message;}}feedback(result);$('station-form-status').textContent=result;showList();}catch(error){$('station-form-status').textContent=error.message;await refresh();}}
async function test(){const name=$('station-name').value.trim(),url=$('station-url').value.trim();if(!name||!/^https?:\/\//i.test(url)){ $('station-form-status').textContent='Enter a station name and direct HTTP(S) stream URL first.';return;}try{await api('/api/stations/test',{name,url});$('station-form-status').textContent='Testing…';clearInterval(testPoll);testPoll=setInterval(async()=>{try{const p=await fetch('/api/player',{cache:'no-store'}).then(r=>r.json());if(p.state==='playing'||p.state==='muted'||p.state==='failed'){$('station-form-status').textContent=p.state==='failed'?'Test failed.':'Playing test stream.';clearInterval(testPoll);}}catch(_){}} ,700);}catch(error){$('station-form-status').textContent=error.message;}}
async function preview(){const text=$('m3u-text').value;if(!text){$('m3u-status').textContent='Choose a file or paste M3U content.';return;}try{const data=await api('/api/stations/m3u/preview',{text});previewRevision=data.revision;$('m3u-status').textContent=`${data.entries.length} valid entries; ${data.free} free slots.`;$('m3u-results').innerHTML=data.entries.map(e=>`<p class="rh-note"><bdi>${esc(e.name)}</bdi><span dir="ltr">${esc(e.url)}</span></p>`).join('')||'<p class="rh-note rh-warning">No valid radio streams were found. HLS segments are not stations.</p>';$('m3u-commit').disabled=!data.entries.length||data.entries.length>data.free;}catch(error){$('m3u-status').textContent=error.message;}}
async function commit(){try{const data=await api('/api/stations/m3u/commit',{text:$('m3u-text').value,revision:previewRevision});render(data);feedback('Stations imported.');showList();}catch(error){$('m3u-status').textContent=error.message;await refresh();}}
$('station-add').onclick=()=>openEditor(null);$('station-cancel').onclick=showList;$('station-save').onclick=save;$('station-test').onclick=test;$('station-remove').onclick=async()=>{const item=state.stations.find(s=>s.slot===draftSlot);if(!item||!confirm(`Remove ${item.name}? This removes its saved station, favorite and artwork. Playback of this exact station will stop.`))return;try{render(await api('/api/stations/remove',{slot:draftSlot,revision:state.revision}));feedback('Station removed.');showList();}catch(error){$('station-form-status').textContent=error.message;}};$('directory-submit').onclick=search;$('directory-query').addEventListener('keydown',e=>{if(e.key==='Enter'){e.preventDefault();search();}});document.querySelectorAll('[data-mode]').forEach(button=>button.onclick=()=>{const searchMode=button.dataset.mode==='search';document.querySelectorAll('[data-mode]').forEach(b=>b.setAttribute('aria-pressed',b===button));$('directory-search').hidden=!searchMode;});$('station-logo-file').onchange=e=>e.target.files[0]&&loadImage(e.target.files[0]);$('station-logo-url').addEventListener('change',e=>e.target.value&&loadImage(e.target.value));$('m3u-import').onclick=()=>{$('station-editor').hidden=true;$('m3u-editor').hidden=false;$('m3u-editor').scrollIntoView({behavior:'smooth'});};$('m3u-cancel').onclick=showList;$('m3u-preview').onclick=preview;$('m3u-commit').onclick=commit;$('m3u-file').onchange=async e=>{const f=e.target.files[0];if(!f)return;if(f.size>4194304){$('m3u-status').textContent='M3U files are limited to 4 MiB.';return;}$('m3u-text').value=await f.text();};refresh();
})();</script>)JS";
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
    // The Station editor carries its local-only discovery and image-preparation
    // code, so reserve once instead of repeatedly growing a transient String.
    html.reserve(section == WebSection::Stations ? 24500 : 9400);
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

void handleArtworkUpload() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        artworkUploadAccepted = false;
        artworkUploadSucceeded = false;
        int slot = -1;
        int revision = 0;
        if (upload.name != "package" || !parseIntegerArg("slot", 0, STATION_COUNT - 1, slot) ||
            !parseIntegerArg("revision", 1, 2147483647, revision)) return;
        artworkUploadAccepted = stationArtworkUploadBegin(slot, static_cast<uint32_t>(revision));
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (!artworkUploadAccepted || !stationArtworkUploadWrite(upload.buf, upload.currentSize)) {
            artworkUploadAccepted = false;
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        artworkUploadSucceeded = artworkUploadAccepted && stationArtworkUploadFinish();
        artworkUploadAccepted = false;
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        stationArtworkUploadAbort();
        artworkUploadAccepted = false;
        artworkUploadSucceeded = false;
    }
}

void sendStationArtworkThumbnail() {
    int slot = -1;
    if (!parseIntegerArg("slot", 0, STATION_COUNT - 1, slot) || !stationArtworkExists(slot)) {
        server.send(404, "text/plain", "Station artwork is unavailable");
        return;
    }
    uint16_t pixels[32 * 32];
    if (!loadStationArtwork(slot, 32, pixels, sizeof(pixels) / sizeof(pixels[0]))) {
        server.send(404, "text/plain", "Station artwork is unavailable");
        return;
    }
    uint8_t header[54] = {};
    header[0] = 'B'; header[1] = 'M';
    const uint32_t size = sizeof(header) + 32 * 32 * 3;
    const auto put32 = [&header](size_t offset, uint32_t value) {
        header[offset] = value & 0xff; header[offset + 1] = (value >> 8) & 0xff;
        header[offset + 2] = (value >> 16) & 0xff; header[offset + 3] = (value >> 24) & 0xff;
    };
    put32(2, size); put32(10, sizeof(header)); put32(14, 40); put32(18, 32); put32(22, 32);
    header[26] = 1; header[28] = 24; put32(34, 32 * 32 * 3);
    server.sendHeader("Cache-Control", "no-store");
    server.setContentLength(size);
    server.send(200, "image/bmp", "");
    server.sendContent(reinterpret_cast<const char*>(header), sizeof(header));
    uint8_t row[32 * 3];
    for (int y = 31; y >= 0; --y) {
        for (int x = 0; x < 32; ++x) {
            const uint16_t pixel = pixels[y * 32 + x];
            row[x * 3] = static_cast<uint8_t>(((pixel & 0x1f) << 3) | ((pixel & 0x1f) >> 2));
            row[x * 3 + 1] = static_cast<uint8_t>((((pixel >> 5) & 0x3f) << 2) | (((pixel >> 5) & 0x3f) >> 4));
            row[x * 3 + 2] = static_cast<uint8_t>((((pixel >> 11) & 0x1f) << 3) | (((pixel >> 11) & 0x1f) >> 2));
        }
        server.sendContent(reinterpret_cast<const char*>(row), sizeof(row));
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
    server.on("/api/stations", HTTP_GET, [] { sendStationList(); });
    server.on("/api/stations/artwork", HTTP_GET, sendStationArtworkThumbnail);
    server.on("/api/stations/save", HTTP_POST, [] {
        if (!hasCurrentStationRevision()) return;
        const String name = server.arg("name");
        const String url = server.arg("url");
        if (name.isEmpty() || name.length() > MAX_STATION_NAME_LENGTH || !isHttpUrl(url)) {
            server.send(400, "application/json; charset=utf-8", "{\"error\":\"Enter a station name and direct HTTP(S) stream URL.\"}");
            return;
        }
        int slot = -1;
        if (server.hasArg("slot") && !server.arg("slot").isEmpty()) {
            if (!parseIntegerArg("slot", 0, STATION_COUNT - 1, slot) || stations[slot].url.isEmpty()) {
                server.send(409, "application/json; charset=utf-8", "{\"error\":\"That station is no longer available to edit.\"}");
                return;
            }
        } else {
            for (int candidate = 0; candidate < STATION_COUNT; ++candidate) {
                if (stations[candidate].url.isEmpty()) {
                    slot = candidate;
                    break;
                }
            }
            if (slot < 0) {
                server.send(409, "application/json; charset=utf-8", "{\"error\":\"All 10 slots are full. Remove a station before adding another.\"}");
                return;
            }
        }
        const bool streamChanged = stations[slot].url != url;
        if (streamChanged) removeStationArtwork(slot);
        stations[slot].name = name;
        stations[slot].url = url;
        const bool favorite = server.arg("favorite") == "1";
        setStationFavorite(slot, favorite);
        saveSettings();
        if (!saveFavorites()) {
            server.send(500, "application/json; charset=utf-8", "{\"error\":\"Station was changed but its favorite could not be saved.\"}");
            return;
        }
        forceRedraw = true;
        sendStationList();
    });
    server.on("/api/stations/favorite", HTTP_POST, [] {
        int slot = -1;
        if (!hasCurrentStationRevision()) return;
        if (!parseIntegerArg("slot", 0, STATION_COUNT - 1, slot) || stations[slot].url.isEmpty()) {
            server.send(400, "application/json; charset=utf-8", "{\"error\":\"Invalid station.\"}");
            return;
        }
        const bool wasFavorite = isStationFavorite(slot);
        if (toggleStationFavorite(slot) && !saveFavorites()) {
            setStationFavorite(slot, wasFavorite);
            server.send(500, "application/json; charset=utf-8", "{\"error\":\"Unable to save favorite.\"}");
            return;
        }
        forceRedraw = true;
        sendStationList();
    });
    server.on("/api/stations/remove", HTTP_POST, [] {
        int slot = -1;
        if (!hasCurrentStationRevision()) return;
        if (!parseIntegerArg("slot", 0, STATION_COUNT - 1, slot) || stations[slot].url.isEmpty()) {
            server.send(400, "application/json; charset=utf-8", "{\"error\":\"Invalid station.\"}");
            return;
        }
        // Do not retune to an arbitrary occupant of a reused slot.  Only the
        // exact removed stream is stopped.
        if (mediaPlayingStation() == slot || mediaRequestedStation() == slot) stopStationPlayback();
        stations[slot].name = "";
        stations[slot].url = "";
        removeStationArtwork(slot);
        clearStationFavorite(slot);
        saveSettings();
        if (!saveFavorites()) {
            server.send(500, "application/json; charset=utf-8", "{\"error\":\"Unable to save station removal.\"}");
            return;
        }
        forceRedraw = true;
        sendStationList();
    });
    server.on("/api/stations/test", HTTP_POST, [] {
        const String name = server.arg("name");
        const String url = server.arg("url");
        if (name.isEmpty() || name.length() > MAX_STATION_NAME_LENGTH || !isHttpUrl(url) || !startStationTest(name, url)) {
            server.send(400, "application/json; charset=utf-8", "{\"error\":\"The radio could not start that test stream.\"}");
            return;
        }
        server.send(202, "application/json; charset=utf-8", "{\"state\":\"testing\"}");
    });
    server.on("/api/stations/m3u/preview", HTTP_POST, [] {
        const String text = server.arg("text");
        bool valid = false;
        const String json = m3uPreviewJson(text, valid);
        if (!valid) {
            server.send(413, "application/json; charset=utf-8", "{\"error\":\"M3U content exceeds the 4 MiB limit.\"}");
            return;
        }
        server.send(200, "application/json; charset=utf-8", json);
    });
    server.on("/api/stations/m3u/commit", HTTP_POST, [] {
        if (!hasCurrentStationRevision()) return;
        const String text = server.arg("text");
        if (text.length() > MAX_M3U_TEXT_BYTES) {
            server.send(413, "application/json; charset=utf-8", "{\"error\":\"M3U content exceeds the 4 MiB limit.\"}");
            return;
        }
        std::vector<RadioStation> entries;
        appendM3UEntries(text, entries);
        std::vector<RadioStation> unique;
        for (const RadioStation& entry : entries) {
            bool duplicate = false;
            for (int slot = 0; slot < STATION_COUNT; ++slot) {
                if (stations[slot].url == entry.url) duplicate = true;
            }
            for (const RadioStation& accepted : unique) {
                if (accepted.url == entry.url) duplicate = true;
            }
            if (!duplicate) unique.push_back(entry);
        }
        const int freeSlots = STATION_COUNT - savedStationCount();
        if (unique.empty()) {
            server.send(400, "application/json; charset=utf-8", "{\"error\":\"No new valid radio streams were found.\"}");
            return;
        }
        if (static_cast<int>(unique.size()) > freeSlots) {
            server.send(409, "application/json; charset=utf-8", "{\"error\":\"Not enough free station slots. Nothing was imported.\"}");
            return;
        }
        size_t entry = 0;
        for (int slot = 0; slot < STATION_COUNT && entry < unique.size(); ++slot) {
            if (!stations[slot].url.isEmpty()) continue;
            removeStationArtwork(slot);
            stations[slot] = unique[entry++];
            clearStationFavorite(slot);
        }
        saveSettings();
        saveFavorites();
        forceRedraw = true;
        sendStationList();
    });
    server.on("/api/stations/artwork", HTTP_POST, [] {
        if (!artworkUploadSucceeded) {
            server.send(400, "application/json; charset=utf-8", "{\"error\":\"Artwork was rejected; the previous artwork was kept.\"}");
            return;
        }
        artworkUploadSucceeded = false;
        forceRedraw = true;
        server.send(200, "application/json; charset=utf-8", "{\"ok\":true}");
    }, handleArtworkUpload);
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
        if (stations[stationIndex].url != url) removeStationArtwork(stationIndex);
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
        if (replaced) removeStationArtwork(stationIndex);
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
