# Pattern Registry Code Audit (`PatternRegistry.cpp`)

**Date:** 2025-12-23  
**Scope:** `v2/src/effects/PatternRegistry.cpp` + `v2/src/effects/PatternRegistry.h` usage in firmware (`WebServer.cpp`, `RendererActor.cpp`)  
**Non-scope clarification:** `PatternRegistry.cpp` does **not** contain effect algorithms; it contains **pattern metadata + lookup helpers**. Effect algorithm correctness must be audited in the relevant effect files (e.g. `LGPQuantumEffects.cpp`, `LGPOrganicEffects.cpp`, etc.).

---

## Executive Summary

`PatternRegistry` is structurally sound for **ESP32 Arduino** builds: it provides a stable **effectId → metadata** mapping for the implemented subset of **68 effects**, supports family/tag lookup, and is integrated into the REST/WS API.

However, there are **two critical gaps**:

- **`getRelatedPatterns()` is a stub** and always returns 0 despite being a public API (misleading and breaks any consumer expecting related-pattern graphs).
- The **PROGMEM / pointer semantics are inconsistent**: some functions correctly `memcpy_P()` out of `PATTERN_METADATA`, while other call sites dereference the returned pointer directly. That’s acceptable on ESP32 (flat address space) but is **fragile for portability** and for `native_test`.

There are also **documentation mismatches**, **boundary handling opportunities**, and **maintainability risks** in policy helpers (`isLGPSensitive`, hardcoded IDs + evolving rules).

---

## What is Verified Correct

### 1) ID↔Metadata parity (implemented subset)

- **Runtime registration is explicitly 68 effects in a fixed order** via `registerAllEffects()`:

```545:591:v2/src/effects/CoreEffects.cpp
uint8_t registerAllEffects(RendererActor* renderer) {
  // ...
  // Core effects (13) - IDs 0-12
  total += registerCoreEffects(renderer);
  // LGP Interference effects (5) - IDs 13-17
  total += registerLGPInterferenceEffects(renderer, total);
  // LGP Geometric effects (8) - IDs 18-25
  total += registerLGPGeometricEffects(renderer, total);
  // LGP Advanced effects (8) - IDs 26-33
  total += registerLGPAdvancedEffects(renderer, total);
  // LGP Organic effects (6) - IDs 34-39
  total += registerLGPOrganicEffects(renderer, total);
  // LGP Quantum effects (10) - IDs 40-49
  total += registerLGPQuantumEffects(renderer, total);
  // LGP Color Mixing effects (10) - IDs 50-59
  total += registerLGPColorMixingEffects(renderer, total);
  // LGP Novel Physics effects (5) - IDs 60-64
  total += registerLGPNovelPhysicsEffects(renderer, total);
  // LGP Chromatic effects (3) - IDs 65-67
  total += registerLGPChromaticEffects(renderer, total);
  // ...
}
```

- **Registry enforces “implemented subset only”** by bounding lookups with `EXPECTED_EFFECT_COUNT`:

```106:119:v2/src/effects/PatternRegistry.cpp
constexpr uint8_t EXPECTED_EFFECT_COUNT = 68;
static_assert(PATTERN_METADATA_COUNT >= EXPECTED_EFFECT_COUNT,
              "PATTERN_METADATA_COUNT must be >= EXPECTED_EFFECT_COUNT (metadata must cover all implemented effects)");
```

- `getPatternMetadata(uint8_t)` correctly returns `nullptr` for `index >= EXPECTED_EFFECT_COUNT`:

```138:146:v2/src/effects/PatternRegistry.cpp
const PatternMetadata* getPatternMetadata(uint8_t index) {
  if (index >= EXPECTED_EFFECT_COUNT) {
    return nullptr;
  }
  return &PATTERN_METADATA[index];
}
```

### 2) Family name lookup is safe (for valid family values)

`getFamilyName()` reads from `PATTERN_FAMILY_NAMES[] PROGMEM` and **always NUL terminates** for the valid-path:

```173:180:v2/src/effects/PatternRegistry.cpp
void getFamilyName(PatternFamily family, char* buffer, size_t bufferSize) {
  if ((uint8_t)family < 10) {
    strncpy_P(buffer, (char*)pgm_read_ptr(&PATTERN_FAMILY_NAMES[(uint8_t)family]), bufferSize - 1);
    buffer[bufferSize - 1] = '\0';
  } else {
    strncpy(buffer, "Unknown", bufferSize);
  }
}
```

