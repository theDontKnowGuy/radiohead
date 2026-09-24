#!/usr/bin/env python3
"""Summarize private [val-sample] serial records without copying raw logs to git."""

import argparse
import json
from collections import Counter
from pathlib import Path


def parse_samples(path: Path):
    samples = []
    events = Counter()
    for line in path.read_text(errors="replace").splitlines():
        if "[val-event]" in line:
            name = line.partition("name=")[2].split()[0]
            if name:
                events[name] += 1
        if "[val-sample]" not in line:
            continue
        fields = {}
        for word in line.partition("[val-sample]")[2].split():
            key, separator, value = word.partition("=")
            if separator:
                try:
                    fields[key] = [int(part) for part in value.split(",")]
                except ValueError:
                    pass
        if "ms" in fields:
            samples.append(fields)
    return samples, events


def scalar(sample, key):
    value = sample.get(key)
    return value[0] if value else None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    samples, events = parse_samples(args.log)
    if not samples:
        raise SystemExit("No validation samples found")
    result = {
        "sample_count": len(samples),
        "sample_span_seconds": round((scalar(samples[-1], "ms") - scalar(samples[0], "ms")) / 1000, 1),
        "source_sample_counts": dict(Counter(str(scalar(s, "source")) for s in samples)),
        "state_sample_counts": dict(Counter(str(scalar(s, "state")) for s in samples)),
        "events": dict(events),
    }
    for key in ("int", "dma", "psram"):
        result[key] = {
            name: min(s[key][index] for s in samples if key in s)
            for index, name in enumerate(("least_free", "minimum_free_since_boot", "least_largest_block"))
        }
    for key in ("stack_loop", "stack_ctrl", "stack_audio", "stack_update", "input"):
        result[f"minimum_{key}"] = min(scalar(s, key) for s in samples if key in s)
    for key in ("alloc_fail", "gap_p95_upper_us", "gap_max_us", "audio_call_max_us", "web_call_max_us", "tasks"):
        result[f"maximum_{key}"] = max(scalar(s, key) for s in samples if key in s)
    print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
