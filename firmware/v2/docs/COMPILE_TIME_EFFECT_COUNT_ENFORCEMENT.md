# Compile-Time Effect Count Enforcement

## Problem Statement

ESP32 firmware previously had hardcoded effect counts scattered across multiple files that could drift out of sync:

```cpp
// RendererActor.h (was correct)
static constexpr uint8_t MAX_EFFECTS = 113;

// WebServer.h (was WRONG - old value!)
const char* effectNames[102];  // Should be 113!

// PatternRegistry.cpp (was hardcoded)
constexpr uint8_t EXPECTED_EFFECT_COUNT = 113;

// AudioEffectMapping.h (was duplicated)
static constexpr uint8_t MAX_EFFECTS = 113;
```

When adding new effects, developers had to manually update 4+ locations. Missing one caused runtime crashes or silent bugs.

## Solution Architecture

### Single Source of Truth

**File:** `/src/config/limits.h`

```cpp
namespace lightwaveos {
namespace limits {

/**
 * @brief Maximum number of registered effects - SINGLE SOURCE OF TRUTH
 *
 * Referenced by:
 * - RendererActor.h: m_effects[MAX_EFFECTS] array
 * - WebServer.h: CachedRendererState::effectNames[MAX_EFFECTS]
 * - AudioEffectMapping.h: AudioMappingRegistry::MAX_EFFECTS
 * - PatternRegistry.cpp: EXPECTED_EFFECT_COUNT validation
 * - BuiltinEffectRegistry.h: Static assertion for headroom
 */
constexpr uint8_t MAX_EFFECTS = 113;

} // namespace limits
} // namespace lightwaveos
```

### Compile-Time Enforcement

All array sizes reference `limits::MAX_EFFECTS`:

#### 1. RendererActor.h
```cpp
#include "../config/limits.h"

class RendererActor {
    static constexpr uint8_t MAX_EFFECTS = limits::MAX_EFFECTS;
    EffectEntry m_effects[MAX_EFFECTS];
    plugins::runtime::LegacyEffectAdapter* m_legacyAdapters[MAX_EFFECTS];
};
```

#### 2. WebServer.h
```cpp
struct CachedRendererState {
    static constexpr uint8_t MAX_CACHED_EFFECTS = limits::MAX_EFFECTS;
    const char* effectNames[MAX_CACHED_EFFECTS];
};
```

#### 3. AudioEffectMapping.h
```cpp
#include "../../config/limits.h"

class AudioMappingRegistry {
public:
    static constexpr uint8_t MAX_EFFECTS = limits::MAX_EFFECTS;
    // ...
};
```

#### 4. PatternRegistry.cpp
```cpp
#include "../config/limits.h"

constexpr uint8_t EXPECTED_EFFECT_COUNT = limits::MAX_EFFECTS;

static_assert(PATTERN_METADATA_COUNT >= EXPECTED_EFFECT_COUNT,
              "PATTERN_METADATA_COUNT must be >= EXPECTED_EFFECT_COUNT");
```

#### 5. BuiltinEffectRegistry.h
```cpp
#include "../config/limits.h"

class BuiltinEffectRegistry {
public:
    static constexpr uint8_t MAX_EFFECTS = 128;  // Headroom for growth
    static_assert(MAX_EFFECTS >= limits::MAX_EFFECTS,
                  "BuiltinEffectRegistry::MAX_EFFECTS must be >= limits::MAX_EFFECTS");
};
```

#### 6. CoreEffects.cpp (Runtime Validation)
```cpp
#include "../config/limits.h"

uint8_t registerAllEffects(RendererActor& renderer) {
    // ... register all effects ...

    constexpr uint8_t EXPECTED_EFFECT_COUNT = limits::MAX_EFFECTS;
    if (total != EXPECTED_EFFECT_COUNT) {
        Serial.printf("[WARNING] Effect count mismatch: registered %d, expected %d\n",
                      total, EXPECTED_EFFECT_COUNT);
    }
    return total;
}
```

## ESP32/Arduino Compatibility

### C++17 Features Used

- `inline constexpr` (C++17): Header-only constant definitions without ODR violations
- `static_assert`: Compile-time validation (available in C++11+)
- Template type aliases (future extension): `template<typename T> using EffectArray = T[MAX_EFFECTS];`

### PROGMEM Compatible

The pattern works with PROGMEM arrays in flash storage:

```cpp
const PatternMetadata PATTERN_METADATA[] PROGMEM = { /* ... */ };
const uint8_t PATTERN_METADATA_COUNT = sizeof(PATTERN_METADATA) / sizeof(PatternMetadata);

static_assert(PATTERN_METADATA_COUNT >= limits::MAX_EFFECTS,
              "Metadata array must cover all effects");
```

### Zero Runtime Overhead

