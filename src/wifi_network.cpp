#include "wifi_network.h"

#include <WiFi.h>
#include <cstring>
#include <time.h>

#include "app_state.h"
#include "settings.h"

namespace {
constexpr unsigned long kRoamScanIntervalMs = 10UL * 60UL * 1000UL;
constexpr int32_t kRoamSignalMarginDb = 10;
bool networkJoinStarted = false;
}  // namespace

void serviceWifiRoaming(unsigned long now) {
    static unsigned long lastScanAt = 0;
    static bool ownScan = false;
    if (isAP || st_ssid.isEmpty()) return;

    if (ownScan) {
        const int count = WiFi.scanComplete();
        if (count == WIFI_SCAN_RUNNING) return;
        ownScan = false;
        if (count > 0 && WiFi.status() == WL_CONNECTED && WiFi.SSID() == st_ssid) {
            const int32_t currentRssi = WiFi.RSSI();
            const String currentBssid = WiFi.BSSIDstr();
            int bestIndex = -1;
            int32_t bestRssi = currentRssi + kRoamSignalMarginDb;
            for (int i = 0; i < count; ++i) {
                if (WiFi.SSID(i) != st_ssid || WiFi.BSSIDstr(i) == currentBssid) continue;
                const int32_t rssi = WiFi.RSSI(i);
                if (rssi >= bestRssi) {
                    bestRssi = rssi;
                    bestIndex = i;
                }
            }
            if (bestIndex >= 0) {
                uint8_t bssid[6];
                memcpy(bssid, WiFi.BSSID(bestIndex), sizeof(bssid));
                const int32_t channel = WiFi.channel(bestIndex);
                WiFi.scanDelete();
                Serial.printf("[wifi] roaming to stronger AP: %ld -> %ld dBm\n",
                              static_cast<long>(currentRssi), static_cast<long>(bestRssi));
                WiFi.begin(st_ssid.c_str(), st_pass.c_str(), channel, bssid);
                return;
            }
        }
        WiFi.scanDelete();
        return;
    }

    if (WiFi.status() != WL_CONNECTED || now - lastScanAt < kRoamScanIntervalMs ||
        WiFi.scanComplete() != WIFI_SCAN_FAILED) return;
    lastScanAt = now;
    ownScan = WiFi.scanNetworks(true, false, false, 50, 0, st_ssid.c_str()) == WIFI_SCAN_RUNNING;
}

bool startSetupAccessPoint(SetupAccessReason reason) {
    // Retain the station interface for the configuration page's asynchronous
    // network scan while presenting the open setup AP.
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_AP_STA);
    if (!WiFi.softAP(kSetupAccessPointSsid)) {
        Serial.println("[wifi] setup access point failed to start");
        return false;
    }
    isAP = true;
    setupAccessReason = reason;
    Serial.print("[wifi] setup access point: ");
    Serial.println(WiFi.softAPIP());
    return true;
}

void beginNetworkConnection() {
    WiFi.mode(WIFI_STA);
    // This must precede WiFi.begin() so the DHCP request and the mDNS responder
    // agree on the radio's name.
    WiFi.setHostname(kRadioMdnsHostname);
    // Every access point in the house may advertise the same SSID.  The default
    // fast scan stops at the first match, so scan every channel before selecting
    // the matching access point with the strongest signal.
    WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
    WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
    WiFi.begin(st_ssid.c_str(), st_pass.c_str());
    networkJoinStarted = true;
}

bool networkJoinPending() {
    return networkJoinStarted && WiFi.status() != WL_CONNECTED;
}

void finishNetworkConnection() {
    if (!networkJoinStarted) return;
    if (WiFi.status() != WL_CONNECTED) {
        startSetupAccessPoint(SetupAccessReason::ConnectionFailed);
        return;
    }

    Serial.printf("[wifi] connected: ip=%s bssid=%s rssi=%ld dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.BSSIDstr().c_str(),
                  static_cast<long>(WiFi.RSSI()));
    configTime(0, 0, ntpServer);
    applyConfiguredTimeZone();
}
