# November 4 radio-silence findings

Times: Asia/Hebron, UTC+02:00. Galatz (`http://glzwizzlv.bynetcdn.com/glz_mp3`),
saved slot 1; observed address `192.168.11.127` (use the displayed address if changed).
**Wi-Fi loss/rejoin failures emptied the input buffer and stopped PCM. Sound returned
when Wi-Fi and playback recovered. The initiating cause remains unknown.**
Automatic roaming was a candidate, not a proved trigger. No settings, playback,
restart or flash action was taken during the read-only incident collection.

## Evidence

Original numeric captures remain in [2026-11-04/](2026-11-04/) with
[SHA256SUMS](2026-11-04/SHA256SUMS). Keep these files unchanged.

| Capture | Use |
| --- | --- |
| [radio-silence-2026-11-04.json](2026-11-04/radio-silence-2026-11-04.json) | Primary outage/recovery, 08:09:48–08:12:25; onset already overwritten |
| [radio-silence-player.json](2026-11-04/radio-silence-player.json) / [resources](2026-11-04/radio-silence-resources.json) | Galatz, volume 8, mute false, notch off; no allocation failures |
| [radio-diagnostics-2026-11-04.json](2026-11-04/radio-diagnostics-2026-11-04.json) | 07:49:06–07:55:32 and prior November 3 tail; software-restart archival |
| [initial](2026-11-04/radio-diagnostics-initial.json), [followup](2026-11-04/radio-diagnostics-followup.json), [latest](2026-11-04/radio-diagnostics-latest.json) | November 3 baseline/redraw delays; “latest” is an old filename |

Spans are retained tails, not continuous recordings. November 3 early hash prefix
`aced42f2ce63fec9`; November 4 live/previous `c78bbe75c4d568ad`, detailed logging on.
Boot 3/previous boot 2 ended by software reset; no crash bank then. Version alone
cannot identify the exact ELF/source.

## Timeline

| Local time | Evidence |
| --- | --- |
| 08:09:48 | Already disconnected/connecting; input 0, PCM 108,222, disconnect count 193 |
| Until 08:12:10 | PCM/input flat; repeated failures, mostly reason 2 AUTH_EXPIRE |
| 08:12:06 onward | Count 235→237; last reason 36 STA_LEAVING |
| 08:12:13 | Connected ~−56 dBm, input still 0 |
| 08:12:18 | Ready/Playing; input 24,599, PCM 108,247 advancing |
| 08:12:25 | Input 381,721, PCM 109,502 |
| Later read (~08:12:51) | Input ~655,350, PCM 113,837, count still 237; user confirmed sound back |

The later read was inspected, not separately saved. Internal free heap ~35–38 KB,
lifetime min 20,352 B, PSRAM ~6.35 MB. No evidence that memory exhaustion or a
broadcaster outage caused this silence. Reason 2 is authentication timeout/expiry,
not proof of a wrong password/router defect; 237 counts include failed rejoins.
Missing onset prevents identifying the first AP/roam event.

## Other findings

- Healthy-period display redraws produced ~550–690 ms loop gaps, often minute-boundary
  clock changes (19/24 events in one capture), sometimes every 5 s from other dirty
  triggers. No observed pause was correlated then. Optimize separately from Wi-Fi.
- Buffer 655,350 main + 65,535 reserve explains fills up to 697,170 B; not overflow
  evidence. Do not patch vendor code for this reading alone.
- PCM counts are pre-I2S, not audible-output proof; idle time is 0 while Connecting.
  Timing fields can describe different windows. Event coalescing can lose onset.
- Original logging build: app 4,700,480 B, DIRAM 261,947 B, RTC SLOW 6352 B.
  Detailed mode was observed on device; physical off/on UI operation was unverified.
  `check_touch_diagnostics.py` had a stale `UiRenderState.volumeOverlay` fixture failure.

## What follows

[Network follow-up](network-followup.md) added first-incident preservation, roam
breadcrumbs, a persisted roaming switch and watcher. [November 7 roaming](../wifi-roaming.md)
then added playback gating and unpinned fallback; device acceptance is still open.
Keep first-failure/AP evidence before attributing cause. Compare roaming-on/off
runs without changing station/position; no router logs are available.
[Collection/API details](../radio-stall-diagnostics.md), [panic reports](../radio-crash-diagnostics.md).

Keep network/storage out of PCM callbacks, USB serial closed, vendor code unchanged,
and current GPIO2 mute/TFT behavior intact. Historical build-only checks do not
replace listening, source-switch, outage and memory measurements on the installed hardware.
