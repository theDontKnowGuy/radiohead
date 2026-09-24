#pragma once

#include <stdint.h>

// S0 diagnostics are absent from the normal firmware build. All calls are
// main-loop owned; the allocation-failure hook only updates fixed counters.
#if defined(RADIO_VALIDATION_DIAGNOSTICS) && RADIO_VALIDATION_DIAGNOSTICS
void validationBegin();
void validationLoopEnter();
void validationAudioServiced(uint32_t elapsedUs);
void validationWebServiced(uint32_t elapsedUs);
void validationEvent(const char* event);
void validationTick();
#else
inline void validationBegin() {}
inline void validationLoopEnter() {}
inline void validationAudioServiced(uint32_t) {}
inline void validationWebServiced(uint32_t) {}
inline void validationEvent(const char*) {}
inline void validationTick() {}
#endif
