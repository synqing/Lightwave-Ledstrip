# Task List: Audio Pipeline Redesign

> **⚠️ STOP:** Complete **[AGENT_ONBOARDING.md](./AGENT_ONBOARDING.md)** checklist FIRST. No exceptions.

> **📋 THEN:** Read **[HARDENED_SPEC.md](./HARDENED_SPEC.md)** for verbatim code and **[MAGIC_NUMBERS.md](./MAGIC_NUMBERS.md)** for constant derivations.

## Quick Reference (Extracted from PRD & Blueprint)

### Ownership Rules
*Source: PRD §7 + Blueprint §2.1*

| Artifact | Created By | App's Role | DO NOT |
|----------|------------|------------|--------|
| Raw audio samples | I2S DMA hardware | Observe (read from buffer) | Generate synthetic samples |
| Goertzel bin magnitudes | GoertzelDFT module | Create (FROM SENSORY BRIDGE) | Use FFT or agent-invented bins |
| Beat/tempo output | PLLTracker module | Create (FROM EMOTISCOPE) | Invent tempo algorithms |
| ControlBusFrame | ControlBus state machine | Create (aggregation) | Expose internal state |
| SnapshotBuffer update | Audio pipeline | Create (lock-free publish) | Modify from Core 1 |

### State Variables
*Source: PRD §8 + Blueprint §3*

| State Variable | Initial Value | Created When | Cleared When | Persists Across |
|----------------|---------------|--------------|--------------|------------------|
| goertzel_coeffs | Sensory Bridge values | AudioNode::onStart() | Never (const) | Forever |
| pll_state | Emotiscope defaults | PLLTracker::init() | resetDspState() | Audio session |
| controlbus_smooth | Zero | ControlBus::Reset() | ControlBus::Reset() | Audio session |
| ring_buffer | Zero | First hop | Overwritten continuously | Rolling window |

### Critical Boundaries
*Source: Blueprint §2.3*

- **DO NOT:** Generate synthetic audio samples (I2S DMA is sole source)
- **DO NOT:** Use FFT for main DSP feed (Goertzel only)
- **DO NOT:** Invent tempo algorithms (Emotiscope 2.0 verbatim only)
- **DO NOT:** Modify lines marked "DO NOT MODIFY"
- **DO NOT:** Allow Core 1 to write to audio state (SnapshotBuffer is read-only)
- **DO NOT:** Expose raw Goertzel bins to visual pipeline (contract layer boundary)
- **MUST:** Use 16kHz sample rate, 128-sample chunks, 8ms window
- **MUST:** Use semitone-spaced bins (not arbitrary FFT bins)
- **MUST:** Mark all sacred code with source annotations
- **MUST:** Run verification tests after any change

### User Visibility Rules
*Source: PRD §6*

| Consumer Action | Consumer Sees | Consumer Does NOT See | Timing |
|-----------------|---------------|----------------------|--------|
| Read ControlBusFrame | Smoothed frequency bands (0.0-1.0) | Raw Goertzel magnitudes, windowing internals | Every 16ms hop |
| Read tempo output | BPM (60-300), phase (0.0-1.0), confidence | PLL internal state, onset buffer | Every hop |
| Read beat tick | Boolean beat_tick flag | Onset detection internals, threshold calculations | On beat |

---

## Requirements Traceability

**Every requirement MUST map to at least one task. Nothing should be lost.**

