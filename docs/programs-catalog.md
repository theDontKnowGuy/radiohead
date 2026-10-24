# Editable recorded programs

Implemented 2026-10-22; ready for device testing, not flashed.

Open **Programs** in the radio's web settings (`/programs`). Filter the saved list
by name/source, edit an entry, or add one of up to ten programs. All ten default
slots initially exist, so remove an unwanted show or edit its source to make room.
Directory suggestions populate an editable RSS draft; manual RSS entry and Omny
program/playlist slugs remain available. Preview reads episode titles without
starting audio. Save does not require a successful preview. Browse episodes and
Play explicitly start playback. Favorites are shared with the TFT.

## Ownership and persistence

- `app_state` retains the original ten Omny defaults and owns the mutable model.
  `podcast_catalog.h` is the shared model and settings interface.
- `settings_programs.cpp` is a settings submodule. It validates and commits the
  complete version-1 JSON catalog and program favorite mask as one checked NVS
  blob, `radio/programs` (maximum 16 KiB). RAM changes only after the commit.
  Failed writes leave the current catalog intact. An unreadable stored catalog
  keeps defaults visible but refuses writes, rather than overwriting stored data.
- Missing catalog data uses the original shows and legacy favorite mask. The first
  program save, removal or show-favorite change makes that catalog durable. A
  saved empty catalog stays empty. Existing radio keys are retained; ordinary
  settings saves and OTA retain programs. Factory reset clears the `radio`
  namespace and therefore restores the defaults on reboot, retaining calibration.
- Identity survives renaming. Replacing a source generates a new ID; removing a
  program clears its favorite. The editor clears the favorite on source edits so
  marking a replacement as a favorite is explicit. Duplicate exact sources are
  rejected. Slots remain in deterministic order; no reorder feature is added.
- Catalog revisions include names, source identities and favorites, including TFT
  favorite changes. Stale browser writes fail with 409 and keep the draft.
- The existing station-favorite persistence path is retained; TFT show favorites
  commit the catalog directly. A program storage error does not prevent station
  favorites from saving.

## Playback, remote sources and limits

`media` still owns one background fetch job. Each job copies its source before
starting; it never reads editable catalog Strings from the worker. Generations
reject old results after edits or newer browse/preview requests. Preview uses that
same bounded worker and can replace the browsing cache; other screens can reopen
their show. A preview token distinguishes its response from another request.
The playing episode and program name are copied independently, so deleting or
editing the saved source keeps playback and transport intact. Browser Play links
include catalog identity and request token; expired links cannot play a different
row after a fetch or slot change. Existing `/programs`, `/episodes` and
`/playepisode` paths remain; old Play links must reload their episode page.

RSS uses the pinned **Espressif Expat 2.8.4** component. Its purpose is streaming
XML parsing; no AI SDK or external service backend is added. Other dependency
versions are retained. The adapter reads RSS items with an audio enclosure,
title, optional GUID/date/iTunes duration, in feed order, retaining at most eight
unique episodes. GUIDs are hashed; absent GUIDs fall back to enclosure URL hashes.
There is no offline audio download or claim of full feed history.

Bounds: 160 UTF-8 bytes per display name/title, 512 bytes per feed/audio URL,
1 MiB of processed XML, depth 24, 32 KiB aggregate Expat allocation payload,
7-second feed connect/read-idle and 20-second body deadlines. Large overall feeds
can succeed by stopping after eight episodes within those bounds. DTDs, external
entities, malformed XML, oversized enclosures, unexpected compressed/chunked
responses and unsupported formats fail without changing saved programs. RSS
fetches retain the existing Omny TLS policy (`setInsecure`); no credential-bearing
feeds are supported. HTTPClient follows up to three same-scheme redirects;
HTTP-to-HTTPS redirects require using the final HTTPS feed URL. Atom, video-only
feeds, website scraping and authenticated/subscription feeds are outside scope.

Directory search runs in the phone/computer browser with explicit submission,
up to 12 Apple results and eight usable suggestions, an eight-second timeout,
a four-second minimum interval and stale-request cancellation. No Apple account
or embedded API key is used. Search outages leave manual entry usable. This is
podcast discovery, separate from the station directory. Verified primary sources:
[Apple Search API](https://performance-partners.apple.com/search-api),
[RSS 2.0](https://www.rssboard.org/rss-specification),
[Expat API](https://libexpat.github.io/doc/api/latest/), and
[Espressif component](https://components.espressif.com/components/espressif/expat/versions/2.8.4).

## Verification and pending acceptance

- `python3 tools/check_programs.py`: production catalog/settings and extracted
  HTTP handlers/request coordinator under ASan/UBSan; migration, stable identity,
  empty catalogs, duplicate/input rejection, failed writes, corrupt storage,
  save/removal/favorite rollback, preview tokens, copied sources, stale results,
  pending replacement, cache and task/network failure behavior. The production RSS
  adapter runs against the host's Expat with XML/UTF-8 packet splits, entities,
  CDATA, enclosure/duration/GUID handling, DTD rejection and resource limits.
- Live public feed snapshots from Ilana Dayan, The Daily and BBC Global News each
  produced eight episodes through the production parser. Host fetching/parsing
  does not establish ESP32 TLS/network or audio compatibility. Live directory
  response included Ilana Dayan and `Access-Control-Allow-Origin: *`.
- `node tools/check_programs_web.cjs`: production browser script with synthetic
  radio/directory responses; filter, add/edit/remove/favorites, preview, escaping,
  stale edits, draft preservation, request races, capacity and connection failure.
- `python3 tools/check_touch_input.py`, `python3 tools/check_podcast_reader.py`
  and `node tools/check_audio_web.cjs` cover retained controller/reader/audio flows.
- `python3 tools/render_ui_fonts.py --programs-only`: production TFT rendering of
  defaults, edited mixed Hebrew names with empty slots and an empty catalog.
  Native-size PNGs in `.pio/ui_native/programs-{default,edited,empty}.png` were
  visually inspected. Appearance remains sunset/blue. The unrelated full native
  renderer suite currently stops on a Home dim-hint assertion at
  `home.cpp` (the `frame.readPixel(299, 190) != photoHintCard` check); this change
  does not mark that broader suite passing or modify the Home behavior.
- Browser review: production Programs content/script and supplied CSS/coast asset
  were inspected in Dia at desktop and a 320 px iframe. A synthetic outer shell
  and read response were used; this is content/layout evidence, not a page served
  by the ESP32 or device persistence evidence.

Normal firmware build and memory results are recorded in the web progress ledger.
Device acceptance remains pending: flash the new application, verify existing
favorites, remove/add/rename and reboot, play default Omny and independent RSS
sources, browse/preview/edit while audio is playing (serial monitor closed),
exercise favorites on TFT and web, and verify stale browser links and network
failure recovery. Confirm internal heap/task stack headroom during concurrent
Spotify, weather, preview and OTA activity. Factory reset behavior must be checked
only when intentionally resetting the test device.
