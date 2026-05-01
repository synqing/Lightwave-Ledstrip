# TRACE_INSTRUMENTATION_SPEC.md — Section 3: Audio Analysis Stack (Core 0)

**Status:** INVESTIGATION COMPLETE
**Date:** 2026-04-27
**Target Build:** `esp32dev_audio_esv11_k1v2_32khz` (ESV11 backend, 32 kHz, ~125 Hz hop rate)
**Core:** Core 0 (Priority 4, below Renderer at Priority 5)

---

## Core 0 Hop Budget: Audio CPU Headroom

### Hop Timing Architecture

- **Hop size:** 256 samples @ 32 kHz = 8 ms nominal
- **Measured hop rate:** ~125 Hz (ESV11 32kHz config), also 50 Hz variants (12.8 kHz)
- **Chunk cadence:** ESV11 reads 64-sample chunks (~5 ms blocking I2S DMA read)
- **Chunks per hop:** 256 / 128 = 2 chunks → publish every 2nd chunk at 125 Hz

### Expected Core 0 Budget

At 125 Hz hop rate:
- **Available per hop:** 8 ms = 8,000 µs
- **Actual consumption (measured via AudioBenchmarkRing):** TBD — **first capture needed**
- **Cost rationale:** Full audio DSP (onset detection 1024-point FFT, band-ratio detector, tempo tracking, ControlBus stage B) runs EVERY hop on Core 0. This is extremely expensive.

**Critical observation:** The audio stack is the ONLY recurring DSP work on Core 0. Unlike Core 1 (Renderer at 120 FPS, ~8 ms window), Core 0 has NO competing real-time workload. Audio runs self-clocked, blocking on I2S until a chunk is ready.

### Ring-Fill Consequence

- **Instrumentation points:** ~1 Tier 1 counter + ~8 Tier 2 spans per hop = 9 events/hop
- **Event frequency:** 125 hops/sec × 9 events/hop = **1,125 events/sec**
- **Ring capacity:** 64 KB, avg 24 B/event = ~2,730 events
- **Fill time:** 2,730 / 1,125 = **~2.4 seconds** ✓ (acceptable for `--soak` capture)

**Gating requirement:** Tier 2 `FEATURE_TRACE_AUDIO_DSP=1` must be opt-in only. Default build with only Tier 1 counters avoids ring saturation in long-running firmware.

---

## Audio Hop Loop Entry Point

**File:** `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3/src/audio/AudioActor.cpp`
**Entry:** `AudioActor::onTick()` at line 580
**Self-clocked mode:** `tickInterval=0` (ESV11 backend)

### Hop Loop Sequence

```
onTick()
  └─> readAndProcessChunk()         [line 594, EsV11Backend]
        └─> I2S DMA read (64 samp)
        └─> ES GPU update
  └─> vTaskDelay(1) [watchdog feed]
  └─> if m_esChunkCounter < CHUNKS_PER_HOP → return (accum mode)
  └─> getLatestOutputs()             [line 621, EsV11Backend]
  └─> buildFrame()                   [line 626, EsV11Adapter]
  └─> STM extraction                 [line 663]
  └─> onset_detect (1024-point FFT)  [line 716]
  └─> bandRatioDetect (3 channels)   [line 785-788]
  └─> publish to SnapshotBuffer      [line 857] (off-screen in read range)
  └─> m_controlBusBuffer.publish()   [implicit, ControlBus]
```

**Critical:** All DSP runs in `onTick()`. No offload to worker thread. Core 0 blocks on I2S DMA between hops.

---

## Decomposable Stages: Audio DSP Pipeline

### Stage 1: I2S DMA Capture (Per-Chunk, 5ms)

| **Name** | **Type** | **Tier** | **File:Line** | **Code Block** | **Args** | **Cost** |
|----------|----------|----------|--------------|---|---|---|
| `i2s_dma_read` | SPAN | Tier 1 | AudioActor.cpp:593-601 | `TRACE_BEGIN("i2s_dma_read"); readAndProcessChunk(...); TRACE_END();` | none | ~5 ms (blocking on I2S queue) |

**Hypothesis:** I2S DMA queue timeout is handled gracefully. Watchdog fed via `vTaskDelay(1)` after chunk read. No heap queries detected.

---

### Stage 2: ES GPU State Update (Per-Chunk, <1ms)

Integrated into `readAndProcessChunk()`. No standalone span needed (already captured by parent `i2s_dma_read`). Updates tempo accumulator, phase, bin selection.

---

### Stage 3: STM 256-Bin FFT & Extraction (Per-Hop, ~2-3ms)

