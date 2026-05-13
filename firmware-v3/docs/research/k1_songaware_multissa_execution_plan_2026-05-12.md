# K1 Song-Aware Director — Multi-SSA Execution Plan (Next Codex Agent)

Status: approved for execution.

## Captain-approved framing

We are not building auto effect-ID switching yet. We are building a product-safe
`Song-Aware Director` concept that uses stable effect IDs first, then (optionally)
constrains family-level changes, then only if validated adds constrained switching.

Reference outcomes from prior pass:

- Required optical/math candidate audit and VP decision are complete in:
  - `firmware-v3/docs/research/k1_medium_phase0_candidate_audit_2026-05-12.md`
  - `firmware-v3/docs/research/k1_medium_phase0_vp_sufficiency_2026-05-12.md`
  - `firmware-v3/docs/research/k1_medium_phase0_decision_record_2026-05-12.md`
- Song-aware translation hypothesis and lane findings are captured in:
  - `firmware-v3/docs/research/k1_songaware_translation_research_task_2026-05-12.md`
  - `firmware-v3/docs/research/k1_songaware_validation_protocol_laneD_2026-05-12.md`
  - `firmware-v3/docs/research/k1_song_aware_director_agent_execution_plan_2026-05-12.md`

Observed signal from Captain: behavior appears musically aware through
parameter/state modulation, not evidence-confirmed automatic effect-ID switching.

## Mission for new Codex agent

Produce a decision-ready research package that tells us whether Song-Aware
parameter mode, family morphing, or constrained switching is warranted.

## Non-negotiables

- No firmware source edits.
- No VP substrate patching.
- No adaptive medium layer, no medium-pass insertion.
- No timing/latency work.
- No production default changes.
- No NVS writes/saves.
- No effect-ID churn beyond controlled prototype gates.
- Evidence-only with explicit PASS/FAIL tables.

## Required runtime control baseline (for any capture in this task)

- brightness `160`
- speed `27`
- intensity `128`
- saturation `128`
- complexity `128`
- variation `0`
- palette `10` / `Vintage 01`
- EdgeMixer `MIRROR`
- fixed repeatable audio source per track

## Deliverables (must be committed as evidence files)

1. `k1_songaware_director_mode_matrix_<date>_analysis.md`
2. `k1_songaware_control_surface_schema_<date>.md`
3. `k1_songaware_rollout_safety_<date>.md`
4. `k1_songaware_validation_protocol_laneD_<date>_runlog.md`
5. `k1_songaware_decision_record_<date>.md`

## Multi-SSA execution lanes (parallel)

### Lane A — Signal / Feature Feasibility (Audio contract reality)

Goal: confirm which local DSP features can drive director state in-band, with
latency, freshness, and failure behaviour.

Tasks:

- confirm source-backed features in `AudioBehaviorSelector` and `EffectContext::AudioContext`
- define v1 director-feasible feature set + confidence model
- define failure/unknown audio states and fallback behavior
- return: lane report with feature readiness and risk notes

### Lane B — Song-state & Visual-intent Mapping

Goal: map song states to bounded, deterministic visual postures.

State set: `silence`, `ambient`, `steady`, `build`, `drop`, `breakdown`,
`dense`, `transition`.

For each state define:

- allowed effect families and fallback
- speed/intensity/complexity/motion/spread bands
- forbidden behaviors
- continuity/transition rules

Output: state→posture matrix and transition constraints.

### Lane C — Product Surface and Control Contract

Goal: define client-visible controls with no API rewrites.

Use existing control planes only:

- `narrative.config`
- `parameters`
- `effects`
- `shows`
- `stimulus`

Must include:

- mode model (`Off`, `Subtle`, `Balanced`, `High`, `Director`)
- tunables (`enabled`, `sensitivity`, `intensityScalar`, `motionScalar`,
  `silencePolicy`, `minDwellMs`, `switchCooldownMs`, `confidenceFloor`,
  `allowedFamilies`, `prohibitedFamilies`)
- ownership precedence (`show > manual > director`)
- computed state telemetry fields for Captain review

### Lane D — Validation Protocol and Comparative Evidence

Goal: run A/B/C(+D if switching exists) comparison and produce concrete PASS/FAIL.

Conditions:

1. Baseline fixed effect
2. Song-aware parameter mode
3. Family morphing
4. Constrained switching (prototype-only)

Must collect:

- `vp stack`, `s`, `dbg memory`
- effect/audio/silence state
- timing/load
- show skips, failures, RMT errors, underruns
- wrong-switch rate, dwell, thrash, stability

Decision rule to include:

- if parameter mode is equal/better than switching on intentionality and health,
  defer switching.

### Lane E — Rollout Safety, Stop-Rules, and Go/No-Go

Goal: prevent speculative implementation.

Output required:

- health kill-switch policy (render errors, latency, skips, RMT)
- rollback and override rules
- minimum dwell and cooldown policy
- confidence floor for any automatic behaviour
- explicit go/no-go checklist for phase-0, phase-1a, phase-1b

## Decision outputs required in final handover

1. Confirmed interpretation: observed behaviour is already largely
   effect-local/adaptive, not confirmed autonomous switching.
2. Recommended phase selection from:
   - keep parameter-only mode,
   - approve family morphing,
   - defer constrained switching,
   - or deny both until stronger evidence.
3. Medium-risk effects to avoid touching with global transforms in this phase.
4. Explicitly deferred work list.

## Exit condition for this task

Stop after:

- evidence files above are generated,
- Captain-visible decision record written,
- and no implementation/rewrite is started.

This handoff is research-only and can run while main/original workstream
continues.
