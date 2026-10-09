# Web configuration implementation progress

### 2026-10-09 — Connected AP MAC address (ready for device testing)

- **Scope:** Show the connected access point's BSSID in Network > Connection to
  identify the AP used by the radio. Use live connection state; clear the value
  in disconnected/setup mode. Preserve ongoing startup and boot-mode changes.
- **Implemented:** `/api/network` reports `bssid` from `WiFi.BSSIDstr()` only
  while connected as a station. Network > Connection shows **AP MAC address
  (BSSID)** left-to-right and refreshes through the existing 2.5-second polling.
  Disconnected/setup/missing values show **Not available**.
- **Functional: pass (host/build).** Embedded JavaScript syntax and an isolated
  render check pass for AP address changes and disconnected/setup/empty values.
  Temporary harness: `/tmp/radiohead-ap-bssid-check.cjs` (synthetic addresses).
  `pio run -e esp32s3` passes, including the integrated image (56.35 s).
  Arduino RAM: 96,252 / 327,680 B (29.4%); flash: 3,693,263 / 6,553,600 B
  (56.4%). Integrated image: 4,495,344 B; app partition free: 2,058,256 B.
  Build log: `/tmp/radiohead-ap-bssid-build.log`. `git diff --check` passes;
  no secrets, generated artifacts or vendor edits added by this task.
- **Visual/device: not verified; not flashed.** Reuses the Connection panel's
  existing data-row styling. After flashing, compare the displayed BSSID with
  the router/AP's Wi-Fi radio address and observe it across a roaming switch.

### 2026-10-09 — Wi-Fi roaming scan fixes (ready for device testing)

- **Scope:** Fix roaming scan timing and abandoned browser scan results blocking
  roaming. Preserve the ten-minute interval, same-SSID selection, 10 dB margin,
  Wi-Fi credentials and existing scan routes. Preserve concurrent boot changes.
- **Implemented:** Centralized scan ownership in `wifi_network`, retain completed browser
  results for 30 seconds, and prevent browser routes from replacing or consuming
  roaming scans. Use a 120 ms active-scan maximum with the library's 100 ms minimum.
- **Functional: pass (host/build).** `python3 tools/check_wifi_roaming.py` compiles
  production `wifi_network.cpp` with a simulated Wi-Fi driver and ASan/UBSan.
  Checks scan timing, strongest same-SSID selection, the exact 10 dB threshold,
  browser/roaming ownership while running and after completion, consumed and
  abandoned results, scan failures, setup-mode cleanup and unsigned timer wrap.
  `pio run -e esp32s3` passes, including the integrated Spotify image (59.23 s).
  Arduino RAM: 96,252 / 327,680 B (29.4%); flash: 3,692,635 / 6,553,600 B
  (56.3%). Integrated image: 4,494,576 B; app partition free: 2,059,024 B.
  These are shared-worktree reports, including unrelated changes preserved by
  this task. Build log: `/tmp/radiohead-wifi-roaming-build.log`.
  `git diff --check` passes; no secrets, generated artifacts or vendor changes
  were added by this task.
- **Visual: unchanged; not newly verified. Device: not verified; not flashed.**
  After flashing, verify browser scan/result retrieval and closing the browser
  mid-scan, then automatic switching between same-SSID APs at the ten-minute
  scan interval. Check playback/reconnection with USB serial closed.

### 2026-10-09 — Persisted boot modes (ready for device testing)

- **Scope:** Include both existing boot screens in one image, persist a software
  mode selection, and expose an API without adding a web/TFT selector. Keep the
  new-artwork default and current branding/colors; future mode-specific branding
  remains a follow-up.
- **Contract:** `GET|POST /api/device/boot-mode`, with `mode=0` (original)
  or `mode=1` (new artwork). Saves apply after an explicit restart and must report
  storage failures. The POST uses the existing configuration request header and
  maintenance/restart guards. Factory reset restores the default.
- **Baseline:** `pio run -e esp32s3` passes. Arduino RAM 96,244 B; flash
  3,590,103 B. Integrated image 4,392,048 B, with 2,161,552 B app space free.
- **Implemented:** Stable `BootMode` IDs and shared active/configured state;
  `radio/bootMode` unsigned-byte persistence with default/fallback to mode 1.
  Checked single-key writes never change active state. Normal settings saves do
  not overwrite the next-boot selection. GET, POST and `/api/device` expose
  `bootMode`, `configuredBootMode`, and `bootModeRestartRequired`. Both existing
  artwork/bar definitions now ship together with independent style values.
  Software usage and reset policy are documented in `docs/boot-modes.md`.
- **Functional: pass (host/build).** `python3 tools/check_boot_modes.py` runs the
  production persistence code and extracted HTTP handlers with ASan/UBSan.
  Covers absent/corrupt/wrong-type data, both save/reboot directions, open/write
  failures, unrelated-key preservation, invalid API input, request-header and
  maintenance/restart guards, active-versus-configured responses, cancellation
  of a pending mode change, and simulated reset. `git diff --check` passes.
  `pio run -e esp32s3` passes (34.40 s), including the integrated image.
  Arduino RAM 96,244 / 327,680 B (29.4%); flash 3,692,335 / 6,553,600 B
  (56.3%). Integrated image 4,494,320 B, with 2,059,280 B app space free.
  Relative to baseline: reported Arduino RAM unchanged; flash +102,232 B;
  integrated image +102,272 B. Both PNGs are present in flash `.rodata`.
  These are shared-worktree reports including concurrent boot-audio edits,
  which were preserved. Build log: `/tmp/radiohead-boot-modes-build.log`.
