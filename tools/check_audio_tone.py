"""Exercise production local PCM hooks and Spotify tone/output methods on host."""
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
out = root / '.pio/audio_tone_native'
(out / 'driver').mkdir(parents=True, exist_ok=True)
(out / 'Arduino.h').write_text('#pragma once\n#include <string>\nusing String = std::string;\n')
(out / 'AudioSink.h').write_text('''#pragma once
#include <cstdint>
#include <cstddef>
class AudioSink {
public:
 virtual ~AudioSink() = default;
 virtual void feedPCMFrames(const uint8_t*, size_t) = 0;
 virtual void volumeChanged(uint16_t) = 0;
 virtual bool setParams(uint32_t, uint8_t, uint8_t) = 0;
};
''')
(out / 'driver/i2s_std.h').write_text('''#pragma once
#include <cstddef>
using i2s_chan_handle_t = void*;
using esp_err_t = int;
constexpr int ESP_OK = 0;
int i2s_channel_write(void*, const void*, size_t, size_t*, unsigned);
''')

source = (root / 'components/radiohead_spotify/MAX98357AAudioSink.cpp').read_text()


def method(signature):
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


# Guard the compensation's dependency on the pinned vendor's exact behavior.
vendor = (root / 'lib/ESP32-audioI2S-master/src/Audio.cpp').read_text()
assert 'm_audio_items.pre_gain = powf(10.0, -total_boost_db / 20);' in vendor
assert 'fminf(fmaxf(gainHighPass, -12.0f), 12.0f)' in vendor
assert re.search(r'total_boost_db = fmax\(fmax\(fmax\(0, m_audio_items.gain_ls_db\), '
                 r'm_audio_items.gain_peq_db\), m_audio_items.gain_hs_db\)', vendor)
hook_order = ['audio_process_raw_samples(m_i2sWorkBuff.get(), readWords)',
              'IIR_filter(m_i2sWorkBuff.get(), readWords)',
              'Gain(m_i2sWorkBuff.get(), readWords)',
              'audio_process_i2s(m_i2sWorkBuff.get(), (int32_t)readWords, &continueI2S)']
assert [vendor.index(call) for call in hook_order] == sorted(vendor.index(call) for call in hook_order)

