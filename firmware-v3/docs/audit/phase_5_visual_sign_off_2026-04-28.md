# Phase 5 Visual Sign-Off — DEGRADED-MODE Attestation

**Date:** 2026-04-28
**Authoriser:** Captain (verbal authority granted in session post-RBDO Gate landing at commit `f7c81775`)
**Scope:** 3 Phase 5 effect exemplars committed in `39406e6b`
**Attestation type:** DEGRADED-MODE per RBDO Gate (`CLAUDE.md` top). NOT a hardware visual sign-off. NOT a ship-gate.

---

## What this attestation IS

A formal acceptance, under explicit calibration debt, that the Phase 5 effect codebase is sound enough to unblock the Synergy-Topology branch's natural integration point. It records that the codebase passed every audit category for which evidence exists, and records the categories for which evidence does NOT exist along with the named risk.

## What this attestation IS NOT

- NOT a claim that hardware visual sign-off occurred
- NOT a claim the effects are ship-quality without further validation
- NOT a promotion from "exemplar" to "production-ratified"
- NOT a bypass of future sign-off gates; cycle 2+ remains available and required for promotion

---

## Effects covered

| Effect ID | Class | Source files | Trace family |
|---|---|---|---|
| `0x2100` | RadialTimeScopeEffect (Move 5.4) | `firmware-v3/src/effects/ieffect/RadialTimeScopeEffect.{h,cpp}` | `rts_*` (10 counters) |
| `0x2101` | AttackOnlyPitchVelocityFieldEffect (Move 5.6) | `firmware-v3/src/effects/ieffect/AttackOnlyPitchVelocityFieldEffect.{h,cpp}` | `pvf_*` (6 counters + 2 instants) |
| `0x2102` | BeatParitySpriteEffect (Move 5.7) | `firmware-v3/src/effects/ieffect/BeatParitySpriteEffect.{h,cpp}` | `bps_*` (8 counters + 6 instants) |

---

## Evidence supporting attestation — GROUNDED

Per the SSA-PHASE5-AUDIT forensic audit (2026-04-27) and AUDIT-PHASE5 forensic re-audit (2026-04-28), the following are independently verifiable in the working tree:

### Code-quality audit (PASS for all 3 effects)
- **Centre origin:** writes emanate from LED 79/80; mirror symmetry verified in source
- **No heap in render():** `heap_caps_malloc` only in `init()` paths; no `new` / `String` / `std::vector` / `malloc` inside `render()` for any effect
- **British English:** comments use "centre" / "colour" / "behaviour" / "initialise"; no American spellings as standalone words
- **EffectBase contract:** all 3 inherit `plugins::IEffect`, implement `init / render / cleanup / getMetadata`
- **dt-correction:** `getSafeRawDeltaSeconds()` used throughout; no `*= rate` FPS-dependent decay
- **Zone-bounds safe:** all writes guarded with `< total` / `< stripLen` bounds; no `ctx.zoneId` indexing without bounds check
- **No rainbows:** hue bounded by palette anchor or chroma-mean; no 0..255 hue sweep

### Integration audit
- Effect-ID registration confirmed at `firmware-v3/src/config/effect_ids.h:380-382`:
  - `EID_RADIAL_TIME_SCOPE = 0x2100`
  - `EID_ATTACK_ONLY_PITCH_VELOCITY = 0x2101`
  - `EID_BEAT_PARITY_SPRITE = 0x2102`
- Factory instantiation confirmed at `firmware-v3/src/effects/CoreEffects.cpp:1496/1501/1506`
- Display order confirmed at `firmware-v3/src/config/display_order.h:328-330`

### Build + test evidence
- `pio test -e native_test_phase5`: **130/130 PASS** in 1.97 s (commit `f49b4d6a`)
- `esp32dev_audio_esv11_k1v2_32khz` (canonical): builds clean, RAM 43.5%, Flash 33.0%
- `esp32dev_audio_esv11_k1v2_32khz_trace`: builds clean
- CI workflow `firmware_build_check.yml` gated by `native_test_phase5` since commit `632132e4`

### Hardware trace evidence
- 8 captures committed in `firmware-v3/tools/baselines/` totalling ~21,000 events:
  - `k1v2_0x2100_2026-04-27.json` — 5,470 events, full `rts_*` family present
  - `k1v2_0x2101_2026-04-27.json` — 5,470 events, full `pvf_*` family present
  - `k1v2_0x2102_2026-04-27.json` — 5,477 events, full `bps_*` family present
  - Plus 4 additional captures (variants + raw serial dump)
