---
abstract: "Phase 4 Move 4.3 curation packet for F3 Liquid Stillness. Establishes the source-grounded candidate pool, hard exclusions, recommended slate, and the exact Captain decision still needed before firmware changes select the final ambient programmes."
---

# Move 4.3 — Liquid Stillness Curation Packet

RBDO label: GROUNDED for source facts, DEGRADED-MODE for the proposed slate until Captain visual arbitration.

## 1. Source Facts

- Canonical move: `Topology_Reconciliation.md` §5 Phase 4 Move 4.3 says F3 Liquid Stillness is `8–12 ambient programmes selected from 106`, with Captain curation required.
- Product source: `SB_ES_MOTION_BRAINSTORM_CATALOGUE_2026-04-26.md` calls Liquid Stillness `"Extraordinary when silent"` and says curation beats generation.
- Existing substrate: Phase 4 Move 4.1 `VoiceMusicClassifier` and Move 4.2 `AudioGatedConditionalDecay` exist for the Liquid Stillness stack.
- Current firmware definition of ambient: `PatternRegistry::buildAmbientEffectArray()` includes effects that are not in `REACTIVE_EFFECT_IDS[]`.
- Current mechanical ambient pool from `DISPLAY_ORDER[]` minus `REACTIVE_EFFECT_IDS[]` minus `EXPERIMENTAL_EFFECT_IDS[]`: 92 effects.
- Existing built-in Ambient show is only four cues: `LGP Spiral Vortex`, `LGP Star Burst`, `LGP Moire Curtains`, `Plasma`. That is not the final F3 curated slate.

## 2. Hard Exclusions

These are excluded before Captain taste arbitration:

- Any effect in `REACTIVE_EFFECT_IDS[]`: not Liquid Stillness; it reacts to music by definition.
- Any effect in `EXPERIMENTAL_EFFECT_IDS[]`: hidden from production rotation or centre-origin suspect.
- Backend or diagnostic effects: `Perlin Test: FastLED`, `Perlin Test: Emotiscope2 Full`, `Perlin Test: Emotiscope2 Quarter`.
- Effects whose story reads as burst, graph, test, high-energy spectacle, or obvious "screensaver" rather than liquid centre-origin calm.

## 3. Recommended Candidate Slate

This is the current engineering shortlist for Captain visual arbitration. It is intentionally 16, so Captain can cut to the final 8–12.

| EID | Effect | Why it belongs in F3 audition |
|---|---|---|
| `0x0101` | Ocean | Baseline liquid motion; readable without audio. |
| `0x0102` | Plasma | Smooth continuous field; useful as a low-information idle state. |
| `0x0400` | LGP Moire Curtains | Optical slow beat envelopes; already used by the Ambient show. |
| `0x0402` | LGP Holographic Vortex | Depth illusion without needing audio input. |
| `0x0403` | LGP Evanescent Drift | Ghost-light drift; likely strong Liquid Stillness fit. |
| `0x0500` | LGP Aurora Borealis | Calm curtain behaviour; factory preset already labels Aurora as calm ambient. |
| `0x0501` | LGP Bioluminescent Waves | Organic liquid wave identity. |
| `0x0502` | LGP Plasma Membrane | Soft cellular motion; centre-origin compliant metadata. |
| `0x0504` | LGP Crystalline Growth | Slow emergent structure; candidate for quiet "what is that" moments. |
| `0x0700` | LGP Colour Temperature | Restrained blackbody gradient; likely useful as a warm idle programme. |
| `0x0709` | LGP Perceptual Blend | Directly aligned with perceptual/JND substrate; low-noise colour transition. |
| `0x0C00` | LGP Perlin Veil Ambient | Explicit ambient variant; slow drifting noise curtains from centre. |
| `0x0C02` | LGP Perlin Caustics Ambient | Explicit ambient variant; caustic lobes and slow parameter drift. |
| `0x0C03` | LGP Perlin Interference Weave Ambient | Explicit ambient + dual-strip moire; should be auditioned after Phase 3 closed. |
| `0x1600` | LGP Opal Film | Thin-film optical drift; product-aligned diffused glass identity. |
| `0x1602` | LGP Stress Glass | Slow photoelastic fringe identity; likely strong on the LGP. |

## 4. Borderline Audition-Only Candidates

These may be strong, but need visual judgement because they risk being too busy, too mathematical, or too spectacle-heavy for Liquid Stillness:

- `0x0406` LGP Fresnel Zones
- `0x0505` LGP Fluid Dynamics
- `0x0602` LGP Time Crystal
- `0x0603` LGP Soliton Waves
- `0x0609` LGP Evanescent Skin
- `0x1603` LGP Moire Silk
- `0x1604` LGP Caustic Shards
- `0x1606` LGP Stress Glass (Melt)
- `0x1700` LGP Reaction Diffusion
- `0x1701` LGP RD Triangle
- `0x1802` LGP Moire Cathedral
- `0x1901` Catastrophe Caustics
- `0x1B01` KdV Soliton Pair
- `0x1B04` Fresnel Caustic Sweep

## 5. Captain Decision Needed

Captain needs to cut the recommended slate to the final **8–12** Liquid Stillness programmes after hardware viewing.

Minimum decision format:

```
Liquid Stillness final slate:
0x____, 0x____, 0x____, 0x____,
0x____, 0x____, 0x____, 0x____
optional:
0x____, 0x____, 0x____, 0x____
```

Once Captain selects the slate, the implementation work is small and determinate:

- Add a firmware-visible `LIQUID_STILLNESS_EFFECTS[]` allowlist.
- Rebuild the built-in Ambient show to use the final slate.
- Add a scoped native test that verifies the slate excludes reactive, experimental, and backend-test effects.
- Update `BACKLOG.md` Phase 4 Move 4.3 from planned to shipped.
