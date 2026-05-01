# TRACE_INSTRUMENTATION_SPEC.md — Section 6: Effect Lifecycle Surface

**Status:** Ready for implementation
**Author:** Code investigation
**Date:** 2026-04-27
**Firmware Version:** v3
**Confidence:** HIGH

---

## Executive Summary

The effect lifecycle is responsible for switching between 160+ selectable effects. Today we have **zero visibility** into effect transitions, initialization performance, memory allocation, and potential render-frame corruption during switchover. The current architecture uses a **clean old-then-new pattern** with sufficient guard barriers to prevent mid-frame render corruption, but init/cleanup duration leaks are uninstrumented.

Key finding: **No broken-render window detected**. The system calls cleanup on the old effect, updates m_currentEffect to the new ID, then initializes the new effect — all off the render critical path (Core 0, not Core 1). render() only executes after m_effectInitialized is set to true.

---

## Architecture: Effect-Switch State Machine

### Entry Point
- **File:** `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3/src/core/actors/RendererActor.cpp:handleSetEffect`
- **Trigger:** SetEffectCommand via message queue (Core 0 context)
- **Precondition:** Internal DRAM heap >= 12 KB (EFFECT_INIT_MIN_HEAP threshold)

### State Machine Sequence (6 steps, 3 are instrumented)

```
1. SetEffect command received → findById(newEffectId)
   [Tier 1: effect_switch instant, args=(old_eid, new_eid)]
   File: RendererActor.cpp:handleSetEffect, line ~570

2. Heap floor check: if freeInternal < 12288, REJECT
   [Tier 1: effect_switch_rejected instant, args=(eid, free_heap_bytes)]
   File: RendererActor.cpp, line ~587

3. Cleanup old effect (if valid & active)
   oldReg->effect->cleanup()
   [Tier 1: effect_cleanup_start_<eid> and effect_cleanup_end_<eid>]
   [Tier 2 (opt-in): effect_cleanup_<eid> span]
   File: RendererActor.cpp, line ~597

4. Update m_currentEffect = effectId (atomic state transition)
   File: RendererActor.cpp, line ~601

5. Pre-init yield: vTaskDelay(1) [lines ~619-620]
   File: RendererActor.cpp, line ~619

6. Initialize new effect
   initCtx populated with LED buffer, parameters, palette, timing
   const bool initOk = newReg->effect->init(initCtx)
   [Tier 1: effect_init_start_<eid> and effect_init_end_<eid>]
   [Tier 1: effect_psram_alloc_<eid> (success or failure)]
   [Tier 2 (opt-in): effect_init_<eid> span]
   File: RendererActor.cpp, line ~628

7. Post-init yield: vTaskDelay(1) [line ~630]
   [Effect only visible to render after m_effectInitialized = true]
   File: RendererActor.cpp, line ~630
```

### Call Chain for Initialization
1. **Before init():** vTaskDelay(1) to yield CPU for loopTask/watchdog
2. **init() call:** Single call per effect, no re-entry, PSRAM allocations permitted
3. **After init():** vTaskDelay(1) again, then m_effectInitialized = true
4. **Effect becomes visible to render():** Only after initialization succeeds

---

## Interface Definitions

### IEffect Lifecycle (from /src/plugins/api/IEffect.h)

```cpp
class IEffect {
public:
    // Called once per effect selection, from Core 0 (RendererActor)
    virtual bool init(EffectContext& ctx) = 0;

    // Called 120x/sec from Core 1 (render task)
    virtual void render(EffectContext& ctx) = 0;

    // Called on effect deselection, from Core 0 (RendererActor)
    virtual void cleanup() = 0;

    virtual const EffectMetadata& getMetadata() const = 0;
};
```

### Effect Metadata
- **name:** Display name (max 32 chars)
- **category:** FIRE, WATER, NATURE, GEOMETRIC, QUANTUM, SHOCKWAVE, AMBIENT, PARTY, CUSTOM, LEGACY_LINEAR
- **id:** Stable EffectId (set during registration)
- **version:** Per-effect version number

### PSRAM Allocation Policy (Mandatory)
All effect buffers > 64 bytes **must** use:
```cpp
void* buffer = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
// Must free in cleanup()
```

