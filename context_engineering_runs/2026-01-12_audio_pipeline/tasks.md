# Task List: LightwaveOS Audio Pipeline Redesign

> **⚠️ CRITICAL:** This is a Context Engineer-formatted synthesis. **COMPREHENSIVE 6-PHASE TASK BREAKDOWN (612 lines) exists at `planning/audio-pipeline-redesign/tasks.md`**. Read that document for full implementation details.

> **⚠️ STOP:** Complete **`planning/audio-pipeline-redesign/AGENT_ONBOARDING.md`** checklist FIRST. No exceptions.

> **📋 THEN:** Read **`planning/audio-pipeline-redesign/HARDENED_SPEC.md`** for verbatim code and **`planning/audio-pipeline-redesign/MAGIC_NUMBERS.md`** for constant derivations.

---

## Quick Reference (Extracted from PRD & Blueprint)

### Ownership Rules
*Source: PRD §7 + Blueprint §2.1*

| Artifact | Created By | App's Role | DO NOT |
|----------|------------|------------|--------|
| Raw audio samples | I2S DMA hardware | Observe (read from ring buffer) | Generate synthetic samples |
| DC offset correction | Audio pipeline | Create (before windowing) | Skip this step |
| Goertzel bin magnitudes | GoertzelDFT module | Create (FROM SENSORY BRIDGE) | Use FFT or agent-invented bins |
| Beat/tempo output | PLLTracker module | Create (FROM EMOTISCOPE) | Invent tempo algorithms |
| ControlBusFrame | ControlBus state machine | Create (aggregation) | Expose internal state |
| SnapshotBuffer update | Audio pipeline | Create (lock-free publish) | Modify from Core 1 |
| Signal quality metrics | Audio pipeline | Create (observability) | Silently ignore or invent thresholds (EXP-1 to EXP-6) |

### State Variables
*Source: PRD §8 + Blueprint §3*

| State Variable | Initial Value | Created When | Cleared When | Persists Across |
|----------------|---------------|--------------|--------------|------------------|
| goertzel_coeffs | Sensory Bridge values | AudioNode::onStart() | Never (const) | Forever |
| pll_state | Emotiscope defaults (BPM=120, phase=0) | PLLTracker::init() | resetDspState() | Audio session |
| controlbus_smooth | Zero | ControlBus::Reset() | ControlBus::Reset() | Audio session |
| ring_buffer | Zero | First hop | Overwritten continuously | Rolling window |
| dc_offset_calib | Default value | AudioNode::calibrateDCOffset() | Never (persists) | Power cycle |
| signal_quality_history | Zero | First hop | resetDspState() | Audio session |

### Critical Boundaries
*Source: Blueprint §2.3*

- ❌ **DO NOT:** Generate synthetic audio samples (I2S DMA is sole source - O-1)
- ❌ **DO NOT:** Skip DC offset correction (SPH0645 has fixed bias - O-2)
- ❌ **DO NOT:** Use FFT for main DSP feed (Goertzel only - FR-6)
- ❌ **DO NOT:** Invent tempo algorithms (Emotiscope 2.0 verbatim only - FR-5)
- ❌ **DO NOT:** Modify lines marked "DO NOT MODIFY" (FR-12)
- ❌ **DO NOT:** Allow Core 1 to write to audio state (SnapshotBuffer is read-only - C-7)
- ❌ **DO NOT:** Expose raw Goertzel bins to visual pipeline (contract layer boundary - O-4)
- ❌ **DO NOT:** Invent signal quality thresholds (domain expertise required - EXP-1 to EXP-6)
- ✅ **MUST:** Use 16kHz sample rate, 128-sample chunks, 8ms window (FR-3, C-1, C-12)
- ✅ **MUST:** Use semitone-spaced bins (not arbitrary FFT bins - FR-2)
- ✅ **MUST:** Mark all sacred code with source annotations (FR-12)
- ✅ **MUST:** Run verification tests after any change (FR-10, M-5)
- ✅ **MUST:** Complete processing within 14ms @ 240MHz (NFR-6, C-2)
- ✅ **MUST:** Stay within ~20KB RAM budget (C-3, M-8)

### User Visibility Rules
*Source: PRD §6*

| Consumer Action | Consumer Sees | Consumer Does NOT See | Timing |
|-----------------|---------------|----------------------|--------|
| Read ControlBusFrame | Smoothed frequency bands (0.0-1.0) | Raw Goertzel magnitudes, windowing internals | Every 16ms hop |
| Read tempo output | BPM (60-300), phase (0.0-1.0), confidence | PLL internal state, onset buffer | Every hop |
| Read beat tick | Boolean beat_tick flag | Onset detection internals, threshold calculations | On beat |
| Read serial telemetry | SNR, SPL, DC offset, clipping flags | Raw I2S DMA buffer contents | On demand |
| Run verification tests | PASS/FAIL + deviation metrics | Internal DSP intermediate buffers | Per test run |

---

## Requirements Traceability

**Every requirement MUST map to at least one task. Nothing should be lost.**

> **NOTE:** Full traceability table (100+ mappings) at `planning/audio-pipeline-redesign/tasks.md` §Requirements Traceability. Below are high-level summaries.

