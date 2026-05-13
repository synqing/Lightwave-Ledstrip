# K1 Song-Aware Director — New Agent Execution Plan

## Objective

Turn Captain’s observed "K1 sounds musically aware" behaviour into a production-safe feature direction without assuming automatic effect-ID switching.

**Primary outcome:** a validated **Song-Aware Director** concept that first improves visual intent via parameter/family-state modulation, then only conditionally enables constrained effect switching.

## Strategic Position (hard decision)

Do **not** start with autonomous effect-ID switching. Ship in this order:

1. **Song-Aware Parameter Mode** (no effect-ID churn)
2. **Family Morphing** (in-family visual posture shifts)
3. **Constrained Effect Switching** (only if validated and materially better than phase 2)

## Scope Boundaries

- Evidence-only planning and protocol work for this agent.
- No firmware code changes.
- No VP substrate patching.
- No medium-layer implementation.
- No timing/cadence changes.
- No NVS saves.
- No hardware command that changes production defaults.

## Core Hypothesis

The user-value signal is likely:

- not "auto-switching",
- but **intentional continuity through adaptive behaviour inside a stable context**.

If this is true, perceived musical intent can be delivered by:
- song-state classification,
- visual-intent mapping,
- controlled parameter and family-posture modulation,
- strict ownership and stability governance.

## Required inputs (must be loaded/read)

- Existing phase 0/1 audit outputs:
  - [k1_medium_phase0_candidate_audit_2026-05-12.md](./k1_medium_phase0_candidate_audit_2026-05-12.md)
  - [k1_medium_phase0_vp_sufficiency_2026-05-12.md](./k1_medium_phase0_vp_sufficiency_2026-05-12.md)
  - [k1_medium_phase0_decision_record_2026-05-12.md](./k1_medium_phase0_decision_record_2026-05-12.md)
  - [k1_medium_phase0_captain_review_table_2026-05-12.md](./k1_medium_phase0_captain_review_table_2026-05-12.md)
- Lane D validation protocol:
  - [k1_songaware_validation_protocol_laneD_2026-05-12.md](./k1_songaware_validation_protocol_laneD_2026-05-12.md)

## Deliverables

1. A concrete **Song-Aware Director design brief** (documented, no code).
2. A **state-to-visual-intent matrix**.
3. A **control-plane decision policy** for ownership, confidence, dwell, cooldown, and rollback.
4. A **comparative evidence protocol** measuring:
   - baseline fixed-effect,
   - song-aware parameter mode,
   - family morphing prototype,
   - constrained switching prototype (only if approved by safety review).
5. A final **Go/No-Go** and phase-gate decision.

## Proposed execution lanes (parallel SSAs)

### Lane A — Signal Feasibility and Classifier
**Lead question:** What local DSP features can reliably drive director state?

- Confirm availability and update-rate of: RMS, flux, onset strength, tempo confidence, silence, novelty/segment-change proxy, density estimate, energy slope.
- Define minimal v1 feature set + failure modes.
- Output: `Lane A Report` with feature confidence and latency budget.

### Lane B — Song State + Visual Intent Contract
**Lead question:** What are the director states and their allowed visual postures?

- Define state set:
  - silence, ambient, steady, build, drop, breakdown, dense, transition.
- For each state define intent bands:
  - brightness envelope,
  - speed range,
  - complexity range,
  - density,
  - spatial spread,
  - motion quality,
  - forbidden behaviours.
- Output: matrix and rulebook.

### Lane C — Product Surface and Control Model
**Lead question:** What user-facing controls are needed?

- Define mode schema (Off / Subtle / Balanced / High Energy / Song-Aware Director).
- Define tunables:
  - enabled,
  - sensitivity,
  - intensity,
  - motion range,
  - silence policy,
  - minimum dwell / cooldown,
  - confidence floor,
  - allowed families.
- Include precedence model:
  - manual override wins,
  - show/cue mode wins,
  - director is opt-in.
- Output: API/control model and default set.

### Lane D — Validation Protocol Extension
**Lead question:** Which mode gives better intentionality: parameter mode, family morphing, constrained switching?

- Extend Lane D protocol to compare 3 conditions:
  1) fixed control baseline,
  2) song-aware parameter mode,
  3) constrained switching (prototype only).
- Add decision rule:
  - if parameter-only mode is equal-or-better than constrained switching on user-fit and stability, switching is deferred.
- Add metrics:
  - wrong-switch rate (only for switching path),
  - dwell and thrash,
  - health hard-gates,
  - section match,
  - deliberate transition score,
  - motion clarity,
  - mean fit.
- Output: evidence protocol and PASS/FAIL template.

### Lane E — Rollout Safety and Stop Conditions
**Lead question:** What allows safe phased release?

- Define exact hard gates:
  - no render-path decision logic,
  - decision loop outside render hot path,
  - cooldown + dwell floors,
  - confidence threshold,
  - kill switch to manual hold,
  - telemetry health stop conditions.
- Define rollback plan and precondition checks.
- Output: risk register + rollout gating checklists.

## Required evidence artifact naming

Create (and keep separate) research files:

- `k1_songaware_director_protocol_<date>_serial.md`
- `k1_songaware_condition_matrix_<date>_analysis.md`
- `k1_songaware_validation_track_<trackid>_<date>_serial.md`
- `k1_songaware_risk_gate_<date>.md`
- `k1_songaware_decision_record_<date>.md`

## Fixed runtime capture controls for all sessions

- brightness 160
- speed 27
- intensity 128
- saturation 128
- complexity 128
- variation 0
- palette 10 / Vintage 01
- EdgeMixer MIRROR

Record exact control state in every file with command sequences and timestamps.

## Decision gates

### Gate 1 — Director design readiness
- Song state model is complete.
- Visual intent mapping is deterministic and bounded.
- Ownership precedence is explicit.

### Gate 2 — Parameter mode viability
- Protocol test proves parameter mode gives stronger or equal intentionality versus baseline.
- No runtime health regressions in test windows.

### Gate 3 — Family morphing readiness
- Only if parameter mode passes and morph transitions are stable within allowed dwell/cooldown.
- No chaos/jarring regressions vs fixed effect.

### Gate 4 — Constrained switching approval
- Only if:
  - wrong-switch rate ≤ 20%,
  - stability gate passes,
  - health gate passes,
  - switching-user fit strictly beats parameter mode by predefined margin.
- Otherwise defer switching.

## Exit criteria

This research task ends when we have:

1. A **Song-Aware Director RFC** (state, policy, control model).
2. A **comparative decision** selecting one of:
   - Acknowledge director as parameter-only v1,
   - Extend to family morphing,
   - Defer further switching work.
3. A safe, explicit test pack and rollout condition package for the next engineering cycle.

## Note for receiving agent

Treat this as a product-definition, evidence-first lane handover. Do not implement until all gates above are met and Captain signs the decision record.