| Source | Requirement | Mapped To Task |
|--------|-------------|----------------|
| PRD §5.1.1 | Bins are semitone-spaced | Task 1.3, Verification 1.0 |
| PRD §5.1.2 | Bin frequencies match Sensory Bridge exactly | Task 1.2, Verification 1.0 |
| PRD §5.1.3 | Verification test compares bin frequencies | Task 1.6 |
| PRD §5.1.4 | Deviation from reference fails test | Task 1.6 |
| PRD §5.2.1 | Window function matches GDFT.h exactly | Task 1.4 |
| PRD §5.2.2 | Window coefficients hardcoded | Task 1.4 |
| PRD §5.2.3 | Verification test for window output | Task 1.7 |
| PRD §5.2.4 | Code marked DO NOT MODIFY | Task 1.5 |
| PRD §5.3.1 | Onset detection matches Emotiscope | Task 2.2, 2.3 |
| PRD §5.3.2 | Threshold uses reference formula | Task 2.3 |
| PRD §5.3.3 | Magic numbers documented | Task 2.4 |
| PRD §5.3.4 | Verification with known input/output | Task 2.6 |
| PRD §5.4.1 | PLL gains match Emotiscope | Task 2.4 |
| PRD §5.4.2 | Phase correction verbatim | Task 2.3 |
| PRD §5.4.3 | State machine matches reference | Task 2.3 |
| PRD §5.4.4 | BPM within ±1 BPM | Task 2.7, Task 6.3 |
| PRD §5.5.1 | ControlBusFrame canonical fields only | Task 3.2 |
| PRD §5.5.2 | Each field documented with source | Task 3.3 |
| PRD §5.5.3 | No legacy fields remain | Task 3.2, Task 4.2 |
| PRD §5.5.4 | Contract update compatible | Task 3.4 |
| PRD §6.1 V-1 | Smoothed bands visible | Task 3.2 (User Sees) |
| PRD §6.1 V-2 | BPM/phase/confidence visible | Task 3.2 (User Sees) |
| PRD §6.2 T-1 | Audio hop every 8ms | Task 4.4 (State Change) |
| PRD §6.2 T-3 | Phase advances smoothly | Task 2.7, Task 6.3 |
| PRD §7.1 O-1 | I2S DMA creates samples | Quick Reference |
| PRD §7.1 O-2 | GoertzelDFT creates bins | Task 1.0 (Ownership) |
| PRD §7.1 O-3 | PLLTracker creates tempo | Task 2.0 (Ownership) |
| PRD §7.3 | Derived ownership rules | Quick Reference DO NOTs |
| PRD §8.1 SI-1 | Audio state isolated to Core 0 | Task 4.4, IC-1 |
| PRD §8.2 SL-1 | Goertzel coefficients immutable | Task 1.3, Task 1.0 Post-condition |
| PRD §8.2 SL-2 | PLL state lifecycle | Task 2.0 Pre/Post conditions |
| Blueprint §2.3 | All boundary rules | Quick Reference DO NOTs |
| Blueprint §3.1 | Init transition | Task 4.0 Pre/Post conditions |
| Blueprint §3.2 | Hop transition | Task 4.0 Verification |
| Blueprint §3.3 | Reset transition | Task 4.5 |
| Blueprint §4.1 | Hop processing pipeline | IC-1 |
| Blueprint §4.2 | Renderer read path | IC-2 |

---

## Overview

This task list implements the complete audio pipeline redesign specified in PRD and Technical Blueprint. The goal is to replace the corrupted audio implementation with a canonical version based 100% on Sensory Bridge 4.1.1 (Goertzel frequency analysis) and Emotiscope 2.0 (beat/tempo tracking). No agent inventions are allowed. Every component receives a 4-layer specification (Algorithm → Pseudocode → Reference Code → Tests) to prevent future drift.

The work is divided into 6 phases:
1. GoertzelDFT foundation from Sensory Bridge
2. Beat tracking foundation from Emotiscope
3. Contract layer redesign
4. AudioNode integration and cleanup
5. File removal and migration
6. Verification and validation

---

## Relevant Files

### Files to CREATE
- `src/audio/goertzel/GoertzelDFT.h` - Goertzel class header
- `src/audio/goertzel/GoertzelDFT.cpp` - Goertzel implementation
- `src/audio/goertzel/GoertzelConstants.h` - Bin frequencies, coefficients
- `src/audio/goertzel/WindowBank.h` - Hann window LUT
- `src/audio/tempo/OnsetDetector.h` - Onset detection header
- `src/audio/tempo/OnsetDetector.cpp` - Onset implementation
- `src/audio/tempo/PLLTracker.h` - PLL tempo tracker header
- `src/audio/tempo/PLLTracker.cpp` - PLL implementation
- `tests/audio/test_goertzel_bins.cpp` - Bin frequency verification
- `tests/audio/test_goertzel_window.cpp` - Window function verification
- `tests/audio/test_onset_detection.cpp` - Onset verification
- `tests/audio/test_pll_tracking.cpp` - PLL verification
- `tests/audio/test_bpm_accuracy.cpp` - BPM accuracy test

### Files to MODIFY
- `src/audio/AudioNode.h` - Remove K1/legacy, add new components
- `src/audio/AudioNode.cpp` - Rewrite processHop()
- `src/audio/contracts/ControlBus.h` - Redesign with canonical fields

