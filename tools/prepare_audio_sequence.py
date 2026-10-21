#!/usr/bin/env python3
"""Export the recording profile using the firmware's integer PCM renderer."""
import array
import csv
import json
import math
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import wave
from firmware_artifacts import digest

ROOT = Path(__file__).resolve().parents[1]
RATE = 44100
PERIOD = 882  # exact common period of 250 Hz and 1 kHz at 44.1 kHz


def prepare_sequence(assets, speaker=None):
    if speaker not in (None, 'left', 'right'):
        raise ValueError('Recording speaker must be left or right')
    channel = int(speaker == 'right')
    boost_db = 6 if speaker else 0
    # Quantize once on the host; firmware and exporter embed these same tables.
    tones = [[math.sin(2 * math.pi * hz * i / RATE) for i in range(PERIOD)]
             for hz in (250, 1000)]
    tones.append([a + b for a, b in zip(*tones)])
    peaks = [max(map(abs, tone)) for tone in tones]
    tables = [[round(x / peak * 32767) for x in tone] for tone, peak in zip(tones, peaks)]
    assert all(max(map(abs, table)) == 32767 for table in tables)
    gains = {name: [round(10 ** ((db + boost_db) / 20) * 2**30) for db in levels]
             for name, levels in [('Tone', (-36, -30)), ('Music', (-18, -12))]}
    header = '#pragma once\n#include <cstdint>\n'
    header += f'inline constexpr uint32_t kSequencePeriod = {PERIOD};\n'
    header += f'inline constexpr int kSequenceChannel = {channel};\n'
    header += 'inline constexpr int16_t kSequenceTones[3][kSequencePeriod] = {\n'
    header += ',\n'.join('{' + ','.join(map(str, table)) + '}' for table in tables) + '\n};\n'
    for name, values in gains.items():
        header += f'inline constexpr uint32_t kSequence{name}Gain[2] = {{{values[0]}, {values[1]}}};\n'
    (assets / 'sequence_tones.h').write_text(header)
    with tempfile.TemporaryDirectory(prefix='radiohead-sequence-') as temporary:
        binary = Path(temporary) / 'export'
        raw = Path(temporary) / 'sequence.pcm'
        subprocess.run(['c++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=undefined', '-fno-sanitize-recover=all',
                        '-I' + str(ROOT / 'experiments/audio-only/main'), '-I' + str(assets),
                        str(ROOT / 'tools/native_audio_only/export_sequence.cpp'),
                        '-o', str(binary)], check=True)
        subprocess.run([str(binary), str(assets / 'music.pcm'), str(raw)], check=True)
        data = raw.read_bytes()
    assert len(data) == 73 * RATE * 4
    pcm = array.array('h', data)
    if sys.byteorder != 'little':
        pcm.byteswap()
    assert not any(pcm[1-channel::2])
    wav = assets / 'sequence-reference.wav'
    with wave.open(str(wav), 'wb') as stream:
        stream.setnchannels(2)
        stream.setsampwidth(2)
        stream.setframerate(RATE)
        stream.writeframes(data)

    # Read timings/names from the actual firmware table, with no second schedule.
    source = (ROOT / 'experiments/audio-only/main/audio_sequence.h').read_text()
    segments = re.findall(r'\{"([^"]+)",(\d+),(\d+),(\d+),(false|true)\}', source)
    assert len(segments) == 8
    rows = []
    end = 0
    for index, (label, start, duration, tone, higher) in enumerate(segments):
        start, duration, tone = int(start), int(duration), int(tone)
        if start > end:
            if index == 0:
                rows.append((0, 5, 'opening silence', None, None))
                end = 5
            assert start - end == 2
            rows.append((end, start, 'gap silence', None, None))
        samples = pcm[start*RATE*2+channel:(start+duration)*RATE*2:2]
        peak = max(map(abs, samples))
        gain = gains['Music' if tone == 3 else 'Tone'][higher == 'true']
        # Independently check all steady samples against the table/source gain,
        # including the music wrap at 8.613 s and symmetric integer rounding.
        fixture = array.array('h', (assets / 'music.pcm').read_bytes())
        if sys.byteorder != 'little':
            fixture.byteswap()
        for i, actual in enumerate(samples):
            source_sample = fixture[(i % (len(fixture)//2))*2] if tone == 3 else tables[tone][i % PERIOD]
            fade = min(1323, i, len(samples)-1-i)
            product = source_sample * gain * fade
            denominator = 2**30 * 1323
            expected = (abs(product) + denominator//2)//denominator
            assert actual == (-expected if product < 0 else expected)
        assert samples[0] == samples[-1] == 0
        rows.append((start, start+duration, label, peak, 20*math.log10(peak/32767)))
        end = start + duration
    rows.append((end, 73, 'final silence (continues indefinitely)', None, None))
    for start, end, _, peak, _ in rows:
        if peak is None:
            assert not any(pcm[start*RATE*2:end*RATE*2])
    assert [row[3] for row in rows if row[3] is not None][:3] == [1036 if speaker else 519]*3
    assert [row[3] for row in rows if row[3] is not None][4:7] == [2067 if speaker else 1036]*3
    timestamps = assets / 'sequence-timestamps.csv'
    with timestamps.open('w', newline='') as stream:
        writer = csv.writer(stream)
        active = 'right' if channel else 'left'
        writer.writerow(['start_seconds', 'end_seconds', 'segment', active+'_peak_counts', active+'_peak_dbfs'])
        writer.writerows(rows)
    metadata = dict(rate=RATE, channels=2, sample_bits=16, active_channel=active,
                    additional_gain_db=boost_db,
                    reference_frames=len(data)//4, sequence_end_seconds=71, reference_seconds=73,
                    fade_ms=30, timing_origin='first I2S frame; boot time excluded',
                    tone_nominal_peak_dbfs=[-36+boost_db, -30+boost_db],
                    music_fixture_trim_db=[-18+boost_db, -12+boost_db],
                    gains_q30=gains, two_tone_sum_peak_before_normalization=peaks[2],
                    fixture_pcm_sha256=digest(assets / 'music.pcm'),
                    sha256={path.name: digest(path) for path in (wav, timestamps, assets / 'sequence_tones.h')})
    (assets / 'sequence.json').write_text(json.dumps(metadata, indent=2) + '\n')
    print('PASS: complete 73-second render, preload alignment, segment/fade/loop samples, '
          f'equal single/combined peaks, silent {"left" if channel else "right"} channel and indefinite zero tail')
    return metadata