- **Visual: not verified.** Existing artwork, bar positions and colors retained;
  no new native boot render evidence captured. Branding/color changes are later
  work, not claimed here.
- **Device: not verified; not flashed.** Save/restart each mode on the radio,
  verify artwork/bar/audio and AP/connected startup, then ordinary settings-save,
  OTA retention and factory-reset behavior. No secrets, generated artifacts or
  vendored changes were added by this task.

### 2026-10-08 — Separate Spotify tab and setup guide (ready for review)

- **Task:** `/root`; files: web server, web CSS, existing browser fixture and setup
  documentation. User explicitly requested a separate Spotify tab, superseding
  the original five-section navigation specification for this addition.
- **Implemented:** Moved both Spotify name and credential forms to `/spotify`, added local
  SVG icons, linked Spotify developer credential instructions and Connect
  playback guidance, and kept the established save APIs and NVS keys. Adapted the
  mobile navigation for six sections, with two rows of three at 420px and below.
  The guide displays the saved Connect name. Device retains maintenance controls;
  stylesheet versioning refreshes previously cached navigation CSS.
- **Functional: pass (host/build).** `node tools/check_spotify_web.cjs` covers
  moved-form ownership, name load/save, UTF-8 byte limits, retained failed drafts,
  unload guards and existing credential flows. Sanitized persistence checks
  (`python3 tools/check_spotify_credentials.py`) and `git diff --check` pass.
  `pio run -e esp32s3` passes (34.87 s), including the integrated Spotify image.
  Arduino RAM: 96,244 / 327,680 B (29.4%); flash: 3,590,103 / 6,553,600 B
  (54.8%). Integrated DIRAM: 260,359 / 341,760 B (76.18%); image: 4,392,048 B,
  with 2,161,552 B app space free. Build log:
  `/tmp/radiohead-spotify-tab-build.log`. No credentials, generated binaries or
  vendor changes are included.
- **Visual: partial, host pass.** Production tab markup and CSS checked on desktop
  and at 320px, including icons, navigation, credential controls and listening
  instructions. Screenshots: `/tmp/radiohead-spotify-tab-preview/desktop.png`,
  `mobile.png` and `mobile-form.png`. Synthetic state only; this fixture does not
  establish full firmware-served visual acceptance.
- **Device: not verified.** No flash, real credential entry or playback changes
  performed. Next: inspect firmware-served desktop/phone pages, save/restart and
  verify Spotify playback/persistence with the serial monitor closed.

### 2026-10-08 — Spotify web credentials (ready for review)

- **Task:** `/root`; files: `src/web_server.cpp`, `src/settings_spotify.cpp`,
  `include/settings.h`, `src/CMakeLists.txt`, integrated Spotify adapter,
  `tools/check_spotify_credentials.py`, `tools/check_spotify_web.cjs`,
  `tools/native_spotify/`, native Spotify README and this ledger. Existing
  worktree changes to root README and touch configuration are preserved.
- **Implemented:** Device page Client ID/Secret form with configured state,
  draft-only Show secret, Save/Cancel, input errors, draft-preserving failures,
  explicit reload and navigation guard. Both values are required for replacement;
  success clears/remasks the draft. Save applies after an explicit restart and
  does not restart or change the active playback session.
- **Contract:** `POST /api/device/spotify-credentials` accepts `revision`,
  `clientId`, `clientSecret` and requires `X-Radiohead-Config: 1` (no CORS).
  Each credential is bounded to 128 non-space printable ASCII bytes. Replies:
  400 invalid input, 403 missing header, 409 stale revision, 409 maintenance or
  scheduled restart, 500 storage failure, 200 durable save. `/api/device` exposes
  only `spotifyCredentialsConfigured` and `spotifyCredentialsRevision`; both
  responses are `no-store`. Revisions change at startup and after saves.
- **Persistence:** One versioned `spotify/app_creds` blob saves the pair together.
  Integrated startup reads it, falling back to legacy `client_id/client_secret`
  only when absent. Legacy keys and `login_blob` stay intact. A malformed new
  record cannot revive old credentials. Existing factory reset preserves Spotify
  credentials/pairing; its web copy now states that policy. CMake source discovery
  tracks the added unit while respecting ESP-IDF's script-mode requirements.
- **Functional: pass (host/build).** Sanitized native checks cover absent/legacy
  data, both invalid fields, limits, complete replacement, failed writes/reads,
  malformed records and bounded unterminated data. Node checks cover entry,
  reveal/remask, duplicate submit, cancel, maintenance, load/save failures,
  conflict/reload and navigation guards. `pio run -e esp32s3` passes (42.35 s),
  including the integrated image. Arduino RAM: 96,244 / 327,680 B (29.4%);
  flash: 3,584,287 / 6,553,600 B (54.7%). Integrated DIRAM: 260,359 / 341,760 B
  (76.18%); image: 4,386,192 B, with 2,167,408 B app space free. These are static
  reports, not runtime/audio measurements. Build log:
  `/tmp/radiohead-spotify-web-build.log`. `git diff --check` passes.
