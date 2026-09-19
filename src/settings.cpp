#include "settings.h"

#include <cctype>

#include "app_state.h"
#include "display.h"

namespace {

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

void normalizeColor(String& value, const char* fallback) {
    if (!isHexColor(value)) {
        value = fallback;
    }
}

void truncate(String& value, size_t maximumLength) {
    if (value.length() > maximumLength) {
        value.remove(maximumLength);
    }
}

}  // namespace

void saveSettings() {
    pref.begin("radio", false);
    pref.putInt("idx", currentStationIdx);
    pref.putInt("vol", mainVal);
    pref.putInt("bass", gB);
    pref.putInt("mid", gM);
    pref.putInt("treb", gT);
    pref.putBool("spec", showSpectrum);
    pref.putInt("almH", alarmH);
    pref.putInt("almM", alarmM);
    pref.putBool("almA", alarmActive);
    pref.putString("cTop", currentSkin.hexTop);
    pref.putString("cBot", currentSkin.hexBottom);
    pref.putString("cMain", currentSkin.hexMain);
    pref.putString("cAcc", currentSkin.hexAccent);
    pref.putString("cWifi", currentSkin.hexWifi);
    pref.putString("owmCity", owmCity);
    pref.putString("owmKey", owmKey);
    pref.putBool("useCelsius", useCelsius);
    pref.putString("cSel", currentSkin.hexSel);
    pref.putString("cClk", currentSkin.hexClk);
    pref.putString("cHInf", currentSkin.hexHInfo);
    pref.putString("cBarL", currentSkin.hexBarL);
    pref.putString("cBarM", currentSkin.hexBarM);
    pref.putString("cBarH", currentSkin.hexBarH);
    pref.putString("cVol", currentSkin.hexVol);
    pref.putString("cAlm", currentSkin.hexAlm);

    for (int i = 0; i < STATION_COUNT; ++i) {
        pref.putString(("n" + String(i)).c_str(), stations[i].name);
        pref.putString(("u" + String(i)).c_str(), stations[i].url);
    }
    pref.end();
}

void loadSettings() {
    pref.begin("radio", true);
    currentStationIdx = pref.getInt("idx", 0);
    mainVal = pref.getInt("vol", 5);
    gB = pref.getInt("bass", 0);
    gM = pref.getInt("mid", 0);
    gT = pref.getInt("treb", 0);
    showSpectrum = pref.getBool("spec", true);
    alarmH = pref.getInt("almH", 7);
    alarmM = pref.getInt("almM", 0);
    alarmActive = pref.getBool("almA", false);
    st_ssid = pref.getString("ssid", "");
    st_pass = pref.getString("pass", "");
    owmCity = pref.getString("owmCity", "Budapest,HU");
    owmKey = pref.getString("owmKey", "");
    useCelsius = pref.getBool("useCelsius", true);

    if (st_ssid.length() > 32) {
        st_ssid = "";
        st_pass = "";
    } else if (st_pass.length() > 63) {
        st_pass = "";
    }
    if (owmCity.isEmpty() || owmCity.length() > 80) {
        owmCity = "Budapest,HU";
    }
    if (owmKey.length() > 128) {
        owmKey = "";
    }
    currentSkin.hexTop = pref.getString("cTop", "#000000");
    currentSkin.hexBottom = pref.getString("cBot", "#000000");
    currentSkin.hexMain = pref.getString("cMain", "#FFFFFF");
    currentSkin.hexAccent = pref.getString("cAcc", "#00FFFF");
    currentSkin.hexWifi = pref.getString("cWifi", "#00FF00");
    currentSkin.hexSel = pref.getString("cSel", "#0000FF");
    currentSkin.hexClk = pref.getString("cClk", "#FFFFFF");
    currentSkin.hexHInfo = pref.getString("cHInf", "#FFFFFF");
    currentSkin.hexBarL = pref.getString("cBarL", "#00FF00");
    currentSkin.hexBarM = pref.getString("cBarM", "#FFFF00");
    currentSkin.hexBarH = pref.getString("cBarH", "#FF0000");
    currentSkin.hexVol = pref.getString("cVol", "#00FFFF");
    currentSkin.hexAlm = pref.getString("cAlm", "#FF0000");

    normalizeColor(currentSkin.hexTop, "#000000");
    normalizeColor(currentSkin.hexBottom, "#000000");
    normalizeColor(currentSkin.hexMain, "#FFFFFF");
    normalizeColor(currentSkin.hexAccent, "#00FFFF");
    normalizeColor(currentSkin.hexWifi, "#00FF00");
    normalizeColor(currentSkin.hexSel, "#0000FF");
    normalizeColor(currentSkin.hexClk, "#FFFFFF");
    normalizeColor(currentSkin.hexHInfo, "#FFFFFF");
    normalizeColor(currentSkin.hexBarL, "#00FF00");
    normalizeColor(currentSkin.hexBarM, "#FFFF00");
    normalizeColor(currentSkin.hexBarH, "#FF0000");
    normalizeColor(currentSkin.hexVol, "#00FFFF");
    normalizeColor(currentSkin.hexAlm, "#FF0000");
    updateColors();

    for (int i = 0; i < STATION_COUNT; ++i) {
        stations[i].name = pref.getString(("n" + String(i)).c_str(), stations[i].name);
        stations[i].url = pref.getString(("u" + String(i)).c_str(), stations[i].url);
        truncate(stations[i].name, 80);
        truncate(stations[i].url, 512);
    }
    pref.end();

    currentStationIdx = constrain(currentStationIdx, 0, STATION_COUNT - 1);
    mainVal = constrain(mainVal, 0, 21);
    gB = constrain(gB, -15, 15);
    gM = constrain(gM, -15, 15);
    gT = constrain(gT, -15, 15);
    alarmH = constrain(alarmH, 0, 23);
    alarmM = constrain(alarmM, 0, 59);
    tempStationIdx = currentStationIdx;
}
