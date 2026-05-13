# VP Validation Protocol

**Date:** 2026-05-06
**RBDO label:** GROUNDED for the validation workflow; DEGRADED-MODE for any visual-default decision until a run report exists.
**Scope:** Required protocol before buffer-ownership correction, silence-policy metadata, or colour-correction default changes.

## Purpose

This protocol replaces the failed subjective two-unit timed A/B workflow. It uses serial frame capture, fixed baseline state, and explicit pass/fail gates before Captain is asked to judge a visible change.

The core model is the VP audit model: one frame lifecycle with a buffer-ownership fork, then a shared output path. The protocol tests the actual authored output surface and the final pre-WS2812 output; it does not assume `m_leds` is always the frame.

## Source Anchors

| Topic | Source |
|---|---|
| Frame lifecycle and risk queue | `firmware-v3/docs/audit/VP_RENDER_PATH_LAYER_AUDIT_2026-05-05.md` |
| Capture taps A/B/C | `docs/CAPTURE_PIPELINE_REFERENCE.md` |
| Fixed capture suites | `docs/CAPTURE_TEST_SUITES.md` |
| Dual-device constraints and serial-only rule | `docs/MULTI_WORKTREE_TESTING_GUIDE.md` |
| K1_Testbed calibration role | `SpectraSynq.K1_Testbed/CALIBRATION_README.md` and `SpectraSynq.K1_Testbed/WORKBENCH_PLAN.md` |
| Current C-1 mic-domain envelope | `firmware-v3/docs/research/c1_mic_domain_envelope_capture_2026-05-06.md` |

## Non-Negotiables

1. Use serial capture for validation. Do not depend on REST, WebSocket, STA, or AP switching during an agent session.
2. Force EdgeMixer to `MIRROR` for baseline captures. Non-MIRROR modes are creative transforms, not diagnostic ground truth.
3. Keep FastLED/RMT wire-time protection. This protocol never asks to remove the fence.
4. Lock effect ID, palette, brightness, speed, renderer mode, colour-correction state, dither state, and audio source before comparing frames.
5. Do not commit private clip paths, clips, or raw media. Commit sanitised numeric reports only.
6. Do not claim visual-default safety from a K1_Testbed simulation alone. The testbed is a pre-hardware filter, not physical LGP truth.
7. Do not revive the cancelled timed two-unit/manual A/B workflow.

## Evidence Ladder

| Step | Evidence | Pass condition |
|---:|---|---|
| 0 | Source checklist | Change type is classified: buffer ownership, silence policy, colour-correction default, or diagnostic-only. |
| 1 | K1_Testbed optional pre-filter | If used, calibration gates pass or the failure is documented as simulation-only. No firmware claim is made from this step. |
| 2 | Host/native tests | New metadata, toggles, or buffer-surface helpers have focused host coverage. |
| 3 | Serial capture Reference suite | Tap B, 30 FPS, v2, 20 s. Actual FPS `30.0 +/- 0.5`, frame drops `0`, parser errors `0`, heap trend bounded. |
| 4 | Serial capture Isolation suite | Taps A, B, C, 30 FPS, 10 s each. A->B and B->C deltas match the intended layer under test. |
| 5 | Serial capture Stress suite | Tap B, 40 FPS, 15 s. Actual FPS `>= 38`, drops `< 0.5%`, show-time p99 stable, parser errors `0`. |
| 6 | Hardware health snapshot | Serial `s` before/after: no panics, no RMT errors, sane LED show time, show skips stable, heap/stack headroom not collapsing. |
| 7 | Captain visual check | Only after Steps 0-6 pass. Captain checks a narrow question, not a broad preference survey. |

## Baseline Lock Sheet

Every VP run report must record these fields before capture starts:

| Field | Required value for diagnostic baseline |
|---|---|
| Device | Port and MAC. K1v2 MAC currently `b4:3a:45:a5:87:f8`; secondary K1 MAC in dual-device docs is `b4:3a:45:a5:89:b4`. |
| Firmware commit | Baseline commit and candidate commit. |
| Build env | Correct hardware env (`esp32dev_audio_esv11_k1v2_32khz` for K1v2). |
| Renderer mode | Unified unless the test specifically targets direct-strip/independent authoring. |
| EdgeMixer | `MIRROR`. |
| Effect and palette | Exact effect ID and palette ID. |
| Global controls | Brightness, speed, intensity/saturation/complexity/variation if exposed. |
| Colour correction | Mode, saturation boost, gamma enabled/value, white guardrail, auto-exposure, brown guardrail, LUT generation/status proof. |
| FastLED output | Brightness, dither state, correction/temperature if exposed. |
| Audio | Silence/no playback for colour-path tests; C-1 private track profile for audio-reactive tests. |
| Capture suite | Reference, Isolation, Stress, or Soak, with output directory and summary report path. |

## Change-Type Protocols

### A. Buffer-Ownership Correction

Use this when changing which pixel surface `ColorCorrectionEngine` processes.

Required effect set:

