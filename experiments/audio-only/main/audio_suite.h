#pragma once
// Shared by the firmware and native tests. No hardware, heap or production globals.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "sine_table.h"

namespace audio_test {
constexpr float kPi = 3.14159265358979323846f;
constexpr size_t kBlock = 128;
constexpr int kCurve[] = {0,1,2,3,4,5,6,8,10,12,14,17,20,23,26,30,34,38,42,46,50,55};
inline float dbGain(float db) { return std::pow(10.f, db / 20.f); }
inline float radioDb(int index) {
    const float t = kCurve[std::clamp(index, 0, 21)] / 55.f;
    return std::clamp(-112.f*t*t*t + 172.f*t*t - 60.f, -60.f, 0.f);
}
inline float radioGain(int index) { return index <= 0 ? 0.f : dbGain(radioDb(index)); }
inline float spotifyGain(int index) { return (kCurve[std::clamp(index,0,21)] * 255 / 55) / 255.f; }
inline int32_t pack32(int16_t sample) { return static_cast<int32_t>(sample) * 65536; }

enum class Wave { Silence, Stopped, Sine, Music, Multi, Transition };
enum class Mapping { Direct, Radio, MatchedDb, Spotify };
struct Case {
    const char* name;
    Wave wave = Wave::Sine;
    float peakDb = -30;
    int frequency = 1000;
    int rate = 44100;
    int bits = 16;
    int channel = 0; // 0 both, 1 left only, 2 right only
    Mapping mapping = Mapping::Direct;
    int index = 21;
    int eqBand = -1; // -1 true bypass, 0/1/2 peaking at 250/1000/4000 Hz
    int eqDb = 0;
    bool eqReference = false; // all EQ/reference cases include -12 dB compensation
    bool abrupt = false;
    bool monoLeft = false; // channel-load profile: same left-source waveform on each active slot
};
inline constexpr Case kCases[] = {
    {"active digital silence", Wave::Silence},
    {"stopped output", Wave::Stopped},
    {"first sine -30", Wave::Sine},
    {"local stereo music", Wave::Music, -12},
    {"ladder -48", Wave::Sine,-48}, {"ladder -36", Wave::Sine,-36},
    {"ladder -30", Wave::Sine,-30}, {"ladder -24", Wave::Sine,-24},
    {"ladder -20.73", Wave::Sine,-20.73f}, {"ladder -18", Wave::Sine,-18},
    {"ladder -12", Wave::Sine,-12}, {"ladder -6", Wave::Sine,-6},
    {"frequency 250", Wave::Sine,-30,250}, {"frequency 1000", Wave::Sine,-30,1000},
    {"frequency 4000", Wave::Sine,-30,4000}, {"100Hz 2s burst / 8s silence", Wave::Sine,-36,100},
    {"left only", Wave::Sine,-30,1000,44100,16,1},
    {"right only", Wave::Sine,-30,1000,44100,16,2},
    {"both same per-channel peak", Wave::Sine,-30},
    {"44.1k 16-bit", Wave::Sine,-30,1000,44100,16},
    {"44.1k 32-bit", Wave::Sine,-30,1000,44100,32},
    {"48k 16-bit", Wave::Sine,-30,1000,48000,16},
    {"48k 32-bit", Wave::Sine,-30,1000,48000,32},
    {"normalized multitone", Wave::Multi,-18},
#define CURVE_CASE(label, kind, i) {label, Wave::Music,-12,1000,44100,16,0,Mapping::kind,i}
    CURVE_CASE("radio 11",Radio,11), CURVE_CASE("radio 14",Radio,14),
    CURVE_CASE("radio 15",Radio,15), CURVE_CASE("radio 16",Radio,16),
    CURVE_CASE("radio 17",Radio,17), CURVE_CASE("radio 18",Radio,18), CURVE_CASE("radio 21",Radio,21),
    CURVE_CASE("direct dB matching 11",MatchedDb,11), CURVE_CASE("direct dB matching 14",MatchedDb,14),
    CURVE_CASE("direct dB matching 15",MatchedDb,15), CURVE_CASE("direct dB matching 16",MatchedDb,16),
    CURVE_CASE("direct dB matching 17",MatchedDb,17), CURVE_CASE("direct dB matching 18",MatchedDb,18),
    CURVE_CASE("direct dB matching 21",MatchedDb,21),
    CURVE_CASE("Spotify 11",Spotify,11), CURVE_CASE("Spotify 14",Spotify,14),
    CURVE_CASE("Spotify 15",Spotify,15), CURVE_CASE("Spotify 16",Spotify,16),
    CURVE_CASE("Spotify 17",Spotify,17), CURVE_CASE("Spotify 18",Spotify,18), CURVE_CASE("Spotify 21",Spotify,21),
#undef CURVE_CASE
    {"abrupt 15/16/17/16 every 12s", Wave::Transition,-12,1000,44100,16,0,Mapping::Radio,16,-1,0,false,true},
    {"40ms ramp 15/16/17/16 every 12s", Wave::Transition,-12,1000,44100,16,0,Mapping::Radio,16},
    {"EQ bypass with compensation", Wave::Multi,-18,1000,44100,16,0,Mapping::Direct,21,-1,0,true},
    {"EQ flat with compensation", Wave::Multi,-18,1000,44100,16,0,Mapping::Direct,21,1,0,true},
    {"low +6", Wave::Multi,-18,1000,44100,16,0,Mapping::Direct,21,0,6,true},
    {"low -6", Wave::Multi,-18,1000,44100,16,0,Mapping::Direct,21,0,-6,true},
    {"mid +6", Wave::Multi,-18,1000,44100,16,0,Mapping::Direct,21,1,6,true},
    {"mid -6", Wave::Multi,-18,1000,44100,16,0,Mapping::Direct,21,1,-6,true},
    {"high +6", Wave::Multi,-18,1000,44100,16,0,Mapping::Direct,21,2,6,true},
    {"high -6", Wave::Multi,-18,1000,44100,16,0,Mapping::Direct,21,2,-6,true}
};
constexpr int kCaseCount = sizeof(kCases)/sizeof(kCases[0]);

struct Ramp {
    float value = 0, target = 0, step = 0;
    uint32_t left = 0;
    void set(float next, uint32_t samples) {
        if (next == target) return;
        target = next; left = samples;
        if (!samples) { value = next; step = 0; }
        else step = (next - value) / samples;
    }
    float tick() {
        if (left && --left == 0) value = target;
        else if (left) value += step;
        return value;
    }
};
// RBJ peaking biquad, Q = 1/sqrt(2), transposed direct form II, independent L/R state.
// This deliberately is NOT the radio's low/high shelf EQ implementation.
struct Eq {
    float b0=1,b1=0,b2=0,a1=0,a2=0,z1[2]{},z2[2]{};
    void configure(int band, int db, int rate) {
        *this = Eq{};
        if (band < 0 || db == 0) return; // exact identity for zero gain
        const int centers[] = {250,1000,4000};
        const float A = dbGain(db/2.f), w = 2*kPi*centers[band]/rate;
        const float alpha = std::sin(w)/(2*0.70710678118f), c = std::cos(w);
        const float a0 = 1+alpha/A;
        b0=(1+alpha*A)/a0; b1=-2*c/a0; b2=(1-alpha*A)/a0;
        a1=-2*c/a0; a2=(1-alpha/A)/a0;
    }
    float tick(float x, int ch) {
        const float y=b0*x+z1[ch];
        z1[ch]=b1*x-a1*y+z2[ch]; z2[ch]=b2*x-a2*y;
        return y;
    }
};
inline float sine(uint64_t frame, int hz, int rate) {
    const uint32_t position = static_cast<uint32_t>((frame % rate) * hz % rate);
    const uint32_t scaled = position * 4096;
    const uint32_t index = scaled / rate;
    const float fraction = (scaled % rate) / static_cast<float>(rate);
    return (kSine[index] + fraction*(kSine[(index+1)&4095]-kSine[index]))/32767.f;
}
struct Synth {
    Case test{};
    const int16_t* music = nullptr;
    size_t musicFrames = 0;
    uint64_t frame = 0, clips = 0;
    int peak = 0;
    float sourceGain = 1;
    Ramp gain{};
    Eq eq{};
    void configure(const Case& c, const int16_t* pcm, size_t frames) {
        test=c; music=pcm; musicFrames=frames; frame=clips=0; peak=0;
        sourceGain = dbGain(c.peakDb) * (c.eqReference ? dbGain(-12) : 1);
        if (c.wave == Wave::Multi) sourceGain /= c.rate==44100 ? kMultiPeak44100 : kMultiPeak48000;
        eq.configure(c.eqBand,c.eqDb,c.rate);
        gain = Ramp{};
        beginBlock(); gain.value=gain.target; gain.left=0;
    }
    void beginBlock() {
        float next = 1;
        if (test.mapping == Mapping::Radio) next=radioGain(test.index);
        if (test.mapping == Mapping::MatchedDb) next=dbGain(radioDb(test.index));
        if (test.mapping == Mapping::Spotify) next=spotifyGain(test.index);
        if (test.wave == Wave::Transition) {
            constexpr int indices[] = {15,16,17,16};
            next=radioGain(indices[(frame / (test.rate*12ULL)) % 4]);
        }
        gain.set(next, test.abrupt ? 0 : test.rate*40/1000);
    }
    void next(int16_t& left, int16_t& right, float envelope=1) {
        float x[2] = {};
        if (test.wave == Wave::Music || test.wave == Wave::Transition) {
            if (musicFrames) {
                x[0]=music[(frame%musicFrames)*2]/32767.f;
                x[1]=test.monoLeft ? x[0] : music[(frame%musicFrames)*2+1]/32767.f;
            }
        } else if (test.wave == Wave::Sine) {
            float v = sine(frame,test.frequency,test.rate)*sourceGain;
            if (test.frequency == 100) {
                const uint64_t p=frame%(test.rate*10ULL), end=test.rate*2ULL;
                v *= p>=end ? 0 : std::min({1.f, p/(test.rate*.03f), (end-p-1)/(test.rate*.03f)});
            }
            x[0]=x[1]=v;
        } else if (test.wave == Wave::Multi) {
            // Equal weights; host-computed rate-specific normalization sets the combined peak.
            x[0]=x[1]=(sine(frame,250,test.rate)+sine(frame,1000,test.rate)+sine(frame,4000,test.rate))*sourceGain/3;
        }
        const float level = gain.tick()*envelope;
        int16_t result[2];
        for (int ch=0;ch<2;++ch) {
            float sample = test.eqBand < 0 ? x[ch] : eq.tick(x[ch],ch);
            // Music already has -12 dBFS peak in flash, unlike oscillators.
            if (test.wave == Wave::Music || test.wave == Wave::Transition) sample *= dbGainMusicCompensation();
            sample *= level;
            if ((test.channel == 1 && ch == 1) || (test.channel == 2 && ch == 0)) sample=0;
            if (std::abs(sample)>1.f) ++clips;
            const int value=static_cast<int>(std::lround(std::clamp(sample,-1.f,1.f)*32767));
            peak=std::max(peak,std::abs(value)); result[ch]=static_cast<int16_t>(value);
        }
        left=result[0];right=result[1];++frame;
    }
    float dbGainMusicCompensation() const { return test.eqReference ? dbGain(-12) : 1.f; }
};
// A quiet decimal case ID: tens are 500 Hz; ones 1500 Hz; zero digit is omitted.
// 300 ms lead, 100 ms on/100 ms off per pip, 300 ms digit separator, 500 ms tail.
inline uint32_t cueFrames(int id, int rate) {
    return static_cast<uint32_t>((1100+(id/10+id%10)*200)*static_cast<uint64_t>(rate)/1000);
}
inline int16_t cueSample(uint32_t frame, int id, int rate) {
    const uint32_t ms = frame*1000ULL/rate;
    int local = static_cast<int>(ms)-300;
    int hz=500;
    const int tens=(id/10)*200;
    if (local>=tens) { local-=tens+300; hz=1500; }
    if (local<0 || local >= (hz==500 ? tens : (id%10)*200) || local%200>=100) return 0;
    const float edge=std::min({1.f,(local%200)/5.f,(100-local%200)/5.f});
    return static_cast<int16_t>(std::lround(sine(frame,hz,rate)*32767*0.0158489319f*edge)); // -36 dBFS
}
// Validate progress in bytes before advancing; never round/discard partial frames.
inline bool validProgress(size_t remaining, size_t written, size_t frameBytes) {
    return written<=remaining && written%frameBytes==0;
}
} // namespace audio_test
