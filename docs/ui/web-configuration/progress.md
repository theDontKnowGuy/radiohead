# Web configuration implementation progress

Last updated: 2026-09-21.

**Current state: design handoff and reusable CSS supplied; production integration
not started by this task.** Existing endpoints are partial building blocks, not
evidence that the new UI is implemented.

Read [the implementation handoff](README.md) before changing a package. This
ledger covers the web work only; keep native TFT results in the existing
[implementation-status.md](../implementation-status.md), and link rather than
copying or declaring unrelated TFT packages complete.

## How every implementing agent updates this ledger

1. Before work, inspect current code and working-tree diffs. Set a package to
   `in progress`, enter your task/agent identifier, affected files and date.
2. Record dependency decisions; do not overwrite another agent's uncommitted work.
3. On each handoff, enter exact implemented behavior, revision/diff reference,
   commands and results, evidence paths, remaining failures and the next action.
4. Use `not started`, `in progress`, `ready for review`, `blocked`, or `complete`.
   A blocked row must name a concrete dependency and the condition that clears it.
5. Track **visual**, **functional** and **device** evidence separately with `pass`,
   `fail`, `not verified`, or `not applicable` plus a reason. A screenshot, build,
   or browser simulation cannot establish physical-device behavior.
6. Mark a package complete only when its listed gates are satisfied. If hardware
   is unavailable, use `ready for review` and leave device verification open.

## Packages

| ID | Deliverable and principal owner | Depends on | Status | Assigned task | Evidence / next action |
| --- | --- | --- | --- | --- | --- |
| W0 | Spec, CSS, local photo and frozen design references | Current configuration/discovery plans | complete | Design handoff · 2026-09-21 | Files in this directory. Handoff checks below; not firmware acceptance. |
| W1 | Local web asset serving, shell, five-section nav, responsive layout, semantic markup | W0 | ready for review | `/root` · 2026-09-21 · `tools/prepare_ui_assets.py`, `src/web_server.cpp`, `radiohead.css` | New shell/assets compile; old configuration page renderers removed. Visual and device evidence remain open. |
| W2 | Committed/draft state, real player/volume/mute/tone, error/reconnect/conflict handling | W1; configuration C2/C3 contracts | ready for review | `/root` · 2026-09-21 · `src/web_server.cpp`, `src/settings.cpp`, `src/main.cpp`, `src/media.cpp`, `radiohead.css` | Bounded player controls and revisioned JSON contract compile; browser/audio/device verification remains open. |
| W3 | Stations CRUD/favorites, discovery/manual flows, test playback and previewed M3U | W1/W2; discovery package 1 | ready for review | `/root` · 2026-09-21 · `src/web_server.cpp`, `src/media.cpp` | Compiled contracts and browser flow; F3–F5/device checks remain open. |
| W4 | Station artwork acquisition, browser preparation and durable identity-safe storage | W3; discovery packages 2/3 | ready for review | `/root` · 2026-09-21 · `src/settings.cpp`, `src/display.cpp`, `src/web_server.cpp` | RGB565 package/identity-safe lifecycle compiles; F6/H2/H3 require device evidence. |
| W5 | Wi-Fi scan/hidden/security/password/connect/forget/recovery | W1/W2 | not started | — | Close F7 and H4; browser disconnect is not proof of success. |
| W6 | Weather/provider access, units/visibility, timezone and 12/24-hour settings | W1/W2; configuration C3 | not started | — | Close F8, H1, H5. Preserve timezone and secret semantics. |
| W7 | Custom background file/framing/preview/default/atomic commit | W1/W2; configuration C4 | not started | — | Define independent asset budget, close F9/H2/H3. |
| W8 | Device info, diagnostics, OTA, restart/reset; release verification | W1/W2; W3–W7 for final release | not started | — | Finalize reset scope, close F11–F13 and H1–H7; remove all demo behavior. |

Do not treat W8 as requiring all features before starting its independent device
pages. Its **release completion** depends on the other packages. Existing
configuration-removal and discovery packages retain their own ownership; link
their work instead of implementing duplicate persistence/state machines.

## Evidence matrix

### Visual / accessibility

| ID | Required result | Status | Evidence / notes |
| --- | --- | --- | --- |
| V1 | Navy/coast/blue shell, exact five nav sections, player, typography and spacing match reference | not verified | Must inspect real firmware-served UI. |
| V2 | All sections at 320, 390, 768, 1024 and 1440px; no clipping; long Hebrew/Latin names and URLs fit | not verified | Include 200% zoom and portrait phone. |
| V3 | Stations, discovery/editor, import preview, network, weather/time, appearance and maintenance screenshots | not verified | Store under `evidence/<date>/web-configuration/`. |
| V4 | Keyboard/focus/touch targets, labeled fields, error announcements and modal focus/Escape/return | not verified | Include file upload focus and primary-action contrast. |