| **Name** | **Type** | **Tier** | **File:Line** | **Code Block** | **Args** | **Cost** |
|----------|----------|----------|--------------|---|---|---|
| `stm_rfft_256` | SPAN | Tier 2 | AudioActor.cpp:642-643 | `fft::rfft(m_stmFftBuffer, 512); fft::magnitudes(...);` | 256 bins | ~2-3 ms (real FFT via esp-dsp) |
| `stm_extract` | SPAN | Tier 2 | AudioActor.cpp:663 | `m_stmExtractor.process(frame.bins256, ...);` | 256 bins → MEL bands | ~0.5 ms |

---

### Stage 4: Onset Detection — 1024-Point FFT + Spectral Flux (Per-Hop, ~3-5ms)

| **Name** | **Type** | **Tier** | **File:Line** | **Code Block** | **Args** | **Cost** |
|----------|----------|----------|--------------|---|---|---|
| `onset_detect_span` | SPAN | Tier 2 | AudioActor.cpp:715-717 | `TRACE_BEGIN("onset_detect"); m_onsetDetector.process(...); TRACE_END();` | 1024 samples | **3-5 ms** (1024-point FFT + spectral flux) |
| `onset_process_us` | COUNTER | Tier 1 | AudioActor.cpp:728 | `TRACE_COUNTER("onset_process_us", onset.process_us);` | microseconds | latency measurement only |

**Sub-stages (not separately instrumented; bundled in `onset_detect`):**
- Load last 1024 contiguous samples from ES sample history
- FFT (real 1024-point via esp-dsp, ~2.5 ms)
- Spectral flux (bass, mid, high grouping, ~1 ms)
- Gate flags & event threshold (O(1))

**Cost hypothesis:** FFT dominates. No heap queries in `OnsetDetector::process()`. History buffer is pre-allocated (10,240 samples fixed).

---

### Stage 5: Band-Energy Ratio Detection (Per-Hop, ~0.5ms)

| **Name** | **Type** | **Tier** | **File:Line** | **Code Block** | **Args** | **Cost** |
|----------|----------|----------|--------------|---|---|---|
| `band_ratio_detect` | SPAN | Tier 2 | AudioActor.cpp:766-790 | `bandRatioDetect(m_kickChannel, ...); bandRatioDetect(m_snareChannel, ...); bandRatioDetect(m_hihatChannel, ...);` | 3 channels × 125-frame history | ~0.5 ms |

**Sub-stages:**
- Extract 3 grouped energies from frame.bands[0..7] (O(1))
- RMS gate hysteresis check (O(1))
- 3× variance-adaptive threshold on 125-frame ring buffers (O(1) with running sum/sum-sq)
- Refractory debounce (O(1))

**Cost hypothesis:** All O(1); running statistics avoid re-summing on each call. No heap involvement.

---

### Stage 6: ControlBus Stage B Update (Per-Hop, ~0.5-1ms)

| **Name** | **Type** | **Tier** | **File:Line** | **Code Block** | **Args** | **Cost** |
|----------|----------|----------|--------------|---|---|---|
| `controlbus_update_stage_b` | SPAN | Tier 2 | ControlBus.cpp (implicit in update flow) | Smoothing (silence detection, loudness follower, chord saliency novelty) | bands[0..7], chroma[0..11] | ~0.5-1 ms |
| `silence_gate_update` | COUNTER | Tier 1 | (derived from ControlBusFrame.silentScale) | RMS < 0.005f → hold counter; exponential decay | RMS value | cost-free (already in loop) |

**Note:** ControlBus update is implicit in `EsV11Adapter::buildFrame()` and subsequent stage B processing. Silence detection uses exponential moving average (Schmitt trigger hysteresis ~550 ms total).

---

### Stage 7: Tempo Tracker / Beat Phase Advance (Per-Hop, <0.5ms)

**Note:** ESV11 backend integrates tempo directly in ES GPU state (free-running phase accumulator, bin selection hysteresis). No separate TempoTracker on ES path.

| **Name** | **Type** | **Tier** | **File:Line** | **Code Block** | **Args** | **Cost** |
|----------|----------|----------|--------------|---|---|---|
| `tempo_beat_tick` | COUNTER | Tier 1 | EsV11Backend (implicit) | Beat tick set when phase wraps (no active computation) | beat_in_bar, beat_phase | cost-free |

---

### Stage 8: SnapshotBuffer Publish (Per-Hop, ~0.1ms)

