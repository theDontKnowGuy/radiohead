# Spotify UI: post-validation implementation plan

Status: U0–U2 code implemented 2026-09-29 at the user's request; native visual,
normal/integrated build and isolated web polling checks pass. Physical-device
acceptance remains open alongside S3/S4 Spotify validation. The phone's Spotify
app remains the normal content browser and playback controller. See
[the native UI evidence](implementation-status.md) and
[the web ledger](web-configuration/progress.md).

## Existing contracts and the gap

- [Spotify integration research](../spotify-integration-research.md) already
  requires a radio/podcast/Spotify source model, transfer-away inactivity,
  retained volume/mute/tone, and a bounded artwork reserve. It does not specify
  Spotify screen behavior.
- [The TFT visual contract](visual-contract.md) governs the coastal sunset,
  navy/blue, artwork-led 320 × 240 presentation. [The configuration and controls
  plan](configuration-and-controls-plan.md) governs device-wide encoder
  volume/mute/hold and removes page-dependent encoder navigation and the TFT
  volume slider. These later instructions supersede older player sketches.
- The [web handoff](web-configuration/README.md) requires a real shared
  now-playing strip, but does not define Spotify. Current `/api/player` exposes
  local volume/tone plus one state and name; the current TFT render state exposes
  radio station playback separately from the podcast snapshot. The experimental
  Spotify adapter supplies output-owner signals and diagnostics, not a public
  track/artist/artwork or transport-control snapshot. Spotify can therefore play
  while the existing surfaces still describe local playback.

## Scope and user behavior

1. **Source truth:** Publish one bounded, read-only media snapshot to the TFT
   controller and web server: source (`none`, `radio`, `podcast`, `spotify`),
   playback state (`connecting`, `playing`, `paused`, `stopped`, `failed`),
   source identity, optional track title/artist/album, optional artwork identity,
   availability of each action, and a monotonically changing revision. Separate
   active output from the last selected station/episode. Publish the snapshot
   from `media` on the Arduino side after consuming adapter events; worker tasks
   must not draw, build HTTP responses, or edit shared `String` objects. Bound
   copied metadata lengths and handle missing or malformed fields as unavailable.
   Record exactly which metadata/control events the pinned cspot candidate
   actually provides before promising a field or action. Keep station catalog
   indexes and podcast browse state untouched by Spotify.
2. **TFT Home and player:** Keep the four existing Home destinations. When
   Spotify owns playback, show an honest Spotify source/status summary in the
   existing active-player area. A new Spotify source automatically opens its
   player page; tapping the Home summary reopens it after Back;
   do not route it to the live station player or add Spotify as a station or
   favorite. Use the shared header directly over the coastal background,
   translucent navy surfaces, blue
   active state, readable mixed-language text, and a prominent artwork region.
   Show available title/artist/album; otherwise show `Spotify` and a clear
   playing/paused/connecting/error state. Use a prepared local placeholder
   while artwork is unavailable. Do not show `LIVE`, radio station frequency,
   podcast progress, or a fabricated track title. Back returns Home without
   stopping playback. Source changes must update the screen promptly and clear
   stale Spotify metadata.
3. **TFT actions:** Preserve touch access to the four Home destinations and
   local source selections, which explicitly take audio ownership from Spotify.
   Apply the device-wide encoder turn/click/hold contract during Spotify just as
   during radio and podcasts. Show the same brief volume/mute feedback, without
   changing the 0–21 curve or treating mute as Spotify pause. The Spotify player
   may show pause/resume and previous/next only for commands supported by the
   adapter and verified against phone/device state. Otherwise show a quiet
   `Control playback in Spotify` hint instead of decorative working-looking
   buttons. Do not add on-device search, playlists, pairing credentials, or a
   Spotify catalog browser.
4. **Web shared player:** Extend the existing `/api/player` response additively
   with the authoritative source, state, metadata, and action availability;
   preserve current fields and the volume/mute/tone endpoints and bounds. Update
   the shared now-playing strip on every settings page when source or metadata
   changes, including when it changes in the phone app, without overwriting an
   in-progress volume/tone edit. Label Spotify and paused/connecting/transfer-
   away states accurately. Escape and bound metadata; support Hebrew/Latin
   wrapping. Keep the shared volume and tone controls only after confirming
   they affect Spotify output. A short local pairing/discovery hint may be
   shown if useful, but no credentials, token state, or debug diagnostics belong
   in the ordinary player UI.
