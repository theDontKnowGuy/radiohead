# Design/CSS handoff checks — 2026-09-21

These checks apply to W0, the handoff assets. They do not verify firmware or a
connected ESP32. Production gates remain open in [progress.md](../progress.md).

## Source and scope

- Read the current configuration/removal and station-discovery specifications,
  the TFT visual contract, and the existing web server's pages/routes/bounds.
- Preserved the mockup's navy/coast/blue direction and archived its source and
  desktop, phone and background-editor screenshots.
- Supplied `radiohead.css` with local photo URL, independent-page reset, readable
  primary action color, focus/error states, responsive controls and modal overlay.
- No application source, firmware config, synced project reference, or native TFT
  implementation was changed by this handoff.

## Browser check of extracted CSS

Environment: temporary headless Google Chrome through Playwright. The archived
mockup's markup/interaction script was used as a test fixture with the **new CSS**
injected and the bundled photo embedded only to make the isolated test iframe
self-contained. The deliverable CSS itself uses a relative local asset URL.

Passed:

- Stylesheet loaded; computed page background `rgb(11, 19, 33)` and filled primary
  action `rgb(23, 104, 206)` matched the supplied tokens.
- Stations, Network, Weather & time, Appearance and Device at 320, 390, 768, 1024
  and 1440px content widths: no horizontal overflow.
- Confirmation overlay uses viewport positioning; Escape dismisses it.
- Search/result/editor smoke check, including editor at 320px.
- No browser page runtime errors during that check.
- Visually inspected [desktop with extracted CSS](css-desktop.png) and
  [320px editor with extracted CSS](css-editor-mobile.png).

The older mockup was also exercised for discovery/test/save, mute, dirty-form
navigation, Wi-Fi, clock format, background upload/framing and reset confirmation.
Those are simulated transitions only. Do not record them as device tests.

## Not established here

- Live endpoints, station-directory integration, firmware upload or device action.
- Actual flash/RAM budget after packaging, audio continuity or service timing.
- Native TFT visual equivalence, device persistence, Wi-Fi reconnection or OTA.
- A complete accessibility audit; V4 still requires real-page keyboard, touch,
  screen-reader and contrast checks. The supplied focus/styles support that work.

Markdown/CSS local references, image dimensions, source/asset hashes and final diff
whitespace are checked during installation. The manifest records the exact files
for this handoff; it is provenance, not a production asset build system.
