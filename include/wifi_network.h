#pragma once

#include <Arduino.h>

enum class SetupAccessReason : uint8_t;

constexpr unsigned long NETWORK_JOIN_TIMEOUT_MS = 30UL * 500UL;

// Retain the station interface for browser-initiated scans in setup mode.
bool startSetupAccessPoint(SetupAccessReason reason);
void beginNetworkConnection();
// BootScreen uses this callback while joining in parallel with its animation.
bool networkJoinPending();
// No-op if a station join was not started (e.g. an explicitly requested AP).
void finishNetworkConnection();
void serviceWifiRoaming(unsigned long now);