| PRD Section | Summary | Mapped To Phases |
|-------------|---------|------------------|
| §5.1 Musically-Aligned Bins | Goertzel bins semitone-spaced, match Sensory Bridge | Phase 1 (Tasks 1.2, 1.3, 1.6) |
| §5.2 Proper Windowing | Hann window from GDFT.h, hardcoded LUT | Phase 1 (Tasks 1.4, 1.7) |
| §5.3 Onset Detection | Spectral flux from Emotiscope 2.0 | Phase 2 (Tasks 2.2, 2.3, 2.6) |
| §5.4 PLL Phase Tracking | Phase-locked loop from Emotiscope 2.0 | Phase 2 (Tasks 2.3, 2.4, 2.7) |
| §5.5 ControlBusFrame | Canonical contract, documented fields | Phase 4 (Tasks 3.2, 3.3, 3.4) |
| §5.6 Signal Quality | SNR, SPL, DC, clipping metrics | Phase 3 (Tasks 3.5 to 3.11) |
| §5.7 Verification Tests | Automated tests catch drift | Phase 6 (Tasks 6.1 to 6.8) |
| §5.8 Expert Validation | EXP-1 to EXP-6 criteria | Phase 6 (Task 6.9 - FLAGGED FOR EXPERT) |
| §6 User Experience | Visibility, timing expectations | All phases (User Sees fields) |
| §7 Artifact Ownership | Creation responsibility, external deps | Quick Reference + Ownership fields |
| §8 State Requirements | Isolation, lifecycle | Quick Reference + State Change fields |
| §9 Technical Considerations | Architecture decisions, constraints | All phases (timing/memory budgets) |
| §10 Success Metrics | M-1 to M-12 | Phase 6 (Verification tasks) |

| Blueprint Section | Summary | Mapped To Sections |
|-------------------|---------|-------------------|
| §2 System Boundaries | Ownership, external systems, DO NOTs | Quick Reference + IC tasks |
| §3 State Transitions | Pre/Post conditions, side effects | Parent task Pre/Post conditions |
| §4 Integration Wiring | Call sequences, critical order | Integration-Critical Tasks (IC-1, IC-2) |
| §5 System Components | Core audio components, data structures | Relevant Files + all phases |
| §9 Technical Risks | Timing budget, agent drift, expert gaps | Notes + Phase 6 validation |
| §10 Testing Strategy | Automated tests, domain expert validation | Phase 6 (Tasks 6.1 to 6.9) |

---

## Overview

This task list implements the **LightwaveOS Audio Pipeline Redesign** specified in:
- **PRD:** `planning/audio-pipeline-redesign/prd.md` (429 lines, §1-§11)
- **Blueprint:** `planning/audio-pipeline-redesign/technical_blueprint.md` (918 lines, §1-§11)
- **This document (synthesis):** Context Engineer-formatted summary with 1:1 requirement mapping

**Key Outcomes:**
- Replace corrupted audio pipeline with 100% canonical implementations (Sensory Bridge 4.1.1 Goertzel, Emotiscope 2.0 beat tracking)
- Remove accidental FFT contamination (K1 path)
- Expose full signal quality visibility (SNR, SPL, DC offset, clipping) to unblock commercial deployment
- Implement 4-layer specification (Algorithm → Pseudocode → Reference Code → Tests) to prevent future agent drift
- Enable CI/CD with automated verification tests
- Deliver mission-critical commercial-grade audio foundation

**Parallel Multi-Agent Strategy (User Q5):**
- **Agent 1:** Goertzel implementation (Phase 1) → Cleanup (Phase 5)
- **Agent 2:** Beat tracking implementation (Phase 2)
- **Agent 3:** Contract layer redesign (Phase 4)
- **Agent 4:** Signal quality instrumentation (Phase 3)
- **Agent 5:** Verification tests (Phase 6)
- **Agent 6:** Documentation (4-layer spec, continuous)

**Synchronization Points:**
- Phase 2 depends on Phase 1 (Goertzel bins feed onset detector)
- Phase 3 depends on Phase 1, 2 (signal quality needs both pipelines)
- Phase 4 depends on Phase 1, 2, 3 (contract aggregates all sources)
- Phase 5 depends on Phase 1, 2, 4 (cleanup after canonical replacements ready)
- Phase 6 depends on Phase 1, 2, 3, 4, 5 (verify complete implementation)

---

## Relevant Files

> **NOTE:** Full file mapping at `planning/audio-pipeline-redesign/technical_blueprint.md` §5, §9.3. Below are primary implementation targets.

**Implementation Targets:**
- `firmware/v2/src/audio/AudioNode.cpp` – Main audio coordinator (rewrite processHop())
- `firmware/v2/src/audio/AudioCapture.cpp` – I2S DMA interface (update to 16kHz/128-sample)
- `firmware/v2/src/audio/AudioRingBuffer.cpp` – 512-sample rolling window
- `firmware/v2/src/audio/WindowBank.cpp` – NEW: Hann window LUT (from Sensory Bridge)
- `firmware/v2/src/audio/GoertzelDFT.cpp` – NEW: Canonical Goertzel (replaces GoertzelAnalyzer)
- `firmware/v2/src/audio/OnsetDetector.cpp` – NEW: Spectral flux (from Emotiscope)
- `firmware/v2/src/audio/tempo/PLLTracker.cpp` – NEW: Phase-locked loop (replaces TempoTracker)
- `firmware/v2/src/audio/SignalQuality.cpp` – NEW: SNR, SPL, DC, clipping metrics
- `firmware/v2/src/audio/contracts/ControlBus.h|cpp` – Update to canonical contract
- `firmware/v2/src/audio/SnapshotBuffer.h|cpp` – Existing lock-free cross-core (no changes)

**Removal Targets:**
- `firmware/v2/src/audio/k1/` – **DELETE entire K1 FFT path**
- `firmware/v2/src/audio/K1_GoertzelTables_16k.h` – **DELETE**
- `firmware/v2/src/audio/GoertzelAnalyzer.cpp` – **REPLACE with canonical GoertzelDFT**
- `firmware/v2/src/audio/ChromaAnalyzer.cpp` – **DELETE (superseded by semitone bins)**
- `firmware/v2/src/audio/tempo/TempoTracker.cpp` – **REPLACE with canonical PLLTracker**

