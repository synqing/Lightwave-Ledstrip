---
abstract: "Coding conventions across firmware-v3 (C++), lightwave-ios-v2 (Swift), and tab5-encoder (C++) including naming patterns, file structure, error handling, and mandatory constraints like centre-origin rendering, no heap in render(), British English, and actor model thread safety."
---

# Coding Conventions

**Analysis Date:** 2026-03-21

## Naming Patterns

### C++ Files (firmware-v3, tab5-encoder)

**Files:**
- Header files: `PascalCase.h`
- Source files: `PascalCase.cpp`
- Test files: `test_snake_case.cpp` in `firmware-v3/test/`
- Examples: `LGPWaveCollisionEffect.h`, `EsBeatClock.cpp`, `WsMotionCodec.cpp`

**Classes and Structs:**
- `PascalCase` — both public and private classes
- Virtual base classes and interfaces: `IPrefixName` (e.g., `IEffect`, `IAdapter`)
- Implementation classes: `ConcreteNameImpl` or `ConcreteNameSpecific` (e.g., `LGPWaveCollisionEffect`, `EsAnalogRefEffect`)
- Enums: `PascalCase` with `PascalCase` values

**Functions and Methods:**
- `camelCase` — instance methods and free functions
- `kConstantName` — constant naming (e.g., `kMaxZones`, `kId`, `CHROMA_HISTORY`)
- Private member variables: `m_variableName` (e.g., `m_phase`, `m_chromaAngle`, `m_ps`)
- Static member variables: `s_variableName` (rare)

**Variables:**
- `camelCase` — local and member variables
- `snake_case` — macro parameters and template args (rare)
- Boolean fields: `m_isSomething`, `m_hasFeature`, `m_canDoX`

### Swift Files (lightwave-ios-v2)

**Files:**
- `PascalCase.swift` — one type per file (standard Swift convention)
- Examples: `AppViewModel.swift`, `RESTClient.swift`, `DeviceDiscoveryService.swift`

**Classes and Structs:**
- `PascalCase` for types, protocols, enums
- Instances: lowercase first letter for let/var bindings

**Functions and Methods:**
- `camelCase` — methods and functions
- `get_` prefix NOT used (Swift convention)
- `set_` prefix NOT used (use property observers instead)

**Variables:**
- `camelCase` — all local and property bindings
- Private properties: `private var name: Type`
- Computed properties: `var derivedValue: Type { ... }`

## Code Style

### C++ Style

**Formatting:**
- PlatformIO standard: `gnu++17` dialect, `-O3 -ffast-math`
- K&R bracing (opening brace on same line)
- 4-space indentation
- Line length: no hard limit, but keep readable (<120 chars preferred for effects)
- Spaces around operators: `x + y`, `m_phase += dt`, `if (condition)`

**Linting:**
- Compiler flags: `-Werror=deprecated-declarations` (warnings as errors)
- No automatic formatting tool enforced; relies on code review consistency
- Comments must use C++ style: `//` for single-line, `/** @brief ... */` for documentation

**Includes:**
- Standard library first: `#include <string>`, `#include <cmath>`
- Third-party libraries next: `#include <FastLED.h>`, `#include <ArduinoJson.h>`
- Local project headers last: `#include "EffectBase.h"`, `#include "../config/features.h"`
- Project headers use relative paths from source file location
- Always use `#pragma once` in headers (no include guards)

### Swift Style

**Formatting:**
- 4-space indentation (configured in .xcodeproj)
- SwiftUI preview syntax standard: `#Preview { ... }`
- No semicolons at end of statements
- Type annotations on property declarations (required)

**Linting:**
- Xcode built-in diagnostics only
- No SwiftLint enforced; relies on convention
- Comments: `//` for single-line, `///` for doc comments (rare in this codebase)

**Imports:**
- Foundation and system frameworks first
- Third-party frameworks second (Rive runtime only)
- No internal grouping required; one per line

## Import Organization

### C++ Order

1. C standard headers: `<cstring>`, `<cmath>`
2. Arduino/ESP-IDF: `<Arduino.h>`, `<freertos/FreeRTOS.h>`
3. Third-party libraries: `<FastLED.h>`, `<ArduinoJson.h>`, `<lvgl.h>`, `<ESPAsyncWebServer.h>`
4. Local project headers: `"EffectBase.h"`, `"../config/features.h"`, `"../../audio/ControlBus.h"`

