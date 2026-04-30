# Surface 2 — Audio→Render Handoff

> ### ⚡ Captain decision 2026-04-27 — `ControlBusFrame` shall live in internal DRAM, not PSRAM
> 5 KB DRAM cost approved. This is the **primary remediation** for H2; expected 5–10× speedup on `audio_snapshot_read`. Tier 2 decomposition (`bus_copy_memcpy` etc.) becomes **diagnostic-only post-fix** — implement only if the DRAM move does NOT close the gap to <200 µs p99. See "Implementation order under Captain decision" below.

## Contract
**audio_snapshot_read (ReadLatest call) shall complete within 200 µs p99** after the DRAM relocation lands. Pre-fix baseline = 836 µs p99 (today's measured value).

## Hypothesis
**H2 (Captain's strategy):** `audio_snapshot_read` is 534 µs p50 / 836 µs p99 — 10× slower than expected memcpy speed (~50 µs at L1 rates). Root cause assumed (a) PSRAM cache miss; Captain has approved the architectural fix without further proof since the cost-of-being-wrong (5 KB DRAM) is small and the cost-of-deferring (continued opacity) is high.

## Implementation order under Captain decision
1. Apply Tier 1 instrumentation (rows #4 `audio_snapshot_age_us`, #5 `hop_seq_lag`, #6 `size_bytes`) — establishes the measurement contract.
2. Capture the **before** baseline trace of any effect (e.g. 0x2102) for 30 s; record `audio_snapshot_read` p50/p99 in `PERFORMANCE_BASELINE.json` as `before_dram_relocation`.
3. Locate the `SnapshotBuffer<ControlBusFrame>` allocation. Confirm the placement attribute (look for `EXT_RAM_ATTR`, `MALLOC_CAP_SPIRAM`, `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`, or PSRAM-affinity static-init). Ensure it ends up in **internal DRAM** — either by leaving it as a value member of a DRAM-resident actor (default), or by explicit `heap_caps_malloc(..., MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)`.
4. Rebuild `esp32dev_audio_esv11_k1v2_32khz_trace`, flash, capture the **after** trace.
5. If `audio_snapshot_read` p99 is now <200 µs, success — record `after_dram_relocation` baseline, do NOT implement Tier 2 decomposition spans.
6. If `audio_snapshot_read` p99 is still >300 µs, fall back to implementing Tier 2 decomposition (rows #1–3, #7–9) to localise the remaining cost.

## Current Architecture

### Lock-free double buffer (SnapshotBuffer<T>)
- **File:** `firmware-v3/src/audio/contracts/SnapshotBuffer.h`
- **Size:** Dual-buffered, T by value; no heap allocation
- **Read path:** `ReadLatest(out)` — acquires m_active index, copies m_buf[idx] by value into caller, with 1 retry on sequence change
- **Sequence:** Monotonic counter m_seq; reader detects staleness via hop_seq delta

### ControlBusFrame structure
- **File:** `firmware-v3/src/audio/contracts/ControlBus.h`
- **Size:** ~5120 bytes (verified by static_assert)
- **Location:** Unknown (PSRAM vs internal DRAM not specified in headers; likely PSRAM if allocated in heap)
- **Content:**
  - Basic RMS/flux/bands/chroma (Stages 1–4)
  - Derived features: saliency, motion-semantic (jitter/syncopation/pitch_contour), silence scale, chord state
  - Spectral details: bins256[], waveform[], STM (temporal/spectral modulation)
  - Tempo tracker state, onset triggers, ES v1.1 compatibility fields

### Renderer handoff call path
- **File:** `firmware-v3/src/core/actors/RendererActor.cpp` (onTick, 120 FPS)
- **Entry:** Via `m_controlBusBuffer->ReadLatest(m_lastControlBus)` on Core 1
- **Destination:** `m_lastControlBus` — member variable (by-value copy)
- **Downstream use:**
  - `m_sharedAudioCtx` (EffectContext.audio) — populated in renderFrame()
  - `m_motionEngine` — processes motion-semantic fields
  - `m_motionShaper` — temporal envelope shaping (Layer 3)

### Derived feature computation
- **File:** `firmware-v3/src/audio/contracts/ControlBus.cpp`
- **Stages completed BEFORE snapshot publish:**
  1. **Stage 1 (Clamp):** raw → [0,1]
  2. **Stage 2 (Spike removal):** lookahead 3-frame delay, despiking
  3. **Stage 3 (Zone AGC):** per-zone normalization (4 zones × 2 bands each for 8-band)
  4. **Stage 4 (Attack/release):** asymmetric smoothing (band_attack 0.15, band_release 0.03)
  5. **Stage 4b–7 (Derived):** `applyDerivedFeatures()` — chord detection, liveliness EMA, saliency, silence hysteresis, motion-semantic jitter/syncopation/pitch contour
- **Key observation:** ALL of Stages 1–7 complete in AudioActor's hop callback (Core 0, 62.5 Hz), **before** snapshot is published via ControlBus::Publish()

## Required Additions

| # | Name | Type | Tier | File:line | Args | Cost | Hypothesis | Sanity |
|---|------|------|------|-----------|------|------|-----------|--------|
| 1 | `audio_snapshot_read` | RAII Span | 2 | RendererActor.cpp:onTick() | — | ~40 B | Outer wrapper to capture entire ReadLatest + early derivation work | Should total to p99 baseline (836 µs) |
| 2 | `bus_copy_memcpy` | Span | 2 | SnapshotBuffer.h:ReadLatest() | size=5120 | ~40 B | Just the m_buf[idx] = copy operation (3× atomic loads + 1× byte copy) | Expected <100 µs; if >300 µs → PSRAM confirmed |
| 3 | `bus_retry_check` | Counter | 2 | SnapshotBuffer.h:ReadLatest() | seq_changed:bool | ~24 B | Track how often retry occurs (s1 != s0) | Should be <5% of calls; >50% = contention issue |
| 4 | `audio_snapshot_age_us` | Counter | 1 | RendererActor.onTick(), post-read | age_us:uint32_t | ~24 B | (micros() - m_lastAudioMicros) OR (now_us - frame.t.monotonic_us) | Histogram [0..8000]; >8000 µs = stale, audio thread stalled |
| 5 | `audio_snapshot_hop_seq_lag` | Counter | 1 | RendererActor.onTick(), post-read | lag:uint32_t | ~24 B | (m_lastControlBusSeq - latest_read_seq) & 0xFFFFFFFF | Detect if renderer laps audio; should be small, stable |
| 6 | `audio_snapshot_size_bytes` | Counter | 1 | RendererActor.onStart() | sizeof(ControlBusFrame):uint32_t | ~24 B | sizeof(audio::ControlBusFrame) — compile-time constant | Should be exactly 5120; validates no bloat |
| 7 | `audio_ctx_populate_us` | Span | 2 | RendererActor.cpp:renderFrame() | — | ~40 B | Duration from after snapshot read to m_sharedAudioCtx fully populated | Expected <100 µs; if significant, indicates slow copying/derivation on Core 1 |
| 8 | `motion_engine_tick_us` | Span | 2 | RendererActor.cpp:renderFrame() | — | ~40 B | Duration of m_motionEngine.Tick() call (motion-semantic inference) | Expected <50 µs; >100 µs = CPU budget issue |
| 9 | `motion_shaper_tick_us` | Span | 2 | RendererActor.cpp:renderFrame() | — | ~40 B | Duration of m_motionShaper.Tick() call (temporal envelope shaping) | Expected <30 µs; >50 µs = unexpected processing |
| 10 | `snapshot_read_retries_total` | Counter | 1 | RendererActor.onTick() | count:uint32_t | ~24 B | Cumulative retries across all snapshot reads since boot | Diagnostic: ratio to total reads tells us contention frequency |

## Code Blocks to Insert

### RendererActor.cpp:onTick() — outer span
```cpp
// Lines ~650 (estimated; search for "onTick()" in RendererActor.cpp)
void RendererActor::onTick() {
    uint32_t frameStartUs = micros();

    TRACE_SCOPE("audio_snapshot_read");  // Tier 2: outer handoff span

    // --- Snapshot read with age tracking
    uint32_t preReadMicros = micros();
    uint32_t seqBefore = m_lastControlBusSeq;
    m_lastControlBusSeq = m_controlBusBuffer->ReadLatest(m_lastControlBus);
    uint32_t postReadMicros = micros();

    // Tier 1: always-on counters
    uint32_t snapshotAgeUs = preReadMicros - m_lastAudioMicros;
    TRACE_COUNTER("audio_snapshot_age_us", snapshotAgeUs);

    uint32_t hopSeqLag = (seqBefore > m_lastControlBusSeq) ?
        (0xFFFFFFFF - seqBefore + m_lastControlBusSeq) :
        (m_lastControlBusSeq - seqBefore);
    TRACE_COUNTER("audio_snapshot_hop_seq_lag", hopSeqLag);

    // Check if retry happened (sequence changed after copy)
    bool readRetried = (m_lastControlBusSeq != seqBefore);
    if (readRetried) {
        static std::atomic<uint32_t> s_retryCount{0};
        s_retryCount.fetch_add(1, std::memory_order_relaxed);
        TRACE_COUNTER("snapshot_read_retries_total", s_retryCount.load());
    }

    m_lastAudioMicros = preReadMicros;  // Update for next age calculation

    // Continue normal render...
```

### SnapshotBuffer.h:ReadLatest() — decompose copy operation
```cpp
// Lines ~60–89 in SnapshotBuffer.h ReadLatest() method
uint32_t ReadLatest(T& out) const {
    uint32_t s0 = m_seq.load(std::memory_order_acquire);
    uint32_t idx = m_active.load(std::memory_order_acquire);

    if (idx > 1) idx = 0;  // Defensive

    // Tier 2: measure just the memcpy-like copy operation
    {
        TRACE_SCOPE("bus_copy_memcpy");
        out = m_buf[idx];  // By-value struct copy (~5 KB)
    }

    std::atomic_thread_fence(std::memory_order_acquire);
    uint32_t s1 = m_seq.load(std::memory_order_acquire);

    // Tier 2: track retry necessity
    if (s1 != s0) {
        TRACE_INSTANT("bus_retry_check");
        TRACE_COUNTER("bus_retry_check", 1);  // Count retry events

        idx = m_active.load(std::memory_order_acquire);
        if (idx > 1) idx = 0;

        TRACE_SCOPE("bus_copy_retry");  // Second attempt
        out = m_buf[idx];
        s1 = m_seq.load(std::memory_order_acquire);
    }
    return s1;
}
```

### RendererActor.cpp:onStart() — log snapshot size once
```cpp
// In onStart() method, after initialization
void RendererActor::onStart() {
    // ... existing init code ...

    // Tier 1: log frame size once at boot (diagnostic constant)
    TRACE_COUNTER("audio_snapshot_size_bytes", sizeof(audio::ControlBusFrame));

    // ... rest of onStart ...
}
```

### RendererActor.cpp:renderFrame() — decompose downstream work
```cpp
// Lines ~750 (estimated; in renderFrame() method after snapshot is read)
void RendererActor::renderFrame() {
    // ... snapshot already read in onTick() ...

    // Tier 2: measure EffectContext population
    {
        TRACE_SCOPE("audio_ctx_populate");

        // Populate m_sharedAudioCtx from m_lastControlBus
        // (Existing code; just wrap with span)
        m_effectContext.audio.rms = m_lastControlBus.rms;
        m_effectContext.audio.flux = m_lastControlBus.flux;
        m_effectContext.audio.bands = m_lastControlBus.bands;  // array copy
        // ... other audio context fields ...
    }

    // Tier 2: measure motion engine inference
    {
        TRACE_SCOPE("motion_engine_tick");
        m_motionEngine.Tick(m_lastControlBus);  // Layer 2: motion-semantic inference
    }

    // Tier 2: measure motion shaper temporal envelope
    {
        TRACE_SCOPE("motion_shaper_tick");
        m_motionShaper.Tick(m_lastControlBus);  // Layer 3: temporal shaping
    }

    // ... rest of renderFrame ...
}
```

## Tier Rationale

### Tier 1: Always-on counters (no cost, diagnostic only)
- `audio_snapshot_age_us`: **Critical** for understanding render-audio synchronization skew. Must be on always; costs only 1 counter per frame.
- `audio_snapshot_hop_seq_lag`: Detects if renderer is outpacing audio thread (lapping). Cheap 32-bit arithmetic.
- `audio_snapshot_size_bytes`: One-time at boot. Validates frame size hasn't bloated.
- `snapshot_read_retries_total`: Running total. Identifies contention pattern over time.

### Tier 2: Opt-in spans (FEATURE_TRACE_AUDIO_HANDOFF=1)
- `audio_snapshot_read`: Outer span captures complete handoff surface. Enabled only for investigation.
- `bus_copy_memcpy` & `bus_retry_check`: Decomposes SnapshotBuffer internals. Required to isolate PSRAM vs lock-free overhead.
- `audio_ctx_populate_us` / `motion_engine_tick_us` / `motion_shaper_tick_us`: Measure downstream work on Core 1 after snapshot read. Critical to distinguish (a) slow read from (b) slow derivation/inference.

**Reason not to enable Tier 2 always:** Each span is ~40 B and runs at 120 FPS (8333 µs period) = ~4.8 KB/s overhead. With ~7 spans, that's ~34 KB/s, which is acceptable for an opt-in build flag but not for the baseline Tier 0.

## Acceptance Criteria

### Phase 1: Instrumentation deployment
1. Merge all Tier 1 counters into Tier 0 build (they are zero-cost diagnostics).
2. Create `FEATURE_TRACE_AUDIO_HANDOFF` build flag; add Tier 2 spans.

### Phase 2: Baseline capture (production device, real audio load)
1. Run with FEATURE_TRACE_AUDIO_HANDOFF=1 for 60 seconds at 120 FPS.
2. Capture trace (binary or JSON export from trace buffer).
3. Extract histograms for:
   - `audio_snapshot_read` → p50, p99, max
   - `bus_copy_memcpy` → p50, p99 (expected <100 µs; >300 µs = PSRAM)
   - `audio_ctx_populate_us` → p50, p99
   - `motion_engine_tick_us` → p50, p99
   - `motion_shaper_tick_us` → p50, p99
   - `audio_snapshot_age_us` → histogram, should peak near 0–100 µs (normal), with tail to ~8 ms (worst case = one full audio hop)
   - `snapshot_read_retries_total` → should be <5% of 120 FPS × 60s = 7200 calls

### Phase 3: Hypothesis validation
**Success:** Sum of (bus_copy_memcpy + audio_ctx_populate_us + motion_engine_tick_us + motion_shaper_tick_us) equals audio_snapshot_read **within ±10 µs**.

**Failure modes:**
- **If bus_copy_memcpy alone > 300 µs:** PSRAM cache miss is bottleneck. Recommend ADR for DRAM-cached snapshot buffer (trade 5 KB internal RAM for 10× speedup).
- **If audio_ctx_populate_us > 200 µs:** EffectContext copy/derivation is slow. Check for field-by-field translation logic instead of memcpy.
- **If motion_engine_tick_us + motion_shaper_tick_us > 200 µs:** Inference layer is CPU-bound on Core 1. May need Tier 3 runtime toggle to disable expensive layer 2/3 during high-effect-complexity frames.
- **If audio_snapshot_age_us histogram peak > 1 ms:** Renderer-audio timing skew is larger than expected; investigate audio hop callback jitter or Core 0 preemption.

### Phase 4: Long-term monitoring
- Add `audio_snapshot_age_us` histogram to dashboard telemetry (Tier 1 counter).
- Alert if p99 age > 2 ms (suggests audio thread stalled or missed hop).
- Archive snapshot_read_retries_total for trend analysis (if rising over weeks, indicates increasing lock contention).

## Open Questions

1. **Where is ControlBusFrame allocated?** PSRAM or internal DRAM?
   - If PSRAM: baseline 5–10 µs read + 50–100 µs PSRAM access latency (empirically 50 MB/s throughput) = ~200 µs. With spike detection (3-frame lookahead) and Zone AGC loops, could explain 500+ µs.
   - **Action:** Check SnapshotBuffer instance creation in AudioActor. If `new SnapshotBuffer<ControlBusFrame>()` without DRAM annotation, assume PSRAM. Measure PSRAM vs DRAM copy speed separately.

2. **Is there per-frame translation work in RendererActor after snapshot read?**
   - EffectContext population looks like simple field copy, but need to verify no hidden loops.
   - **Action:** Capture `audio_ctx_populate_us` span; if >100 µs, audit renderFrame() for unexpected O(n) work.

3. **What is the motion-semantic inference latency?**
   - MotionSemanticEngine::Tick() and MotionShaper::Tick() are on Core 1; if they're heavy (>50 µs), they account for significant handoff latency.
   - **Action:** Add Tier 2 spans; if either >50 µs, consider Tier 3 runtime toggle.

4. **Why is snapshot read 10× slower than memcpy?**
   - Lock-free retry (1–5% overhead): Not enough.
   - PSRAM cache miss: Likely culprit if SnapshotBuffer is in PSRAM.
   - Renderer-audio timing skew: If audio is delayed, snapshot is stale, renderer re-reads → retry penalty.
   - **Action:** Correlation analysis — cross-reference `snapshot_read_retries_total` with audio thread jitter. If retries spike when audio jitter spikes, timing skew is root cause.

5. **Should derived features computation move earlier?**
   - Currently `applyDerivedFeatures()` runs in audio thread's UpdateFromHop(); renderer just reads the published frame.
   - No further computation needed on Core 1 after snapshot read (motion-semantic engine is separate).
   - **Implication:** audio_snapshot_read latency is **purely** lock-free copy + retry + PSRAM access; not derivation. But motion_engine_tick + motion_shaper_tick are on Core 1 and add to total handoff time.
   - **Action:** Clarify contract: is "audio_snapshot_read" just the SnapshotBuffer::ReadLatest call, or the full audio→render pipeline up to effect execution? (This spec assumes just the read; motion engine is downstream.)

## Token-Relevant Observation

**Single most important finding:** ControlBusFrame is **~5 KB and likely allocated in PSRAM** (slow, 50 MB/s vs ~1 GB/s for internal DRAM). A by-value copy at 62.5 Hz should take ~50 µs in DRAM but ~200 µs in PSRAM. Lock-free retry under contention doubles this. **If bus_copy_memcpy measures > 300 µs, switching to internal DRAM (or a smaller summary frame) would reduce handoff latency by 5–10×.**

This is the architectural decision point: **trade 5 KB internal RAM for 10× faster audio sync**, or accept current latency and compensate with jitter-tolerant effects.
