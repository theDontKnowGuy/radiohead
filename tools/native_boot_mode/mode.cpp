#include "settings.h"
#include "Preferences.h"
#include <cassert>
#include <functional>
#include <iostream>
#include <map>

BootMode bootMode = DEFAULT_BOOT_MODE;
BootMode configuredBootMode = DEFAULT_BOOT_MODE;
constexpr int HTTP_GET = 0, HTTP_POST = 1;
bool webRestartScheduled = false;
bool maintenanceBusy = false;
bool webMaintenanceBusy() { return maintenanceBusy; }

struct FakeServer {
    std::map<int, std::function<void()>> routes;
    String requestedMode, configurationHeader;
    String cacheControl, response;
    int status = 0;
    void on(const char*, int method, std::function<void()> handler) { routes[method] = handler; }
    String arg(const char*) { return requestedMode; }
    String header(const char*) { return configurationHeader; }
    void sendHeader(const char* name, const char* value) {
        assert(std::string(name) == "Cache-Control"); cacheControl = value;
    }
    void send(int code, const char* type, const String& body) {
        assert(std::string(type) == "application/json; charset=utf-8");
        status = code; response = body;
    }
    void request(int method) {
        cacheControl.clear(); response.clear(); status = 0; routes.at(method)();
        assert(cacheControl == "no-store");
    }
} server;

void sendMaintenanceBusy() {
    server.send(409, "application/json; charset=utf-8", "{\"error\":\"Busy\"}");
}

// Generated directly from production handlers by check_boot_modes.py.
#include "routes.inc"

void expectModes(BootMode active, BootMode configured) {
    assert(bootMode == active && configuredBootMode == configured);
}

int main() {
    using namespace fakeNvs;
    values["radio/vol"] = uint8_t(8);
    values["touch/data"] = std::string("fixture-calibration");
    loadBootModeSettings();
    expectModes(BootMode::NewArtwork, BootMode::NewArtwork);
    assert(saveBootMode(BootMode::Original));
    expectModes(BootMode::NewArtwork, BootMode::Original);
    assert(std::get<uint8_t>(values.at("radio/vol")) == 8);
    assert(std::get<std::string>(values.at("touch/data")) == "fixture-calibration");
    loadBootModeSettings(); // Simulated next startup.
    expectModes(BootMode::Original, BootMode::Original);
    assert(saveBootMode(BootMode::NewArtwork));
    expectModes(BootMode::Original, BootMode::NewArtwork);
    loadBootModeSettings();
    expectModes(BootMode::NewArtwork, BootMode::NewArtwork);

    for (uint8_t invalid : {uint8_t(2), uint8_t(255)}) {
        const int previousWrites = writes;
        assert(!saveBootMode(static_cast<BootMode>(invalid)));
        assert(writes == previousWrites);
        values["radio/bootMode"] = invalid;
        loadBootModeSettings();
        expectModes(DEFAULT_BOOT_MODE, DEFAULT_BOOT_MODE);
    }
    values["radio/bootMode"] = std::string("wrong NVS type");
    loadBootModeSettings();
    expectModes(DEFAULT_BOOT_MODE, DEFAULT_BOOT_MODE);
    failOpen = true;
    assert(!saveBootMode(BootMode::Original));
    loadBootModeSettings();
    expectModes(DEFAULT_BOOT_MODE, DEFAULT_BOOT_MODE);
    failOpen = false;
    assert(saveBootMode(BootMode::NewArtwork));
    failWrite = true;
    assert(!saveBootMode(BootMode::Original));
    expectModes(BootMode::NewArtwork, BootMode::NewArtwork);
    loadBootModeSettings(); // Failed write did not change persisted selection.
    expectModes(BootMode::NewArtwork, BootMode::NewArtwork);
    failWrite = false;

    registerBootModeRoutes();
    server.request(HTTP_GET);
    assert(server.status == 200);
    assert(server.response == "{\"bootMode\":1,\"configuredBootMode\":1,\"bootModeRestartRequired\":false}");
    server.requestedMode = "0";
    server.request(HTTP_POST);
    assert(server.status == 403);
    server.configurationHeader = "1";
    const int previousWrites = writes;
    for (const char* invalid : {"", "2", "-1", "256", "01", " 1", "1 ", "1x", "0.0"}) {
        server.requestedMode = invalid;
        server.request(HTTP_POST);
        assert(server.status == 400 && writes == previousWrites);
    }
    server.requestedMode = "0";
    maintenanceBusy = true;
    server.request(HTTP_POST);
    assert(server.status == 409 && writes == previousWrites);
    maintenanceBusy = false;
    webRestartScheduled = true;
    server.request(HTTP_POST);
    assert(server.status == 409 && writes == previousWrites);
    webRestartScheduled = false;
    failWrite = true;
    server.request(HTTP_POST);
    assert(server.status == 500 && writes == previousWrites);
    expectModes(BootMode::NewArtwork, BootMode::NewArtwork);
    failWrite = false;
    server.request(HTTP_POST);
    assert(server.status == 200 && !webRestartScheduled);
    assert(server.response == "{\"bootMode\":1,\"configuredBootMode\":0,\"bootModeRestartRequired\":true}");
    server.request(HTTP_GET);
    assert(server.response == "{\"bootMode\":1,\"configuredBootMode\":0,\"bootModeRestartRequired\":true}");
    server.requestedMode = "1";
    server.request(HTTP_POST); // Reverting a pending change needs no restart.
    assert(server.status == 200);
    expectModes(BootMode::NewArtwork, BootMode::NewArtwork);
    assert(saveBootMode(BootMode::Original));
    values.erase("radio/bootMode"); // Existing factory reset clears radio NVS.
    loadBootModeSettings();
    expectModes(DEFAULT_BOOT_MODE, DEFAULT_BOOT_MODE);
    std::cout << "Boot mode storage and HTTP handler checks passed.\n";
}
