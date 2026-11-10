# Spotify on the TFT and web

The phone/computer Spotify app remains the music browser. No on-device catalog,
playlist editor or credential entry. Spotify is in the normal build; UI work began
September 29 at the owner's request while [resource/soak checks](../spotify-validation/progress.md)
remained open. Keep the [sunset/navy design](visual-contract.md) and knob volume/mute/hold.

## Behavior to preserve

- `media` publishes one bounded source/status/metadata/action snapshot with revision
  after consuming adapter events on the Arduino loop. No worker draws or mutates
  shared Strings. Active source is separate from last-selected station/episode.
- New Spotify ownership opens its player. Back returns Home without stopping;
  Home's Spotify summary reopens it. Keep four Home destinations; Spotify is not
  a station/favorite slot. A radio selection still returns Home per the local UI contract.
- Show real playing/paused/connecting/stopped/failed, bounded title/artist/album or
  honest fallback. No radio LIVE/frequency, podcast progress or fabricated track.
  Source change clears stale metadata; late Spotify events cannot undo local override.
- Mute stays distinct from pause and preserves stream/selected level. Turn while
  muted keeps mute; remote volume must not clear it. Volume 0–21 and custom tone
  stay shared with radio/podcasts and web.
- Transfer away releases Spotify and leaves local playback stopped, never auto-radio.
  Network/session failure retains usable local selection/recovery.
- `/api/player` extends existing fields additively; all web settings pages update
  the source-aware strip without overwriting volume/tone drafts. Escape metadata,
  preserve mixed Hebrew/Latin, and keep credentials/token diagnostics out of ordinary UI.

## Implemented versus future

U0 source/metadata, U1 Home/player and U2 web strip were implemented September 29;
native fixtures/build and isolated polling checks passed. Pinned events include
ACTIVATE, PLAY_PAUSE, TRACK_INFO, DISC and VOLUME. September 30 added touch previous/
play-pause/next, queued to the connection task; connecting/failed states have no
active transport. Broader phone/TFT transport agreement still needs device testing.

The player uses a local vector placeholder; **album-art download is future U4**.
Only add it after measured resource headroom, with byte/dimension/decode/time/cache
limits, stale cancellation and off-audio-path work. Missing/corrupt/late images
must leave a usable player; no persistence without a demonstrated need.

## Evidence and next checks

Production 320×240 [playing](evidence/2026-09-29-spotify-ui/spotify-playing.png),
[paused](evidence/2026-09-29-spotify-ui/spotify-paused.png),
[connecting](evidence/2026-09-29-spotify-ui/spotify-connecting.png),
[failed](evidence/2026-09-29-spotify-ui/spotify-failed.png) and
[mixed title](evidence/2026-09-29-spotify-ui/spotify-mixed-long.png) were inspected.
They are native renders, not physical-display approval. [November 9](../spotify-validation/stop-disconnect-2026-11-09.md)
added allocation-free alpha assets; owner then verified remote pause/resume,
Home and radio return on `fcc2828…`. This does not close sustained UI/resource tests.

Test active/paused/missing/long metadata, phone transfers, local override, late events,
encoder mute/volume/hold, web edits and Wi-Fi recovery with serial closed. Inspect
TFT legibility and timing during sustained playback. Reproduce native/render/input/
web checks and build after code changes; record visual, functional and hardware
results in [status](implementation-status.md) and the [web ledger](web-configuration/progress.md).
