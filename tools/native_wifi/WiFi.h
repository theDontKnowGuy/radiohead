#pragma once
#include <Arduino.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <utility>
#include <vector>

constexpr int WIFI_SCAN_FAILED = -2;
constexpr int WIFI_SCAN_RUNNING = -1;
constexpr int WL_CONNECTED = 3;
constexpr int WIFI_STA = 1;
constexpr int WIFI_AP_STA = 3;
constexpr int WIFI_ALL_CHANNEL_SCAN = 1;
constexpr int WIFI_CONNECT_AP_BY_SIGNAL = 1;

struct TestAp {
    String ssid;
    String id;
    int32_t rssi;
    int32_t channel;
    std::array<uint8_t, 6> bssid;
};

struct TestWiFi {
    int scanState = WIFI_SCAN_FAILED;
    bool connected = true;
    bool scanFails = false;
    int scans = 0;
    int deletes = 0;
    int joins = 0;
    int32_t signal = -75;
    int32_t joinedChannel = 0;
    std::array<uint8_t, 6> joinedBssid{};
    std::vector<TestAp> aps;

    int scanComplete() const { return scanState; }
    void scanDelete() { ++deletes; scanState = WIFI_SCAN_FAILED; aps.clear(); }
    int scanNetworks(bool async, bool hidden, bool passive = false,
                     uint32_t maxMs = 300, uint8_t channel = 0, const char* ssid = nullptr) {
        assert(async && !passive && channel == 0);
        // Model the installed Arduino adapter's 100 ms active minimum.
        assert(maxMs >= 100);
        assert(hidden ? ssid == nullptr : ssid && String(ssid) == "home");
        ++scans;
        scanState = scanFails ? WIFI_SCAN_FAILED : WIFI_SCAN_RUNNING;
        return scanState;
    }
    void complete(std::vector<TestAp> results) {
        aps = std::move(results);
        scanState = static_cast<int>(aps.size());
    }
    int status() const { return connected ? WL_CONNECTED : 0; }
    String SSID() const { return "home"; }
    String SSID(int i) const { return aps.at(i).ssid; }
    String BSSIDstr() const { return "current"; }
    String BSSIDstr(int i) const { return aps.at(i).id; }
    int32_t RSSI() const { return signal; }
    int32_t RSSI(int i) const { return aps.at(i).rssi; }
    uint8_t* BSSID(int i) { return aps.at(i).bssid.data(); }
    int32_t channel(int i) const { return aps.at(i).channel; }
    void begin(const char*, const char*, int32_t channel = 0, const uint8_t* bssid = nullptr) {
        ++joins;
        joinedChannel = channel;
        if (bssid) std::copy(bssid, bssid + 6, joinedBssid.begin());
    }
    void disconnect(bool, bool) {}
    void mode(int) {}
    bool softAP(const char*) { return true; }
    String softAPIP() const { return "192.0.2.1"; }
    void setHostname(const char*) {}
    void setScanMethod(int) {}
    void setSortMethod(int) {}
    struct Address { String toString() const { return "192.0.2.2"; } };
    Address localIP() const { return {}; }
};
inline TestWiFi WiFi;
