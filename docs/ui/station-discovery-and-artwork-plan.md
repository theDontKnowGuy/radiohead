# Station discovery and persistent artwork

Date: 2026-09-21  
Status: implementation plan; firmware, browser flow and hardware acceptance pending.

## Decision and feasibility

Implement station search in JavaScript served by the ESP32. Search a public radio
directory, suggest a matching station, and populate editable name, stream URL and
logo fields. The browser prepares small RGB565 images; the ESP32 stores and draws
them. No project-operated backend, paid API, runtime AI or compile-time station
artwork is required.

“No server” means no additional server we deploy: online discovery still depends
on a public directory and image hosts. Already saved stations and artwork must not
depend on that directory remaining available. Audio playback continues to require
the station's stream service.

This is practical on the configured ESP32-S3 with 16 MB flash and 8 MB PSRAM.
Finding a correct station is less certain than displaying its logo: arbitrary
phrases, abbreviations and publisher names do not necessarily identify a public
live stream. Never invent a stream URL or treat a publisher's website as one.

Follow the [visual contract](visual-contract.md),
[implementation guide](agent-implementation-guide.md) and
[implementation status](implementation-status.md). Preserve the sunset/blue,
artwork-led UI. This document does not close any existing UI acceptance package.

## Current repository baseline

- `platformio.ini`: ESP32-S3, 16 MB flash, 8 MB OPI PSRAM, LovyanGFX 1.2.21.
- The installed `default_16MB.csv` reserves two 6.25 MiB OTA slots, a 3.375 MiB
  filesystem partition labelled `spiffs`, and 20 KiB NVS. These are partition
  capacities, not measurements of free space or runtime memory.
- `include/app_state.h`: ten mutable station slots; `RadioStation` has name and URL.
- `src/settings.cpp`: existing station names/URLs use `n<slot>` and `u<slot>` keys
  in the `radio` Preferences namespace. Preserve those keys.
- `src/web_server.cpp`: `/stations`, `/edit`, manual selection and M3U import
  already provide the editing path. Preserve their existing behavior.
- `src/display.cpp`: `drawArtwork()` currently draws initials. Live list, player
  and information screens use 32, 88 and 64 px artwork respectively.
- No artwork filesystem lifecycle or upload route was found in the inspected
  application paths. Filesystem mounting and storage verification are new work.

Ten slots remain the initial limit. More slots require a separate catalog and
favorite-bitmask change; adequate image storage alone does not remove that limit.

## Discovery source and evidence