### Functional browser / integration

| ID | Required result | Status | Evidence / notes |
| --- | --- | --- | --- |
| F1 | Real playback status; volume 0–21; tone −15…15; mute preserves stream/volume; no unwanted retune | not verified | Mockup ranges must not be copied. |
| F2 | Saved/draft separation; Cancel; unsaved navigation; network failure retains draft; real save acknowledgements | not verified | No optimistic success from HTTP request dispatch alone. |
| F3 | Search exact/ambiguous/no-match/outage/AP-only/stale-response cases; explicit test only changes playback | not verified | Unknown format is “untested”. |
| F4 | Add/edit/favorite/remove; exact station confirmation; capacity rechecked at ten; deletion/slot reuse identity safe | not verified | Favorite removal is not station removal. |
| F5 | File/paste/existing remote M3U paths; preview; invalid/duplicate/capacity handling; no silent replacements | not verified | HLS segments must not become station rows. |
| F6 | Logo URL and local upload; fit/crop; decode/CORS/size/storage/interruption failure; station/artwork outcomes separate | not verified | Follow discovery plan limits and RGB565 variants. |
| F7 | Scan/hidden/open/secured Wi-Fi; keep/replace/clear credentials; connect/fail/reconnect/setup recovery; forget | not verified | Never return a stored password. |
| F8 | Location/provider/key keep/replace/clear, units/visibility, full supported timezone set, 12/24-hour and DST | not verified | Save config and weather refresh have distinct outcomes. |
| F9 | Background preview/crop/fit/default/cancel; commit and failure; no station-artwork side effects | not verified | Old valid background remains on failed replacement. |
| F10 | Two-browser and TFT edits reject stale revision; upload commit rechecks slot identity; draft survives conflict | not verified | Do not silently overwrite newer state. |
| F11 | Real firmware info/diagnostics; absent metrics honest; logs redact secrets | not verified | No placeholder/sample values in release. |
| F12 | OTA invalid image/write error/disconnect/reboot/version re-read; actual TFT/browser progress | not verified | Successful upload is not successful installation. |
| F13 | Restart keeps config; reset confirmation matches actual scope and preserves calibration; retired features stay inert | not verified | No new standby/off, source editing or rename feature. |

### Device / release

| ID | Required result | Status | Evidence / notes |
| --- | --- | --- | --- |
| H1 | Real web/TFT/encoder synchronization; retained settings persist across reboot | not verified | Browser simulator cannot close this. |
| H2 | Artwork/background persist after reboot and directory outage; interruption/storage-full preserves prior valid assets | not verified | Include slot replacement and failed upload. |
| H3 | Audio continuity and measured service gaps during searching/upload/persistence; bounded memory | not verified | Record measurement method, values and pass criteria. |
| H4 | Phone joins setup AP, configures network, loses/reopens browser as expected; recovery works on failed credentials | not verified | Verify actual SSID/address shown by TFT. |
| H5 | Weather unavailable/stale and clock-unsynchronized/DST states behave honestly | not verified | Include migration of current saved timezone. |
| H6 | OTA, restart and reset on device; power/storage conflicts controlled; calibration retained | not verified | No destructive test on valuable settings without scoped test setup. |
| H7 | `pio run -e esp32s3`, RAM/flash review, `git diff --check`, secret/demo scan, mixed-use soak per discovery plan | not verified | Record revision and output summary, not just “tests pass”. |

## Decisions to record before enabling dependent behavior

| Decision | Current state | Owner / resolution |
| --- | --- | --- |
| Asset-serving paths and reproducible flash packaging | Proposed `/ui/radiohead.css` with `/ui/assets/coast.jpg`; not wired | W1 — record actual choice and measured bytes. |
| Endpoint contract and shared revision mechanism | Audit required; existing handlers use forms/redirects | W2 — list real methods, fields and responses below. |
| Production image/import budgets | Discovery artwork budgets already specified; background/import need their own limits | W4/W7 — enforce browser and device bounds. |
| Weather/timezone representation and migration | Preserve current saved timezone, verify supported regional rules | W6 — record actual mapping and DST checks. |
| Factory-reset policy | Mockup proposal only; preserve calibration by default | W8 — enumerate reset keys/assets and recovery copy. |
| Runtime power/standby behavior | Explicitly TBD in configuration plan | Outside this web handoff; not a reason to add a power menu. |

## Implemented endpoint contract

Populate from verified code when wiring each operation. Do not invent routes here
and treat them as existing. Include payload bounds, authentication/access behavior
already in the product, request method, success/error body, durable completion,
revision requirements, and any disconnect/restart consequence.

