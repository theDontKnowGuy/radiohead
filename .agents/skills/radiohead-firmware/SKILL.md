---
name: radiohead-firmware
description: Implement, refactor, diagnose, or review this ESP32-S3 PlatformIO internet-radio firmware. Use for application code, device behavior, web routes, display/audio/input/persistence changes, or build configuration; do not use for unrelated vendored-library maintenance.
---

# Radiohead firmware workflow

Work within the existing module boundaries and leave the repository in a buildable,
reviewable state. Preserve behavior unless the user requests a change.

## Establish context

- Inspect `git status` and relevant diffs before editing; existing changes belong to the
  user unless the current task created them.
- Read [references/architecture.md](references/architecture.md) before changing module
  ownership, hardware configuration, startup, or the main loop.
- For open-ended vibe coding, agent routing, plan/review judgments, or any proposed
  TypeSafe integration, also read
  [references/typesafe-vibe-coding.md](references/typesafe-vibe-coding.md).
- Read only the source, headers, and direct callers involved in the requested behavior.

## Make the change

- Put behavior in the module that owns it; keep `main.cpp` focused on coordination.
- Keep deterministic rules in C++. Use Jev only for bounded semantic judgments where
  ordinary code would otherwise rely on fragile natural-language heuristics.
- Validate web inputs, remote response fields, persisted indexes, and values used as
  array subscripts.
- Preserve Preferences keys and HTTP routes unless an intentional migration or API change
  is part of the request.
- Do not edit the bundled audio library unless explicitly asked.
- Keep credentials outside the firmware and repository.

## Verify and hand off

- Build with `pio run -e esp32s3` after source or configuration changes.
- Run `git diff --check` and inspect the scoped diff.
- Review PlatformIO RAM and flash usage for material regressions.
- Explain which behavior was compiler-verified and which still needs testing on the
  physical device.
- When a live Jev judgment influenced a decision, report its purpose and model version;
  never present that judgment as compiler or runtime evidence.

