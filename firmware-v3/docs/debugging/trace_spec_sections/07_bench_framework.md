# Surface 7 — Runtime A/B Bench Command Framework (Tier 3)

## Contract

The runtime toggle invariant is:
- **All toggles default to canonical/shipping behaviour** — the firmware acts identically to non-bench code until explicitly toggled via serial CLI.
- **Toggle changes fire TRACE_INSTANT immediately** — enabling post-process trace analysis to self-label which configuration variant was active during each trace segment.
- **Zero-cost abstraction** — toggle checks are direct volatile bool reads in hot paths (no map lookups, no string parsing at runtime).
- **Static registration at boot** — subsystems register their toggles once during init; toggle name → pointer mapping is compile-time fixed-size.

## Hypothesis

This eliminates rebuild cycles for hypothesis testing. An agent (Captain) can:
1. Run a 30-second trace with `render.async_rmt=on`
2. Toggle `render.async_rmt=off` without restart
3. Run another 30-second trace with `render.async_rmt=off`
4. Compare render frame timing histograms in the same trace file

This is the highest-leverage performance investigation surface because it converts rebuild-test-iterate (5-10 min/cycle) into toggle-trace-analyse (30 sec/cycle).

---

## CLI Grammar

```
bench list                          # List all registered toggles, current values, defaults
bench begin <test_name>             # Start a benchmark run; fire TRACE_INSTANT("bench_begin", name=<test_name>)
bench split <variant_name>          # Marker for A/B variant; fire TRACE_INSTANT("bench_split", variant=<variant_name>)
bench end                           # End the benchmark run; fire TRACE_INSTANT("bench_end")
bench toggle <name> <value>         # Set toggle to value; fire TRACE_INSTANT("bench_toggle_set", name=<name>, value=<value>)
bench reset                         # Restore all toggles to default; fire TRACE_INSTANT("bench_reset")
```

### Examples

```
> bench list
  render.async_rmt              [ON]     (default: ON) — async vs sync FastLED show()
  render.color_correction       [ON]     (default: ON) — colour correction pipeline on/off
  render.dual_strip_parallel    [ON]     (default: ON) — parallel vs sequential RMT writes
  audio.lookahead               [ON]     (default: ON) — lookahead spike smoothing on/off
  audio.zone_agc                [ON]     (default: ON) — zone AGC on/off
  audio.chroma_zone_agc         [ON]     (default: ON) — chroma zone AGC on/off
  effect.subpixel               [OFF]    (default: OFF) — SubpixelRenderer vs integer fallback
  effect.fade_to_black          [ON]     (default: ON) — fadeToBlackBy on/off per-effect override

> bench begin async_rmt_off
[bench] Starting run: async_rmt_off (timestamp: 1234567890 μs)

> bench toggle render.async_rmt off
[bench] Toggle render.async_rmt → 0 (was 1)

> [run application for 30s, capture trace]

> bench split now_using_sync
[bench] Variant marker: now_using_sync (timestamp: 1234597890 μs)

> bench toggle render.async_rmt on
[bench] Toggle render.async_rmt → 1 (was 0)

> [run application for 30s more]

> bench end
[bench] Ended run. Flushed 2 bench markers to trace.

> trace
[returns JSON trace with bench_begin, bench_toggle_set × 2, bench_split, bench_end instants]
```

---

## Toggle Registry Design

### Header File API (`BenchRegistry.h`)

The registry is a **compile-time fixed-size array** of toggle descriptors. At boot, each subsystem calls `BENCH_REGISTER_TOGGLE()` which pushes a descriptor into the registry.

