# Old static/channel investigation

**Closed for retired MAX98357A hardware, 2026-10-26.** Current
[PCM5100/HX-A244 assembly](current-audio-hardware.md) needs its own tests.
These observations are history, not instructions to reinstall MAX boards.

## What was observed, 2026-10-16

Radio and Spotify produced volume-dependent harsh static; Spotify pause silenced it.
A proven left/right test played both prompts from both speakers. Reported SD values
were 0.9/3.8 V; local-GND referencing was unconfirmed. Dayton PC83 speakers were
reported 4 Ω, wiring soldered, supply set 5 V and showing about 180 mA (not peak current).
A short thick hub-to-ground wire cleaned the first amp. The second stayed noisy;
probe contact at its GND reduced noise. This implicates a path, not a proven cause.

ISDS210A/Multi VirAnalyzer 3.13.12.0, X1; clip actually at the ground hub:

| Capture | Displayed result |
| --- | --- |
| VIN-to-hub DC, 5 V/div, 1 ms | 4.504–5.207 V |
| VIN-to-hub AC, 200 mV/div, 1 ms | 668 mVpp playing; 414 mVpp paused |
| Tip at amp GND, clip at hub | 764 mVpp while sound became clear |
| LRC, DC 1 V/div, 10 µs/div, 50 MSa/s | 44.100512 kHz, ~50%; −184 mV to 3.583 V |

These are displayed extrema, not established local ripple. Tip-to-hub ground
measurements include return-path/pickup effects. BCLK probing silenced the amp;
probe loading invalidates conclusions about missing clock. X10 scaling remains
unresolved. DIN activity does not prove stereo contents. Never ground a speaker output.

## Code finding and test source

Both paths configured Philips stereo; no intentional downmix was found.
Spotify still hard-coded GPIO15/17/16 while local audio used 39/41/40. Both were
changed to `include/app_audio_pins.h`; build/diff passed, not installed in this
record. App 4,514,288 B, DIRAM 260,523 B; previous export:
`.pio/build/before-shared-audio-pins/`. The stale pins were a source defect, not
proof of the reported noise's cause. A separate local volume-curve 55 vs library
21-step mismatch was observed and left unchanged.

[Direct stereo MP3](https://aoakley.com/articles/stereo-test-long.mp3): 18 minutes,
16 kHz stereo, verified opposite-channel zeros during isolated speech; center
prompts intentionally use both. Saved slot 1 “Left right”, or POST
`/api/stations/test` with name/url for an unsaved test. Still uses Wi-Fi.
User heard left playing both prompts and right speech-correlated static after
removing the probe. No mechanism was established. Do not carry old proposed MAX
scope/rewiring work forward to the new assembly.