Use Radio Browser as the first provider. Its API supplies station identity,
name, source/resolved stream URLs, logo URL (`favicon`), and stream metadata.
Search can be bounded and can exclude entries marked broken. A directory health
flag does not establish playback on this device.
[API reference](https://docs.radio-browser.info/)

Use HTTPS and mirror discovery/failover rather than depending permanently on one
host. Browsers can obtain mirrors through `/json/servers`. Retain `stationuuid`
as provider identity. Follow the provider's client identification and click-reporting
guidance when integrating actual user playback; never report searches as playback.
[Client guidance](https://api.radio-browser.info/)

Read-only probes on 2026-09-21 used
`https://de1.api.radio-browser.info/json/stations/search`, with `name`, `limit=5`
and `hidebroken=true`, plus an HTTP local-origin header:

| Query | Observed response |
| --- | --- |
| `nyt` | Two unrelated entries containing “Anytime”; no intended publisher match. |
| `The New York Times` | Empty result. |
| `New York Times` | Empty result. |
| `BBC Radio 4` | Five entries with resolved stream URLs; several included logo URLs, including SVG. Codec metadata was `UNKNOWN`. |

All four responses were HTTP 200 with `Access-Control-Allow-Origin: *`. Requests
used curl with normal certificate validation after Python's local certificate
store failed validation. This establishes live API responses and CORS headers,
not an end-to-end browser or device test. No stream playback or logo download was
verified. Results can change; empty results with `hidebroken=true` do not prove a
service does not exist elsewhere or among entries marked broken.

The specific promise “nyt always finds The New York Times live radio” is therefore
unsupported. Alias expansion can understand a name without finding a live stream.
If no suitable station exists in the directory, show that result and permit a
manual stream URL. Publisher podcasts belong in Recorded Shows, not in live radio
slots with an invented continuous stream.

## User flow

1. In the station editor, enter a phrase and press **Search**. Optional country
   and language filters help distinguish similarly named stations.
2. Show a short result list with name, country/language, stream format and logo
   preview where available. Label directory results as suggestions.
3. For a clear, strong match, preselect it and populate an editable draft: name,
   stream URL, logo URL and preview. Show alternatives below. Ambiguous or weak
   matches require choosing a result before filling the draft.
4. Allow independent edits to every field and local logo file selection. A later
   search response must never overwrite fields the user has already edited.
5. Offer **Test on radio** as an explicit action that changes playback. Searching
   or selecting a suggestion must not interrupt current audio. Browser playback,
   if later added, is not proof of ESP32 compatibility.
6. **Save** persists the station and prepared artwork. Show separate station and
   artwork outcomes if either step fails. Missing artwork may use the existing
   placeholder; do not report a failed image upload as successful.

Keep manual entry available during directory outages and AP-only setup. If the
phone has no internet while connected to the radio's AP, explain why discovery
is unavailable; local image upload and manual configuration still work.

## Search and matching policy

Implement matching and selection in browser code:

- Submit explicit searches initially; cap phrases at 80 characters, results at 20
  per request, and the visible alternatives at five. Cancel stale requests.
- Query with URL-encoded parameters, a finite timeout and bounded mirror retries.
  Distinguish network failure from a successful search with no matches.
- Normalize Unicode, case and whitespace while preserving meaningful letters and
  numbers. Rank exact names, explicit aliases and whole-token matches above
  substring matches. Popularity may break ties, but never establishes identity.
- Use a small, explicit alias map as an optional aid. `nyt` may expand to
  `New York Times`; an unrelated `Anytime` substring is not an acceptable default.
  Show the expanded search term. Arbitrary semantic abbreviation expansion is
  outside the initial guarantee.
- Preselect only an unambiguous top identity match. Multiple regional stations,
  similarly named brands or uncertain matches produce alternatives. Treat stream
  variants separately and prefer variants verified on this firmware; unknown
  codec metadata is “untested”, not necessarily unsupported.
- Do not discard distinct regional entries merely because their names match.
  Deduplicate exact provider UUIDs and identical URLs within the same identity.
- Prefer a nonempty resolved URL as the editable suggestion, while retaining the
  source URL and provider identity. Resolved URLs can expire. Refresh only on
  explicit request, and never silently replace a user-edited URL.

The browser searches a structured directory, not arbitrary Google results or
station webpages. General web scraping is not a reliable extension of this plan.
An image search by itself also cannot establish that a logo belongs to a stream.

The TypeSafe skill separates semantic identity judgment from exact operations.
A future narrow Jev Choice could rank retrieved candidates with a no-match option,
but it would require a trusted credential-bearing service and representative
evaluation. That conflicts with this deployment's initial architecture. Use
deterministic matching plus user choice here; no live Jev call was made.

## Image acquisition and conversion

The directory supplies a logo URL, not necessarily a usable or correct logo.
Keep a visible preview and user override.

1. Try browser download with CORS permission, decode as an image, and draw into
   canvas. Local file selection is the dependable fallback.
2. If the host denies CORS, optionally ask the ESP32 to fetch the original and
   expose it as an inert same-origin download. JavaScript then performs conversion.
3. Fit the logo with padding by default; crop only when the user requests it.
   Flatten transparency against a user-visible tile background. Draw rounded
   corners against the current screen background in the native renderer, so the
   asset does not bake in the wrong selected/unselected background.
4. Generate 32×32, 64×64 and 88×88 RGB565 versions for the existing station views.
   Supporting shows at their separate 102 px size is later work.
5. Upload binary bytes, not Base64 or C++ arrays. Define byte order explicitly and
   verify red/green/blue/gray patches through the installed LovyanGFX draw path.

A remote image can be displayed in an HTML image element yet still be forbidden
for canvas pixel extraction. API CORS permission does not extend to the independent
logo host, and `no-cors` fetch does not fix this.
[Canvas security](https://developer.mozilla.org/en-US/docs/Web/HTML/How_to/CORS_enabled_image)

Support PNG/JPEG/WebP as browser inputs and self-contained SVG through an image
decoder, never by inserting remote SVG markup into the document. Do not execute
image content or load scripts from provider data. Reject unsupported, corrupt,
oversized or externally dependent images with a clear local-upload fallback.
SVG rasterization and pixel export need explicit browser acceptance tests.

Proposed initial limits: 512 KiB original download, 2048×2048 decoded dimensions,
one conversion at a time and 32 KiB final artwork package. Enforce browser input
limits where possible before decoding; enforce all final-package limits again on
the device. A browser decoder may allocate before dimensions become available,
which is another reason to bound input and keep originals off the ESP32 decode path.

The optional relay is a bounded image fetcher, not an open proxy: HTTP(S) only,
public destinations only, no credentials, finite timeouts, at most three redirects,
destination revalidation on every hop, and a byte-count limit even without
Content-Length. Block loopback, local/private and link-local destinations including
resolved addresses. Fetch with TLS validation; reject certificate failures.
Serve originals as downloads with `nosniff`, consume them as blobs, and never serve
remote HTML/SVG as an executable same-origin page.

## Persistent format and lifecycle

Use LittleFS in the existing filesystem partition after verifying its current
contents and ownership. The installed Arduino LittleFS interface supports the
`spiffs` partition label. Do not change partition boundaries or auto-format on an
ordinary mount failure. A confirmed unused partition can be initialized during
the implementation's provisioning step; preserve existing data if present.

Propose a versioned artwork package with magic, fixed variant descriptors,
dimensions, pixel format, explicit byte order, payload lengths and checksum.
Accept only the defined size set and exact expected lengths. A checksum detects
corruption; it is not authentication. Reject client-provided filenames/paths.

| RGB565 variant | Pixel bytes |
| --- | ---: |
| 32×32 | 2,048 |
| 64×64 | 8,192 |
| 88×88 | 15,488 |
| All three, one station | 25,728 (25.125 KiB) |
| All three, ten stations | 257,280 (251.25 KiB) |

Headers, metadata, temporary files and filesystem overhead are additional. Keep
headroom for a new package and the previous valid one during replacement. A
32 KiB upload ceiling accommodates the defined package. Do not retain originals
on the device by default.

Store compact optional metadata separately from the established station keys:
provider/UUID, source logo URL, artwork revision and association with the exact
station URL. Add bounded fields only after checking available NVS capacity; larger
metadata belongs beside artwork in the filesystem.

Prepare and validate a new revision before switching the active association.
Keep the previous version until the new reference is durable. If station settings
and artwork metadata are interrupted between writes, the URL association must
reject stale artwork instead of showing another station's logo. Clean orphaned
temporary revisions on a bounded maintenance pass, not during rendering.

Renaming a station preserves its artwork. Replacing its stream clears the previous
association unless the same save explicitly associates the chosen image. Apply
this rule to manual edits and every M3U/import path. Recheck slot and station
revision at upload commit so two browser tabs cannot attach an old draft to a
new station. Removing a logo restores the placeholder. Normal firmware OTA should
preserve the filesystem; filesystem flashing/full erase is a separate operation.

## Firmware ownership and audio protection

| Owner | Planned changes |
| --- | --- |
| `web_server` | Search/editor JavaScript, bounded upload handlers, optional relay job API, browser error responses. |
| `app_state` | Bounded station artwork identity/revision and shared result state. |
| `settings` | Filesystem lifecycle, metadata persistence, migration defaults and orphan cleanup. |
| `display` | Validated image cache, RGB565 drawing, clipping, placeholders and targeted invalidation. |
| `media` | Explicit test playback using existing playback ownership and status. |
| `main` | Startup/service orchestration only. |

Cache visible 32 px thumbnails and the current larger asset; do not decode images
or download anything while drawing. Check allocation failure and use a placeholder.
Keep display ownership on the existing application path.

Do not synchronously fetch an internet image inside `server.handleClient()`.
If the relay is implemented, return a job ID, use one bounded worker and poll
status; the worker must not call the display or mutate playback state. A separate
task does not eliminate network, internal-heap or flash contention.

The synchronous upload handling and flash commits also need measured service-gap
budgets. Limit per-request work and stage uploads as needed. Never claim chunking
alone guarantees uninterrupted audio: flash erase/write can stall execution.
If device tests cannot maintain playback, queue persistence until playback is
stopped and show that state, rather than hiding dropouts.

## Delivery packages and acceptance

Implement one package at a time, keeping each buildable and preserving user changes.

| Package | Deliverable | Acceptance evidence |
| --- | --- | --- |
| 1. Browser discovery | Search adapter, mirror failover, ranking, editable drafts and manual fallback. | Browser requests from the radio page; exact, ambiguous, alias, unrelated substring, empty, outage and stale-response cases. No automatic playback or saving. |
| 2. Local artwork | Local image conversion, upload, filesystem lifecycle and native rendering at all three station sizes. | Byte/color fixtures; malformed uploads; reboot, interrupted replacement, full storage, slot reuse and concurrent-draft handling. Native 320×240 visual fixtures. |
| 3. Directory logos | CORS-capable remote logo import integrated with selected results; optional bounded relay for blocked hosts. | Same-origin export; CORS denial, SVG, invalid image, oversized body, redirect and timeout cases. Honest fallback when a logo cannot be acquired. |
| 4. Device acceptance | Test playback and interaction while browsing, uploading and saving; document operational limits. | Audio/service-gap measurements, minimum internal heap, PSRAM/largest allocation, flash usage, and a 60-minute mixed interaction soak. |

For each firmware/source change run `pio run -e esp32s3`, inspect RAM/flash output,
and run `git diff --check`. Rendering tests must use the production C++ path and
native size, including focused/unfocused rows, Hebrew names, player/info artwork,
and missing/corrupt images. Inspect on the physical TFT before visual acceptance.

Verify that saved artwork still displays when the directory and image hosts are
unreachable, that manual station entry always works, and that settings/OTA/favorite
behavior remains intact. Do not equate directory health with device playback or
an HTTP success with a valid image.

## Plan verification and remaining uncertainty

Verified for this plan: repository interfaces/layout sizes, configured partition
capacity, live directory response fields and CORS headers, and deterministic pixel
storage arithmetic. Implementation, browser integration, actual free storage,
image-host access, stream compatibility, native visuals and uninterrupted audio
remain unverified. No source/configuration changes or firmware build were required
to create this document.
