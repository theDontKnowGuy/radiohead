# Add podcasts and recorded shows

Open **Programs** (`/programs`) in the web settings. There are ten slots, initially
filled with Omny shows; edit or remove one to make room. Search for a podcast or
paste a public RSS 2.0 URL. Omny program/playlist slugs also work.

Preview lists episodes without playing; Save does not require a successful preview.
Browse on the TFT or web, then choose Play. At most eight unique episodes are shown
in feed order. No offline downloads, Atom, scraping, video-only or authenticated feeds.
Favorites are shared with the TFT. Replacing a source clears its favorite;
renaming preserves identity. Removing a source leaves an already-playing episode running.

If a browser says the catalog changed, reload before saving. An old episode link
must be refreshed; it cannot silently play a replacement row. Search outages do
not prevent manual entry. For redirects from HTTP to HTTPS, enter the final HTTPS URL.
Feeds use the existing insecure TLS policy; do not put credentials in feed URLs.

## Storage and limits

- `settings_programs.cpp` commits a version-1 JSON catalog and favorite mask to
  `radio/programs` (16 KiB maximum). RAM updates only after a successful commit.
  Corrupt storage shows defaults but refuses writes. Missing data migrates defaults
  and legacy favorites on the first edit; an intentionally empty catalog stays empty.
- OTA retains programs. Factory reset restores defaults. Exact duplicate sources
  are rejected; slots keep a stable order. Stale writes return 409 and retain the draft.
- Names/titles: 160 UTF-8 bytes; URLs: 512 bytes. RSS parser: Espressif Expat 2.8.4,
  1 MiB XML, depth 24, 32 KiB parser allocations, 7 s connect/read-idle and 20 s body deadline.
  DTDs/external entities, malformed XML, unsupported compression/chunking and oversize
  enclosures fail without changing the catalog. Up to three same-scheme redirects.
- One background fetch job copies its source. Generation/token checks reject stale
  previews and Play links; playback owns separate episode/name copies.
- Browser directory search: up to 12 Apple results/eight suggestions, 8 s timeout,
  at least 4 s between requests, stale-request cancellation; no account or API key.

## Recorded checks

2026-10-22: `tools/check_programs.py`, `tools/check_programs_web.cjs`, touch,
podcast-reader and audio checks passed. Host parsing of Ilana Dayan, The Daily
and BBC feeds returned eight episodes each. Programs native fixtures and desktop/
320 px browser content were inspected. These were host/synthetic checks, not an ESP32 test.
The full native renderer still failed the unrelated Home `(299,190)` dim-hint assertion.

Still needed on the radio: add/remove/rename/reboot, favorites on both screens,
Omny and RSS playback, stale links, network errors, and browsing during audio with
the serial monitor closed. Check heap/stack reserve with Spotify/weather/preview.
