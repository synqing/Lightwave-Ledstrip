# FFT Redesign Phase 1 Implementation Roadmap
## Integration, Validation & Hardware Profiling

**Prepared**: 2026-01-09
**Phase 1 Duration**: 3-5 days (estimated)
**Target Completion**: End of this week
**Success Criteria**: Hardware profiling complete + baseline validation

---

## Phase 1 Objectives (Priority-Ordered)

### PRIMARY (Must Complete)
1. **Integrate K1FFTAnalyzer into K1AudioFrontEnd**
   - Replace Goertzel banks with FFT processing
   - Verify AudioFeatureFrame output format unchanged
   - Compile & link on ESP32-S3 target

2. **Hardware Profiling on ESP32-S3**
   - Measure actual KissFFT performance (cycles/wall-clock time)
   - Profile memory allocation & usage
   - Verify CPU stays under 2ms per hop budget
   - Generate baseline metrics for future optimization

3. **Baseline Unit Test Validation**
   - Run full test suite on native (desktop) target
   - Verify all 25+ tests pass
   - Run on ESP32-S3 hardware if possible

### SECONDARY (Should Complete)
4. **Frequency Mapping Validation**
   - Verify mag_rhythm[24] energy matches Goertzel version
   - Verify mag_harmony[64] energy matches Goertzel version
   - Document any discrepancies

5. **Real Audio Signal Testing**
   - Test with sine wave test signals (key frequencies)
   - Test with pink noise (spectral flatness)
   - Test with sample music (EDM, hip-hop)

### TERTIARY (Nice-to-Have)
6. **TempoTracker Interface Verification**
   - Confirm novelty_flux output format
   - Verify beat detection still works end-to-end
   - Compare beat tick timing vs Goertzel version

---

## Phase 1 Work Breakdown

### Task 1.1: K1AudioFrontEnd Integration (Estimated: 2-3 hours)

**Objective**: Replace Goertzel banks with K1FFTAnalyzer

**Current State** (Phase 0):
- K1FFTAnalyzer: ✅ Fully implemented
- SpectralFlux: ✅ Fully implemented
- FrequencyBandExtractor: ✅ Fully implemented
- K1AudioFrontEnd: ❌ Still uses Goertzel banks

**Integration Steps**:

1. **Header inclusion** (5 min)
   ```cpp
   // K1AudioFrontEnd.h
   #include "K1FFTAnalyzer.h"
   #include "SpectralFlux.h"
   #include "FrequencyBandExtractor.h"
   ```

2. **Member variable replacement** (10 min)
   ```cpp
   // Before (Goertzel-based)
   GoertzelBank m_rhythmBank;
   GoertzelBank m_harmonyBank;

   // After (FFT-based)
   K1FFTAnalyzer m_fftAnalyzer;
   SpectralFlux m_spectralFlux;
   ```

3. **Initialization logic** (30 min)
   ```cpp
   bool K1AudioFrontEnd::init() {
       // Goertzel init: m_rhythmBank.init(); m_harmonyBank.init();
       // FFT init:
       if (!m_fftAnalyzer.init()) return false;
       // SpectralFlux init: (no explicit init needed)
       return true;
   }
   ```

4. **Processing pipeline** (60-90 min)
   ```cpp
   AudioFeatureFrame K1AudioFrontEnd::processHop(const AudioChunk& chunk, bool is_clipping) {
       // 1. Buffer audio chunk
       m_ringBuffer.write(chunk);

       // 2. When 512 samples available:
       if (ringBuffer.availableSamples() >= 512) {
           float inputFrame[512];
           m_ringBuffer.read(inputFrame, 512);

           // 3. FFT processing
           if (!m_fftAnalyzer.processFrame(inputFrame)) {
               return AudioFeatureFrame();  // Error
           }

           // 4. Spectral flux
           float flux = m_spectralFlux.process(m_fftAnalyzer.getMagnitude());

           // 5. Band extraction
           const float* magnitude = m_fftAnalyzer.getMagnitude();
           FrequencyBandExtractor::mapToRhythmArray(magnitude, m_rhythmMags);
           FrequencyBandExtractor::mapToHarmonyArray(magnitude, m_harmonyMags);

           // 6. Fill AudioFeatureFrame
           m_currentFrame.novelty_flux = flux;
           m_currentFrame.rhythm_energy = FrequencyBandExtractor::getRhythmEnergy(magnitude);
           m_currentFrame.harmony_energy = FrequencyBandExtractor::getMidEnergy(magnitude);
           memcpy(m_currentFrame.mag_rhythm, m_rhythmMags, sizeof(m_rhythmMags));
           memcpy(m_currentFrame.mag_harmony, m_harmonyMags, sizeof(m_harmonyMags));
       }

       return m_currentFrame;
   }
   ```

