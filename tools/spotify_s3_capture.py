#!/usr/bin/env python3
"""Bounded, read-only S3 HTTP capture. Keep the JSONL output outside git."""

import argparse
import json
import math
import time
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.parse import urlencode
from urllib.request import Request, urlopen


def request(base, path, timeout):
    started = time.monotonic()
    try:
        with urlopen(base + path, timeout=timeout) as response:
            body = response.read(4096)
            value = json.loads(body) if path.startswith("/api/spotify/") else None
            if path == "/api/player":
                player = json.loads(body)
                value = {key: player[key] for key in ("revision", "volume") if key in player}
            status = response.status
    except (HTTPError, URLError, TimeoutError, ValueError, OSError) as error:
        return {"ms": round((time.monotonic() - started) * 1000, 2),
                "error": type(error).__name__}
    return {"ms": round((time.monotonic() - started) * 1000, 2),
            "status": status, "value": value}


def probe_same_volume(base, player, timeout):
    if not isinstance(player, dict) or not {"revision", "volume"} <= player.keys():
        return {"error": "PlayerStateUnavailable"}
    body = urlencode({"revision": player["revision"], "volume": player["volume"]}).encode()
    started = time.monotonic()
    try:
        request = Request(base + "/api/player/volume", data=body,
                          headers={"Content-Type": "application/x-www-form-urlencoded"})
        with urlopen(request, timeout=timeout) as response:
            response.read(4096)
            status = response.status
    except HTTPError as error:
        status = error.code
    except (URLError, TimeoutError, OSError) as error:
        return {"ms": round((time.monotonic() - started) * 1000, 2),
                "error": type(error).__name__}
    return {"ms": round((time.monotonic() - started) * 1000, 2), "status": status}


def percentile(values, fraction):
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, math.ceil(len(ordered) * fraction) - 1)] if ordered else None


def histogram_upper(bucket_counts, fraction):
    bounds = [1000, 2000, 5000, 10000, 20000, 50000, 100000, None]
    target = math.ceil(sum(bucket_counts) * fraction)
    seen = 0
    for count, bound in zip(bucket_counts, bounds):
        seen += count
        if seen >= target:
            return bound
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-url", required=True, help="Radio URL, e.g. http://radio.local")
    parser.add_argument("--output", required=True, type=Path, help="Private JSONL capture path")
    parser.add_argument("--duration", type=int, default=600, help="Seconds; default 600")
    parser.add_argument("--interval", type=float, default=5, help="Seconds; default 5")
    parser.add_argument("--timeout", type=float, default=3, help="Seconds per request")
    parser.add_argument("--weather", action="store_true", help="Include bounded weather status GET")
    parser.add_argument("--control-probes", type=int, default=0,
                        help="Up to 30 same-volume player POST probes, one per interval")
    args = parser.parse_args()
    if args.duration < 1 or args.interval < 1 or args.timeout <= 0:
        parser.error("duration, interval, and timeout must be positive; interval >= 1")
    if not 0 <= args.control_probes <= 30:
        parser.error("control-probes must be between 0 and 30")
    if args.output.exists():
        parser.error("output already exists; choose a new capture path")
    paths = ["/api/spotify/resources", "/api/spotify/diagnostics", "/api/player"]
    if args.weather:
        paths.append("/api/weather")
    start = time.monotonic()
    next_sample = start
    samples = []
    with args.output.open("x", encoding="utf-8") as output:
        while time.monotonic() - start < args.duration:
            row = {"elapsed_s": round(time.monotonic() - start, 2)}
            for path in paths:
                row[path] = request(args.base_url.rstrip("/"), path, args.timeout)
            if len(samples) < args.control_probes:
                row["control"] = probe_same_volume(
                    args.base_url.rstrip("/"), row["/api/player"].get("value"), args.timeout)
            output.write(json.dumps(row, separators=(",", ":")) + "\n")
            output.flush()
            samples.append(row)
            next_sample += args.interval
            time.sleep(max(0, min(next_sample - time.monotonic(), args.interval)))
    summary = {"samples": len(samples), "elapsed_s": round(time.monotonic() - start, 1)}
    for path in paths:
        latencies = [row[path]["ms"] for row in samples if row[path].get("status") == 200]
        summary[path] = {"ok": len(latencies), "errors": len(samples) - len(latencies),
                         "p95_ms": percentile(latencies, .95),
                         "p99_ms": percentile(latencies, .99),
                         "max_ms": max(latencies, default=None)}
    controls = [row["control"] for row in samples if "control" in row]
    if controls:
        latencies = [item["ms"] for item in controls if item.get("status") == 200]
        summary["control"] = {"ok": len(latencies), "errors": len(controls) - len(latencies),
                              "p95_ms": percentile(latencies, .95),
                              "p99_ms": percentile(latencies, .99),
                              "max_ms": max(latencies, default=None)}
    resources = [row["/api/spotify/resources"].get("value") for row in samples]
    resources = [value for value in resources if isinstance(value, dict)]
    for name in ("internal", "dma", "psram"):
        values = [value[name] for value in resources if name in value]
        if values:
            summary[name] = {"min_free": min(item[0] for item in values),
                             "min_watermark": min(item[1] for item in values),
                             "min_largest": min(item[2] for item in values),
                             "first_free": values[0][0], "last_free": values[-1][0]}
    if resources:
        summary["max_allocation_failures"] = max(value.get("allocationFailures", 0) for value in resources)
        summary["stack_min_bytes"] = {
            name: min(value.get("stackBytes", {}).get(name, 0) for value in resources)
            for name in resources[0].get("stackBytes", {})
        }
        summary["max_loop_gap_us"] = max(value.get("loopGapMaxUs", 0) for value in resources)
        first, last = resources[0], resources[-1]
        if last.get("uptimeMs", 0) >= first.get("uptimeMs", 0):
            early, late = first.get("loopGapBuckets", []), last.get("loopGapBuckets", [])
            if len(early) == len(late) == 8:
                differences = [b - a for a, b in zip(early, late)]
                if min(differences) >= 0 and sum(differences):
                    summary["loop_gap_capture"] = {
                        "samples": sum(differences),
                        "p95_upper_us": histogram_upper(differences, .95),
                        "p99_upper_us": histogram_upper(differences, .99),
                    }
        summary["reset_observed"] = any(
            later.get("uptimeMs", 0) < earlier.get("uptimeMs", 0)
            for earlier, later in zip(resources, resources[1:])
        )
    pcm = [row["/api/spotify/diagnostics"].get("value") for row in samples]
    active = [value for value in pcm if isinstance(value, dict) and
              value.get("outputOwned") and not value.get("paused") and
              not value.get("buffering") and value.get("reportAtMs", 0)]
    summary["spotify_active_samples"] = len(active)
    if active:
        summary["spotify_pcm"] = {
            "min_nonzero_bytes_5s": min((value.get("pcmBytes5s", 0) for value in active
                                      if value.get("pcmBytes5s", 0) > 0), default=None),
            "max_empty_polls_5s": max(value.get("emptyPolls5s", 0) for value in active),
            "max_write_failures": max(value.get("writeFailures", 0) for value in active),
        }
    print(json.dumps(summary, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