```cpp
#include <stdint.h>
#include <stddef.h>

namespace lightwaveos {
namespace bench {

// ── Toggle type identifier ──
enum class ToggleType : uint8_t {
    BoolToggle  = 0,  // bool (value: 0 or 1)
    IntToggle   = 1,  // int32_t (range validation TBD per toggle)
};

// ── Single toggle descriptor ──
struct ToggleDescriptor {
    const char*      name;                    // e.g. "render.async_rmt"
    void*            ptr;                     // pointer to volatile bool or int32_t backing variable
    ToggleType       type;                    // BoolToggle or IntToggle
    bool             defaultValue;            // canonical/shipping value (int cast to bool)
    const char*      description;             // e.g. "async vs sync FastLED show()"
};

// ── Registry (max 32 toggles; fixed-size array) ──
constexpr size_t BENCH_REGISTRY_MAX = 32;

class BenchRegistry {
public:
    // Initialize and register all subsystem toggles.
    // Called once during system boot (e.g., from main() or SystemInit()).
    static void init();

    // Register a single toggle. Called at static init time via macro.
    // Returns true if registered successfully, false if full.
    static bool registerToggle(const ToggleDescriptor& desc);

    // Query registry size and entries.
    static size_t count() { return s_count; }
    static const ToggleDescriptor& getToggle(size_t index) { return s_registry[index]; }

    // Lookup toggle by name. Returns nullptr if not found.
    static const ToggleDescriptor* findByName(const char* name);

    // Get/set toggle value. Returns true if successful.
    static bool getValue(const char* name, int32_t& out);
    static bool setValue(const char* name, int32_t value);

    // Reset all toggles to their default values.
    static void resetAll();

private:
    static ToggleDescriptor s_registry[BENCH_REGISTRY_MAX];
    static size_t           s_count;
};

// ── Macro for subsystem-side registration ──
// Usage in RendererActor.cpp:
//   BENCH_REGISTER_TOGGLE("render.async_rmt", &g_async_rmt_enabled, BoolToggle, true, "async vs sync FastLED show()");
//
// Expands to a static initializer that calls BenchRegistry::registerToggle() at startup.
#define BENCH_REGISTER_TOGGLE(name, ptr, type, defaultVal, desc) \
    namespace { \
        struct BenchRegister_##__LINE__ { \
            BenchRegister_##__LINE__() { \
                lightwaveos::bench::ToggleDescriptor d; \
                d.name = name; \
                d.ptr = (void*)(ptr); \
                d.type = lightwaveos::bench::type; \
                d.defaultValue = (bool)(defaultVal); \
                d.description = desc; \
                lightwaveos::bench::BenchRegistry::registerToggle(d); \
            } \
        } bench_reg_##__LINE__; \
    }

// ── Convenience query helpers for consumers ──
// Inline function to avoid forcing every consumer to link BenchRegistry.cpp.
// Reads the toggle value; if not registered, returns the provided default.
inline bool isToggleEnabled(const char* name, bool defaultValue = true) {
    int32_t val = 0;
    if (BenchRegistry::getValue(name, val)) {
        return (val != 0);
    }
    return defaultValue;
}

} // namespace bench
} // namespace lightwaveos
```

### Implementation (`BenchRegistry.cpp`)

