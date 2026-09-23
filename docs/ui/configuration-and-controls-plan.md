# Configuration and physical controls: implementation specification

Date: 2026-09-21

Status: implementation specification following the user's configuration review.
No firmware changes or hardware acceptance are claimed by this document.

## Authority and scope

This document supersedes conflicting configuration, encoder, alarm, timer and
customization requirements in the older UI implementation guide and concept
panels. In particular, do not implement the old P6 package as written. The
[visual contract](visual-contract.md) still governs appearance, and the
[station discovery and artwork plan](station-discovery-and-artwork-plan.md)
still governs station search and persistent station logos.

The user's rules are:

- Put practical everyday controls on the device.
- Put typing, passwords, URLs, searching for new content and file uploads on the web.
- Put technical configuration and debugging on the web.
- The encoder controls volume and mute; a long press requests sleep, or power-off
  on battery. Separate Standby / Power Off UI remains **TBD**.
- Removed features are not to be relocated to the web as a workaround.

Implement the retained scope below. Items explicitly labelled future or TBD are
not prerequisites for completing the independent settings/removal work. Do not
advertise an unavailable feature as functional.

## 1. Encoder contract

| Input | Required normal-runtime behavior |
| --- | --- |
| Turn | Change volume using the existing bounded volume mapping. |
| Short click | Toggle mute/unmute, preserving the chosen volume. Mute must not stop, pause, retune or restart playback. |
| Long press | Request sleep on external power; request turn-off on battery, subject to the hardware/power decisions below. |

Interpret these as consistent device-wide controls, including while browsing
stations, shows and settings. Replace the current page-dependent encoder focus,
selection and Back behavior. Touch owns navigation, selection, form editing and
confirmation; retain usable on-screen Back and paging controls everywhere.
The TFT supplies volume/mute feedback rather than requiring an interactive
volume slider or mute button. Existing web remote-volume controls may remain.

Use one debounce/short/hold classifier. A hold fires once and must not also emit a
mute click on release. A short click is resolved on release so the beginning of a
long press does not mute first. Retain the current 700 ms hold threshold initially.
Preserve encoder direction, detents, bounds and shared-task synchronization.

Implementation defaults: turning while muted adjusts the stored volume while
remaining muted; unmute uses that value. A dimmed but awake screen wakes while
applying the explicit encoder volume/mute action once. Touch dim-wake still
consumes its first press. No action may leak into a newly opened page.

Preserve the deliberate encoder-held-at-boot calibration entry; it is an exception
to normal-runtime controls. During OTA, defer power transitions until writing has
finished safely and display the reason. Do not use encoder click to accept a
destructive dialog.

### Power decisions that remain open

The requested long-press intent is settled; the electrical behavior and separate
Standby / Power Off menu are not. Source inspection found deep sleep with K0 wake,
but no battery-source detection or controllable power-latch/PMIC implementation
in application code. This is a code observation, not a hardware inventory.

Before completing power behavior, establish:

1. How external power versus battery is detected reliably, including unknown state.
2. Whether the board can cut power, or only enter a low-power sleep state.
3. What sleep means on external power, and which physical control wakes it.
4. Whether the separate Standby / Power Off UI and existing `/off` route remain,
   and whether a confirmation is wanted.

Do not invent pins, equate deep sleep with electrical power-off, or promise wake
from touch/encoder without verification. Preserve existing power/wake recovery
until an explicit replacement is implemented; do not mark the battery branch
complete by silently calling `goToSleep()`. The current K0 path must be reviewed
alongside the new encoder hold behavior. Removing the alarm must remove its timer
wake scheduling without accidentally removing the physical wake source.

## 2. Retained configuration and placement

TFT means a complete local interaction; existing useful web equivalents may remain.
Web-only setup may have a short TFT handoff showing the actual local address and,
if scan-tested, a QR code. Never include credentials in that code.

