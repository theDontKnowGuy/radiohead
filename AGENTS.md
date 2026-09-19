# Radiohead firmware agent instructions

## Project intent

Maintain the ESP32-S3 internet-radio firmware as a reliable embedded application.
Preserve working device behavior while making changes easy to review, build, and
reverse. Treat the web UI, TFT UI, audio stream, encoder, alarm, settings, podcasts,
weather, OTA flow, and sleep behavior as parts of one product.

## Required skills

- Use the `typesafe-ai` skill for every task in this repository, including explanation,
  implementation, refactoring, diagnosis, and review. Use it to separate deterministic
  work from semantic judgment and to consider whether a narrow Jev decision would
  improve the workflow.
- Use the `radiohead-firmware` skill for application code in `src/`, public interfaces
  in `include/`, or changes to `platformio.ini`.
- A skill informs the work; it does not automatically justify adding an SDK, making a
  paid API call, or changing firmware behavior.

## TypeSafe and Jev in the coding workflow

- Jev is a typed judgment model, not a code generator. The coding agent writes code;
  compilers, tests, and device observations establish correctness.
- Jev may assist vibe coding by routing an ambiguous request, selecting relevant
  skills, checking whether a plan covers natural-language requirements, scoring review
  concerns, or deciding whether uncertainty warrants a deeper reasoning pass.
- Keep exact operations in code: arithmetic, array bounds, time comparisons, parsing
  with a defined grammar, pin assignments, build configuration, and side effects.
- Never claim that Jev compiled, executed, or proved code correct.
- If `TYPESAFE_API_KEY` is unavailable, use the TypeSafe skill's design guidance and
  continue without pretending a live judgment occurred.
- If live TypeSafe experiments are appropriate, keep state small, ask narrow typed
  questions, centralize questions and thresholds, record the model version, and validate
  the result against representative examples before relying on it.
- Never embed a TypeSafe credential in firmware, browser-delivered HTML, serial output,
  committed files, or logs. Any future runtime integration belongs in a trusted service
  unless the user explicitly chooses a different architecture after reviewing the risk.

## Repository boundaries

- Application code lives in `src/`; public module interfaces live in `include/`.
- `src/main.cpp` owns startup and top-level runtime coordination. Keep it orchestration-
  focused rather than moving feature implementations back into it.
- Shared hardware objects, constants, models, and application state belong in
  `app_state`.
- Keep media/network playback logic in `media`, persistence in `settings`, TFT rendering
  in `display`, input/power behavior in `device_control`, and HTTP/OTA behavior in
  `web_server`.
- Treat `lib/ESP32-audioI2S-master/` as vendored third-party code. Do not edit it unless
  the user explicitly requests a vendor patch and the reason is documented.
- Preserve user changes already present in the worktree. Inspect `git status` and the
  relevant diff before modifying overlapping files.

## Embedded constraints

- Do not block or perform slow network work in the hot audio servicing path.
- Keep `audio.loop()` and `server.handleClient()` serviced frequently.
- Avoid unbounded allocation, recursion, and large temporary buffers. Be especially
  careful with repeated `String` concatenation inside fast loops.
- Validate every index and web-supplied number before indexing stations, episodes, or
  `volCurve`.
- Keep interrupt/task-shared state minimal. Preserve `volatile` where data is written by
  the encoder task and read by the Arduino loop.
- Preserve persisted Preferences keys unless migration behavior is intentionally added.
- Do not expose Wi-Fi passwords, API keys, or other secrets in logs, pages, diffs, or
  test output.
- Keep hardware pin and display-controller changes explicit and easy to audit.

## Change workflow

1. Identify the behavior requested and the module that owns it.
2. Read the relevant header and implementation plus their direct callers.
3. Distinguish observed facts from assumptions. Verify unstable library or API behavior
   in primary documentation when it affects the implementation.
4. Prefer the smallest coherent change. Preserve routes, settings keys, and device
   behavior unless the request requires a change.
5. Add validation at external boundaries rather than scattering defensive checks across
   unrelated modules.
6. Build after source or configuration changes and inspect the final diff for accidental
   edits, credentials, generated binaries, and changes to vendored code.

## Verification

Run the checks relevant to the change. The normal firmware baseline is:

```sh
pio run -e esp32s3
git diff --check
```

Also inspect PlatformIO's RAM and flash report. A successful build is necessary but does
not replace on-device verification for audio, display, encoder, Wi-Fi, OTA, alarm, sleep,
or wake behavior. State clearly when hardware verification was not performed.

## Code review rules

- Flag out-of-range indexing, stale UI state, blocking work in `loop()`, credential
  exposure, persistence incompatibility, unsafe OTA handling, and task/shared-state races.
- Flag behavior changes hidden inside a refactor, including altered web routes, HTML form
  semantics, station selection, volume mapping, alarm timing, or display redraw behavior.
- Treat external API response shape, timeout handling, and memory use as correctness
  concerns on this device.
- Prefer findings backed by a concrete execution path or build/runtime evidence. Do not
  report speculative style preferences as defects.

## Definition of done

A code task is complete when module ownership remains clear, required behavior is
implemented, relevant builds/checks pass, no secrets or generated artifacts were added,
and the handoff distinguishes verified results from items that still require hardware.