- **Visual: partial.** Desktop production-form markup/CSS inspected in a host
  browser fixture (`/tmp/radiohead-spotify-preview/desktop.png`). Navy forms and
  existing CSS/assets are reused. Phone layout and full firmware-served visual
  acceptance remain open.
- **Device: partial, read-only.** No firmware was flashed by this task. The radio
  was subsequently observed serving the new form with empty credential fields;
  `GET /api/device` returned HTTP 200, `Cache-Control: no-store`, configured=true
  and a numeric revision with no credential fields. No real credentials were
  entered, changed or logged. Next: save a real pair, restart, verify persistence
  and Spotify playback with the serial monitor closed; check phone layout and
  audio continuity while saving. These acceptance gates remain open.

### 2026-10-07 — W6 weather API-key entry (ready for review)

- **Task:** `/root`; affected files: `src/web_server.cpp` and this ledger.
- **Observed:** The live radio reports weather visible but no configured key.
  Existing key entry and durable keep/replace/clear support are hidden under
  Provider access; the page only reports generic weather unavailability.
- **Implemented:** Provider access opens automatically when no API key is saved.
  The field explains entry/save and blank-to-keep behavior; Show entered API key
  applies only to the draft. Success clears and masks the field, while failure
  retains it. Explicit clear disables key entry and sends no draft credential.
  Missing-key, refresh and availability feedback is separate from save feedback,
  and polling leaves drafts and save acknowledgements intact. Save/Cancel wait
  for initial state and cannot dispatch duplicate saves. Existing routes,
  `radio` / `owmKey` persistence, and concurrent calibration edits are preserved.
- **Functional: pass (host/build).** `node --check` and a temporary Node DOM/fetch
  harness pass for missing-key entry, show/mask, replace/keep/clear, cancel,
  draft-preserving polling, availability, and save/load failures. Harness:
  `/tmp/radiohead-weather-key-check.cjs`; no real key was used or logged.
  `pio run -e esp32s3` passes (35.54 seconds), including integrated Spotify.
  RAM: 96,316 / 327,680 B (29.4%); Arduino flash: 3,581,651 / 6,553,600 B
  (54.7%). Integrated image: 4,384,640 B; app partition free: 2,168,960 B.
  Build log: `/tmp/radiohead-weather-key-build.log`. `git diff --check` passes;
  no credential, generated binary or vendor changes were added by this package.
- **Visual: not verified. Device: not verified.** No firmware was flashed.
  Next: inspect the firmware-served page on desktop/phone; enter a real key,
  verify weather arrives, then reboot and verify key persistence. F8/H1/H5
  remain open for the full weather/time package.

### 2026-09-30 — Spotify Connect name (ready for review)

- **Task:** `/root`; files: `src/web_server.cpp`, `src/settings.cpp`, `src/app_state.cpp`, `include/settings.h`, `include/app_state.h`, and `components/radiohead_spotify/spotify_adapter.cpp`.
- **Implemented:** The Device page edits the bounded Spotify Connect name. It is saved in the radio Preferences namespace and loaded before Spotify starts on the next reboot. The existing name remains the default. cspot derives its Connect device ID from the name, so renaming may require selecting the newly named device in Spotify.
- **Functional:** `pio run -e esp32s3` passes, including the integrated Spotify image; `git diff --check` passes. Browser and device checks remain open.
- **Visual:** not verified in a browser. **Device:** not flashed or verified. Next action: save a name, restart, confirm it appears in Spotify Connect, then verify paired playback and the Device page after reboot.

Last updated: 2026-09-30.

**Current state:** The web configuration shell is implemented in firmware.
Spotify's shared-player extension is ready for review; firmware-served browser
and physical-device acceptance remain open.

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
| W5 | Wi-Fi scan/hidden/security/password/connect/forget/recovery | W1/W2 | ready for review | `/root` · 2026-09-25 · `src/web_server.cpp`, web progress ledger | S2 network state/draft/forget corrections compile and script parses; browser and device evidence remain open. |
| W6 | Weather/provider access, units/visibility, timezone and 12/24-hour settings | W1/W2; configuration C3 | ready for review | `/root` · 2026-09-23 · `src/settings.cpp`, `src/app_state.cpp`, `include/settings.h` | Tel Aviv/Jerusalem is now the new-install and untouched-placeholder default; DST boundaries and build pass, while physical-device verification remains open. |
| W7 | Custom background file/framing/preview/default/atomic commit | W1/W2; configuration C4 | not started | — | Define independent asset budget, close F9/H2/H3. |
| W8 | Device info, diagnostics, OTA, restart/reset; release verification | W1/W2; W3–W7 for final release | ready for review | `/root` · 2026-09-22 · `src/web_server.cpp`, `src/firmware_updater.cpp`, `src/settings.cpp`, `src/display.cpp`, `scripts/release.sh` | Browser upload/recovery and GitHub release OTA compile; F11–F13/H6 and real release-channel checks remain unverified on hardware. |
| W9 | mDNS discovery plus QR setup/configuration handoffs | W5; native Network handoff | ready for review | `/root` · 2026-09-23 · `src/main.cpp`, `src/web_server.cpp`, `src/display.cpp`, `tools/generate_network_qr_codes.py` | `radio.local`, both generated QR badges, the 10-second connected handoff and persistent branded AP recovery compile; phone/TFT/device discovery scans remain pending. |

