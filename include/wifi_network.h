#pragma once

#include <Arduino.h>

enum class SetupAccessReason : uint8_t;

constexpr unsigned long NETWORK_JOIN_TIMEOUT_MS = 30UL * 500UL;

// Retain the station interface for browser-initiated scans in setup mode.
bool startSetupAccessPoint(SetupAccessReason reason);
void beginNetworkConnection();
// Joining runs in parallel with boot artwork and the configuration QR screen.
bool networkJoinPending();
// Resolve the join before radio startup; switch to setup AP on failure.
// No-op if a station join was not started (e.g. an explicitly requested AP).
void finishNetworkConnection();
void serviceWifiRoaming(unsigned long now);

enum class WifiScanStartResult : uint8_t { Started, Busy, Failed };
// Call from the Arduino loop/web handlers. Browser and roaming scans share the
// driver's result buffer; completed browser results expire after 30 seconds.
WifiScanStartResult startWifiBrowserScan();
// Returns WiFi scan status/count for browser-owned results only.
int wifiBrowserScanComplete();
void clearWifiBrowserScan();
