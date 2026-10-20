#pragma once

// Opt-in listening experiment; normal firmware keeps its persistence behavior.
#if defined(RADIOHEAD_STORAGE_QUIET_TEST)
inline constexpr bool kStorageQuietTest = true;
#else
inline constexpr bool kStorageQuietTest = false;
#endif

// Written once during single-threaded setup, before starting playback/network
// workers. Runtime readers see an immutable value.
inline bool storageBootReadsFinished = false;
inline bool storageReadsAllowed() {
    return !kStorageQuietTest || !storageBootReadsFinished;
}
inline void finishStorageBootReads() {
    storageBootReadsFinished = true;
}
