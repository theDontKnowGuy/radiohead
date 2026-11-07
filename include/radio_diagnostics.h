#pragma once
#include <cstddef>
#include <cstdint>
class String;

enum class RadioPhase : uint32_t { Boot, Audio, Web, Network, Weather, Controls, Media, Display, Idle, Diagnostics };
enum RadioEvent : uint32_t {
    RadioSlow = 1, RadioLost = 2, RadioEof = 4, RadioReady = 8,
    RadioRecovery = 16, RadioTimeout = 32, RadioStopped = 64,
    RadioState = 128, RadioWifi = 256, RadioPcmStall = 512,
    RadioPcmResumed = 1024, RadioLoopGap = 2048, RadioAllocation = 4096, RadioOutputTimeout = 8192,
};
// Begin after artwork storage mounts, before any audio starts. Other calls are
// loop-owned except the three atomic-only signal functions below.
void radioDiagnosticsBegin(bool filesystemReady);
bool radioDiagnosticsDetailedEnabled();
// Persist first; an unsuccessful save leaves the running mode unchanged.
bool radioDiagnosticsSetDetailedEnabled(bool enabled);
void radioDiagnosticsPhase(RadioPhase phase);
// Loop-owned fixed numeric breadcrumbs; active even when detailed logging is off.
// Web code = route ID * 16 + HTTPMethod (see web_diagnostic_routes.h).
enum class RadioMediaOperation : uint32_t { Playlist = 1, Connect = 2, AwaitPlayback = 3, RetryWait = 4 };
void radioDiagnosticsWebBegin(uint32_t route, uint32_t method);
void radioDiagnosticsWebEnd(int32_t status);
void radioDiagnosticsMediaBegin(RadioMediaOperation operation, uint32_t attempt);
void radioDiagnosticsMediaEnd(int32_t result);
void radioDiagnosticsPlaybackReady();
void radioDiagnosticsMediaRetryWait(uint32_t attempt);
void radioDiagnosticsTick();
void radioDiagnosticsSignal(uint32_t events);
void radioDiagnosticsPcmBlock();
void radioDiagnosticsAllocationFailure(size_t bytes);
// Loop-owned, bounded numeric instrumentation; never changes reconnect policy.
uint32_t radioDiagnosticsApId(const uint8_t* bssid);
void radioDiagnosticsRoamStart(const uint8_t* from, const uint8_t* target,
                               int32_t channel, int32_t fromRssi, int32_t targetRssi);
void radioDiagnosticsRoamResult(int32_t status);
// Eight physical slots/page; sort by sequence offline.
String radioDiagnosticsJson(const String& bank, size_t offset);
