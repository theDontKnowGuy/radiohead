# Native Spotify: execution handoff and pre-UI acceptance

Updated: 2026-09-24. Initial source baseline: `b47961c`.
Status: ready to execute; every hardware gate below is **NOT VERIFIED**.
Research and candidate links: [Spotify investigation](spotify-integration-research.md).

## Binding scope

The user wants actual Spotify Premium audio on the existing ESP32-S3 radio,
smooth transitions to/from existing playback, enough memory and demonstrated
stability **before Spotify UI implementation**.

- Native ESP32 only. No Linux, Raspberry Pi, proxy, token broker or other
  always-running companion. The Spotify phone app is the normal controller.
  A development computer may build, flash, provision and collect diagnostics;
  it must be removable during playback, renewal, reboot and reconnection.
- Preserve internet radio, recorded shows, existing web/TFT operation, volume,
  mute, custom tone, persistence and device recovery. Do not replace the product
  with another project's firmware. A separate standalone build is only a spike.
- No new Spotify screens, album-art downloader, catalog browser, navigation
  redesign or polished setup flow until S0–S4 pass. Minimal temporary diagnostic
  controls are allowed and must be gated off in normal builds.
- Keep the current renderer, weather, web handling and other normal workloads
  enabled during integrated acceptance. An audio-only demo cannot establish fit.
- License/commercialization work is not a gate: this is a personal project.
  Retain upstream notices without diverting into certification or sales work.
- No secrets in source, compiled-in headers, logs, URLs or committed evidence.
  Keep test provisioning private and redact identifiers/account data. A source
  file excluded by `.gitignore` still embeds credentials if compiled.
- Follow AGENTS.md and required TypeSafe/radiohead skills. No AI dependency or
  live Jev call is needed; measured evidence establishes these gates.
- Preserve unrelated worktree changes. `build/` was already untracked when this
  handoff was prepared; it is not ours to remove or assume safe to overwrite.

The user has authorized pursuing native feasibility and validation. Continue
through available stages without routine permission questions. Ask only for
missing physical access, necessary account actions/private provisioning, an
ambiguous target device, or a genuinely new out-of-scope decision. Never request
credentials in chat or treat absent hardware as a passing test.

## First actions for the executing agent

1. Read this file, research, AGENTS.md, required skills and
   [configuration/controls](ui/configuration-and-controls-plan.md). Recheck HEAD,
   status and relevant diffs; the recorded source baseline may have changed.
2. Read `include/media.h`, `include/app_state.h`, `src/media.cpp`,
   `src/app_state.cpp`, `src/main.cpp`, direct audio callers in display/web/power,
   and the vendor audio public API/lifecycle. Identify pending recovery timers,
   callbacks and workers that can act after switching sources.
3. Establish an isolated branch/worktree if needed. Keep generated firmware and
   private device state outside committed artifacts. Preserve the current
   partition layout and NVS/filesystems; do not run full-chip erase.
4. Run `pio run -e esp32s3` and `git diff --check`; record toolchain/library
   versions, RAM/flash and application/OTA slot sizes. Enumerate serial devices
   (e.g. `pio device list`) and identify the radio before upload. Confirm its
   actual DAC/amplifier and wiring from available hardware evidence; ask the
   user only if it cannot be established.
5. Create `docs/spotify-validation/progress.md` with the evidence ledger below.
   Preserve a known-good build, exact restore/upload command and private recovery
   material as appropriate before the first experimental flash. Never commit
   a full flash/NVS backup; it can contain Wi-Fi and account credentials.
6. Implement S0 instrumentation and collect baseline before adding Spotify.
   If hardware/account access is unavailable, finish buildable instrumentation,
   candidate audits and reproducible scripts; leave affected gates NOT VERIFIED
   and identify the exact user action needed. Do not substitute simulated results.

## Candidate and build strategy

Start by auditing the research's pinned Waveshare S3 project (`9c51b08`) and
cspot fork (`37b650a`), including exact Bell/submodule revisions. Its IDF 6 example
does not establish compatibility with our pinned Arduino/pioarduino framework.
Review StreamCore32's relevant protocol fixes as a second native reference.
Pin all adopted revisions and record each compatibility change.

