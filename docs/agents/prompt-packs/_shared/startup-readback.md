# Shared Startup Readback

Start every LightwaveOS task with:

```text
GROUNDED / DEGRADED-MODE / REFUSED:

READBACK:
- Confidence: YES / NO, plus missing facts if NO.
- Task scope: single-target or multi-target. If multi-target, list SSA splits.
- Subsystem: audio / effects / network / core / docs / hardware / cross-cutting.
- First reads: exact files to read before acting.
- Hard constraints: centre-origin, no rainbows, no heap in render, 2 ms effect slot, dt-correct smoothing, AP-only unless task explicitly authorises WiFi-mode work, British English.
- Tool route: clangd for C++ symbols, targeted rg/read for docs, protocol YAMLs for network, serial/hardware checks for firmware behaviour.
- Non-goals: what this task must not touch.
```

If the agent cannot produce a grounded readback, it must stop and gather source truth before editing.