| Role | Example |
|---|---|
| Unified authored control | Any normal mirrored/global effect that renders into `m_leds`. |
| Direct dual-strip authored target | `EID_CROSS_STRIP_WAVE_INTERFERENCE` or another `DUAL_CHANNEL` effect. |
| Additive/tone-map guard | One effect from the explicit additive tone-map list. |
| Gradient/ramp guard | One effect that is known to be colour-ramp sensitive or skips correction by design. |

Required captures:

1. Baseline commit: Reference + Isolation for each effect.
2. Candidate commit: Reference + Isolation for each effect.
3. Candidate Stress for the direct dual-strip target.

Pass conditions:

- Transport gates pass: no parser errors, no frame drops in Reference, Stress within limits, no show-skip accumulation.
- For unified authored effects, A->B colour-correction delta stays within the existing baseline pattern unless the change explicitly targets that effect.
- For direct-strip authored effects, Tap C reflects the intended correction or an explicit skip policy. A/B must not be used as the only proof because `m_leds` may not be the authored surface.
- Strip A/B symmetry or deliberate asymmetry is documented. With EdgeMixer MIRROR, unexplained hue/saturation divergence is a fail.
- Any visible colour shift is tied to a named layer and accepted before commit.

### B. Silence-Policy Metadata

Use this when adding per-effect `IgnoreGlobalSilence`, `ApplySoftScale`, or hard-gate behaviour.

Required effect set:

| Role | Example |
|---|---|
| Pure ambient/non-reactive | An ambient effect that should not dim simply because playback stops. |
| Hybrid ambient/reactive | An effect with ambient body plus audio seasoning. |
| Reactive/audio-only | `EID_BEAT_PARITY_SPRITE` (`0x2102`) or equivalent. |
| Diagnostic pattern | Any validation/test pattern should ignore global silence. |

Required captures:

1. No-audio Reference capture per effect.
2. C-1 stop-recovery capture per effect if the effect is reactive or hybrid.
3. Candidate Stress capture for at least one ambient and one reactive effect.

Pass conditions:

- Ambient `IgnoreGlobalSilence` effects keep their intentional idle output without pretending to react to silence.
- Reactive effects still fade/gate deliberately under stop-recovery.
- No effect changes class silently; the metadata explains the observed behaviour.
- Captain visual check asks one question: "Does this effect's silence behaviour match its class?"

### C. Colour-Correction Defaults

Use this when changing gamma, saturation boost, white guardrail, brown guardrail, auto-exposure, or global correction mode.

Required captures:

1. Isolation suite with correction current/default.
2. Isolation suite with exactly one layer changed.
3. Reference suite with final candidate state.

Pass conditions:

- Gamma status proof is captured before every visual judgement.
- Mean brightness, white-pixel ratio, saturation/chroma, and hue-angle deltas are reported for each tap pair.
- A layer can only become a default if it improves the named defect without degrading a guard effect class.
- Intentional white, silver, pastel, and pale gradients must be represented before changing white guardrail defaults.

## Metrics To Report

| Metric | Why |
|---|---|
| Actual capture FPS and frame drops | Detect stutter/capture invalidity. |
| Parser error count | Reject corrupted capture data. |
| Show-time p50/p95/p99 | Ensure output changes do not destabilise transport. |
| Heap free and heap trend | Catch leaks or ratchets. |
| Mean RGB energy | Brightness/overall output shift. |
| White-pixel ratio | Detect haze or clipping. |
| Mean saturation/chroma | Detect washout or overcooking. |
| Hue angular delta | Detect colour drift. |
| Strip A/B delta | Detect unintended top/bottom divergence. |
| Temporal gradient energy | Detect flicker, stepping, or smoothing loss. |

## K1_Testbed Integration

K1_Testbed can help before hardware by simulating operator pipelines and using its existing gates (`L2 < 0.01`, `EDiv < 0.01`, `ToneMatch > 0.99`) as a candidate filter. It is especially useful for transport-core style logic, parameter viability, and effect-distillation work.

K1_Testbed cannot validate:

- LGP optical behaviour.
- FastLED/RMT timing.
- Physical strip buffer ownership.
- Silence perception on hardware.
- Product colour defaults.

Therefore the workflow is:

1. Use K1_Testbed only when the change can be represented in its operator/calibration model.
2. Treat a passing K1_Testbed report as permission to proceed to firmware capture, not as a firmware pass.
3. Do not edit K1_Testbed from this repo session unless Captain explicitly opens that repo for writes; it currently has its own dirty tree.

## Run Report Template

Each validation run should produce one sanitised markdown report:

```text
Title:
Firmware baseline commit:
Firmware candidate commit:
Device/port/MAC:
Build env:
Change type:
Effect set:
Baseline lock sheet:
Capture commands:
Artefacts generated:
Metric table:
Intended visual deltas:
Unexpected deltas:
Transport gate result:
Captain visual question:
Decision: PASS / FAIL / DEGRADED-PASS
Revisit trigger:
```

## Commit Gate

Do not commit a buffer-ownership correction, silence-policy default, or colour-correction default change unless the commit body cites:

- This protocol.
- The specific run report.
- The exact Captain visual sign-off line, if the change is visible.
- The validation commands and pass/fail result.
