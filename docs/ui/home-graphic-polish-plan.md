# Home-screen refinement notes

Use current production assets and [visual rules](visual-contract.md), not the old
iteration tables. Scope is Home appearance at 320×240; audio/input/settings and
original `docs/bg1.png` stay unchanged.

## Current direction

- Direct 4:3 coastal background resize, no crop; current recipe uses saturation
  .92 and 16% black veil. Earlier 25%/different-tile proposals are superseded.
- Weather left as one compact icon/temperature/city/condition unit; clock/date/
  Wi-Fi right with no collisions. Brand/station subtitle must not compete with clock.
- Four identical separated category tiles; navy/blue/green/purple/slate treatment,
  muted outlines, readable light labels. Shared geometry owns rendering and hit boxes.
- Home clock uses the committed Inter alpha atlas, not stretched VLW digits.
  Old Light Condensed/Roboto/Medium width and anchor experiments are history;
  [asset manifest](assets-manifest.md) and current source define the final recipe.
- 2026-09-23 weather follow-up used 30 px Inter SemiBold and city/condition origins
  y=97/115, with 7 px visible-ink gaps. Later status entries take precedence.

## Evidence and remaining checks

September native fixture/geometry/text/restore/RGB565 checks passed across the
refinements. Some earlier device photos rejected opaque cards and unreadable
readback-dependent anti-aliasing; the memory-canvas approach replaced that path.
Those photos do not accept later revisions.

Reproduce with `python3 tools/render_ui_fonts.py` after a build. Inspect
`.pio/ui_native/home-smooth.png` and clock fixtures against `uiconcept.png` at
native size. Check long Hebrew station names, 12/24-hour time, unavailable weather,
focus/tile gaps, dimmed contrast, overlays and clock rollover. Never bake fixture
sample values into production.

Physical TFT appearance/touch and sustained-audio redraw timing remain open
except for the specific November 9 Home-return trial in [status](implementation-status.md).
That trial does not close broader P2/P3/soak acceptance. Future notes should record
one current result and actual differences, not every abandoned font-size iteration.
