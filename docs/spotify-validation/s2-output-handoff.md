# Spotify output handoff notes

Spotify is now in the default firmware; old opt-in-only statements are superseded.
Current pins: BCK/DIN/LRCK 39/40/41 into [PCM5100/HX-A244](../hardware/current-audio-hardware.md).
September MAX/pin tests are historical. Owner accepted S2 on 2026-09-29;
formal transition/resource testing remains incomplete.

## Why the handoff exists

`Audio::stopSong()` leaves I2S0 allocated. The owner authorized the narrow vendor
patch on 2026-09-25: `releaseOutputForHandoff(timeoutMs)` parks the decoder,
acknowledges stop, clears/deletes I2S; `restoreOutputAfterHandoff()` recreates it
from saved pins before unpark. The shared `Audio` object is never destroyed.
Two idle release/restore cycles passed initially; that was not audible acceptance.

`media` serializes ownership with generations. Adapter mailbox: 16 events with
sequence numbers; stop/transfer-away always admitted. PCM cancellation is separate
so a full queue cannot prevent release. Local selection waits up to 1 s for sink
release/decoder acknowledgment; timeout leaves stopped/retryable. ACTIVATE grants
ownership, not arbitrary decoded PCM. Pause keeps session, transfer-away leaves
local stopped, and remote volume maps 0–21 while preserving mute/tone.
Pairing uses existing HTTP port 80 and mDNS; private keys stay in `spotify` NVS.
Changing the Connect name can change the derived ID and affect old pairing.

## The serial slowdown finding

A five-second I2S-task diagnostic print blocked in Arduino 3.3.11 `HWCDC::write`
when computer USB stayed attached without a reader: 100 ms timeout ×20 ≈2 s.
With serial closed, image `7e32a071` produced exactly **534,528 B/5 s** in 28
samples despite full queue/no I2S failures; audio was sluggish.
Removing all Spotify-output-task Serial writes (`32e24232…`) restored
**881,664–882,688 B/5 s** in 19 samples with zero empty polls/write failures;
owner heard clear sound. HTTP `/api/spotify/diagnostics` replaces hot-task printing.
Opening serial also resets this radio: keep it closed for listening tests.

PCM queue was separately enlarged 128→384 KiB PSRAM with 320 KiB prebuffer/rebuffer
threshold. A ~270 KiB production deficit motivated reserve; queue size alone did
not fix the repeatable serial slowdown.

## Recorded trials and limits

| Trial | Result |
| --- | --- |
| Sep 28 first integration | Underruns, decoder watchdog after local return, then web failures with ~6 KiB internal free; modem-sleep disable restored short PCM rate, not memory acceptance |
| Sep 29 clear short handoffs | Owner heard radio/podcast↔Spotify without overlap; exact required counts absent |
| Integrated Wi-Fi interruption, `ed4b809d…` | ~32 s HTTP outage, then full-rate clear audio without reselect/reboot; one recovery only |
| Counted local batch | 10 radio→podcast +10 reverse reached Playing; median 2.42 s radio /9.51 s podcast; no listening report |
| Rapid alternation | Third request exceeded 5 s; later recovered. Formal rapid test failed |
| Phone Next | One tap moved output to phone; later tap worked. Pause/mute/volume/nonflat tone short checks passed |
| Sep 30 Spotify→episode | Producer released; local I2S restore failed a 2048 B DMA allocation despite ~35 KiB free/7424 B largest block |

The Sep 30 candidate reduced integrated local I2S from eight to six 256-frame
descriptors (16→12 KiB), leaving old normal-only settings separate; repeated device
handoffs were not verified there. cspot tasks remained resident with incomplete
general stop/join after session failure. [Newer cancellation/TX/alpha fixes](stop-disconnect-2026-11-09.md)
take precedence over these early failures; a short success still does not replace
100 directed/20 rapid transitions, leak trend, renewal and soak.
