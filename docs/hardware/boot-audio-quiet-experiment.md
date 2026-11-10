# Old boot-static experiment

Historical MAX98357A test only. Those boards are permanently removed; current
hardware is [PCM5100/HX-A244](current-audio-hardware.md) with [GPIO2 mute](speaker-mute.md).
Do not apply the old pins/standby explanation to the installed assembly.

The experiment (`kBootQuietOutputExperiment` in `src/media.cpp`) first held old
BCLK/LRC/DIN GPIO15/17/16 low and software volume zero, initialized I2S after
settings load, then restored boot volume only when the embedded sound opened.
Disabling the flag/rebuilding restored the prior sequence. User mute/Preferences
and later saved-station playback were unchanged. It could not control pre-setup
or supply-induced noise.

Build/diff passed; integrated image 4,492,016 B, DIRAM 260,359/341,760 B.
Totals included concurrent boot-mode work, not the experiment's isolated cost.
No flash or acoustic result was recorded.

For any new startup test, use the current mute circuit: compare ten cold starts
and software restarts at identical supply/volume, serial closed. Record burst
position relative to boot audio, complete boot sound, saved-station level, mute
and AP startup. A successful build does not demonstrate quiet speakers.