| Feature | Placement | Implementation notes |
| --- | --- | --- |
| Volume and mute | Encoder; TFT feedback | Follow section 1. Preserve web remote volume. |
| Custom bass / mid / treble | TFT; existing web equivalent | Keep the three tone controls and their saved values. Presets are removed. |
| Automatic dimming | Device; TFT timeout setting | Retained from the initial proposal. No brightness-level editor or separate idle/clock mode. Preserve existing dim/wake behavior until this setting is implemented. |
| Prepared theme selection | TFT, only for supported themes | Retained from the initial proposal; do not invent a theme collection. One supported theme needs no selector. Arbitrary color editing is removed. |
| Custom background upload | Web | Retained explicitly; see section 4. |
| Clock format and time zone | Web | 12/24-hour choice and user-friendly timezone selection. TFT displays the resulting time. A recognized unambiguous weather location selects its matching implemented zone; otherwise retain the explicit zone choice. New installs and untouched legacy Budapest/Europe-Paris placeholders default to `Asia/Jerusalem`; explicitly configured non-placeholder zones remain unchanged. |
| Weather units and Home visibility | Web | Celsius/Fahrenheit and weather on/off. Keep unavailable/stale weather states honest. Weather and clock may still appear on Home, but have no TFT Settings section. |
| Weather location | Web | Search or text entry; TFT shows the chosen location. |
| Weather API key/provider setup | Web | Keep secrets out of returned pages, display, logs and artifacts. |
| Wi-Fi status / retry | TFT | Network name, connection state, signal indicator, Retry and setup handoff. |
| Start Wi-Fi setup mode | TFT | Reachable when the saved network is unavailable; show actual AP name and local address. |
| Wi-Fi network/password/forget | Web | Network selection and credentials stay in one browser flow. |
| Discover/add/edit stations | Web | Search, editable name/stream URL, explicit Test on radio, Save and manual fallback per the discovery plan. Initial limit remains ten slots. |
| M3U import | Web | Preserve manual and file import paths and station/artwork identity rules. |
| Station artwork | Web preparation; TFT display | Search/logo suggestion, local upload and crop/fit belong in the browser. Saved artwork survives reboot and directory outages. |
| Station/show favorites | TFT; web equivalent where supported | Add/remove beside each entity. No manual reorder feature. |
| Remove a saved station | TFT + Web | Confirm the exact station; distinguish deletion from removing a favorite. Do not silently play a replacement slot occupant. |
| Add/edit recorded-show sources | Web, future capability | Still future work; existing show browsing/playback/favorites remain on TFT. |
| Touch calibration | TFT | Preserve boot recovery; an in-device maintenance action is appropriate because calibration requires touching the panel. |
| About | TFT | Firmware version, device name and web address; detailed diagnostics go on the web. |
| Rename device | Web, future capability | Text entry; not a prerequisite for the core settings work. |
| Firmware update | Web action; TFT status | Upload through the browser; reflect actual update progress/result on the TFT. |
| Restart / factory reset | TFT + Web | Separate these from the TBD sleep/off menu. Confirm reset scope and preserve calibration by default. |
| Logs / diagnostics | Web | Network/stream errors, memory/storage, service timing and touch diagnostics. Do not expose secrets. |
| Advanced network/audio configuration | Web, only when needed | E.g. DNS, NTP or buffering. This placement is not a requirement to add speculative tuning controls. |
| Standby / Power Off menu | **TBD** | No new menu implementation until section 1's power questions are resolved. |

## 3. Remove and deprecate

Here, deprecate means remove active application behavior and user-facing controls,
not merely hide a TFT row. Do not add new implementations of absent features.
Remove stale declarations, handlers, commands, render branches and tests/fixtures
that exist solely for the removed feature. Keep shared infrastructure still used
by retained behavior and do not modify the vendored audio library.

| Removed feature | Required disposition |
| --- | --- |
| Equalizer presets | Remove preset buttons, preset-name dispatch and `/setpreset` behavior. Keep custom bass/mid/treble and `/seteq`. Do not replace old presets with Normal/News/Music/Voice. |
| All alarms | Remove enable/time, station, volume, repeat-day plans; runtime triggering, volume ramp, dismissal, alarm overlays/state, `/setalarm` and alarm-based timer wake. |
| Sleep timer | Remove countdown settings/commands/state if present; do not implement the planned duration menu or timed sleep. Physical long-press sleep is distinct and retained. |
| Screen brightness setting | Remove user adjustment UI/routes/settings if present. Keep the backlight primitives needed for normal rendering, dimming and power transitions. |
| Idle screen / clock view | Remove the selectable mode and dedicated idle-clock feature; retain the ordinary Home clock and automatic dimming. |
| Visualizer on/off and mode | Remove spectrum/VU setting UI, `/togglespec`, `/setVisual`, mode/state and feature-only rendering. Keep unrelated playback progress and volume feedback. |
| Interface language setting | Remove language-selection UI and unfinished locale-switching plans. Preserve existing readable Hebrew, Latin, mixed text and fonts. |
| Reorder saved stations/favorites | Remove this configuration feature. Keep a stable deterministic display order and identity-safe slot editing/import. |
| Backup / restore configuration | Remove from scope and UI; do not add export/import endpoints. Station M3U import and OTA remain. |
| Detailed color customization | Retire the color-editor form and `/setskin`/`/defaultskin` mutations. Remove obsolete skin preference behavior; keep renderer palette tokens needed by the supported visual design. Custom background upload remains. |

### Persistence and old routes

- Stop loading/saving retired settings (`almH`, `almM`, `almA`, `spec` and obsolete
  customizable skin keys, plus any newly discovered feature-specific keys).
  Old stored values must not re-enable removed behavior after upgrade.
- Prefer leaving ignored legacy keys inert initially rather than clearing an
  entire Preferences namespace. Any later cleanup must target only retired keys.