```cpp
#include "BenchRegistry.h"
#include "config/Trace.h"
#include "utils/Log.h"

namespace lightwaveos {
namespace bench {

// Static storage
ToggleDescriptor BenchRegistry::s_registry[BENCH_REGISTRY_MAX] = {};
size_t           BenchRegistry::s_count = 0;

void BenchRegistry::init() {
    // No-op; actual registration happens via BENCH_REGISTER_TOGGLE macro
    // at static init time (before main() runs).
}

bool BenchRegistry::registerToggle(const ToggleDescriptor& desc) {
    if (s_count >= BENCH_REGISTRY_MAX) {
        return false;  // Registry full
    }
    s_registry[s_count] = desc;
    s_count++;
    return true;
}

const ToggleDescriptor* BenchRegistry::findByName(const char* name) {
    if (!name) return nullptr;
    for (size_t i = 0; i < s_count; i++) {
        if (strcmp(s_registry[i].name, name) == 0) {
            return &s_registry[i];
        }
    }
    return nullptr;
}

bool BenchRegistry::getValue(const char* name, int32_t& out) {
    const ToggleDescriptor* desc = findByName(name);
    if (!desc) return false;

    if (desc->type == ToggleType::BoolToggle) {
        volatile bool* ptr = reinterpret_cast<volatile bool*>(desc->ptr);
        out = *ptr ? 1 : 0;
    } else if (desc->type == ToggleType::IntToggle) {
        volatile int32_t* ptr = reinterpret_cast<volatile int32_t*>(desc->ptr);
        out = *ptr;
    }
    return true;
}

bool BenchRegistry::setValue(const char* name, int32_t value) {
    const ToggleDescriptor* desc = findByName(name);
    if (!desc) return false;

    if (desc->type == ToggleType::BoolToggle) {
        volatile bool* ptr = reinterpret_cast<volatile bool*>(desc->ptr);
        *ptr = (value != 0);
    } else if (desc->type == ToggleType::IntToggle) {
        volatile int32_t* ptr = reinterpret_cast<volatile int32_t*>(desc->ptr);
        *ptr = value;
    }
    return true;
}

void BenchRegistry::resetAll() {
    for (size_t i = 0; i < s_count; i++) {
        setValue(s_registry[i].name, (int32_t)s_registry[i].defaultValue);
    }
}

} // namespace bench
} // namespace lightwaveos
```

---

## Initial Toggle Catalogue

| Name | Type | Default | Description | Consumer file:line | Status |
|---|---|---|---|---|---|
| `render.async_rmt` | bool | true | Async vs sync FastLED show() | RendererActor.cpp:TBD | **To wire** |
| `render.color_correction` | bool | true | Colour correction pipeline on/off | RendererActor.cpp:TBD | To wire |
| `render.dual_strip_parallel` | bool | true | Parallel vs sequential RMT writes | RendererActor.cpp:TBD | To wire |
| `audio.lookahead` | bool | true | Lookahead spike smoothing on/off | AudioBeatTracker.cpp:TBD | To wire |
| `audio.zone_agc` | bool | true | Zone AGC on/off | AudioActor.cpp:TBD | To wire |
| `audio.chroma_zone_agc` | bool | true | Chroma zone AGC on/off | AudioActor.cpp:TBD | To wire |
| `effect.subpixel` | bool | false | SubpixelRenderer vs integer fallback | SubpixelRenderer.cpp:TBD | Not yet implemented |
| `effect.fade_to_black` | bool | true | fadeToBlackBy on/off per-effect override | EffectBase.cpp:TBD | To wire |

---

## Implementation Specification

### File 1: `src/utils/BenchRegistry.h`

**Location**: `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3/src/utils/BenchRegistry.h`

**Content**: See "Header File API" section above.

**Integration points**:
- Included by SerialCLI.cpp (for `bench` command handler)
- Included by each subsystem that registers toggles (RendererActor.cpp, AudioActor.cpp, etc.)
- Included by hot-path consumers (e.g., RendererActor::renderFrame()) for inline toggle checks

---

### File 2: `src/utils/BenchRegistry.cpp`

**Location**: `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3/src/utils/BenchRegistry.cpp`

**Content**: See "Implementation (`BenchRegistry.cpp`)" section above.

**Build integration**: Add to CMakeLists.txt or PlatformIO build (same as other utils/).

---

### File 3: `src/serial/SerialCLI.cpp` (modification)

**Location of modification**: Around line 1174, after the `trace` command handler. The existing pattern is:

```cpp
#if FEATURE_MABUTRACE
    else if (inputLower == "trace") {
        handledMulti = true;
        Serial.println(F("[TRACE] Flushing trace buffer..."));
        TRACE_FLUSH();
        get_json_trace_chunked(nullptr, [](void* /*ctx*/, const char* chunk, size_t len) {
            Serial.write(chunk, len);
        });
        Serial.println();
        Serial.println(F("[TRACE] Done."));
    }
#endif
```

