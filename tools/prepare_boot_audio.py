"""Normalize the supplied boot clip and encode the firmware's ADTS AAC asset.

Run from any directory: python3 tools/prepare_boot_audio.py
Requires macOS afconvert, which is also used for the project's boot image build.
"""

import array
import struct
import subprocess
import sys
import tempfile
import wave
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "docs/audio/boot.m4a"
OUTPUT = ROOT / "docs/audio/boot.aac"
TARGET_PEAK_DBFS = -1.5
SILENCE_THRESHOLD = 100  # 16-bit PCM amplitude; ignore decoder noise.
LEAD_IN_MS = 50


def pcm_from_wav(path: Path) -> tuple[int, int, array.array]:
    data = path.read_bytes()
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError("afconvert did not produce a WAV file")
    offset = 12
    fmt = None
    samples = None
    while offset + 8 <= len(data):
        name = data[offset:offset + 4]
        length = struct.unpack_from("<I", data, offset + 4)[0]
        start = offset + 8
        if start + length > len(data):
            raise ValueError("Invalid WAV chunk length")
        if name == b"fmt ":
            fmt = data[start:start + length]
        elif name == b"data":
            samples = data[start:start + length]
        offset = start + length + (length & 1)
    if fmt is None or samples is None or len(fmt) < 16:
        raise ValueError("WAV format or samples missing")
    codec, channels, sample_rate, _, _, bits = struct.unpack_from("<HHIIHH", fmt)
    if codec not in (1, 0xFFFE) or bits != 16 or channels != 2:
        raise ValueError("Expected stereo 16-bit PCM from afconvert")
    if sys.byteorder != "little":
        raise ValueError("This conversion expects little-endian PCM")
    pcm = array.array("h")
    pcm.frombytes(samples)
    return channels, sample_rate, pcm


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="radiohead-boot-") as directory:
        decoded = Path(directory) / "decoded.wav"
        normalized = Path(directory) / "normalized.wav"
        subprocess.run(
            ["afconvert", str(SOURCE), str(decoded), "-f", "WAVE", "-d", "LEI16"],
            check=True,
        )
        channels, sample_rate, pcm = pcm_from_wav(decoded)
        frame_count = len(pcm) // channels
        first_sound_frame = next(
            (frame for frame in range(frame_count)
             if max(abs(pcm[frame * channels + channel])
                    for channel in range(channels)) > SILENCE_THRESHOLD),
            None,
        )
        if first_sound_frame is None:
            raise ValueError("Boot sound has no audible samples")
        first_frame = max(0, first_sound_frame - sample_rate * LEAD_IN_MS // 1000)
        pcm = pcm[first_frame * channels:]
        peak = max((abs(sample) for sample in pcm), default=0)
        if peak == 0:
            raise ValueError("Boot sound is silent")
        target_peak = 32767 * 10 ** (TARGET_PEAK_DBFS / 20)
        gain = target_peak / peak
        normalized_pcm = array.array(
            "h", (max(-32768, min(32767, round(sample * gain))) for sample in pcm)
        )
        with wave.open(str(normalized), "wb") as output:
            output.setnchannels(channels)
            output.setsampwidth(2)
            output.setframerate(sample_rate)
            output.writeframes(normalized_pcm.tobytes())
        subprocess.run(
            ["afconvert", str(normalized), str(OUTPUT), "-f", "adts", "-d", "aac", "-b", "128000"],
            check=True,
        )
        print(f"Wrote {OUTPUT.relative_to(ROOT)}: {len(pcm) / channels / sample_rate:.2f} s, trimmed {first_frame / sample_rate:.2f} s, {gain:.1f}x gain")


if __name__ == "__main__":
    main()