Effects declared with stack-only state report `effect_psram_alloc_bytes = 0`.

---

## Tier 1 Instrumentation (Always-On, Cheap)

### Instants (Timestamps + Metadata)

| Instant Name | Arguments | Line | Notes |
|--------------|-----------|------|-------|
| `effect_switch` | old_eid, new_eid, timestamp_us | RendererActor.cpp:570 | Fired when SetEffect command processed |
| `effect_switch_rejected` | new_eid, reason_code, free_heap_bytes | RendererActor.cpp:587 | Fired if heap floor check fails (reason=1=LOW_HEAP) |
| `effect_cleanup_start_<eid>` | eid, timestamp_us | RendererActor.cpp:597 | Before oldReg->effect->cleanup() |
| `effect_cleanup_end_<eid>` | eid, duration_us | RendererActor.cpp:597+5 | After cleanup() returns |
| `effect_init_start_<eid>` | eid, timestamp_us | RendererActor.cpp:628 | Before newReg->effect->init(initCtx) |
| `effect_init_end_<eid>` | eid, duration_us, success_bool | RendererActor.cpp:628+5 | After init() returns; includes success flag |
| `effect_psram_alloc_<eid>` | eid, bytes_allocated, success_bool | *effect init()* | Reports PSRAM consumption per effect (0 if stack-only) |
| `effect_psram_alloc_failed_<eid>` | eid, requested_bytes, available_bytes | *effect init()* | When init() returns false due to PSRAM exhaustion |

**Tier 1 Counter Updates:**
- `effect_id_current` — sampled on every render frame (unify with Surface 1 if not already done)
- `effect_init_us` — duration of init() (derived: effect_init_end - effect_init_start)
- `effect_cleanup_us` — duration of cleanup() (derived: effect_cleanup_end - effect_cleanup_start)
- `effect_psram_alloc_bytes` — bytes allocated by currently-active effect (updated on effect_init_end)

---

## Tier 2 Instrumentation (Opt-In, FEATURE_TRACE_EFFECT_LIFECYCLE=1)

### Spans (Hierarchical Timing)

| Span Name | Start | Stop | Parent | Notes |
|-----------|-------|------|--------|-------|
| `effect_init_<eid>` | effect_init_start_<eid> | effect_init_end_<eid> | (none) | Wraps the entire init() call including vTaskDelay buffers |
| `effect_cleanup_<eid>` | effect_cleanup_start_<eid> | effect_cleanup_end_<eid> | (none) | Wraps cleanup() call |
| `effect_render_first_frame_<eid>` | First render() call after init() | render() returns | (none) | Detects cold-cache cost; fires once per effect load |
| `effect_zone_dispatch` | (if multi-zone) | (if multi-zone) | (optional) | If zone composition is involved; wrap per-zone dispatch |

**Tier 2 Counter Updates:**
- `effect_init_first_frame_us` — wall-clock duration of first render() after init() (detects cache misses, PSRAM latency)

---

## Failure Modes & Edge Cases

### 1. Heap Floor Rejection
**Scenario:** Internal DRAM < 12 KB during effect switch
**Detection:** effect_switch_rejected instant with reason_code=1
**Action:** System stays on current effect; caller is notified via log
**Instrumentation:** Tier 1 instant captures rejection + free heap at time of rejection

### 2. Init Failure (PSRAM Exhaustion)
**Scenario:** effect->init() returns false (usually heap_caps_malloc fails)
**Detection:**
  - effect_init_end_<eid> instant includes success_bool=false
  - effect_psram_alloc_failed_<eid> instant fired with requested vs available bytes
**Action:** System should fall back to safe default effect (REQUIRES VERIFICATION)
**Instrumentation:** Both Tier 1 instants capture the failure; counter `effect_psram_alloc_bytes` remains at previous value

### 3. Cleanup Leak Detection (Future Enhancement)
**Hypothesis:** cleanup() may not free all allocated memory
**Detection:** Compare heap before/after cleanup span (Tier 2 span overhead)
**Instrumentation:** Not yet required; can be added post-Tier-1 if needed

