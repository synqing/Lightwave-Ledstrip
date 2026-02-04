# P4 Contracts Layer Recommendation (Timing, Memory, Concurrency)

## 1. Summary

Two documents were produced:
- Control flow mapping: `docs/analysis/P4_Control_Flow_Mapping.md`
- HAL + audio pipeline deep scan: `docs/analysis/P4_HAL_Audio_Pipeline_Deep_Scan.md`

They answer:
- “What happens?” — **Yes** (call paths, scheduling semantics, and major integration points are mapped).
- “Where are the risks?” — **Yes** (concurrency hazards, silent degradation patterns, and timing hotspots are identified).

However, they do **not** prevent regression.

**Recommendation**: implement a lightweight contracts/invariants layer to continuously enforce the critical properties that keep P4 stable at 120 FPS with audio.

---

## 2. Completeness Evaluation

### 2.1 What the Documents Cover Well

- Execution and initialisation order with concrete code anchors.
- Actor scheduling semantics and the tickInterval contract (periodic vs self-clocked): `firmware/v2/src/core/actors/Actor.cpp:326-365`.
- Renderer hot path and its explicit watchdog yields around LED output: `firmware/v2/src/core/actors/RendererActor.cpp:646-652`, `RendererActor.cpp:704-710`.
- Audio publish/read handshake via SnapshotBuffer and freshness gating.
- HAL compile-time selection and P4 LED driver design.

### 2.2 What the Documents Cannot Guarantee

| Gap | Can it regress silently? | Why |
|---|---|---|
| Timing budgets (render frame, LED show, audio hop) | Yes | No build-time enforcement; runtime logs can be ignored; new effects can exceed budgets without failing CI. |
| “No heap allocations in render paths” | Yes | Heuristic scans are insufficient; indirect allocations can creep in. |
| Concurrency safety of MessageBus publish | Yes | Risk remains unless enforced by contract or code change. |
| Audio pipeline readiness (task running vs capture initialised) | Yes | ActorSystem sees task creation success, not pipeline health. |

**Conclusion**: documentation is necessary, but insufficient for long-term correctness.

---

## 3. Contracts Layer: Value Proposition

P4 characteristics make “silent rot” likely:
- Multi-core coordination (Audio ↔ Renderer) relies on careful cross-core sharing.
- Real-time deadlines are tight and are affected by both compute and I/O (WS2812 wire time).
- Long uptimes amplify heap fragmentation and timing drift.

A contracts layer adds:
- Continuous enforcement of the few invariants that must remain true.
- Fast feedback (fail builds / fail fast in debug) when invariants are violated.
- “Known-good envelope” for performance during ongoing effect and audio iteration.

---

## 4. Recommended Contract Types

### 4.1 Timing Contracts

Examples:
- Renderer `renderFrame()` (effect + prep) must complete in < 2ms (soft), < 4ms (hard fail in debug).
- LED driver `show()` must start transmission quickly (< 1ms CPU) and complete within expected wire time (wait time bounded).
- Audio hop capture + processing must complete within the hop deadline (10ms on P4 config): `firmware/v2/src/config/audio_config.h:82-91`.

### 4.2 Memory Contracts

Examples:
- “No heap delta during render frame” (heap free must not decrease across N frames).
- “Largest free block must remain above X” (fragmentation guardrail).

### 4.3 Concurrency Contracts

Examples:
- MessageBus subscription table must not be mutated while publishing (either enforce operational constraint or upgrade implementation).
- Cross-core buffers must use acquire/release fences (SnapshotBuffer already does).

---

## 5. Implementation Mechanisms

### 5.1 Compile-Time

- `static_assert` on config relationships.
  - Example: ensure `AUDIO_ACTOR_TICK_MS` is non-zero and within reasonable bounds.
  - Example: ensure `HOP_SIZE * 1000 / SAMPLE_RATE` matches tick alignment assumptions.

### 5.2 Runtime (Debug Builds)

- Per-frame timing guards (p99 window, worst-case trip).
- Heap delta guards around hot paths.
- Periodic health reporting of contract status.

### 5.3 CI Enforcement

- Native unit tests for pure contracts (math, ring buffers, mapping tables).
- Hardware-in-the-loop benchmarks for real-time budgets (optional; if available).

---

## 6. Proposed File Structure

Add a small contracts subsystem:
- `firmware/v2/src/core/contracts/StaticContracts.h`
- `firmware/v2/src/core/contracts/RealTimeContracts.h`
- `firmware/v2/src/core/contracts/ContractValidator.h/.cpp`

Keep it tiny and dependency-light (no dynamic allocation).

---

## 7. Code Examples (Sketches)

### 7.1 Timing Contract Macro

```cpp
// RealTimeContracts.h
#pragma once

#include <stdint.h>

namespace lightwaveos::core::contracts {

struct TimingBudget {
    uint32_t budget_us;
    uint32_t worst_us;
    uint32_t violations;
};

inline void timing_record(TimingBudget& b, uint32_t duration_us) {
    if (duration_us > b.worst_us) b.worst_us = duration_us;
    if (duration_us > b.budget_us) b.violations++;
}

} // namespace lightwaveos::core::contracts
```

Usage (RendererActor):
```cpp
uint32_t t0 = micros();
renderFrame();
uint32_t dt = micros() - t0;
contracts::timing_record(g_render_budget, dt);
```

### 7.2 Heap Delta Contract

```cpp
// ContractValidator.h
inline bool heap_delta_ok(size_t before, size_t after) {
    return after >= before;
}
```

Usage:
```cpp
size_t h0 = esp_get_free_heap_size();
renderFrame();
size_t h1 = esp_get_free_heap_size();
if (!contracts::heap_delta_ok(h0, h1)) {
    // Log + increment violations; optionally disable effect or force safe mode in debug
}
```

### 7.3 Concurrency Contract (Operational)

If MessageBus publish must remain lock-free, enforce:
- Subscriptions are only mutated during init/stop.

Implement as:
- A global “subscriptions frozen” flag set at end of init; subscribe/unsubscribe assert in debug.

---

## 8. Phased Implementation Plan

1. Phase 1 (P0): Heap delta checks around RendererActor frame hot path.
2. Phase 2 (P0): Timing budgets for audio hop and renderer frame sections (capture, DSP, renderFrame, show).
3. Phase 3 (P1): Static asserts for config relationships (hop/tick alignment, buffer sizing).
4. Phase 4 (P1): CI integration for native tests + optional benchmark reporting.

---

## 9. Bottom Line and Next Steps

**Bottom line**: The missing keystone is *enforceable invariants*.

**Next steps**
1. Decide whether MessageBus publish must remain lock-free; if yes, enforce “subscriptions frozen” contract.
2. Add heap delta + timing budgets in debug builds first (low risk, immediate value).
3. Promote the most important checks to always-on telemetry once stable.