### Files to REMOVE
- `src/audio/k1/` - K1 FFT path directory
- `src/audio/K1_GoertzelTables_16k.h` - Wrong approach
- `src/audio/fft/` - FFT directory
- `src/audio/GoertzelAnalyzer.cpp` - Legacy path
- `src/audio/GoertzelAnalyzer.h` - Legacy path
- `src/audio/tempo/TempoTracker.cpp` - Corrupted implementation
- `src/audio/tempo/TempoTracker.h` - Corrupted implementation

### Reference Files (READ ONLY)
- `/Users/spectrasynq/.../SensoryBridge-4.1.1/SENSORY_BRIDGE_FIRMWARE/constants.h`
- `/Users/spectrasynq/.../SensoryBridge-4.1.1/SENSORY_BRIDGE_FIRMWARE/GDFT.h`
- `/Users/spectrasynq/.../Emotiscope-2.0/main/` - Beat tracking source

---

## Tasks

### 1.0 GoertzelDFT Foundation (FROM SENSORY BRIDGE 4.1.1)

**Pre-condition:** Sensory Bridge 4.1.1 source files accessible at reference path. No existing GoertzelDFT in codebase.

#### Sub-Tasks:

- [ ] 1.1 Review context: PRD §5.1, §5.2, §9.2 and Blueprint §5.1, §6.1
  - **Relevant Sections:** Goertzel bin frequencies, windowing implementation, reference paths
  - **Key Decisions:** Semitone-spaced bins, Hann window, 512-sample window, 64/96 bins
  - **Watch Out For:** Agent temptation to "improve" - copy verbatim only

- [ ] 1.2 Extract GoertzelConstants from Sensory Bridge constants.h
  - **Input:** Sensory Bridge constants.h file
  - **Output:** `src/audio/goertzel/GoertzelConstants.h` with bin frequencies
  - **Ownership:** App creates (O-2: derived from reference)
  - **State Change:** None (const values)
  - **Implements:** PRD §5.1.2, Blueprint §6.1

- [ ] 1.3 Extract and validate bin frequencies (semitone-spaced)
  - **Input:** GoertzelConstants.h
  - **Output:** Validated frequency table (A1=55Hz to C7=2093Hz)
  - **State Change:** goertzel_coeffs populated (SL-1)
  - **Implements:** PRD §5.1.1, §5.1.2

- [ ] 1.4 Extract Hann window LUT from GDFT.h
  - **Input:** Sensory Bridge GDFT.h window implementation
  - **Output:** `src/audio/goertzel/WindowBank.h` with 512 coefficients
  - **Ownership:** App creates (derived from reference)
  - **State Change:** None (const values)
  - **Implements:** PRD §5.2.1, §5.2.2

- [ ] 1.5 Implement GoertzelDFT class matching GDFT.h exactly
  - **Input:** Sensory Bridge GDFT.h analyze function
  - **Output:** `src/audio/goertzel/GoertzelDFT.h`, `GoertzelDFT.cpp`
  - **Ownership:** App creates (O-2: Goertzel bin magnitudes)
  - **State Change:** None (pure function)
  - **Implements:** PRD §5.1.2, §5.2.4, Blueprint §7.1

- [ ] 1.6 Mark all code with "// FROM SENSORY BRIDGE 4.1.1 - DO NOT MODIFY"
  - **Input:** All GoertzelDFT source files
  - **Output:** Annotated source files
  - **Implements:** PRD §5.2.4, §4.1 FR-10

- [ ] 1.7 Create bin frequency verification test
  - **Input:** GoertzelConstants.h
  - **Output:** `tests/audio/test_goertzel_bins.cpp`
  - **Implements:** PRD §5.1.3, §5.1.4

- [ ] 1.8 Create window function verification test
  - **Input:** WindowBank.h, reference output
  - **Output:** `tests/audio/test_goertzel_window.cpp`
  - **Implements:** PRD §5.2.3

**Post-condition:** GoertzelDFT class exists with const coefficients from Sensory Bridge. All code annotated with source. Verification tests pass.

**Verification:**
- [ ] All 64 bin frequencies within 0.01 Hz of reference (PRD §5.1.3)
- [ ] Goertzel coefficients exact float match to reference
- [ ] Window output matches reference ±0.0001 (PRD §5.2.3)
- [ ] All sacred code has DO NOT MODIFY markers (PRD §5.2.4)

