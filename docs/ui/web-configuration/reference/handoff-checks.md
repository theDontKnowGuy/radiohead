# Web design checks (2026-09-21)

Historical W0 CSS/mockup checks only; [production progress](../progress.md) is separate.
Kept local coast/navy/blue assets, `rh-` styles, focus/error states and viewport modal.
Old Appearance/M3U mockup flows are superseded by the current scope.

Headless Chrome/Playwright with extracted CSS and mockup markup passed:

- Widths 320/390/768/1024/1440 without horizontal overflow.
- Computed background `rgb(11,19,33)` and primary fill `rgb(23,104,206)`.
- Modal positioning/Escape, search/editor smoke flow, no runtime errors.
- Inspected [desktop](css-desktop.png) and [320 px editor](css-editor-mobile.png).

Other mockup actions used simulated data. No ESP32 endpoints, persistence, OTA,
audio timing, native TFT equivalence or complete accessibility audit was tested.
Manifest hashes document source assets, not production correctness. Check real
pages/keyboard/focus/contrast/200% zoom and hardware separately.