Do not treat W8 as requiring all features before starting its independent device
pages. Its **release completion** depends on the other packages. Existing
configuration-removal and discovery packages retain their own ownership; link
their work instead of implementing duplicate persistence/state machines.

### 2026-09-25 — S2 / W5 Network correction

- **State and affected files:** W5 is ready for review. The prior S2 routes and
  bounded saved-network persistence were already present. This pass changes
  `src/web_server.cpp` and this ledger; S0/S1 are accepted as dependencies for
  this package without declaring their remaining browser/device gates passed.
- **Implemented behavior:** Setup mode identifies the actual soft-AP SSID and
  address. Disconnected state no longer presents `0.0.0.0` as a reachable
  address. The form initializes from saved network metadata without returning
  the password, while subsequent status polling leaves its draft intact.
  Hidden SSIDs are submitted exactly as typed. Open-network selection clears
  the submitted password; the server rejects contradictory password actions.
  Forget names the network and explains restart and setup recovery before the
  request. It now acknowledges durable removal and a scheduled reboot, leaving
  the current connection in place long enough to send the response.
- **Endpoint contract:** `POST /api/network/connect` and `/forget` return 202
  after durable settings changes and deferred restart scheduling. A 202 does
  not assert connection success. The next boot tries only the active saved
  network, or starts the actual setup AP if none is saved or joining times out.
  Other deliberately saved networks remain available to the TFT; forgetting
  the active network can select one of those on reboot.
- **Visual:** not verified; no firmware-served screenshots or keyboard review.
  **Functional:** `node --check` on the embedded Network script,
  `pio run -e esp32s3`, and `git diff --check` pass. Build reports
  96,340 / 327,680 B RAM (29.4%) and 3,492,851 / 6,553,600 B flash (53.3%).
  **Device:** not verified; no Wi-Fi scan, reboot, phone or AP test was run.
- **Next action:** run F7/H4 on a radio and phone: scan while editing, hidden
  exact SSID, open and secured joins, unchanged-password keep, explicit open
  clear, forget with and without saved alternatives, wrong-password timeout,
  setup AP recovery, and address/SSID displayed by browser and TFT.

### 2026-09-25 — S2 live radio follow-up

- Flashed the current `esp32s3` image over the ESP32-S3 USB serial/JTAG port;
  esptool verified the written image and rebooted the radio. The saved network
  remained configured. `GET /api/network` reported `connected`, the expected local
  address, a configured password indicator without disclosing the password,
  and RSSI near −83 dBm. `GET /network` served the 14,276-byte Network page
  with its scan, hidden-network, change and forget controls.
- `POST /api/network/scan/start` returned 202, then the asynchronous scan
  completed with 17 visible secured networks. A following HTTP request timed
  out from the computer's routed `192.168.44.x` network; later API and page
  requests returned HTTP 200. USB output after a subsequent boot reported the
  mDNS responder and `wifi=3` (connected). This does not establish whether
  the scan caused the temporary HTTP timeout.
- The supplied serial log shows station 1 repeatedly timing out during audio
  stream connection at RSSI −81 to −84 dBm, while Wi-Fi remained connected.
  It contains no Wi-Fi credential-change or setup-recovery event and therefore
  does not close F7/H4. Browser draft/confirmation, credential replacement,
  restart and setup-AP recovery remain for a controlled local browser test.

### 2026-09-23 — W9 branded AP recovery follow-up

- No-credential boot, encoder-requested setup and the existing 15-second failed
  station join now render the Wi-Fi join QR in the same branded native card as
  the connected-device boot handoff. The deterministic connection timeout and
  setup AP behavior are unchanged.
- The setup copy reads “Connect to Wi-Fi” and “After joining, open:”, followed
  by the actual soft-AP address and `Radio_Setup` for manual connection. The QR
  still contains only the open-network join payload; it contains no credential.
- The AP handoff is shown immediately after the web server starts and remains
  visible across ordinary UI redraws. The connected `radio.local` screen keeps
  its existing ten-second duration.
- **Visual:** passed in the native production renderer at
  `.pio/ui_native/wifi-setup-boot-smooth.ppm`. **Functional:**
  `python3 tools/render_ui_fonts.py`, `pio run -e esp32s3`, and
  `git diff --check` pass at 96,164 B RAM (29.3%), 3,382,107 B flash (51.6%),
  and 3,432,487 bytes total. **Device:** not flashed or verified; scan and all
  three AP entry paths remain to be tested on hardware.

### 2026-09-22 — W9 mDNS and QR handoffs

- `radio.local` is advertised with an HTTP mDNS service after a successful
  station join. The hostname is set before DHCP. Setup AP mode intentionally
  remains a local `Radio_Setup` network and does not promise `radio.local`.
- Two generated, compile-time QR module grids encode `http://radio.local` and
  the open-AP `WIFI:T:nopass;S:Radio_Setup;;` payload. Static assertions tie
  them to the shared network constants; rerun
  `python3 tools/generate_network_qr_codes.py` after changing either constant.
- A connected radio shows the configuration QR for ten seconds at boot while
  still servicing HTTP. The Network TFT handoff repeats the configuration QR;
  setup mode instead shows the Wi-Fi-join QR with the precise no-credentials,
  connection-failed, or manually requested reason and the AP address.