**Test Files:**
- `firmware/v2/test/audio/test_goertzel_match.cpp` – NEW: Verify bin frequencies (M-1)
- `firmware/v2/test/audio/test_windowing_match.cpp` – NEW: Verify Hann window (M-2)
- `firmware/v2/test/audio/test_beat_tracking_match.cpp` – NEW: Verify Emotiscope implementation (M-3, M-4)
- `firmware/v2/test/audio/test_timing_budget.cpp` – NEW: Verify <14ms processing (M-7)
- `firmware/v2/test/audio/test_memory_budget.cpp` – NEW: Verify ~20KB RAM (M-8)
- `firmware/v2/test/audio/test_signal_quality.cpp` – NEW: Verify metrics exposure (M-9)

**Documentation:**
- `planning/audio-pipeline-redesign/HARDENED_SPEC.md` – Verbatim canonical code extracts (1066 lines)
- `planning/audio-pipeline-redesign/MAGIC_NUMBERS.md` – Constant derivations
- `planning/audio-pipeline-redesign/AGENT_ONBOARDING.md` – Mandatory checklist

---

## Tasks

> **⚠️ COMPREHENSIVE TASK BREAKDOWN:** Full 6-phase implementation details (112 tasks, 612 lines) at **`planning/audio-pipeline-redesign/tasks.md`**

> **Below:** Context Engineer-formatted summary highlighting parallel multi-agent strategy and critical dependencies.

---

### Phase 1: Goertzel Foundation (Agent 1)
*Implements: PRD §5.1, §5.2 (Sensory Bridge pattern)*

**Pre-condition:** 
- HARDENED_SPEC.md read (contains verbatim Sensory Bridge GDFT.h code)
- MAGIC_NUMBERS.md read (16kHz, 128-sample, semitone bin frequencies)
- AGENT_ONBOARDING.md checklist complete

#### Summary Tasks:

- [ ] 1.1 Review context: PRD §5.1, §5.2, Blueprint §5.1, HARDENED_SPEC §2, §3
- [ ] 1.2 Port Sensory Bridge Goertzel coefficients (64/96 semitone bins)
  - **Implements:** PRD §5.1.2 (FR-1, FR-2), HARDENED_SPEC §2
  - **Output:** `goertzel_coeffs[64]` const array (SL-1)
  - **Ownership:** App creates (canonical values from Sensory Bridge)
  - **State Change:** SL-1 created, never cleared
- [ ] 1.3 Verify bin frequencies are semitone-spaced
  - **Implements:** PRD §5.1.1 (FR-2)
  - **Verification:** Compare to Sensory Bridge reference (M-1)
- [ ] 1.4 Port Sensory Bridge Hann window LUT
  - **Implements:** PRD §5.2.1, §5.2.2 (FR-4), HARDENED_SPEC §3
  - **Output:** `hann_window_lut[512]` const array
  - **Ownership:** App creates (canonical values from GDFT.h)
- [ ] 1.5 Implement `GoertzelDFT::analyze()` - verbatim from HARDENED_SPEC
  - **Implements:** PRD §5.1 (FR-1), HARDENED_SPEC §2
  - **Input:** Windowed 512-sample buffer
  - **Output:** 64 Goertzel bin magnitudes (O-4)
  - **Ownership:** App creates (GoertzelDFT module)
  - **MUST:** Mark code "// FROM SENSORY BRIDGE 4.1.1 - DO NOT MODIFY" (FR-12)
- [ ] 1.6 Verification test: Goertzel bin frequency match
  - **Implements:** PRD §5.1.3, §5.1.4 (M-1)
  - **Verification:** 100% match to Sensory Bridge reference
- [ ] 1.7 Verification test: Windowing output match
  - **Implements:** PRD §5.2.3 (M-2)
  - **Verification:** Window output matches GDFT.h reference

**Post-condition:**
- Goertzel coefficients immutable (SL-1, SI-3)
- GoertzelDFT produces 64 semitone-spaced bins
- Timing: <10ms for Goertzel analysis (part of 14ms budget)
- Verification tests PASS (M-1, M-2)

**Verification:**
- [ ] Goertzel bin frequencies match Sensory Bridge (M-1)
- [ ] Hann window LUT matches GDFT.h (M-2)
- [ ] Code marked "DO NOT MODIFY" (FR-12)
- [ ] Timing <10ms on ESP32-S3 @ 240MHz

**Dependencies:** None (foundational)

---

### Phase 2: Beat Tracking (Agent 2)
*Implements: PRD §5.3, §5.4 (Emotiscope pattern)*

**Pre-condition:**
- Phase 1 complete (Goertzel bins available)
- HARDENED_SPEC.md §5 read (Emotiscope onset detection + PLL)

#### Summary Tasks:

- [ ] 2.1 Review context: PRD §5.3, §5.4, Blueprint §5.1, HARDENED_SPEC §5
- [ ] 2.2 Implement `OnsetDetector::update()` - verbatim from HARDENED_SPEC
  - **Implements:** PRD §5.3.1 (FR-5), HARDENED_SPEC §5.1
  - **Input:** Goertzel bins from Phase 1
  - **Output:** Novelty curve (spectral flux)
  - **Ownership:** App creates (OnsetDetector module)
  - **MUST:** Mark code "// FROM EMOTISCOPE 2.0 - DO NOT MODIFY" (FR-12)
- [ ] 2.3 Implement `PLLTracker::update()` - verbatim from HARDENED_SPEC
  - **Implements:** PRD §5.4.1, §5.4.2, §5.4.3 (FR-5), HARDENED_SPEC §5.2
  - **Input:** Novelty curve from OnsetDetector
  - **Output:** BPM (60-300), phase (0.0-1.0), confidence (O-6)
  - **State Change:** SL-2 (pll_state) updated every hop
  - **Ownership:** App creates (PLLTracker module)
