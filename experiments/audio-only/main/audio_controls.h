#pragma once
#include "audio_suite.h"
namespace audio_test {
struct Request { int id=1, trimDb=0; bool muted=false; uint32_t epoch=1; int slotBits=0, cueId=0, radioIndex=-1, channelMode=-1; bool monoLeft=false; };
// Profile overrides are internal constants; encoder/CLI case IDs are already bounded.
inline Case selectedCase(const Request& r) {
    Case c=kCases[r.id-1];
    if (r.slotBits) c.bits=r.slotBits;
    if (r.channelMode>=0) c.channel=r.channelMode;
    c.monoLeft=r.monoLeft;
    if (r.radioIndex>=0) { c.mapping=Mapping::Radio;c.index=r.radioIndex; }
    return c;
}
// Microsecond timestamps; no long-press event can leak into a short click.
struct Controls {
    Request state;
    uint8_t ab=3;
    int edges=0, selected=1;
    bool raw=false, stable=false, priorMuted=false, turned=false;
    uint64_t rawSince=0, pressedAt=0;
    Request poll(uint64_t now, bool pressed, uint8_t next) {
        if (pressed!=raw) { raw=pressed; rawSince=now; }
        if (raw!=stable && now-rawSince>=20000) {
            stable=raw;
            if (stable) {
                pressedAt=now;priorMuted=state.muted;turned=false;selected=state.id;
                state.muted=true;
            } else {
                const uint64_t held=now-pressedAt;
                if (turned) { state.id=selected; ++state.epoch; state.trimDb=0; state.muted=false; }
                else if (held>=4000000) { ++state.epoch;state.trimDb=0;state.muted=false; }
                else if (held>=1500000) { state.id=state.id%kCaseCount+1; ++state.epoch;state.trimDb=0;state.muted=false; }
                else state.muted=!priorMuted;
            }
        }
        constexpr int8_t transitions[16]={0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};
        if ((ab^next)==3) edges=0;
        else edges+=transitions[(ab<<2)|next];
        ab=next;
        if (std::abs(edges)>=4) {
            const int direction=edges>0 ? 1 : -1; edges=0;
            if (stable) { selected=std::clamp(selected+direction,1,kCaseCount); turned=true; }
            else state.trimDb=std::clamp(state.trimDb+direction,-60,0);
        }
        return state;
    }
};
// First-listening sequence only. No automatic level ladder or replay.
// Timer starts only after the output task confirms the new case's cue ended.
struct HandsFree {
    Request state;
    int stage=0;
    bool started=false;
    uint64_t since=0;
    Request poll(uint64_t now, int activeId, uint32_t activeEpoch, bool holding, bool fault) {
        constexpr int ids[]={1,2,3,4,2};
        constexpr uint64_t durations[]={10000000,10000000,15000000,30000000};
        if (fault || stage==4 || activeId!=state.id || activeEpoch!=state.epoch || !holding) return state;
        if (!started) { since=now; started=true; }
        if (now-since>=durations[stage]) {
            ++stage; state.id=ids[stage];state.trimDb=stage==3 ? -12 : 0;
            ++state.epoch;started=false;
        }
        return state;
    }
};
// Music format A/B/A: same case/source/gain, 30 s each, reset from frame zero.
// The cue IDs are stage numbers 1/2/3, followed by 4 for final stopped output.
struct MusicFormats {
    Request state{4,-12,false,1,16,1};
    explicit MusicFormats(int radioIndex=-1) {
        state.radioIndex=radioIndex;
        if (radioIndex>=0) state.trimDb=0; // exact production gain, no second attenuation
    }
    int stage=0;
    bool started=false;
    uint64_t since=0;
    Request poll(uint64_t now, int activeId, uint32_t activeEpoch, bool holding, bool fault) {
        if (fault || stage==3 || activeId!=state.id || activeEpoch!=state.epoch || !holding) return state;
        if (!started) { since=now;started=true; }
        if (now-since>=30000000) {
            ++stage;++state.epoch;state.cueId=stage+1;started=false;
            state.slotBits=stage==1 ? 32 : 16;
            if (stage==3) { state.id=2;state.trimDb=0;state.radioIndex=-1; }
        }
        return state;
    }
};
// Same source waveform and per-channel level, switching only active data slots.
// All stages use the left source channel, avoiding stereo-content differences.
struct MusicChannels {
    Request state{4,-12,false,1,16,1};
    explicit MusicChannels(int radioIndex=18) {
        state.radioIndex=radioIndex;state.channelMode=1;state.monoLeft=true;
        if (radioIndex>=0) state.trimDb=0;
    }
    int stage=0;
    bool started=false;
    uint64_t since=0;
    Request poll(uint64_t now, int activeId, uint32_t activeEpoch, bool holding, bool fault) {
        if (fault || stage==3 || activeId!=state.id || activeEpoch!=state.epoch || !holding) return state;
        if (!started) { since=now;started=true; }
        if (now-since>=30000000) {
            ++stage;++state.epoch;state.cueId=stage+1;started=false;
            state.channelMode=stage==1 ? 2 : 0;
            if (stage==3) {
                state.id=2;state.trimDb=0;state.radioIndex=-1;
                state.channelMode=-1;state.monoLeft=false;
            }
        }
        return state;
    }
};
} // namespace audio_test