- **Visual:** not verified on a QR-specific native fixture or physical TFT.
  **Functional:** `python3 tools/render_ui_fonts.py`, `pio run -e esp32s3`, and
  `git diff --check` pass. Build: 89,540 B RAM (27.3%) and 3,352,219 B flash
  (51.2%). **Device:** not verified; scan both QR codes, resolve `radio.local`,
  test the 15-second failed-join transition and test a no-credential boot.

### 2026-09-23 — W6 Tel Aviv timezone default and DST follow-up

- New installs now default to `Asia/Jerusalem`. Version-1 installs that still
  have the untouched Budapest/Europe-Paris placeholder pair migrate in memory
  to Jerusalem, while explicitly configured non-placeholder zones remain
  unchanged. Plain `Tel Aviv`, `Tel Aviv,IL`, and `Tel Aviv,Israel` weather
  locations select the same implemented Jerusalem rule.
- The existing post-2013 Israel DST calculation remains unchanged. An hourly
  comparison with the host IANA `Asia/Jerusalem` data passed for 2024–2037,
  including the 2026 transitions at `2026-03-27T00:00:00Z` and
  `2026-10-24T23:00:00Z`.
- **Visual:** not applicable; no layout changed. **Functional:**
  `pio run -e esp32s3` passed at 96,780 B / 327,680 B RAM (29.5%) and
  3,381,299 B / 6,553,600 B flash (51.6%); `git diff --check` passed.
  **Device:** not verified; flash the radio, confirm its existing placeholder
  settings show UTC+3 during Israeli DST, reboot once, and exercise both DST
  boundaries before closing F8/H1/H5.

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
| Weather/timezone representation and migration | `UTC`, Paris, London, New York, Los Angeles, Tokyo, Jerusalem and Sydney have implemented recurring rules. Exact supported weather `city,country` matches select their zone; unknown or ambiguous locations retain the explicit choice. The former Paris/CET rule is the migration fallback. | W6 — device DST checks remain open. |
| Factory-reset policy | `POST /api/device/factory-reset` requires literal `RESET`. It clears the `radio` namespace (Wi-Fi, stations, volume/tone, weather/time and old radio preferences), the `favorites` namespace, and all station-artwork LittleFS files; the `touch` calibration namespace is retained. There is no custom background asset until W7 implements one, and W7 must extend this policy before enabling its reset integration. | W8 source review + compile; destructive device test remains open. |
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
| Read/scan Wi-Fi | `GET /api/network`; `POST /api/network/scan/start`; `GET /api/network/scan` | No secrets. Scan runs asynchronously and returns observed SSID, security and RSSI for at most 20 visible networks. | State JSON never returns a password. Scan reports `scanning`, `complete`, or `failed`; the existing connection form draft remains browser-local. | Source review + compile; device not verified. |
| Connect/retry/forget Wi-Fi | `POST /api/network/connect`, `/retry`, `/forget` | `connect`: revision, exact SSID ≤32, password ≤63, and explicit `keep`/`replace`/`clear` semantics. | 400 invalid, 409 stale/busy, 202 means credentials were saved and a reboot was scheduled, not that connection succeeded. Next boot attempts the active saved network and falls back to setup AP after timeout; there is no in-place or previous-network recovery. Forget removes the active credential, acknowledges a scheduled reboot, then boots with another saved network or setup AP. | Source review + compile; device not verified. |
| Read/save weather & time | `GET /api/weather`; `POST /api/weather/save` | Save requires revision, location ≤80, C/F, visibility, supported timezone, 12/24-hour choice, and explicit key `keep`/`replace`/`clear`. An exact supported `city,country` location replaces the submitted zone with its location zone. | JSON never returns API key. Successful persistence requests a bounded refresh but does not claim weather success; result exposes refreshing/available/freshness and clock-sync state. | Source review + compile; device not verified. |
| Read device / diagnostics | `GET /api/device` | No arguments. | JSON reports real connection/address, build timestamp, ESP model, uptime, heap/PSRAM, sketch/update space, artwork storage and calibration presence. Service timing is explicitly `Not measured`; logs are `Not available`; no credentials are returned. | Source review + compile; device not verified. |
| Firmware update | `POST /api/device/ota` | Multipart field `firmware`; browser locally reviews its filename/size and the Update library validates while writing. | `202` only after `Update.end(true)` succeeds and a deferred restart is scheduled. Failed/invalid writes return 500 and retain the active firmware. TFT receives write-byte progress; conflicting web and power actions are rejected/deferred while writing. | Source review + compile; device not verified. |
| Restart / factory reset | `POST /api/device/restart`; `POST /api/device/factory-reset` | Reset requires form field `confirm=RESET`; both actions require no update/restart in progress. | `202` schedules the action after its acknowledgement can leave the radio. Reset follows the recorded scope above; touch calibration remains. | Source review + compile; device not verified. |

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
- Implemented behavior: The Stations page lists only saved stream slots, supports optimistic-concurrency add/edit/favorite/remove, rechecks the ten-slot limit at save/import, and stops only the exact removed stream. Discovery uses an explicit browser search against bounded Radio Browser mirrors, ranks exact/whole-token matches deterministically, retains a separately editable draft, and always leaves manual URL entry available. M3U file/paste input is capped at 4 MiB for stress testing, previewed before an all-or-nothing additive import, deduplicated by URL and never replaces occupied slots. Test playback is explicit and non-persistent; it reports the real media state rather than treating dispatch as success.
- Artwork behavior: Browser PNG/JPEG/WebP/CORS-permitted logo inputs are capped at 512 KiB and 2048×2048, fit/crop into 32/64/88 RGB565 variants and upload a fixed 25,744-byte package. The device checks magic, exact size, URL-derived revision and payload checksum, stages to a deterministic temporary file, verifies it, then uses active/backup rename recovery. A changed stream, removal or slot reuse clears prior artwork; a name-only edit preserves it. The browser thumbnail and native renderer reject an association whose current stream URL differs. Logo failure is reported separately after a successful station save; remote CORS failure explicitly falls back to local upload. On 2026-09-21 the user explicitly authorized erasing the current corrupt filesystem: boot now attempts one mount, formats/re-mounts on failure, and latches a failed recovery rather than retrying in page rendering.
- Dependency decisions / actual endpoint changes: New routes are `GET /api/stations`, `POST /api/stations/save`, `/favorite`, `/remove`, `/test`, `/m3u/preview`, `/m3u/commit`, `GET|POST /api/stations/artwork`. They use form fields because the installed Arduino `WebServer` provides bounded form parsing. The artwork endpoint accepts only the browser-generated package, never client paths or base64. No Jev call or runtime AI was added: identity ranking, bounds, revisions and storage validation are deterministic; live TypeSafe documentation was unavailable through the current reader.
- Visual result + evidence: not verified. The page needs screenshots at the required desktop/phone widths, plus focus, modal and CORS/error-state checks.
- Functional result + exact checks/revision: `node` syntax compilation of the embedded Station script passed; `pio run -e esp32s3` passed; final image is 3,262,963 B / 6,553,600 B (49.1% flash) and RAM 82,268 B / 327,680 B (25.1%). `git diff --check` passed.
- Device result + measurements: not verified. In particular, test-stream transitions, LittleFS mount/migration, interrupted replacement/full-storage recovery, RGB565 byte order/native rendering, reboot persistence and audio/service gaps during upload remain open.
- Remaining failures / blockers: The 4 MiB M3U cap is a user-requested stress-test setting, not a validated device budget: Arduino form parsing may require large contiguous allocations and can disrupt audio service. Remote M3U URL handling remains on the retained legacy path and has not been adapted to this preview contract. Browser CORS can prevent a directory logo from being prepared; the visible local-file fallback is intentional, and no open proxy was added. F3–F6 and H2/H3 therefore remain open pending real browser and device evidence.
- Next concrete action: flash this checkpoint; exercise exact/ambiguous/no-match directory search, station conflict/delete/slot reuse, pasted/file M3U capacity and duplicates, CORS-denied/local-logo upload, reboot persistence, native 32/64/88 renders, and measure audio/service gaps during upload.

