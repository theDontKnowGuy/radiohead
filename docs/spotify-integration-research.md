# Why native Spotify Connect

Research began 2026-09-24 at `b47961c`. **The chosen design is ESP32-only playback**,
controlled by the Spotify app, with no Raspberry Pi/Linux/helper service. It is
an experimental unofficial Connect implementation requiring Premium. Preserve
radio, podcasts, TFT/web, mute/tone and stored settings. Current [hardware](hardware/current-audio-hardware.md)
is PCM5100/HX-A244, I2S 39/40/41 and GPIO2 mute; old MAX/pin examples are obsolete.

Spotify now ships in the default build. This page explains the choice;
[validation results](spotify-validation/progress.md) and [remaining tests](spotify-native-validation.md)
take precedence over the original research sequence. UI work was later explicitly requested.

## Useful upstream references

| Project | Use here |
| --- | --- |
| [cspot](https://github.com/feelfreelinux/cspot) | C++ Connect/decoder foundation; upstream age does not establish current service compatibility |
| [Waveshare S3 fork](https://github.com/VitaliBorys/cspot-waveshare-esp32s3) | Closest 16 MB/8 MB precedent; different ES8311 codec/IDF 6 example, not drop-in firmware |
| [Squeezelite-ESP32](https://github.com/sle118/squeezelite-esp32) | Lifecycle/source arbitration reference, not a firmware replacement |
| [StreamCore32](https://github.com/tobiasguyer/StreamCore32) | Native protocol/build reference; different external decoder |
| [librespot](https://github.com/librespot-org/librespot) | Protocol reference; Linux runtime is outside the chosen design |

Adopted pinned candidate/cspot/Bell revisions are in the [build record](framework-consolidation-validation.md).
Keep upstream notices (cspot GPLv3-or-later); commercial certification is not a
requirement for this personal project. Repository activity alone is not a playback test.

## Design lessons

- The Spotify Web API controls an existing player; it supplies no full-track audio.
  Web Playback SDK plays on the browser host. A Spotify track URL is not an MP3 stream.
  ESP32-S3 lacks Classic Bluetooth A2DP. These do not implement native Connect.
- Candidate examples compiled a client secret into firmware. This project instead
  provisions privately into NVS; gitignored headers still embed secrets. Never log
  tokens/pairing blobs/signed URLs or ask for account passwords. Test real CDN
  authorization, renewal and restart; token issuance alone proves none of them.
- `Audio::stopSong()` retains I2S. A second sink needs serialized release/restore,
  not two live masters. The shared Audio object must remain valid. See [S2](spotify-validation/s2-output-handoff.md).
- `media` owns source generations, cancels local retry on takeover and rejects late
  events. Local selection stops Spotify; transfer away leaves local playback stopped.
  Preserve 0–21 volume, mute and custom tone, including remote volume while muted.
- Use bounded events/PCM queues, one output owner and existing HTTP/mDNS. Measure
  internal/DMA memory, contiguous blocks, stacks and service gaps separately from PSRAM.
  No runtime AI, proxy/token broker, or catalog browser is required.

Original research did not authenticate, flash or measure playback; subsequent
results are in the ledger. Do not revive the excluded companion architecture or
claim full reliability from a short successful song.
