#!/usr/bin/env python3
"""Summarize a private S1 serial capture without printing account or track data."""

import argparse
import json
import re
from collections import Counter, defaultdict
from pathlib import Path


ANSI = re.compile(r"\x1b\[[0-9;]*m")
PAIR = re.compile(r"(int|dma|psram)=(\d+),(\d+),(\d+)")
STACK = re.compile(r"\bstack=(\d+)")
TOKEN = re.compile(r"Access token fetched successfully count=(\d+)")
ALLOCATION_FAILURE = re.compile(r"\balloc_fail=(\d+)")
PCM = re.compile(r"\bpcm_bytes=(\d+) partial_writes=(\d+) write_failures=(\d+)")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    counts = Counter()
    states = defaultdict(list)
    stacks = defaultdict(list)
    token_counts = []
    allocation_failure_counts = []
    pcm_counters = []
    timestamps = []

    for raw in args.log.read_text(errors="replace").splitlines():
        line = ANSI.sub("", raw)
        try:
            timestamps.append(float(line.split(" ", 1)[0]))
        except ValueError:
            pass
        for phrase, name in (
            ("rst:", "boots"),
            ("Rebooting...", "reboots"),
            ("stack overflow", "stack_overflows"),
            ("abort()", "aborts"),
            ("Got credentials, starting player", "paired_sessions"),
            ("Selected audio format=", "selected_files"),
            ("Audio fallback format=", "fallback_files"),
            ("Got track ID=", "tracks_started"),
            ("Opening CDN audio stream", "cdn_opens"),
            ("PCM producer cancelled or queue full", "pcm_cancellations"),
            ("I2S write incomplete", "i2s_short_writes"),
            ("Playing done", "tracks_finished"),
            ("task_wdt: Task watchdog got triggered", "watchdog_events"),
            ("esp-aes: Failed to allocate memory", "aes_allocation_errors"),
        ):
            if phrase in line:
                counts[name] += 1
        match = TOKEN.search(line)
        if match:
            token_counts.append(int(match.group(1)))
        match = ALLOCATION_FAILURE.search(line)
        if match:
            allocation_failure_counts.append(int(match.group(1)))
        match = PCM.search(line)
        if match:
            pcm_counters.append(tuple(map(int, match.groups())))
        if "val-s1: state=" in line:
            state = line.split("val-s1: state=", 1)[1].split()[0]
            pairs = {name: tuple(map(int, values)) for name, *values in PAIR.findall(line)}
            if pairs:
                states[state].append(pairs)
            match = STACK.search(line)
            if match:
                stacks[state].append(int(match.group(1)))

    result = {
        "capture_span_seconds": round(timestamps[-1] - timestamps[0], 1) if timestamps else 0,
        "events": dict(counts),
        "highest_token_fetch_count_within_boot": max(token_counts, default=0),
        "highest_allocation_failure_count_within_boot": max(allocation_failure_counts, default=0),
        "highest_audio_counters_within_boot": {
            name: max((sample[index] for sample in pcm_counters), default=0)
            for index, name in enumerate(("pcm_bytes", "partial_writes", "write_failures"))
        },
        "samples_by_state": {state: len(samples) for state, samples in states.items()},
        "least_stack_reserve_bytes_by_state": {state: min(values) for state, values in stacks.items()},
        "least_heap_bytes_by_state": {
            state: {
                name: {
                    "free": min(sample[name][0] for sample in samples),
                    "minimum_since_boot": min(sample[name][1] for sample in samples),
                    "largest_block": min(sample[name][2] for sample in samples),
                }
                for name in ("int", "dma", "psram")
            }
            for state, samples in states.items()
        },
    }
    print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