### 2026-09-21 — W5 Network and W6 Weather & time

- State and affected files: W5/W6 are ready for review. `src/web_server.cpp` provides the responsive Network and Weather & time forms plus revisioned JSON contracts; `src/settings.cpp` owns persistence and a bounded timezone table; `src/display.cpp` owns weather freshness and the shared TFT visibility state; `src/main.cpp` applies the chosen time format/timezone and services the deferred web-requested reboot.
- Implemented behavior: Wi-Fi scans are asynchronous and report observed SSID/security/RSSI without overwriting form drafts. Connect uses explicit keep/replace/clear password semantics and never returns stored credentials. It persists the requested network and schedules a short delayed reboot so the acknowledgement can leave the radio; it does not attempt an in-place connection or previous-network recovery. The next boot tries only the active saved network and otherwise falls back to the actual setup AP. Forget persists credential removal before a deferred reboot; the next boot selects another deliberately saved network if one remains, or starts setup AP. The weather form supports location, C/F, Home visibility, OpenWeather key keep/replace/clear, 12/24-hour display and eight named zones with implemented recurring rules; its default is the prior Central European rule. Exact supported weather `city,country` locations select their corresponding zone, while unknown or ambiguous locations keep the submitted zone. A settings save requests a weather refresh but reports availability separately. Late/stale worker data is rejected; weather becomes unavailable after 30 minutes without a successful refresh. The TFT reads the same visibility, timezone, and clock-format settings.
- Boot setup behavior: holding the encoder switch while booting starts the actual setup AP before the existing calibration interaction runs. Once calibration completes, the Network page is already served from that AP and can register a network through the W5 connection flow. The held press continues to request calibration; device evidence is still required for this combined path.
- Network-form follow-up: removed the Retry connection and saved-password-clear controls; the scan action is labeled “Scan networks.” Selecting a discovered result now copies only its SSID into Network name, leaving security and password choices entirely manual.
- Dependency decisions / actual endpoint changes: Added `GET /api/network`, async scan start/result, connect/retry/forget; `GET /api/weather` and `POST /api/weather/save`. All form mutations use deterministic revisions; they return JSON and never include Wi-Fi passwords or API keys. Wi-Fi connect/retry and retained legacy `/setwifi` save first and request a deferred reboot; `/setweather` and `/scan_data` remain non-restarting/non-blocking. No Jev/runtime AI was added: validation, bounds, persistence, restart timing, timezone selection and freshness calculations are deterministic.
- Visual result + evidence: not verified; no firmware-served browser screenshots at required widths were captured.
- Functional result + exact checks/revision: embedded Network/Weather JavaScript parsed successfully. The deterministic Jerusalem rule gives the 2026 UTC transition instants `2026-03-27T00:00:00Z` and `2026-10-24T23:00:00Z`. `pio run -e esp32s3` and `git diff --check` passed.
- Device result + measurements: not verified. In particular, open/secured/hidden SSIDs, browser loss after a Wi-Fi-save restart, connection to the newly saved network, setup-AP fallback after invalid credentials (without old-network recovery), password/key persistence across reboot, NTP/DST transitions, TFT 12-hour rendering, weather outage/freshness and stream-service gaps during scanning need hardware evidence.
- Build and resource results: current build reported 86,852 B / 327,680 B RAM (26.5%) and 3,266,363 B / 6,553,600 B flash (49.8%).
- Timezone follow-up: OpenWeather's current-weather timezone field is only the location's current UTC offset, so it cannot safely supply the rule for the next daylight-saving transition. The firmware therefore never substitutes that offset for a zone rule. It maps only exact supported `city,country` inputs and adds the post-2013 `Asia/Jerusalem` rule (Friday on/after 23 March through the last Sunday of October); all other supported zones retain their POSIX rule. The shared clock, Home date, and alarm sleep scheduling now use this same conversion.
- Remaining failures / blockers: The bounded zone list intentionally excludes locations whose changing legal DST rules are not yet implemented. Browser/device validation, responsive visual review, two-browser/TFT conflict checks, and all F7/F8/H1/H4/H5 gates remain open.
- Next concrete action: flash this checkpoint and exercise setup AP → scanned/hidden/open/secured network paths, Wi-Fi-save restart/new-network connection, wrong-password restart/setup-AP fallback/forget, weather key keep/replace/clear, C/F + visibility + 12/24 persistence, an exact match and an unmatched location, unavailable/stale weather, and the listed DST boundaries.

