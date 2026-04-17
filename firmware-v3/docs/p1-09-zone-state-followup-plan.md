---
abstract: "P1-09 follow-up plan: migrate remaining 18 AR effects (plus 3 TRM Mod variants) from shared-state to per-zone [kMaxZones] state arrays. Three prototypes (AiryComet, CatastropheCaustics, LangtonHighway) validated with PASS build. Batches of 6-7 effects, each PR mechanical and small-blast-radius."
---

# P1-09 — Zone State Refactor Follow-Up Plan

## Context

Forensic audit 2026-04-17 §P1-09 identified that `ZoneComposer::renderZone` reuses a single `IEffect*` across all zones. Effects with temporal state (followers, smoothing EMAs, time accumulators, per-pixel histograms) advance N times per frame when assigned to N zones, collapsing audio smoothing, stacking motion, and multiplying effective speed.

**Decision:** Approach A (per-zone `[kMaxZones]` arrays) — matches existing exemplars (`EsBloomRefEffect`, `SnapwaveLinearEffect`, `WaveformParityEffect`, `BloomParityEffect`, `BeatPulseTransportCore`, `KuramotoOscillatorField`, `KuramotoTransportBuffer`, `EsAnalogRefEffect`, `EsOctaveRefEffect`, `EsSpectrumRefEffect`, `EsWaveformRefEffect`, `SbWaveform310RefEffect`). Rejected Approach B (per-zone instances) to avoid a second divergent pattern and larger blast radius.

Constants: `kMaxZones = 4` (defensive oversize versus `MAX_ZONES = 3` for 0xFF fallback — matches existing exemplars).

## Prototype Status — COMPLETE

Three effects migrated in sandbox at `/tmp/k1_p1deferred_20260418_011913/p109_effects/firmware-v3/`:

| Effect | Scalars | PSRAM buffer | Per-zone PSRAM cost | Notes |
|---|---|---|---|---|
| `LGPAiryCometAREffect` | 7 | none | 0 | Scalar-only; simplest mechanical template |
| `LGPCatastropheCausticsAREffect` | 7 | `I[160]` | +1,920 B SPIRAM | LED-domain histogram now `[kMaxZones][160]` |
| `LGPLangtonHighwayAREffect` | 5 scalars + 5 ant ints | 64x64 grid | +12,288 B SPIRAM | 3 additional ant grids + `stepAnt(z)` / `sampleProjection(z, ...)` |

**Build verification:** `pio run -e esp32dev_audio_esv11_k1v2_32khz` — PASS, RAM 41.1%, Flash 32.6%.

## Remaining Work — 18 AR effects + 3 TRM Mod variants

### Category 1: Scalar-only (trivial, ~30-50 LOC each)

These have no per-pixel or per-grid internal buffer — only followers, time accumulators, chroma angle. Mechanical edit identical to `LGPAiryCometAREffect`.

1. `LGPCymaticLadderAREffect` (8 scalar floats)
2. `LGPHarmonographHaloAREffect` (7)
3. `LGPHyperbolicPortalAREffect` (7)
4. `LGPMachDiamondsAREffect` (7)
5. `LGPMoireCathedralAREffect` (7)
6. `LGPRoseBloomAREffect` (8)
7. `LGPSchlierenFlowAREffect` (9)
8. `LGPSpirographCrownAREffect` (7)
9. `LGPSuperformulaGlyphAREffect` (11)
10. `LGPTalbotCarpetAREffect` (7) — QUARANTINED, audit notes "not in registry" — defer / verify before touching
11. `LGPWaterCausticsAREffect` (9)

**Estimated LOC per effect:** 30-50 lines (header + cpp). Copy-paste of the AiryComet pattern.

### Category 2: Scalars + LED-domain PSRAM buffer (moderate)

These need both scalar dimensioning AND `buffer[160]` dimensioning to `[kMaxZones][160]`. Pattern exactly matches `LGPCatastropheCausticsAREffect`.

12. `LGPReactionDiffusionAREffect` (5 scalars + PSRAM RD grids) — RD grids MUST be per-zone (spatial accumulators)
13. `LGPRDTriangleAREffect` (9 scalars + PSRAM — likely UV RD grid pair)
14. `LGPRule30CathedralAREffect` (8 scalars + PSRAM cellular-automaton history)

**Estimated LOC:** 60-100 per effect. Requires care when struct member is a `float buf[N]` nested in PSRAM struct — must promote to `float buf[kMaxZones][N]`.

### Category 3: Scalars + LED-domain PSRAM + CA/grid state (complex)

These carry a 2D cellular-automaton or large internal state. Pattern matches `LGPLangtonHighwayAREffect`: grid becomes `[kMaxZones]` contiguous block, helpers take `int z` parameter.

