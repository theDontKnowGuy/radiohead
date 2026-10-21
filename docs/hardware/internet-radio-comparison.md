# Comparison with the clean Internet-Radio project

2026-10-21. The comparison below records the earlier 3.4.7 snapshot. The user
subsequently requested an exact **3.4.0 test without Spotify adaptation**,
then requested restoration of **4.0.0**. The original 4.0.0 library, callbacks,
podcast seeking and Spotify-enabled build configuration are now restored.
The 3.4.0 test export remains at `.pio/build/audio-340-test/`.
That test uses upstream 16 × 512-frame DMA buffering and 16-bit I2S, so the
Radiohead buffer/format entries below are historical, not the test settings.
The user subsequently confirmed the **same hardware and same stream** and
reports more crackles in Radiohead. Matched acoustic loudness and EQ have not
been separately specified.

Reference directory:
`/Users/avner/Documents/Projects/ESP32Projects/radio-original/Internet-Radio/`.

The user reports no right-speaker crackles with that project, but right-speaker
crackles with Radiohead. The user has now confirmed the same hardware and
stream for this working-reference comparison; do not ask for those again.
Listening level and EQ remain unspecified. The files inspected are source snapshots, not
proof of which exact images were running during either listening observation.

## Source differences

The reference's `main/CMakeLists.txt` builds `main/main.cpp` and its local
`components/audio` library. The root `x.ino` is not that CMake entry point.

| Area | Internet-Radio | Current Radiohead |
| --- | --- | --- |
| Audio library | Manifests 3.4.4; source banner 3.4.4ij | Upstream 3.4.7 plus documented Spotify handoff patch, downgraded from 4.0.0 earlier in this session |
| Stream feeding | Dedicated `audio_loop` task on core 1, priority 5, calls `audio.loop()` every loop with a 2 ms delay | `audio.loop()` shares Arduino loop execution with web requests, input, settings saves, display and other services; additional calls exist during some display/startup work |
| Decoder/output task | Library task on core 0, priority 2 | Library task on core 0, priority 2; cooperative Spotify handoff added |
| Local I2S DMA | 32 descriptors × 222 frames | 6 descriptors × 256 frames because the build defines `RADIO_SPOTIFY_EXPERIMENT` |
| Local sample output | Philips stereo; 16-bit slots for 8/16-bit decoded audio, 32-bit slots for 24/32-bit | Philips stereo; always 32-bit samples/slots |
| Volume | UI 0–30 converted to integer library 0–21; default UI level 15 requests library level 10 | UI 0–21 maps through `volCurve` 0–55; library explicitly configured for 55 steps |
| Tone | No application `setTone()` calls found | Restored bass/mid/treble applied through `setTone()` |
| Volume persistence | `set_volume()` changes audio/UI and logs; no NVS write in that function | Volume changes schedule settings persistence after one second; legacy route also has a synchronous save |
| Source pin definitions | BCLK 7, LRC 5, DOUT 6 | BCLK 39, LRC 41, DOUT 40 |
| Configured CPU | 160 MHz in reference `sdkconfig` | 240 MHz in generated normal-build `sdkconfig` |

At a hypothetical matching 44.1 kHz, descriptor capacity corresponds to about
161 ms versus 35 ms of frames. These are capacities, not measured occupancy or
underrun tolerance. The ordinary 16-bit reference queue holds about 28 KiB of
sample data; Radiohead's 32-bit queue holds 12 KiB. Radiohead intentionally
reduced its DMA allocation so local I2S can be recreated with Spotify resident.
Blindly restoring the larger queue can therefore break source handoff through
internal-memory allocation failure.

## Interpretation and next discriminating comparison

The earlier downgrade to 3.4.7 does **not** reproduce this reference's 3.4.4
decoder, DSP or output implementation. Library version, stream feeding and
DMA capacity are concrete software differences, not established causes.
Neither project deliberately treats the right channel as a noisy channel;
the timing differences alone do not explain why only that speaker crackles.

Keep crackles separate from the shared sandy distortion. The existing direct
PCM tests reproduced sand without production streaming/UI/DSP, and the
16/32/16-bit comparison did not remove it. Those results do not establish the
cause of this newly contrasted right-only crackle. Do not repeat those tests
as though they had never been done, or attribute a clean result to storage
suppression without verifying the installed mode.

Same hardware and stream are now confirmed. Keep the exact installed image,
listening level and EQ explicit for subsequent comparisons. With 4.0.0
restored, a controlled larger-DMA test is a useful next comparison; change
only buffering before combining it with task or library changes.
Preserve shared pins and Spotify ownership; do not copy the reference's GPIO
numbers or move `audio.loop()` into a concurrent task without coordinating all
Audio API calls. Record crackle and sand separately at matched loudness and
flat EQ, then return to the baseline for an A/B/A check.

This investigation changed documentation only. No firmware, reference-project
files, device settings or wiring were changed; no image was flashed.


## DMA capacity explanation after restoring 4.0.0

The restored 4.0.0 image also selects six 256-frame descriptors because
`components/radiohead_audio/CMakeLists.txt` defines `RADIO_SPOTIFY_EXPERIMENT`.
At 44.1 kHz, its maximum final DMA queue holds 1,536 / 44,100 = 34.83 ms;
the reference holds 7,104 / 44,100 = 161.09 ms. The latter has 4.625 times the
frame capacity. These calculations assume 44.1 kHz; duration changes with rate.

A full queue can keep the I2S peripheral fed for that duration if the output
writer temporarily stops supplying frames. Actual occupancy may be lower.
If output starves, discontinuities can be audible. Capacity does not measure
underruns or establish that starvation causes the reported right-only crackles.
4.0.0 also has compressed-input and decoded-PCM buffers before DMA; those cannot
replace servicing the final DMA queue if the output task itself is stalled.

The six-descriptor reduction was a Radiohead integration change to permit
late local-I2S reallocation with Spotify resident, rather than a 4.0.0 upstream
requirement. A larger queue is a concrete software test candidate, with local
output restoration memory fit checked separately. No buffer changes, build or
flash were made while answering this question.


## Larger-DMA experiment prepared

At the user's request, prepared a separate opt-in 4.0.0 image changing only
local DMA descriptor count from 6 to 28. Frames remain 256, giving 162.54 ms
at 44.1 kHz, close to the reference's 161.09 ms. Public library settings are
applied before I2S initialization; the vendor sources are unchanged. Normal
firmware remains at six descriptors. The existing read-only resource endpoint
now reports configured local DMA descriptor/frame counts in both new builds.

Baseline and test builds, output-configuration/diagnostics host checks, export
validation and diff checks pass. SDK settings and partition bytes match.
The 44 KiB extra DMA sample payload is runtime memory, so build success does
not establish startup or late Spotify-return allocation fit. No image was
flashed and no new listening result is available.

See [builds, readback, install and restore steps](../../experiments/large-dma/README.md).