A standalone native build may establish current authentication/CDN/decoder
behavior first. It must use our board's output wiring. Success is provisional
until reproduced in the real Radiohead framework with existing services enabled.
Do not silently migrate the entire project to IDF 6 to make a demo build.

Resolve authentication early: verify Connect pairing, usable CDN authorization,
expiry/renewal and restart on this Premium account. Prefer native pairing/session
flows. If the candidate needs developer app credentials, establish private
runtime provisioning, storage/forget behavior and renewal before adopting it.
Do not assume a client-credentials token or PKCE works for undocumented CDN
endpoints just because token issuance succeeds. If the only working route needs
a compiled secret or runtime helper, report that precise blocker and investigate
another native flow; do not introduce a companion behind the user's back.

## Instrumentation: small, bounded, reproducible

Use a compile-time diagnostics option and a fixed-size record/event buffer.
Collect counters in hot paths; serialize at a low rate from a safe context.
Do not print per sample/frame or perform extra network work inside audio service.
Retain a short event window around failures. Record instrumentation overhead.

Capture at boot, Wi-Fi ready, idle, radio playing, podcast playing, Spotify
pairing/TLS, Spotify playing/paused, switching, reconnecting and after teardown:

| Signal | Required measurement |
| --- | --- |
| Memory | Free, minimum free and largest free block for internal 8-bit RAM, DMA-capable RAM and PSRAM separately; capabilities can overlap, so do not sum these totals. |
| Allocations | Failed allocation count; known buffer sizes, capability requirements and peak simultaneous allocations during connect/renew/reconnect. |
| Tasks | Task count and per-task stack high-water marks for owned tasks; document units from the installed ESP-IDF version before converting to bytes. |
| Audio | Source/state/generation, queue fill/minimum, underruns, bytes/frames produced and consumed, discontinuity/flush events, I2S init/release failures. |
| Responsiveness | Maximum and percentile audio-service/loop gaps, command acknowledgement and web response latency; CPU/task runtime if available without intrusive changes. |
| Lifecycle | Connect/stop/reconnect durations, retry counts, reset reason, watchdog/panic/heap-integrity failures; expected source versus actual output owner. |
| Build | ELF/map size, static RAM, firmware size and actual OTA slot headroom. Static reports do not prove runtime memory fit. |

Take heap-integrity checks at safe checkpoints, not continuously in audio loops.
Use a monotonic clock and bounded histograms; avoid unbounded collections. Never
record auth headers/blobs, tokens, Wi-Fi passwords or signed audio URLs.

## Packages, executed in order

### S0 — Existing radio baseline and recovery

Collect at least 30 minutes across radio and podcasts on stable Wi-Fi, including
web requests, normal display updates and repeated controls. Use a repeatable
playlist/station set and record workload versions and test conditions.

Deliver: baseline build/report, instrumented baseline, private recovery path,
hardware identity, state-by-state resource table and existing defects clearly
separated from Spotify regressions. A pre-existing failure is not a reason to
claim the same failure is acceptable in the final integrated build.

### S1 — Real native Spotify audio

Using the actual Premium account, demonstrate same-LAN discovery, pairing and
audible output on the intended speaker. Verify at least 20 track changes,
pause/resume, seek, remote volume, device transfer, three cold restarts and a
Wi-Fi interruption. Run at least two hours and observe actual authorization
renewal; elapsed time alone is insufficient evidence of renewal.

Record negotiated codec/bitrate/sample rate and test every mode the implementation
advertises. If only a specific lossy mode is supported, state that limit; do not
claim lossless. Playback, renewal and restart must work with development/helper
services stopped. Temporary serial diagnostics may remain connected for capture.

Pass: real audio, bounded errors/retries, no crash/watchdog/heap corruption,
working standalone authentication lifecycle, and saved evidence. Failure in this
stage blocks broader integration; document and fix the native cause first.

### S2 — Coexistence and deterministic source handoff