**Testing**:
- [ ] Compiles without errors
- [ ] Compiles without warnings (check Werror flag)
- [ ] Links successfully
- [ ] Initialization succeeds (call init() and check return value)

**Verification Checklist**:
- [ ] AudioFeatureFrame structure matches old interface
- [ ] Frame rate unchanged (still ~125 Hz = 8 Hz / hop)
- [ ] No compilation errors in unit tests

---

### Task 1.2: Hardware Profiling (Estimated: 2-3 hours)

**Objective**: Measure CPU time for FFT processing on actual ESP32-S3 hardware

**Current Estimates** (theoretical):
```
Hann windowing:        ~0.05 ms
KissFFT 512-point:     ~0.50 ms
Magnitude extraction:  ~0.10 ms
Spectral flux:         ~0.10 ms
Band extraction:       ~0.02 ms
─────────────────────────────────
Total:                 ~0.77 ms per 32ms buffer (amortized 0.19 ms per 8ms hop)
Budget remaining:      ~2.0 - 0.19 = 1.81 ms/hop ✅
```

**Hardware Profiling Setup**:

1. **Instrumentation (ESP-IDF)** (30-45 min)
   ```cpp
   // K1AudioFrontEnd::processHop()
   #include "esp_timer.h"

   uint64_t t0 = esp_timer_get_time();  // Start in microseconds

   // FFT processing
   m_fftAnalyzer.processFrame(inputFrame);
   uint64_t t1 = esp_timer_get_time();

   // Spectral flux
   m_spectralFlux.process(m_fftAnalyzer.getMagnitude());
   uint64_t t2 = esp_timer_get_time();

   // Logging
   printf("FFT: %.3f ms, SpectralFlux: %.3f ms\n",
          (t1-t0)/1000.0f, (t2-t1)/1000.0f);
   ```

2. **Test Execution** (30 min)
   - Run on hardware with various audio inputs
   - Silence (baseline)
   - Pink noise (typical case)
   - Music (full complexity)
   - Collect 100+ samples for statistics

3. **Data Collection** (15-30 min)
   ```
   Expected output:
   FFT: 0.48 ms, SpectralFlux: 0.12 ms  [silence]
   FFT: 0.49 ms, SpectralFlux: 0.12 ms  [pink noise]
   FFT: 0.50 ms, SpectralFlux: 0.13 ms  [music]

   Min: 0.48 ms
   Max: 0.51 ms
   Mean: 0.49 ± 0.01 ms
   Confidence: ✅ Well under 2.0 ms budget
   ```

4. **Report Generation** (15 min)
   - Create PERFORMANCE_PROFILE.txt with raw data
   - Calculate mean, stddev, percentiles
   - Verify budget compliance

**Deliverable**: PERFORMANCE_PROFILE_HARDWARE.md with:
- [ ] Mean CPU time per operation
- [ ] Min/max outliers
- [ ] Budget verification
- [ ] Thermal/power observations

---

### Task 1.3: Unit Test Validation (Estimated: 1 hour)

**Objective**: Run full test suite to verify Phase 0 implementation quality

**Current Tests** (all in test_k1_fft_analyzer.cpp):
- ✅ K1FFTAnalyzer initialization
- ✅ Zero signal handling
- ✅ Sine wave detection
- ✅ Magnitude range validation
- ✅ Individual bin access
- ✅ Range summation
- ✅ Hann window properties
- ✅ SpectralFlux onset detection
- ✅ Flux history management
- ✅ Frequency band extraction

**Test Execution** (30 min):

```bash
# Native (desktop) target
platformio run -e native_test -t test

# Expected output:
# test_k1fft_analyzer_init PASSED
# test_k1fft_analyzer_process_zero_signal PASSED
# ... (25+ tests)
# 25/25 tests PASSED
```

**Verify on Hardware** (15-30 min):
```bash
# If possible, also run on ESP32-S3 target
platformio upload -e esp32s3_audio
# (Run tests via serial monitor or build system)
```

**Success Criteria**:
- [ ] All 25+ tests pass on native target
- [ ] All tests pass on ESP32-S3 (if hardware available)
- [ ] No compiler warnings (treat warnings as errors)
- [ ] Test execution time < 10 seconds (desktop)

---

### Task 1.4: Frequency Mapping Validation (Estimated: 2-3 hours)

**Objective**: Verify new FFT band mapping produces energy equivalent to Goertzel version