| Operation | Route / method | Request / bounds | Response / errors / revisions | Verified by |
| --- | --- | --- | --- | --- |
| Read shared player state | `GET /api/player` | No arguments | JSON: player revision, 0–21 volume, mute, −15…15 tone, playback state and current name. No secrets. | Source review + compile; device not verified. |
| Change volume | `POST /api/player/volume` | Form fields: `revision`, `volume` 0–21 | JSON state after applying. Invalid input: 400 JSON. Stale revision: 409 with current JSON state. Audio changes immediately; Preferences write is queued for 1 s of inactivity. | Source review + compile; device not verified. |
| Toggle mute | `POST /api/player/mute` | Form field: `revision` | JSON state after applying; stale revision: 409 current JSON. Preserves stream and selected volume. | Source review + compile; device not verified. |
| Change custom tone | `POST /api/player/tone` | Form fields: `revision`, `bass`, `mid`, `treble`; each −15…15 | JSON state after applying. Invalid input: 400 JSON. Stale revision: 409 with current JSON state. Tone applies immediately; Preferences write is queued for 1 s of inactivity. | Source review + compile; device not verified. |

## Handoff verification

- W0 supplies the spec, this ledger, extracted/adapted CSS, local image and frozen
  mockup references. Production files in `src/` and `include/` are unchanged by W0.
- The original browser mockup was exercised for station discovery/test/save, mute,
  unsaved navigation, Wi-Fi, clock format, background upload/framing and reset
  confirmation. Layout checks covered all sections at desktop/tablet/phone widths.
  This is **mockup-only** evidence; it does not close F or H checks above.
- CSS/reference validation details are recorded in `reference/handoff-checks.md`.

## Agent handoff log

### 2026-09-21 — W0 design handoff

- Delivered: spec, reusable CSS, photo asset, frozen reference and this ledger.
- Firmware integration: not performed. Build/flash/device testing: not applicable
  to this documentation/reference change; all production gates remain open.
- Next task: W1; first audit the current web server and asset-serving strategy.
- Preserve all unrelated working-tree changes, including display/font/media work.

### Template for the next update

```text
Date / task / package:
State and affected files:
Implemented behavior:
Dependency decisions / actual endpoint changes:
Visual result + evidence:
Functional result + exact checks/revision:
Device result + measurements (or not verified):
Build and resource results (or reason not applicable):
Remaining failures / blockers:
Next concrete action:
```

### 2026-09-21 — W1 shell, asset serving and legacy configuration-page replacement

- State and affected files: W1 ready for review. `tools/prepare_ui_assets.py` packages the supplied CSS and coast image into program flash; `src/web_server.cpp` serves them as `/ui/radiohead.css` and `/ui/assets/coast.jpg` and supplies the new shared shell. `radiohead.css` now styles navigation links as well as the mockup's original buttons.
- Implemented behavior: `/` defaults to Stations; `/network`, `/weather`, `/appearance` and `/device` provide the five-section shell and real connection/playback/settings context. The old configuration page renderers were removed. `/stations` and `/weather` now render their new sections; legacy `/skin`, `/audio` and `/update_ui` paths redirect into the new sections. Existing recorded-show browsing/playback routes remain separate.
- Dependency decisions / actual endpoint changes: W1 deliberately does not add settings mutations, station editing, Wi-Fi scans, OTA controls or browser-side state. Those existing form handlers need W2–W8 contracts rather than being copied into the new visual layer. The supplied CSS keeps its relative `./assets/coast.jpg` reference, resolved by the chosen asset routes. Asset source size is 22,116 B CSS plus 128,477 B JPEG before compiler/flash measurement.
- Visual result + evidence: not verified; browser/device screenshots remain required for V1–V4.
- Functional result + exact checks/revision: `pio run -e esp32s3` passed after dead-renderer removal; image size 3,140,363 B / 6,553,600 B (47.9% flash) and RAM 66,476 B / 327,680 B (20.3%). `git diff --check` passed after final cleanup.
- Device result + measurements: not verified.
- Remaining failures / blockers: W2 must define committed/draft revision and bounded control endpoints before the shared volume, mute and tone controls become interactive. W3–W8 own their respective section actions.
- Next concrete action: compile W1, inspect generated asset size and diff; then review the shell at desktop and phone widths before starting W2.

### 2026-09-21 — W2 shared player controls and state contract

