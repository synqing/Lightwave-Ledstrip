> K1's hardest problem is not "can firmware generate patterns?" It clearly can. The hard problem is building a shared language between what you see on the fixture and what the code is actually doing. This characterisation layer is becoming that bridge. It turns "this feels cheap / locked / alive / wrong / loitering" into named, testable, source-anchored system behaviour.
>
> The danger is also clear: if we let this become loose prose, it turns into lore. The value only holds if every trait gets a stable name, affected effect IDs, Captain visual statement, likely mechanism, source anchor, and whether it is desired, tolerated, parked, or rejected. Done that way, it becomes a product design database for K1's visual language, not just debugging notes.

# K1 Visual Characterisation Database

**RBDO label:** GROUNDED for source-anchored entries; DEGRADED-MODE for any visual-quality conclusion without explicit Captain hardware judgement.

**Purpose:** preserve the bridge between Captain's fixture-level visual observations and the firmware/AP-VP mechanisms that produce them.

**Scope:** K1 Lightwave visual traits, effect classes, failure patterns, tolerated artefacts, and future-class definitions. This is not a ship-gate document and not a substitute for hardware validation.

## Agent Rule

Do not add loose prose. This is a lightweight characterisation ledger, not an essay database and not a lore dump. Every entry must use these fixed fields:

- Label
- Effect IDs
- Captain visual statement
- Observed condition
- vp stack anchor
- Likely mechanism
- Source anchor
- Test anchor
- Decision: desired / tolerated / parked / rejected
- Next action

If any field is unknown, write `unknown` and say what evidence would close it.

## Characterisation Ledger

