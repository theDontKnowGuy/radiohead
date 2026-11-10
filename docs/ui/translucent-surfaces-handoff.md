# Translucent list surfaces

Keep the sunset visible through rounded navy rows; focused rows are stronger blue,
with opaque text/artwork/favorite icons. Use four 46 px rows and the right Up/Down
rail, not the example's six-row density. Highlight follows focus, not necessarily
playing identity. Do not change input/playback behavior in a surface change.

## Draw path

The existing 320×240 RGB565 PSRAM canvas is 153,600 B. Restore clean photo, blend
surface, then border/artwork/text/star. Do not repeatedly tint the prior composition.
Use [current prepared list assets](assets-manifest.md); old 304 px station-row
proposals predate the right rail. Normal/focused corners/opacity should leave
identical gaps. Header remains over the photograph.

Production Home/list alpha surfaces now use the allocation-free flash-run painter:
**do not reintroduce the unchecked `drawPng` DMA allocation path** documented in
[status](implementation-status.md). The physical TFT cannot provide safe alpha
readback. If the canvas is unavailable, use legible opaque native fills and bitmap text.
Keep `swap565_t` transfer ordering and reusable eight-row stripes.

On focus, favorite, paging, long→short title or overlay close, reconstruct all
intersecting layers from the photo. Dirty regions must cover rounded edges and
the old/new union. Service audio between bounded composition/transfer steps;
measure both costs. No continuous full-frame animation, runtime blur, scaling,
network fetch or additional full-frame cache without measured need.

## Verify

Use production `tools/native_ui/home.cpp` through `tools/render_ui_fonts.py`.
Check A→B→A and repeated identical renders equal a fresh frame, with no cumulative
tint; test pages/favorites/empty/missing art/Hebrew and allocation-failure fallback.
Existing hit targets must remain unchanged.

After code changes, build, inspect RAM/flash, run native/alpha checks and diff check.
Inspect 320×240 output, then physical contrast, rapid paging, minute rollover,
touch and sustained audio; record composition/transfer time, audio service gaps,
internal free/largest block and PSRAM. Keep visual/functional/device evidence
separate. A prepared asset or build alone does not close P2/P3.