### 2026-09-21 — W8 Device & maintenance

- State and affected files: W8 ready for review. `src/web_server.cpp` supplies the maintenance page, bounded APIs and deferred destructive actions; `src/device_control.cpp` owns reset/power protection; `src/settings.cpp` owns artwork-storage reporting and removal; `src/display.cpp` renders the TFT OTA-progress overlay.
- Implemented behavior: The page shows actual device/build/connection/memory/storage/calibration state and labels unavailable service timing/logs honestly. It reviews a selected `.bin` before upload, reports browser transfer separately from the device’s completed write, schedules restart only after `Update.end(true)`, and shows write progress on the TFT. Restart and factory reset have keyboard-dismissible confirmations; reset requires typing `RESET` and follows the recorded scope. The old direct `/update` endpoint no longer writes firmware. Retired preset, visualizer, alarm and color-mutation routes now return 410 without changing state.
- Dependency decisions / actual endpoint changes: Added `GET /api/device`, `POST /api/device/ota`, `/restart` and `/factory-reset`. OTA, reset and power transitions cannot overlap; physical sleep/reset defers while firmware is writing. Custom-background reset remains a W7 dependency because that asset does not yet exist.
- Visual result + evidence: not verified; capture firmware-served desktop/phone Device screenshots, dialog focus/Escape/return behavior and TFT upload progress.
- Functional result + exact checks/revision: all four embedded browser scripts parse with Node `new Function`; a clean `pio run -e esp32s3` passed; `git diff --check` passed. Build: 82,756 B / 327,680 B RAM (25.3%) and 3,274,267 B / 6,553,600 B flash (50.0%).
- Device result + measurements: not verified. Do not test reset on valuable settings without a scoped test device.
- Remaining failures / blockers: F11–F13, H1–H7 and final release completion remain open. Test invalid/oversized/aborted OTA, write failure, browser disconnect, reboot/version re-read, storage/power contention, restart persistence, failed reset storage handling and calibration retention. The retained alarm/visualizer runtime cleanup remains configuration package C1 work; only their web mutations were made inert here.
- Next concrete action: flash a disposable test device; run the F11–F13/H6 matrix and record browser, TFT and serial evidence separately before declaring W8 complete.

### 2026-09-22 — W8 GitHub release-channel follow-up

- State and affected files: Added `include/FirmwareVersion.h`, pinned GitHub roots in `include/UpdateRootCAs.h`, the low-priority `src/firmware_updater.cpp` worker, release-policy persistence in `settings`, Device-page release controls, PlatformIO build identity, and `scripts/release.sh`. Existing browser `.bin` upload remains the local recovery path.
- Implemented behavior: On a connected station the worker waits two minutes after boot, then polls GitHub's `releases/latest/download/manifest.json` hourly or on request. It validates semantic version, per-build image selection, HTTPS GitHub host, manifest MD5 format, image length and inactive-slot capacity before downloading. The image is written to the inactive OTA slot and only restarted after `Update.end()` verifies its manifest MD5. Automatic installation defaults on; an owner can persist ask-first mode and explicitly install a checked release.
- Dependency decisions / actual endpoint changes: Added `GET /api/update/status` and `POST /api/update/check`, `/install`, `/auto`. The update task owns remote TLS/download work so `audio.loop()` and HTTP service remain on the Arduino loop. No Jev/runtime AI was added: release/version ordering, bounds, URL allow-listing, digests and policy persistence are deterministic; the TypeSafe documentation reader was unavailable in this environment.
- Functional result + exact checks/revision: `pio run -e esp32s3` passed with 94,236 B / 327,680 B RAM (28.8%) and 3,353,555 B / 6,553,600 B flash (51.2%). GitHub publishing has not been run by this work.
- Device result + measurements: not verified. Validate pinned-TLS manifest fetch, no-release/up-to-date/manual/automatic paths, corrupt digest/length/URL rejection, interrupted download, reboot into the inactive slot, rollback behavior, browser status transitions, audio continuity and TLS heap under concurrent weather/podcast activity.

