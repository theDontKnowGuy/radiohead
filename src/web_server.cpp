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

void handleRoot() {
    String html = pageStart("Internet Radio");
    html +=
        "<a href='/weather' class='btn' style='border-color:#FFD700;color:#FFD700'>WEATHER</a>"
        "<a href='/stations' class='btn' style='border-color:yellow;color:yellow'>STATIONS</a>"
        "<a href='/programs' class='btn' style='border-color:#00FFFF;color:#00FFFF'>PROGRAMS</a>"
        "<a href='/audio' class='btn' style='border-color:cyan;color:cyan'>AUDIO</a>"
        "<a href='/skin' class='btn' style='border-color:magenta;color:magenta'>SKIN</a>";

    if (showSpectrum) {
        html += "<div class='card'><label>VISUAL MODE: </label><select onchange='setVisual(this.value)'>";
        html += "<option value='1' " + String(visualMode == 1 ? "selected" : "") + ">Spectrum Only</option>";
        html += "<option value='2' " + String(visualMode == 2 ? "selected" : "") + ">VU Meter Only</option>";
        html += "<option value='3' " + String(visualMode == 3 ? "selected" : "") + ">Both</option></select></div>";
    }

    html += "<h3>" + htmlEscape(stations[currentStationIdx].name) + "</h3>"
        "<div class='card'><a href='/prev' class='btn'>&lt;</a><b> VOLUME </b><a href='/next' class='btn'>&gt;</a>"
        "<form action='/setvol'><input type='range' name='v' min='0' max='21' value='" + String(mainVal) +
        "' onchange='this.form.submit()' style='width:90%'></form></div>";

    html += "<div class='card' style='border-color:#00FF00'><b>WIFI SETTINGS</b><br><small>Current: " +
        htmlEscape(WiFi.status() == WL_CONNECTED ? WiFi.SSID() : String("Disconnected")) +
        "</small><form action='/setwifi' method='POST'>SSID: <select name='s' id='ssid_list'>"
        "<option value='" + htmlEscape(st_ssid) + "'>" + htmlEscape(st_ssid) + "</option></select>"
        " <a href='/scan' class='btn'>SCAN</a><br><br>Pass: "
        "<input type='password' name='p' autocomplete='new-password' placeholder='Leave blank to keep current'>"
        "<br><label><input type='checkbox' name='clear_pass'> Clear saved password</label>"
        "<br><br><input type='submit' value='SAVE & RESTART' class='btn'></form></div>";

    html += "<div class='card' style='border-color:cyan'><b>ALARM</b>"
        "<form action='/setalarm'>Time: <input type='number' name='h' min='0' max='23' value='" + String(alarmH) +
        "' style='width:55px'> : <input type='number' name='m' min='0' max='59' value='" + String(alarmM) +
        "' style='width:55px'><br><br><input type='checkbox' name='active' " +
        String(alarmActive ? "checked" : "") + "> Enabled<br>"
        "<input type='submit' value='SAVE ALARM' class='btn'></form></div>";

    html += "<button onclick='toggleVisual()' class='btn' style='border-color:" +
        String(showSpectrum ? "#00FF00" : "#FF0000") + "'>" +
        String(showSpectrum ? "VISUAL: ON" : "VISUAL: OFF") + "</button>"
        "<a href='/update_ui' class='btn'>OTA UPDATE</a>"
        "<a href='/off' class='btn' style='border-color:red;color:red'>POWER OFF</a>"
        "<script>function toggleVisual(){fetch('/togglespec').then(()=>location.reload())}"
        "function setVisual(v){fetch('/setVisual?mode='+v)}"
        "if(location.search.includes('scan=1'))fetch('/scan_data').then(r=>r.json()).then(d=>{"
        "let s=document.getElementById('ssid_list');s.innerHTML='';d.forEach(n=>{let o=document.createElement('option');"
        "o.value=n;o.text=n;s.appendChild(o)})})</script></body></html>";
    server.send(200, "text/html; charset=utf-8", html);
}

void handleStations() {
    String html = pageStart("Radio Stations", "yellow");
    html += "<a href='/' class='btn'>BACK TO RADIO</a>"
        "<div class='card'><h3>Import M3U</h3><form action='/scan_m3u'>"
        "<input type='text' name='m3u_url' maxlength='512' placeholder='http://.../playlist.m3u'>"
        "<input type='submit' value='SCAN' class='btn'></form><hr>"
        "<form method='POST' action='/upload_m3u' enctype='multipart/form-data'>"
        "<input type='file' name='f' accept='.m3u,.m3u8'><input type='submit' value='UPLOAD' class='btn'></form></div>";

    if (!m3uTempList.empty()) {
        html += "<div class='card' style='border-color:cyan'><h3>Found:</h3><table>";
        for (size_t i = 0; i < m3uTempList.size(); ++i) {
            html += "<tr><td>" + htmlEscape(m3uTempList[i].name) + "</td><td><form action='/apply_manual'>"
                "<input type='hidden' name='u' value='" + htmlEscape(m3uTempList[i].url) + "'>"
                "<input type='hidden' name='n' value='" + htmlEscape(m3uTempList[i].name) + "'><select name='s'>";
            for (int slot = 0; slot < STATION_COUNT; ++slot) {
                html += "<option value='" + String(slot) + "'>Slot " + String(slot + 1) + "</option>";
            }
            html += "</select><input type='submit' value='ADD' class='btn'></form></td></tr>";
        }
        html += "</table><a href='/clear_list' class='btn' style='border-color:red'>CLEAR LIST</a></div>";
    }

    html += "<h3>Presets</h3><table>";
    for (int i = 0; i < STATION_COUNT; ++i) {
        html += "<tr><form action='/edit'><td>" + String(i + 1) + "</td><td>"
            "<input type='text' name='name' maxlength='80' value='" + htmlEscape(stations[i].name) + "'><br>"
            "<input type='text' name='url' maxlength='512' value='" + htmlEscape(stations[i].url) + "'></td><td>"
            "<input type='hidden' name='id' value='" + String(i) + "'>"
            "<input type='submit' value='SAVE' class='btn'></td></form></tr>";
    }
    html += "</table></body></html>";
    server.send(200, "text/html; charset=utf-8", html);
}

