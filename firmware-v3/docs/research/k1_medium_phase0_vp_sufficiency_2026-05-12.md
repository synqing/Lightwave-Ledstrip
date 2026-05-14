# K1 Medium Programme Phase 0/1 VP Surface Sufficiency Report - 2026-05-12

RBDO label: DEGRADED-MODE for medium-layer readiness, GROUNDED for current VP stack capabilities.

This report answers whether the current Visual Pipeline substrate is sufficient to plan an optical/medium layer. It does not implement VP substrate changes.

## Current VP Reporting

The current `vp stack` command reports:

- effect ID/name and palette ID/name;
- control state: brightness, speed, intensity, saturation, complexity, variation, hue, mood;
- topology, authored surface, correction surface, output surface, and mismatch;
- colour-correction toggle, per-effect skip, apply/skip counts;
- tone-map active/bypassed;
- split/converge destination;
- global silence state, hard-gate effect flag, silent scale, audio availability;
- EdgeMixer mode, spatial mode, temporal mode, spread, strength;
- LED show stats: expected wire time, show skips, failures, RMT errors, underruns;
- timing/load and capture state.

Source anchors:

- `firmware-v3/src/core/diagnostics/VpStackIntrospection.h:8-91`
- `firmware-v3/src/serial/SerialCLI.cpp:130-248`
- `firmware-v3/src/core/actors/RendererActor.cpp:1272-1338`

## Required Answers

| Question | Answer | Evidence / limitation |
|---|---|---|
| Does every required effect report authored surface? | Per active runtime sample, yes: `vp stack` reports the current authored surface. For source-only planning, all registered required candidates inspected here are expected to author `m_leds` in normal single-effect mode. `0x1903` is quarantined and not currently registered, so it cannot produce a valid current sample. | Surface is derived from renderer topology and `m_effectContext.dualChannelMode`, not from a per-effect declared authored-surface field. |
| Do direct-strip effects report accurately? | Partially. Direct-strip mode is represented as `physical_strips`, and a correction mismatch is reported if colour correction applies to `m_leds` while authored output is physical strips. | The model cannot express a physical-strip correction surface because `VpCorrectionSurface` currently has only `none` and `m_leds`. It detects mismatch; it does not provide a complete medium insertion contract. |
| Can colour-correction skip status be queried per effect? | Yes for the active effect. `vp stack` prints `skipped_by_effect`, and `RendererActor` uses `PatternRegistry::shouldSkipColorCorrection()`. | The reason for skip is not machine-readable in the sample. The audit has to map it back to family/tag/source rules. |
| Can tone-map status be queried per effect? | Yes for the active effect. `vp stack` prints tone map active/bypassed from `needsToneMap()`. | Tone-map reason is not machine-readable; current list is explicit ID policy. |
| Can silence policy be queried per effect? | Partially. The sample reports global silence activity, bypass flag, `hard_gate_effect`, `silent_scale`, and audio availability. | It does not report product silence state such as Reactive Active, Reactive Release, Idle Alive, Standby Premium, or Hard Black. It also gates `hard_gate_effect` through `needsSilenceGate(id) && isAudioReactive(id)`, so source-audio effects missing from `isAudioReactive()` can appear non-hard-gated. |
| Can EdgeMixer state be captured with each visual sample? | Yes. `vp stack` reports mode, spatial, temporal, spread, and strength. | Baseline must force MIRROR for substrate isolation. Product/tetradic state should be a comparison pass only. |
| Can medium-layer eligibility be derived from existing metadata? | No. Current family/tags, colour skip, tone-map, silence, and surface fields are enough to identify risk, not enough to safely decide medium pass eligibility. | Required candidates are mostly effect-owned optical/math/physics. A future layer needs explicit opt-in/opt-out, not inference from family names. |
| Is a new `MediumPolicy` enum required? | Yes for implementation, but not in this Phase 0/1 audit. | Existing metadata lacks a direct contract for corrective-only, creative, effect-owned, diagnostic-only, and never-apply policies. |

## Minimum Metadata / Policy Gaps

The following gaps must close before any optical/medium layer is implemented:

1. Authored-surface declaration:
   Runtime derivation is useful, but a medium layer needs a clear per-sample contract for the actual buffer it will modify.

2. Medium surface:
   Current VP surfaces model authored, correction, and output surfaces. They do not model the planned medium transform surface.

3. Physical-strip correction capability:
   `VpCorrectionSurface` cannot represent correction on `physical_strips`. That is acceptable for reporting current behaviour, but incomplete for future insertion planning.

4. Medium eligibility:
   Family/tags are insufficient. Existing candidates that already implement optical/chromatic/math behaviour should not be globally reprocessed without a declared policy.

5. Skip reason reporting:
   Colour-correction skip is reported as a boolean, but a medium layer needs to know whether the effect was skipped because of LGP sensitivity, mathematical mapping, previous-frame state, physics amplitude, or explicit hardcoded ID.

6. Source-audio versus registry-audio mismatch:
   Several `0x1Bxx` effects consume `ctx.audio` directly, but `PatternRegistry::isAudioReactive()` does not list them. This affects silence policy and future adaptive-medium gating.

7. Stateful/self-trailing declaration:
   Effects such as `0x0204`, `0x0603`, `0x1B00`, and variants use fading/history/persistence internally, but required candidates do not expose a medium-safe statefulness policy.

8. Silence product states:
   Current reporting exposes global and hard-gate mechanics, not product-level states. Medium or idle work needs explicit states before implementation.

9. Hardware evidence linkage:
   `vp stack` is sufficient for serial evidence capture, but no repository object currently enforces the required evidence filename, fixed controls, and Captain visual verdict fields.

## VP Sufficiency Decision

Current VP metadata is sufficient for Phase 0/1 audit planning and serial evidence capture.

Current VP metadata is not sufficient to implement a global optical/medium layer safely.

The smallest next substrate decision is not a render-path patch. It is a metadata design decision: define the future policy fields and sample contract, then validate them against the required candidate set before any medium transform is written.
