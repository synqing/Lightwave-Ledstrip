# K1 Song-Aware Director Rollout Safety - 2026-05-12

RBDO label: DEGRADED-MODE.

## Calibration Debt

- **Unresolved assumption:** A numeric classifier confidence floor is not calibrated by the mandatory inputs.
- **Risk if wrong:** Director behaviour could adapt or switch on weak or stale song-state evidence.
- **Fallback:** Hold current manual/effect state; allow parameter-only evidence runs; keep family morphing and constrained switching disabled.
- **Revisit trigger:** Lane A publishes calibrated thresholds and Lane D captures PASS evidence against annotated tracks.
- **Debt count / affected outputs:** 2 outputs: this safety record and the decision record.

## Rollout Principle

Rollout must stay parameter-first and switching-last. The execution plan explicitly orders work as:

1. Song-Aware Parameter Mode.
2. Family Morphing.
3. Constrained Effect Switching, only if validated and materially better than phase 2.

Evidence: `firmware-v3/docs/research/k1_song_aware_director_agent_execution_plan_2026-05-12.md:9-16`.

## Ownership And Kill Switch Policy

| Owner | Precedence | Rule |
|---|---:|---|
| Show | 1 | Active show playback or cue injection owns the surface. |
| Manual | 2 | User command wins over director. |
| Director | 3 | Director acts only when enabled, opt-in, above confidence floor, outside dwell/cooldown, and health gates pass. |

Show/cue ownership is grounded because cue execution can change effects, parameters, zones, transitions, narrative, and palettes (`firmware-v3/docs/reference/fsm-reference.md:112-125`). The translation task also requires manual and show/cue ownership to win (`firmware-v3/docs/research/k1_songaware_translation_research_task_2026-05-12.md:61-70`).

Kill switch:

- Set effective mode to `off`.
- Set `familyMorphing=false`.
- Set `constrainedSwitching=false`.
- Preserve current active effect/manual state.
- Do not save to NVS.

## Rollback Procedure

| Step | Action | Persistence |
|---|---|---|
| 1 | Suppress director decisions. | Runtime only. |
| 2 | Hold current effect and parameters unless Captain requests reset. | Runtime only. |
| 3 | Re-apply fixed baseline controls only for validation capture. | Runtime only. |
| 4 | Capture `vp stack`, `s`, `dbg memory`, `adbg status`, `dbg status`. | Evidence file only. |
| 5 | Mark condition failed if any health counter regressed. | Evidence file only. |

Do not call save routes such as preset save, colour correction save, zone config save, or EdgeMixer save; those persistence surfaces exist and are out of scope (`docs/protocol/k1-rest-contract.yaml:509-540`, `:817-843`; `docs/protocol/k1-ws-contract.yaml:645-708`, `:1093-1102`).

## Dwell, Cooldown, Confidence Gates

| Gate | Minimum rule |
|---|---|
| Confidence | No automatic movement below the calibrated floor. Floor not yet calibrated; default research fallback is hold/manual. |
| Dwell | At least 4 s minimum between automatic family/switch transitions. |
| Cooldown | At least 8 s target after drop/family/switch actions until evidence tunes this. |
| Thrash | Any back-and-forth between the same pair inside 10 s is failure. |
| Switch rate | Any switching condition above 2 automatic switches/min fails. |

The 4 s dwell, zero-thrash, and 2 switches/min thresholds are grounded in the Lane D protocol (`firmware-v3/docs/research/k1_songaware_validation_protocol_laneD_2026-05-12.md:104-112`).

## Runtime Health Stop Conditions

Any non-zero value fails the running condition:

- show skips;
- failures;
- RMT errors;
- underruns.

Evidence: `firmware-v3/docs/research/k1_songaware_validation_protocol_laneD_2026-05-12.md:113-117`.

Additional K1 invariants:

- no render-path heap allocation;
- preserve 120 FPS / 2.0 ms effect ceiling;
- preserve sub-8 ms audio-to-visual latency;
- no AP/STA architecture changes;
- British English in comments/docs/logs/UI;
- no rainbow/full hue-wheel behaviour;
- centre-origin visual motion only.

## Phase Gate Table

| Gate | Opens | Required proof | Decision if not proven |
|---|---|---|---|
| Gate 1 | Director design readiness | Complete state model, bounded visual intent, explicit ownership. | Keep as research only. |
| Gate 2 | Parameter mode | B passes health/no-degradation and has useful parameter activity. | Do not ship director mode. |
| Gate 3 | Family morphing | Parameter mode already passes; C adds material fit without chaos or health regression. | Keep parameter-only. |
| Gate 4 | Constrained switching | D runs, C/D pass, wrong-switch <=20%, stability and health pass, and switching strictly beats parameter/family modes. | Defer switching. |

Evidence: gate definitions in the execution plan (`firmware-v3/docs/research/k1_song_aware_director_agent_execution_plan_2026-05-12.md:160-181`) and Lane D decision rule (`firmware-v3/docs/research/k1_songaware_validation_protocol_laneD_2026-05-12.md:158-171`).

## Go / No-Go Rules

| Candidate | Current status | Rule |
|---|---|---|
| Parameter mode | GO for validation protocol only. | No effect-ID movement; fixed baseline controls; no health regression. |
| Family morphing | NO-GO until B passes. | Requires bounded family map and stability evidence. |
| Constrained switching | NO-GO. | Requires runtime support and Lane D PASS; currently unproven. |
| Medium layer / VP substrate | NO-GO. | Current medium decision record explicitly defers global medium work and VP substrate patching (`firmware-v3/docs/research/k1_medium_phase0_decision_record_2026-05-12.md:5-12`, `:116-146`). |

## Explicit Deferred List

- Firmware feature logic.
- VP substrate patch.
- Medium layer or `MediumPolicy` code.
- Timing/cadence changes.
- NVS saves or production default changes.
- New DSP feature growth.
- Unconstrained or autonomous effect-ID switching.
- Constrained switching until Lane D proves material improvement and stability.
- Captain visual/product sign-off claims.

Stop condition: this is an evidence handoff only, not an implementation handoff.
