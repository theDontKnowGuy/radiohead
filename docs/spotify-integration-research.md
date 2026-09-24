# Spotify Premium: feasibility, alternatives and project plan

Research date: 2026-09-24. Repository inspected at `b47961c`.
Status: research background; no firmware changes or device tests.

**User decision, 2026-09-24:** native ESP32-only playback; no Linux, Raspberry Pi
or runtime companion. Prove memory headroom, source transitions and stability
before Spotify UI work. License/commercialization review is not a feasibility
gate for this personal project. The authoritative execution plan is
[Native Spotify validation handoff](spotify-native-validation.md); it supersedes
the recommendations and sequencing in the original investigation.

## Recommendation

Treat the desired experience as **select Radiohead in the Spotify phone app and
hear music from this radio**, while retaining internet radio, recorded shows,
the existing screen and physical controls. Premium is assumed, as requested.

First run a bounded native ESP32-S3 feasibility experiment using a recent cspot
fork. Do not integrate it into the main firmware until both current Spotify
authentication/playback and audio-output ownership have been demonstrated.
Native playback best fits the existing single-board product, but is an
experimental, unofficial integration with ongoing protocol maintenance risk.

If that experiment fails, document the exact native blocker and try a bounded
alternative native fix or candidate. Linux/Soloist and Raspberry Pi are excluded
by the user. Do not replace this firmware with Squeezelite or build a full
Spotify catalog browser. A desktop may build, flash and collect test evidence;
it must not be required for normal playback or token renewal.

## What this radio actually has

Source inspection, rather than hardware measurement, established:

| Area | Current implementation | Implication |
| --- | --- | --- |
| Processor/storage | ESP32-S3 N16R8; 16 MB flash, 8 MB OPI PSRAM; Arduino under pinned pioarduino PlatformIO platform | A plausible native target, but free internal RAM and CPU headroom remain unmeasured. |
| Audio | Global `Audio audio`; vendored ESP32-audioI2S; I2S BCK 15, DOUT 16, WS 17 | Preserve these pins. The physical DAC/amplifier model must be confirmed; do not copy another board's codec setup. |
| Playback | `media.cpp` owns radio/podcast playback and recovery; `main.cpp` services audio and HTTP | Introduce source ownership in `media`, with Spotify behind a separate adapter. |
| Output lifetime | `Audio::stopSong()` stops the stream/decoder but does not delete its I2S channel; the destructor does | Calling stop and starting another I2S driver is not a sufficient switching design. |
| PCM interface | Inspected public `Audio` API has URL/file playback, but no general public PCM input | cspot PCM cannot simply be passed to `connecttohost()`. Output integration is a first-class work package. |
| Controls | Volume index 0–21 through `volCurve`; custom bass/mid/treble; shared mute | A second decoder does not automatically inherit existing volume or tone processing. |
| Display | Native 320 × 240 ILI9341; a full RGB565 canvas allocated in PSRAM | Preserve artwork, Hebrew/mixed text, coastal background and responsive drawing. |
| Discovery | Existing `radio.local` mDNS and HTTP service | Add Spotify discovery without replacing the hostname or breaking configuration/OTA. |

Relevant sources: [platformio.ini](../platformio.ini),
[app_state.h](../include/app_state.h), [media.h](../include/media.h),
[media.cpp](../src/media.cpp), [main.cpp](../src/main.cpp),
[display.cpp](../src/display.cpp), [web_server.cpp](../src/web_server.cpp),
[Audio.h](../lib/ESP32-audioI2S-master/src/Audio.h) and
[Audio.cpp](../lib/ESP32-audioI2S-master/src/Audio.cpp).

The configuration/controls specification supersedes stale alarm and encoder
behavior still present in parts of the source. Spotify must follow the intended
volume/mute contract and must not preserve or reintroduce retired features.

## Have other people done it?

Yes. These are playback implementations, not just album-art displays.
Their authors' reports establish useful precedents; none was reproduced on
Radiohead during this investigation.

