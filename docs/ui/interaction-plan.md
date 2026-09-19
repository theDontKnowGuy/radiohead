# Radiohead touch UI proposal

Status: design proposal, not implemented firmware. September 2026.

This is the earlier paper/olive direction. For the later sixteen-screen
`uiconcept.png` reference, use [the agent implementation guide](agent-implementation-guide.md).
Its visual and screen requirements supersede conflicting choices in this proposal.

## Direction

A small, quiet listening instrument. Let the station name, the current programme,
and the physical knob carry the experience. Warm paper or charcoal surfaces,
soft white text, a muted olive accent, generous spacing, and simple rules.
No decorative gradients, bouncing spectrum, simulated chrome, or dashboard tiles
in the default view. Existing visualizers can remain an optional listening view.

The interactive companion is `radio-ui.html`. It uses illustrative station and
programme content and simulates actions locally; it does not control the radio.
It demonstrates listening, station/show/episode lists, alarm editing, quiet view,
and standby confirmation. Network errors, dimming, OTA, sound, brightness, and
sleep-timer editors are specified here for implementation rather than simulated.

## Confirmed constraints and current behavior

- User confirmed the existing 2.8-inch, 240 × 320 TFT. Firmware rotates it to
  **320 × 240 landscape** (`src/main.cpp`, `setRotation(1)`).
- ILI9341 display and XPT2046 touch are configured in `src/app_state.cpp` on a
  shared SPI bus. Touch calibration is already persisted. Normal touch actions
  are not implemented; touch reporting currently sits behind a debug flag.
- Encoder rotation currently changes volume. Holding the encoder while turning
  previews stations, and releasing tunes the previewed station.
- Volume is an integer from 0 through 21, mapped through `volCurve`. Keep this
  mapping; display the actual setting rather than a misleading percentage.
- The code has a separate K0 input for sleep/reset and external deep-sleep wake.
  Do not assume that encoder push or touch can wake deep sleep. Confirm the
  physical wiring before promising a single-knob power experience.
- Ten station slots, ten podcast shows, and up to eight fetched episodes are
  represented today. Blank station slots should be omitted from listening lists.
- Existing TFT fonts do not reliably display Hebrew; the code uses transliterated
  show names. Native Hebrew/RTL is a font/layout milestone, not an implicit promise.
- Firmware configuration declares 8 MB PSRAM; usable memory must be measured.
  The architecture reference has stale backlight information: `app_state.h`
  currently declares GPIO 3. This proposal changes no pins.

## Visual system

| Element | Proposed rule |
| --- | --- |
| Day palette | Paper `#EEEADF`, ink `#242923`, secondary `#586054`, accent `#526442` |
| Evening palette | Charcoal `#191D1A`, text `#EEEADF`, secondary `#B0B7A7`, accent `#BDD09B` |
| Type | One sans-serif family; station 26–28 px, rows 17–18 px, metadata 14 px, status 12 px |
| Grid | 16 px content inset; 4 px spacing unit; 44 px minimum primary touch target |
| Main screen | 32 px status, 148 px listening content, 60 px action strip |
| List screen | 44 px title, three 44 px rows, 64 px footer with 44 px actions |
| Selected item | Thin inset focus outline plus a position marker; separate from the playing marker |
| Motion | Immediate feedback, optional brief transition; no continuous motion by default |
| Text overflow | Two lines for main station/title; ellipsis at valid character boundaries; detail view for full text |

Use measured font bounds, not character counts. Verify contrast after conversion
to RGB565 and check actual legibility at arm's length. A 44 px target is an initial
layout choice, to be tested with fingers on this panel, including the edges.
Avoid thin hairline fonts and dependence on subtle gray differences.

## Screen map

```text
Listening ── station name / Stations ── Station list ── select ── Listening
    │
    ├── knob press / Menu ── Browse
    │                          ├── Stations
    │                          ├── Podcasts ── Shows ── Episodes ── Listening
    │                          └── Settings
    │                               ├── Alarm
    │                               ├── Display (brightness, quiet view, appearance)
    │                               ├── Sleep timer
    │                               ├── Sound (bass, mid, treble)
    │                               ├── Network / phone setup / device info
    │                               └── Standby
    ├── volume turn ── temporary volume readout
    └── hold ── Standby confirmation
```

### Listening

Small time at upper left, next alarm at upper right when enabled. The content area
has a quiet source label, prominent station name, and up to two metadata lines.
The footer has **Stations**, **Mute**, **Menu**. Tap the station name to browse.
Mute preserves the set volume; turning changes volume and unmutes. Show `Muted`
explicitly. Turning reveals `Volume 8 / 21` and a restrained horizontal bar for
1.5 seconds without obscuring the station name or covering controls.

For podcasts, use show and episode title, and replace Mute with a labeled playback
action only after pause/resume is verified against the installed audio dependency.
Initially support truthful Play/Stop behavior; do not imply live radio has a
time-shift buffer. Seek controls and resume positions are a separate capability
milestone, with disabled/absent controls until supported.

