# Defensive Bounds Checking

## Overview

LightwaveOS v2 implements comprehensive defensive bounds checking to prevent memory access violations (LoadProhibited crashes) that can occur from corrupted indices, invalid input, or race conditions. This document describes the validation pattern used throughout the codebase.

## Philosophy

**Defensive Programming Principle**: Always validate array indices and input parameters before use, even if they "should" be valid. Memory corruption, race conditions, and invalid input can cause indices to be out of bounds, leading to system crashes.

## Validation Pattern

### Standard Function Signature

All validation functions follow this pattern:

```cpp
/**
 * @brief Validate and clamp [parameter] to safe range [min, max]
 * 
 * DEFENSIVE CHECK: Prevents LoadProhibited crashes from corrupted [parameter].
 * 
 * [Explanation of why this check is necessary]
 * 
 * @param [param] [Description]
 * @return Valid [param] in [range], defaults to [safe_default] if out of bounds
 */
[type] validate[Param]([type] [param]) const {
    if ([param] >= [MAX]) {
        return [SAFE_DEFAULT];  // Return safe default if corrupted
    }
    return [param];
}
```

### Key Characteristics

1. **Always returns a valid value** - Never throws exceptions or returns invalid indices
2. **Safe defaults** - Returns sensible fallback values (typically 0 for indices)
3. **Const correctness** - Validation functions are `const` where possible
4. **Inline where appropriate** - Small validation functions are often `inline`
5. **Comprehensive comments** - Every validation function explains why it's necessary

## Examples from Codebase

### Example 1: Effect ID Validation

**Location**: `v2/src/core/actors/RendererActor.cpp`

```cpp
/**
 * @brief Validate and clamp effectId to safe range [0, MAX_EFFECTS-1]
 * 
 * DEFENSIVE CHECK: Prevents LoadProhibited crashes from corrupted effect ID.
 * 
 * RendererActor uses m_effects[MAX_EFFECTS] array where MAX_EFFECTS = 80. If
 * effectId is corrupted (e.g., by memory corruption, invalid input, or race
 * condition), accessing m_effects[effectId] would cause out-of-bounds access
 * and crash.
 * 
 * @param effectId Effect ID to validate
 * @return Valid effect ID in [0, MAX_EFFECTS-1], defaults to 0 if out of bounds
 */
uint8_t RendererActor::validateEffectId(uint8_t effectId) const {
    if (effectId >= MAX_EFFECTS) {
        return 0;  // Return safe default (effect 0)
    }
    return effectId;
}
```

**Usage**:
```cpp
void RendererActor::handleSetEffect(uint8_t effectId) {
    uint8_t safeEffectId = validateEffectId(effectId);  // Validate before access
    if (!m_effects[safeEffectId].active) {
        return;
    }
    // ... use safeEffectId for all m_effects array accesses ...
}
```

### Example 2: Zone ID Validation

**Location**: `v2/src/effects/zones/ZoneComposer.cpp`

```cpp
/**
 * @brief Validate and clamp zoneId to safe range [0, MAX_ZONES-1]
 * 
 * DEFENSIVE CHECK: Prevents LoadProhibited crashes from corrupted zone ID.
 * 
 * ZoneComposer uses arrays m_zones[MAX_ZONES] and m_zoneConfig[MAX_ZONES] where
 * MAX_ZONES = 4. If zoneId is corrupted (e.g., by memory corruption, invalid
 * input, or uninitialized state), accessing these arrays would cause out-of-bounds
 * access and crash.
 * 
 * @param zoneId Zone ID to validate
 * @return Valid zone ID in [0, MAX_ZONES-1], defaults to 0 if out of bounds
 */
uint8_t ZoneComposer::validateZoneId(uint8_t zoneId) const {
    if (zoneId >= MAX_ZONES) {
        return 0;  // Return safe default (zone 0)
    }
    return zoneId;
}
```

### Example 3: Tempo Bin Validation

**Location**: `v2/src/audio/tempo/TempoTracker.cpp`

```cpp
/**
 * @brief Validate and clamp winner_bin_ to safe range [0, NUM_TEMPI-1]
 * 
 * DEFENSIVE CHECK: Prevents LoadProhibited crashes from corrupted winner_bin_ index.
 * 
 * If winner_bin_ is corrupted (e.g., by memory corruption, race condition, or
 * uninitialized state), accessing tempi_[winner_bin_] would cause an out-of-bounds
 * memory access and crash the system.
 * 
 * This validation ensures we always access valid array indices, returning a safe
 * default (centre of BPM range) if corruption is detected.
 * 
 * @return Valid bin index, clamped to [0, NUM_TEMPI-1]
 */
uint16_t TempoTracker::validateWinnerBin() const {
    if (winner_bin_ >= NUM_TEMPI) {
        return NUM_TEMPI / 2;  // Return safe default (centre of BPM range)
    }
    return winner_bin_;
}
```

