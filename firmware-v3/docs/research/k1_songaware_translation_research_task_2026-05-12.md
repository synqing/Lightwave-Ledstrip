# K1 Song-Aware Translation Research Task — 2026-05-12

## What was observed

Captain reported a strong visual effect: current playback feels section-aware and like the K1 is “choosing” suitable visuals by itself.

## Confirmed evidence (from existing audit pass and current source checks)

- This behaviour is currently **not** caused by automatic effect-ID switching.
- Existing logic already supports dynamic per-frame/per-control adaptation from audio:
  - `AudioBehaviorSelector` drives narrative phase (`REST`, `BUILD`, `HOLD`, `RELEASE`) from DSP fields.
  - `NarrativeEngine` drives temporal modulation, not direct effect selection.
  - `ShowDirectorActor` and `TRINITY_SEGMENT` adjust show/narrative state, not explicit `SET_EFFECT` sequencing.
- Therefore, the observed behaviour is most likely:
  - effect-local audio reactivity,
  - effect-parameter modulation,
  - narrative-state modulation,
  - and not true section-to-effect auto-switching at runtime.

## Research goal

Convert this observation into a real feature/function that is product-safe and auditable.

**Question:** is the target feature
1) "Automatic effect switching by section/energy/liveliness" (new control-plane feature), or
2) "Song-aware behaviour mode" that upgrades existing narrative/audio-behaviour adaptation only.

## Executing SSA angles

Multiple SSAs were run in parallel to cover architecture, product definition, risk, and validation:

- Lane A (Franklin): architecture + control-plane feasibility.
- Lane B (Peirce): product function spec + control surface.
- Lane C (Nash): risk and rollout safety.
- Lane D (McClintock): evidence protocol + validation gates.

### Notable lane outputs

- Lane A: No music-structure-driven effect-ID switching exists today in the inspected paths; only command/show-cue routes can change effects.
- Lane B: proposes an explicit feature surface: `songAware.mode`, confidence/hold/switch cooldown controls, family allow/prohibit lists, and silence fallback policy.
- Lane C: mandates no render-path switching logic and strict gates (decision-rate caps, cooldown/hold, manual/show ownership, hard rollback).
- Lane D: created `firmware-v3/docs/research/k1_songaware_validation_protocol_laneD_2026-05-12.md` with pass/fail telemetry and wrong-switch/stability metrics.

## Recommended onward research task (documentation-only)

## 1) Define product target (decision-first)

Decide one of three states before any implementation:

- **A. Song-aware parameter mode only**
  - Keep existing effect IDs fixed.
  - Formalise narrative/audio-behaviour mapping as a user-visible mode.

- **B. Constrained effect-switching mode**
  - Add explicit, safe auto-switch policy with small effect-family set and strict cooldown/hold.
  - Keep as opt-in and disabled by default.

- **C. Defer effect-switching**
  - Keep adaptive behaviour as-is until stronger music-structure signal is proven.

## 2) Build the feature contract map (before code)

- Create one matrix mapping:
  - observed classes (ambient/build/drop/breakdown/dense/silence),
  - approved effect families,
  - approved fallback visual for silence.
- Add explicit ownership rules:
  - manual control wins,
  - show/cue mode wins,
  - song-aware mode applies only when no conflicting ownership.

## 3) Measurement-first proof task

Use Lane D protocol as controlled evidence baseline:

- 3 tracks × 2–4 min, annotated segment logs,
- fixed runtime control set (brightness 160, speed 27, intensity 128, saturation 128, complexity 128, variation 0, palette 10, EdgeMixer MIRROR),
- capture command loop at 1 Hz:
  - `vp stack`, `s`, `dbg memory`, `adbg status`, `dbg status`,
- compute:
  - wrong-switch rate,
  - dwell-time stability,
  - runtime health gates (no show-skips/RMT errors/underruns),
  - user-fit table.

## 4) Output decision

Research is complete when:

- evidence either proves a true auto-switch gap,
- or formally closes feature as "song-aware parameter mode only",
- and one of the two design tracks below is approved by Captain.

## 5) Deferred items (explicit)

No firmware code changes in this phase.
No VP substrate patch, no medium layer, no timing work, no implementation.

### Next research owners (non-overlapping)

- Architecture owner: validate switch/control plane once decision is made.
- Product owner: lock user-visible controls and defaults.
- Stability owner: define gate thresholds and rollback triggers.
- Measurement owner: complete evidence matrix from protocol and report PASS/FAIL.