Add the smallest owned Spotify adapter and source coordinator under `media`.
Keep `main.cpp` orchestration-focused. Normal builds must retain a way to disable
the experiment and restore current radio behavior.

The global `Audio` object currently retains its I2S channel after `stopSong()`.
Audit its worker/static state and all direct callers before selecting a lifetime
or shared-output approach. Never call a destructor on the shared object and
continue using references to it. Do not drive the same pins from two live owners.
Vendor modifications remain subject to the repository's explicit vendor-patch
rule; prepare the concrete need if a safe application-level solution is absent.

Use a serialized transition: invalidate the old generation and cancel recovery;
request producer stop; await bounded acknowledgement; flush old PCM; release
output ownership; configure the new source; apply mute/volume/tone; start new
output. On timeout, enter a recoverable stopped/failed state. Never free buffers
or I2S while a worker still uses them. Define queue-full cancellation behavior.

Test these semantics explicitly:

| Trigger | Required result |
| --- | --- |
| Phone selects Radiohead and starts Spotify | Radio/podcast stops once; Spotify owns output; catalog selection remains intact. |
| Local radio or podcast selection | Spotify stops/releases output; requested local source starts; late Spotify events cannot reclaim it. |
| Spotify paused | Silence with session retained; radio recovery must not activate. |
| Playback transferred to another device | Radiohead stops Spotify; no automatic radio restart. |
| Rapid competing local/remote commands | Serialized order and generation checks; no stale callback, overlap or use-after-free. Log the chosen deterministic ordering. |
| Mute click / encoder turn while muted | Mute never pauses/restarts; stored volume changes while output remains muted. |
| Remote volume while muted | Preserve local mute by default; clamp/map level without feedback loops. |
| Network/session failure | Bounded retry/cancel; existing local selection remains usable; no endless blocking wait. |

Complete 100 integrated transitions, at least 10 in each directed pair among
radio/podcast/Spotify, plus 20 rapid or interrupted transitions. Warm each source
before measuring leak trends. Exercise non-flat custom tone, minimum/maximum
volume and mute across every direction. Check for pops, clipping, old-track
fragments and audible overlap using loopback capture if available, otherwise
record explicit listening observations. Serial counters alone do not prove sound.

### S3 — Resource and responsiveness acceptance

Repeat S2 with the existing TFT/web/weather workloads active, and during Spotify
TLS connection, renewal and network recovery. Use bounded stress representative
of actual web use, not unlimited concurrent requests.

Set and record resource budgets **before** declaring pass. The following are
initial engineering gates, not guarantees from Espressif or Spotify:

- Zero allocation failures, stack overflow/corruption, watchdog resets and I2S
  ownership errors throughout the accepted run.
- Every owned task retains at least 25% of its allocated stack and at least
  1 KiB unused at its measured worst point, with units verified. If an upstream
  task cannot be instrumented, record the gap instead of asserting coverage.
- For each relevant allocation capability, retain enough total free memory for
  the documented remaining worst-case burst plus 25%, and a largest free block
  at least 1.25 times the largest pending contiguous allocation. Build this budget
  from measured/labeled allocations; PSRAM cannot stand in for internal/DMA RAM.
  Add the eventual bounded Spotify UI/artwork reserve to a separate budget; do
  not implement UI to obtain that reserve.
- After warm-up and equal teardown/settling periods, compare ten-transition
  batch endpoints. Investigate cumulative loss exceeding 4 KiB internal RAM,
  32 KiB PSRAM or 10% of the largest internal block; any reproducible continuing
  decline fails even below those investigation thresholds. Account for bounded
  caches explicitly. A reboot is not a memory-recovery mechanism.
- Same-LAN control acknowledgement p95 <= 250 ms and simple status-page
  response p95 <= 500 ms during playback. Record p99/max and baseline differences.
  Target source selection to first audio p95 <= 5 s on healthy test services;
  separate external network delay from local handoff time rather than hiding it.
- No unexplained steady-state PCM underruns or audible dropouts under stable
  network conditions. Bound audio-service gaps against actual buffered audio
  duration and compare against baseline; an arbitrary loop-delay figure is not
  proof that decoding receives enough CPU.

