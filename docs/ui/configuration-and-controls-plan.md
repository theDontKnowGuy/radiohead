# Controls and settings scope

This is the current specification, not proof every item is finished.
It supersedes older alarm/timer/preset/encoder proposals. Keep the [visual design](visual-contract.md).
Use the [web ledger](web-configuration/progress.md) for implementation evidence.

## Encoder and sleep

| Action | Behavior |
| --- | --- |
| Turn | Volume 0–21 through the existing curve, on every page |
| Click/release | Mute/unmute; retain volume and stream position |
| Hold 3 s | Current external-power behavior: light sleep; release then press to wake |
| Held at power-on/reset | Wi-Fi recovery, not touch calibration |

Touch owns navigation and destructive confirmations. Encoder never selects a
menu row. Turning while muted changes the stored volume without unmuting.
A hold fires once and cannot also click on release. Dimmed touch wakes only and
is consumed through release; encoder wakes and applies its explicit volume/mute action once.
Consume both initiating and waking releases before normal startup resumes.

Pins: A/B/SW 4/7/13; backlight 1; touch clock/MOSI share 12/11 (remove old 4/7 touch wiring).
Optional K0 stays 6; physical sleep/current/wake still needs full acceptance.
A dimming race was fixed by snapshotting activity before reading a fresh clock,
preventing unsigned wrap from falsely dimming during rotation; host checks/build passed.

## What belongs where

| On the radio | In the web interface |
| --- | --- |
| Browse/play stations and programs; favorites; confirmed station removal | Discover/add/edit stations and program sources |
| Volume/mute feedback; custom bass/mid/treble | Remote volume/mute; Audio tone/filter settings |
| Automatic dimming timeout; supported theme only if more than one exists | Weather key/location/units/visibility; timezone and 12/24-hour clock |
| Wi-Fi state, Retry and actual setup address/QR handoff | Scan/network/password/forget; advanced diagnostics |
| About; explicit touch calibration; confirmed restart/reset | OTA/restart/reset; calibration start (tap corners on TFT) |

TFT Settings groups: **Audio, Display, Wi-Fi, Device**. No Weather/Time TFT section.
Use shared committed values and explicit Save/Cancel drafts; reject stale edits.
New/untouched legacy placeholder timezones default to `Asia/Jerusalem`; preserve
explicit non-placeholder zones. Only unambiguous supported city/country matches
may select a timezone automatically. Unknown weather/clock states must stay honest.
Calibration loads saved values or defaults at boot, never blocks startup for taps.

Current web order: **Stations, Programs, Audio, Spotify, Network, Weather & time,
Device & maintenance**. Setup AP shows Network only. The October updates remove
Appearance/M3U import and hide logo upload/configuration; existing saved artwork stays.
Add 20 px between Add station and the saved list. Programs has ten slots with
Omny/public RSS sources; [backup/restore is API-only](../settings-backup.md).

## Removed features

Remove active behavior and controls for alarms (including timer wake), sleep timer,
EQ presets, brightness editor, idle-clock mode, spectrum/VU settings, language
selector, station/favorite reordering and arbitrary color editing. Do not relocate
them to web, leave disabled rows, or add Normal/News/Music preset replacements.
Keep custom tone, ordinary Home clock, dimming, fonts/Hebrew, backlight primitives
and physical wake. Retired `/setalarm`, `/setpreset`, `/togglespec`, `/setVisual`,
`/setskin`, `/defaultskin` must not mutate; use 410 or not-found, never fake success.

Stop using retired `almH/almM/almA/spec` and skin settings. Leave legacy keys inert
rather than clearing a namespace. Preserve stations/URLs, Wi-Fi/weather, volume,
custom EQ, favorites and calibration. Later cleanup must target only retired keys.

## Future/background and power decisions

Custom TFT background upload remains a requested capability, **not implemented**.
Appearance tab removal does not authorize restoring it. Before enabling another
entry point, prepare crop/fit to 320×240 in-browser, define an independent bounded
format/budget, validate again on-device, and keep the previous asset until durable
commit. Restore-default, full storage, interruption/reboot and factory-reset policy
must preserve unrelated artwork/settings. No runtime blur or image/network work in rendering.

Separate Standby/Power Off menu remains TBD. The [battery proposal](../hardware/battery-power-architecture-plan.md)
accepts immediate hardware double-click off and no off guarantee while charging;
it bypasses saves/OTA protection. Software long-press shutdown and the current 12 V
amplifier power path are unfinished. Do not label light/deep sleep electrical off,
invent source detection, or remove current recovery before a replacement is tested.
OTA defers software power transitions. Source-unknown/failed cutoff must be reported honestly.

## Work order and checks

C1 remove features; C2 encoder; C3 retained settings; C4 background; C5 power after
hardware decisions. Keep media/settings/display/web/device responsibilities separate.
Build and run focused boundary/input/persistence/conflict checks after code changes.
Then test TFT/audio, web synchronization, upgrade with legacy keys, dim/wake,
Wi-Fi recovery, OTA and physical power. Record native visuals separately from
hardware results; neither a compile nor a dark screen proves completion.
