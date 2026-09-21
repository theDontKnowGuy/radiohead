# Radiohead UI design and implementation

Read the **[configuration and controls specification](configuration-and-controls-plan.md)**
for the current TFT/web split, encoder volume/mute/hold behavior, removed features
and unresolved power decisions. It supersedes conflicting settings, alarm, timer
and encoder requirements in the older guide and concept panels.

For the ESP32-hosted web interface, use the
**[web configuration implementation handoff](web-configuration/README.md)**,
**[progress ledger](web-configuration/progress.md)** and
**[reusable CSS](web-configuration/radiohead.css)**. The handoff includes screen
content, interaction rules, desktop/mobile references, local assets, delivery
packages and separate visual/functional/device acceptance. The archived browser
mockup is illustrative; production integration is tracked independently.

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
