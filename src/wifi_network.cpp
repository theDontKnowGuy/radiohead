#include "wifi_network.h"

#include <WiFi.h>
#include <cstring>
#include <time.h>

#include "app_state.h"
#include "settings.h"

namespace {
constexpr unsigned long kRoamScanIntervalMs = 10UL * 60UL * 1000UL;
constexpr int32_t kRoamSignalMarginDb = 10;
constexpr uint32_t kRoamScanMaxMsPerChannel = 120;
constexpr unsigned long kBrowserScanRetentionMs = 30UL * 1000UL;
enum class ScanOwner : uint8_t { None, Browser, Roaming };
ScanOwner scanOwner = ScanOwner::None;
bool browserResultsReady = false;
unsigned long browserResultsReadyAt = 0;
bool networkJoinStarted = false;
}  // namespace

WifiScanStartResult startWifiBrowserScan() {
    if (scanOwner == ScanOwner::Roaming || WiFi.scanComplete() == WIFI_SCAN_RUNNING) {
        return WifiScanStartResult::Busy;
    }
    WiFi.scanDelete();
    scanOwner = ScanOwner::None;
    browserResultsReady = false;
    if (WiFi.scanNetworks(true, true) != WIFI_SCAN_RUNNING) {
        return WifiScanStartResult::Failed;
    }
    scanOwner = ScanOwner::Browser;
    return WifiScanStartResult::Started;
}

int wifiBrowserScanComplete() {
    return scanOwner == ScanOwner::Browser ? WiFi.scanComplete() : WIFI_SCAN_FAILED;
}

void clearWifiBrowserScan() {
    if (scanOwner != ScanOwner::Browser) return;
    WiFi.scanDelete();
    scanOwner = ScanOwner::None;
    browserResultsReady = false;
}

void serviceWifiRoaming(unsigned long now) {
    static unsigned long lastScanAt = 0;

    // Keep browser results long enough to fetch, but release abandoned scans
    // without waiting for another HTTP request (including in setup AP mode).
    if (scanOwner == ScanOwner::Browser) {
        const int count = WiFi.scanComplete();
        if (count == WIFI_SCAN_RUNNING) return;
        if (count == WIFI_SCAN_FAILED) {
            clearWifiBrowserScan();
        } else if (!browserResultsReady) {
            browserResultsReady = true;
            browserResultsReadyAt = now;
        } else if (now - browserResultsReadyAt >= kBrowserScanRetentionMs) {
            clearWifiBrowserScan();
        }
        return;
    }

    if (scanOwner == ScanOwner::Roaming) {
        const int count = WiFi.scanComplete();
        if (count == WIFI_SCAN_RUNNING) return;
        scanOwner = ScanOwner::None;
        if (!isAP && count > 0 && WiFi.status() == WL_CONNECTED && WiFi.SSID() == st_ssid) {
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

    if (isAP || st_ssid.isEmpty()) return;
    if (WiFi.status() != WL_CONNECTED || now - lastScanAt < kRoamScanIntervalMs ||
        WiFi.scanComplete() != WIFI_SCAN_FAILED) return;
    lastScanAt = now;
    // Arduino's default active minimum is 100 ms; the maximum must not be 50 ms.
    if (WiFi.scanNetworks(true, false, false, kRoamScanMaxMsPerChannel, 0, st_ssid.c_str()) == WIFI_SCAN_RUNNING) {
        scanOwner = ScanOwner::Roaming;
    }
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
