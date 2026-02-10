# IEffect Migration Checklist

**Effect Name**: [EffectName]  
**Legacy Location**: [file path]  
**Target Location**: `firmware/v2/src/effects/ieffect/[EffectName]Effect.h|cpp`

---

## Pre-Migration

- [ ] Identify legacy effect function and parameters
- [ ] Review PatternRegistry metadata (effect ID stability)
- [ ] Check for centre-origin compliance (LEDs 79/80)
- [ ] Identify render loop allocations (must be eliminated)

---

## Migration Steps

### 1. Create IEffect Class Files

- [ ] Create `[EffectName]Effect.h` with class definition
- [ ] Create `[EffectName]Effect.cpp` with implementation
- [ ] Move static render state to instance members
- [ ] Implement `init(EffectContext&)` - reset instance state
- [ ] Implement `render(EffectContext&)` - no heap allocations
- [ ] Implement `cleanup()` - usually no-op
- [ ] Implement `getMetadata()` - return effect metadata

### 2. Registration

- [ ] Register in `CoreEffects.cpp` with stable effect ID
- [ ] Update PatternRegistry if metadata changed (rare)
- [ ] Remove legacy function from old location (or mark deprecated)

### 3. Validation

- [ ] Centre-origin compliance: `validate <effectId>` passes
- [ ] No heap allocation: No `new`/`malloc`/`std::vector` growth in `render()`
- [ ] Performance: FPS ≥120 (check with `s` serial command)
- [ ] Palette-aware: Uses `ctx.palette`, not hardcoded colours
- [ ] Zone-aware: Respects `ctx.ledCount` and `ctx.zoneStart` if applicable

---

## Acceptance Checks

- [ ] Effect ID matches PatternRegistry index (stable)
- [ ] `validate <effectId>` passes all checks
- [ ] No American spellings in comments (British English)
- [ ] Build passes: `cd firmware/v2 && pio run -e esp32dev_audio_esv11`