If these provisional budgets prove inappropriate, justify a revised value from
measured allocations/timing **before rerunning** acceptance. Never lower a gate
after a failure simply to mark a package complete. Report all resource minima,
not just average free heap or the linker report.

### S4 — Integrated resilience and soak, still before UI

Run an eight-hour integrated soak with at least four hours of Spotify and the
remaining time covering radio, podcasts and source switching. Include real token
renewal, normal screen/web activity and the following fault/recovery cases:

- Ten Wi-Fi interruptions of varying duration, including recovery during
  playback and a source switch while Spotify reconnects.
- Five device transfers away and back, five cold boots, and five sleep/wake
  cycles using supported physical wake behavior. No automatic unmute/blast.
- Pause with a full producer queue, rapid seeks/skips, remote app closing,
  temporary DNS/CDN failure and expired/rejected session where safely reproducible.
- One supported OTA update/restart and return to all sources. Preserve existing
  updater quiescence behavior or implement the required coordinator shutdown;
  no new audio task may access resources being torn down. Do not deliberately
  interrupt flash writes as part of this task.
- Confirm station/podcast catalog, favorites, Wi-Fi, calibration and custom tone
  survive. Confirm `radio.local`, web configuration and existing control paths
  work with Spotify both active and disabled.

Pass only with no unexplained reset, leak trend, corruption, ownership conflict,
stuck transition or stable-network dropout. Record recoverable network failures
and their recovery times. An unattended log proves uptime/counters, not that
every minute sounded correct; distinguish captured audio from sampled listening.

Finish with `pio run -e esp32s3`, appropriate existing checks for changed areas,
`git diff --check`, dependency/secret/generated-artifact inspection and a repeat
of any invalidated tests after fixes. Preserve failing-run evidence and link the
successful rerun. Restore a known-good build if the test device is left unusable.

## Evidence and completion contract

The executing agent creates the progress ledger before implementing packages:

| Package | Code/build | Hardware/function | Resources/timing | Evidence / next action |
| --- | --- | --- | --- | --- |
| S0 | NOT VERIFIED | NOT VERIFIED | NOT VERIFIED | Collect baseline |
| S1 | NOT VERIFIED | NOT VERIFIED | NOT VERIFIED | Native playback/auth |
| S2 | NOT VERIFIED | NOT VERIFIED | NOT VERIFIED | Source ownership |
| S3 | NOT VERIFIED | NOT VERIFIED | NOT VERIFIED | Budget and stress |
| S4 | NOT VERIFIED | NOT VERIFIED | NOT VERIFIED | Soak and recovery |

Use PASS / FAIL / NOT VERIFIED. Every PASS links an exact commit/build, hardware
identity, dependency versions, test command/script, duration/counts, redacted
measurements and observed result. Keep small summaries/CSV in
`docs/spotify-validation/`; keep large captures, binaries and private logs outside
git. Automation should save durable evidence so a long soak can be resumed or
reviewed by another agent. A failed run does not count toward a clean soak.

The final report must answer: does Spotify really play; can we repeatedly switch
sources; what are worst-case internal/DMA/PSRAM and stack reserves; what failed
and recovered; what remains unverified; how to reproduce and roll back?

Only when S0–S4 have device evidence may Spotify UI work be planned as the next
implementation stage. If native viability is blocked, finish with the concrete
failure, attempted fixes and remaining native options. No Linux/RPi fallback.

## Copyable task for another agent

> Execute `docs/spotify-native-validation.md` in order through S0–S4. Read
> AGENTS.md and required skills, preserve the current radio, and implement only
> the native Spotify backend, diagnostics and tests needed to establish real
> playback, safe source switching, memory headroom and stability. No Linux,
> Raspberry Pi, runtime helper or Spotify UI. License review is not a gate.
> Start with current status/diffs, baseline build, device identification and a
> recoverable test setup. Continue autonomously within scope; request only
> genuinely missing hardware/account actions. Publish measured evidence and
> leave every unperformed hardware check NOT VERIFIED.
