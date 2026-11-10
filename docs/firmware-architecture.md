# Where the code lives

Application implementations are in `src/`, public headers in `include/`, and build/
check scripts in `tools/`. Keep third-party code in `lib/` and `components/` separate.
The [current audio path](hardware/current-audio-hardware.md) is PCM5100 → HX-A244.

| Job | File/module |
| --- | --- |
| Arduino entry points, startup and loop order | `main.cpp` |
| Wi-Fi, setup AP, NTP, roaming | `wifi_network.cpp` |
| Encoder, touch calibration, dimming, sleep | `device_control.cpp` |
| Navigation/drafts; input side effects | `ui_controller.cpp`; `ui_runtime.cpp` |
| Radio, podcasts, Spotify handoff | `media.cpp` |
| TFT rendering, hit testing, redraws, weather | `display.cpp` |
| Preferences, validation, artwork | `settings.cpp` and settings submodules |
| HTTP pages/routes/uploads/mDNS | `web_server.cpp` |
| Release checks and firmware installation | `firmware_updater.cpp` |
| Shared hardware/models/state | `app_state.cpp` |
| Boot; fonts/text; timing diagnostics | `boot_screen.cpp`; `display_fonts.cpp`/`ui_text.cpp`; `validation_diagnostics.cpp` |

Keep startup orchestration separate from features. Preserve routes, saved keys,
pins, volume mapping and audio servicing when moving code. The [single native build](building.md)
includes Arduino as a component and Spotify.

## Findings from the 2026-10-03 review

- Synchronous M3U resolution could hold the cooperative loop: a 3 s read timeout
  did not bound a continuously dripping response. A bounded background job with
  a total deadline was recommended.
- Ordinary `saveSettings()` ignored write failures; a live change could appear
  saved and disappear on reboot. Checked persistence and caller errors were needed.

These are source findings from that review, not new device observations.
The structural refactor passed build, touch, podcast and diff checks without
changing behavior. Native rendering failed an existing Home `(299,190)` fixture;
no hardware test was claimed. Keep feature verification separate from code moves.
