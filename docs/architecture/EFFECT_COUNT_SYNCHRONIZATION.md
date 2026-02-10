# Effect Count Synchronization Architecture

## Executive Summary

**Problem**: Hardcoded effect and palette counts are scattered across multiple files, causing systemic drift when new effects are added. Example: `RendererActor::MAX_EFFECTS = 113`, but `WebServer::CachedRendererState::effectNames[102]` and hardcoded caps at 102/96.

**Impact**: New effects become invisible to clients, array bounds violations, compile-time/runtime mismatches.

**Solution**: Centralised effect registry with compile-time validation, runtime self-healing, and zero manual synchronisation.

---

## 1. Current Architecture Analysis

### 1.1 Identified Synchronisation Points

| File | Symbol | Value | Purpose |
|------|--------|-------|---------|
| `RendererActor.h:605` | `MAX_EFFECTS` | `113` | Effect registry array size |
| `PatternRegistry.cpp:180` | `EXPECTED_EFFECT_COUNT` | `113` | Metadata validation |
| `PatternRegistry.cpp:173` | `PATTERN_METADATA_COUNT` | `sizeof(PATTERN_METADATA) / sizeof(PatternMetadata)` | Metadata array size |
| `WebServer.h:287` | `CachedRendererState::MAX_CACHED_EFFECTS` | `113` | WebSocket cache array size |
| `CoreEffects.cpp:1060` | `EXPECTED_EFFECT_COUNT` | `113` | Registration validation |

**Good**: PatternRegistry uses `static_assert` to validate metadata coverage.
**Bad**: WebServer hardcodes effect count independently.
**Ugly**: Three separate `EXPECTED_EFFECT_COUNT` constants (two in PatternRegistry.cpp, one in CoreEffects.cpp).

### 1.2 Palette Count Analysis

Similar issue exists for palettes, though less severe:

| File | Symbol | Value |
|------|--------|-------|
| `Palettes_Master.h` | `PALETTE_COUNT` | `75` (likely) |
| `WebServer` | Hardcoded assumptions | Unknown |

---

## 2. Architectural Solution

### 2.1 Single Source of Truth

**Principle**: Effect count must be defined ONCE, in a central location, and propagated at compile-time.

**Recommended Location**: `firmware/v2/src/effects/EffectRegistry.h` (new file)

**Rationale**:
- Effects are defined in `CoreEffects.cpp` during registration
- Registry header can be included by all consumers
- Clear semantic ownership (effects subsystem)

### 2.2 Implementation Design

#### Phase 1: Centralised Constants

Create `src/effects/EffectRegistry.h`:

```cpp
#pragma once

namespace lightwaveos {
namespace effects {

/**
 * @brief Effect Registry Configuration
 *
 * SINGLE SOURCE OF TRUTH for effect counts.
 * All effect-related arrays MUST use these constants.
 *
 * When adding a new effect:
 * 1. Increment EFFECT_COUNT
 * 2. Register effect in CoreEffects.cpp
 * 3. Add metadata entry to PatternRegistry.cpp
 * 4. Compile - static_assert will validate consistency
 */
struct EffectRegistryConfig {
    /**
     * @brief Total number of registered effects
     *
     * This is the ONLY place to update effect count.
     * All arrays sized by effect count MUST reference this constant.
     *
     * Current: 113 effects (0-112)
     * Last updated: 2026-02-07 (Beat Pulse Shockwave variants added)
     */
    static constexpr uint8_t EFFECT_COUNT = 113;

    /**
     * @brief Maximum effect ID (inclusive)
     *
     * Derived from EFFECT_COUNT. Use for bounds checking.
     */
    static constexpr uint8_t MAX_EFFECT_ID = EFFECT_COUNT - 1;

    /**
     * @brief Reserve capacity for future effects
     *
     * Allows metadata array to be larger than implemented effects
     * without breaking compile-time validation. Useful for pre-defining
     * metadata for effects under development.
     */
    static constexpr uint8_t METADATA_RESERVE_CAPACITY = 10;
};

/**
 * @brief Palette Registry Configuration
 *
 * SINGLE SOURCE OF TRUTH for palette counts.
 */
struct PaletteRegistryConfig {
    /**
     * @brief Total number of registered palettes
     *
     * Current: 75 palettes (33 CPT-City + 24 Crameri + 18 Colorspace)
     */
    static constexpr uint8_t PALETTE_COUNT = 75;

    /**
     * @brief Maximum palette ID (inclusive)
     */
    static constexpr uint8_t MAX_PALETTE_ID = PALETTE_COUNT - 1;
};

} // namespace effects
} // namespace lightwaveos
```

