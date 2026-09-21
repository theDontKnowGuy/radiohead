# Radiohead UI design and implementation

Start with the **[visual contract](visual-contract.md)** and inspect
**[uiconcept.png](uiconcept.png)**. The user wants this sunset/blue, artwork-led
design adapted to the TFT. Visual similarity is required in P2 and P3, not deferred
to final polish.

Then read the **[agent implementation guide](agent-implementation-guide.md)** for
native layout, touch/encoder behavior, module contracts, budgets and work packages.
Check [implementation status](implementation-status.md) before continuing: the
existing cream/olive functional slice does not complete P2 or P3.

See the [station discovery and artwork plan](station-discovery-and-artwork-plan.md)
for browser station search, editable stream/logo suggestions, image conversion and
persistent artwork on the ESP32 without a project-operated backend.

- [Earlier interaction proposal](interaction-plan.md) and its `radio-ui.html`
  prototype: **superseded; historical only; do not implement this direction**.

The user has offered a clean background. Its handoff requirements and the visual
comparison checklist are in the visual contract; the separate asset is pending.

`uiconcept.png` is the multi-screen implementation reference. There is no dedicated
static-image firmware mode.