- [ ] 2.4 Document all magic numbers with sources
  - **Implements:** PRD §5.3.3 (FR-11), HARDENED_SPEC §5.3
  - **Verification:** All constants traced to Emotiscope reference
- [ ] 2.5 Implement `resetDspState()` for beat tracker
  - **Implements:** Blueprint §3.3, SL-2
  - **State Change:** SL-2 cleared to Emotiscope defaults (BPM=120, phase=0)
- [ ] 2.6 Verification test: Onset detection with known input
  - **Implements:** PRD §5.3.4 (M-3)
  - **Verification:** Known input produces known output
- [ ] 2.7 Verification test: BPM accuracy ±1 BPM
  - **Implements:** PRD §5.4.4 (M-4)
  - **Verification:** Test with known-BPM audio samples

**Post-condition:**
- Beat tracker PLL produces BPM, phase, confidence
- Timing: <2ms for beat tracking (part of 14ms budget)
- State resets correctly via resetDspState()
- Verification tests PASS (M-3, M-4)

**Verification:**
- [ ] Onset detection matches Emotiscope 2.0 (M-3)
- [ ] BPM within ±1 BPM of ground truth (M-4)
- [ ] Code marked "DO NOT MODIFY" (FR-12)
- [ ] Timing <2ms on ESP32-S3 @ 240MHz

**Dependencies:** Phase 1 (Goertzel bins)

---

### Phase 3: Signal Quality Instrumentation (Agent 4)
*Implements: PRD §5.6 (observability layer), User Q1 visibility requirement*

**Pre-condition:**
- Phase 1 complete (Goertzel bins available)
- Phase 2 complete (Tempo output available)
- MAGIC_NUMBERS.md read (timing/memory budgets)

#### Summary Tasks:

- [ ] 3.1 Review context: PRD §5.6, §4.1 (FR-7, FR-8), §9.5 (EXP-1 to EXP-6)
- [ ] 3.2 Implement `SignalQuality::updateRaw(samples)`
  - **Implements:** PRD §5.6.1 (FR-7, O-9), User Q1
  - **Input:** Raw 128 samples from I2S DMA
  - **Output:** Min, max, mean, stddev, DC offset measurement
  - **Ownership:** App creates (SignalQuality module)
  - **State Change:** SL-6 (signal_quality_history) updated
- [ ] 3.3 Implement `SignalQuality::updateSpectral(bins)`
  - **Implements:** PRD §5.6.2 (FR-7)
  - **Input:** Goertzel bins from Phase 1
  - **Output:** SNR estimate, spectral energy distribution
  - **State Change:** SL-6 updated
- [ ] 3.4 Implement SNR estimation
  - **Implements:** PRD §5.6.4, §5.8.1 (FR-8, EXP-1)
  - **⚠️ EXPERT ADVICE NEEDED:** Validation threshold for SPH0645 @ 16kHz
  - **Document Assumptions:** Best-effort metric, flagged for expert review
- [ ] 3.5 Implement SPL range estimation
  - **Implements:** PRD §5.6 (FR-8, EXP-2)
  - **⚠️ EXPERT ADVICE NEEDED:** Valid dB SPL range for music/speech
  - **Document Assumptions:** Best-effort metric, flagged for expert review
- [ ] 3.6 Implement clipping detection
  - **Implements:** PRD §5.6.5 (FR-8, EXP-3)
  - **⚠️ EXPERT ADVICE NEEDED:** Sample amplitude threshold
  - **Document Assumptions:** Best-effort metric, flagged for expert review
- [ ] 3.7 Expose serial telemetry output
  - **Implements:** PRD §5.6.6, §6.1 V-4, §6.2 T-5
  - **User Sees:** SNR, SPL, DC offset, clipping flags via serial monitor
  - **User Does NOT See:** Raw I2S DMA buffer contents
  - **Timing:** Metrics available every hop (16ms)
- [ ] 3.8 Add `SignalQuality::getMetrics()` API
  - **Implements:** Blueprint §7.1
  - **Output:** `SignalQualityMetrics` struct
  - **Ownership:** App creates (read-only accessor)

**Post-condition:**
- Signal quality metrics exposed at all stages (mic → DSP → contract)
- Telemetry output available via serial monitor (V-4)
- Timing: <1ms for signal quality updates (part of 14ms budget)
- EXP-1 to EXP-6 documented as requiring domain expert review

**Verification:**
- [ ] SNR, SPL, DC, clipping metrics exposed (M-9)
- [ ] Serial telemetry works (`s` command shows metrics)
- [ ] Timing <1ms on ESP32-S3 @ 240MHz
- [ ] All assumptions documented for expert review (EXP-1 to EXP-6)

**Dependencies:** Phase 1 (Goertzel bins), Phase 2 (Tempo output)

---

### Phase 4: Contract Layer Redesign (Agent 3)
*Implements: PRD §5.5 (ControlBusFrame redesign)*

**Pre-condition:**
- Phase 1 complete (Goertzel bins)
- Phase 2 complete (Tempo output)
- Phase 3 complete (Signal quality metrics)

#### Summary Tasks:

- [ ] 4.1 Review context: PRD §5.5, §7.1 (O-7), §9.1 (AD-5), Blueprint §2.1
- [ ] 4.2 Define new `ControlBusFrame` struct
  - **Implements:** PRD §5.5.1, §5.5.3 (FR-9, O-7)
  - **Output:** Canonical contract struct (no legacy fields)
  - **Ownership:** ControlBus creates (aggregation)
  - **User Sees:** Visual pipeline sees smoothed bands, BPM, phase
  - **User Does NOT See:** Raw bins, PLL state, windowing internals