### 3) Family filtering/counting uses bounded iteration

Both `getPatternsByFamily()` and `getFamilyCount()` iterate only `0..EXPECTED_EFFECT_COUNT-1` and use `memcpy_P()` for reading struct fields:

```148:159:v2/src/effects/PatternRegistry.cpp
for (uint8_t i = 0; i < EXPECTED_EFFECT_COUNT && count < maxOutput; i++) {
  PatternMetadata meta;
  memcpy_P(&meta, &PATTERN_METADATA[i], sizeof(PatternMetadata));
  if (meta.family == family) output[count++] = i;
}
```

---

## Critical Issues (must-fix)

### C1) `getRelatedPatterns()` is unimplemented (API contract violation)

`PatternRegistry.h` exposes related patterns as a feature, but the implementation returns 0 even when `relatedPatterns` exists:

```161:170:v2/src/effects/PatternRegistry.cpp
uint8_t getRelatedPatterns(const char* name, const char** output, uint8_t maxOutput) {
  const PatternMetadata* meta = getPatternMetadata(name);
  if (!meta || !meta->relatedPatterns) return 0;
  uint8_t count = 0;
  // For now, return 0 - full parsing would be implemented here
  return count;
}
```

- **Impact:** Any future UI / choreography / automation consuming related patterns silently gets nothing.
- **Recommended fix:** Implement proper CSV parsing (trim spaces, ignore empty tokens, bound by `maxOutput`), or remove the function until implemented.

### C2) PROGMEM pointer semantics are inconsistent and risky (portability / native tests)

The registry stores metadata in `PROGMEM`:

```18:18:v2/src/effects/PatternRegistry.cpp
const PatternMetadata PATTERN_METADATA[] PROGMEM = { ... };
```

Yet there are two access patterns:

- **Safe pattern:** copy entries out with `memcpy_P()` (used in `getPatternsByFamily`, `getFamilyCount`, and `getPatternMetadata(name)` comparisons).
- **Unsafe/assumption pattern:** return a pointer into `PATTERN_METADATA` and then dereference fields directly (e.g., `WebServer.cpp` uses `meta->family`, `meta->story`, etc.).

On **ESP32**, this is typically fine because PROGMEM is in normal flash-mapped address space. On platforms with separate program memory semantics, this is fragile.

- **Impact:** Unexpected behaviour on other architectures / host builds; inconsistent mental model.
- **Recommended fix (choose one):**
  - **Option A:** Stop using `PROGMEM` for `PatternMetadata` on ESP32 and keep it as `const` in flash (simplest, consistent).
  - **Option B:** Keep PROGMEM and never return raw pointers; instead return a copied `PatternMetadata` value or provide accessors that read via `memcpy_P`.

---

## Warnings (correctness / maintainability risks)

### W1) Documentation mismatches (confusing for future maintainers)

- Header claims “**118+ patterns**” from taxonomy:

```10:14:v2/src/effects/PatternRegistry.h
* Organizes 118+ patterns into 10 families with relationships.
```

- Implementation says “**metadata for all 68 registered patterns**”:

```1:6:v2/src/effects/PatternRegistry.cpp
* Provides metadata for all 68 registered patterns.
```

Both can be true conceptually (taxonomy vs implemented subset), but the current comments don’t explain the implemented-subset strategy clearly.

### W2) `getFamilyName()` “Unknown” branch may not NUL terminate for tiny buffers

For invalid families, it uses `strncpy(buffer, "Unknown", bufferSize);` but does not force `buffer[bufferSize-1]='\0'`. This can leave an unterminated string when `bufferSize` is small.

### W3) `PM_STR` macro is a no-op cast (portability and intent clarity)

```14:16:v2/src/effects/PatternRegistry.cpp
#define PM_STR(s) (const char*)(s)
```

This doesn’t actually place strings into PROGMEM on platforms that require `PSTR()`. On ESP32, string literals are flash-resident anyway, but the macro is misleading.

### W4) `isLGPSensitive()` is mixing policy and metadata in a way that will drift

`isLGPSensitive()` is now a hybrid:
- hardcoded IDs (historically derived from regressions)
- family-based logic
- tag-based logic for physics-based QUANTUM/ORGANIC