- Preserve unrelated station names/URLs, Wi-Fi credentials, weather configuration,
  volume, custom EQ, favorites and touch calibration. In particular, removing
  presets must not reset the user's saved bass/mid/treble curve.
- Remove navigation/forms for retired endpoints. Retired mutations must have no
  side effects: use an explicit 410 Gone response during transition or the normal
  not-found response after removal. Never report a successful save for them.
- Older instructions to preserve every alarm/skin route and key are superseded
  for these specifically retired features; retained routes remain compatible.

## 4. Custom background upload

This is independent of station logos and survives removal of detailed colors.
Serve the editor from the ESP32; use the browser to select, preview and crop/fit
an image to native 320 × 240. Prepare the device-ready representation in the
browser and validate dimensions, format and bounded payload again on the ESP32.
Do not add an AI service, paid API or project-operated backend.

Use a separate background asset identity/storage path from station artwork. Keep
the previous valid background until the new asset is validated and durably saved;
handle interrupted uploads, invalid images, full storage and reboot. Provide a
web action to restore the bundled default background. Do not reuse the retired
skin-reset endpoint or erase station artwork. Define background reset behavior
as part of the factory-reset policy.

Retain readable navy surfaces, white text and blue active states over custom
images. Keep dynamic text and controls rendered natively; do not upload a baked
screen image as the application UI. No image decoding/download or runtime blur
inside rendering. Reuse the discovery plan's bounded upload/storage principles,
but define a separate background size budget rather than its station-logo limit.

## 5. Navigation and shared behavior

Compact TFT Settings groups: **Audio, Display, Wi-Fi, Device**. Weather and time
configuration has no TFT Settings section; it remains available on the web.
Display contains only retained dimming/theme choices that actually work and a
background-upload handoff. Clock format/timezone editing is on the web. Favorites
and station removal stay in entity options. Do not leave removed settings as
disabled rows or Coming Soon entries.

Suggested web groups: **Stations, Network, Weather & Time, Appearance, Device &
Maintenance**, with retained remote controls. Add Recorded Shows configuration
only when editable sources exist. Technical details belong under maintenance.

Both surfaces use shared validation and committed state. Web changes must reach
the TFT, while an open TFT draft must not silently overwrite a newer web value.
Use explicit save/cancel for multi-value editors and honest persistence outcomes.
Ordinary browsing and station search must not interrupt playback; Test on radio
is a clearly labelled action that changes playback.

## 6. Implementation order and ownership

Read current source and diffs before each package; the repository has concurrent
display/font/artwork work. These packages do not authorize overwriting that work.

| Package | Owners and deliverable | Required evidence |
| --- | --- | --- |
| C1 — Retire features | `web_server`, `settings`, `app_state`, `main`, `device_control`, controller/display: remove section 3's active behavior and obsolete scope. | Upgrade with old alarm/skin/visualizer values; no alarm playback or timer wake; retained custom EQ/settings still work; retired routes have no effect. |
| C2 — Encoder controls | `device_control` for input, `ui_controller` for routing, `media` for volume/mute; keep `main` orchestration-only. | Turn/click/hold across Home, live and podcast players, lists, editors and dialogs; no navigation side effects or click-after-hold. All navigation remains touch-accessible. Power hardware branch remains open until verified. |
| C3 — Retained settings | `settings`, `web_server`, controller/display; time startup remains coordinated by `main`. | Native TFT settings fixtures, shared web/TFT edits, cancel/save/reboot behavior, timezone/DST and 12/24-hour display checks. No removed controls. |
| C4 — Background upload | `web_server` browser/upload path, `settings` storage lifecycle, `display` validated cached rendering. | Browser conversion and failures, reboot/interrupted replacement/full storage, native render comparisons, physical TFT legibility and audio continuity. |
| C5 — Power completion | `device_control` and explicit hardware configuration after resolving section 1. | External-power and battery behavior, unknown source, actual current/power state and physical wake path. Do not close on a display message alone. |

Station discovery/artwork continues under its existing delivery packages. Keep
new background uploads and OTA from racing storage/power transitions. Do not
block `audio.loop()` or `server.handleClient()` with network/image processing or
copy the existing five-second power-screen delay into normal UI work.

For source/configuration changes run `pio run -e esp32s3` and `git diff --check`;
inspect RAM/flash usage and the final diff. Use targeted host tests for meaningful
input classification, bounds, migration and state conflicts. Verify audio, TFT,
encoder, mute, dim-wake, Wi-Fi setup, OTA and power behavior on the physical device.
Record native visual evidence and hardware evidence separately in implementation
status; compile success does not close visual or power acceptance.

## Planning provenance

This specification follows the user's 2026-09-21 corrections and inspected
settings, web routes, encoder/controller and power code. The TypeSafe skill's
separation of explicit rules from semantic judgment was applied; these decisions
need deterministic implementation, not runtime AI. Live TypeSafe documentation
was inaccessible during planning; no live Jev judgment or API call was made.
This documentation task requires no firmware build or device flash.