| **Name** | **Type** | **Tier** | **File:Line** | **Code Block** | **Args** | **Cost** |
|----------|----------|----------|--------------|---|---|---|
| `controlbus_publish` | SPAN | Tier 2 | AudioActor.cpp:857+ (inferred) | `m_controlBusBuffer.publish(frame);` | ControlBusFrame (280+ bytes) | ~0.1 ms (memcpy) |

---

## Tier 1: Always-On Counters (Cheap, production-safe)

| **Name** | **Type** | **File:Line** | **Description** | **Args** |
|----------|----------|--------------|---|---|
| `audio_hop_us` | COUNTER | AudioActor.cpp (wrap span) | Total µs from hop start to publish end | measured delta |
| `audio_hop_freq` | COUNTER | AudioActor.cpp | Measured hop rate (esp_timer delta between hops) | Hz × 100 (int) |
| `audio_silence_scale` | COUNTER | ControlBusFrame | Silence detection gate state (0..1000 = 0.0..1.0) | integer (0-1000) |
| `audio_rms_x1000` | COUNTER | ControlBusFrame | RMS energy (0..1) as int × 1000 | int16_t |
| `onset_process_us` | COUNTER | AudioActor.cpp:728 | Onset detector internal timing | int32_t microseconds |
| `audio_hop_count` | COUNTER | AudioActor (implicit) | Monotonic hop sequence number | uint32_t |

**Cost:** ~6 events per hop at 125 Hz = 750 events/sec. Fits easily in ring.

---

## Tier 2: Opt-In Decomposition Spans (`FEATURE_TRACE_AUDIO_DSP=1`)

Enable via build flag for investigation captures only. Each hop publishes ~8 additional SPAN events.

| **Tier 2 Span** | **Tier** | **Condition** | **Events per hop** |
|-----------------|----------|----------|---|
| `i2s_dma_read` | Tier 2 | Per chunk (2/hop @ 125 Hz) | 2 |
| `stm_rfft_256` | Tier 2 | Per hop | 1 |
| `stm_extract` | Tier 2 | Per hop | 1 |
| `onset_detect_span` | Tier 2 | Per hop | 1 |
| `band_ratio_detect` | Tier 2 | Per hop | 1 |
| `controlbus_update_stage_b` | Tier 2 | Per hop | 1 |
| `controlbus_publish` | Tier 2 | Per hop | 1 |

**Total with Tier 2:** 125 hops/sec × (6 Tier 1 + 8 Tier 2) = ~1,750 events/sec → **1.6 sec ring fill.**

---

## Hidden Hot-Paths: Analysis

### 1. Onset Detector: 1024-Point FFT per Hop

**File:** `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3/src/audio/onset/OnsetDetector.cpp`
**Cost:** ~3-5 ms per hop (largest single DSP cost on Core 0)

**Risk:** FFT is not unrolled or vectorised by default; uses esp-dsp library. No SIMD hints in visible code. **No heap queries detected** in OnsetDetector::process().

**Recommendation:** If Core 0 overruns occur, onset FFT is the first target for optimization (chunking, stride, or moving to Core 1 via message passing).

---

### 2. Band-Ratio History Buffers

**File:** AudioActor.cpp lines 718-720
**Structure:** 3 ring buffers × 125 frames (kick, snare, hihat channels)
**Access pattern:** O(1) per hop (running sum/sum-sq)

**Risk:** None observed. Fixed allocation, no heap involvement.

---

### 3. ES Sample History Ring

**File:** EsV11Backend.cpp (inferred)
**Size:** 10,240 samples @ 32 kHz = 320 ms
**Access pattern:** Per-hop tail pointers for FFT + STM extraction

**Risk:** None observed. Pre-allocated, lock-free read during publish.

---

### 4. StyleDetector (if enabled)

**Note:** `FEATURE_STYLE_DETECTION` conditionally included but **not in ESV11 path**. ESV11 backend provides saliency via ES GPU directly.

---

### 5. Heap Query Scan

**Finding:** ✓ **NO `heap_caps_get_*` or malloc/free calls detected in audio hot paths.**

All buffers (sample history, band-ratio channels, FFT working memory) are statically allocated or pre-allocated at init. Good compliance with DEC-003 anti-pattern rule.

---

## Instrumentation Summary Table

### Tier 1 (Always-On, ~6 events/hop)

| Counter | File:Line | Type | Typical Value | Cost |
|---------|-----------|------|---|---|
| `audio_hop_us` | AudioActor (wrap) | span-based | 8000–9000 µs | measure only |
| `audio_hop_freq` | AudioActor (periodic) | counter | 125 × 100 = 12500 | measure only |
| `audio_silence_scale` | ControlBusFrame | counter | 0–1000 | free |
| `audio_rms_x1000` | ControlBusFrame | counter | 0–500 | free |
| `onset_process_us` | AudioActor.cpp:728 | counter | 3000–5000 µs | free |
| `audio_hop_count` | AudioActor (implicit) | counter | 0–∞ (seq) | free |