#### Phase 2: Enforce Compile-Time Validation

**RendererActor.h** (line 605):

```cpp
#include "../../effects/EffectRegistry.h"

class RendererActor : public Actor, public plugins::IEffectRegistry {
    // ...

    // COMPILE-TIME SYNCHRONIZED CAPACITY
    // This value is automatically synchronized with EffectRegistry.h
    // DO NOT modify this value - update EffectRegistry.h instead
    static constexpr uint8_t MAX_EFFECTS = effects::EffectRegistryConfig::EFFECT_COUNT;

    EffectEntry m_effects[MAX_EFFECTS];
    // ...
};
```

**PatternRegistry.cpp** (line 180):

```cpp
#include "EffectRegistry.h"

// Compile-time assertion: metadata must cover all implemented effects
static_assert(PATTERN_METADATA_COUNT >= effects::EffectRegistryConfig::EFFECT_COUNT,
              "PATTERN_METADATA must have entries for all registered effects");

// Compile-time assertion: metadata doesn't exceed reserve capacity
static_assert(PATTERN_METADATA_COUNT <=
              effects::EffectRegistryConfig::EFFECT_COUNT +
              effects::EffectRegistryConfig::METADATA_RESERVE_CAPACITY,
              "PATTERN_METADATA exceeds reserve capacity - update EffectRegistry.h");

// Remove duplicate EXPECTED_EFFECT_COUNT - use EffectRegistryConfig::EFFECT_COUNT instead
```

**WebServer.h** (line 287):

```cpp
#include "../effects/EffectRegistry.h"

struct CachedRendererState {
    // ...

    // COMPILE-TIME SYNCHRONIZED CAPACITY
    // This value is automatically synchronized with EffectRegistry.h
    // DO NOT modify this value - update EffectRegistry.h instead
    static constexpr uint8_t MAX_CACHED_EFFECTS =
        effects::EffectRegistryConfig::EFFECT_COUNT;

    const char* effectNames[MAX_CACHED_EFFECTS];
    // ...
};
```

**CoreEffects.cpp** (line 1060):

```cpp
#include "EffectRegistry.h"

// Runtime validation: ensure registered count matches expected
if (total != effects::EffectRegistryConfig::EFFECT_COUNT) {
    Serial.printf("[ERROR] Effect count mismatch: registered %d, expected %d\n",
                  total, effects::EffectRegistryConfig::EFFECT_COUNT);
    Serial.printf("[ACTION REQUIRED] Update EffectRegistry.h EFFECT_COUNT to %d\n", total);
} else {
    Serial.printf("[OK] Effect count validated: %d effects registered\n", total);
}
```

---

## 3. Runtime Self-Healing Strategies

### 3.1 Defensive Bounds Checking

**Principle**: Never trust effect IDs from external sources (WebSocket, REST API, NVS).

**PatternRegistry.cpp** (already implemented):

```cpp
uint8_t validateEffectId(uint8_t effectId) {
    if (effectId >= effects::EffectRegistryConfig::EFFECT_COUNT) {
        Serial.printf("[WARN] Effect ID %d exceeds count %d, clamping to 0\n",
                      effectId, effects::EffectRegistryConfig::EFFECT_COUNT);
        return 0;  // Safe default
    }
    return effectId;
}
```

**Recommendation**: Add similar validation to:
- `RendererActor::validateEffectId()` (line 281) - already exists
- All WebSocket command handlers that accept effect IDs
- All REST API handlers that accept effect IDs