**Insertion point**: Immediately after the `#endif` closing the FEATURE_MABUTRACE block (after line 1184).

**New command handler**:

```cpp
    // -----------------------------------------------------------------
    // Bench Commands: runtime A/B toggle framework (Tier 3)
    // -----------------------------------------------------------------
#if FEATURE_MABUTRACE
    else if (inputLower.startsWith("bench")) {
        handledMulti = true;

        // Parse subcommand
        String args = input.substring(5);  // After "bench"
        args.trim();
        String argsLower = args;
        argsLower.toLowerCase();

        if (args.length() == 0 || argsLower == "list") {
            // bench list
            Serial.println(F("\n=== Registered Toggles ==="));
            for (size_t i = 0; i < lightwaveos::bench::BenchRegistry::count(); i++) {
                const auto& desc = lightwaveos::bench::BenchRegistry::getToggle(i);
                int32_t val = 0;
                lightwaveos::bench::BenchRegistry::getValue(desc.name, val);
                bool enabled = (val != 0);
                Serial.printf("  %-30s [%s]  (default: %s)  %s\n",
                    desc.name,
                    enabled ? "ON " : "OFF",
                    desc.defaultValue ? "ON " : "OFF",
                    desc.description);
            }
            Serial.println();

        } else if (argsLower.startsWith("begin ")) {
            // bench begin <test_name>
            String testName = args.substring(6);
            testName.trim();
            if (testName.length() > 0) {
                uint64_t ts = esp_timer_get_time();
                TRACE_INSTANT("bench_begin");
                Serial.printf("[bench] Starting run: %s (timestamp: %llu μs)\n",
                    testName.c_str(), ts);
            } else {
                Serial.println("ERROR: bench begin <test_name>");
            }

        } else if (argsLower.startsWith("split ")) {
            // bench split <variant_name>
            String variantName = args.substring(6);
            variantName.trim();
            if (variantName.length() > 0) {
                uint64_t ts = esp_timer_get_time();
                TRACE_INSTANT("bench_split");
                Serial.printf("[bench] Variant marker: %s (timestamp: %llu μs)\n",
                    variantName.c_str(), ts);
            } else {
                Serial.println("ERROR: bench split <variant_name>");
            }

        } else if (argsLower == "end") {
            // bench end
            TRACE_INSTANT("bench_end");
            Serial.println(F("[bench] Ended run. Flushed markers to trace."));

        } else if (argsLower.startsWith("toggle ")) {
            // bench toggle <name> <value>
            String toggleArgs = args.substring(7);
            toggleArgs.trim();
            int spaceIdx = toggleArgs.indexOf(' ');
            if (spaceIdx > 0) {
                String toggleName = toggleArgs.substring(0, spaceIdx);
                String toggleValStr = toggleArgs.substring(spaceIdx + 1);
                toggleValStr.trim();

                // Parse value: on/off or numeric
                int32_t newVal = 0;
                if (toggleValStr == "on" || toggleValStr == "1") {
                    newVal = 1;
                } else if (toggleValStr == "off" || toggleValStr == "0") {
                    newVal = 0;
                } else {
                    newVal = toggleValStr.toInt();
                }

                int32_t oldVal = 0;
                lightwaveos::bench::BenchRegistry::getValue(toggleName.c_str(), oldVal);

                if (lightwaveos::bench::BenchRegistry::setValue(toggleName.c_str(), newVal)) {
                    TRACE_INSTANT("bench_toggle_set");
                    Serial.printf("[bench] Toggle %s → %ld (was %ld)\n",
                        toggleName.c_str(), (long)newVal, (long)oldVal);
                } else {
                    Serial.printf("ERROR: Toggle not found: %s\n", toggleName.c_str());
                }
            } else {
                Serial.println("ERROR: bench toggle <name> <on|off|value>");
            }

        } else if (argsLower == "reset") {
            // bench reset
            lightwaveos::bench::BenchRegistry::resetAll();
            TRACE_INSTANT("bench_reset");
            Serial.println(F("[bench] All toggles reset to defaults."));

        } else {
            Serial.println("ERROR: bench {list|begin|split|end|toggle|reset}");
        }
    }
#endif
```