---

### 2.0 Beat Tracking Foundation (FROM EMOTISCOPE 2.0)

**Pre-condition:** Emotiscope 2.0 source files accessible. Task 1.0 complete (GoertzelDFT available for onset input).

#### Sub-Tasks:

- [ ] 2.1 Review context: PRD §5.3, §5.4, §9.2 and Blueprint §5.1, §7.2, §7.3
  - **Relevant Sections:** Onset detection, PLL tracking, reference paths
  - **Key Decisions:** Spectral flux for onsets, phase-locked loop for tempo
  - **Watch Out For:** Previous 20% agent corruption - extract 100% verbatim

- [ ] 2.2 Extract OnsetDetector from Emotiscope 2.0
  - **Input:** Emotiscope 2.0 onset detection source
  - **Output:** `src/audio/tempo/OnsetDetector.h`, `OnsetDetector.cpp`
  - **Ownership:** App creates (component of O-3)
  - **State Change:** m_prevBins updated each hop
  - **Implements:** PRD §5.3.1, Blueprint §7.3

- [ ] 2.3 Extract PLLTracker from Emotiscope 2.0
  - **Input:** Emotiscope 2.0 PLL implementation
  - **Output:** `src/audio/tempo/PLLTracker.h`, `PLLTracker.cpp`
  - **Ownership:** App creates (O-3: Beat/tempo output)
  - **State Change:** pll_state updated (SL-2)
  - **Implements:** PRD §5.4.1, §5.4.2, §5.4.3, Blueprint §7.2

- [ ] 2.4 Document all magic numbers with Emotiscope source reference
  - **Input:** PLLTracker and OnsetDetector source
  - **Output:** Inline comments with source file:line references
  - **Implements:** PRD §5.3.3, §5.4.1

- [ ] 2.5 Mark all code with "// FROM EMOTISCOPE 2.0 - DO NOT MODIFY"
  - **Input:** All beat tracking source files
  - **Output:** Annotated source files
  - **Implements:** PRD §4.1 FR-10

- [ ] 2.6 Create onset detection verification test
  - **Input:** Known test vectors from reference
  - **Output:** `tests/audio/test_onset_detection.cpp`
  - **Implements:** PRD §5.3.4

- [ ] 2.7 Create PLL tracking verification test
  - **Input:** Known BPM test audio, reference output
  - **Output:** `tests/audio/test_pll_tracking.cpp`
  - **Implements:** PRD §5.4.4

**Post-condition:** OnsetDetector and PLLTracker exist with exact Emotiscope 2.0 logic. All code annotated. Verification tests pass.

**Verification:**
- [ ] Onset threshold calculation matches Emotiscope formula (PRD §5.3.2)
- [ ] PLL gains exact value match to Emotiscope (PRD §5.4.1)
- [ ] State machine transitions match reference (PRD §5.4.3)
- [ ] BPM output within ±1 BPM of ground truth (PRD §5.4.4)

---

### 3.0 Contract Layer Redesign

**Pre-condition:** Tasks 1.0 and 2.0 complete. New component interfaces defined.

#### Sub-Tasks:

- [ ] 3.1 Review context: PRD §5.5, §6, §7 and Blueprint §2, §6.3
  - **Relevant Sections:** ControlBusFrame fields, visibility rules, ownership
  - **Key Decisions:** Remove legacy fields, canonical fields only
  - **Watch Out For:** Breaking visual pipeline (Phase 2 handles that)

- [ ] 3.2 Redesign ControlBusFrame with canonical fields only
  - **Input:** Blueprint §6.3 contract specification
  - **Output:** Updated `src/audio/contracts/ControlBus.h`
  - **Ownership:** App creates (O-4: ControlBusFrame)
  - **User Sees:** Smoothed bands, BPM/phase/confidence (V-1, V-2)
  - **State Change:** controlbus_smooth structure changes (SL-3)
  - **Implements:** PRD §5.5.1, §5.5.3, Blueprint §6.3

- [ ] 3.3 Document each field with source annotation
  - **Input:** ControlBusFrame fields
  - **Output:** Inline comments: "// Source: GoertzelDFT bin X" etc.
  - **Implements:** PRD §5.5.2

- [ ] 3.4 Update smoothing state machine for new fields
  - **Input:** Existing ControlBus smoothing logic
  - **Output:** Updated smoothing for canonical fields
  - **State Change:** Smoothing state adapted
  - **Implements:** PRD §5.5.4

