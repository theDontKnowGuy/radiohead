# Boot audio quiet-output experiment

Purpose: test whether a short, loud startup-static burst comes from the
firmware's initial I2S state. This is an experiment, not a confirmed fix.

The experiment is enabled by `kBootQuietOutputExperiment` in `src/media.cpp`.
Set it to `false` and rebuild to restore the previous startup sequence. The
same source is used by the baseline and integrated Spotify builds.

## Sequence

1. The first call in `setup()` holds BCLK (GPIO 15), LRC (GPIO 17), and DIN
   (GPIO 16) low and sets software volume to zero. Output latches are cleared
   before GPIO output drivers are enabled.
2. The existing I2S initialization runs after settings load, with volume still
   zero. The audio library supplies silence while waiting for playback.
3. Once the embedded boot sound opens successfully, its normal volume is
   restored before the boot loop services playback. If opening fails, output
   stays at zero until the existing saved-playback startup restores volume.

This does not change the user mute state, Preferences, the boot sound asset,
or the subsequent playback flow. No amplifier shutdown GPIO is wired or added.
The MAX98357A enters standby with BCLK stopped, but software cannot control the
interval before `setup()` runs or eliminate a supply-induced transient.
See the [manufacturer datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX98357A-MAX98357B.pdf),
Standby Mode and SD_MODE and Shutdown Operation.

## Device comparison (pending)

With the USB serial monitor closed, compare at least ten cold power-on boots
with the experiment enabled against the previous image or a disabled build.
Use the same supply, speakers and saved volume. Record whether a burst occurs,
its loudness, and whether it is before, at the start of, or after the boot sound.
Also compare software restarts separately from cold starts.

Check that the entire boot sound plays, the saved station starts at the normal
volume, and mute/volume controls still work. Check setup/AP boot as well.
Do not count a successful compile as acoustic verification.

## Build verification

- `pio run -e esp32s3`: passed, including the integrated Spotify image.
- Baseline: 96,244 bytes static RAM; 3,690,003 bytes reported flash use.
- Integrated: 260,359 / 341,760 bytes DIRAM used; 81,401 bytes static
  headroom. Application binary: 4,492,016 bytes, with 2,061,584 bytes free
  in the app partition.
- `git diff --check`: passed. No vendored code changes.

These totals include concurrent boot-mode work in the shared worktree and
are not a measurement of this experiment's memory delta. This task did not
flash the device; acoustic and playback checks remain pending.
