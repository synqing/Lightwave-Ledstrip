# Commits 5-6: feat(k1): Add K1 beat tracker debug infrastructure and CLI

## Original Problem/Requirement

The K1-Lightwave beat tracker pipeline (4-stage: Novelty → Resonators → Tactus → Beat Clock) runs on Core 0 (AudioActor) at 62.5Hz. Debugging requires:
- Cross-core visibility (Core 0 → Core 1 for RendererActor/WebSocket)
- Real-time state capture without blocking audio processing
- Compact data format (minimize memory and transfer overhead)
- CLI interface for interactive debugging (BPM, confidence, phase, lock state)

Previous approach had no debug visibility into K1 pipeline internals.

## Solution Design Rationale

**Commit 5: Debug Infrastructure**

**K1DebugMetrics (`K1DebugMetrics.h`):**
- 64-byte cache-aligned debug sample structure
- Fixed-point encoding for compact storage:
  - BPM: `uint16_t bpm_x10` (60-180 BPM → 600-1800)
  - Magnitudes: `uint16_t magnitude_x1k` (0.0-1.0 → 0-1000)
  - Phases: `int16_t phase_x100` (radians × 100)
  - Z-scores: `int16_t novelty_z_x100` (z-score × 100)
- Top-3 resonator candidates, tactus state, beat clock (PLL) state
- Flag bits: beat_tick, tempo_changed, lock_changed

**K1DebugRing (`K1DebugRing.h`):**
- Lock-free SPSC (Single Producer, Single Consumer) ring buffer
- 32 samples × 64 bytes = 2KB memory
- Non-blocking push/pop (drops samples if full)
- Producer: AudioActor (Core 0) at 62.5Hz
- Consumer: RendererActor/WebSocket (Core 1) on demand

**K1DebugMacros (`K1DebugMacros.h`):**
- Zero-overhead instrumentation macros (expand to nothing when disabled)
- `K1_DEBUG_DECL()`: Declare local sample variable
- `K1_DEBUG_CAPTURE_*`: Capture pipeline state at each stage
- `K1_DEBUG_END()`: Push sample to ring buffer
- Conditional compilation via `FEATURE_K1_DEBUG`

**Integration:**
- `K1Pipeline`: Instrumented with debug macros in `processNovelty()`
- `AudioActor`: Holds `K1DebugRing` instance, connects to pipeline
- `K1BeatClock`: Exposes `phaseError()` and `freqError()` for debug

**Commit 6: CLI Interface**

**K1DebugCli (`K1DebugCli.h/cpp`):**
- Tab5.DSP-compatible output format (matches reference implementation)
- Commands:
  - `k1`: Full diagnostic (BPM, confidence, phase, lock, top-3 candidates)
  - `k1s`: Stats summary (all stages, internal metrics)
  - `k1spec`: ASCII resonator spectrum (121 bins, every 5 BPM)
  - `k1nov`: Recent novelty z-scores with visual bars
  - `k1reset`: Reset pipeline state
  - `k1c`: Compact output (for continuous monitoring)

**CLI Integration (`main.cpp`):**
- Command handler in `loop()` function
- Accesses K1Pipeline via AudioActor
- Gated by `FEATURE_K1_DEBUG` (defaults to `FEATURE_AUDIO_SYNC`)

## Technical Considerations

**Performance:**
- Debug capture: ~50-100 CPU cycles per sample (fixed-point conversions)
- Ring buffer push: Lock-free, non-blocking (no audio impact)
- CLI output: Only when command issued (no continuous overhead)

**Memory:**
- Debug ring: 2KB (32 samples × 64 bytes)
- Debug macros: Zero bytes when disabled (compile-time elimination)
- CLI functions: ~1KB code size (only included when `FEATURE_K1_DEBUG` enabled)

**Cross-Core Safety:**
- Lock-free SPSC queue (no mutexes, no blocking)
- Cache-aligned samples (64-byte alignment prevents false sharing)
- Producer (Core 0) never blocks (drops samples if ring full)
- Consumer (Core 1) drains on demand (non-blocking pop)

