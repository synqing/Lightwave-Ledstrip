# P1-09 Migration Cookbook

## Purpose

Mechanical reference for migrating audio-reactive effects from shared temporal state to per-zone state arrays under forensic finding `P1-09`.

`ZoneComposer::renderZone` reuses one `IEffect*` across multiple zones. Any temporal scalar, follower, accumulator, per-pixel histogram, or grid that is not indexed by zone will be advanced once per zone per frame, causing lockstep, acceleration, or cross-zone contamination.

This cookbook exists so every migration batch uses the same proven pattern.

## Hard constraints

- Centre origin remains mandatory.
- No heap allocation in `render()` or anything transitively called from it.
- `kMaxZones = 4` per class.
- Use `const int z = (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0;` at the top of `render()`.
- `init()` must reset all `kMaxZones` slots, not just slot 0.
- Do not change an effect's existing `cleanup()` or destructor free policy in a mechanical P1-09 migration.
- British English in comments and docs.

## Lifecycle note

The framework side already calls `cleanup()` on outgoing effects during direct effect switches and concurrent-transition pre-emption in [`src/core/actors/RendererActor.cpp`](../src/core/actors/RendererActor.cpp).

Empirical SPIRAM measurements are still required before Phase 5/6 hardware gating, but the current code strongly suggests an active-only lifecycle for effects that correctly free in `cleanup()`.

## Prototype 1: Scalar-only pattern

Reference: `LGPAiryCometAREffect`

### Header pattern

```cpp
private:
    static constexpr uint8_t kMaxZones = 4;

    float m_t[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};
    float m_bass[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};
    float m_treble[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};
    float m_chromaAngle[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};
    float m_bassMax[kMaxZones] = {0.15f, 0.15f, 0.15f, 0.15f};
    float m_trebleMax[kMaxZones] = {0.15f, 0.15f, 0.15f, 0.15f};
    float m_impact[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};
```

### `.cpp` pattern

```cpp
bool MyEffect::init(plugins::EffectContext& ctx) {
    for (uint8_t zi = 0; zi < kMaxZones; ++zi) {
        m_t[zi] = 0.0f;
        m_bass[zi] = 0.0f;
        m_treble[zi] = 0.0f;
        m_chromaAngle[zi] = 0.0f;
        m_bassMax[zi] = 0.15f;
        m_trebleMax[zi] = 0.15f;
        m_impact[zi] = 0.0f;
    }
    return true;
}

void MyEffect::render(plugins::EffectContext& ctx) {
    const int z = (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0;
    m_bass[z] += (rawBass - m_bass[z]) * alpha;
    m_t[z] += dt;
}
```

Use this pattern for:

- `LGPCymaticLadderAREffect`
- `LGPHarmonographHaloAREffect`
- `LGPHyperbolicPortalAREffect`
- `LGPMachDiamondsAREffect`
- `LGPMoireCathedralAREffect`
- `LGPRoseBloomAREffect`
- `LGPSchlierenFlowAREffect`
- `LGPSpirographCrownAREffect`
- `LGPSuperformulaGlyphAREffect`
- `LGPWaterCausticsAREffect`

## Prototype 2: PSRAM histogram / LED-domain buffer pattern

Reference: `LGPCatastropheCausticsAREffect`

### Header pattern

```cpp
private:
    static constexpr uint8_t kMaxZones = 4;

#ifndef NATIVE_BUILD
    struct PsramData {
        float buffer[kMaxZones][160];
    };
    PsramData* m_ps = nullptr;
#else
    float m_buffer[kMaxZones][160];
#endif
```

### `.cpp` pattern

```cpp
bool MyEffect::init(plugins::EffectContext& ctx) {
#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<PsramData*>(
            heap_caps_malloc(sizeof(PsramData), MALLOC_CAP_SPIRAM));
        if (!m_ps) return false;
    }
    for (uint8_t zi = 0; zi < kMaxZones; ++zi) {
        for (int i = 0; i < 160; ++i) m_ps->buffer[zi][i] = 0.0f;
    }
#else
    for (uint8_t zi = 0; zi < kMaxZones; ++zi) {
        for (int i = 0; i < 160; ++i) m_buffer[zi][i] = 0.0f;
    }
#endif
    return true;
}

void MyEffect::render(plugins::EffectContext& ctx) {
    const int z = (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0;
#ifndef NATIVE_BUILD
    if (!m_ps) return;
    float* buffer = m_ps->buffer[z];
#else
    float* buffer = m_buffer[z];
#endif
}
```

Use this pattern for:

- `LGPReactionDiffusionAREffect`
- `LGPRDTriangleAREffect`
- `LGPRule30CathedralAREffect`
- `LGPTimeReversalMirrorEffect_AR`
- `LGPTimeReversalMirrorEffect_Mod1`
- `LGPTimeReversalMirrorEffect_Mod2`
- `LGPTimeReversalMirrorEffect_Mod3`

