#include "wifi_network.h"
#include "radio_diagnostics.h"
#include "app_state.h"
#include "WiFi.h"
#include "Preferences.h"
#include <iostream>
#include <limits>

int roamStarts=0, roamResults=0, handoffs=0;
bool mediaReady = true;
bool mediaCanRoamWifi() { return mediaReady; }
void mediaWifiHandoffStarted() { ++handoffs; }
void radioDiagnosticsRoamStart(const uint8_t* from, const uint8_t* target, int32_t channel, int32_t fromRssi, int32_t targetRssi) {
    assert(from && target && channel>0 && targetRssi>=fromRssi+10); ++roamStarts;
}
void radioDiagnosticsRoamResult(int32_t status) { assert(status==0 || status==WL_CONNECT_FAILED); ++roamResults; }
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
    if (scenario != "weak_wrap") serviceWifiRoaming(0);
    if (scenario == "startup_station" || scenario == "startup_ap") {
        assert(!WiFi.staticBuffers && !WiFi.initialized);
        if (scenario == "startup_station") beginNetworkConnection();
        else assert(startSetupAccessPoint(SetupAccessReason{}));
        assert(WiFi.staticBuffers && WiFi.initialized && WiFi.bufferSelections == 1);
        // Fallback/retry must retain the initial policy without trying to
        // reconfigure an already initialized driver.
        beginNetworkConnection();
        assert(startSetupAccessPoint(SetupAccessReason{}));
        assert(WiFi.bufferSelections == 1);
    } else if (scenario == "selection") {
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
        assert(WiFi.joins == 0 && handoffs == 0 && !WiFi.autoReconnect);
        // No begin while association is still active, even if IP is gone.
        WiFi.connected = false;
        serviceWifiRoaming(interval + 2); assert(WiFi.joins == 0);
        WiFi.disconnected(); serviceWifiRoaming(interval + 3);
        assert(WiFi.joins == 1 && WiFi.joinedChannel == 11 && handoffs == 1);
        assert(WiFi.joinedBssid == nearby.bssid);
        assert(WiFi.scanComplete() == WIFI_SCAN_FAILED);
        WiFi.gotIp(); serviceWifiRoaming(interval + 4);
        assert(WiFi.autoReconnect);
        serviceWifiRoaming(interval * 2 - 1);
        assert(WiFi.scans == 1);
        startRoaming(interval * 2);
        assert(roamStarts==1 && roamResults==1);
    } else if (scenario == "disabled") {
        assert(wifiBackgroundRoamingEnabled());
        setWifiBackgroundRoaming(false);
        serviceWifiRoaming(interval*3);
        assert(WiFi.scans==0 && WiFi.joins==0);
        assert(startWifiBrowserScan()==WifiScanStartResult::Started);
        WiFi.complete({nearby}); clearWifiBrowserScan();
        setWifiBackgroundRoaming(true);
        serviceWifiRoaming(interval*4 - 30000);
        startRoaming(interval*4);
        setWifiBackgroundRoaming(false);
        WiFi.complete({nearby});
        serviceWifiRoaming(interval*4+1);
        assert(WiFi.joins==0 && WiFi.scanComplete()==WIFI_SCAN_FAILED);
        assert(roamStarts==0 && roamResults==0);
    } else if (scenario == "persistence") {
        beginNetworkConnection();
        assert(wifiBackgroundRoamingEnabled() && roamingWrites == 0);
        assert(setWifiBackgroundRoaming(false) && roamingWrites == 1);
        assert(!wifiBackgroundRoamingEnabled() && !savedRoaming);
        assert(setWifiBackgroundRoaming(false) && roamingWrites == 1);
        // Production startup must reload the saved state before scans run.
        beginNetworkConnection();
        assert(!wifiBackgroundRoamingEnabled());
        serviceWifiRoaming(interval * 3);
        assert(WiFi.scans == 0);
        failRoamingOpen = true;
        assert(!setWifiBackgroundRoaming(true) && !wifiBackgroundRoamingEnabled());
        failRoamingOpen = false; failRoamingWrite = true;
        assert(!setWifiBackgroundRoaming(true) && !wifiBackgroundRoamingEnabled());
        assert(!savedRoaming && roamingWrites == 1);
        failRoamingWrite = false;
        assert(setWifiBackgroundRoaming(true) && savedRoaming && roamingWrites == 2);
        savedRoaming = false; // Simulate the next boot reading a stored off value.
        startSetupAccessPoint(SetupAccessReason{});
        assert(!wifiBackgroundRoamingEnabled());
        roamingKeyExists = false; // Factory reset restores the enabled default.
        beginNetworkConnection();
        assert(wifiBackgroundRoamingEnabled());
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
        WiFi.disconnected(); serviceWifiRoaming(interval * 2 + 2);
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
        serviceWifiRoaming(readyAt + 30001UL);
        startRoaming(readyAt + 60001UL < interval ? interval : readyAt + 60001UL);
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
    } else if (scenario == "healthy") {
        WiFi.signal = -55;
        serviceWifiRoaming(interval); serviceWifiRoaming(interval * 2);
        assert(WiFi.scans == 0);
        WiFi.signal = -75; serviceWifiRoaming(interval * 2 + 1);
        serviceWifiRoaming(interval * 2 + 30000); assert(WiFi.scans == 0);
        WiFi.signal = -74; serviceWifiRoaming(interval * 2 + 30001);
        WiFi.signal = -75; serviceWifiRoaming(interval * 2 + 30002);
        serviceWifiRoaming(interval * 2 + 60001); assert(WiFi.scans == 0);
        serviceWifiRoaming(interval * 2 + 60002); assert(WiFi.scans == 1);
        WiFi.signal = -55; WiFi.complete({nearby});
        serviceWifiRoaming(interval * 2 + 60003);
        assert(WiFi.joins == 0 && handoffs == 0);
    } else if (scenario == "buffer") {
        mediaReady = false; serviceWifiRoaming(0); serviceWifiRoaming(interval);
        assert(WiFi.scans == 0);
        mediaReady = true; startRoaming(interval + 1);
        mediaReady = false; WiFi.complete({nearby}); serviceWifiRoaming(interval + 2);
        assert(WiFi.joins == 0 && handoffs == 0);
    } else if (scenario == "fallback" || scenario == "later_loss" || scenario == "join_failure") {
        startRoaming(); WiFi.complete({nearby}); serviceWifiRoaming(interval + 1);
        WiFi.disconnected(); serviceWifiRoaming(interval + 2);
        assert(WiFi.joins == 1 && !WiFi.autoReconnect);
        unsigned long failedAt = interval + 2;
        if (scenario == "later_loss") {
            WiFi.gotIp(); serviceWifiRoaming(interval + 3);
            assert(WiFi.autoReconnect);
            setWifiBackgroundRoaming(false);
            WiFi.disconnected(); failedAt = interval + 100;
            serviceWifiRoaming(failedAt); assert(handoffs == 2);
        }
        serviceWifiRoaming(failedAt + 7999); assert(WiFi.joins == 1);
        serviceWifiRoaming(failedAt + 8000);
        assert(startWifiBrowserScan() == WifiScanStartResult::Busy);
        WiFi.joinFails = scenario == "join_failure";
        serviceWifiRoaming(failedAt + 8001);
        assert(WiFi.joins == 2 && WiFi.joinedChannel == 0);
        assert((WiFi.joinedBssid == std::array<uint8_t, 6>{}));
        assert(WiFi.autoReconnect);
        if (WiFi.joinFails) {
            // Failure to install an unpinned configuration must remain bounded.
            serviceWifiRoaming(failedAt + 8002);
            serviceWifiRoaming(failedAt + 16002);
            WiFi.joinFails = false;
            serviceWifiRoaming(failedAt + 16003); assert(WiFi.joins == 3);
        }
        WiFi.gotIp(); serviceWifiRoaming(failedAt + 20000);
    } else if (scenario == "disconnect_failure" || scenario == "disconnect_timeout") {
        startRoaming(); WiFi.complete({nearby});
        WiFi.disconnectFails = scenario == "disconnect_failure";
        serviceWifiRoaming(interval + 1);
        serviceWifiRoaming(interval + 1501);
        assert(WiFi.joins == 0 && WiFi.autoReconnect);
        assert(startWifiBrowserScan() == WifiScanStartResult::Started);
        WiFi.complete({}); clearWifiBrowserScan();
        assert(handoffs == 0);
        if (scenario == "disconnect_timeout") {
            // The requested disconnect can arrive after our timeout.
            WiFi.disconnected(); serviceWifiRoaming(interval + 2000);
            serviceWifiRoaming(interval + 10000); serviceWifiRoaming(interval + 10001);
            assert(WiFi.joins == 1 && WiFi.joinedChannel == 0 && WiFi.autoReconnect);
        }
    } else if (scenario == "weak_wrap") {
        const unsigned long start = UINT32_MAX - 10000UL;
        serviceWifiRoaming(start);
        serviceWifiRoaming(uint32_t(start + 29999)); assert(WiFi.scans == 0);
        serviceWifiRoaming(uint32_t(start + 30000));
        // Cooldown is still measured from boot; at 20 seconds after wrap a scan
        // is deferred until the interval unless one was previously issued.
        assert(WiFi.scans == 0);
        serviceWifiRoaming(interval); assert(WiFi.scans == 1);
    } else {
        assert(false && "unknown scenario");
    }
    std::cout << "Wi-Fi regression passed: " << scenario << '\n';
}