**Key integration notes**:
- Guarded by `#if FEATURE_MABUTRACE` (bench is only meaningful when tracing is enabled)
- Uses the else-if pattern consistent with existing handlers
- Uses `esp_timer_get_time()` for timestamp capture (already used throughout firmware)
- Calls `TRACE_INSTANT()` which expands to mabutrace calls or no-ops based on Trace.h
- Minimal serial output, no heap allocations in toggle path

---

## Subsystem Integration — Toggle Consumer Wiring

For each toggle in the catalogue, the implementing agent must wire the consumer code as follows:

### Toggle: `render.async_rmt`

**Consumer file**: `src/core/actors/RendererActor.cpp` (or wherever `FastLED.show()` is called)

**Before**:
```cpp
void RendererActor::renderFrame() {
    // ... build frame ...
    FastLED.show();  // Always sync
}
```

**After**:
```cpp
void RendererActor::renderFrame() {
    // ... build frame ...
    if (lightwaveos::bench::isToggleEnabled("render.async_rmt")) {
        FastLED.show_async();
    } else {
        FastLED.show();
    }
}
```

**Consumer line**: RendererActor.cpp:TBD (locate by searching `FastLED.show()`)

**Priority**: **FIRST** (highest leverage; directly impacts frame timing)

---

### Toggle: `render.color_correction`

**Consumer file**: `src/core/actors/RendererActor.cpp` (colour correction pipeline entry point)

**Pattern**:
```cpp
if (lightwaveos::bench::isToggleEnabled("render.color_correction")) {
    applyColorCorrectionPipeline(leds, ledCount);
}
```

**Consumer line**: RendererActor.cpp:TBD

**Priority**: Second

---

### Toggle: `render.dual_strip_parallel`

**Consumer file**: `src/core/actors/RendererActor.cpp` (RMT/dual-strip render dispatch)

**Pattern**:
```cpp
if (lightwaveos::bench::isToggleEnabled("render.dual_strip_parallel")) {
    renderDualStripParallel(zone0Leds, zone1Leds);
} else {
    renderDualStripSequential(zone0Leds, zone1Leds);
}
```

**Consumer line**: RendererActor.cpp:TBD

**Priority**: Second

---

### Toggle: `audio.lookahead`

**Consumer file**: `src/audio/AudioBeatTracker.cpp` or equivalent (spike smoothing in beat detection)

**Pattern**:
```cpp
if (lightwaveos::bench::isToggleEnabled("audio.lookahead")) {
    smoothedBeat = applyLookaheadSmoothing(rawBeat);
} else {
    smoothedBeat = rawBeat;
}
```

**Consumer line**: AudioBeatTracker.cpp:TBD

**Priority**: Third

---

### Toggle: `audio.zone_agc`

**Consumer file**: `src/audio/AudioActor.cpp` (audio level post-processing)

**Pattern**:
```cpp
if (lightwaveos::bench::isToggleEnabled("audio.zone_agc")) {
    applyZoneAGC(audioFrame);
}
```

**Consumer line**: AudioActor.cpp:TBD

**Priority**: Third

---

### Toggle: `audio.chroma_zone_agc`

**Consumer file**: `src/audio/AudioActor.cpp` (chroma-specific AGC variant)

**Pattern**:
```cpp
if (lightwaveos::bench::isToggleEnabled("audio.chroma_zone_agc")) {
    applyChromaZoneAGC(chromaFrame);
}
```

**Consumer line**: AudioActor.cpp:TBD