### 4. Mid-Render Effect Corruption (NOT OBSERVED)
**Potential Issue:** If render() executes while init() is running, render reads from half-initialized effect state
**Current Safeguard:** m_effectInitialized flag prevents render() dispatch until init() completes. render() is Core 1; handleSetEffect is Core 0. No data race observed.
**Instrumentation:** Tier 2 span `effect_render_first_frame_<eid>` will surface any anomalies

### 5. Double-Dispatch (NOT OBSERVED)
**Potential Issue:** Is init() called on new effect before cleanup() on old?
**Finding:** Code shows cleanup FIRST (line ~597), then init (line ~628). Order is safe.
**Instrumentation:** Sequence of instants will confirm order: cleanup_end should precede init_start

---

## Instrumentation Point Details

### effect_switch Instant
```
Triggered: RendererActor.cpp:handleSetEffect, after findById succeeds
Arguments:
  - old_eid: uint16_t (previous m_currentEffect)
  - new_eid: uint16_t (effectId parameter)
  - timestamp_us: uint64_t (micros())
Macro: TRACE_INSTANT_EFFECT_SWITCH(old_eid, new_eid)
Tier: Always-on (Tier 1)
Cost: ~20 bytes per event + timestamp
```

### effect_init_start_<eid> / effect_init_end_<eid> Instants
```
Triggered: RendererActor.cpp:handleSetEffect
  - Start: Before vTaskDelay(1); line ~619
  - End: After newReg->effect->init(initCtx); line ~629
Arguments (start):
  - eid: uint16_t
  - timestamp_us: uint64_t
Arguments (end):
  - eid: uint16_t
  - duration_us: uint32_t (end_timestamp - start_timestamp)
  - success: bool (initOk return value)
Macro: TRACE_INSTANT_EFFECT_INIT_START(eid); ... TRACE_INSTANT_EFFECT_INIT_END(eid, duration, success)
Tier: Always-on (Tier 1)
Cost: ~30 bytes per effect switch
```

### effect_psram_alloc_<eid> Instant
```
Triggered: effect->init() returning (success path)
Arguments:
  - eid: uint16_t
  - bytes: uint32_t (from effect's internal sizeof(PsramData) or heap_caps_get_free_size delta)
  - success: bool (always true in this variant)
Macro: TRACE_INSTANT_EFFECT_PSRAM_ALLOC(eid, bytes)
Tier: Always-on (Tier 1)
Cost: ~20 bytes per effect switch
Note: Effects must self-report allocation in init() or we compute delta via heap queries
```

### effect_cleanup_start_<eid> / effect_cleanup_end_<eid> Instants
```
Triggered: RendererActor.cpp:handleSetEffect
  - Start: Before oldReg->effect->cleanup(); line ~597
  - End: After cleanup() returns; line ~600
Arguments (start):
  - eid: uint16_t (oldEffectId)
  - timestamp_us: uint64_t
Arguments (end):
  - eid: uint16_t
  - duration_us: uint32_t
Macro: TRACE_INSTANT_EFFECT_CLEANUP_START(eid); ... TRACE_INSTANT_EFFECT_CLEANUP_END(eid, duration)
Tier: Always-on (Tier 1)
Cost: ~30 bytes per effect switch
```

---

## Measurement Strategy

### Counter: effect_id_current
- **Update Frequency:** Every render frame (120 Hz on Core 1)
- **Value:** Current m_currentEffect
- **Use:** Verify effect is active; correlate with other performance metrics
- **Sampling:** Can be a simple gauge (latest value) or histogram (distribution)

### Counter: effect_init_us
- **Computed From:** effect_init_end - effect_init_start
- **Update:** On effect_init_end instant
- **Use:** Identify effects with slow init(); flag PSRAM allocation dominance
- **Expected Range:** 1–500 ms (geometrical effects fast; PSRAM-heavy can hit 100+ ms)

### Counter: effect_cleanup_us
- **Computed From:** effect_cleanup_end - effect_cleanup_start
- **Update:** On effect_cleanup_end instant
- **Use:** Flag missing cleanup() free() calls; compare init vs cleanup symmetry
- **Expected Range:** 1–50 ms (PSRAM free is usually fast unless TLB flush is involved)