```205:243:v2/src/effects/PatternRegistry.cpp
bool isLGPSensitive(uint8_t effectId) {
  // ...
  if (effectId == 10 || effectId == 13 || ...) return true;
  if (metadata->family == PatternFamily::INTERFERENCE) return true;
  if (metadata->family == PatternFamily::ADVANCED_OPTICAL && metadata->hasTag(PatternTags::CENTER_ORIGIN)) return true;
  if ((metadata->family == PatternFamily::QUANTUM || metadata->family == PatternFamily::ORGANIC) &&
      metadata->hasTag(PatternTags::CENTER_ORIGIN) && metadata->hasTag(PatternTags::PHYSICS)) return true;
  return false;
}
```

Meanwhile, the header comment for `isLGPSensitive()` does **not** reflect the QUANTUM/ORGANIC rule:

```173:186:v2/src/effects/PatternRegistry.h
* Includes:
* - INTERFERENCE family effects (10, 13-17)
* - ADVANCED_OPTICAL family effects with CENTER_ORIGIN tag (26-33, 65-67)
* - Specific IDs: 10, 13, 16, 26, 32, 65, 66, 67
```

- **Risk:** Future changes to metadata/families will accidentally change correction policy.
- **Suggestion:** Introduce a dedicated tag like `PatternTags::LGP_SENSITIVE` (or reuse an existing tag) so policy is data-driven and auditable.

---

## Suggestions / Optimisation Opportunities

### S1) Add a “metadata vs registration” validation check at boot

You already validate effect **count** in `registerAllEffects()`; add a **name parity** check:
- For `id in 0..EXPECTED_EFFECT_COUNT-1`:
  - `renderer->getEffectName(id)` should match `PatternRegistry::getPatternMetadata(id)->name`
  - log warning on mismatch

This catches accidental drift immediately.

### S2) Prefer `size_t` for counts and indices if taxonomy grows beyond 255

Several loops use `uint8_t` indices (`PATTERN_METADATA_COUNT` and loop counter). If you later add more than 255 patterns, this becomes unsafe. Consider `uint16_t` or `size_t` for counts/loops (especially if you re-expand toward the 118+ taxonomy).

---

## Verification Steps (current state)

### Unit tests coverage analysis

- There are **unit tests for effect rendering** (`v2/test/unit/test_effects.cpp`), but **no unit tests directly exercising `PatternRegistry`** (no matches in `v2/test/`).
- Recommendation: add `test_pattern_registry.cpp` under `v2/test/unit/` or `v2/test/test_native/` covering:
  - `getPatternMetadata(id)` bounds behaviour
  - `getPatternMetadata(name)` lookup correctness
  - `getPatternsByFamily()` returns correct IDs/counts
  - `getFamilyName()` returns expected strings and always terminates
  - `isLGPSensitive()` policy matches tags/expected cases

### Runtime behaviour validation

- REST handler uses `PatternRegistry::getPatternMetadata(effectId)` and reads fields directly:

```1688:1694:v2/src/network/WebServer.cpp
const PatternMetadata* meta = PatternRegistry::getPatternMetadata(effectId);
if (meta) {
  PatternRegistry::getFamilyName(meta->family, familyName, sizeof(familyName));
  data["family"] = familyName;
  data["familyId"] = static_cast<uint8_t>(meta->family);
}
```

This works on ESP32; if portability is required, adjust per **C2**.

### Memory usage profiling

- `PATTERN_METADATA` stored in flash (good).
- The registry functions create small stack temporaries (`PatternMetadata meta`) for `memcpy_P` copies.
- No heap allocations inside registry.

### Cross-platform compatibility checks

- Current design assumes Arduino PROGMEM helpers exist (`memcpy_P`, `strcmp_P`, `strncpy_P`).
- For host (`native_test`) builds, mocks/headers must provide these; otherwise, compilation may break.
- If you want truly portable host tests for PatternRegistry, consider **Option A** in **C2** (remove PROGMEM usage for PatternMetadata in ESP32 builds) and isolate AVR-specific PROGMEM behind `#ifdef`.

---

## Prioritised Recommendations

1. **Implement `getRelatedPatterns()` or remove it until implemented** (Critical).
2. **Choose a consistent PROGMEM strategy** (Critical for portability/consistency).
3. **Align header documentation to the implemented-subset strategy (68 vs 118+)** (Warning).
4. **Make `isLGPSensitive()` policy data-driven (tag-based) and update its header comment** (Warning).
5. **Add PatternRegistry unit tests** (Suggestion; prevents drift regressions).


