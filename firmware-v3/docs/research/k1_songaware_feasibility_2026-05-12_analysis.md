# K1 Song-Aware Director Signal Feasibility Analysis - 2026-05-12

RBDO label: DEGRADED-MODE.

## Calibration Debt

- **Unresolved assumption:** This orchestrating Codex session could not complete live clangd semantic confirmation after `clangd/diagnostics` returned `Transport closed`. Lane A SSA reported a separate successful clangd diagnostic on `ControlBus.h`, but this file treats source-level symbol detail as secondary to the current reference docs and protocol contracts.
- **Risk if wrong:** A future implementation could bind director state to a field that is stale, unpopulated, or more expensive than the docs imply.
- **Fallback:** Keep Song-Aware Director v1 as parameter-only and drive it from the documented high-level fields already exposed in the audio reference and contracts.
- **Revisit trigger:** Fresh Codex session with working clangd MCP, or live Lane D capture proving feature freshness and stability.
- **Debt count / affected outputs:** 3 outputs: this feasibility file, the mode matrix, and the decision record.

## Source Boundary

This is an evidence-only feasibility handoff. It did not modify firmware, VP substrate, timing, medium layers, NVS, or production defaults.

Mandatory source reads completed:

- `firmware-v3/docs/research/k1_song_aware_director_agent_execution_plan_2026-05-12.md`
- `firmware-v3/docs/research/k1_songaware_validation_protocol_laneD_2026-05-12.md`
- `firmware-v3/docs/research/k1_songaware_translation_research_task_2026-05-12.md`
- `firmware-v3/docs/research/k1_medium_phase0_decision_record_2026-05-12.md`
- `firmware-v3/docs/reference/codebase-map.md`
- `firmware-v3/docs/reference/fsm-reference.md`
- `docs/protocol/k1-ws-contract.yaml`
- `docs/protocol/k1-rest-contract.yaml`

Note: the prompt listed `docs/reference/codebase-map.md` and `docs/reference/fsm-reference.md`, but those root paths do not exist in this checkout. The current repo instruction points to `firmware-v3/docs/reference/...`; those files were read instead.

## Grounded Baseline Facts

- The execution plan forbids starting with autonomous effect-ID switching and orders work as parameter mode, family morphing, then constrained switching only after validation (`firmware-v3/docs/research/k1_song_aware_director_agent_execution_plan_2026-05-12.md:9-16`).
- The translation task states the observed behaviour is not confirmed automatic effect-ID switching and is more likely effect-local audio reactivity, effect-parameter modulation, and narrative-state modulation (`firmware-v3/docs/research/k1_songaware_translation_research_task_2026-05-12.md:7-18`).
- The production audio chain is documented as Microphone > I2S DMA > AudioActor > ESV11 Backend > EsV11Adapter > ControlBusFrame > Effects; ESV11 is production, PipelineCore is broken (`firmware-v3/docs/reference/audio-pipeline-parameters.md:10-15`).
- The documented input stage is 32 kHz with 128-sample DMA chunks, giving a 4 ms chunk granularity (`firmware-v3/docs/reference/audio-pipeline-parameters.md:16-25`).
- Audio hop identity and staleness can be reasoned about from `t` and `hop_seq` (`firmware-v3/docs/reference/audio-pipeline-parameters.md:219-224`).

## Director Signal Inventory