### Browsing

Three rows per page. Keep the current audio running while moving the focus.
Use a small `Playing` marker independently of the focused row. Only an explicit
tap or push selects playback. Turn the encoder through all items; the viewport
follows. Do not wrap at list boundaries. Back, Previous page, and Next page are
visible touch actions. A hold provides encoder Back. Remember focus and scroll
position per list, but never persist them as the selected station.

The listening knob opens Browse with Stations initially focused; one more push
opens the station list. This costs one extra push but makes Podcasts and Settings
equally discoverable with just the knob. Touch Stations is the faster direct path.

No keyboard on the tiny display. Adding stations, editing URLs, Wi-Fi credentials,
API keys, firmware upload, and complex management stay in the existing phone/web UI.
The radio shows concise connection instructions and its address only in setup/info.

### Settings and numeric editing

Use the same three-row list and Back behavior. Press a numeric row to edit;
outline its value and show `Turn to adjust · Press to save`. Rotation now changes
that value, push commits, and hold cancels the uncommitted draft. Touch exposes
large − / + and Save / Cancel controls; dragging is never required.

Alarm: hour, minute, enabled, then explicit Save. Show a summary before leaving.
Brightness applies a preview and reverts on Cancel. Coalesce persistence writes;
do not save every encoder tick. Existing keys and alarm semantics stay intact
until intentional behavior changes are implemented and validated.

### Quiet view, dimming, and standby

Quiet view is an optional clock-led layout while the device remains awake. It is
not deep sleep and does not stop audio. Retain a small station label and next alarm.
After the existing 30-second inactivity period, dim the backlight. A touch or push
wakes the display and is consumed, avoiding a hidden action. An encoder turn
wakes and adjusts volume; discard remaining transitions from a consumed press.
Do not auto-close an unfinished editor or save its draft on timeout.

Standby requires a visible confirmation with Cancel initially selected. It stops
audio and enters the existing deep-sleep flow. The screen goes dark. The current
firmware wakes from K0 or an alarm timer; touch/encoder wake is a hardware question.
Do not draw an always-on standby clock while claiming the device is asleep.

### Alarm and exceptional states

- Alarm takes visual priority. Initial release must preserve the current dismissal
  semantics: a labeled `Dismiss` ends the alarm state and returns to normal listening.
  Do not promise snooze or stopped audio without implementing those changes.
  Press dismisses and is consumed; rotation adjusts active alarm volume within bounds.
- Reconnecting: preserve the station name, replace metadata with `Reconnecting…`,
  and keep controls available. Show Playing only from actual playback state.
- Failure: `Station unavailable` with Retry and Stations actions; retain a path
  back to the last working selection. A pending selection is not yet confirmed
  playing; ignore late results from superseded requests.
- Missing metadata: show `Live radio`, never an old song title from another station.
- Empty list: explain how to add stations on the phone; provide Back and Setup.
- Podcast fetch: show Loading, allow Back, ignore stale results after leaving.
- Clock not synchronized: show `--:--`; alarm editor shows `Waiting for time`.
- Wi-Fi setup: legible AP name/address, no credentials. Calibration remains a
  deliberate maintenance flow with its existing boot shortcut preserved.
- OTA: dedicated progress/error state, prevent competing UI commands, preserve
  explicit success/failure. OTA work remains in `web_server`.

## Input contract

| Context | Turn | Short push | Hold (~700 ms, tune on hardware) |
| --- | --- | --- | --- |
| Listening | Volume | Open Browse | Open Standby confirmation |
| List | Move focus / scroll | Activate focused item | Back |
| Numeric edit | Change draft | Save field | Cancel field |
| Confirmation | Move between choices | Confirm focused choice | Cancel |
| Quiet view | Volume and return to listening | Return to listening | Open Standby confirmation |
| Dimmed display | Wake + volume | Wake only | Wake only; consume until release |
| Alarm | Alarm volume | Dismiss | Same consumed alarm press; no navigation |

Generate exactly one semantic action per press. A held press must not emit a
short-push on release. Tap activates on release inside its original target;
movement beyond a calibrated tolerance cancels the tap. No essential double taps,
swipes, hold-and-turn chords, or multi-touch gestures. Preserve the K0 maintenance
behavior during initial migration; do not silently remap its factory-reset gesture.

## Firmware architecture

Use the existing LovyanGFX stack for the first release. These screens need a small
set of primitives and a deterministic state machine; adding a GUI framework now
would add migration scope before validating the interaction. Reconsider only if
complex text, accessibility, or richer layouts warrant a measured prototype.

```text
encoder sampling ─┐
touch polling ────┼─> device_control: debounced InputEvent
button timing ───┘                     │
                                       v
media / network events ──> ui_controller: state transitions + commands
web commands ──────────────────────────┤
                                       ├─> media / settings / power services
                                       └─> display: snapshot + dirty regions ─> TFT
```