15. `LGPIFSBioRelicAREffect` (9 scalars + PSRAM IFS attractor buffer)
16. `LGPLorenzRibbonAREffect` (10 scalars + PSRAM ribbon history) — Lorenz state (x, y, z) must be per-zone

**Estimated LOC:** 80-150 per effect.

### Category 4: Large PSRAM + multi-buffer (high blast radius)

17. `LGPTimeReversalMirrorEffect_AR` — already 45.76 KB PSRAM; per-zone = 4 x 45.76 KB = **183 KB SPIRAM**. Verify PSRAM headroom first. Likely worth splitting this into its own PR.
18. `LGPChimeraCrownAREffect` (7 scalars + PSRAM) — check cpp for multi-buffer complexity
19. `LGPTimeReversalMirrorEffect_Mod1`, `_Mod2`, `_Mod3` — audit declares TRM variants; apply same pattern as TRM_AR. Verify each is in registry before touching.

### Out-of-scope / quarantined

The forensic audit lists 4 effects as "quarantined / not in registry". Cross-check before touching:
- `LGPChimeraCrownAREffect` — audit calls this quarantined; defer until confirmed in registry
- `LGPLorenzRibbonAREffect` — same
- `LGPTalbotCarpetAREffect` — same
- Kuramoto transport effect — ALREADY MIGRATED (per audit note)

## Suggested Batching

Each PR MUST include a clean build verification line and a screenshot-style visual diff note if possible. Keep batches small; reviewer fatigue causes mechanical-change regressions.

### PR 1 — Prototypes (THIS SANDBOX)
- `LGPAiryCometAREffect`, `LGPCatastropheCausticsAREffect`, `LGPLangtonHighwayAREffect`
- **Already validated.** Apply to real tree as first merge.

### PR 2 — Scalar-only batch A (6 effects, ~300 LOC total)
- Cymatic, Harmonograph, Hyperbolic, Mach, Moire, Rose

### PR 3 — Scalar-only batch B (5 effects, ~250 LOC total)
- Schlieren, Spirograph, Superformula, WaterCaustics, Talbot (if confirmed in registry)

### PR 4 — Scalars + LED-domain PSRAM (3 effects, ~240 LOC total)
- ReactionDiffusion, RDTriangle, Rule30Cathedral

### PR 5 — Scalars + grid/CA PSRAM (2 effects, ~300 LOC)
- IFSBioRelic, LorenzRibbon (if confirmed in registry)

### PR 6 — TRM family (4 effects, ~800 LOC, high PSRAM impact)
- `LGPTimeReversalMirrorEffect_AR` + `_Mod1` + `_Mod2` + `_Mod3`
- **MUST verify PSRAM budget** before merge: 4 effects x 4 zones x 45.76 KB = 732 KB. If unacceptable, promote to approach B for this family only (instantiate distinct effect per zone at registration).

### PR 7 — ChimeraCrown (if confirmed in registry)
- Solo PR due to uncertain scope + quarantine status

## Review Criteria (applies to every PR)

Per PR checklist:
- [ ] All per-zone scalar reads/writes indexed by `[z]` where `const int z = (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0;`
- [ ] `init()` resets ALL `[kMaxZones]` slots, not just slot 0
- [ ] `cleanup()` frees PSRAM once (shared across zones); never per-zone
- [ ] PSRAM allocation sized for `kMaxZones * sizeof(...)` where LED-domain buffers exist
- [ ] Build: `pio run -e esp32dev_audio_esv11_k1v2_32khz` PASS, Flash delta reported
- [ ] Grep: `grep -nE "\bm_(member)\b[^\[]"` returns zero unindexed accesses in the render() function
- [ ] Behaviour spot-check: assign effect to 2 zones in serial CLI, verify visuals are independent (not accelerated or collapsed)

## Total Estimated Change

- **Files touched:** 2 x 18 = 36 files (headers + cpp) for remaining AR effects; 2 x 4 = 8 files for TRM family. Total: **~44 files**.
- **Estimated LOC:** ~1,400-1,800 net delta across all PRs combined.
- **PSRAM delta (K1v2):** ~180 KB extra SPIRAM when TRM family is migrated (most of the footprint). All other effects add < 20 KB combined.
- **DRAM delta:** negligible (< 2 KB total, since `kMaxZones` grows scalar floats by factor of 4).
- **Render-time impact:** zero. Per-zone arrays add one index load per access; any impact is dwarfed by the existing sinf/expf calls. No extra memory bandwidth since only one zone's slot is touched per render() call.

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-04-17 | agent:embedded | Created follow-up plan from P1-09 sandbox prototype work. |