**Validation Approach**:

1. **Create test signals** (30 min)
   ```cpp
   // Test Signal 1: 100 Hz sine (in rhythm band)
   float testSignal[512];
   for (int i = 0; i < 512; i++) {
       testSignal[i] = 0.5f * sin(2*M_PI * 100 * i / 16000);
   }

   // Test Signal 2: 500 Hz sine (in rhythm band)
   for (int i = 0; i < 512; i++) {
       testSignal[i] = 0.5f * sin(2*M_PI * 500 * i / 16000);
   }

   // Test Signal 3: 1000 Hz sine (in harmony band)
   for (int i = 0; i < 512; i++) {
       testSignal[i] = 0.5f * sin(2*M_PI * 1000 * i / 16000);
   }

   // Test Signal 4: White noise (broadband)
   for (int i = 0; i < 512; i++) {
       testSignal[i] = randomFloat(-1, 1);
   }
   ```

2. **Process through both pipelines** (60 min)
   ```cpp
   // OLD: Goertzel-based
   GoertzelBank rhythmBank, harmonyBank;
   rhythmBank.init();
   harmonyBank.init();
   float goertzelRhythm[24], goertzelHarmony[64];
   // (process testSignal through Goertzel banks)

   // NEW: FFT-based
   K1FFTAnalyzer fftAnalyzer;
   fftAnalyzer.init();
   fftAnalyzer.processFrame(testSignal);
   float fftRhythm[24], fftHarmony[64];
   FrequencyBandExtractor::mapToRhythmArray(fftAnalyzer.getMagnitude(), fftRhythm);
   FrequencyBandExtractor::mapToHarmonyArray(fftAnalyzer.getMagnitude(), fftHarmony);
   ```

3. **Compare results** (30 min)
   ```cpp
   float rhythmError = 0.0f, harmonyError = 0.0f;
   for (int i = 0; i < 24; i++) {
       float err = fabs(fftRhythm[i] - goertzelRhythm[i]) / (goertzelRhythm[i] + 1e-6f);
       rhythmError += err;
   }
   rhythmError /= 24;

   for (int i = 0; i < 64; i++) {
       float err = fabs(fftHarmony[i] - goertzelHarmony[i]) / (goertzelHarmony[i] + 1e-6f);
       harmonyError += err;
   }
   harmonyError /= 64;

   printf("Rhythm energy error: %.1f%%\n", rhythmError * 100);
   printf("Harmony energy error: %.1f%%\n", harmonyError * 100);
   ```

4. **Generate report** (30 min)
   - Expected results:
     ```
     Test Signal | Frequency | FFT Rhythm | Goertzel Rhythm | Error
     ─────────────────────────────────────────────────────────────
     Sine 100Hz  | 100 Hz    | 2.34       | 2.31            | 1.3%
     Sine 500Hz  | 500 Hz    | 4.56       | 4.52            | 0.9%
     Sine 1000Hz | 1000 Hz   | 5.23       | 5.19            | 0.8%
     White noise | Broad     | 18.5       | 18.2            | 1.6%
     ```

**Success Criteria**:
- [ ] Rhythm energy error < 3% across test signals
- [ ] Harmony energy error < 3% across test signals
- [ ] No discrepancies in frequency response shape
- [ ] Document any deviations

---

### Task 1.5: Real Audio Testing (Estimated: 1-2 hours)

**Objective**: Validate FFT processing with actual audio samples

**Test Suite**:

1. **Silence** (10 min)
   - 5 seconds of silence (all zeros)
   - Verify FFT magnitude near zero
   - Verify spectral flux near zero
   - Verify no false onsets

2. **Tone Sweep** (15 min)
   - Generate sweep 60-600 Hz (rhythm band)
   - Verify all frequencies detected with adequate magnitude
   - Check for spectral leakage artifacts

3. **Pink Noise** (10 min)
   - 5 seconds of pink noise (1/f spectrum)
   - Verify flat magnitude spectrum (within 3dB per octave)
   - Verify stable spectral flux (no spurious onsets)

4. **Sample Music** (15-30 min)
   - 30 seconds each:
     - EDM (120 BPM, clear kick drums)
     - Hip-hop (95 BPM, complex percussion)
     - Acoustic (80 BPM, sparse beats)
   - Verify:
     - Spectral flux correlates with beat onsets
     - Adaptive threshold reasonable
     - No constant false positives

**Test Execution**:
```bash
# If audio samples available, process through FFT and log:
platformio run -e esp32s3_audio --upload
# (Monitor serial output for magnitude & flux values)
```