**Priority**: Third

---

### Toggle: `effect.subpixel`

**Consumer file**: `src/effects/SubpixelRenderer.cpp` (or within EffectBase if SubpixelRenderer is integrated)

**Pattern**:
```cpp
// In the effect render loop, choose renderer:
if (lightwaveos::bench::isToggleEnabled("effect.subpixel")) {
    renderSubpixel(leds, ledCount);
} else {
    renderInteger(leds, ledCount);
}
```

**Consumer line**: SubpixelRenderer.cpp:TBD or EffectBase.cpp:TBD

**Status**: Not yet implemented; SubpixelRenderer must be integrated into effect pipeline first.

**Priority**: TBD (depends on SubpixelRenderer MVP)

---

### Toggle: `effect.fade_to_black`

**Consumer file**: `src/effects/EffectBase.cpp` (post-render fadeToBlackBy logic)

**Pattern**:
```cpp
if (lightwaveos::bench::isToggleEnabled("effect.fade_to_black")) {
    fadeToBlackBy(leds, ledCount, fadeAmount);
}
```

**Consumer line**: EffectBase.cpp:TBD

**Priority**: Fourth (post-processing; lower impact on core rendering)

---

## Output Contract — Trace Analyser Integration (Surface 8)

The `analyse_trace.py` script (Surface 8) must parse the following TRACE_INSTANT events to segment the trace:

### Bench Instant Events

All bench events are fired as `TRACE_INSTANT()` with the event name and optional arguments. The JSON trace output will contain objects like:

```json
{
  "name": "bench_begin",
  "ts": 1234567890,
  "args": {
    "name": "async_rmt_off"
  }
}
```

**Event types**:
- `bench_begin` — start of A/B run; args: `name` (string)
- `bench_split` — variant marker; args: `variant` (string)
- `bench_toggle_set` — toggle changed; args: `name` (string), `value` (int)
- `bench_reset` — all toggles reset; args: none
- `bench_end` — end of A/B run; args: none

### Segmentation Algorithm (pseudocode for Surface 8)

```python
def segment_trace_by_bench(events):
    """
    Parse bench_* instants to group render_frame events into variants.
    Returns list of (variant_name, start_ts, end_ts, render_frames).
    """
    segments = []
    current_segment = None
    current_variant = None

    for event in events:
        if event['name'] == 'bench_begin':
            if current_segment:
                segments.append(current_segment)
            current_segment = {
                'run_name': event['args']['name'],
                'start_ts': event['ts'],
                'end_ts': None,
                'variants': {}
            }
            current_variant = '__default__'
            current_segment['variants'][current_variant] = []

        elif event['name'] == 'bench_split':
            # Create new variant bucket
            current_variant = event['args']['variant']
            current_segment['variants'][current_variant] = []

        elif event['name'] == 'bench_toggle_set':
            # Log toggle in current variant
            pass  # Metadata for trace.json

        elif event['name'] == 'bench_end':
            current_segment['end_ts'] = event['ts']
            segments.append(current_segment)
            current_segment = None
            current_variant = None

        elif event['name'] == 'render_frame_us' and current_segment:
            # Attribute this frame to current variant
            current_segment['variants'][current_variant].append(event)

    return segments
```

The analyser should then compute per-variant histograms (render_frame_us distribution, p50, p95, p99) for hypothesis testing.

---

## Acceptance Criteria

1. **Baseline**: After all subsystems register toggles via `BENCH_REGISTER_TOGGLE()` macro:
   - `bench list` over serial returns all N toggles (initially 8) with current values and defaults.
   - No crashes or memory issues (registry is fixed-size, no heap).

2. **Toggle state persistence**: Run `bench toggle render.async_rmt off` and verify:
   - Serial output confirms toggle changed.
   - Subsequent `bench list` shows `render.async_rmt [OFF]`.
   - Toggle remains off until reset or changed again (no timeout).

