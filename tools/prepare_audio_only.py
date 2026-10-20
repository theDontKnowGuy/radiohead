#!/usr/bin/env python3
"""Decode the existing stereo boot music on the host; no decoder in the image."""
import argparse
import array
import json
import math
import subprocess
import tempfile
import wave
from pathlib import Path
from prepare_boot_audio import pcm_from_wav
from firmware_artifacts import digest

ROOT = Path(__file__).resolve().parents[1]


def prepare(output):
    output.mkdir(parents=True, exist_ok=True)
    source = ROOT / 'docs/audio/boot.m4a'
    with tempfile.TemporaryDirectory(prefix='radiohead-audio-only-') as temp:
        decoded = Path(temp) / 'decoded.wav'
        subprocess.run(['afconvert', str(source), str(decoded), '-f', 'WAVE', '-d', 'LEI16'], check=True)
        channels, rate, pcm = pcm_from_wav(decoded)
    if channels != 2 or rate != 44100 or not 1 <= len(pcm) // 2 <= 44100 * 12:
        raise ValueError('Expected bounded stereo 44.1 kHz source')
    # Keep the full supplied musical passage, including stereo. Remove each DC mean,
    # add 30 ms loop-edge fades, then one common gain (preserves channel balance).
    frames = len(pcm) // 2
    means = [sum(pcm[ch::2]) / frames for ch in range(2)]
    fade = rate * 30 // 1000
    shaped = [(sample - means[i % 2]) * min(1., (i // 2) / fade, (frames - 1 - i // 2) / fade)
              for i, sample in enumerate(pcm)]
    peak = max(map(abs, shaped))
    gain = (32767 * 10 ** (-12 / 20)) / peak
    normalized = array.array('h', (round(x * gain) for x in shaped))
    (output / 'music.pcm').write_bytes(normalized.tobytes())
    # A bit-exact, playable wrapper around the flash fixture for source listening.
    # No extra gain here: the hands-free image separately applies -12 dB trim.
    with wave.open(str(output / 'music-reference.wav'), 'wb') as reference:
        reference.setnchannels(2)
        reference.setsampwidth(2)
        reference.setframerate(rate)
        reference.writeframes(normalized.tobytes())
    table = [round(32767 * math.sin(2 * math.pi * i / 4096)) for i in range(4096)]
    def multi_peak(rate):
        def sine(frame, hz):
            scaled = ((frame * hz) % rate) * 4096
            index, rem = divmod(scaled, rate)
            return (table[index] + rem / rate * (table[(index+1)&4095]-table[index])) / 32767
        return max(abs(sum(sine(i,hz) for hz in (250,1000,4000))/3) for i in range(rate))
    multi_peaks = {rate:multi_peak(rate) for rate in (44100,48000)}
    (output / 'sine_table.h').write_text('#pragma once\n#include <cstdint>\ninline constexpr int16_t kSine[4096] = {\n' +
        ',\n'.join(','.join(map(str, table[i:i+32])) for i in range(0,4096,32)) + '\n};\n' +
        f'inline constexpr float kMultiPeak44100={multi_peaks[44100]:.12f}f;\n' +
        f'inline constexpr float kMultiPeak48000={multi_peaks[48000]:.12f}f;\n')
    metadata = dict(source=str(source.relative_to(ROOT)), source_sha256=digest(source),
        multitone_peak_before_normalization=multi_peaks,
        decoder='macOS afconvert -> stereo LEI16 WAVE', rate=rate, channels=2, frames=frames,
        duration_seconds=frames/rate, removed_dc_samples=means, loop_fade_ms=30,
        common_gain_after_dc_and_fade=gain, gain_db=20*math.log10(gain),
        source_clipped_samples=sum(abs(x) >= 32767 for x in pcm),
        peak_samples=max(map(abs, normalized)),
        peak_dbfs=20*math.log10(max(map(abs, normalized))/32767),
        dc_samples=[sum(normalized[ch::2])/frames for ch in range(2)],
        clipped_samples=sum(abs(x) >= 32767 for x in normalized), pcm_sha256=digest(output/'music.pcm'))
    (output/'fixture.json').write_text(json.dumps(metadata, indent=2)+'\n')
    print(json.dumps(metadata, indent=2))
    return metadata

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, default=ROOT/'.pio/audio-only-assets')
    prepare(parser.parse_args().output_dir)
