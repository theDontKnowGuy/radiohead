#pragma once
#include <cstddef>
#include <cstdint>
class String;
// Startup only, before audio: validate flash and cache a selected summary.
void crashDiagnosticsBegin();
String crashDiagnosticsJson();
// Explicit maintenance action; exact cached partition fingerprint required.
bool crashDiagnosticsClear(const String& expectedId);
bool crashDiagnosticsEmpty();
#if defined(RADIOHEAD_CRASH_TEST)
bool crashDiagnosticsReadTestPartition(const String& expectedId, size_t offset, uint8_t* bytes, size_t count);
[[noreturn]] void radioCrashTestFault();
#endif