- [ ] 3.5 Verify SnapshotBuffer compatibility
  - **Input:** SnapshotBuffer implementation
  - **Output:** Confirmed lock-free publish works with new frame
  - **Integration:** SnapshotBuffer::publish()
  - **Implements:** PRD §7.1 O-5

**Post-condition:** ControlBusFrame contains only canonical fields. Legacy fields removed. Smoothing updated. SnapshotBuffer compatible.

**Verification:**
- [ ] No legacy fields in ControlBusFrame (flux, chroma, snare/hihat triggers removed)
- [ ] All fields have source documentation (PRD §5.5.2)
- [ ] SnapshotBuffer publish/read works (PRD §7.1 O-5)

---

### 4.0 AudioNode Integration

**Pre-condition:** Tasks 1.0, 2.0, 3.0 complete. All new components ready.

#### Sub-Tasks:

- [ ] 4.1 Review context: PRD §9.3, Blueprint §4.1, §5
  - **Relevant Sections:** Integration points, hop processing pipeline, files to modify
  - **Key Decisions:** Single canonical path, remove K1/legacy
  - **Watch Out For:** Breaking existing functionality during transition

- [ ] 4.2 Remove K1/legacy includes from AudioNode.h
  - **Input:** Current AudioNode.h includes
  - **Output:** Cleaned AudioNode.h with new component includes
  - **Implements:** PRD §4.1 FR-6, Blueprint §5.3

- [ ] 4.3 Add new component instances to AudioNode
  - **Input:** GoertzelDFT, OnsetDetector, PLLTracker classes
  - **Output:** AudioNode with member instances
  - **Implements:** Blueprint §5.1

- [ ] 4.4 Rewrite processHop() to use canonical pipeline
  - **Input:** Blueprint §4.1 call sequence
  - **Output:** Updated AudioNode::processHop()
  - **State Change:** ring_buffer shift (SL-4), controlbus update (SL-3)
  - **Integration:** GoertzelDFT→OnsetDetector→PLLTracker→ControlBus→SnapshotBuffer
  - **Implements:** Blueprint §4.1, PRD §6.2 T-1

- [ ] 4.5 Implement resetDspState() with new components
  - **Input:** PLLTracker::reset(), ControlBus::Reset()
  - **Output:** Updated resetDspState() function
  - **State Change:** SL-2 cleared, SL-3 cleared
  - **Implements:** Blueprint §3.3

- [ ] 4.6 Build and verify RAM/Flash usage
  - **Input:** Compiled firmware
  - **Output:** Memory usage report
  - **Implements:** Blueprint §9 C-2

**Post-condition:** AudioNode uses only canonical components. K1/legacy removed. processHop() follows Blueprint §4.1 sequence exactly.

**Verification:**
- [ ] No K1 includes in AudioNode.h
- [ ] processHop() calls in exact order: ringBuffer→GoertzelDFT→OnsetDetector→PLLTracker→ControlBus→SnapshotBuffer
- [ ] RAM usage within budget (~20KB for audio)
- [ ] Build succeeds with `pio run -e esp32dev_audio`

---

### 5.0 File Removal and Cleanup

**Pre-condition:** Task 4.0 complete. AudioNode working with new components.

#### Sub-Tasks:

- [ ] 5.1 Review context: PRD Appendix B, Blueprint §5.3
  - **Relevant Sections:** Files to remove list
  - **Key Decisions:** Remove all K1/FFT/legacy paths
  - **Watch Out For:** Any remaining references to deleted files

- [ ] 5.2 Remove K1 directory (src/audio/k1/)
  - **Input:** K1 directory path
  - **Output:** Directory deleted
  - **Implements:** PRD Appendix B, Blueprint §5.3

- [ ] 5.3 Remove K1_GoertzelTables_16k.h
  - **Input:** File path
  - **Output:** File deleted
  - **Implements:** PRD Appendix B

- [ ] 5.4 Remove fft directory (src/audio/fft/)
  - **Input:** FFT directory path
  - **Output:** Directory deleted
  - **Implements:** PRD Appendix B

- [ ] 5.5 Remove legacy GoertzelAnalyzer files
  - **Input:** GoertzelAnalyzer.h, GoertzelAnalyzer.cpp paths
  - **Output:** Files deleted
  - **Implements:** Blueprint §5.3