| Project | Evidence and lesson | Suitability here |
| --- | --- | --- |
| [cspot](https://github.com/feelfreelinux/cspot) | C++ Spotify Connect library targeting ESP32 and Premium accounts. Separates Spotify protocol/decoding from a PCM audio sink. | Architectural foundation; do not assume upstream master works today. |
| [Waveshare ESP32-S3 cspot project](https://github.com/VitaliBorys/cspot-waveshare-esp32s3) | Reports Connect playback on an S3 with 8 MB PSRAM/16 MB flash, ES8311 codec and ESP-IDF 6.0. Uses modified cspot/Bell forks. | Closest hardware precedent; candidate for the first standalone experiment, not a drop-in Arduino dependency. |
| [Squeezelite-ESP32](https://github.com/sle118/squeezelite-esp32) | Integrates cspot with a larger audio system. Documents starting/stopping Spotify services around Zeroconf connections to conserve memory. | Learn source arbitration and lifecycle management. Replacing Radiohead would require recreating its UI, settings and behavior. |
| [StreamCore32](https://github.com/tobiasguyer/StreamCore32) | cspot-derived Spotify/Qobuz/internet-radio implementation targeting ESP32 plus VS1053; documents IDF 5.5+ and Spotify app credentials. | Useful protocol/build reference, particularly nearer the current toolchain family; its external decoder architecture differs from ours. |
| [librespot](https://github.com/librespot-org/librespot) | Open-source Premium Connect receiver/library, with Linux audio backends. | Linux companion option and protocol reference; not an Arduino ESP32 library to add directly. |

Default-branch snapshots from the GitHub API, checked on the research date:

| Repository | Latest inspected commit | Commit date |
| --- | --- | --- |
| feelfreelinux/cspot | `1b07a9c` | 2024-07-12 |
| VitaliBorys/cspot-waveshare-esp32s3 | `9c51b08` | 2026-05-30 |
| VitaliBorys/cspot | `37b650a` | 2026-05-30 |
| sle118/squeezelite-esp32 (`master-v4.3`) | `1d542bd` | 2026-07-30 |
| tobiasguyer/StreamCore32 | `62075a3` | 2026-02-09 |
| librespot-org/librespot (`dev`) | `939dc5e` | 2026-09-11 |

Commit recency is maintenance evidence, not a playback test. In particular,
cspot's March 2026 [build/status report](https://github.com/feelfreelinux/cspot/issues/181)
and [S3 CDN-resolution failure](https://github.com/feelfreelinux/cspot/issues/180)
make an early account-to-speaker test essential. Squeezelite also has a
[connection/playback failure report](https://github.com/sle118/squeezelite-esp32/issues/465).
These reports do not prove every release or fork is broken.

### Lessons from the closest S3 example

Its [flow notes](https://github.com/VitaliBorys/cspot-waveshare-esp32s3/blob/main/spotify_connect_flow.md)
describe fixes for CDN authorization, TLS/crypto compatibility, reconnects and
device takeover. These are version-specific findings, not reasons to copy its
TLS or crypto changes into our different framework blindly.

Its [player source](https://github.com/VitaliBorys/cspot-waveshare-esp32s3/blob/main/main/main.cpp)
uses a 128 KiB PCM ring, separate player task, and explicitly internal task
stacks. That suggests measuring internal RAM separately from PSRAM. It also
reinitializes mDNS and contains a producer wait loop without an evident local
cancellation bound; reuse the architecture only after auditing shutdown,
backpressure and ownership.

Authentication needs engineering work; license provenance is retained as background:

- The example compiles a Spotify client secret into a credentials header. This
  conflicts with our credential-handling requirements. A gitignored header
  still embeds the secret in the binary. Establish a working authentication
  design without a compiled client secret and without a runtime helper.
  Investigate native provisioning and session renewal. Do not assume PKCE can
  replace its internal CDN-token flow without testing.
- Its README says cspot is MIT, but both the
  [upstream license](https://github.com/feelfreelinux/cspot/blob/master/LICENSE.md)
  and [fork license](https://github.com/VitaliBorys/cspot/blob/master/LICENSE.md)
  say GPLv3-or-later. Preserve existing notices. The user has explicitly removed
  license/commercialization review as a gate for this personal experiment.

## Alternatives compared

Historical comparison: the user subsequently selected native ESP32 only.
Excluded alternatives below are retained as research, not fallback instructions.
Effort/risk below is an engineering assessment for this repository.

| Approach | Sound through this radio? | Extra requirements | Assessment |
| --- | --- | --- | --- |
| Native cspot-derived Connect receiver | Yes, if integrated with its I2S output | Protocol fork, auth solution, buffer/task budget, audio lifecycle work | Best fit for one board; highest Spotify compatibility uncertainty. Start with a short feasibility gate. |
| Replace firmware with Squeezelite-ESP32 | Yes, on a compatible tested configuration | Rebuild/adapt UI and controls, migrate settings, verify S3 target | Good bench comparison; poor incremental fit for preserving Radiohead. |
| Official Spotify Soloist on Linux | Yes, with a designed physical audio path | Pi/other supported Linux host, DAC/output selection, control bridge | Preferred official companion route. More hardware and system administration. |
| librespot on Linux | Yes, with a designed audio path | Same companion integration plus community protocol maintenance | Useful alternative if Soloist is unsuitable; unofficial. |
| Spotify Web API controller | No, by itself; audio stays on another Spotify device | OAuth/app access and a separate player | Suitable only if the requirement changes to a remote control. |
| Web Playback SDK in the radio's web page | Audio plays on the browser's host | Supported browser, Premium and authorization | A web page served by ESP32 does not make ESP32 the player. |
| External Bluetooth audio receiver | Yes, after audio hardware integration | Bluetooth receiver and input switching; phone remains audio source | Practical non-Connect alternative. S3 cannot supply Classic Bluetooth A2DP itself. |
| Official embedded Spotify SDK | Potentially, subject to platform support | Organization partner approval, agreements and certification | Commercial-product route, not an ordinary individual SDK download. |

The official [Web API playback endpoint](https://developer.spotify.com/documentation/web-api/reference/start-a-users-playback)
controls an existing device; it does not return a full-track audio stream.
The [Web Playback SDK](https://developer.spotify.com/documentation/web-playback-sdk)
creates the player in the browser. Ordinary Spotify track/playlist links cannot
be used as station MP3 URLs. Bluetooth feasibility is constrained by the
[ESP32-S3's BLE-only radio](https://documentation.espressif.com/esp32_s3_datasheet_en.pdf).
Spotify's [hardware program](https://developer.spotify.com/documentation/commercial-hardware)
currently accepts organizations rather than individuals; Premium alone does
not provide the embedded SDK.

### Official Linux companion: excluded by user decision

[Spotify Soloist](https://developer.spotify.com/documentation/soloist) is a
headless Linux Spotify player with local output and optional control APIs.
Its official builds cover ARM32, ARM64 and x86-64 Linux, not the ESP32-S3.
Its overview says builds expire after 90 days, so updates are a product
requirement, not optional maintenance.

The [authentication documentation](https://developer.spotify.com/documentation/soloist/concepts/authentication)
requires a personal Soloist API key from a Premium account, then pairing by
selecting the device in Spotify. Keep the key and session on the Linux host.
The [CLI documentation](https://developer.spotify.com/documentation/soloist/reference/command-line)
specifies PipeWire/PulseAudio output. Its
[WebSocket API surface](https://developer.spotify.com/documentation/soloist/features)
has no built-in client authentication or TLS: keep it on loopback and put an
authenticated, bounded bridge between the radio and that local service.

Proposed companion architecture: ESP32 controls/display → authenticated LAN
bridge → Soloist; Linux audio → DAC → selected amplifier input → speaker.
Confirm the existing amplifier's inputs first. If it only accepts I2S, a
separate DAC/analog input selector may be required. Never connect two I2S
masters directly to the same output wires. This is a hardware design option,
not an already available connection on Radiohead. Streaming audio back over
LAN would be a separate latency/buffering/transport project and is not assumed.

### Web API constraints if browsing or a remote is added later

Use user authorization with PKCE where applicable, narrow playback scopes and
bounded polling/backoff. Do not copy old tutorials using password login or an
HTTP LAN redirect: [current redirect rules](https://developer.spotify.com/documentation/web-api/concepts/redirect_uri)
require HTTPS except explicit loopback IP addresses. `http://radio.local`
does not qualify for that exception.

Developer access changed during 2026. The
[February guide](https://developer.spotify.com/documentation/web-api/tutorials/february-2026-migration-guide)
describes Premium ownership, a five-user development cap and reduced catalog
capabilities; its one-client-ID statement was superseded by the
[July update](https://developer.spotify.com/blog/2026-07-23-web-api-quota-updates),
which allows 25 client IDs with a shared per-developer quota. Test the actual
account/app and required endpoints before committing to catalog features.
These public Web API rules are not proof that an unofficial Connect/CDN flow works.

## Proposed native implementation

Phone Spotify app ⇄ Spotify adapter → bounded PCM queue → exclusive audio output.
The media coordinator owns source transitions and publishes display snapshots.

- Add `PlaybackSource` (radio / podcast / Spotify) and explicit source states.
  Spotify is not a station catalog slot and must not consume one of the ten slots.
- Keep protocol/session/task implementation in a new Spotify adapter owned by
  `media`; keep startup/ticking orchestration in `main`. Worker callbacks publish
  bounded events, never draw or mutate Preferences directly.
- Cancel radio recovery and invalidate stale callbacks when Spotify takes over.
  Otherwise `mediaTick()` can restart a station while Spotify is playing.
- Establish one I2S owner. First investigate safe application-level lifetime
  management of the existing `Audio` object versus a compatible shared sink.
  Audit all direct `audio` callers before changing its lifetime. Do not rely on
  destructor/recreation safety until tested, and do not edit the vendor library
  as an incidental shortcut. A required vendor change is a separate documented
  decision outside the default implementation path.
- Reuse existing mDNS; isolate Spotify HTTP endpoints from web configuration.
  Bound request bodies, strings, metadata, image downloads and task queues.
- Preserve local volume 0–21 and mute semantics: a click mutes without pausing;
  turning while muted changes the remembered level. Clamp and map remote volume
  deterministically and prevent echo loops. Decide and test whether a remote
  volume command preserves mute; default to preserving it until explicit unmute.
- Preserve custom tone values. Either apply equivalent processing to Spotify
  PCM in an owned output layer or explicitly identify the missing capability;
  never show working EQ controls that have no effect on the active source.
- Allocate large audio/image buffers in PSRAM where supported, retain required
  DMA/internal allocations, and measure largest free internal blocks and task
  stack headroom. Avoid a second full-time decoder when Spotify is inactive.
- On local radio/podcast selection, release Spotify ownership and stop its audio.
  On transfer to another Spotify device, stop local Spotify output and show an
  inactive state; do not unexpectedly resume radio. Sleep/OTA must quiesce all
  producers before releasing output or flash resources.

## Execution and sequencing

Follow [Native Spotify validation handoff](spotify-native-validation.md).
Its S0–S4 packages cover baseline, native playback, source coexistence, memory
and resilience. All must pass on the actual device before a separate Spotify UI
package begins. The prior ordering that placed UI before the reliability soak
is superseded. Existing UI remains enabled as realistic load during testing.

## Verification and remaining unknowns

Completed: repository/source inspection, live primary-source research, candidate
commit checks and this plan. No Spotify credentials were used; no service was
authenticated; no audio playback, memory measurement, flashing or hardware
acceptance was performed. No claim is made that a candidate works today on this
particular radio. Firmware build was not run because this change is documentation only.

Implementation must run `pio run -e esp32s3`, inspect RAM/flash and actual OTA
partition fit, run `git diff --check`, and perform the package's device tests.
The full 16 MB chip size is not the OTA application-slot budget.

Unresolved inputs: exact DAC/amplifier model, actual Premium pairing/developer
access needed by the selected native flow, runtime authentication without a
compiled secret or companion, measured internal RAM headroom and safe output
ownership. Native-only is settled; do not ask again about Linux or Raspberry Pi.

TypeSafe guidance was used to separate design judgments from deterministic
evidence. A narrow requirement-coverage judgment could assist a later review,
but it adds little to these protocol/build/hardware gates. No live Jev judgment,
paid call or AI runtime integration was used or proposed for playback.