3. **Trace markers in JSON**: After `bench begin test1 / [30s run] / bench split variant2 / [30s run] / bench end / trace`:
   - JSON output contains `bench_begin`, `bench_split`, `bench_end` instants with correct timestamps.
   - All `render_frame_us` events between `bench_begin` and `bench_split` are attributable to variant 1.
   - All `render_frame_us` events between `bench_split` and `bench_end` are attributable to variant 2.

4. **Analyser segmentation**: The Surface 8 analyser script can parse the JSON and generate:
   ```
   Variant 1 (async_rmt_off):
     render_frame_us: p50=1200µs, p95=1500µs, p99=1800µs
   Variant 2 (async_rmt_on):
     render_frame_us: p50=1100µs, p95=1300µs, p99=1600µs
   Delta: 8% improvement in p50 latency with async_rmt=on
   ```

5. **Hot-path zero-cost**: With a high-volume effect (e.g., many FastLED operations per frame):
   - Compile a build with `FEATURE_MABUTRACE=0` (tracing disabled).
   - Verify `isToggleEnabled("render.async_rmt")` compiles to zero instructions (inlined volatile bool read).

---

## Open Questions

1. **Volatile bool vs std::atomic<bool>** — For Core 0 (audio) and Core 1 (render) concurrency, should toggle backing variables be `volatile bool` or `std::atomic<bool>`?
   - Volatile is simpler and zero-cost on ESP32 (sequential consistency is implicit).
   - Atomic is more explicit but adds fence instructions.
   - **Recommendation**: Start with `volatile bool`; if we see write-ordering bugs, switch to atomic.

2. **Int toggle range validation** — Some toggles might need bounds (e.g., `audio.agc_level` 0-100). Should BenchRegistry enforce per-toggle ranges, or leave validation to consumers?
   - **Recommendation**: Registry accepts any int; consumers validate on read. Keeps registry simple.

3. **Toggling during render frame** — If Agent toggles a value mid-frame, does it apply to the current frame or the next? Should we snapshot toggles at frame start?
   - **Recommendation**: Snapshot at frame start (cheaply: read toggle once per frame into local bool). Prevents mid-frame inconsistencies.

4. **Stateful toggles** (e.g., LUT tables) — Some subsystems may cache derived state from toggles (e.g., colour correction LUT). Should toggle changes invalidate caches?
   - **Recommendation**: Toggling invalidates cache automatically (TRACE_INSTANT signals to cache manager). Cache manager TBD in Surface 8.

5. **Performance profiling feedback** — Should the bench command accept a callback to notify the application when a toggle changes? (E.g., to rebuild LUTs.)
   - **Recommendation**: Out of scope for Surface 7; deferred to Surface 8 (analyser) or consumer subsystems.

---

## Implementation Roadmap

| Phase | Owner | Task | Depends on |
|-------|-------|------|-----------|
| 1 | Arch | Write BenchRegistry.h/.cpp, integrate into build | — |
| 1 | Arch | Integrate `bench` command handler into SerialCLI.cpp | — |
| 1 | Test | Verify `bench list` with no registered toggles | Phase 1 |
| 2 | RendererActor owner | Register `render.*` toggles; wire `render.async_rmt` consumer | Phase 1 |
| 2 | AudioActor owner | Register `audio.*` toggles; wire consumers | Phase 1 |
| 3 | Effects owner | Register `effect.*` toggles; wire consumers | Phase 1 |
| 3 | Captain | End-to-end A/B test: toggle → trace → analyse | Phase 2 + 3 |

---

## Summary

This Tier 3 surface provides the **runtime experiment framework** for hypothesis testing without rebuilds. The toggle registry is minimal (32-entry fixed array), the hot-path cost is negligible (single volatile bool read), and the trace integration is transparent (TRACE_INSTANT events self-label the configuration). This is the highest-leverage performance investigation tool because it converts slow iterative cycles into fast interactive exploration.