All array sizing and validation happens at compile time:
- Array sizes resolved during compilation
- `static_assert` evaluated by compiler (no runtime code)
- `constexpr` values inlined (no memory allocation)

## How to Add a New Effect

### Step 1: Update limits.h

```cpp
// Change this line in /src/config/limits.h
constexpr uint8_t MAX_EFFECTS = 114;  // Was 113
```

### Step 2: Add Effect Implementation

Create the effect class (e.g., `MyNewEffect.h`/`.cpp`) following the `IEffect` interface.

### Step 3: Register Effect

In `CoreEffects.cpp`:

```cpp
uint8_t registerAllEffects(RendererActor& renderer) {
    // ... existing effects ...

    // Add new effect
    renderer.registerEffect("My New Effect", new MyNewEffect());
    total++;

    // Runtime validation will catch mismatch
    constexpr uint8_t EXPECTED_EFFECT_COUNT = limits::MAX_EFFECTS;
    if (total != EXPECTED_EFFECT_COUNT) {
        Serial.printf("[WARNING] Effect count mismatch: registered %d, expected %d\n",
                      total, EXPECTED_EFFECT_COUNT);
    }
    return total;
}
```

### Step 4: Add Metadata

In `PatternRegistry.cpp`:

```cpp
const PatternMetadata PATTERN_METADATA[] PROGMEM = {
    // ... existing metadata ...
    {PM_STR("My New Effect"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN,
     PM_STR("Description"), PM_STR("Technical details"), PM_STR("")}
};
```

### Step 5: Build & Verify

```bash
pio run -e esp32dev_audio_esv11
```

**Compile-time checks:**
- If `PATTERN_METADATA_COUNT < limits::MAX_EFFECTS`: **BUILD FAILS** with static_assert
- If `BuiltinEffectRegistry::MAX_EFFECTS < limits::MAX_EFFECTS`: **BUILD FAILS**

**Runtime validation:**
- If `registerAllEffects()` registers fewer effects than `limits::MAX_EFFECTS`: **WARNING** printed to serial

## Benefits

### 1. Compile-Time Safety
```
❌ Before: Mismatched counts caused runtime crashes
✅ After:  Build fails if arrays are undersized
```

### 2. Single Update Point
```
❌ Before: Update 4+ files manually
✅ After:  Update 1 constant in limits.h
```

### 3. Self-Documenting
```cpp
// limits.h clearly states what references it:
// "Referenced by: RendererActor.h, WebServer.h, AudioEffectMapping.h..."
```

### 4. Future-Proof
```cpp
// Easy to add new arrays that must match:
constexpr uint8_t MAX_EFFECT_PRESETS = MAX_EFFECTS * 8;
static_assert(PRESET_STORAGE_SIZE >= MAX_EFFECT_PRESETS, "...");
```

### 5. No Runtime Cost
- All validation at compile time
- Zero memory overhead
- No performance impact on 120 FPS render loop

## Testing

### Positive Test: Matching Counts

```bash
# All counts match (current state)
pio run -e esp32dev_audio_esv11
# ✅ SUCCESS
```

### Negative Test: Metadata Undersized

```cpp
// Temporarily break PatternRegistry.cpp
const PatternMetadata PATTERN_METADATA[] PROGMEM = {
    // ... only 110 entries instead of 113 ...
};
```

```bash
pio run -e esp32dev_audio_esv11
# ❌ COMPILE ERROR:
# static assertion failed: PATTERN_METADATA_COUNT must be >= EXPECTED_EFFECT_COUNT
```

### Negative Test: Missing Registration

```cpp
// registerAllEffects() only registers 112 effects
```

```bash
pio run -e esp32dev_audio_esv11
# ✅ Compiles (no static array overflow)
# ⚠️  Runtime warning on serial:
# [WARNING] Effect count mismatch: registered 112, expected 113
```

## Architecture Diagram

```
┌──────────────────────────────────────────────────────────┐
│                  config/limits.h                         │
│                                                          │
│  namespace lightwaveos::limits {                        │
│    constexpr uint8_t MAX_EFFECTS = 113; ← SINGLE SOURCE │
│  }                                                       │
└────────────────────┬───────────────────────────────────┘
                     │
                     │ #include "../config/limits.h"
                     │
        ┌────────────┴────────────┬────────────┬──────────┬──────────┐
        │                         │            │          │          │
        ▼                         ▼            ▼          ▼          ▼
┌───────────────┐    ┌────────────────┐  ┌──────────┐ ┌──────────┐ ┌──────────┐
│RendererActor.h│    │  WebServer.h   │  │Pattern   │ │Audio     │ │Builtin   │
│               │    │                │  │Registry  │ │Effect    │ │Effect    │
│ m_effects[    │    │ effectNames[   │  │.cpp      │ │Mapping.h │ │Registry.h│
│  MAX_EFFECTS] │    │  MAX_EFFECTS]  │  │          │ │          │ │          │
│               │    │                │  │EXPECTED= │ │MAX_      │ │static_   │
│               │    │                │  │MAX_      │ │EFFECTS=  │ │assert()  │
│               │    │                │  │EFFECTS   │ │MAX_      │ │          │
└───────────────┘    └────────────────┘  └──────────┘ └──────────┘ └──────────┘
        │                         │            │          │          │
        └────────────┬────────────┴────────────┴──────────┴──────────┘
                     │
                     ▼
            ✅ Compile-Time Parity
            ✅ Single Update Point
            ✅ Zero Runtime Overhead
```