- BPS firing ratio: `bps_kick_fired` → `bps_sprite_spawn` 1:1 confirmed in trace data

---

## DEGRADED-MODE label — per RBDO Gate

**Unresolved assumption.** That the 3 Phase 5 effects render correctly in their VISUAL phenomenon under live audio playback on K1 V2 hardware as observed by a human operator. Specifically: that RTS time-axis scrolling reads correctly, that PVF chord interference reads as continuum (not "3 dots"), that BPS reads as bloom-breath (not metronome), that mirror symmetry is preserved across the centre seam, and that no off-centre flashes / rainbow drift / wagon-wheel aliasing occurs on sustained material.

**Risk if wrong.** A subtle visual failure mode is not caught at this attestation and propagates downstream. The trace data confirms event-firing ratios and timing-budget compliance but cannot independently validate perceptual quality. Brand-voice risks (per `PHASE5_EFFECTS_RESEARCH_SYNTHESIS_2026-04-27.md` §7) for PVF (continuum vs 3 dots) and BPS (bloom-breath vs metronome) are explicitly NOT validated by this attestation.

**Fallback.** If a failure mode manifests in a subsequent sign-off cycle (cycle 2+ when C-2 / C-5 land), the offending effect is held back from any merge-to-main / production-promotion while the others proceed. Diagnostic-baseline status (per C-4) means no production claim depends on this attestation.

**Revisit trigger.** When C-1 (mic-domain envelope), C-2 (feature × effect × dwell matrix), and C-5 (per-effect timestamped observables) land, cycle 2 of sign-off becomes a calibrated ship-gate. This DEGRADED-MODE attestation is provisional pending that cycle.

**Debt count / affected outputs.** This attestation depends on:
- C-1 (URGENT) — 4th dependent (was 3); resolution priority remains URGENT
- C-2 (HIGH) — 2nd dependent
- C-3 (HIGH) — would become a dependent if any calibrated audio sweep is built; not yet
- C-5 (MEDIUM) — 1st dependent
- C-4 (DECIDED) — provides the framing under which this attestation is acceptable

---

## Captain authorisation

Captain granted verbal authority in session on 2026-04-28 to accept this attestation under DEGRADED-MODE rather than block on hardware visual sign-off, given:

- The diagnostic-baseline purpose set in C-4 (not a ship-gate, not a regression-detector — first-cycle baseline)
- The accumulated cost of orchestration drift if the codebase remains unattested longer
- The forward roadmap requires unblocking the branch's natural integration point
- The codebase audit produced PASS on every category for which independent evidence exists

The verbal authority is recorded here as the canonical attestation; no subsequent agent should treat the absence of hardware visual sign-off as a bug requiring re-litigation. The path to a hardware-validated cycle is documented above (revisit trigger).

---

## Promotion path (for cycle 2+, NOT this attestation)

| Step | Gate | Owner |
|---|---|---|
| 1 | Resolve C-1 (mic-domain envelope) | Captain hardware pass OR audit of `audio_feature_surface_v2_baseline_2026-04-27.md` |
| 2 | Author C-2 (feature × effect × dwell matrix) | Agent, ~2 hr, depends on C-1 |
| 3 | Resolve C-3 (clip licence + repo public-status) | Captain answers |
| 4 | Author C-5 (per-effect timestamped observables) | Agent, depends on C-2 |
| 5 | Build calibrated sign-off harness | Agent, depends on C-1/C-2/C-3/C-5 |
| 6 | Cycle 2 sign-off — calibrated, hardware-validated, ship-gate purpose | Captain |
| 7 | Promote effects from exemplar → production-ratified | this is the genuine sign-off this attestation does NOT provide |

---

## Acceptance log

| Date | Authoriser | Decision |
|---|---|---|
| 2026-04-28 | Captain (verbal, in session) | DEGRADED-MODE accepted for Phase 5 effects `0x2100` / `0x2101` / `0x2102` — diagnostic baseline only; does not promote to ship-quality |

---

**Document Changelog**

| Date | Author | Change |
|---|---|---|
| 2026-04-28 | Claude Opus 4.7 (1M context) | Created under Captain verbal authorisation post-RBDO-Gate (commit `f7c81775`). DEGRADED-MODE attestation, not hardware visual sign-off. References commits `39406e6b` (effects), `f49b4d6a` (tests), `fc122a25` (trace tooling), `929e6817` (baselines), `f7c81775` (RBDO gate). |