| Signal | Readiness | Evidence | Director use | Failure mode |
|---|---:|---|---|---|
| RMS / fast RMS | High | ESV11 maps `vu_level` to `frame.rms` and `frame.fast_rms`; field list documents both as 0..1 (`firmware-v3/docs/reference/audio-pipeline-parameters.md:110-115`, `:225-230`). | Silence, energy floor, build/drop intensity scaling. | Mic/noise floor can look active; must be gated by confidence and silence state. |
| Flux / fast flux | High | ESV11 maps novelty to `frame.flux` and `frame.fast_flux`; liveliness combines tempo confidence and fast flux (`firmware-v3/docs/reference/audio-pipeline-parameters.md:112-115`, `:172-175`). | Build/transition pressure, density change. | Flux alone cannot prove song section identity. |
| Onset/percussion | Medium-high | Snare and hi-hat energy/trigger mappings are documented (`firmware-v3/docs/reference/audio-pipeline-parameters.md:144-148`, `:300-304`). | Drop/transition accent, density estimate, short-term confidence boost. | Better as event evidence than as a continuous state classifier. |
| Tempo/confidence/beat phase | Medium | ESV11 maps BPM, tempo confidence, beat tick, strength, phase, beat-in-bar, and downbeat (`firmware-v3/docs/reference/audio-pipeline-parameters.md:150-157`, `:314-329`). Runtime tempo tuning has confidence floors and hold rules (`firmware-v3/docs/reference/audio-pipeline-parameters.md:83-101`). | Beat-locked motion and confidence gating. | Tempo may stabilise late, alias, or freeze under low confidence. |
| Silence / active confidence | High for fallback | Silence detection exposes `silentScale` and `isSilent`; docs require `silentScale` as multiplier (`firmware-v3/docs/reference/audio-pipeline-parameters.md:199-203`, `:336-338`, `:351-358`). | Safe fallback, hold/manual policy, ambient/silence boundary. | Do not branch hard on silence alone; use fade/hold policy. |
| Narrative phase | Medium | Reference FSM defines REST, BUILD, HOLD, RELEASE from RMS/flux/downbeat-like conditions (`firmware-v3/docs/reference/fsm-reference.md:187-199`). | Initial prior for state matrix. | This is not evidence of auto effect selection. |
| Saliency / novelty | Medium | Harmonic, rhythmic, timbral, and dynamic novelty plus smoothed fields are documented; effects are told to use smoothed saliency (`firmware-v3/docs/reference/audio-pipeline-parameters.md:177-188`, `:263-280`, `:351-354`). | Transition and section-change proxy. | Needs Lane D capture to prove stability on real tracks. |
| Style / density proxy | Low-medium | `currentStyle` and `styleConfidence` are documented (`firmware-v3/docs/reference/audio-pipeline-parameters.md:190-192`, `:282-284`). | Advisory label only. | Do not use as an authoritative state without confidence evidence. |
| Scene parameters | Medium, if enabled | Scene parameters include motion type, brightness scale, motion rate/depth, phrase progress, transition progress, and timing reliability (`firmware-v3/docs/reference/audio-pipeline-parameters.md:205-218`, `:286-298`). | Possible parameter-mode surface. | Feature-gated; must be verified before product reliance. |
| STM | Optional/gated | REST exposes `/api/v1/audio/stm` with ready, sequence, 42 spectral bins, 16 temporal bands, and derived metrics (`docs/protocol/k1-rest-contract.yaml:251-269`). WS exposes STM streaming at 30 FPS (`docs/protocol/k1-ws-contract.yaml:1104-1116`). | Optional density/timbre context if `ready` and fresh. | Runtime cost and extractor semantics are not validated here. |

## Freshness And Latency Model

Minimum freshness gates for any later implementation:

| Gate | Rule |
|---|---|
| Fresh audio | `hop_seq` advances and audio timestamp monotonicity holds. |
| Confidence | Tempo/style/saliency-derived states require confidence above a documented floor; no numeric floor is calibrated in this pass. |
| Silence | Silence lowers output through `silentScale`/fallback policy, not abrupt effect churn. |
| STM | Use only when REST/WS reports ready and sequence changes; otherwise ignore. |
| Latency | No director decision may add render-path work; Lane D must show no runtime health regression. |

The audio lattice investigation records a prior hard budget crisis: p99 `audio_hop_us` was measured at 18.230 ms versus an 8 ms budget, and the conclusion was to fix pipeline headroom before expanding feature complexity (`firmware-v3/docs/research/AUDIO_LATTICE_CONFIGURATION_INVESTIGATION.md:20-26`, `:62-83`). This does not prohibit parameter-mode research, but it blocks speculative DSP growth.

## Minimal V1 Feature Set

Recommended v1 inputs for Song-Aware Parameter Mode:

- RMS / fast RMS.
- Flux / fast flux.
- `silentScale` / `isSilent`.
- Tempo confidence, beat tick, beat strength, beat phase.
- Smoothed saliency fields.
- Narrative REST/BUILD/HOLD/RELEASE as a coarse prior.

Gated or deferred:

- STM as optional only, behind `ready` and freshness.
- Style labels as advisory only until Lane D proves useful confidence.
- Any signal requiring new DSP, new endpoints, or render-path work.

## Lane A Decision

Signal feasibility is sufficient for **Song-Aware Parameter Mode research**.

Signal feasibility is not sufficient to claim proven automatic effect switching. Family morphing and constrained switching remain validation-gated, not implementation-ready.