**Path Aliases:**
- Relative paths from source file location (PlatformIO default)
- No CMake include directories or alias setup in firmware-v3 or tab5-encoder
- `#include "config/features.h"` assumes config/ is alongside src/

### Swift Import Order

1. Foundation and system: `import Foundation`, `import Observation`
2. Frameworks: `import SwiftUI`, `import Combine`
3. Test frameworks (tests only): `import XCTest`, `@testable import LightwaveOS`
- No explicit grouping; one import per line

## Error Handling

### C++ Patterns

**Runtime Checks:**
- Null pointer checks before dereferencing: `if (!ptr) return;` or `if (!m_ps) return;`
- Bounds checking for zone IDs: `const int z = (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0;`
- PSRAM allocation failures: `if (!m_ps) { LW_LOGE("...alloc failed..."); return false; }`
- Compilation guards: `#ifdef FEATURE_AUDIO_SYNC` for conditional features

**Return Values:**
- `bool` for init/setup functions: return `true` on success, `false` on failure
- `void` for render functions: no return value; assume success
- Custom result structs for codec operations: `MotionSimpleDecodeResult { bool success; Request request; }`

**Logging:**
- Macros: `LW_LOGI()`, `LW_LOGW()`, `LW_LOGE()` (defined in logging system)
- Format: `LW_LOGE("ModuleName: message %u bytes", value)`
- Used for: initialization traces, allocation failures, error conditions
- Serial fallback: `Serial.printf()` for debug output in development mode

**Assertions:**
- Compile-time: `static_assert(kConstant == ExpectedValue)` for invariants
- Runtime: Rare; only in validation macros for effect debugging
- Crash mitigation: configASSERT guards in critical sections (e.g., RMT ISR)

### Swift Patterns

**Runtime Checks:**
- Optional unwrapping: `if let value = optional { ... }` or `guard let value = optional else { return }`
- Force unwrapping (`!`) only used in @MainActor closures capturing weak self after existence check
- Network error handling: switch on `APIClientError` enum

**Return Values:**
- Async/await: functions marked `async throws` for network operations
- `Result<T, Error>` used in completion handlers (rare; prefer async/await)
- Void for UI state updates

**Error Handling:**
- Custom enum: `APIClientError` with cases for `.connectionFailed(Error)`, `.httpError(Int, String?)`, `.decodingError(Error)`
- try/catch in network client for URLSession errors
- do { try } catch { ... } for decoding failures
- Never rethrow network errors to UI; always map to localised strings via `.errorDescription`

**Logging:**
- `print()` statements in ViewModels for debug (no persistent logging)
- Console output for connection state changes
- No third-party logging framework used

## Comments

### C++ Comments

**When to Comment:**
- Complex algorithmic sections: "CENTRE ORIGIN: Wave packets expand outward from centre"
- State machine transitions: "// Hop-based updates: update targets only on new hops"
- Non-obvious constants: "// 200ms rise, 500ms fall" (for smoothing parameters)
- Critical fixes and design decisions: "// CRITICAL FIX: Single phase for travelling waves"

**JSDoc/Doxygen:**
- File headers: `@file`, `@brief`, implicit for firmware-v3
- Function headers: `@brief` for public APIs, parameters documented for public interfaces
- Examples: LGPWaveCollisionEffect.cpp has full JSDoc header
- Effect metadata: tagged with `Effect ID`, `Family`, `Tags` in file header

**Code Comments:**
- Inline: explain "why", not "what" (code already shows what)
- CRITICAL, TODO, FIXME markers used in code
- Multi-line comments for complex sections; single-line for quick notes

### Swift Comments

**When to Comment:**
- Architecture decisions: "Root ViewModel managing connection lifecycle"
- Non-obvious logic: rare, as Swift is more self-documenting
- Workarounds: "iOS 17+ requires @Observable for Observation framework"

**Documentation:**
- File headers: brief one-line description + iOS version requirement
- Methods: rare; only for public/complex functions
- Property observers: explain purpose if non-obvious

## Function Design

### C++ Functions

