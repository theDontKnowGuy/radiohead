"""Simulate production startup/network handoff with a timed Wi-Fi driver.

Uses the actual setup(), join/fallback functions and HTTP/mDNS lifecycle. Boot
audio and TFT drawing are timed seams; this does not validate physical hardware.
"""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / ".pio/startup_native"
out.mkdir(parents=True, exist_ok=True)
main = (root / "src/main.cpp").read_text()
web = (root / "src/web_server.cpp").read_text()
wifi = (root / "src/wifi_network.cpp").read_text()
setup = main[main.index("void setup() {"):main.index("\nvoid loop() {")]
mdns = web[web.index("void startWebMdns() {"):web.index("\nString networkMessage;")]
connectivity = web[web.index("void serviceWebConnectivity() {"):]
listener = web[web.index("    server.begin();\n    webStationConnected"):web.index("\nvoid serviceWebConnectivity() {")]
join = wifi[wifi.index("bool startSetupAccessPoint("):]

source = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>
#include "boot_screen.h"
#include "wifi_network.h"

struct String : std::string {
    using std::string::string;
    bool isEmpty() const { return empty(); }
};
unsigned long tick = 0, joinAt = 0, connectionDelay = 0, bootDuration = 6750;
unsigned long firstQrAt = 0, bootEndedAt = 0, playbackAt = 0;
bool requestedSetup = false, joined = false, isAP = false, flap = false;
bool radioStarted = false, ntpStarted = false;
bool networkJoinStarted = false;
bool webStationConnected = false, webSetupAccessPoint = false, webMdnsStarted = false;
enum class SetupAccessReason : uint8_t { Requested, NoCredentials, ConnectionFailed };
SetupAccessReason setupAccessReason = SetupAccessReason::Requested;
const char *kSetupAccessPointSsid = "Radio_Setup", *kRadioMdnsHostname = "radio";
const char *ntpServer = "test.invalid";
String st_ssid = "test-network", st_pass;
unsigned long millis() { return tick; }
void delay(unsigned long ms) { tick += ms; assert(tick < 40000); }
constexpr int INPUT_PULLUP = 0, PIN_SW = 7, LOW = 0;
constexpr int WIFI_STA = 1, WIFI_AP_STA = 3, WL_CONNECTED = 3;
constexpr int WIFI_ALL_CHANNEL_SCAN = 1, WIFI_CONNECT_AP_BY_SIGNAL = 1;
void pinMode(int, int) {}
int digitalRead(int) { return requestedSetup ? LOW : 1; }
struct SerialStub {
    void begin(int) {}
    void print(const char*) {}
    template<class... Args> void println(Args...) {}
    template<class... Args> void printf(Args...) {}
} Serial;
struct Address { String toString() const { return "192.0.2.2"; } };
struct WifiStub {
    int status() const {
        if (!joined || tick - joinAt < connectionDelay) return 0;
        if (flap && tick >= 12000 && tick < 14000) return 0;
        return WL_CONNECTED;
    }
    void begin(const char*, const char*) { joined = true; joinAt = tick; }
    void disconnect(bool, bool) { joined = false; }
    void mode(int) {}
    bool softAP(const char*) { return true; }
    Address localIP() const { return {}; }
    Address softAPIP() const { return {}; }
    String BSSIDstr() const { return "00:00:00:00:00:00"; }
    int RSSI() const { return -50; }
    void setHostname(const char*) {}
    void setScanMethod(int) {}
    void setSortMethod(int) {}
} WiFi;
void configTime(int, int, const char*) { ntpStarted = true; }
void applyConfiguredTimeZone() {}
struct ServerStub {
    bool listening = false;
    unsigned begins = 0, handles = 0;
    void begin() { listening = true; ++begins; }
    void stop() { listening = false; }
    void handleClient() { ++handles; }
} server;
struct MdnsStub {
    unsigned begins = 0, http = 0, spotify = 0, txt = 0;
    bool begin(const char*) { assert(WiFi.status() == WL_CONNECTED); ++begins; return true; }
    bool addService(const char* name, const char*, int) {
        if (std::string(name) == "http") ++http;
        else { assert(std::string(name) == "spotify-connect"); ++spotify; }
        return true;
    }
    void addServiceTxt(const char*, const char*, const char*, const char*) { ++txt; }
} MDNS;
void serviceWebConnectivity();
void startWebServer();
unsigned restartsServiced = 0, audioServiced = 0;
void serviceWebNetworkRequests(unsigned long) { ++restartsServiced; }
struct AudioStub { void loop() { ++audioServiced; } } audio;
bool mediaLocalAudioAvailable() { return true; }
void mediaPrepareBootOutput() {}
void mediaConfigureOutput() {}
void validationBegin() {}
void validationEvent(const char*) {}
void initializeDisplay() {}
void initializeTouchCalibration() {}
void loadSettings() {}
void stationArtworkBegin() {}
void startDeviceControl() {}
unsigned long lastInteraction = 0;
bool firmwareAutoUpdate = false, forceRedraw = false;
void uiControllerBegin() {}
struct UpdaterStub { void begin(bool) { assert(!isAP); } } firmwareUpdater;
namespace BootScreen {
    void draw() { assert(joined || isAP); }
    void startAudio() {}
    void hold(unsigned long) { delay(bootDuration); bootEndedAt = millis(); }
}
enum class QrState { Joining, Connected, Setup };
std::vector<QrState> qrStates;
void showNetworkQrScreen() {
    if (qrStates.empty()) { firstQrAt = millis(); assert(firstQrAt == bootEndedAt); }
    qrStates.push_back(isAP ? QrState::Setup :
        WiFi.status() == WL_CONNECTED ? QrState::Connected : QrState::Joining);
}
void mediaStartSavedPlayback() {
    playbackAt = millis();
    radioStarted = !isAP;
    assert(isAP || WiFi.status() == WL_CONNECTED);
    assert(server.listening);
    if (radioStarted) assert(ntpStarted && MDNS.begins == 1 && MDNS.http == 1);
}
'''
source += join + "\n" + mdns
source += "\nvoid startWebServer() {\n" + listener
source += "\n" + connectivity + "\n" + setup
source += r'''
int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string scenario = argv[1];
    connectionDelay = 1000;
    if (scenario == "during-qr") connectionDelay = 8000;
    if (scenario == "after-qr") { bootDuration = 2000; connectionDelay = 14000; }
    if (scenario == "missing" || scenario == "short-boot-missing")
        connectionDelay = std::numeric_limits<unsigned long>::max();
    if (scenario == "short-boot-missing") bootDuration = 2000;
    if (scenario == "no-credentials") st_ssid.clear();
    if (scenario == "requested-setup") requestedSetup = true;
    if (scenario == "flap") { connectionDelay = 8000; flap = true; }
    setup();
    assert(firstQrAt == bootDuration); // No Wi-Fi wait before the second screen.
    if (scenario == "no-credentials" || scenario == "requested-setup") {
        assert(!joined && isAP && !radioStarted && playbackAt == bootDuration);
        assert(qrStates.front() == QrState::Setup && MDNS.begins == 0);
        assert(setupAccessReason == (requestedSetup ? SetupAccessReason::Requested : SetupAccessReason::NoCredentials));
    } else {
        assert(joinAt == 0 && playbackAt >= firstQrAt + 10000);
        assert(server.handles > 0 && restartsServiced == server.handles);
        assert(audioServiced == server.handles);
        if (scenario == "missing" || scenario == "short-boot-missing") {
            assert(playbackAt == std::max(bootDuration + 10000, NETWORK_JOIN_TIMEOUT_MS));
            assert(!radioStarted && isAP && setupAccessReason == SetupAccessReason::ConnectionFailed);
            assert(qrStates.front() == QrState::Joining && qrStates.back() == QrState::Setup);
            assert(MDNS.begins == 0 && server.begins == 2);
        } else {
            assert(radioStarted && !isAP && playbackAt >= connectionDelay);
            if (scenario == "after-qr") {
                // At the first successful status sample the expired QR hold
                // ends directly; HTTP/mDNS are still started before playback.
                assert(playbackAt == 14000 && qrStates.back() == QrState::Joining);
            } else {
                assert(qrStates.back() == QrState::Connected);
            }
            if (scenario != "early") assert(qrStates.front() == QrState::Joining);
            assert(MDNS.begins == 1); // Reconnect never re-registers services.
#if defined(RADIO_SPOTIFY_EXPERIMENT)
            assert(MDNS.spotify == 1 && MDNS.txt == 3);
#endif
        }
    }
    printf("Startup passed: %s; QR at %lu ms, radio/fallback at %lu ms\n",
           argv[1], firstQrAt, playbackAt);
}
'''
(out / "startup.cpp").write_text(source)
# Arduino.h is a seam for timing/types; production public headers are included.
(out / "Arduino.h").write_text("#pragma once\n#include <cstdint>\n")
for spotify in (False, True):
    exe = out / ("startup-spotify" if spotify else "startup")
    subprocess.run([
        "clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=address,undefined", f"-I{out}", f"-I{root / 'include'}",
        *(["-DRADIO_SPOTIFY_EXPERIMENT"] if spotify else []),
        str(out / "startup.cpp"), "-o", str(exe),
    ], check=True)
    for scenario in ("early", "during-qr", "after-qr", "missing", "short-boot-missing",
                     "no-credentials", "requested-setup", "flap"):
        subprocess.run([str(exe), scenario], check=True)