5. **Transfer and failure:** Phone transfer to another device releases Spotify
   output and leaves the local player inactive; never display the old track as
   still playing or auto-start radio. A local radio/podcast selection replaces
   the Spotify player with that source's state, even if a late Spotify event
   arrives. Spotify pause retains the session and shows `Paused`; mute remains
   separate. On reconnect, metadata loss, failed acquisition, or adapter error,
   show the observed state and retain a usable route to local sources. When the
   feature is absent from a normal build, render the existing local UI with no
   empty Spotify destination.

## Artwork decision and budget

The first Spotify UI package uses a prepared local placeholder, so UI correctness
does not depend on album-art retrieval. If the pinned adapter exposes a trustworthy
image URL or bytes, add artwork only in a separate package after measuring S3/S4
headroom with the TFT/web/weather workloads. Fetch and decode off the audio path;
cap download, dimensions, decode memory, cache count, and time; cancel stale
requests on track/source change; avoid persistence unless a demonstrated need
justifies it. Reuse the existing prepared-asset/display pattern where applicable.
The player remains complete and legible if artwork is absent, late, corrupt, or
unavailable offline. Document the actual image source and licensing constraints
before enabling download.

## Delivery packages and acceptance

| Package | Owner and work | Acceptance evidence |
| --- | --- | --- |
| U0 — Contract | `media` and Spotify adapter: audit available events, publish bounded source/metadata/action snapshot with revision; expose a single read path. | Host checks for activate, pause, resume, transfer away, local override, missing/long/mixed-language metadata and late events. No stale source or unsafe shared-string access. |
| U1 — TFT | `ui_controller`, `display`, prepared assets: active Home summary, Spotify player, source-aware navigation and volume overlay. | Native 320 × 240 production-render fixtures for playing, paused, connecting, failed, missing metadata, long Hebrew/Latin titles and source switch; compare with the visual contract and record visual/functional/device evidence separately in `implementation-status.md`. |
| U2 — Web | `web_server` and existing CSS: additive player JSON and source-aware shared strip. | Browser checks for polling, editing conflict, escaping, transfer away, absence in normal build, and station/podcast regressions; device HTTP checks during playback. Update `web-configuration/progress.md` before and after the package. |
| U3 — Supported controls | Adapter, `media`, TFT/web only if verified commands exist. | Phone and TFT/web actions agree; unsupported actions are absent; volume/mute/tone retain their existing contract. |
| U4 — Optional artwork | Bounded fetch/cache and display path, only after resource gate. | Missing/slow/corrupt image cases, rapid track changes, memory/latency measurements, visual TFT evidence and uninterrupted audio. |

Do not mark a package complete from a build or mockup alone. For source changes,
run `pio run -e esp32s3`, the integrated Spotify build, `git diff --check`, and
inspect RAM/flash reports. On the physical radio, test phone play/pause/transfer,
Spotify ↔ radio/podcast selection, encoder volume/mute, TFT/web updates, Wi-Fi
loss/recovery, and responsiveness with the USB serial reader closed. Keep the
S3/S4 resource and soak thresholds authoritative; measure any added UI/artwork
cost separately. Record rollback to the prior normal image and any Spotify
image with no UI changes.

## 2026-09-29 implementation note

The pinned cspot candidate emits `ACTIVATE`, `PLAY_PAUSE`, `TRACK_INFO`, `DISC`
and `VOLUME` events. The adapter now forwards bounded, UTF-8 checked
title/artist/album fields and both play/pause values to the Arduino-loop media
snapshot. No track image bytes or verified phone/device transport command result
is available in this package. Spotify action availability is therefore false,
and the TFT/web show a local vector placeholder and a phone-control hint. The
vector placeholder matches the mockup's artwork area without storing or
decoding a bitmap during playback. Album artwork fetching remains U4.

On the 2026-09-29 follow-up, the Spotify player opens on the source transition.
Back keeps the active session on Home. Its shared header has no opaque bar; all
header controls use the same centerline over the photo. The three navy cards
blend over the restored photo in the PSRAM canvas, with an opaque fallback if
the readable canvas is unavailable.