- [ ] 5.6 Remove corrupted TempoTracker files
  - **Input:** TempoTracker.h, TempoTracker.cpp paths
  - **Output:** Files deleted
  - **Implements:** Blueprint §5.3

- [ ] 5.7 Search for and remove any remaining references
  - **Input:** Grep for K1, FFT, legacy includes
  - **Output:** All references removed
  - **Implements:** PRD §4.1 FR-6

- [ ] 5.8 Build verification after cleanup
  - **Input:** Cleaned codebase
  - **Output:** Successful build
  - **Implements:** Build verification

**Post-condition:** All K1/FFT/legacy files removed. No dangling references. Build succeeds.

**Verification:**
- [ ] `src/audio/k1/` does not exist
- [ ] `src/audio/fft/` does not exist
- [ ] No files reference removed paths
- [ ] Build succeeds: `pio run -e esp32dev_audio`

---

### 6.0 Verification and Validation

**Pre-condition:** Tasks 1.0-5.0 complete. Full canonical pipeline integrated.

#### Sub-Tasks:

- [ ] 6.1 Review context: PRD §10, Blueprint §10
  - **Relevant Sections:** Success metrics M-1 through M-5
  - **Key Decisions:** 100% match required, no deviations
  - **Watch Out For:** False positives in tests

- [ ] 6.2 Run M-1: Goertzel bin match test
  - **Input:** test_goertzel_bins.cpp
  - **Output:** 100% match confirmed
  - **Implements:** PRD §10 M-1

- [ ] 6.3 Run M-2: Beat tracking match test
  - **Input:** test_onset_detection.cpp, test_pll_tracking.cpp
  - **Output:** 100% match confirmed
  - **Implements:** PRD §10 M-2

- [ ] 6.4 Run M-3: BPM accuracy test with known audio
  - **Input:** 60/120/180 BPM test files
  - **Output:** All within ±1 BPM
  - **Implements:** PRD §10 M-3

- [ ] 6.5 Run M-4: Drift detection test
  - **Input:** Hash of sacred code sections
  - **Output:** No modifications detected
  - **Implements:** PRD §10 M-4

- [ ] 6.6 Audit M-5: 4-layer spec coverage
  - **Input:** All audio component documentation
  - **Output:** All components have 4 layers
  - **Implements:** PRD §10 M-5, §11

- [ ] 6.7 End-to-end integration test
  - **Input:** I2S audio through full pipeline
  - **Output:** Valid ControlBusFrame in SnapshotBuffer
  - **Integration:** Full pipeline
  - **Implements:** Blueprint §4.1, §4.2

- [ ] 6.8 Profile timing budget compliance
  - **Input:** processHop() timing
  - **Output:** <14ms average confirmed
  - **Implements:** Blueprint §9 C-1

**Post-condition:** All M-1 through M-5 metrics achieved. Pipeline verified end-to-end. Timing within budget.

**Verification:**
- [ ] M-1: Goertzel 100% match - PASS
- [ ] M-2: Beat tracking 100% match - PASS
- [ ] M-3: BPM ±1 accuracy - PASS
- [ ] M-4: No drift detected - PASS
- [ ] M-5: 4-layer coverage complete - PASS
- [ ] Timing: <14ms average processHop() - PASS

---

## Integration-Critical Tasks
*Source: Blueprint §4 - Integration Wiring*

### IC-1: Hop Processing Pipeline
*Maps to: Blueprint §4.1*

**Critical Sequence:**
```
1. m_ringBuffer.push(chunk)           // REQUIRED: Update rolling window FIRST (SL-4)
2. m_goertzelDFT.analyze(window, bins) // REQUIRED: Frequency analysis before onset (O-2)
3. onset = m_onsetDetector.process(bins) // REQUIRED: Onset before PLL
4. m_pllTracker.update(onset, &tempo)  // REQUIRED: Tempo after onset (O-3)
5. m_controlBus.ingest(bins, tempo)    // REQUIRED: Aggregate before publish (O-4)
6. m_snapshotBuffer.publish()          // REQUIRED: Cross-core publish LAST (O-5)
```

**Ownership Rules (from PRD §7):**
- GoertzelDFT creates bin magnitudes (O-2) - App is responsible
- PLLTracker creates tempo output (O-3) - App is responsible
- ControlBus creates aggregated frame (O-4) - App is responsible
- App must NOT generate synthetic samples (use I2S only)