### 3.2 Dynamic Effect Count Reporting

**Principle**: Clients should query effect count at runtime, not hardcode it.

**API Enhancement** - Add to `/api/v1/effects`:

```json
{
  "effectCount": 113,
  "maxEffectId": 112,
  "effects": [...]
}
```

**WebSocket Enhancement** - Add to `device.status`:

```json
{
  "capabilities": {
    "effectCount": 113,
    "paletteCount": 75,
    "maxEffectId": 112,
    "maxPaletteId": 74
  }
}
```

### 3.3 NVS Preset Validation

**Problem**: Effect presets stored in NVS may reference deleted effects.

**Solution**: Validate and sanitise presets on load:

```cpp
// In EffectPresetManager.cpp
bool EffectPresetManager::loadPreset(uint8_t slot, EffectPreset& preset) {
    // ... load from NVS ...

    // Sanitise effect ID
    if (preset.effectId >= effects::EffectRegistryConfig::EFFECT_COUNT) {
        Serial.printf("[WARN] Preset %d references deleted effect %d, resetting to 0\n",
                      slot, preset.effectId);
        preset.effectId = 0;
    }

    return true;
}
```

---

## 4. Compile-Time Enforcement Mechanisms

### 4.1 Static Assertions

**RendererActor.h**:

```cpp
// Ensure effect array capacity matches registry
static_assert(MAX_EFFECTS == effects::EffectRegistryConfig::EFFECT_COUNT,
              "RendererActor::MAX_EFFECTS must match EffectRegistryConfig::EFFECT_COUNT");
```

**WebServer.h**:

```cpp
// Ensure cache capacity matches registry
static_assert(CachedRendererState::MAX_CACHED_EFFECTS ==
              effects::EffectRegistryConfig::EFFECT_COUNT,
              "CachedRendererState must match EffectRegistryConfig::EFFECT_COUNT");
```

**PatternRegistry.cpp**:

```cpp
// Ensure metadata covers all effects
static_assert(PATTERN_METADATA_COUNT >= effects::EffectRegistryConfig::EFFECT_COUNT,
              "PATTERN_METADATA must cover all registered effects");
```

### 4.2 Runtime Assertions

**CoreEffects.cpp** `registerAllEffects()`:

```cpp
uint8_t registerAllEffects(RendererActor* renderer) {
    // ... registration code ...

    // MANDATORY: Validate final count matches registry
    if (total != effects::EffectRegistryConfig::EFFECT_COUNT) {
        Serial.printf("[FATAL] Effect registration mismatch: %d != %d\n",
                      total, effects::EffectRegistryConfig::EFFECT_COUNT);
        Serial.printf("[ACTION] Update EffectRegistry.h or fix registration loop\n");
        // In debug builds, halt
        #ifdef DEBUG
        assert(false && "Effect count mismatch - update EffectRegistry.h");
        #endif
    }

    return total;
}
```

---

## 5. Developer Workflow

### 5.1 Adding a New Effect

**Old Workflow** (error-prone):
1. Add effect implementation
2. Register in `CoreEffects.cpp`
3. **FORGET** to update `RendererActor::MAX_EFFECTS`
4. **FORGET** to update `PatternRegistry::EXPECTED_EFFECT_COUNT`
5. **FORGET** to update `WebServer::MAX_CACHED_EFFECTS`
6. **FORGET** to add metadata entry
7. Build succeeds, effect invisible to clients

**New Workflow** (fail-fast):
1. Add effect implementation
2. Register in `CoreEffects.cpp`
3. Add metadata entry to `PatternRegistry.cpp`
4. **Increment** `EffectRegistry.h::EFFECT_COUNT` by 1
5. Build - if metadata missing, **static_assert fails at compile time**
6. Build - if registration missing, **runtime assertion prints error**

### 5.2 Build-Time Feedback

**Scenario**: Developer adds effect but forgets metadata entry.

**Old Behaviour**: Compiles, crashes at runtime with `LoadProhibited`.

