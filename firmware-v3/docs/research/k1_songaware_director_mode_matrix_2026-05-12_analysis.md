# K1 Song-Aware Director Mode Matrix Analysis - 2026-05-12

RBDO label: DEGRADED-MODE.

## Calibration Debt

- **Unresolved assumption:** State thresholds are not calibrated from live track captures in this pass.
- **Risk if wrong:** The director could classify sections too early, too late, or with low confidence.
- **Fallback:** Use the matrix only as bounded product intent for parameter-only validation; do not switch effects.
- **Revisit trigger:** Lane D captures at least three annotated tracks and proves state fit without health regression.
- **Debt count / affected outputs:** 2 outputs: this matrix and the decision record.

## Evidence Boundary

No firmware/source edits were made. The matrix below is a research contract for visual intent, not production tuning.

Grounding:

- Required state set: silence, ambient, steady, build, drop, breakdown, dense, transition (`firmware-v3/docs/research/k1_song_aware_director_agent_execution_plan_2026-05-12.md:71-84`).
- Fixed evidence controls: brightness `160`, speed `27`, intensity `128`, saturation `128`, complexity `128`, variation `0`, palette `10` / Vintage 01, EdgeMixer MIRROR (`firmware-v3/docs/research/k1_song_aware_director_agent_execution_plan_2026-05-12.md:147-158`).
- Validation requires baseline/family/switching comparisons with health and no-degradation gates (`firmware-v3/docs/research/k1_songaware_validation_protocol_laneD_2026-05-12.md:37-58`, `:92-171`).
- All K1 effects must remain centre-origin and avoid rainbow/full hue-wheel behaviours per repo hard constraints (`CLAUDE.md:292-299`).

## Interpretation

The product signal is "intentional continuity through adaptive behaviour inside a stable context", not effect-ID churn. That interpretation is explicitly stated in the execution plan (`firmware-v3/docs/research/k1_song_aware_director_agent_execution_plan_2026-05-12.md:27-38`) and the translation task (`firmware-v3/docs/research/k1_songaware_translation_research_task_2026-05-12.md:7-18`).

## State To Visual Intent Matrix

Bands are runtime comparison bands around the fixed baseline. They are not production defaults and must not be persisted.

| Song state | Brightness band | Speed band | Intensity band | Complexity band | Spatial / density intent | Motion quality | Forbidden behaviours | Transition safety |
|---|---:|---:|---:|---:|---|---|---|---|
| silence | 56-88 | 8-16 | 32-72 | 24-64 | Narrow centre glow, low density. | Slow breathing, idle-alive posture. | Hard black unless explicitly selected, rainbow cycling, effect switch. | Enter only after sustained low confidence; exit through ambient or build. |
| ambient | 88-128 | 12-22 | 56-96 | 48-88 | Soft centre-out spread, sparse texture. | Drifting, low contrast. | Strobes, sharp wipes, palette churn. | Minimum dwell target 8 s. |
| steady | 128-160 | 20-32 | 96-144 | 96-144 | Balanced field with readable pulse. | Stable flow or beat-aware pulse. | Frantic modulation, automatic switching. | Hold unless confidence rises or falls for at least 4 s. |
| build | 144-176 | 28-44 | 128-184 | 128-176 | Widening centre-out spread, increasing density. | Accelerating, rising pressure. | Premature drop posture, linear sweep. | Ramp only; no effect-ID change. |
| drop | 168-200 | 36-60 | 168-224 | 144-200 | Full centre-out impact with clear edge response. | Decisive pulse, collision, or bloom. | Random chaos, rainbow, default change. | Enter only on high confidence; cooldown target at least 8 s. |
| breakdown | 96-136 | 12-26 | 64-112 | 48-96 | Reduced density, exposed centre. | Suspended, clean decay. | Busy texture, high-speed motion. | Release from drop/dense; avoid immediate re-drop. |
| dense | 152-184 | 32-50 | 152-208 | 176-224 | High fill while retaining centre-origin structure. | Layered but legible. | Over-saturation, uncontrolled randomness, health regression. | Cap complexity if FPS, show timing, or health counters degrade. |
| transition | 112-168 | 18-36 | 80-160 | 80-144 | Bridge posture, not a destination. | Smoothing/crossfade posture. | Abrupt effect-ID churn, NVS save, AP/STA changes. | Temporary state; must resolve under dwell/cooldown policy. |

## Transition Rules

| Rule | Requirement |
|---|---|
| Ownership | show > manual > director. Show cue execution can change effects, parameters, transitions, narrative, and palettes (`firmware-v3/docs/reference/fsm-reference.md:112-125`). |
| Dwell | Minimum 4 s for any automatic family/switching test; higher for ambient/silence states. |
| Cooldown | Any future family morph or constrained switch must cool down before a second automatic movement. |
| Confidence | Low-confidence state becomes hold/manual or ambient fallback, not switching. |
| Health | Any show skip, failure, RMT error, or underrun fails the run (`firmware-v3/docs/research/k1_songaware_validation_protocol_laneD_2026-05-12.md:113-117`). |
| Centre origin | All visual posture must read from LED 79/80 outward or inward. |
| Colour | No rainbow/full hue-wheel sweep; palette 10 / Vintage 01 remains fixed for baseline captures. |

## Mode Implication

This matrix supports **parameter-only mode first**. It can later support family morphing if Lane D proves stability and user fit. It does not justify constrained effect switching by itself.