**Success Criteria**:
- [ ] Silence: FFT magnitude < 0.01, flux < 0.01
- [ ] Tone sweep: All frequencies 60-600 Hz detected
- [ ] Pink noise: Magnitude stable across frequency
- [ ] Music: Flux spikes correlate with beat onsets

---

## Phase 1 Timeline

```
Day 1 (Today):
  ├─ 2 hours: Integration work (K1AudioFrontEnd)
  ├─ 30 min: Unit test validation
  └─ 1 hour: Documentation update

Day 2:
  ├─ 2-3 hours: Hardware profiling setup
  ├─ 1 hour: Test signal generation
  └─ 1 hour: Initial profiling run

Day 3:
  ├─ 1-2 hours: Frequency mapping validation
  ├─ 1-2 hours: Real audio testing
  └─ 1 hour: Report generation

Day 4-5 (Contingency):
  ├─ Troubleshooting any issues
  ├─ Additional profiling if needed
  └─ Final documentation
```

**Estimated Total**: 15-20 hours of work spread over 3-5 days

---

## Success Criteria for Phase 1 Completion

### MUST HAVE (Go/No-Go)
- [ ] K1AudioFrontEnd successfully integrated
- [ ] Compiles without errors on ESP32-S3 target
- [ ] All unit tests pass (25+)
- [ ] Hardware profiling shows CPU < 2ms per hop budget
- [ ] AudioFeatureFrame format unchanged
- [ ] No memory regression

### SHOULD HAVE (Strong Confidence)
- [ ] Frequency mapping error < 3% (rhythm & harmony)
- [ ] Real audio testing validates FFT behavior
- [ ] Spectral flux correlates with beats
- [ ] Comprehensive profiling report generated

### NICE-TO-HAVE (Validation)
- [ ] TempoTracker integration verified end-to-end
- [ ] Beat detection latency measured
- [ ] Multi-genre audio tested
- [ ] Performance headroom quantified for future optimization

---

## Risk Mitigations During Phase 1

### Risk: Integration breaks existing functionality
**Mitigation**:
- Keep Goertzel banks in codebase (disabled, not deleted)
- Use feature flag to switch between implementations
- Revert quickly if issues found

### Risk: Hardware profiling exceeds budget
**Mitigation**:
- Pre-calculate theoretical worst-case
- Have fallback plan (Phase 7: 1024-point FFT already designed)
- Measure with minimal debug overhead

### Risk: Frequency mapping doesn't match
**Mitigation**:
- Use side-by-side comparison test
- Investigate discrepancies (likely minor precision/windowing)
- Document as Phase 2 follow-up if < 5% error

### Risk: Real audio testing shows unexpected behavior
**Mitigation**:
- Use known test signals first (sine waves, noise)
- Compare FFT output directly with known good FFT library
- Document issues for Phase 2 investigation

---

## Phase 1 Deliverables

**Code**:
- [ ] Modified K1AudioFrontEnd.h/.cpp (FFT-integrated)
- [ ] All Phase 0 components (K1FFTAnalyzer, SpectralFlux, FrequencyBandExtractor)
- [ ] Hardware profiling instrumentation code

**Documentation**:
- [ ] FFT_REDESIGN_PHASE1_COMPLETION.md (summary)
- [ ] PERFORMANCE_PROFILE_HARDWARE.md (profiling results)
- [ ] FREQUENCY_MAPPING_VALIDATION.md (band energy comparison)
- [ ] AUDIO_TEST_RESULTS.md (real audio validation)

**Test Results**:
- [ ] Unit test summary (25+ tests passed)
- [ ] Performance metrics (CPU, memory)
- [ ] Frequency response plots (if tools available)

---

## Next Phase (Phase 2) Dependency

Phase 2 (Validation & Spectral Analysis) can **only proceed** if Phase 1 delivers:
1. ✅ Working FFT integration
2. ✅ Hardware profiling confirming < 2ms budget
3. ✅ Baseline unit test pass rate > 95%

---

## Sign-Off

**Phase 1 Ready**: ✅ Yes, all prerequisites met (Phase 0 complete)

**Estimated Completion**: End of week (5 days from start)

**Go/No-Go Decision Point**: End of Day 2 (hardware profiling results)
- If CPU < 2ms: **PROCEED** to Phase 2
- If CPU > 2.5ms: **ESCALATE** to Phase 7 (1024-point FFT design)
- If critical issues: **PAUSE** for root cause analysis

---

**Prepared By**: Claude Code (comprehensive FFT redesign analysis)
**Date**: 2026-01-09 02:40 UTC
**Document Version**: 1.0 (Final)
