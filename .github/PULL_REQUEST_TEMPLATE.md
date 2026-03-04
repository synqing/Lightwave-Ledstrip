# Pattern Effect Pull Request

## Pattern Specification

### 1) Overview
- **Name**: [Pattern name]
- **Category**: [standing / travelling / moire / depth / spectral split]
- **One-sentence story**: [Brief narrative description]
- **Primary medium**: [LGP (dual edge) / strips]

### 2) Optical Intent (LGP)
- **Which optical levers are used**: [phase / spatial frequency / edge balance / diffusion / extraction softness]
- **Film-stack assumptions**: [prism/BEF present; diffusion layer present; extraction features present]
- **Expected signature**: [What the plate should do that raw LEDs cannot]

### 3) Control Mapping (Encoders)
- **Speed** -> [Description]
- **Intensity** -> [Description]
- **Saturation** -> [Description]
- **Complexity** -> [Description]
- **Variation** -> [Description]

### 4) Performance Budget
- **Target frame rate**: 120 FPS minimum (8.33 ms budget)
- **Worst-case compute path**: [Description]
- **Memory strategy**: [Static buffers preferred; avoid large stack allocations]

## Visual Pipeline and Memory Safety (Mandatory)

Reference context:
- `AGENTS.md` -> Visual Pipeline Guardrails (Agents)
- `firmware-v3/docs/INCIDENT_LED_STABILITY_POSTMORTEM_2026-03-04.md`
- `firmware-v3/docs/STAGE1_CHERRY_PICK_PLAN.md`
- `firmware-v3/docs/TECHNICAL_DEBT_AUDIT_2026-03-04.md` (H-5 + addendum)

### A) Timing and Render Invariants
- [ ] `FastLED.show()` wire-time sanity checked on target hardware. On dual 160 strips, average show time is in expected range (~4.8-5.5 ms), not ~1 ms.
- [ ] FPS cross-checked from at least two sources: `currentFPS` and `framesRendered/uptime` (plus `avgFrameTimeUs` where available).
- [ ] Renderer cadence remains self-clocked to 120 FPS budget (`~8333 us`), not RTOS tick-quantised.
- [ ] `showSkips=0` under normal load in serial status output.

### B) Stack and Heap Safety
- [ ] No `new`/`malloc`/`String` in any `render()` path.
- [ ] No large transient arrays on `loopTask` hot paths (especially LED/audio snapshots, effect lists, telemetry frames).
- [ ] Large shared snapshots use persistent buffers allocated during init (PSRAM-preferred, DRAM/static fallback).
- [ ] Cross-core audio snapshot access avoids by-value large copies in hot paths.
- [ ] Stack high-water marks and free heap remain stable during soak (no progressive collapse).

### C) Target and Hardware Validation
- [ ] Flash target verified by MAC before upload (do not trust port name alone).
- [ ] K1v2 builds keep status strip disabled (`FEATURE_STATUS_STRIP_TOUCH=0`) unless hardware wiring explicitly exists.
- [ ] Any pin-map edits were validated against the intended hardware revision (K1 v1 vs K1v2).

### D) Runtime Gates (Post-Flash)
- [ ] Boot is panic-free (`Guru Meditation`, stack canary, assert, WDT, RMT channel faults absent).
- [ ] Audio gate passed (expected non-zero live band activity under music input).
- [ ] Visual gate passed (no dual-channel corruption/tearing, no severe stutter).
- [ ] Soak gate passed (duration recorded below) with stable stack/heap telemetry.

### E) Evidence (Required)
- [ ] Include serial `s` status excerpt(s) showing frame/LED timing, stack, heap.
- [ ] Include build environment and exact upload command used.
- [ ] Include tested hardware ID (MAC) and board revision (K1 v1 / K1v2 / non-K1).

## Implementation Details

### Code Changes
- [ ] Effect function implemented
- [ ] Effect registered in effect registry
- [ ] Header declaration added to appropriate effects header file
- [ ] FastLED optimisation utilities used where applicable (`FastLEDOptim.h`)

### Testing
- [ ] Visual verification on hardware completed
- [ ] Performance profiling shows >= 120 FPS
- [ ] Memory usage within budget (< 10KB per pattern, unless justified)
- [ ] No compiler warnings introduced by this PR

### Documentation
- [ ] Pattern story documented
- [ ] Encoder mappings documented
- [ ] Performance characteristics documented
- [ ] Incident/guardrail docs updated if new failure modes were discovered

## Definition of Done

- [ ] Pattern has clear one-sentence story
- [ ] Fits taxonomy category (see `docs/creative/LGP_PATTERN_TAXONOMY.md`)
- [ ] Encoder mappings documented
- [ ] Centre origin rule satisfied (LEDs 79/80)
- [ ] No-rainbows rule satisfied (hue range < 60 degrees)
- [ ] Visual verification steps completed
- [ ] Performance validation passed (>= 120 FPS)
- [ ] No new stability risks introduced
- [ ] Code follows FastLED optimisation patterns
- [ ] Memory safety verified (no large stack allocations)
- [ ] Visual pipeline and memory safety section above is fully checked

## Debrief and Evidence

### Problem and Requirement
[What issue or requirement this PR addresses]

### Design Rationale
[Why this solution was chosen and which alternatives were rejected]

### Technical Challenges
[Non-obvious issues encountered and how they were resolved]

### Performance and Memory Impact
- **SRAM delta**: [bytes]
- **Heap free after change**: [bytes]
- **Stack high-water delta**: [bytes/words]
- **PSRAM usage change**: [bytes]

### Test Methodology and Results
[Build targets, hardware used, soak duration, pass/fail criteria]

### Known Limitations
[Any remaining risks or follow-up work]

## Additional Notes

[Any additional context, known issues, or follow-up work needed]