**Size:**
- Render functions: aim for 50-100 lines
- Large effects split into private helpers (e.g., `computeChromaEnergy()`, `drawWave()`)
- No hard limit; readability is the guide

**Parameters:**
- `const` references for read-only objects: `const JsonObject& obj`
- Mutable references for state: `plugins::EffectContext& ctx`
- Pointers for nullable objects: `float* buffer`
- PSRAM data passed as pointer: `PsramData* m_ps`

**Return Values:**
- `bool` for success/failure (init, allocations)
- Structs for multiple return values: `struct DecodeResult { bool success; Data data; }`
- Void for side-effect functions (render, drawing)

### Swift Functions

**Size:**
- ViewModels: methods 10-30 lines
- Views: minimal logic; extract to ViewModel
- Network client: one method per endpoint type

**Parameters:**
- Labeled arguments always: `func setSpeed(_ newSpeed: Int)` or `func setZone(id: Int, speed: Int)`
- Default values for optional behavior: `func fetchDevice(timeout: TimeInterval = 5.0)`
- Trailing closures for callbacks: `fetch { device in ... }`

**Return Values:**
- `async` functions return `Void` or `T` (not Result); throw errors
- Computed properties for derived state: `var isConnected: Bool { connectionState == .connected }`

## Module Design

### C++ Module Structure

**Exports:**
- Header files define public interface; `.cpp` files implement
- Namespaces: `lightwaveos::effects`, `lightwaveos::codec`, `lightwaveos::audio`
- Hidden implementation: private members and static functions in `.cpp`
- No barrel files; each module is one interface

**Class Hierarchy:**
- Single inheritance from interface: `class ConcreteEffect : public plugins::IEffect`
- No multiple inheritance except for mixins (rare)
- Virtual destructors in base classes: `virtual ~EffectBase() = default;`

**Configuration:**
- Compile-time flags: `#define FEATURE_AUDIO_SYNC`, `#ifdef NATIVE_BUILD`
- Runtime configuration: passed in init() or constructor
- Const fields: effect IDs, metadata, constants

### Swift Module Structure

**Exports:**
- Public types in their own files
- No barrel files; import specific types
- Access control: `public`, `internal` (default), `private`, `fileprivate`
- Most ViewModels are `public` or `internal`; helpers are `private`

**Protocol Conformance:**
- `Codable` for API response types
- `Sendable` for types used across actor boundaries
- `Observable` for ViewModels (requires Swift 6 / iOS 17+)
- `Equatable` for state enums

**Configuration:**
- Stored properties in ViewModel: `@Observable` types
- Environment variables: `.environment(\.someValue, instance)`
- UserDefaults for persistence: wrapped in helper if needed

## Mandatory Project Constraints

These are HARD RULES, not style preferences. Violations produce bugs or performance failures.

### Constraint: Centre Origin (79/80 — Firmware Only)

**Rule:** All LED effects and transitions must treat LEDs 79 and 80 as the centre point. All animations originate from centre and expand outward OR converge inward.

**Implementation:**
```cpp
// CENTRE ORIGIN: Expand outward from centre
float dist = std::abs(i - 79.5f);  // Distance from centre pair
// Shorter distance = brighter/first to light up
```

**Examples in codebase:**
- `LGPWaveCollisionEffect.cpp:46` — "CENTRE ORIGIN WAVE COLLISION"
- `TransitionEngine.cpp:372` — "Calculate distance from center (LED 79)"
- Zone IDs map to LED ranges around centre pair

**When violated:** Effects appear to start from arbitrary edges instead of flowing from centre, breaking musical synchrony.

### Constraint: No Heap in render()

**Rule:** The render() function and all functions transitively called from render() must NOT allocate from heap (no `new`, `malloc`, `String` concatenation that allocates).

**Implementation:**
- Static buffers: `static float buffer[160];`
- PSRAM allocations: `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` in init() only
- Use stack for temporary values (< 1KB safe)
- Pre-allocate all working memory in init()

**Example Pattern:**
```cpp
struct PsramData { float buffer[160]; };
bool init(plugins::EffectContext& ctx) {
    m_ps = static_cast<PsramData*>(heap_caps_malloc(sizeof(PsramData), MALLOC_CAP_SPIRAM));
    if (!m_ps) return false;
    return true;
}

void render(plugins::EffectContext& ctx) {
    if (!m_ps) return;  // Guard against init failure
    // Use m_ps->buffer[i] — no allocation
}
```