| Label | Effect IDs | Captain visual statement | Observed condition | vp stack anchor | Likely mechanism | Source anchor | Test anchor | Decision | Next action |
|---|---|---|---|---|---|---|---|---|---|
| PVF Uncontrolled Flicker | `0x2101` | PVF-2 is "100% broken"; background appears to flicker wildly; node formation/count/expansion is unintelligible. | Music-reactive PVF visual run. | unknown; capture `vp stack` before any future PVF repair run. | Raw hop-rate chroma/energy likely owns structural state and/or bed behaviour without enough event ownership. | `firmware-v3/docs/research/lgp_beat_emotiscope_architecture_review_2026-05-06.md` | unknown; candidate native test existed but no accepted hardware validation. | rejected for current lane | Keep parked unless Captain explicitly reopens PVF. |
| BPS Event-Sprite Class | `0x2102` | Silence is dark/unresponsive; musical behaviour remains visually unsatisfactory. | BPS-1 visual-only silence row plus later music observation. | unknown; only serial `s` status is recorded for BPS-1. Capture `vp stack` before future BPS work. | Fixed sprite pool, kick-triggered state, event-sprite identity; not faithful Emotiscope tempo-bank architecture. | `firmware-v3/docs/research/lgp_beat_emotiscope_architecture_review_2026-05-06.md`, `docs/adr/lightweight-architecture-decision-ledger.md` | `firmware-v3/test/test_native/test_beat_parity_sprite.cpp` | tolerated / parked | Do not iterate in this lane; reopen only as its own future event-sprite class. |
| Hybrid/V1 Waveform Pull-In | future class | Desired class: centre-organised around LEDs 79/80 while primary visible transport pulls inward from edges toward centre. | Concept declared after BPS was parked. | unknown; no implementation exists yet. | Waveform/history semantics plus musical endpoints should own structure; RMS may shade/gate only. | `firmware-v3/docs/research/lgp_beat_emotiscope_architecture_review_2026-05-06.md`, `docs/adr/lightweight-architecture-decision-ledger.md` | unknown; no implementation exists yet. | desired / gated | Open a dedicated design task before implementation; study Snapwave without blind porting. |
| Waveform Hybrid Musical Response | `0x1313`, compared with `0x1302` | Hybrid is musically superior: snappier to trigger and snappier to fade. | Captain comparison of Hybrid and standard Waveform with music. | `firmware-v3/docs/research/k1_waveform_hybrid_serial_evidence_2026-05-07.md` (`0x1313` unified path, no surface mismatch). | Hybrid colour synthesis/smoothing and existing dot/trail topology produce stronger perceived audio lock than standard Waveform. | `firmware-v3/docs/research/k1_waveform_hybrid_serial_evidence_2026-05-07.md` | unknown; judgement is visual, not locked by a behaviour test. | desired | Preserve Hybrid as stronger Waveform-family candidate; avoid global VP default changes. |
| Waveform Native Speed Floor | `0x1302`, `0x1313` | Both standard and Hybrid must run at native speed `27`; lower values feel sluggish/out of sync. | Waveform-family visual tuning at lower speed values. | `firmware-v3/docs/research/k1_waveform_hybrid_serial_evidence_2026-05-07.md` (K1v2 `0x1313` post-flash stack reports unified path, `speed=25` control value, no output faults; speed floor is effect-local). | Scroll-rate calculation below 27 under-drives the galloping visual transport. | `firmware-v3/docs/research/k1_waveform_hybrid_serial_evidence_2026-05-07.md` | `firmware-v3/test/test_sbk1_waveform_hybrid/test_main.cpp` | desired | Keep floor effect-local; do not change global speed defaults. |
| Hybrid Colour Budget Floor | `0x1313` | Hybrid is too dark below high PHOTONS; saturation appears to max around brightness `200`, and below roughly `150` is unacceptable. | Hybrid brightness/saturation tuning at non-max brightness. | `firmware-v3/docs/research/k1_waveform_hybrid_serial_evidence_2026-05-07.md` (`0x1313` K1v2 post-flash unified path, colour correction/tone map/EdgeMixer state recorded, no output faults). | SB-style per-bin share was too conservative for K1 LGP; local colour budget needed widening without changing global VP defaults. | `firmware-v3/docs/research/k1_waveform_hybrid_serial_evidence_2026-05-07.md` | `firmware-v3/test/test_sbk1_waveform_hybrid/test_main.cpp` | desired | Keep change effect-local and collect next Captain visual judgement before any further promotion. |
| Waveform Loiter | `0x1302`, `0x1313` | During cold-start acquisition and trailing release, the effect can lose galloping sprite trails and become a uniform sheet-like colour/brightness state. | Below waveform floor while audio is still available and confidence/silence gates remain open. | `firmware-v3/docs/research/k1_waveform_hybrid_serial_evidence_2026-05-07.md` (`audio=true`, silence policy and output path recorded for Waveform-family samples). | Chroma still injects colour while waveform peak falls below the local floor, collapsing motion amplitude into repeated centre-origin injection. | `firmware-v3/docs/research/k1_waveform_hybrid_serial_evidence_2026-05-07.md` | `firmware-v3/test/test_sbk1_waveform_hybrid/test_main.cpp` | tolerated | Decide later whether to shape into a feature or suppress; do not classify as fail yet. |
| Waveform Runtime Timing Pressure | `0x1302`, `0x1313` | unknown; Captain has not yet linked this runtime trait to a visual statement. | K1v2 serial baseline after `555f841e`: both Waveform-family effects show no LED output faults but run near/over the 120 FPS frame period with high accumulated drops. | `firmware-v3/docs/research/k1_waveform_hybrid_serial_evidence_2026-05-07.md` (`0x1313`: `effect_render avg_us=478`, `colour_correction avg_us=801`, `show_leds avg_us=6390`, `pre_pacing_work avg_us=8464`; `0x1302`: `effect_render avg_us=449`, `colour_correction avg_us=1177`, `show_leds avg_us=6477`, `pre_pacing_work avg_us=8879`; both `show_skips=0`, no RMT/output faults). | Per-layer evidence shows Waveform effect render is under `0.6 ms` in the captured samples. Total pressure is dominated by protected LED show and shared VP work; do not treat total frame time as effect-code-only timing. | `firmware-v3/docs/research/k1_waveform_hybrid_serial_evidence_2026-05-07.md` | K1v2 serial `vp stack`, `s`, `dbg memory`; trace capture attempt failed because the current image did not emit MabuTrace markers. | parked | Use the new per-layer `vp stack` timing before any optimisation; do not change global VP defaults without explicit Captain approval. |

## Update Discipline

This file is a product-language database. It is allowed to record surprising or accidental behaviour, but it must not romanticise it. An accidental behaviour earns a durable name only when Captain can describe it visually and the firmware mechanism can be source-anchored or instrumented.