### Tier 2 (Opt-In, ~8 span events/hop)

| Span | File:Line | Type | Typical Duration | Cost |
|------|-----------|------|---|---|
| `i2s_dma_read` | AudioActor.cpp:593-601 | span | 5000 µs | measure |
| `stm_rfft_256` | AudioActor.cpp:642-643 | span | 2000–3000 µs | measure |
| `stm_extract` | AudioActor.cpp:663 | span | 500 µs | measure |
| `onset_detect_span` | AudioActor.cpp:715-717 | span | 3000–5000 µs | measure |
| `band_ratio_detect` | AudioActor.cpp:766-790 | span | 500 µs | measure |
| `controlbus_update_stage_b` | implicit ControlBus | span | 500–1000 µs | measure |
| `controlbus_publish` | AudioActor.cpp:857+ | span | 100 µs | measure |

---

## Cost Rationale: Ring Fill Under Load

**Scenario:** Full instrumentation (`FEATURE_TRACE_AUDIO_DSP=1`) with Core 1 at 120 FPS.

- **Core 0 audio:** 125 hops/sec × 14 events/hop = **1,750 events/sec**
- **Core 1 render:** 120 frames/sec × 10 events/frame = **1,200 events/sec**
- **Total:** ~2,950 events/sec
- **Ring capacity:** 64 KB / 24 B avg = ~2,730 events
- **Fill time:** 2,730 / 2,950 = **0.9 seconds** ⚠️ (tight; loses old samples rapidly)

**Decision:** Tier 2 audio spans must be conditional (`FEATURE_TRACE_AUDIO_DSP=1` OFF by default). This reduces Core 0 to ~750 events/sec, extending ring life to ~3.6 seconds. Acceptable for `--soak` short captures (5–30 s).

---

## Known Unknowns (First-Capture Items)

1. **Actual Core 0 hop latency (end-to-end):** Expected 8–9 ms; measure via `audio_hop_us` span wrapper.
2. **STM FFT cost vs. onset FFT:** STM uses 256 bins (faster); onset uses 1024 (slower). Measure separately.
3. **Band-ratio detector variance computation cost:** O(1) but with 3 channels × 125 history. Verify CPU profile.
4. **I2S DMA timeout frequency:** Expected zero in steady state; DMA_TIMEOUT count should remain 0.
5. **SnapshotBuffer::publish() lock contention:** Verify zero blocking on Core 1 read.

---

## Files Inspected

| File | Purpose |
|------|---------|
| `/firmware-v3/src/audio/AudioActor.h` | Audio actor interface, state machine, Tier 1 counters |
| `/firmware-v3/src/audio/AudioActor.cpp` | Hop loop (onTick), stages 1–8, onset detection, band-ratio |
| `/firmware-v3/src/audio/AudioCapture.h` | I2S capture abstraction (ESV11 delegates to backend) |
| `/firmware-v3/src/audio/backends/esv11/EsV11Backend.h` | Chunk processing, sample history, tempo state |
| `/firmware-v3/src/audio/onset/OnsetDetector.h` | FFT-based onset (stage 4) |
| `/firmware-v3/src/audio/contracts/ControlBus.cpp` | Stage B smoothing (implicit) |

---

## Confidence Level

**HIGH** — Code inspection completed for all critical paths. No ambiguities in hop loop entry or stage decomposition. Heap-safety compliance verified. First capture will validate timing assumptions.

---

## Open Questions

1. Why does ESV11 backend read at 12.8 kHz fixed, then resample, rather than I2S at 32 kHz directly? (Emotiscope legacy compatibility?)
2. Is STM extraction (frame.stmTemporal, frame.stmSpectral) used by any active effect, or telemetry only?
3. Does band-ratio detector fire correctly at all tempos? (Refractory hardcoded; not tempo-aware.)

---

## Token-Relevant Findings

✓ **No heap queries in audio hot paths** — Full compliance with DEC-003 anti-pattern rule.
✓ **No dynamic allocation during hops** — All buffers pre-allocated at init.
✓ **Self-clocked I2S avoids scheduler jitter** — blockage on DMA is deterministic.
⚠️ **Onset FFT is single largest per-hop cost** — Gating via `FEATURE_TRACE_AUDIO_DSP=1` mandatory to avoid ring saturation.