**Why:** render() runs at 120 FPS on Core 1. Heap allocation contends with WiFi and network tasks on Core 0, causing stutter and data corruption (discovered Feb 2026 in d14e2696 commit).

### Constraint: 2.0ms Render Ceiling

**Rule:** Effect render() code must complete in < 2.0 milliseconds at 120 FPS.

**Measurement:**
- Use `esp_timer_get_time()` to measure frame time
- Include all operations: fades, drawing, audio sampling
- Exclude: colour correction (pipeline step 5), FastLED.show() (step 7)

**Timing Budget (8.33ms frame at 120 FPS):**
- Effect render: < 2.0ms
- Colour correction: ~2.0ms
- FastLED.show() (parallel dual-strip): 6.3ms
- Throttle/watchdog: < 0.1ms

**When violated:** Frames drop below 120 FPS, causing stuttering and lost beats.

### Constraint: No Rainbows

**Rule:** No full hue-wheel sweeps or rainbow cycling effects.

**What's forbidden:**
```cpp
// WRONG — rainbow cycling
float hue = fmod(m_phase, 360.0f);  // Cycles through all hues
CRGB color = CHSV(hue, 255, 128);
```

**What's allowed:**
- Narrow hue ranges (octave of colour wheel)
- Saturation/brightness animations
- Discrete colour palettes

### Constraint: British English in Comments/Logs/UI

**Rule:** All comments, log messages, UI strings, and documentation use British English spelling.

**Examples:**
- ✓ "centre" not "center"
- ✓ "colour" not "color"
- ✓ "initialise" not "initialize"
- ✓ "behaviour" not "behavior"
- ✓ "optimise" not "optimize"

**Enforcement:** In code review; grep for American variants in commit checks (future).

**Exceptions:** Library names, brand names, external API responses (leave as-is).

### Constraint: dt-Decay for Frame-Rate Independence

**Rule:** All time-varying state must use frame-rate independent decay, not fixed per-frame deltas.

**Pattern:**
```cpp
// CORRECT — frame-rate independent
float dt = ctx.getSafeDeltaSeconds();  // In seconds
m_value = effects::chroma::dtDecay(m_value, decayRate, dt);
// Or manual: m_value *= std::pow(decayRate, dt);

// WRONG — tied to 120 FPS
m_value *= 0.95f;  // Breaks if frame rate changes
```

**Decay constants:** typically 0.50-0.98 per-second decay rate.

### Constraint: Zone Bounds Checking

**Rule:** All zone-indexed accesses must bounds-check or clamp to valid zone ID (0 to kMaxZones-1).

**Pattern:**
```cpp
// CORRECT
const int z = (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0;
float speed = m_zoneSpeeds[z];

// WRONG — no bounds check
float speed = m_zoneSpeeds[ctx.zoneId];  // Can crash if zoneId = 0xFF (global render)
```

**Global render:** ctx.zoneId = 0xFF means render all LEDs globally; map to zone 0 or skip zone-specific code.

### Constraint: Actor Model Thread Safety (iOS Swift Only)

**Rule:** All ViewModels must be `@MainActor @Observable`. All network services must be `actor` types. Closures capturing self must use `[weak self]`.

**Pattern:**
```swift
// CORRECT
@MainActor
@Observable
class AppViewModel {
    var connectionState: ConnectionState = .disconnected

    func connectToDevice() {
        Task { @MainActor [weak self] in
            await self?.doConnection()
        }
    }
}

actor WebSocketService {
    nonisolated let url: URL
    func send(message: Data) async throws { ... }
}

// WRONG
class AppViewModel { ... }  // Missing @MainActor
Task { [self] in ... }  // Strong reference, potential leak
```

**Why:** Swift 6 concurrency model requires explicit thread safety; Sendable + @MainActor enforce compile-time correctness.

---

*Convention analysis: 2026-03-21*

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-21 | agent:gsd-mapper | Created with C++, Swift, and Tab5 conventions; hard constraints section includes centre origin, no heap in render, 2.0ms ceiling, no rainbows, British English, dt-decay, zone bounds, actor model |