void handleSkin() {
    String html = pageStart("Skin Engine", "magenta");
    html += "<a href='/' class='btn'>BACK TO RADIO</a><div class='card'><form action='/setskin'>";
    const auto row = [](const String& label, const String& name, const String& value) {
        return "<p>" + label + " <input type='color' name='" + name + "' value='" + value + "'></p>";
    };
    html += row("Header Bar", "top", currentSkin.hexTop);
    html += row("Clock", "clk", currentSkin.hexClk);
    html += row("Header Info", "hinf", currentSkin.hexHInfo);
    html += row("Background", "bot", currentSkin.hexBottom);
    html += row("Main Text", "txt", currentSkin.hexMain);
    html += row("Accent Text", "acc", currentSkin.hexAccent);
    html += row("WiFi/Signal", "wifi", currentSkin.hexWifi);
    html += row("Selector", "sel", currentSkin.hexSel);
    html += "<input type='submit' value='APPLY & SAVE' class='btn'>"
        "<a href='/defaultskin' class='btn' style='border-color:red'>RESET DEFAULT</a></form></div></body></html>";
    server.send(200, "text/html", html);
}

void handleWeatherPage() {
    String html = pageStart("Weather Settings", "#FFD700");
    html += "<a href='/' class='btn'>BACK TO RADIO</a><div class='card'><form action='/setweather' method='POST'>"
        "<b>City, Country:</b><br><input type='text' name='city' maxlength='80' value='" + htmlEscape(owmCity) +
        "'><br><b>OWM API Key:</b><br><input type='password' name='key' maxlength='128' "
        "autocomplete='new-password' placeholder='Leave blank to keep current'>"
        "<br><label><input type='checkbox' name='clear_key'> Clear saved API key</label>"
        "<br><b>Unit:</b><br><input type='radio' name='u' value='C' " +
        String(useCelsius ? "checked" : "") + "> Celsius "
        "<input type='radio' name='u' value='F' " + String(!useCelsius ? "checked" : "") +
        "> Fahrenheit<br><br><input type='submit' value='APPLY' class='btn'></form></div></body></html>";
    server.send(200, "text/html", html);
}

void handleAudioPage() {
    String html = pageStart("Audio Settings", "cyan");
    html += "<a href='/' class='btn'>BACK TO RADIO</a><div class='card'><b>PRESETS</b><br>"
        "<a href='/setpreset?p=rock' class='btn'>ROCK</a><a href='/setpreset?p=pop' class='btn'>POP</a>"
        "<a href='/setpreset?p=jazz' class='btn'>JAZZ</a><a href='/setpreset?p=flat' class='btn'>FLAT</a></div>"
        "<div class='card'><b>3-BAND EQ</b><form action='/seteq'>"
        "<p>Bass <input type='range' name='b' min='-15' max='15' value='" + String(gB) + "'></p>"
        "<p>Mid <input type='range' name='m' min='-15' max='15' value='" + String(gM) + "'></p>"
        "<p>Treble <input type='range' name='t' min='-15' max='15' value='" + String(gT) + "'></p>"
        "<input type='submit' value='APPLY' class='btn'></form></div></body></html>";
    server.send(200, "text/html", html);
}

void handleUpdatePage() {
    server.send(
        200,
        "text/html",
        pageStart("Firmware Update") +
            "<form method='POST' action='/update' enctype='multipart/form-data'>"
            "<input type='file' name='update'><br><br><input type='submit' value='START UPDATE'></form></body></html>");
}

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
    if (!loadPodcastEpisodes(showIndex)) {
        html += "<h3>לא ניתן לטעון פרקים</h3>";
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
    server.on("/stations", handleStations);
    server.on("/skin", handleSkin);
    server.on("/audio", handleAudioPage);
    server.on("/weather", handleWeatherPage);
    server.on("/update_ui", handleUpdatePage);
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
        mainVal = volume;
        audio.setVolume(volCurve[mainVal]);
        lastVolChange = millis();
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
        saveSettings();
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
        stations[stationIndex].url = url;
        stations[stationIndex].name = name;
        saveSettings();
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
        currentStationIdx = (currentStationIdx - 1 + STATION_COUNT) % STATION_COUNT;
        tempStationIdx = currentStationIdx;
        playStation(currentStationIdx);
        saveSettings();
        redirectTo("/");
    });
    server.on("/next", [] {
        currentStationIdx = (currentStationIdx + 1) % STATION_COUNT;
        tempStationIdx = currentStationIdx;
        playStation(currentStationIdx);
        saveSettings();
        redirectTo("/");
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
        if (loadedPodcastShow != showIndex && !loadPodcastEpisodes(showIndex)) {
            server.send(500, "text/plain", "Unable to load podcast");
            return;
        }
        if (episodeIndex >= podcastEpisodeCount) {
            sendBadRequest("Invalid episode");
            return;
        }
        playPodcastEpisode(showIndex, episodeIndex);
        redirectTo("/");
    });

    server.begin();
}
