# Phase 5 Effects Visual Resume Run - 2026-05-06

**RBDO label:** GROUNDED for the serial command, device identity, and Captain visual judgement captured in-session. DEGRADED-MODE for trace counters because the active firmware image did not emit MabuTrace markers during this resumed run.

## Run Context

| Field | Value |
|---|---|
| Device / port / MAC | K1v2 on `/dev/cu.usbmodem2101`, MAC `b4:3a:45:a5:87:f8` |
| Firmware commit | `dba7edf3` |
| Build env | Active flashed image not independently proven as `_trace` during this run |
| Control path | Serial only |
| Effect ID tested | `0x2101` Attack-Only Pitch Velocity Field |
| Row tested | `PVF-2` |
| Private labels | None recorded; operator-controlled sustained tonal material only |
| Serial commands | `effect 0x2101`; attempted `trace` |

## Evidence

`PVF-2` asks whether sustained tonal material reads as a continuous standing-wave field rather than isolated dots (`firmware-v3/docs/research/c5_phase5_timestamped_observables_2026-05-06.md:61`).

The prior C-5 hardware sweep recorded trace health for `PVF-2`: 5506 events, effect p99 1000 us, and RMT p50/p99 6160/6255 us, but no Captain visual answer (`firmware-v3/docs/research/c5_phase5_hardware_sweep_2026-05-06.md:41`).

During this resumed visual run, `capture_trace.py` switched the effect successfully but the later `trace` command did not return `[TRACE]` markers. No trace JSON was saved. This is consistent with the active firmware image not being a `_trace` build, or with the trace command not being accepted by that image.

## Captain Visual Judgement

Captain reported that `0x2101` is "100% broken". The visible failure was not merely "isolated dots"; the effect wildly and uncontrollably flickered, with the background appearing to flicker. The node behaviour had no clear readable rule: node formation count, expansion, and expansion size did not appear intelligible.

## Result

**FAIL.**

This row is a visual-quality failure for PVF / `0x2101`. It must not be counted as PASS or DEGRADED-PASS, and Phase 5 must not be promoted toward ship-gate readiness from the previous trace-only evidence.

## Classification

Current classification is **effect-local production render-path failure isolated, candidate patch unvalidated on hardware**.

Source evidence:

- The production `render()` path previously captured chroma targets on every fresh audio hop regardless of onset state, while the native onset-only test covered the independent `debugTickFollowers()` seam rather than production render behaviour.
- The production bed was directly `fastRms`-driven across the whole strip. That matches Captain's observation that the "background" was the part wildly flickering.
- A focused native regression test now covers sustained chroma plus high `fast_rms` without onset, and requires the production render path to stay dark.

Trace capture remains separately degraded because the active firmware image did not emit trace markers during the visual run.

## What Changed

A candidate firmware patch was made after the visual failure:

- `AttackOnlyPitchVelocityFieldEffect::render()` now captures new chroma targets only on attack/onset hops, so per-hop chroma wobble cannot continuously reshuffle the top-K field.
- The PVF bed no longer follows raw `fastRms`; it only appears when the attack-gated chroma field is alive and follows the slower confidence/silence gate.
- Native coverage was extended with `test_render_sustained_chroma_without_onset_stays_dark`.

## What Remains Unproven

- No PVF trace counters were captured for this resumed visual run.
- The candidate patch has not been flashed or visually checked on K1v2 hardware.
- It is unknown whether the patched PVF now reads as a useful field or merely stops the worst flicker.

## Next Action

Flash the candidate firmware to K1v2 and rerun a short PVF-2 visual check before any commit or PASS claim.