| Module | Responsibility |
| --- | --- |
| `app_state` | Shared hardware, catalog/domain state, shared event and snapshot types |
| `device_control` | Encoder sampling, touch/button normalization, power execution |
| new `ui_controller` | Screen stack, focus, edit drafts, input routing, command generation; private UI state |
| `display` | Layout, hit rectangles, fonts, palettes, render snapshots, region invalidation |
| `media` | Actual playback state, stream requests, bounded metadata and podcast results |
| `settings` | Persistent domain settings, validation, coalesced saves |
| `web_server` | Existing routes/OTA; route domain changes through shared command boundaries |
| `main` | Startup and cooperative service loop only |

Separate `UiState` (page, focus, list offset, editing draft, modal, last input)
from `PlaybackState` (source, requested ID, playing ID, status, title, volume,
muted) and `PowerState` (awake/dimmed/standby transition). Use typed enums, bounded
strings, IDs validated before lookup, and a bounded navigation stack. Do not encode
page, playback status, and power mode in one growing collection of booleans.

Render from a stable snapshot. Display code owns both layout rectangles and the
touch-target map; controller receives target IDs rather than duplicating coordinates.
Web updates must refresh the same state and invalidate matching regions. If the
web changes a field under an active editor, use a revision check: show the new value
and require a fresh edit instead of silently overwriting it from a stale draft.

The UI loop owns the TFT and touch SPI access. The encoder task publishes deltas
through an atomic exchange/critical section or fixed queue; merely using volatile
does not make read-reset operations safe. Coalesce rotation deltas and retain
button transitions; define queue-full behavior. Network workers publish immutable,
size-limited results tagged with request IDs. They never draw or call the audio
object concurrently. Validate the audio dependency's thread-affinity expectations
before changing playback scheduling.

Move blocking podcast/weather/playlist fetches behind bounded requests with
timeouts and cancellation/late-result handling. Route selection commands through
the existing media owner. Do not assume putting `connecttohost()` in a queue makes
it nonblocking: measure the installed implementation and plan around its actual
behavior while retaining one owner for audio operations.

Run `audio.loop()` and `server.handleClient()` frequently. Poll touch on a budget,
consume a bounded batch of input events, tick the controller, then render only
dirty rectangles within a measured time budget. Never run a network request from
a paint or touch callback. Keep shared SPI transactions short and serialized.

Memory reference: one 320 × 240 RGB565 buffer is 153,600 bytes (150 KiB); a
320 × 44 row buffer is 28,160 bytes (27.5 KiB). Start with reusable region buffers
and direct static drawing. Do not add double buffering without measuring heap,
PSRAM allocation/DMA compatibility, SPI transfer cost, and audio under load.

Core interaction, time arithmetic, bounds, rendering, and side effects are
deterministic C++. TypeSafe was considered for reviewing ambiguous aesthetic
requirements; it adds no needed runtime capability here. No Jev call or SDK is
part of this proposal. Live documentation could not be fetched in this session;
only the local skill's separation-of-judgment guidance was used.

## Implementation sequence and acceptance

1. **Validate this prototype on the 2.8-inch panel.** Confirm readable text,
   edge targets, contrast, tap calibration, turn direction, and button timing.
2. **Input and state foundation.** Introduce typed events/controller and move
   current rendering out of `main`, preserving audio, web, persistence and pins.
   Verify debouncing, hold suppression, bounds, and wake input consumption.
3. **Listening + station selection.** Shared playback commands, confirmed status,
   mute, volume readout, focus versus playing, and connection/error handling.
4. **Podcasts + settings.** Async request lifecycle, three-row lists, explicit edit
   drafts, alarm/display/sound settings; implement only verified media controls.
5. **Quiet/alarm/power/OTA integration.** Validate event priority, actual wake
   wiring, timer behavior, alarm dismissal, and recovery from interrupted operations.
6. **Finish and measure.** RGB565 assets, text overflow, optional visualizer,
   long listening sessions with web traffic and touch activity, memory and latency.

At each code milestone run `pio run -e esp32s3` and `git diff --check`; inspect
RAM/flash output and test on the device. Target visible input feedback within
100 ms, but treat that as a measurement target, not a verified guarantee.

Required scenarios: browse without retuning; cancel without persistence; hold
without accidental selection; invalid and empty indexes; long/missing/Hebrew
metadata; dim wake without hidden activation; alarm during an editor; web volume
change during touch use; slow/offline network; repeated station changes; stale
podcast response; touch while audio is playing; deep-sleep K0/alarm wake; OTA
failure/success. No firmware build or hardware validation is claimed for this
design-only change.

Prototype verification: JavaScript syntax checked with Node; browser rendering
visually inspected for listening and station-list layouts. Exercised encoder
browsing without changing the Playing marker, explicit station selection, and
knob-only alarm editing/saving through to the updated listening header. Physical
target sizes, latency, font rasterization, and hardware behavior remain unverified.