**Data Encoding:**
- Fixed-point prevents floating-point serialization overhead
- Compact format enables high-frequency capture (62.5Hz)
- Conversion helpers in `debug_conv` namespace for readability

## Dependencies

**Commit 5 depends on:**
- Commit 1 (unified logging - for error messages)
- `utils/LockFreeQueue.h` (existing infrastructure)

**Commit 6 depends on:**
- Commit 5 (debug infrastructure)
- Commit 1 (unified logging)

**These commits enable:**
- Real-time beat tracker debugging
- WebSocket telemetry (future: stream debug samples to dashboard)
- Performance analysis (BPM accuracy, lock stability)

## Testing Methodology

**Build Verification:**
```bash
# Should compile with FEATURE_K1_DEBUG enabled (defaults to FEATURE_AUDIO_SYNC)
pio run -e esp32dev_audio_esv11

# Verify debug infrastructure is included
grep -r "FEATURE_K1_DEBUG" v2/src/
```

**Runtime Verification:**
1. Flash firmware with audio enabled
2. Play music and wait for beat lock
3. Test CLI commands:
   ```
   k1      # Show current BPM, confidence, phase, lock state
   k1s     # Full stats (all 4 stages)
   k1spec  # ASCII spectrum (should show peaks at beat frequencies)
   k1nov   # Novelty history (should show spikes on beats)
   k1reset # Reset pipeline (should unlock and re-lock)
   k1c     # Compact output (for continuous monitoring)
   ```
4. Verify output matches Tab5.DSP format
5. Verify ring buffer doesn't overflow (check for dropped samples)

**Expected Output:**
```
BPM: 138.2 | Conf: 0.85 | Phase: 0.42 | LOCKED | Top3: 138(0.92) 69(0.45) 276(0.32)

=== K1 Resonator Spectrum ===
 60 BPM: ####
 65 BPM: #####
 ...
138 BPM: ######################################## (peak)
 ...
```

## Known Limitations

1. **Ring buffer size:** 32 samples = 3.2 seconds at 10Hz consumption. High-frequency consumers may miss samples. Future: Configurable size or multiple rings.

2. **No WebSocket streaming:** CLI-only. Future: Stream debug samples to dashboard via WebSocket.

3. **Fixed-point precision:** BPM precision is 0.1 BPM (bpm_x10). Phase precision is 0.01 rad (phase_x100). Sufficient for debugging but not for high-precision analysis.

4. **No history persistence:** Ring buffer is circular (oldest samples overwritten). Future: Optional linear buffer for full history capture.

5. **Single consumer:** SPSC queue supports one consumer. Multiple consumers require separate rings or shared memory.

## Migration Notes

**Breaking Changes:**
- None (additive feature, gated by `FEATURE_K1_DEBUG`)

**Feature Flag:**
- `FEATURE_K1_DEBUG` defaults to `FEATURE_AUDIO_SYNC`
- Automatically enabled when audio sync is enabled
- Can be disabled for minimal builds

**Code Changes:**
- `K1Pipeline.cpp`: Added debug macro instrumentation
- `AudioActor.h/cpp`: Added `K1DebugRing` member and initialization
- `K1BeatClock.h`: Added `phaseError()` and `freqError()` accessors
- `main.cpp`: Added `k1` command handler

**Future Enhancements:**
- WebSocket streaming of debug samples
- Configurable ring buffer size
- Multiple debug rings for different consumers
- History persistence for offline analysis

## Code References

**Commit 5:**
- Metrics: `v2/src/audio/k1/K1DebugMetrics.h`
- Ring buffer: `v2/src/audio/k1/K1DebugRing.h`
- Macros: `v2/src/audio/k1/K1DebugMacros.h`
- Integration: `v2/src/audio/k1/K1Pipeline.h/cpp`, `v2/src/audio/AudioActor.h/cpp`, `v2/src/audio/k1/K1BeatClock.h`

**Commit 6:**
- CLI: `v2/src/audio/k1/K1DebugCli.h/cpp`
- Integration: `v2/src/main.cpp` (lines 917-940)