fixture = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include "audio_tone.h"
#include "MAX98357AAudioSink.h"
uint32_t rate = 44100;
int appliedBass = 0, appliedMid = 0, appliedTreble = 0, applications = 0;
uint32_t mediaLocalSampleRate() { return rate; }
void mediaApplyLocalTone(int b, int m, int t) {
    appliedBass = b; appliedMid = m; appliedTreble = t; ++applications;
}
void audio_process_raw_samples(int32_t*, int16_t);
void audio_process_i2s(int32_t*, int16_t, bool*);
std::vector<int16_t> captured;
int i2s_channel_write(void*, const void* data, size_t bytes, size_t* written, unsigned) {
    const auto* pcm = static_cast<const int16_t*>(data);
    captured.insert(captured.end(), pcm, pcm + bytes / sizeof(int16_t));
    *written = bytes;
    return ESP_OK;
}
int64_t esp_timer_get_time() { return 0; }
#define ESP_LOGE(...) ((void)0)
MAX98357AAudioSink::MAX98357AAudioSink() { tx_ = reinterpret_cast<void*>(1); }
MAX98357AAudioSink::~MAX98357AAudioSink() = default;
bool MAX98357AAudioSink::setParams(uint32_t, uint8_t, uint8_t) { return true; }
'''
checks = r'''
void publish(int b, int m, int t) {
    radioToneRequested.store(radioToneConfiguration(b, m, t));
    int32_t silence[2] = {};
    audio_process_raw_samples(silence, 2);
}
double rms(const std::vector<int16_t>& samples) {
    double sum = 0;
    for (size_t i = samples.size() / 2; i < samples.size(); i += 2)
        sum += double(samples[i]) * samples[i];
    return std::sqrt(sum / (samples.size() / 4));
}
std::vector<int16_t> spotify(int treble, float frequency, float amplitude, uint8_t gain) {
    MAX98357AAudioSink sink;
    sink.setTone(0, 0, treble);
    sink.volumeChanged(gain);
    std::vector<int16_t> samples(44100 * 2);
    for (size_t i = 0; i < samples.size(); i += 2) {
        samples[i] = radioPcm16(amplitude * std::sin(6.28318530718 * frequency * (i / 2) / 44100));
        samples[i + 1] = samples[i] / 2;
    }
    captured.clear();
    sink.feedPCMFrames(reinterpret_cast<const uint8_t*>(samples.data()), samples.size() * 2);
    assert(captured.size() == samples.size());
    if (!treble) {
        for (size_t i = 0; i < samples.size(); ++i)
            assert(captured[i] == int32_t(samples[i]) * gain / 255);
    }
    return captured;
}
int main() {
    for (uint32_t sampleRate : {16000U, 22050U, 44100U, 48000U}) {
        rate = sampleRate;
        for (int level : {1, 6, 12, 15}) {
            publish(0, 0, level);
            assert(appliedTreble == level);
            // Model a bass component unaffected by treble EQ, after the pinned
            // library's global pre-gain and its volume stage. Old output drops
            // 12 dB at maximum treble; corrected output stays at the chosen level.
            const float makeup = radioToneMakeup(0, 0, level);
            int32_t samples[] = {radioPcm32(0.125f / makeup), radioPcm32(-0.0625f / makeup)};
            bool keep = true;
            audio_process_i2s(samples, 2, &keep);
            assert(std::abs(samples[0] / 2147483648.0 - 0.125) < 0.000001);
            assert(std::abs(samples[1] / 2147483648.0 + 0.0625) < 0.000001);
            assert(keep);
        }
    }
    // All bands share the same restoration rule; clamp to actual library bounds.
    assert(radioToneMakeup(15, 0, 0) == radioToneMakeup(0, 15, 0));
    assert(radioToneMakeup(0, 15, 0) == radioToneMakeup(0, 0, 12));
    publish(-99, 99, -99);
    assert(appliedBass == -15 && appliedMid == 15 && appliedTreble == -15);
    publish(0, 0, 15);
    // Requested updates must wait for a raw-block boundary: this block still
    // uses +15 restoration even if the loop publishes a flat curve midway.
    radioToneRequested.store(radioToneConfiguration(0, 0, 0));
    int32_t block[] = {radioPcm32(0.1f), radioPcm32(-0.05f)};
    audio_process_i2s(block, 2, nullptr);
    assert(block[0] > radioPcm32(0.39f));
    publish(0, 0, 0);
    for (int level : {0, -15}) {
        publish(level, level, level);
        int32_t exact[] = {INT32_MIN, INT32_MAX, 0, -1};
        audio_process_i2s(exact, 4, nullptr);
        assert(exact[0] == INT32_MIN && exact[1] == INT32_MAX && exact[2] == 0 && exact[3] == -1);
    }
    publish(15, 15, 15);
    int32_t peak[] = {INT32_MAX, INT32_MAX / 2};
    audio_process_i2s(peak, 2, nullptr);
    assert(peak[0] > 0 && peak[0] <= INT32_MAX);
    assert(std::abs(double(peak[1]) / peak[0] - 0.5) < 0.000001);
    int32_t silent[] = {0, 0};
    audio_process_i2s(silent, 2, nullptr);
    assert(silent[0] == 0 && silent[1] == 0);
    audio_process_i2s(nullptr, 2, nullptr);
    audio_process_i2s(peak, 0, nullptr);
    int32_t odd[] = {123};
    audio_process_i2s(odd, 1, nullptr);
    assert(odd[0] == 123);
    // Release converges across sample rates without frame/block dependence.
    for (uint32_t sr : {8000U, 44100U, 48000U}) {
        AudioToneLimiter limiter;
        limiter.configure(sr);
        float l = 4, r = -2;
        limiter.process(l, r);
        assert(std::fabs(l - 1) < 0.000001 && std::fabs(r + 0.5) < 0.000001);
        for (uint32_t i = 0; i < sr; ++i) { l = 0.1f; r = -0.05f; limiter.process(l, r); }
        assert(l > 0.0999f && l <= 0.1f);
        assert(std::fabs(r / l + 0.5f) < 0.000001);
    }
    const double bassFlat = rms(spotify(0, 40, 0.1f, 64));
    const double bassBoost = rms(spotify(15, 40, 0.1f, 64));
    const double trebleFlat = rms(spotify(0, 14000, 0.1f, 64));
    const double trebleBoost = rms(spotify(15, 14000, 0.1f, 64));
    assert(bassBoost / bassFlat > 0.98 && bassBoost / bassFlat < 1.05);
    assert(trebleBoost / trebleFlat > 2.5);
    const auto loud = spotify(15, 14000, 0.99f, 255);
    for (size_t i = 0; i < loud.size(); i += 2)
        assert(std::abs(int(loud[i]) - 2 * int(loud[i + 1])) < 30);
    const auto muted = spotify(15, 14000, 0.99f, 0);
    for (int16_t sample : muted) assert(sample == 0);
    MAX98357AAudioSink invalid;
    captured.clear();
    const uint8_t bytes[3] = {};
    invalid.feedPCMFrames(bytes, 3);
    assert(captured.empty());
    printf("Tone PCM checks passed: bass ratio %.4f, treble ratio %.4f; bounds, stereo, mute, bypass, publication and limiter release.\n",
           bassBoost / bassFlat, trebleBoost / trebleFlat);
}
'''
unit = out / 'tone.cpp'
unit.write_text(fixture + '\n' + '\n'.join(method(signature) for signature in [
    'void MAX98357AAudioSink::feedPCMFrames(',
    'void MAX98357AAudioSink::volumeChanged(',
    'void MAX98357AAudioSink::setTone(']) + checks)
exe = out / 'tone'
subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                '-fsanitize=address,undefined,float-cast-overflow',
                '-I', str(out), '-I', str(root / 'include'),
                '-I', str(root / 'components/radiohead_spotify'),
                str(unit), str(root / 'src/audio_filter.cpp'), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)

if '--firmware' in sys.argv:
    # Static archives can silently leave the vendor's weak no-op hooks active.
    # Verify the linked image uses our DSP, rather than only compiling its file.
    link_map = (root / '.pio/idf-build/radiohead.map').read_text()
    for hook in ['audio_process_raw_samples', 'audio_process_i2s']:
        assert re.search(r'\.text\._Z\d+' + hook + r'\w+\s+0x[0-9a-f]+\s+'
                         r'0x[0-9a-f]+ esp-idf/src/libsrc.a\(audio_filter.cpp.obj\)', link_map), hook
    print('Firmware link map selects both application PCM hooks.')
