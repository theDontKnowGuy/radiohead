#include "wifi_network.h"

#include <WiFi.h>
#include <cstring>
#include <time.h>

#include "app_state.h"
#include "settings.h"
#include "radio_diagnostics.h"
#include "media.h"

namespace {
constexpr unsigned long kRoamScanIntervalMs = 10UL * 60UL * 1000UL;
constexpr int32_t kRoamSignalMarginDb = 10;
constexpr uint32_t kRoamScanMaxMsPerChannel = 120;
constexpr unsigned long kBrowserScanRetentionMs = 30UL * 1000UL;
constexpr int32_t kRoamWeakRssi = -75;
constexpr uint32_t kRoamWeakMs = 30000;
constexpr uint32_t kRoamJoinMs = 8000;
constexpr uint32_t kRoamDisconnectMs = 1500;
enum class ScanOwner : uint8_t { None, Browser, Roaming };
ScanOwner scanOwner = ScanOwner::None;
bool browserResultsReady = false;
unsigned long browserResultsReadyAt = 0;
bool networkJoinStarted = false;
bool backgroundRoaming = true;
bool weakSignal = false;
uint32_t weakSince = 0;
enum class Handoff : uint8_t { None, DisconnectTarget, JoinTarget, DisconnectFallback };
Handoff handoff = Handoff::None;
uint32_t handoffAt = 0;
uint8_t targetBssid[6] = {};
int32_t targetChannel = 0;
bool savedAutoReconnect = true;
bool pinnedByRoam = false;
bool pinnedDisconnected = false;
uint32_t pinnedDisconnectedAt = 0;

void finishHandoff() {
    WiFi.setAutoReconnect(savedAutoReconnect);
    handoff = Handoff::None;
}

bool startHandoffDisconnect(Handoff next, uint32_t now) {
    if (handoff == Handoff::None) savedAutoReconnect = WiFi.getAutoReconnect();
    WiFi.setAutoReconnect(false);
    if (!WiFi.disconnectAsync(false, false)) {
        finishHandoff();
        return false;
    }
    handoff = next;
    handoffAt = now;
    return true;
}

bool serviceHandoff(uint32_t now) {
    if (handoff == Handoff::DisconnectTarget || handoff == Handoff::DisconnectFallback) {
        // begin() while associated can wait up to one second. Yield until both
        // association and IP status have cleared before configuring the join.
        if (WiFi.STA.connected() || WiFi.status() == WL_CONNECTED) {
            if (uint32_t(now - handoffAt) >= kRoamDisconnectMs) {
                // A delayed voluntary disconnect must not strand the station
                // after we abandon this attempt. Watch for it just like a lost
                // pinned AP and issue an unpinned fallback if it occurs.
                pinnedByRoam = true;
                radioDiagnosticsRoamResult(int32_t(WL_CONNECT_FAILED));
                finishHandoff();
            }
            return true;
        }
        const bool fallback = handoff == Handoff::DisconnectFallback;
        if (!fallback) mediaWifiHandoffStarted();
        const auto result = WiFi.begin(st_ssid.c_str(), st_pass.c_str(),
            fallback ? 0 : targetChannel, fallback ? nullptr : targetBssid);
        if (fallback) {
            // An unpinned all-channel join lets the driver try the old AP or
            // another matching AP. Restore normal reconnect after issuing it.
            pinnedByRoam = result == WL_CONNECT_FAILED;
            pinnedDisconnected = false;
            finishHandoff();
        } else {
            radioDiagnosticsRoamResult(int32_t(result));
            pinnedByRoam = true;
            handoff = Handoff::JoinTarget;
            handoffAt = now;
        }
        return true;
    }
    if (handoff == Handoff::JoinTarget) {
        if (WiFi.status() == WL_CONNECTED) {
            finishHandoff();
        } else if (uint32_t(now - handoffAt) >= kRoamJoinMs) {
            startHandoffDisconnect(Handoff::DisconnectFallback, now);
        }
        return true;
    }
    // A successful roam also leaves a BSSID pin. Bound later retries against
    // that AP, even if the user has since disabled voluntary roaming.
    if (pinnedByRoam && WiFi.status() != WL_CONNECTED) {
        if (!pinnedDisconnected) {
            pinnedDisconnected = true;
            pinnedDisconnectedAt = now;
            mediaWifiHandoffStarted();
        } else if (uint32_t(now - pinnedDisconnectedAt) >= kRoamJoinMs) {
            startHandoffDisconnect(Handoff::DisconnectFallback, now);
        }
        return true;
    }
    pinnedDisconnected = false;
    return false;
}
}  // namespace

bool wifiBackgroundRoamingEnabled() { return backgroundRoaming; }
bool setWifiBackgroundRoaming(bool enabled) {
    if (enabled == backgroundRoaming) return true;
    if (!saveWifiRoamingEnabled(enabled)) return false;
    backgroundRoaming = enabled;
    return true;
}

WifiScanStartResult startWifiBrowserScan() {
    if (handoff != Handoff::None || scanOwner == ScanOwner::Roaming || WiFi.scanComplete() == WIFI_SCAN_RUNNING) {
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
    const bool weak = backgroundRoaming && !isAP && WiFi.status() == WL_CONNECTED &&
        WiFi.RSSI() <= kRoamWeakRssi;
    if (!weak) weakSignal = false;
    else if (!weakSignal) {
        weakSignal = true;
        weakSince = uint32_t(now);
    }
    const bool sustainedWeak = weakSignal && uint32_t(now - weakSince) >= kRoamWeakMs;
    if (!isAP && serviceHandoff(uint32_t(now))) return;

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
        if (sustainedWeak && mediaCanRoamWifi() && count > 0 && WiFi.SSID() == st_ssid) {
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
                memcpy(targetBssid, WiFi.BSSID(bestIndex), sizeof(targetBssid));
                targetChannel = WiFi.channel(bestIndex);
                radioDiagnosticsRoamStart(WiFi.BSSID(), targetBssid, targetChannel, currentRssi, bestRssi);
                WiFi.scanDelete();
                if (!startHandoffDisconnect(Handoff::DisconnectTarget, uint32_t(now)))
                    radioDiagnosticsRoamResult(int32_t(WL_CONNECT_FAILED));
                return;
            }
        }
        WiFi.scanDelete();
        return;
    }

    if (!sustainedWeak || st_ssid.isEmpty() || !mediaCanRoamWifi()) return;
    if (WiFi.status() != WL_CONNECTED || now - lastScanAt < kRoamScanIntervalMs ||
        WiFi.scanComplete() != WIFI_SCAN_FAILED) return;
    lastScanAt = now;
    // Arduino's default active minimum is 100 ms; the maximum must not be 50 ms.
    if (WiFi.scanNetworks(true, false, false, kRoamScanMaxMsPerChannel, 0, st_ssid.c_str()) == WIFI_SCAN_RUNNING) {
        scanOwner = ScanOwner::Roaming;
    }
}

bool startSetupAccessPoint(SetupAccessReason reason) {
    if (handoff != Handoff::None) finishHandoff();
    pinnedByRoam = pinnedDisconnected = weakSignal = false;
    backgroundRoaming = loadWifiRoamingEnabled();
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
    if (handoff != Handoff::None) finishHandoff();
    pinnedByRoam = pinnedDisconnected = weakSignal = false;
    backgroundRoaming = loadWifiRoamingEnabled();
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