**New Behaviour**:
```
PatternRegistry.cpp:183: error: static assertion failed:
  "PATTERN_METADATA must have entries for all registered effects"
static_assert(PATTERN_METADATA_COUNT >= effects::EffectRegistryConfig::EFFECT_COUNT,
              ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Build FAILED - 1 error
```

**Scenario**: Developer updates `EFFECT_COUNT` but forgets to register effect.

**Old Behaviour**: Compiles, registration count mismatch ignored.

**New Behaviour**:
```
[FATAL] Effect registration mismatch: 112 != 113
[ACTION] Update EffectRegistry.h or fix registration loop
```

---

## 6. Migration Plan

### 6.1 Phase 1: Create Central Registry (Low Risk)

1. Create `src/effects/EffectRegistry.h` with current counts
2. Add static assertions to all consumers
3. Build - verify no regressions
4. **Commit**: "feat(effects): add centralised effect registry"

### 6.2 Phase 2: Eliminate Hardcoded Constants (Medium Risk)

1. Replace `RendererActor::MAX_EFFECTS` with `EffectRegistryConfig::EFFECT_COUNT`
2. Replace `WebServer::MAX_CACHED_EFFECTS` with `EffectRegistryConfig::EFFECT_COUNT`
3. Replace all `EXPECTED_EFFECT_COUNT` with `EffectRegistryConfig::EFFECT_COUNT`
4. Build - verify static assertions pass
5. **Commit**: "refactor(effects): use centralised registry for all effect counts"

### 6.3 Phase 3: Add Runtime Validation (Low Risk)

1. Add effect count to `/api/v1/effects` response
2. Add effect count to WebSocket `device.status`
3. Add preset validation to `EffectPresetManager`
4. **Commit**: "feat(effects): add runtime effect count validation and reporting"

### 6.4 Phase 4: Palette Registry (Low Risk)

1. Add palette count to `EffectRegistry.h` as `PaletteRegistryConfig`
2. Replace hardcoded palette counts
3. Add static assertions for palette arrays
4. **Commit**: "feat(palettes): add centralised palette registry"

---

## 7. Testing Strategy

### 7.1 Compile-Time Tests

**Test**: Metadata array too small
```cpp
// In PatternRegistry.cpp, temporarily reduce PATTERN_METADATA array size
const PatternMetadata PATTERN_METADATA[100] PROGMEM = { ... };

// Expected: Compile error
// static_assert(PATTERN_METADATA_COUNT >= effects::EffectRegistryConfig::EFFECT_COUNT)
```

**Test**: Cache array too small
```cpp
// In WebServer.h, temporarily reduce MAX_CACHED_EFFECTS
static constexpr uint8_t MAX_CACHED_EFFECTS = 100;

// Expected: Compile error
// static_assert(MAX_CACHED_EFFECTS == effects::EffectRegistryConfig::EFFECT_COUNT)
```

### 7.2 Runtime Tests

**Test**: Registration count mismatch
```cpp
// In CoreEffects.cpp, temporarily skip one effect registration
// if (false && renderer->registerEffect(total, &effectInstance)) { total++; }

// Expected: Runtime error
// [FATAL] Effect registration mismatch: 112 != 113
```

**Test**: Invalid effect ID from API
```bash
curl -X POST http://lightwaveos.local/api/v1/effects/200

# Expected: 400 Bad Request
# { "error": "Effect ID 200 exceeds maximum 112" }
```

### 7.3 Integration Tests

**Test**: Effect count reported correctly
```bash
curl http://lightwaveos.local/api/v1/effects | jq '.effectCount'
# Expected: 113
```

**Test**: WebSocket capabilities
```javascript
ws.send(JSON.stringify({ type: "device.getStatus" }));
// Expected: { capabilities: { effectCount: 113, maxEffectId: 112 } }
```

---

## 8. Long-Term Architectural Considerations

### 8.1 Dynamic Effect Registration

**Vision**: Plugin-based effect system where effects can be loaded/unloaded at runtime.

