#include "audio_notch.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>

void audio_process_raw_samples(int32_t*, int16_t);
uint32_t rate = 44100;
uint32_t mediaLocalSampleRate() { return rate; }
void mediaApplyLocalTone(int, int, int) {}
constexpr double pi = 3.14159265358979323846;

float response(unsigned sampleRate, unsigned center, double signal, unsigned endHz = 0) {
    AudioNotch notch;
    notch.configure(radioNotchConfigurationValue(true, center, endHz), sampleRate);
    double before = 0, after = 0;
    for (unsigned n = 0; n < sampleRate; ++n) {
        const float input = 0.2f * std::sin(2 * pi * signal * n / sampleRate);
        const float output = notch.process(input, 0);
        assert(std::isfinite(output));
        assert(notch.process(0, 1) == 0); // Independent stereo histories.
        if (n > sampleRate / 2) { before += input * input; after += output * output; }
    }
    return 10 * std::log10(after / before);
}

int main() {
    for (unsigned sampleRate : {8000, 16000, 22050, 44100, 48000, 96000}) {
        for (unsigned center : {100, 3000, 10000}) {
            if (center * 2 >= sampleRate) continue;
            const float attenuation = response(sampleRate, center, center);
            // Low center frequencies need longer settling with a narrow notch.
            assert(attenuation < (center == 100 ? -25 : -65));
            assert(response(sampleRate, center, center == 100 ? 1000 : 200) > -0.1f);
            std::printf("%u Hz rate, %u Hz notch: %.1f dB\n", sampleRate, center, attenuation);
        }
    }
    for (unsigned sampleRate : {16000, 22050, 44100, 48000, 96000}) {
        const double center = sampleRate / pi * std::atan(std::sqrt(
            std::tan(pi * 5000 / sampleRate) * std::tan(pi * 5100 / sampleRate)));
        assert(response(sampleRate, 5000, center, 5100) < -65);
        for (unsigned edge : {5000, 5100}) {
            const float gain = response(sampleRate, 5000, edge, 5100);
            assert(gain > -3.05f && gain < -2.97f);
        }
        for (unsigned inside : {5025, 5075}) assert(response(sampleRate, 5000, inside, 5100) < -6);
        for (unsigned outside : {1000, 7000}) assert(response(sampleRate, 5000, outside, 5100) > -0.1f);
        std::printf("%u Hz rate, 5000-5100 Hz: center suppressed; -3 dB edges verified\n", sampleRate);
    }
    // User's broad-band test: zero is valid ONLY as the start of a range.
    for (unsigned sampleRate : {22050, 44100, 48000, 96000}) {
        assert(response(sampleRate, 0, 1000, 10000) < -39);
        assert(response(sampleRate, 0, 3000, 10000) < -20);
        const float edge = response(sampleRate, 0, 10000, 10000);
        assert(edge > -3.05f && edge < -2.97f);
    }
    assert(response(44100, 0, 18000, 10000) > -0.1f);
    // A range crossing Nyquist must remove its playable part, not bypass.
    assert(response(16000, 1000, 3000, 10000) < -20);
    assert(response(16000, 1000, 100, 10000) > -0.1f);
    AudioNotch range;
    for (unsigned sampleRate : {8000, 16000, 20000}) {
        range.configure(radioNotchConfigurationValue(true, 0, 10000), sampleRate);
        assert(range.active());
        for (float input : {-1.0f, 0.0f, 0.25f, 1.0f})
            for (unsigned channel : {0, 1}) assert(range.process(input, channel) == 0);
    }
    for (auto bounds : {std::pair<int,int>{5000, 5000}, {5100, 5000}, {99, 5100}, {5000, 10001}, {0, 0}, {0, 99}}) {
        range.configure(radioNotchConfigurationValue(true, bounds.first, bounds.second), 44100);
        assert(!range.active());
        assert(range.process(0.123456f, 0) == 0.123456f);
    }
    for (unsigned sampleRate : {8000, 10000}) {
        range.configure(radioNotchConfigurationValue(true, 5000, 5100), sampleRate);
        assert(!range.active()); // No overlap when the LOWER edge reaches Nyquist.
    }
    for (auto bounds : {std::pair<int,int>{100, 101}, {5000, 5001}, {100, 10000}, {0, 100}, {0, 10000}}) {
        for (unsigned sampleRate : {22050, 96000}) {
            range.configure(radioNotchConfigurationValue(true, bounds.first, bounds.second), sampleRate);
            assert(range.active());
            for (int i = 0; i < 100000; ++i) {
                const float output = range.process(i == 0 ? 1.0f : 0.0f, 0);
                assert(std::isfinite(output) && std::fabs(output) <= 1.01f);
            }
        }
    }
    range.configure(radioNotchConfigurationValue(true, 5000, 5100), 44100);
    range.process(1.0f, 0);
    range.configure(radioNotchConfigurationValue(true, 5000, 5200), 44100);
    assert(range.process(0, 0) == 0); // Upper endpoint alone invalidates history.
    range.configure(radioNotchConfigurationValue(false, 5000, 5100), 44100);
    assert(!range.active() && range.process(0.123456f, 0) == 0.123456f);
    AudioNotch notch;
    for (auto config : {uint32_t(3000), kNotchEnabledBit | 3000U,
                        kNotchEnabledBit | 10000U, kNotchEnabledBit | 99U}) {
        notch.configure(config, 8000);
        if (config != (kNotchEnabledBit | 3000U)) {
            assert(!notch.active());
            for (float sample : {-1.0f, 0.0f, 0.123456f, 1.0f}) assert(notch.process(sample, 0) == sample);
        }
    }
    notch.configure(kNotchEnabledBit | 3000U, 44100);
    for (int i=0; i<100; ++i) notch.process(1, 0);
    notch.configure(kNotchEnabledBit | 100U, 48000);
    assert(notch.process(0, 0) == 0); // Clear history on settings/rate changes.
    notch.configure(kNotchEnabledBit | 3000U, 0);
    assert(!notch.active());

    std::vector<int32_t> pcm(1000);
    for (size_t i=0; i<pcm.size(); ++i) pcm[i] = i & 1 ? INT32_MIN : INT32_MAX;
    auto original = pcm;
    radioNotchConfiguration.store(3000);
    audio_process_raw_samples(pcm.data(), pcm.size());
    assert(pcm == original); // Actual production hook is bit-exact when disabled.
    radioNotchConfiguration.store(kNotchEnabledBit | 3000);
    for (unsigned hz : {8000, 16000, 44100, 48000}) {
        rate = hz;
        for (int block=0; block<100; ++block) {
            pcm=original;
            audio_process_raw_samples(pcm.data(), pcm.size()); // Sanitized full-scale conversion.
        }
    }
    rate=44100;
    radioNotchConfiguration.store(radioNotchConfigurationValue(true, 5000, 5100));
    for (unsigned block=0; block<100; ++block) {
        for (unsigned i=0;i<pcm.size();i+=2) {
            pcm[i]=int32_t(100000000 * std::sin(2*pi*5050*(block*500+i/2)/rate));
            pcm[i+1]=0;
        }
        audio_process_raw_samples(pcm.data(),pcm.size());
        for (unsigned i=1;i<pcm.size();i+=2) assert(pcm[i]==0);
    }
    for (unsigned sampleRate : {16000, 44100}) {
        rate = sampleRate;
        for (unsigned startHz : {0, 1000}) {
            radioNotchConfiguration.store(radioNotchConfigurationValue(true, startHz, 10000));
            double before = 0, after = 0;
            const unsigned signal = startHz ? 3000 : 1000;
            for (unsigned block = 0; block < 100; ++block) {
                for (unsigned i = 0; i < pcm.size(); i += 2) {
                    pcm[i] = int32_t(100000000 * std::sin(2*pi*signal*(block*500+i/2)/rate));
                    pcm[i+1] = 0;
                    if (block > 50) before += double(pcm[i]) * pcm[i];
                }
                audio_process_raw_samples(pcm.data(), pcm.size());
                for (unsigned i = 0; i < pcm.size(); i += 2) {
                    assert(pcm[i+1] == 0);
                    if (block > 50) after += double(pcm[i]) * pcm[i];
                    if (!startHz && sampleRate == 16000) assert(pcm[i] == 0);
                }
            }
            assert(after / before < 0.01); // At least 20 dB in the actual local PCM hook.
        }
    }
    radioNotchConfiguration.store(radioNotchConfigurationValue(false, 0, 10000));
    pcm = original;
    audio_process_raw_samples(pcm.data(), pcm.size());
    assert(pcm == original); // Disabling a whole-signal stop restores exact bypass.
    audio_process_raw_samples(nullptr,0);
    std::puts("Production PCM hook, bypass, attenuation, stereo and full-scale checks passed.");
}