- State and affected files: W2 ready for review. `src/web_server.cpp` adds bounded player JSON endpoints and browser controls; `src/settings.cpp` owns debounced persistence; `src/main.cpp` services it without adding work to the audio hot path; `src/media.cpp` preserves mute during volume changes. The supplied stylesheet now keeps menu SVGs white and volume values on one line.
- Implemented behavior: The shared banner has a 0–21 volume slider and mute button. The collapsed sound group has Bass/Mid/Treble sliders bounded to −15…15. Requests coalesce while sliding, serialize in the browser, reject stale values with the current state, and poll state every three seconds so encoder/TFT edits are reflected. A requested volume while muted is retained without resuming stream playback.
- Dependency decisions / actual endpoint changes: The `/api/player` routes use form-encoded POSTs because the installed Arduino `WebServer` already parses bounded form arguments. The deterministic revision is a hash of current shared player values, not a new persisted Preferences key. Existing legacy form routes remain temporarily compatible but are not used by the new UI.
- Visual result + evidence: user screenshot identified and this change corrects the volume wrap and white menu icons; final browser review at all target widths remains not verified.
- Functional result + exact checks/revision: `pio run -e esp32s3` passed after the mute-preservation correction; image size 3,150,147 B / 6,553,600 B (48.1% flash) and RAM 66,484 B / 327,680 B (20.3%). `git diff --check` passed.
- Device result + measurements: not verified. Verify mute, volume/tone response, one-second persistence, stream continuity and encoder/browser synchronization on hardware.
- Remaining failures / blockers: W2 still needs two-browser/TFT conflict and physical audio validation before it can be complete. W3 owns station actions; no station mutation was added here.
- Next concrete action: flash this checkpoint and verify player controls, then start W3 Station CRUD/discovery work.

### 2026-09-21 — W3 Stations and W4 artwork

- State and affected files: W3/W4 are ready for review. `src/web_server.cpp` now owns the bounded Station JSON contracts and browser editor; `src/media.cpp` owns explicit unsaved-stream test playback; `src/settings.cpp` owns LittleFS artwork staging/validation; `src/display.cpp` uses only the prepared 32/64/88 RGB565 variants. Public interfaces are in `include/media.h` and `include/settings.h`.
- Implemented behavior: The Stations page lists only saved stream slots, supports optimistic-concurrency add/edit/favorite/remove, rechecks the ten-slot limit at save/import, and stops only the exact removed stream. Discovery uses an explicit browser search against bounded Radio Browser mirrors, ranks exact/whole-token matches deterministically, retains a separately editable draft, and always leaves manual URL entry available. M3U file/paste input is capped at 48 KiB, previewed before an all-or-nothing additive import, deduplicated by URL and never replaces occupied slots. Test playback is explicit and non-persistent; it reports the real media state rather than treating dispatch as success.
- Artwork behavior: Browser PNG/JPEG/WebP/CORS-permitted logo inputs are capped at 512 KiB and 2048×2048, fit/crop into 32/64/88 RGB565 variants and upload a fixed 25,744-byte package. The device checks magic, exact size, URL-derived revision and payload checksum, stages to a deterministic temporary file, verifies it, then uses active/backup rename recovery. A changed stream, removal or slot reuse clears prior artwork; a name-only edit preserves it. The browser thumbnail and native renderer reject an association whose current stream URL differs. Logo failure is reported separately after a successful station save; remote CORS failure explicitly falls back to local upload.
- Dependency decisions / actual endpoint changes: New routes are `GET /api/stations`, `POST /api/stations/save`, `/favorite`, `/remove`, `/test`, `/m3u/preview`, `/m3u/commit`, `GET|POST /api/stations/artwork`. They use form fields because the installed Arduino `WebServer` provides bounded form parsing. The artwork endpoint accepts only the browser-generated package, never client paths or base64. No Jev call or runtime AI was added: identity ranking, bounds, revisions and storage validation are deterministic; live TypeSafe documentation was unavailable through the current reader.
- Visual result + evidence: not verified. The page needs screenshots at the required desktop/phone widths, plus focus, modal and CORS/error-state checks.
- Functional result + exact checks/revision: `node` syntax compilation of the embedded Station script passed; `pio run -e esp32s3` passed; final image is 3,262,623 B / 6,553,600 B (49.1% flash) and RAM 82,268 B / 327,680 B (25.1%). `git diff --check` passed.
- Device result + measurements: not verified. In particular, test-stream transitions, LittleFS mount/migration, interrupted replacement/full-storage recovery, RGB565 byte order/native rendering, reboot persistence and audio/service gaps during upload remain open.
- Remaining failures / blockers: Remote M3U URL handling remains on the retained legacy path and has not been adapted to this preview contract. Browser CORS can prevent a directory logo from being prepared; the visible local-file fallback is intentional, and no open proxy was added. F3–F6 and H2/H3 therefore remain open pending real browser and device evidence.
- Next concrete action: flash this checkpoint; exercise exact/ambiguous/no-match directory search, station conflict/delete/slot reuse, pasted/file M3U capacity and duplicates, CORS-denied/local-logo upload, reboot persistence, native 32/64/88 renders, and measure audio/service gaps during upload.
