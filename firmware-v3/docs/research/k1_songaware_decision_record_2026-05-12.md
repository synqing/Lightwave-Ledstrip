# K1 Song-Aware Director Decision Record - 2026-05-12

RBDO label: DEGRADED-MODE.

## Calibration Debt

- **Unresolved assumption:** No live A/B/C/D comparison was run in this handoff, and this orchestrating Codex session could not use clangd after `Transport closed`.
- **Risk if wrong:** The recommendation could overstate current source or runtime readiness for director behaviour.
- **Fallback:** Treat this record as a research decision only: validate parameter mode first, keep family morphing and switching disabled, and require fresh runtime evidence before shipping product behaviour.
- **Revisit trigger:** Lane D evidence files exist for at least three annotated tracks and fresh source confirmation is available in a working clangd session.
- **Debt count / affected outputs:** 1 output: this decision record.

## Decision

Proceed with **Song-Aware Parameter Mode as the only defensible next candidate**.

Do **not** ship autonomous or constrained effect switching from the current evidence. Family morphing may be evaluated only after parameter mode passes Lane D health/no-degradation gates. Constrained switching remains deferred until it is proven materially better than parameter/family modes and stable under the Lane D wrong-switch, dwell, thrash, and health gates.

## Upstream Facts That Make This Decidable

| Fact | Evidence |
|---|---|
| The approved strategic order is parameter mode, then family morphing, then constrained switching only if validated. | `firmware-v3/docs/research/k1_song_aware_director_agent_execution_plan_2026-05-12.md:9-16` |
| The observed current behaviour is not confirmed automatic effect-ID switching; it is more likely effect-local audio reactivity, parameter modulation, and narrative-state modulation. | `firmware-v3/docs/research/k1_songaware_translation_research_task_2026-05-12.md:7-18` |
| Lane D requires fixed controls, A/B/C/D comparison, no-degradation checks, and health gates before any claim. | `firmware-v3/docs/research/k1_songaware_validation_protocol_laneD_2026-05-12.md:24-36`, `:49-58`, `:127-171` |
| Current required Lane D evidence files are not present, and no live comparison was run in this handoff. | `firmware-v3/docs/research/k1_songaware_validation_protocol_laneD_2026-05-12.md:16-23`; see `k1_songaware_validation_protocol_laneD_2026-05-12_runlog.md`. |
| Current contracts expose parameter, effect, narrative, show, stimulus, audio, status, and EdgeMixer surfaces, but no `songAware.*` endpoint. | `docs/protocol/k1-rest-contract.yaml:96-166`, `:222-269`, `:388-402`, `:601-638`, `:829-843`; `docs/protocol/k1-ws-contract.yaml:115-293`, `:1290-1424`, `:1499-1544`, `:1673-1700`, `:1930-1962` |
| The audio reference exposes enough high-level fields for parameter-mode research: RMS/flux, tempo, silence, saliency, style, scene parameters. | `firmware-v3/docs/reference/audio-pipeline-parameters.md:110-115`, `:150-157`, `:177-203`, `:205-230`, `:263-298` |
| Medium/VP work is not authorised here. | `firmware-v3/docs/research/k1_medium_phase0_decision_record_2026-05-12.md:5-12`, `:116-146` |

## What The Observed Behaviour Likely Is Today

The current "K1 feels song-aware" observation is most likely:

- effect-local audio reactivity;
- global or effect parameter modulation;
- narrative REST/BUILD/HOLD/RELEASE modulation;
- existing visual continuity inside a stable effect context.

It is **not proven** to be automatic effect-ID switching.

## Is Auto Effect Switching Proven?

No.

Reasons:

- The translation research says current evidence does not show automatic effect-ID switching (`firmware-v3/docs/research/k1_songaware_translation_research_task_2026-05-12.md:7-18`).
- Lane D requires wrong-switch, dwell, thrash, no-degradation, runtime health, and user-fit evidence before switching can be claimed (`firmware-v3/docs/research/k1_songaware_validation_protocol_laneD_2026-05-12.md:92-171`).
- The runlog for this handoff marks A/B/C/D live comparison as not run.
- The contracts expose effect-setting commands, but a command surface is not evidence of autonomous song-state switching (`docs/protocol/k1-ws-contract.yaml:115-130`; `docs/protocol/k1-rest-contract.yaml:96-115`).

## Recommended Phase Decision

| Phase | Decision | Reason |
|---|---|---|
| Song-aware parameter mode | Proceed to validation candidate. | Enough documented signals and existing contract surfaces exist for parameter-only research without effect-ID churn. |
| Family morphing | Defer until parameter mode passes. | Needs stable state matrix, family map, and no-degradation evidence. |
| Constrained effect switching | Defer. | Not proven, not run, and only valid if it strictly beats parameter/family modes under Lane D. |

## Required Evidence Before Product Claim

1. Build ground-truth segment CSVs for at least three 2-4 minute tracks.
2. Capture fixed-control Condition A baseline.
3. Capture Condition B parameter mode with zero automatic switches.
4. Only if B passes, capture Condition C family morphing.
5. Only if C passes and runtime support exists, capture Condition D constrained switching.
6. Fill wrong-switch, switch-rate, dwell, thrash, health, no-degradation, parameter activity, and user-fit tables.
7. Keep D as NOT RUN / NOT ENABLED unless switching support is explicitly evidenced.

## Explicit Deferred List

- Firmware implementation logic.
- Protocol endpoint additions.
- VP substrate patch.
- Medium layer or `MediumPolicy` code.
- Timing/cadence work.
- NVS saves or production default changes.
- New DSP expansion.
- Automatic effect-ID switching.
- Constrained switching without Lane D PASS.
- Visual sign-off claims without Captain review.

## Stop Condition

This decision record completes the evidence handoff. No implementation handoff is authorised by this file.