- [ ] 4.3 Document each field with source
  - **Implements:** PRD §5.5.2 (FR-11)
  - **Example:** `float frequency_bands[64]; // From GoertzelDFT bins, smoothed by ControlBus`
- [ ] 4.4 Implement `ControlBus::update()` with canonical inputs
  - **Implements:** PRD §5.5.1 (O-7), Blueprint §4.1
  - **Input:** Goertzel bins (Phase 1), BPM/phase (Phase 2), signal quality (Phase 3)
  - **Output:** Updated ControlBusFrame
  - **State Change:** SL-3 (controlbus_smooth) updated
  - **Ownership:** ControlBus creates (aggregation, smoothing)
- [ ] 4.5 Implement lock-free `ControlBus::publishSnapshot()`
  - **Implements:** PRD §7.1 (O-8), §9.4 (C-4), Blueprint §4.1
  - **Output:** Atomic write to SnapshotBuffer (cross-core)
  - **Ownership:** App creates (lock-free publish)
  - **User Sees:** Core 1 visual pipeline sees updated ControlBusFrame
  - **Timing:** Every hop (16ms)
- [ ] 4.6 Remove legacy/corrupted fields from contract
  - **Implements:** PRD §5.5.3 (FR-9)
  - **Verification:** No K1 FFT fields, no agent-invented fields
- [ ] 4.7 Verification test: Contract fields match canonical sources
  - **Implements:** PRD §5.5.1 (FR-9)
  - **Verification:** Every field traced to Goertzel, PLL, or SignalQuality

**Post-condition:**
- ControlBusFrame contains only canonical fields
- Lock-free snapshot publishing works (Core 0 → Core 1)
- Visual pipeline can consume new contract (backward compatible for Phase 1)
- Timing: <1ms for contract aggregation (part of 14ms budget)

**Verification:**
- [ ] ControlBusFrame has no legacy fields (FR-9)
- [ ] Each field documented with source (FR-11)
- [ ] Lock-free publish works (C-4)
- [ ] Timing <1ms on ESP32-S3 @ 240MHz

**Dependencies:** Phase 1, 2, 3 (all canonical sources ready)

---

### Phase 5: Cleanup (Agent 1)
*Implements: PRD §4.1 (FR-6), Appendix B*

**Pre-condition:**
- Phase 1 complete (canonical Goertzel replacement ready)
- Phase 2 complete (canonical beat tracking replacement ready)
- Phase 4 complete (canonical contract ready)

#### Summary Tasks:

- [ ] 5.1 Review context: PRD §4.1 (FR-6), §9.3, Appendix B
- [ ] 5.2 Remove `firmware/v2/src/audio/k1/` directory
  - **Implements:** PRD §4.1 (FR-6), Appendix B
  - **Rationale:** K1 FFT path was accidental contamination (PP-1)
  - **Verification:** FFT code absent from audio DSP feed
- [ ] 5.3 Remove `firmware/v2/src/audio/K1_GoertzelTables_16k.h`
  - **Implements:** Appendix B
  - **Rationale:** Wrong approach, superseded by Sensory Bridge coefficients
- [ ] 5.4 Remove legacy `firmware/v2/src/audio/GoertzelAnalyzer.cpp`
  - **Implements:** §9.3
  - **Rationale:** Replaced by canonical GoertzelDFT (Phase 1)
- [ ] 5.5 Remove `firmware/v2/src/audio/ChromaAnalyzer.cpp`
  - **Implements:** §9.3
  - **Rationale:** Superseded by semitone Goertzel bins
- [ ] 5.6 Replace `firmware/v2/src/audio/tempo/TempoTracker.cpp` with canonical PLLTracker
  - **Implements:** §9.3, PRD §2.1 (80% correct, 20% invented)
  - **Rationale:** Phase 2 implemented canonical Emotiscope version
- [ ] 5.7 Update all references to removed components
  - **Verification:** Build succeeds, no linker errors
- [ ] 5.8 Verification test: FFT code absent
  - **Implements:** PRD §4.1 (FR-6)
  - **Verification:** Grep for K1FFT, no matches in audio DSP path

**Post-condition:**
- K1 FFT path completely removed (PP-1, PP-3 resolved)
- Legacy Goertzel replaced with canonical (PP-4 resolved)
- Corrupted beat tracker replaced with canonical (PP-2 resolved)
- Build succeeds with only canonical components

**Verification:**
- [ ] K1 FFT code absent from codebase (FR-6)
- [ ] Build succeeds (no linker errors)
- [ ] Audio DSP path uses only canonical components

**Dependencies:** Phase 1, 2, 4 (canonical replacements in place)

---

### Phase 6: Verification & Documentation (Agent 5, Agent 6)
*Implements: PRD §5.7, §5.8, §10 (automated tests + domain expert validation)*

**Pre-condition:**
- Phase 1, 2, 3, 4, 5 complete (full implementation ready)
- All code marked "DO NOT MODIFY" where applicable

#### Summary Tasks:

- [ ] 6.1 Goertzel bin frequency match test
  - **Implements:** PRD §5.1.3, §5.1.4 (M-1)
  - **Verification:** 100% match to Sensory Bridge reference
  - **Input:** Goertzel coefficients from Phase 1
  - **Output:** PASS/FAIL + deviation metrics
- [ ] 6.2 Goertzel windowing match test
  - **Implements:** PRD §5.2.3 (M-2)
  - **Verification:** Window output matches GDFT.h reference
  - **Input:** Hann window LUT from Phase 1
  - **Output:** PASS/FAIL + deviation metrics
- [ ] 6.3 Beat tracking algorithm match test
  - **Implements:** PRD §5.3.4, §5.4.1 to §5.4.3 (M-3)
  - **Verification:** Line-by-line code comparison to Emotiscope 2.0
  - **Input:** OnsetDetector + PLLTracker from Phase 2
  - **Output:** PASS/FAIL + code diff