## Technical Deep Dive

### Why `inline constexpr` (C++17)?

**Problem:** Header-only constants can violate One Definition Rule (ODR) in C++11

```cpp
// C++11: Each translation unit gets its own copy (may cause linker errors)
constexpr uint8_t MAX_EFFECTS = 113;

// C++17: Guaranteed single definition across all translation units
inline constexpr uint8_t MAX_EFFECTS = 113;
```

**ESP32 Arduino Core:** Supports C++17 via `-std=gnu++17` flag (enabled by default in PlatformIO)

### Why Not Macros?

```cpp
// ❌ Macro approach (no type safety, no namespace)
#define MAX_EFFECTS 113

// ✅ Constexpr approach (type-safe, scoped, debuggable)
namespace limits {
    constexpr uint8_t MAX_EFFECTS = 113;
}
```

Benefits of constexpr over macros:
- Type checking by compiler
- Namespace scoping prevents collisions
- Shows up in debugger with symbol name
- Respects C++ access control

### Why Not Enum?

```cpp
// Alternative: enum class
enum class Limits : uint8_t {
    MAX_EFFECTS = 113
};

// Usage is more verbose:
EffectEntry m_effects[static_cast<uint8_t>(Limits::MAX_EFFECTS)];

// vs constexpr (cleaner):
EffectEntry m_effects[limits::MAX_EFFECTS];
```

Constexpr wins for:
- Cleaner syntax
- Direct use in array declarations
- Better compatibility with existing code

## Future Enhancements

### 1. Template Array Helper

```cpp
namespace lightwaveos {
namespace limits {

template<typename T>
using EffectArray = T[MAX_EFFECTS];

} // namespace limits
} // namespace lightwaveos

// Usage:
class RendererActor {
    limits::EffectArray<EffectEntry> m_effects;  // Expands to EffectEntry[113]
};
```

### 2. Additional Compile-Time Checks

```cpp
// Ensure effect ID fits in uint8_t
static_assert(MAX_EFFECTS <= 255, "Effect ID must fit in uint8_t");

// Ensure WebSocket packet can hold all effect names
static_assert(MAX_EFFECTS * 32 <= 4096, "Effect name list exceeds WebSocket MTU");

// Ensure NVS can store all presets
static_assert(MAX_EFFECTS * sizeof(EffectPreset) <= NVS_PRESET_PARTITION_SIZE,
              "Preset storage exceeds NVS partition size");
```

### 3. Automatic Registration Counting

```cpp
// Use registration counter to auto-validate
class EffectRegistrar {
    static inline uint8_t s_count = 0;
public:
    static void registerEffect(const char* name, IEffect* effect) {
        ++s_count;
        // ...
    }
    static constexpr uint8_t getCount() { return s_count; }
};

// At end of registerAllEffects():
static_assert(EffectRegistrar::getCount() == limits::MAX_EFFECTS,
              "Effect registration count mismatch");
```

## Maintenance Checklist

When modifying the effect system:

- [ ] Update `limits::MAX_EFFECTS` in `/src/config/limits.h`
- [ ] Add effect implementation
- [ ] Register in `CoreEffects.cpp::registerAllEffects()`
- [ ] Add metadata to `PatternRegistry.cpp::PATTERN_METADATA[]`
- [ ] Build with `pio run -e esp32dev_audio_esv11`
- [ ] Verify no static_assert failures
- [ ] Verify no runtime warnings in serial monitor
- [ ] Test effect switching via web interface
- [ ] Update API documentation if needed

## Conclusion

This architecture leverages modern C++ (C++17) features to enforce compile-time parity across effect counts while maintaining zero runtime overhead. The system is:

- **Type-safe:** Uses constexpr over macros
- **Maintainable:** Single source of truth
- **Robust:** Static assertions catch drift
- **ESP32-compatible:** Works with PROGMEM and Arduino framework
- **Performance-neutral:** No runtime cost

When adding effects, developers only update `limits::MAX_EFFECTS` once. The compiler enforces consistency everywhere else.