**Implications**:
- Effect count becomes runtime variable, not compile-time constant
- Registry must support dynamic resizing or fixed capacity with occupancy tracking
- WebSocket clients must receive `effectList.changed` events
- Preset validation becomes more complex (effect might exist during save, deleted during load)

**Recommendation**: Current compile-time approach is correct for v2. Dynamic registration is v3+ feature.

### 8.2 Effect Metadata Separation

**Observation**: `PatternRegistry.cpp` duplicates effect names from `CoreEffects.cpp`.

**Future Enhancement**: Generate metadata from IEffect instances at runtime:
```cpp
const EffectMetadata& meta = effect->getMetadata();
const char* name = meta.name;
const char* description = meta.description;
```

**Benefit**: Single source of truth for effect metadata (IEffect class).

**Blocker**: Legacy effects still use function pointers, not IEffect. Wait until all effects migrated.

### 8.3 Code Generation

**Idea**: Generate `EffectRegistry.h` from `CoreEffects.cpp` during build.

**Tool**: Python script parses `registerEffect()` calls, counts effects, generates header.

**Benefit**: Zero-maintenance - effect count always correct.

**Risk**: Build complexity, harder to debug generated code.

**Recommendation**: Defer until effect count exceeds 200 and manual sync becomes unbearable.

---

## 9. Success Criteria

### 9.1 Compile-Time Validation

- [ ] Adding effect without metadata → compile error
- [ ] Reducing `MAX_EFFECTS` below `EFFECT_COUNT` → compile error
- [ ] Reducing `MAX_CACHED_EFFECTS` below `EFFECT_COUNT` → compile error

### 9.2 Runtime Validation

- [ ] Registration count mismatch → runtime error logged
- [ ] Invalid effect ID from API → 400 error returned
- [ ] Invalid effect ID in preset → sanitised to 0 on load

### 9.3 Developer Experience

- [ ] Single location to update effect count (`EffectRegistry.h`)
- [ ] Clear error messages when counts drift
- [ ] Zero manual synchronisation required

### 9.4 Client Robustness

- [ ] Effect count available via REST API
- [ ] Effect count available via WebSocket
- [ ] Clients can query capabilities dynamically

---

## 10. Appendix: File Change Summary

### Files to Create

| File | Purpose |
|------|---------|
| `src/effects/EffectRegistry.h` | Central effect/palette count registry |

### Files to Modify

| File | Change |
|------|--------|
| `src/core/actors/RendererActor.h` | Replace `MAX_EFFECTS = 113` with `EffectRegistryConfig::EFFECT_COUNT` |
| `src/effects/PatternRegistry.cpp` | Replace `EXPECTED_EFFECT_COUNT` with `EffectRegistryConfig::EFFECT_COUNT` |
| `src/effects/CoreEffects.cpp` | Replace `EXPECTED_EFFECT_COUNT` with `EffectRegistryConfig::EFFECT_COUNT` |
| `src/network/WebServer.h` | Replace `MAX_CACHED_EFFECTS = 113` with `EffectRegistryConfig::EFFECT_COUNT` |
| `src/network/webserver/handlers/EffectHandlers.cpp` | Add effect count to API response |
| `src/network/WebServer.cpp` | Add effect count to `device.status` broadcast |

### Build Verification

```bash
cd firmware/v2
pio run -e esp32dev_audio_esv11  # Must succeed with no warnings
```

---

## 11. Conclusion

This architecture eliminates the systemic drift problem by:

1. **Single Source of Truth**: `EffectRegistry.h` defines all counts
2. **Compile-Time Enforcement**: Static assertions prevent drift
3. **Runtime Self-Healing**: Defensive validation prevents crashes
4. **Zero Manual Sync**: Updating one constant synchronises all consumers

**Estimated Implementation Time**: 2-3 hours for full migration and testing.

**Risk Level**: Low - changes are additive and validated at compile-time.

**Maintenance Burden**: Near-zero - developers only update one constant when adding effects.

---

**Document Version**: 1.0
**Last Updated**: 2026-02-07
**Author**: Software Architect (Claude Sonnet 4.5)
**Status**: Architecture Design - Pending Implementation