**Note**: This example returns `NUM_TEMPI / 2` instead of `0` because the centre of the BPM range is a more sensible default for tempo tracking.

## When to Add Validation

### Always Validate:

1. **Array indices from external input** (network requests, user input)
2. **Array indices from shared state** (volatile variables, atomic variables)
3. **Array indices calculated from other values** (offsets, computed indices)
4. **Array indices in hot loops** (render loops, audio processing)

### Validation Not Required:

1. **Loop indices in bounded loops** (e.g., `for (int i = 0; i < arraySize; i++)`)
2. **Compile-time constants** (e.g., `array[0]`, `array[CONSTANT]`)
3. **Indices already validated** (don't validate twice)

## Best Practices

### ✅ Good: Validate Before Every Access

```cpp
void renderEffect(uint8_t effectId) {
    uint8_t safeId = validateEffectId(effectId);  // Validate once
    if (m_effects[safeId].active) {
        m_effects[safeId].effect->render(ctx);  // Use validated ID
    }
}
```

### ❌ Bad: Direct Array Access

```cpp
void renderEffect(uint8_t effectId) {
    if (m_effects[effectId].active) {  // BAD: No validation!
        m_effects[effectId].effect->render(ctx);
    }
}
```

### ✅ Good: Validate in Helper Functions

```cpp
// In RequestValidator.h
inline uint8_t validateEffectIdInRequest(uint8_t effectId) {
    constexpr uint8_t MAX_EFFECTS = 80;
    if (effectId >= MAX_EFFECTS) {
        return 0;
    }
    return effectId;
}

// In handler
void handleSetEffect(uint8_t effectId) {
    effectId = validateEffectIdInRequest(effectId);  // Reusable validation
    // ... use effectId ...
}
```

### ❌ Bad: Duplicate Validation Logic

```cpp
// BAD: Validation logic duplicated in multiple places
void handler1(uint8_t id) {
    if (id >= 80) id = 0;  // Duplicate logic
    // ...
}

void handler2(uint8_t id) {
    if (id >= 80) id = 0;  // Duplicate logic
    // ...
}
```

## Anti-Patterns to Avoid

### 1. Validating After Access

```cpp
// BAD: Validation happens too late
m_effects[effectId].active = true;  // Crash if effectId is invalid!
if (effectId >= MAX_EFFECTS) {     // Too late!
    // ...
}
```

### 2. Assuming Input is Valid

```cpp
// BAD: Assumes network input is always valid
void handleRequest(uint8_t effectId) {
    m_effects[effectId].active = true;  // Could crash!
}
```

### 3. Inconsistent Safe Defaults

```cpp
// BAD: Different defaults in different places
uint8_t validate1(uint8_t id) { return id >= MAX ? 0 : id; }      // Default 0
uint8_t validate2(uint8_t id) { return id >= MAX ? 255 : id; }     // Default 255 - inconsistent!
```

## Migration Guide

### Adding Validation to Existing Code

1. **Identify array accesses** - Find all `array[index]` patterns
2. **Create validation function** - Follow the standard pattern
3. **Apply validation** - Call validation before every array access
4. **Add comments** - Document why validation is necessary
5. **Test** - Verify behavior with invalid input

### Example Migration

**Before**:
```cpp
void setZoneEffect(uint8_t zoneId, uint8_t effectId) {
    m_zones[zoneId].effectId = effectId;  // No validation
}
```

**After**:
```cpp
// Add validation function
uint8_t ZoneComposer::validateZoneId(uint8_t zoneId) const {
    if (zoneId >= MAX_ZONES) {
        return 0;
    }
    return zoneId;
}

// Use validation
void setZoneEffect(uint8_t zoneId, uint8_t effectId) {
    uint8_t safeZone = validateZoneId(zoneId);  // Validate before access
    m_zones[safeZone].effectId = effectId;
}
```

## Performance Impact

Validation functions are designed to be extremely lightweight:

- **CPU Cost**: 1-2 CPU cycles per validation (simple comparison)
- **Memory Cost**: Zero (all validation is stack-based, no heap allocation)
- **Impact**: Negligible (<0.1% CPU overhead even with 10+ validations per frame)

See `v2/docs/performance/VALIDATION_OVERHEAD.md` for detailed performance analysis.

## Related Documentation

- `ARCHITECTURE.md` - Main architecture guide (includes defensive programming section)
- `docs/security/ARCHITECTURE_SECURITY_REVIEW.md` - Security architecture (memory safety)
- `docs/architecture/06_ERROR_HANDLING_FAULT_TOLERANCE.md` - Error handling patterns

## Summary

Defensive bounds checking is a **non-negotiable requirement** in LightwaveOS v2. All array accesses must be validated before use to prevent memory access violations. The validation pattern is simple, consistent, and has negligible performance impact while providing critical safety guarantees.