**User Visibility (from PRD §6):**
- Consumer sees: Smoothed bands (V-1), BPM/phase/confidence (V-2)
- Consumer does NOT see: Raw Goertzel magnitudes, PLL internal state

**State Changes (from Blueprint §3):**
- Before: ring_buffer has previous 512 samples, pll_state from last hop
- After: ring_buffer updated with 128 new samples, pll_state updated, frame published

**Common Mistakes to Avoid (from Blueprint §2.3):**
- **DO NOT** call analyze() before push() - window will have stale samples
- **DO NOT** call pllTracker.update() before onset detection - tempo will be wrong
- **DO NOT** publish() before controlBus.ingest() - frame will be incomplete
- **DO NOT** modify bin values after analyze() - violates O-2 ownership

**Verification:**
- [ ] Call sequence matches exactly (no reordering)
- [ ] Each step completes before next begins
- [ ] Frame published within 16ms timing budget

---

### IC-2: Renderer Read Path
*Maps to: Blueprint §4.2*

**Critical Sequence:**
```
1. m_audioFrame = m_snapshotBuffer.read()  // Lock-free copy
2. ctx.audio = AudioAccessor(m_audioFrame)  // Wrap for effects
3. effect->render(ctx)                      // Effects use ctx.audio.*
```

**Ownership Rules (from PRD §7):**
- SnapshotBuffer is read-only for Core 1
- Renderer must NOT write to m_audioFrame
- Effects receive smoothed values only (contract boundary)

**User Visibility (from PRD §6):**
- Effect sees: ctx.audio.bin(i) returns smoothed value (V-1)
- Effect sees: ctx.audio.tempo.bpm returns validated BPM (V-2)
- Effect does NOT see: Raw internal state

**Common Mistakes to Avoid:**
- **DO NOT** modify SnapshotBuffer from renderer
- **DO NOT** cache audio frame across multiple ticks (may be stale)
- **DO NOT** access internal audio state directly (use ctx.audio accessor)

**Verification:**
- [ ] Renderer only reads, never writes
- [ ] Fresh frame read each tick
- [ ] Effects use accessor methods only

---

## Validation Checklist

Before implementation, verify 1:1 mapping is complete:

### PRD Coverage
- [x] Every §5 acceptance criterion has a corresponding subtask
- [x] Every §6 visibility rule has "User Sees" in relevant subtask
- [x] Every §7 ownership rule is in Quick Reference AND relevant subtask "Ownership" field
- [x] Every §8 state requirement has "State Change" in relevant subtask

### Blueprint Coverage
- [x] Every §2 boundary rule is in Critical Boundaries AND enforced in tasks
- [x] Every §3 state transition maps to Pre/Post conditions
- [x] Every §4 integration wiring maps to an Integration-Critical Task

### Task Quality
- [x] First subtask of each parent references relevant docs
- [x] All subtasks are specific and actionable (not vague)
- [x] All "Implements" fields trace back to PRD/Blueprint sections

---

## Notes

- **Build Command:** `pio run -e esp32dev_audio -t upload`
- **Monitor Command:** `pio device monitor -b 115200`
- **Test Command:** `pio test -e native`
- Unit tests live in `tests/audio/`
- Follow existing lint & format scripts
- Complete each parent task fully before starting next
- **Pay special attention to IC-1 and IC-2** - follow sequences exactly
- Verify pre-conditions are met before starting each parent task
- Confirm post-conditions after completing each parent task
- **Reference the Quick Reference section frequently during implementation**
- **When in doubt, check the original PRD §X or Blueprint §Y cited in "Implements"**
- **NEVER modify lines marked "DO NOT MODIFY" without explicit user approval**
- **Run verification tests after ANY change**

### Reference Paths

```
Sensory Bridge 4.1.1:
/Users/spectrasynq/Workspace_Management/Software/K1.node1/references/
  Sensorybridge.sourcecode/SensoryBridge-4.1.1/SENSORY_BRIDGE_FIRMWARE/

Emotiscope 2.0:
/Users/spectrasynq/Workspace_Management/Software/K1.node1/references/
  Emotiscope.sourcecode/Emotiscope-2.0/main/
```

---

*Document Version: 1.0*
*Created: 2026-01-12*
*Status: Ready for Implementation*