For TRM family classes, preserve the existing `cleanup()` policy exactly and only promote the history buffers to `[kMaxZones]`.

## Prototype 3: Grid / CA pattern

Reference: `LGPLangtonHighwayAREffect`

### Header pattern

```cpp
private:
    static constexpr uint8_t kMaxZones = 4;
    static constexpr uint16_t kGridBytes = W * H;

    uint8_t* m_grid = nullptr;

    int8_t m_antX[kMaxZones] = {32, 32, 32, 32};
    int8_t m_antY[kMaxZones] = {32, 32, 32, 32};
    int8_t m_antDir[kMaxZones] = {0, 0, 0, 0};

    float m_antStepAccum[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};
    float m_sliceOffset[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};

    void stepAnt(int z);
    float sampleProjection(int z, float offset);
```

### `.cpp` pattern

```cpp
bool MyEffect::init(plugins::EffectContext& ctx) {
    if (!m_grid) {
#ifndef NATIVE_BUILD
        m_grid = static_cast<uint8_t*>(
            heap_caps_malloc(static_cast<size_t>(kMaxZones) * kGridBytes,
                             MALLOC_CAP_SPIRAM));
#else
        m_grid = new uint8_t[static_cast<size_t>(kMaxZones) * kGridBytes];
#endif
        if (!m_grid) return false;
    }

    for (size_t i = 0; i < static_cast<size_t>(kMaxZones) * kGridBytes; ++i) {
        m_grid[i] = 255;
    }
    return true;
}

void MyEffect::stepAnt(int z) {
    uint8_t* grid = m_grid + static_cast<size_t>(z) * kGridBytes;
    // operate only on this zone's slice
}
```

Use this pattern for:

- `LGPIFSBioRelicAREffect` if it stores zone-specific grid-like state in a contiguous PSRAM block

If the effect actually stores per-zone LED-domain histories rather than a true grid, fall back to Prototype 2 instead.

## Mechanical review checklist

- Every temporal member used in `render()` is indexed `[z]`.
- Every helper touching temporal or spatial state accepts `int z`.
- `init()` resets all zone slots.
- No new allocations in `render()`.
- No changes to an effect's cleanup/destructor lifecycle policy.
- `cleanup()` frees once or retains once, according to the file's pre-existing behaviour.

## Verification recipe

For each migrated representative effect on hardware:

1. Enable zone mode.
2. Assign the same effect ID to zone 0 and zone 1.
3. Apply divergent per-zone speed with `zs`.
4. Reassign one zone only.
5. Confirm no lockstep, no cross-zone reset, no one-zone dominance.

## Phase 0 measurement table

Measured on K1v2 after flashing the current migration image to `/dev/cu.usbmodem2101` on 2026-04-18.

Effect IDs used:

- `B1/B4/B9`: `EID_LGP_CYMATIC_LADDER_AR` (`0x1C04`)
- `B2`: `EID_LGP_MOIRE_CATHEDRAL_AR` (`0x1C08`)
- `B3`: `EID_LGP_TIME_REVERSAL_MIRROR_AR` (`0x1B05`)
- `B5`: `EID_LGP_TIME_REVERSAL_MIRROR_MOD1` (`0x1B06`)
- `B6`: `EID_LGP_TIME_REVERSAL_MIRROR_MOD2` (`0x1B07`)
- `B7`: `EID_LGP_TIME_REVERSAL_MIRROR_MOD3` (`0x1B08`)
- `B8`: back to `Mod1`

| Label | Meaning | Value |
|---|---|---:|
| B0 | Free SPIRAM at fresh boot | 8,058,167 |
| B1 | Low-PSRAM effect A active | 8,058,167 |
| B2 | Low-PSRAM effect B active | 8,055,839 |
| B3 | TRM_AR active | 7,875,111 |
| B4 | Back to effect A after TRM_AR | 8,058,303 |
| B5 | Mod1 active | 6,743,727 |
| B6 | Mod2 active | 6,743,591 |
| B7 | Mod3 active | 6,743,591 |
| B8 | Back to Mod1 | 6,743,591 |
| B9 | Back to effect A after Mod family | 8,058,167 |

## Lifecycle inference

This is an **active-only** lifecycle model in practice.

Evidence:

- `B4` returns to the baseline band immediately after leaving `TRM_AR`.
- `B5-B8` stay at one stable low-water mark while switching between similarly sized Mod variants instead of ratcheting downward.
- `B9` returns exactly to the original `B0/B1` baseline after leaving the Mod family.

If the lifecycle were persistent, SPIRAM free would decrease cumulatively across `B3 -> B8` and would not recover at `B4` or `B9`.

## Phase 6 gate memo

Gate basis: active-only model, reserve threshold `>= 4 MiB free` while any one Mod variant is active.

Measured worst case: `B6/B7/B8 = 6,743,591` bytes free.

Result: **PASS**. The free-SPIRAM floor remains roughly `2.55 MiB` above the `4 MiB` reserve threshold even with a Mod variant active.