- [ ] 6.4 BPM accuracy test
  - **Implements:** PRD §5.4.4 (M-4)
  - **Verification:** ±1 BPM of ground truth on known-BPM audio
  - **Input:** Known-BPM test audio samples
  - **Output:** PASS/FAIL + BPM deviation
- [ ] 6.5 Agent drift detection test suite
  - **Implements:** PRD §4.1 (FR-8, FR-10), §4.2 (NFR-3), §10 (M-5)
  - **Verification:** All "DO NOT MODIFY" sections unchanged
  - **Input:** HARDENED_SPEC.md reference code
  - **Output:** PASS/FAIL + list of deviations
- [ ] 6.6 Timing budget compliance test
  - **Implements:** PRD §6.2 (T-2), §9.4 (C-1, C-2), §10 (M-7)
  - **Verification:** processHop() completes in <14ms @ 240MHz
  - **Input:** Worst-case audio input
  - **Output:** PASS/FAIL + timing profile (Goertzel <10ms, beat <2ms, contract <1ms)
- [ ] 6.7 Memory budget compliance test
  - **Implements:** PRD §9.4 (C-3), §10 (M-8)
  - **Verification:** Audio pipeline uses ~20KB RAM
  - **Input:** Build-time memory analysis
  - **Output:** PASS/FAIL + RAM usage breakdown
- [ ] 6.8 Signal quality visibility test
  - **Implements:** PRD §5.6.6, §10 (M-9)
  - **Verification:** SNR, SPL, DC, clipping exposed via telemetry
  - **Input:** Serial monitor output from Phase 3
  - **Output:** PASS/FAIL + metric presence check
- [ ] 6.9 **FLAG EXP-1 to EXP-6 for domain expert review**
  - **Implements:** PRD §5.8, §9.5, §10 (M-10)
  - **⚠️ CRITICAL:** Not qualified to determine bulletproof signal quality validation
  - **Document:** All assumptions for best-effort metrics (Phase 3)
  - **Request:** Expert guidance on SNR thresholds, SPL range, clipping detection, noise floor, stability, edge cases
  - **Status:** **PENDING EXPERT INPUT**
- [ ] 6.10 4-layer specification documentation audit
  - **Implements:** PRD §4.2 (NFR-1), §10 (M-6), §11
  - **Verification:** Algorithm → Pseudocode → Reference Code → Tests complete for all components
  - **Output:** PASS/FAIL + coverage report

**Post-condition:**
- All automated verification tests PASS (M-1 to M-9)
- Agent drift detection confirms zero deviations (M-5)
- Timing budget <14ms (M-7)
- Memory budget ~20KB (M-8)
- Signal quality metrics exposed (M-9)
- EXP-1 to EXP-6 flagged for domain expert review (M-10)
- 4-layer specification complete (M-6)
- **READY FOR COMMERCIAL DEPLOYMENT** (User Q4 mission-critical requirement)

**Verification:**
- [ ] All tests PASS (M-1 to M-9)
- [ ] Zero deviations from canonical references (M-5)
- [ ] Timing <14ms, memory ~20KB (M-7, M-8)
- [ ] EXP-1 to EXP-6 documented for expert review (M-10)
- [ ] 4-layer spec audit complete (M-6)

**Dependencies:** Phase 1, 2, 3, 4, 5 (complete implementation)

---

## Integration-Critical Tasks
*Source: Blueprint §4 - Integration Wiring*

> **NOTE:** Full integration sequences at `planning/audio-pipeline-redesign/technical_blueprint.md` §4. Below are Context Engineer-formatted critical paths.

These tasks have specific wiring requirements that must be followed exactly. Deviating from the specified sequence can cause bugs.

### IC-1: Audio Hop Processing Pipeline
*Maps to: Blueprint §4.1 - processHop() sequence*

**Critical Sequence:**
```
1. AudioCapture::readSamples()                          // REQUIRED: O-1 sole source of truth
2. SignalQuality::updateRaw(samples)                    // REQUIRED: FR-7 visibility before any modification
3. applyDCOffsetCorrection(samples)                     // REQUIRED: O-2 SPH0645 has fixed DC bias
4. AudioRingBuffer::write(samples)                      // REQUIRED: SL-4 state update
5. WindowBank::apply(ringBuffer)                        // REQUIRED: FR-4 Sensory Bridge windowing
6. GoertzelDFT::analyze(windowedSamples)                // REQUIRED: FR-1 canonical Goertzel
7. SignalQuality::updateSpectral(bins)                  // REQUIRED: FR-7 visibility after DSP
8. OnsetDetector::update(bins)                          // REQUIRED: FR-5 Emotiscope onset
9. PLLTracker::update(novelty)                          // REQUIRED: FR-5 Emotiscope PLL
10. ControlBus::update(bins, bpm, phase, signalQuality) // REQUIRED: O-7 contract aggregation
11. ControlBus::publishSnapshot()                       // REQUIRED: O-8 lock-free cross-core
```

**Ownership Rules (from PRD §7):**
- Raw audio samples (step 1) created by **I2S DMA** — App MUST NOT generate synthetic samples
- DC offset correction (step 3) created by **App** — App is responsible for this BEFORE windowing
- Goertzel bins (step 6) created by **App (GoertzelDFT)** — MUST use canonical Sensory Bridge algorithm
- BPM/phase (step 9) created by **App (PLLTracker)** — MUST use canonical Emotiscope algorithm
- ControlBusFrame (step 10) created by **App (ControlBus)** — Aggregates all canonical sources
- SnapshotBuffer (step 11) created by **App** — Lock-free atomic write (Core 0 → Core 1)

