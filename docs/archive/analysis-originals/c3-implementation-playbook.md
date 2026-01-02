# Implementation Playbook: Light Pattern Development (LGP)

This playbook is the engineering companion to:

- `docs/features/CREATIVE_PATTERN_DEVELOPMENT_GUIDE.md`
- `docs/effects/STORYTELLING_FRAMEWORK.md`

It defines how to specify, implement, and validate new patterns/effects safely and consistently.

## Hard constraints (non‑negotiable)

- **Centre origin only**: all propagation expands from LEDs **79/80** or contracts to **79/80**.
- **No rainbows**: no full‑spectrum hue sweeps, rainbow cycling, or continuous hue rotation.

## Pattern specification template (copy into PRs)

### 1) Overview
- **Name**:
- **Category**: standing / travelling / moiré / depth / spectral split
- **One‑sentence story**:
- **Primary medium**: LGP (dual edge) / strips

### 2) Optical intent (LGP)
- **Which optical levers are used**: phase / spatial frequency / edge balance / diffusion / extraction softness
- **Film‑stack assumptions**: prism/BEF present; diffusion layer present; extraction features present
- **Expected signature**: what the plate should do that raw LEDs cannot

### 3) Control mapping (encoders)
- Speed →
- Intensity →
- Saturation →
- Complexity →
- Variation →

### 4) Performance budget
- **Target frame rate**: 120 FPS minimum (8.33 ms budget)
- **Worst‑case compute path**: describe it
- **Memory strategy**: static buffers preferred; avoid large stack allocations

### 5) Compliance checklist
- Centre origin: yes/no
- No rainbows: yes/no
- No unsafe strobe: yes/no

## Implementation rules (what keeps the system stable)

### Memory safety
- **Do not allocate large buffers on the stack** (LGP interference maps are the classic failure mode).
- Prefer:
  - **static fixed‑size buffers**, or
  - **PSRAM allocations** (if available) for truly large maps, with graceful fallback.
- Avoid repeated `new`/`delete` in hot paths; use pools or static storage.

Reference: stack overflow risk analysis in `docs/optimization/STABILITY_ANALYSIS_AND_FIXES.md`.

### Performance hygiene
- Keep hot loops branch‑light and cache‑friendly.
- Prefer integer / FastLED optimised maths where possible (avoid heavy `sin()`/`cos()` in tight loops unless proven safe).
- Avoid per‑frame divisions; precompute steps or use scaled integer mapping.

### Centre‑origin enforcement
When building spatial functions, define everything in terms of:

- `distFromCentre = abs(i - 79.5)`
- `side = (i < 80) ? left : right`

and drive the narrative from centre behaviour.

### No‑rainbows enforcement
- Do not spin hue continuously.
- “Colour motion” must come from:
  - palette cross‑fade between **approved constrained palettes**,
  - edge dominance swaps,
  - intensity/contrast envelopes,
  - diffusion changes,
  - phase relationships and spatial frequency.

## Test procedures (required)

### A) Visual verification (LGP)

1. **Box count calibration (standing patterns)**
   - Sweep Complexity through its range and record approximate box counts per side.
   - Confirm stability: boxes do not “jump” unpredictably when Complexity changes by one detent.

2. **Phase sweep (interference patterns)**
   - Test phase regimes: 0°, 90°, 180°.
   - Validate: 0° is most standing; 180° increases travelling/cancellation; 90° produces drifting complexity.

3. **Diffusion sweep**
   - Validate that diffusion softens aliasing but does not erase motif legibility.

4. **Viewing angle check (prism/BEF stacks)**
   - View on‑axis and off‑axis.
   - Confirm the motif survives angular redistribution (no “only visible at one angle” failure).

### B) Performance validation

1. **Worst‑case settings**
   - Set Intensity and Complexity to maximum.
   - Confirm frame stability (no visible judder, no watchdog behaviour).

2. **Long‑run stability**
   - Run a 30–60 minute loop over the new effect and adjacent effects.
   - Watch for memory regressions (heap fragmentation, stack warnings, instability).

### C) Safety and operational checks

- Confirm the **emergency stop** behaviour remains functional in the control surface (see `docs/features/WEB_INTERFACE_GUIDE.md`).
- Avoid high‑contrast narrow pulses that resemble strobing.
- Ensure brightness is reasonable under expected power limits; rely on existing power management mechanisms where configured.

## Definition of done (DoD)

- The pattern has a clear **one‑sentence story** and fits a taxonomy category.
- Encoder mappings are consistent and documented.
- Centre origin rule is satisfied.
- No‑rainbows rule is satisfied.
- Visual verification steps A1–A4 completed.
- Performance validation B1 completed at minimum; B2 recommended for merges to main.
- No new stability risks introduced (no large stack allocations, no uncontrolled heap churn).


