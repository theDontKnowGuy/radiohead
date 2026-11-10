# Station search and saved artwork

Current scope: ten station slots, browser directory/manual editing and explicit
Test on radio. October 11 removed M3U import and **hid logo upload/configuration**;
keep displaying saved artwork. The image pipeline below records storage/design
rules, not permission to restore those controls. [Web status](web-configuration/progress.md).

## Find a station

Search runs in browser JavaScript against [Radio Browser](https://docs.radio-browser.info/),
with HTTPS mirrors/failover, finite timeout/retries and stale-request cancellation.
Initial bounds: query 80 characters, ≤20 results, ≤5 visible alternatives.
Rank normalized exact names/explicit aliases/whole tokens before substring matches;
preselect only a clear unique identity. Country/language help; popularity alone
cannot establish identity. Keep distinct regional variants; deduplicate exact
UUID/URL within an identity. Retain `stationuuid`, source and resolved URLs;
never silently refresh a user-edited URL.

Suggestions fill an editable draft, never save or start playback. Only explicit
Test changes audio; success must come from device feedback. Unknown codec/health
flag means untested, not guaranteed playable. Manual URL entry works during
outages/AP-only setup; explain that online search needs browser internet.

September 21 probes: `nyt` returned unrelated “Anytime”; full New York Times names
returned empty; BBC Radio 4 returned stream/logo entries with UNKNOWN codec.
HTTP 200/CORS was observed, not ESP32 playback. Never invent streams or treat
publisher websites/podcasts as continuous live radio. No general scraper/runtime
AI/backend is required. Follow provider click-reporting guidance for real play,
not searches; a missing logo never blocks saving a valid stream.

## Existing artwork identity and storage

Station names/URLs retain `radio/n0…n9,u0…u9`. Row index differs from persistent slot.
Rename retains artwork/favorite; replacing URL clears the old association unless
explicitly committed for that identity. Removal returns placeholder. Recheck
slot/URL/revision at upload commit; stale browser drafts cannot attach another
station's logo. Report station-save and artwork-save outcomes separately.

LittleFS uses the existing filesystem partition. Preserve contents and boundaries;
do not auto-format ordinary mount failure as a new artwork strategy. Prepare a
validated revision before switching association; preserve last valid data on
interruption/full storage. Reject stale URL-bound artwork after split commits,
then clean orphans in bounded maintenance. Normal OTA preserves files; full erase
or filesystem flashing is separate. Images/metadata must not fill NVS.

## Image format and limits

When maintaining the hidden artwork pipeline, browser-decode PNG/JPEG/WebP or
self-contained SVG as an image, never executable HTML/SVG markup. Fit/pad by default,
crop only explicitly; flatten alpha against the shown tile color. Native renderer
owns corners/background. Remote image display does not imply CORS canvas export
permission; `no-cors` is no fix. Local file is the dependable fallback.

| RGB565 variant | Bytes |
| --- | ---: |
| 32×32 / 64×64 / 88×88 | 2048 / 8192 / 15488 |
| All variants | 25728/station; 257280/ten, plus format/temp/filesystem overhead |

Original proposal limits: original ≤512 KiB, dimensions ≤2048×2048, one conversion,
final binary package ≤32 KiB, explicit byte order. Device must independently validate
version/magic, exact variants/dimensions/lengths and checksum; never accept filenames/
paths from clients. Keep replacement headroom and no originals by default.
Check RGB565 red/green/blue/gray through the real typed draw path.

An optional image relay is not an open proxy: HTTP(S), public resolved destinations,
no credentials, validated TLS, ≤3 revalidated redirects, byte/time bounds even without
Content-Length. Block loopback/private/link-local each hop. Return inert downloads
with nosniff, never same-origin executable remote markup. One worker/job ID; no
synchronous internet fetch inside web handling or decode/fetch while painting.

## Checks that remain important

Browser: exact/ambiguous/alias/unrelated/no-match/outage/stale results, CORS denial,
SVG/bad/oversize/redirect/timeout, editable draft preservation and explicit-only play.
Storage/native: corruption, color order, all three sizes, empty/missing art, Hebrew,
slot reuse/stale saves, reboot/offline/full/interrupted replacement.
Device: service-gap/internal-heap/largest-block/PSRAM measurements during upload/
search/saving and a 60 min mixed-use soak. Chunking or a worker alone does not prove
uninterrupted audio. Queue persistence until stopped with a visible explanation if
required. Ownership: web handlers, settings storage, media playback, display cache;
keep one Audio owner and no vendor edits. This plan alone establishes no device pass.