### 2026-09-22 — W8 release-status lifecycle and TFT follow-up

- State and affected files: Corrected the Device-page release control lifecycle in `src/web_server.cpp`; `firmware_updater` now exposes non-blocking request acceptance and a state revision; `src/main.cpp`, `ui_controller`, and `display` add the local update screen and availability signal.
- Implemented behavior: A web `Check now` request returns immediately with `202`, then the page polls `/api/update/status` through checking, up-to-date, update-found/download/install, manual decision, and failure states. This follows the micro-radar release-status pattern and avoids the former client-side condition that stopped status polling as soon as a check was busy. The web page persists either automatic installation or manual approval; in manual mode it presents `Install update` only after a release has been checked. On the TFT, Device → Firmware updates provides `Check for updates` and a tap-to-switch automatic/manual mode. A held manual release is signaled by `UPDATE AVAILABLE` on that screen and on Home.
- Dependency decisions / actual endpoint changes: `/api/update/check` now reports queue acceptance only; terminal state remains exclusively observable through the status endpoint. The update worker still owns TLS and image transfer, rather than performing network work in HTTP or audio service.
- Visual result + evidence: native rendering and on-device alert legibility not verified.
- Functional result + exact checks/revision: `pio run -e esp32s3` passed; embedded release-control JavaScript parsed with Node `new Function`; `git diff --check` and `bash -n scripts/release.sh` passed. Build: 94,252 B / 327,680 B RAM (28.8%) and 3,357,055 B / 6,553,600 B flash (51.2%).
- Device result + measurements: not verified. Exercise page navigation after a no-release check, failed TLS request, manual release discovery/alert/install, automatic install/reboot, policy switching while a release is waiting, and ensure stream continuity throughout each worker stage.

### 2026-09-29 — Spotify shared-player package U2 (ready for review)

- State and affected files: `src/web_server.cpp` and `radiohead.css` add source-aware fields and presentation to the shared player on all five settings sections. Existing control fields, optimistic control revision, endpoints and 0–21 volume/−15…15 tone bounds remain.
- Implemented behavior: `/api/player` adds `source`, canonical `playbackState`, `mediaRevision`, bounded metadata, empty artwork identity, and action availability. The strip polls every three seconds, replaces Spotify metadata on pause/transfer/source switch, uses `textContent` for remote strings, and keeps a focused or debounced volume/tone edit intact. The Spotify placeholder is CSS geometry; no image URL or credential is exposed.
- Visual: not verified in a firmware-served browser. The desktop mockup remains a design reference.
- Functional: embedded player JavaScript parsed with Node; an isolated DOM/fetch check passed for playing → paused → inactive, placeholder visibility, metadata clearing and a focused volume edit. Normal and integrated firmware builds passed. Browser/device HTTP checks remain open.
- Device: not verified. Check phone-driven metadata, volume/mute/tone audibility, transfer away and browser/TFT agreement on the physical radio with USB serial closed.

### 2026-10-09 — Startup Wi-Fi handoff (ready for device testing)

- Scope: remove the post-boot blank Wi-Fi wait; keep the QR welcome screen visible while joining and gate radio startup at its end. Start/reopen HTTP and mDNS when the network becomes usable, including setup-AP fallback. Existing forms/routes and saved keys remain.
- Functional: `pio run -e esp32s3`, `python3 tools/check_startup_sequence.py` and `git diff --check` pass. Sixteen host scenarios cover early/late joining, no network, short-boot deadline extension, missing credentials, explicit setup and a dropped/recovered connection for normal and Spotify mDNS registration. HTTP/restart/audio servicing continues during the QR hold. No web routes/forms or persistence keys changed.
- Build: normal target RAM 96,252 B (29.4%, unchanged); flash 3,693,027 B (56.4%, +392 B). The post-build integrated Spotify image is 4,495,024 B (+448 B), with 2,058,576 B of application-slot headroom.
- Visual: `python3 tools/render_ui_fonts.py --network-boot-only` passes; connecting/connected/setup layouts inspected at native 320 × 240 under `.pio/ui_native/`. QR module grids in that host seam are placeholders; camera readability is not verified. No browser UI was restyled.
- Broader native fixture: the default full renderer still fails the existing Spotify `photoHintCard` pixel assertion; the pre-change source snapshot reproduces that failure. It is outside this startup change.
- Device: not verified or flashed. Check both boot modes with fast/slow/unavailable saved Wi-Fi, the final setup fallback, actual QR scanning/mDNS reachability, HTTP during the handoff, and boot/station/Spotify audio with USB serial closed.
