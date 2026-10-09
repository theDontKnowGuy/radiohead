#pragma once
#include <Arduino.h>

enum class SetupAccessReason : uint8_t { Requested, ConnectionFailed };
inline bool isAP = false;
inline String st_ssid = "home";
inline String st_pass = "test-only";
inline SetupAccessReason setupAccessReason = SetupAccessReason::Requested;
inline constexpr const char* kSetupAccessPointSsid = "Radiohead-Setup";
inline constexpr const char* kRadioMdnsHostname = "radiohead";
inline constexpr const char* ntpServer = "test.invalid";
struct TestSerial {
    template <typename... Args> void printf(const char*, Args...) {}
    template <typename T> void print(const T&) {}
    template <typename T> void println(const T&) {}
};
inline TestSerial Serial;
inline void configTime(long, int, const char*) {}

