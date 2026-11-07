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
// Loop-owned: only scan after sustained weak RSSI and media reserve approval.
// AP handoffs yield between disconnect/join and bound pinned-target retries.
void serviceWifiRoaming(unsigned long now);
// Persist before applying; defaults to enabled when no setting is stored.
// Does not reconnect. A failed save leaves the active state unchanged.
// Disabling stops future scans/switches; an already-started handoff/fallback
// finishes so a voluntary disconnect cannot strand the radio offline.
bool wifiBackgroundRoamingEnabled();
bool setWifiBackgroundRoaming(bool enabled);

enum class WifiScanStartResult : uint8_t { Started, Busy, Failed };
// Call from the Arduino loop/web handlers. Browser and roaming scans share the
// driver's result buffer; completed browser results expire after 30 seconds.
WifiScanStartResult startWifiBrowserScan();
// Returns WiFi scan status/count for browser-owned results only.
int wifiBrowserScanComplete();
void clearWifiBrowserScan();
