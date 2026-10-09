#include "wifi_network.h"
#include "app_state.h"
#include "WiFi.h"
#include <iostream>
#include <limits>

constexpr unsigned long interval = 600000;
const TestAp nearby{"home", "nearby", -45, 11, {1, 2, 3, 4, 5, 6}};

void startRoaming(unsigned long now = interval) {
    serviceWifiRoaming(now);
    assert(WiFi.scanComplete() == WIFI_SCAN_RUNNING);
}

void checkRoamingOwnership() {
    const int scans = WiFi.scans;
    const int deletes = WiFi.deletes;
    assert(startWifiBrowserScan() == WifiScanStartResult::Busy);
    assert(wifiBrowserScanComplete() == WIFI_SCAN_FAILED);
    clearWifiBrowserScan();
    assert(WiFi.scans == scans && WiFi.deletes == deletes);
}

int main(int argc, char** argv) {
    assert(argc == 2);
    const String scenario = argv[1];
    if (scenario == "selection") {
        serviceWifiRoaming(interval - 1);
        assert(WiFi.scans == 0);
        startRoaming();
        checkRoamingOwnership();
        WiFi.complete({{"other", "other", -20, 1, {}},
                       {"home", "current", -10, 1, {}},
                       {"home", "remote", -60, 6, {}}, nearby});
        // Protect results even after the driver completes but before service.
        checkRoamingOwnership();
        serviceWifiRoaming(interval + 1);
        assert(WiFi.joins == 1 && WiFi.joinedChannel == 11);
        assert(WiFi.joinedBssid == nearby.bssid);
        assert(WiFi.scanComplete() == WIFI_SCAN_FAILED);
        serviceWifiRoaming(interval * 2 - 1);
        assert(WiFi.scans == 1);
        startRoaming(interval * 2);
    } else if (scenario == "margin") {
        startRoaming();
        TestAp candidate = nearby;
        candidate.rssi = WiFi.signal + 9;
        WiFi.complete({candidate});
        serviceWifiRoaming(interval + 1);
        assert(WiFi.joins == 0);
        startRoaming(interval * 2);
        candidate.rssi = WiFi.signal + 10;
        WiFi.complete({candidate});
        serviceWifiRoaming(interval * 2 + 1);
        assert(WiFi.joins == 1);
    } else if (scenario == "abandoned" || scenario == "setup" || scenario == "wrap") {
        isAP = scenario == "setup";
        const unsigned long readyAt = scenario == "wrap"
            ? std::numeric_limits<unsigned long>::max() - 10000UL : interval;
        assert(startWifiBrowserScan() == WifiScanStartResult::Started);
        assert(startWifiBrowserScan() == WifiScanStartResult::Busy);
        serviceWifiRoaming(readyAt);
        assert(WiFi.scans == 1);
        WiFi.complete(isAP ? std::vector<TestAp>{} : std::vector<TestAp>{nearby});
        serviceWifiRoaming(readyAt);
        const int count = isAP ? 0 : 1;
        assert(wifiBrowserScanComplete() == count);
        serviceWifiRoaming(readyAt + 29999UL);
        assert(wifiBrowserScanComplete() == count);
        serviceWifiRoaming(readyAt + 30000UL);
        assert(wifiBrowserScanComplete() == WIFI_SCAN_FAILED);
        assert(WiFi.scanComplete() == WIFI_SCAN_FAILED);
        // Leaving setup mode can resume roaming; expiry also works while AP-only.
        isAP = false;
        startRoaming(readyAt + 30001UL < interval ? interval : readyAt + 30001UL);
    } else if (scenario == "consumed") {
        assert(startWifiBrowserScan() == WifiScanStartResult::Started);
        WiFi.complete({nearby});
        serviceWifiRoaming(interval);
        assert(wifiBrowserScanComplete() == 1);
        clearWifiBrowserScan();
        startRoaming(interval + 1);
    } else if (scenario == "failure") {
        WiFi.scanFails = true;
        assert(startWifiBrowserScan() == WifiScanStartResult::Failed);
        WiFi.scanFails = false;
        assert(startWifiBrowserScan() == WifiScanStartResult::Started);
        // Driver timeout/failure after the scan started must release ownership.
        WiFi.scanState = WIFI_SCAN_FAILED;
        serviceWifiRoaming(interval);
        startRoaming(interval + 1);
        WiFi.scanState = WIFI_SCAN_FAILED;
        serviceWifiRoaming(interval + 2);
        assert(startWifiBrowserScan() == WifiScanStartResult::Started);
    } else {
        assert(false && "unknown scenario");
    }
    std::cout << "Wi-Fi regression passed: " << scenario << '\n';
}