**User Visibility (from PRD §6):**
- **User Sees (V-4, T-5):** Signal quality metrics (SNR, SPL, DC, clipping) via serial telemetry every hop
- **User Sees (V-1, T-4):** Smoothed frequency bands (0.0-1.0) in ControlBusFrame every 16ms
- **User Sees (V-2, T-3):** BPM (60-300), phase (0.0-1.0), confidence in ControlBusFrame every hop
- **User Does NOT See:** Raw Goertzel magnitudes, windowing internals, PLL internal state, onset buffer, I2S DMA buffer

**State Changes (from Blueprint §3):**
- **Before (§3.2 Pre-condition):** Ring buffer contains previous hop samples, signal quality history outdated
- **During:** SL-4 (ring buffer) appends new samples, discards oldest; SL-6 (signal quality) updates; SL-2 (PLL) advances; SL-3 (ControlBus smoothing) updates
- **After (§3.2 Post-condition):** Ring buffer has latest 512 samples, signal quality current, ControlBusFrame published

**Common Mistakes to Avoid (from Blueprint §2.3):**
- ❌ **Skipping step 2 (SignalQuality before modification):** Cannot diagnose raw mic signal quality (User Q1 requirement)
- ❌ **Skipping step 3 (DC offset correction):** SPH0645 has fixed DC bias, corrupts DSP if not removed (O-2)
- ❌ **Using FFT instead of step 6 (Goertzel):** FFT was accidental contamination, must use canonical Sensory Bridge (FR-6)
- ❌ **Inventing tempo algorithm in step 9:** Must use canonical Emotiscope PLL (FR-5, NFR-2)
- ❌ **Exposing raw bins in step 10:** ControlBusFrame is boundary, visual pipeline sees smoothed bands only (O-4)
- ❌ **Non-atomic publish in step 11:** Core 1 may read stale frame, use lock-free SnapshotBuffer (C-4)

**Verification:**
- [ ] Sequence order followed exactly (all 11 steps in correct order)
- [ ] Timing: Total <14ms @ 240MHz (M-7) — Goertzel <10ms, beat <2ms, contract <1ms
- [ ] Memory: No heap allocation in any step (C-9)
- [ ] Signal quality exposed before and after DSP (FR-7, M-9)
- [ ] ControlBusFrame contains only canonical fields (FR-9)

---

### IC-2: State Lifecycle Management
*Maps to: Blueprint §3.1, §3.3 - Initialize and Reset*

**Critical Sequence (Initialize):**
```
1. GoertzelDFT::init()                  // REQUIRED: SL-1 coefficients from Sensory Bridge (immutable)
2. PLLTracker::init()                   // REQUIRED: SL-2 PLL state from Emotiscope defaults
3. ControlBus::Reset()                  // REQUIRED: SL-3 smoothing state to zero
4. AudioNode::calibrateDCOffset()       // REQUIRED: SL-5 DC offset measurement/default
```

**Critical Sequence (Reset DSP State):**
```
1. PLLTracker::reset()                  // REQUIRED: SL-2 to Emotiscope defaults (BPM=120, phase=0)
2. ControlBus::Reset()                  // REQUIRED: SL-3 smoothing to zero
3. SignalQuality::reset()               // REQUIRED: SL-6 signal quality history to zero
4. DO NOT clear: goertzel_coeffs        // FORBIDDEN: SL-1 is const, never cleared
5. DO NOT clear: ring_buffer            // FORBIDDEN: SL-4 is continuous audio stream
6. DO NOT clear: dc_offset_calib        // FORBIDDEN: SL-5 persists across sessions
```

**Ownership Rules (from PRD §7, §8):**
- Goertzel coefficients (SL-1) created by **App** — MUST use Sensory Bridge canonical values, immutable after init
- PLL state (SL-2) created by **App** — MUST use Emotiscope canonical defaults, reset on command
- ControlBus smoothing (SL-3) created by **App** — Temporal smoothing state, reset on command
- Ring buffer (SL-4) created by **App** — Continuous rolling window, never cleared (only overwritten)
- DC offset calibration (SL-5) created by **App** — Measured or default, persists across power cycle
- Signal quality history (SL-6) created by **App** — Rolling metrics, reset on command

**State Changes (from Blueprint §3):**
- **Initialize (§3.1):** All state variables created from canonical defaults
- **Reset (§3.3):** SL-2, SL-3, SL-6 cleared; SL-1, SL-4, SL-5 preserved
- **After Reset:** Next hop starts fresh (no temporal continuity), BPM re-locks over several seconds

**Common Mistakes to Avoid (from Blueprint §2.3):**
- ❌ **Clearing SL-1 (goertzel_coeffs) on reset:** Coefficients are const, never cleared (SI-3)
- ❌ **Clearing SL-4 (ring_buffer) on reset:** Continuous audio stream, only overwritten by new samples
- ❌ **Clearing SL-5 (dc_offset_calib) on reset:** Calibration persists across audio sessions
- ❌ **Forgetting to reset SL-3 (ControlBus smoothing):** Stale smoothing state affects next audio session
- ❌ **Using non-canonical defaults:** SL-1 MUST be Sensory Bridge, SL-2 MUST be Emotiscope (NFR-2)

**Verification:**
- [ ] Initialize: All state variables created from canonical sources (M-1, M-3)
- [ ] Reset: SL-2, SL-3, SL-6 cleared; SL-1, SL-4, SL-5 preserved
- [ ] After reset: Next hop processes correctly, no stale state bugs

---

## Validation Checklist

Before implementation, verify 1:1 mapping is complete:

### PRD Coverage
- [ ] Every §5 acceptance criterion (§5.1.1 to §5.8.7) has a corresponding subtask
  - **Verification:** See Requirements Traceability table (100+ mappings)
