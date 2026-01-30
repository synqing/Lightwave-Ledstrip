# Drift Fixes Log (Confirmed + Applied)

This file tracks “drift vectors” where multiple definitions of the same concept exist and can silently diverge.

---

## 1) Palette-range drift (CONFIRMED)

### 1.1 Sources (before fix)
- Master palette system:
  - `firmware/v2/src/palettes/Palettes_Master.h` → `MASTER_PALETTE_COUNT = 75`
- Persistence (zones/system):
  - `firmware/v2/src/core/persistence/ZoneConfigManager.h` → `MAX_PALETTE_ID = 36` (stale)
- CQRS/state:
  - `firmware/v2/src/core/state/SystemState.h` → `MAX_PALETTE_COUNT = 64` (stale)

### 1.2 Applied fixes (this workspace)
- `ZoneConfigManager.h` now includes `Palettes_Master.h` and derives:
  - `MAX_PALETTE_ID = MASTER_PALETTE_COUNT - 1`
  - with a `static_assert(MASTER_PALETTE_COUNT == 75)`
- `SystemState.h` now includes `Palettes_Master.h` and derives:
  - `MAX_PALETTE_COUNT = MASTER_PALETTE_COUNT`
  - with a `static_assert(MASTER_PALETTE_COUNT == 75)`

### 1.3 Remaining follow-ups
- Consider removing the hard `==75` asserts once the ecosystem is robust (or replace with a softer `static_assert(MASTER_PALETTE_COUNT <= 255)` and update client code).

---

## 2) Centre-point duplication (CONFIRMED, NOT YET FIXED)

### Current duplicated definitions
- `firmware/v2/src/effects/CoreEffects.h`: `CENTER_LEFT=79`, `CENTER_RIGHT=80`
- `firmware/v2/src/core/actors/RendererActor.h`: `LedConfig::CENTER_POINT=79`
- `firmware/v2/src/effects/transitions/TransitionEngine.h`: `CENTER_POINT=79`
- `firmware/v2/src/plugins/api/EffectContext.h`: centre helpers/fields

### Recommended fix
- Introduce a single canonical header (e.g. `config/led_geometry.h`) containing:
  - centre pair constants
  - strip lengths
  - derived helpers
- Replace local literals with includes + `static_assert`s.

---

## 3) Tab5 effect-count drift (CONFIRMED, NOT YET FIXED)

### Conflicting ranges
- `Tab5.encoder/src/config/Config.h` uses 0–103
- `Tab5.encoder/src/parameters/ParameterMap.cpp` uses 0–99

### Recommended fix
- Ingest `effectCount` from v2 status (or effects.list pagination total) and call:
  - `updateParameterMetadata(EFFECT_INDEX, 0, effectCount-1)`
  - and same for zone effect indices.