### Counter: effect_psram_alloc_bytes
- **Updated From:** effect_psram_alloc_<eid> instant
- **Sampling:** Latest value per active effect
- **Use:** Per-effect PSRAM footprint; identify memory hogs
- **Expected Range:** 0 (stack-only) to ~128 KB (complex effects)

---

## Files Inspected

1. `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3/src/core/actors/RendererActor.cpp` (handleSetEffect handler, init/cleanup calls)
2. `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3/src/core/actors/RendererActor.h` (m_currentEffect, m_effectInitialized state)
3. `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3/src/plugins/api/IEffect.h` (IEffect interface: init, cleanup, render)
4. `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3/src/plugins/PluginManagerActor.cpp` (effect registry)
5. `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3/src/core/state/Commands.h` (SetEffectCommand definition)
6. `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3/src/core/EffectTypes.h` (timing, metadata structures)

**Files Count:** 6 core files analyzed

---

## Summary Table: Instrumentation Points

| Category | Item | Tier | Type | Count |
|----------|------|------|------|-------|
| **Instants** | effect_switch | 1 | Instant | 1 |
| | effect_switch_rejected | 1 | Instant | 1 |
| | effect_cleanup_start/end | 1 | Instant Pair | 2 |
| | effect_init_start/end | 1 | Instant Pair | 2 |
| | effect_psram_alloc_* | 1 | Instant | 1 |
| | effect_psram_alloc_failed_* | 1 | Instant | 1 |
| **Spans (Tier 2)** | effect_init_<eid> | 2 | Span | 1 |
| | effect_cleanup_<eid> | 2 | Span | 1 |
| | effect_render_first_frame_<eid> | 2 | Span | 1 |
| **Counters** | effect_id_current | 1 | Gauge (120 Hz) | 1 |
| | effect_init_us | 1 | Histogram | 1 |
| | effect_cleanup_us | 1 | Histogram | 1 |
| | effect_psram_alloc_bytes | 1 | Gauge | 1 |

**Tier 1 Total:** 8 instants + 4 counters = 12 instrumentation points
**Tier 2 Total (opt-in):** 3 spans + 1 counter = 4 instrumentation points

---

## Open Questions & Recommendations

1. **Q: Where does init() failure fallback occur?**
   A: If newReg->effect->init() returns false, the code (per grep) does NOT explicitly show fallback. Recommend investigation: does m_effectInitialized stay false, causing next frame to retry? Or does it switch to a safe default?

2. **Q: How are PSRAM allocations measured inside effects?**
   A: Effects must manually track sizeof(PsramData) and report in init(). Recommend standardizing an optional EffectContext::reportPsramAlloc(bytes) call.

3. **Q: What is the cold-cache cost of first render() post-init()?**
   A: Tier 2 span `effect_render_first_frame_<eid>` will surface this. Expected: 2–3x slower than steady-state render if PSRAM accesses miss L3 cache.

4. **Q: Does multi-zone rendering complicate cleanup/init ordering?**
   A: Current code is single-zone (zoneId = 0xFF = full-strip). If future multi-zone effects are added, verify per-zone init() order. Recommend adding effect_zone_dispatch Tier 2 span.

---

## Confidence & Caveats

**Confidence: HIGH** — Code inspection is exhaustive; no ambiguity in the state machine.

**Caveats:**
- Init failure fallback behavior requires empirical verification (add trace to confirm)
- PSRAM allocation reporting is manually coded per effect (no automatic detection)
- First-frame render cost is hypothesis pending Tier 2 span measurement

---

## Next Steps for Implementation

1. Add TRACE_INSTANT_EFFECT_* macros to RendererActor.cpp:handleSetEffect
2. Add TRACE_INSTANT_EFFECT_PSRAM_ALLOC to effect->init() call sites (optional: effect self-reporting in EffectContext)
3. Implement Tier 2 span wrappers in RendererActor.cpp (effect_init_<eid>, effect_cleanup_<eid>)
4. Add effect_render_first_frame_<eid> span in RendererActor::render() after m_effectInitialized becomes true
5. Synchronize effect_id_current counter with Surface 1 if already instrumented there
6. Add counter derivatives: effect_init_us, effect_cleanup_us (computed from instant times)
7. Field-validate PSRAM allocation measurements against heap monitor

---

**End Section 6**