- [ ] Every §6 visibility rule (V-1 to V-6, T-1 to T-6) has "User Sees" in relevant subtask
  - **Verification:** IC-1 includes all V/T mappings
- [ ] Every §7 ownership rule (O-1 to O-10, E-1 to E-5) is in Quick Reference AND relevant subtask "Ownership" field
  - **Verification:** Quick Reference Ownership table + IC-1, IC-2
- [ ] Every §8 state requirement (SI-1 to SI-5, SL-1 to SL-7) has "State Change" in relevant subtask
  - **Verification:** Quick Reference State table + IC-2 + all phase State Change fields

### Blueprint Coverage
- [ ] Every §2 boundary rule is in Critical Boundaries AND enforced in tasks
  - **Verification:** Quick Reference DO NOTs + IC-1, IC-2 "Common Mistakes"
- [ ] Every §3 state transition (§3.1, §3.2, §3.3) maps to Pre/Post conditions
  - **Verification:** Phase 1-6 Pre/Post conditions + IC-2
- [ ] Every §4 integration wiring (§4.1) maps to an Integration-Critical Task
  - **Verification:** IC-1 (processHop pipeline), IC-2 (state lifecycle)

### Task Quality
- [ ] First subtask of each phase references relevant docs
  - **Verification:** Tasks 1.1, 2.1, 3.1, 4.1, 5.1, 6.1 all reference PRD/Blueprint/HARDENED_SPEC
- [ ] All subtasks are specific and actionable (not vague)
  - **Verification:** Each subtask has Input/Output/Ownership/User Sees/State Change/Implements
- [ ] All "Implements" fields trace back to PRD/Blueprint sections
  - **Verification:** Every task includes "Implements: PRD §X.Y, Blueprint §Z.W"

### Expert Advice Gaps
- [ ] All EXP-1 to EXP-6 flagged in relevant tasks
  - **Verification:** Tasks 3.4, 3.5, 3.6, 6.9 flag expert review needed
- [ ] All assumptions documented for best-effort metrics
  - **Verification:** Phase 3 tasks document assumptions for SNR, SPL, clipping

---

## Notes

### Build & Test Commands

**Build:**
```bash
cd firmware/v2
pio run -e esp32dev_audio
```

**Upload:**
```bash
cd firmware/v2
pio run -e esp32dev_audio -t upload
```

**Serial Monitor:**
```bash
pio device monitor -b 115200
```

**Verification Tests:**
```bash
cd firmware/v2
pio test -e esp32dev_audio
```

### Runtime Testing (Serial Interface)

- `s` - Status: FPS, CPU, current effect, **signal quality metrics**
- `validate <effectId>` - Run verification tests on specific effect
- `resetDspState` - Reset beat tracker + smoothing (triggers §3.3 transition)
- `calibrateDC` - Measure and store DC offset (updates SL-5)

### Standards & Best Practices

- **Canonical fidelity:** Port code line-by-line from HARDENED_SPEC.md (no agent inventions)
- **Code annotations:** Mark sacred code "// FROM [SOURCE] - DO NOT MODIFY"
- **Magic numbers:** Document every constant with source (HARDENED_SPEC or MAGIC_NUMBERS)
- **State management:** Follow lifecycle rules (Quick Reference State table)
- **Ownership boundaries:** Respect "App creates" vs "External creates" (Quick Reference Ownership table)
- **User visibility:** Distinguish "User Sees" vs "User Does NOT See" (Quick Reference Visibility table)
- **Critical boundaries:** Never violate DO NOTs (Quick Reference Critical Boundaries)
- **Timing budget:** Profile processHop(), ensure <14ms total (Goertzel <10ms, beat <2ms, contract <1ms)
- **Memory budget:** No heap allocation in processHop(), stay within ~20KB RAM
- **Test-driven:** Run verification tests after every phase (Phase 6 tasks 6.1 to 6.10)

### Integration Notes

- **Synchronization Points:** Follow parallel multi-agent strategy (Agent 1, 2, 3, 4, 5, 6)
- **Dependencies:** Phase 2 depends on Phase 1, Phase 3 depends on Phase 1+2, Phase 4 depends on Phase 1+2+3, etc.
- **Integration-Critical:** Follow exact sequences in IC-1 and IC-2 (order matters)
- **Cross-Core:** SnapshotBuffer is lock-free atomic (Core 0 writes, Core 1 reads read-only)
- **Expert Consultation:** Flag EXP-1 to EXP-6 when domain expertise required (User Q3 protocol)

### Documentation Hierarchy

**Read in this order:**
1. **AGENT_ONBOARDING.md** - Mandatory checklist (START HERE)
2. **HARDENED_SPEC.md** - Verbatim canonical code (WHAT to implement)
3. **MAGIC_NUMBERS.md** - Constant derivations (WHY these values)
4. **prd.md** - Requirements specification (WHY this design)
5. **technical_blueprint.md** - Architecture (HOW it's structured)
6. **tasks.md** - Implementation work items (WHAT to do)

**This Document:** Context Engineer-formatted synthesis with 1:1 requirement mapping
**Comprehensive Tasks:** `planning/audio-pipeline-redesign/tasks.md` (612 lines, 112 tasks, full 6-phase breakdown)

---

*Document Version: 1.0 (Context Engineer Synthesis)*  
*Created: 2026-01-12*  
*Synthesized From: planning/audio-pipeline-redesign/* comprehensive documentation*  
*Parallel Multi-Agent Strategy: User Q5 incremental build with swarm execution*  
*Status: READY FOR PARALLEL MULTI-AGENT IMPLEMENTATION*  
*Mission: Commercial-Grade Audio Pipeline - Blocks Deployment, CI/CD, System Design*
