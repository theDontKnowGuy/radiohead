# Firmware organization review — 2026-10-03

`main.cpp` contains only the Arduino entry points. `application.cpp` coordinates
startup and the cooperative loop; implementations stay in their feature modules.
Keep application code in `src/`, public interfaces in `include/`, build/validation
tools in `tools/`, and third-party code in `lib/` or `components/`. The existing
flat application layout makes each implementation/header pair easy to find.

| Responsibility | Owner |
| --- | --- |
| Arduino entry points | `main.cpp` |
| Startup and runtime service ordering | `application.cpp` |
| Wi-Fi join, setup AP, NTP and roaming | `wifi_network.cpp` |
| Hardware input, calibration, dimming and power | `device_control.cpp` |
| Navigation, focus, drafts and semantic commands | `ui_controller.cpp` |
| Input routing and command side effects | `ui_runtime.cpp` |
| Audio setup, station/podcast playback and Spotify handoff | `media.cpp` |
| TFT initialization, render/hit-test code, redraw scheduling and weather | `display.cpp` |
| Preferences, validation and artwork storage | `settings.cpp` |
| HTTP routes, browser UI, uploads and mDNS | `web_server.cpp` |
| Release checks and automatic/manual installation | `firmware_updater.cpp` |
| Shared hardware objects, models and state | `app_state.cpp` |
| Boot presentation and audio | `boot_screen.cpp` |
| Typography and text preparation | `display_fonts.cpp`, `ui_text.cpp` |
| Service timing and opt-in diagnostics | `validation_diagnostics.cpp` |

## Refactor guarantees

The moves preserve startup ordering, audio/HTTP servicing order, dim-wake input
sampling, command validation and side effects, task priority/core/stack size,
redraw intervals, network timeout/roaming rules, routes and Preferences keys.
The UI controller retains its hardware-independent boundary. The application
captures the clock between input polling and command dispatch as before.

No pin, visual design, feature scope, SDK or vendored-library change is included.
Module ownership was reviewed directly against source and callers. TypeSafe's
separation of semantic judgment from deterministic work guided the review; no
live Jev call was needed or made. Compiler and host checks establish the results
below, with device behavior requiring physical verification.

## Existing review findings outside the structural refactor

- **Medium: synchronous playlist fetching stalls cooperative services.**
  `/scan_m3u` in `web_server.cpp` calls `parseM3UPro()` synchronously. Station
  playback similarly resolves `.m3u`/`.m3u8` through `parseM3U()`. Both execute
  HTTP GET on the Arduino loop with a 3-second read timeout. `readHttpLines()`
  services local audio while consuming the body, but has only an idle timeout,
  so a continuously dripping response without usable station entries can hold
  the handler indefinitely. HTTP/UI/media state servicing cannot resume until
  it returns. A bounded worker/result handoff with a total deadline is needed.
- **Medium: ordinary settings writes cannot report persistence failure.**
  `settings.cpp::saveSettings()` ignores `Preferences::begin()` and all write
  results and returns void. `/seteq`, `/setvol` and local tone commits can change
  live state and return normally despite a failed flash write; the previous
  values then reappear after reboot. A checked save result and caller error
  handling are needed. The separately checked favorites, Wi-Fi and weather
  save paths do not eliminate this ordinary-save failure path.

These are source-backed findings, not observed hardware failures. Resolving them
changes runtime/failure behavior and is kept separate from the source moves.

## Verification

- `pio run -e esp32s3`: passed for the Arduino target and integrated Spotify
  image. Reported Arduino RAM is unchanged at 96,252 / 327,680 bytes (29.4%).
  Arduino flash is 3,511,599 / 6,553,600 bytes (53.6%), up 1,300 bytes. The
  integrated image is 4,314,672 bytes, up 1,888 bytes, with 2,238,928 bytes free
  in its OTA application slot.
- `python3 tools/check_touch_input.py`: passed raw sampling, press/re-arm,
  paging and Spotify source transitions.
- `python3 tools/check_podcast_reader.py`: passed packet splits, truncation,
  idle/total deadlines and clock wrap checks.
- `git diff --check`: passed. Scoped source/header diff contains no credential,
  generated artifact, pin, route, persistence-key or vendor edit.
- Direct source comparison confirmed the moved network, power, input, command,
  refresh and media bodies preserve their original logic.
- `python3 tools/render_ui_fonts.py`: compiled, then failed the existing Spotify
  fixture assertion at `tools/native_ui/home.cpp:649`, checking the background
  pixel at `(299, 190)`. The script's extracted production layout and hit-test
  source are byte-identical to pre-refactor HEAD. This existing fixture failure
  remains unresolved; visual acceptance is not claimed.

No device was flashed or exercised. Boot, continuous radio/Spotify/podcast audio,
HTTP responsiveness, touch/encoder dim-wake behavior, roaming, OTA and sleep/wake
still require physical verification.
