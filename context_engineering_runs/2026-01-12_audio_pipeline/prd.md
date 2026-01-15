# Product Requirements Document: LightwaveOS Audio Pipeline Redesign

> **Traceability Note:** This PRD synthesizes existing comprehensive planning documentation into Context Engineer format. All requirements are sourced from verified existing documents. Section numbers (§1-§10) are referenced by Technical Blueprint and Task List.

> **CRITICAL: This is mission-critical commercial infrastructure. Audio pipeline blocks commercial deployment, visual pipeline CI/CD, and system design progression.**

---

## §1. Overview & Vision

### §1.1 Executive Summary

This PRD specifies a **complete redesign** of the LightwaveOS v2 audio pipeline for an ESP32-S3 dual-strip Light Guide Plate (LGP) system. The current audio pipeline has been corrupted by **agent session discontinuity** and requires replacement with a clean, well-documented architecture based on two canonical references:

1. **Sensory Bridge 4.1.1** - Goertzel frequency analysis (musically-aligned bins, proper windowing)
2. **Emotiscope 2.0** - Beat/tempo tracking (onset detection, PLL phase locking)

**Current Critical Problem:** ZERO visibility into:
- SPH0645 I2S MEMS microphone signal quality
- DC offset correctness at audio source
- DSP algorithm validity and data range/SNR/SPL
- Contract layer data quality feeding visual pipeline

**Goal:** Bulletproof commercial-grade audio frontend with granular oversight of entire chain: mic → DSP → contract layer → visual pipeline.

**Source:** `planning/audio-pipeline-redesign/prd.md` §1.1, §2.1

### §1.2 Key Outcomes

- **O-1**: Clean audio pipeline based 100% on Sensory Bridge 4.1.1 Goertzel implementation
- **O-2**: Working beat tracking based 100% on Emotiscope 2.0 tempo detection
- **O-3**: 4-layer specification (Algorithm → Pseudocode → Reference Code → Tests) preventing future agent drift
- **O-4**: Updated contract layer (ControlBusFrame) exposing canonical audio features to visual pipeline
- **O-5**: Granular visibility into signal quality at every stage (mic → DSP → contract)
- **O-6**: Automated validation tests as first line of defense, domain expert analysis when required

**Source:** `planning/audio-pipeline-redesign/prd.md` §1.2, User Q3 validation strategy

### §1.3 Scope

**Phase 1 (This PRD):**
- Audio pipeline redesign (Goertzel + beat tracking)
- Contract layer (ControlBusFrame) update
- 4-layer specification documentation
- Signal quality verification at all stages
- Automated verification tests

**Phase 2 (Future - Iterative):**
- Visual pipeline updates (45+ effects)
- Advanced audio features
- Edge case handling
- Performance optimization

**NOT in scope:** Visual pipeline effects migration (blocked on this foundation)

**Source:** `planning/audio-pipeline-redesign/prd.md` §1.3, User Q4 timeline

---

## §2. Problem Statement

### §2.1 Root Cause Analysis

The audio pipeline has suffered from **architectural drift due to agent session discontinuity**:

```
Session 1: Agent implements 64-bin Goertzel (correct)
Session 2: Agent adds FFT for beat tracking experiment (reasonable)
Session 3: Beat tracking fails, but FFT stays in codebase
Session 4: New agent doesn't know FFT was experimental, integrates it into main DSP feed
Session 5: Agent ports Emotiscope beat tracking, gets 80% right, invents 20%
Session N: Frankenstein codebase with no single source of truth
```

**Commercial Impact Chain:**
```
Bad audio → Poor UX → Loss of trust → No word-of-mouth marketing → 
Financial loss → Project failure
```

**Source:** `planning/audio-pipeline-redesign/prd.md` §2.1, User Q4 cost of failure

### §2.2 Current Pain Points

| ID | Pain Point | Impact | Blocking | Source |
|----|------------|--------|----------|--------|
| PP-1 | FFT accidentally in main DSP feed | Duplicate processing, architectural confusion | - | User Q1 |
| PP-2 | Beat tracking 80% correct, 20% agent-invented | Unreliable tempo detection | - | User Q1, Q5 |
| PP-3 | Multiple overlapping analysis paths | CPU waste, maintenance burden | - | User Q1 |
| PP-4 | No specification to prevent drift | Future agents will corrupt again | - | User Q1, Q5 |
| PP-5 | ZERO visibility into signal quality | Cannot debug/tune DSP pipeline | **YES - Commercial deployment** | User Q1 |
| PP-6 | Unknown mic signal quality | Cannot validate audio source integrity | **YES - Commercial deployment** | User Q1 |
| PP-7 | Blocks visual pipeline CI/CD | Cannot build reliable automated tests | **YES - CI/CD** | User Q4 |

**Source:** `planning/audio-pipeline-redesign/prd.md` §2.2, User Q1, Q4

### §2.3 Why Full Redesign Required

Current codebase cannot be incrementally fixed because:
1. No clear boundary between "correct" and "corrupted" code
2. Agent-invented code interleaved with reference implementations
3. Multiple failed fix attempts made it worse
4. Zero observability into signal chain prevents diagnosis
5. **Only solution:** Start fresh from canonical references with full instrumentation

**Source:** `planning/audio-pipeline-redesign/prd.md` §2.3, User Q1

---

## §3. Target Users

| User Type | Needs | Key Actions | Priority |
|-----------|-------|-------------|----------|
| **Commercial End Users** | Reliable, responsive audio-reactive LED effects | Experience smooth beat-synced visuals | CRITICAL |
| **Future AI Agents** | Unambiguous specification they cannot misinterpret | Read spec, implement exactly, run verification tests | CRITICAL |
| **Human Developers** | Understanding of design intent and constraints | Review spec, audit agent implementations, debug issues | HIGH |
| **Domain Experts** | Validation criteria for signal quality/DSP correctness | Provide professional guidance on SNR/SPL/robustness metrics | HIGH |
| **Audio Pipeline** | Correctly processed audio features for visual effects | Receive I2S audio, produce ControlBusFrame, publish snapshots | CRITICAL |
| **Visual Pipeline** | Reliable, well-documented audio contract | Consume ControlBusFrame, drive 45+ LED effects @ 120 FPS | CRITICAL |
| **CI/CD System** | Automated validation tests for audio pipeline | Run verification tests, catch regressions, enable sustainable development | HIGH |

**Source:** `planning/audio-pipeline-redesign/prd.md` §3, User Q1, Q3, Q4

---

## §4. Core Requirements

### §4.1 Functional Requirements

| ID | Requirement | Priority | Source | Validation |
|----|-------------|----------|--------|------------|
| FR-1 | Implement Goertzel frequency analysis exactly as Sensory Bridge 4.1.1 | CRITICAL | User Q1, Q2 | Automated comparison test |
| FR-2 | Use 64/96 musically-aligned bins (semitone-spaced, not arbitrary FFT bins) | CRITICAL | User Q2, prd.md §4.1 | Frequency match test |
| FR-3 | Use 16kHz sample rate, 128-sample chunks, 8ms window | CRITICAL | User Q2, MAGIC_NUMBERS.md | Hardcoded enforcement |
| FR-4 | Implement proper windowing (Sensory Bridge pattern) | CRITICAL | User Q2, HARDENED_SPEC.md | Window output comparison |
| FR-5 | Implement beat tracking exactly as Emotiscope 2.0 | CRITICAL | User Q1, Q2 | Known-input test vectors |
| FR-6 | Remove all FFT code from main audio DSP feed | CRITICAL | User Q1, prd.md §4.1 | Code inspection |
| FR-7 | Expose signal quality metrics at every stage | CRITICAL | User Q1, Q3 | Runtime telemetry |
| FR-8 | Validate SPH0645 mic signal quality (SNR, SPL, DC offset) | CRITICAL | User Q1, Q3 | **EXPERT ADVICE NEEDED** |
| FR-9 | Define new ControlBusFrame contract based on canonical output | HIGH | prd.md §4.1 | Contract validation |
| FR-10 | Create verification tests that catch implementation drift | HIGH | User Q3, Q5 | Test suite execution |
| FR-11 | Document WHY each design decision was made | HIGH | prd.md §4.1 | Documentation audit |
| FR-12 | Mark sacred code sections as "DO NOT MODIFY" | HIGH | User Q2, prd.md §4.1 | Code annotation |

**🚨 CRITICAL GAPS REQUIRING EXPERT ADVICE:**
- **FR-8 validation criteria:** Not qualified to determine bulletproof signal quality requirements for SPH0645 @ 16kHz
- **Needed:** SNR floor validation, SPL range validation, clipping detection, noise floor measurement, long-term stability metrics
- **Validation hierarchy:** Automated tests first → Domain expert analysis when needed

**Source:** `planning/audio-pipeline-redesign/prd.md` §4.1, User Q1, Q2, Q3, Q5

### §4.2 Non-Functional Requirements

| ID | Category | Requirement | Target | Source |
|----|----------|-------------|--------|--------|
| NFR-1 | Maintainability | 4-layer specification for every component | 100% coverage | User Q5, prd.md §4.2 |
| NFR-2 | Correctness | 100% match to canonical references (no agent inventions) | Zero deviations | User Q2, prd.md §4.2 |
| NFR-3 | Verifiability | Automated tests that fail on drift from reference | 100% drift detection | User Q3, Q5 |
| NFR-4 | Documentation | Every function has WHAT/HOW/WHY documented | 100% coverage | prd.md §4.2 |
| NFR-5 | Observability | Signal quality visible at every pipeline stage | Full visibility | User Q1 |
| NFR-6 | Performance | Audio processing within 14ms budget @ 240MHz | <14ms per hop | User Q2, MAGIC_NUMBERS.md |
| NFR-7 | Reliability | Commercial-grade stability for deployment | Zero crashes | User Q1, Q4 |
| NFR-8 | Predictability | Deterministic behavior for CI/CD testing | 100% reproducible | User Q4 |

**Source:** `planning/audio-pipeline-redesign/prd.md` §4.2, User Q1, Q2, Q3, Q4, Q5

---

## §5. User Stories & Acceptance Criteria

### Epic 1: Goertzel Frequency Analysis (Sensory Bridge Pattern)

#### Story §5.1: Musically-Aligned Frequency Bins
**As a** future AI agent implementing the audio pipeline  
**I want** an unambiguous specification of the Goertzel bin frequencies  
**So that** I cannot accidentally use arbitrary FFT frequencies

**Acceptance Criteria:**
- [ ] §5.1.1: Bins are semitone-spaced (musically meaningful intervals)
- [ ] §5.1.2: Bin frequencies match Sensory Bridge 4.1.1 `constants.h` exactly
- [ ] §5.1.3: Verification test compares bin frequencies to reference values
- [ ] §5.1.4: Any deviation from reference frequencies fails the test
- [ ] §5.1.5: Code marked "// FROM SENSORY BRIDGE 4.1.1 - DO NOT MODIFY"

**Source:** `planning/audio-pipeline-redesign/prd.md` §5.1, User Q2

#### Story §5.2: Proper Windowing Implementation
**As a** future AI agent implementing Goertzel analysis  
**I want** line-level reference code for the windowing function  
**So that** I cannot accidentally implement a different window

**Acceptance Criteria:**
- [ ] §5.2.1: Window function matches Sensory Bridge `GDFT.h` exactly
- [ ] §5.2.2: Window coefficients are hardcoded (not computed) to prevent drift
- [ ] §5.2.3: Verification test compares window output to reference output
- [ ] §5.2.4: Code section marked "// FROM SENSORY BRIDGE 4.1.1 - DO NOT MODIFY"

**Source:** `planning/audio-pipeline-redesign/prd.md` §5.2, User Q2

#### Story §5.3: Signal Quality Visibility
**As a** human developer debugging the audio pipeline  
**I want** visibility into signal quality at every stage  
**So that** I can diagnose mic issues, DC offset problems, and DSP data validity

**Acceptance Criteria:**
- [ ] §5.3.1: Expose raw SPH0645 sample statistics (min, max, mean, stddev)
- [ ] §5.3.2: Expose DC offset measurement and correction status
- [ ] §5.3.3: Expose Goertzel bin magnitude ranges (0.0-1.0 normalized)
- [ ] §5.3.4: Expose SNR estimation (signal-to-noise ratio)
- [ ] §5.3.5: Expose clipping detection (samples > threshold)
- [ ] §5.3.6: All metrics available via serial telemetry and/or WebSocket
- [ ] §5.3.7: **EXPERT ADVICE NEEDED:** Validation thresholds for SNR, SPL, noise floor

**Source:** User Q1, Q3

### Epic 2: Beat Tracking (Emotiscope Pattern)

#### Story §5.4: Onset Detection
**As a** future AI agent implementing beat tracking  
**I want** the complete Emotiscope 2.0 onset detection algorithm  
**So that** I cannot accidentally invent my own version

**Acceptance Criteria:**
- [ ] §5.4.1: Onset detection matches Emotiscope 2.0 `main/` implementation exactly
- [ ] §5.4.2: Threshold calculation uses reference formula (not agent interpretation)
- [ ] §5.4.3: All magic numbers are documented with their source
- [ ] §5.4.4: Verification test with known input produces known output
- [ ] §5.4.5: Code marked "// FROM EMOTISCOPE 2.0 - DO NOT MODIFY"

**Source:** `planning/audio-pipeline-redesign/prd.md` §5.3, User Q2

#### Story §5.5: PLL Phase Tracking
**As a** future AI agent implementing tempo tracking  
**I want** the exact PLL implementation from Emotiscope 2.0  
**So that** the 20% that was previously corrupted is now correct

**Acceptance Criteria:**
- [ ] §5.5.1: PLL gains match Emotiscope 2.0 values exactly
- [ ] §5.5.2: Phase correction logic is verbatim from reference
- [ ] §5.5.3: State machine transitions match reference
- [ ] §5.5.4: Verification test confirms BPM output within ±1 BPM of reference
- [ ] §5.5.5: Code marked "// FROM EMOTISCOPE 2.0 - DO NOT MODIFY"

**Source:** `planning/audio-pipeline-redesign/prd.md` §5.4, User Q1, Q2

### Epic 3: Contract Layer

#### Story §5.6: ControlBusFrame Redesign
**As a** the visual pipeline consuming audio features  
**I want** a clean contract that exposes canonical audio features  
**So that** I receive only correct, documented data

**Acceptance Criteria:**
- [ ] §5.6.1: ControlBusFrame contains only fields produced by canonical pipeline
- [ ] §5.6.2: Each field is documented with its source (Goertzel bin X, tempo phase, etc.)
- [ ] §5.6.3: No legacy/corrupted fields remain in the contract
- [ ] §5.6.4: Contract update does not break visual pipeline (Phase 2 handles migration)
- [ ] §5.6.5: Signal quality metrics exposed for CI/CD validation

**Source:** `planning/audio-pipeline-redesign/prd.md` §5.5, User Q1, Q4

### Epic 4: Observability & Validation

#### Story §5.7: Automated Verification Tests
**As a** the CI/CD system validating audio pipeline correctness  
**I want** automated tests that catch deviations from canonical references  
**So that** agent drift is detected immediately

**Acceptance Criteria:**
- [ ] §5.7.1: Test suite compares Goertzel bin frequencies to Sensory Bridge reference
- [ ] §5.7.2: Test suite compares windowing output to reference
- [ ] §5.7.3: Test suite validates beat tracking against known-BPM audio samples
- [ ] §5.7.4: Test suite validates timing budget compliance (<14ms processing)
- [ ] §5.7.5: Test suite validates memory budget compliance (~20KB RAM)
- [ ] §5.7.6: All tests fail loudly on ANY deviation from spec

**Source:** User Q3, Q5, prd.md §4.1 (FR-10)

#### Story §5.8: Domain Expert Validation Criteria
**As a** domain expert reviewing the audio pipeline  
**I want** clear criteria for validating signal quality and DSP correctness  
**So that** I can provide professional guidance on robustness

**Acceptance Criteria:**
- [ ] §5.8.1: **EXPERT ADVICE NEEDED:** Define SPH0645 SNR validation thresholds @ 16kHz
- [ ] §5.8.2: **EXPERT ADVICE NEEDED:** Define SPL range validation for music/speech
- [ ] §5.8.3: **EXPERT ADVICE NEEDED:** Define clipping detection thresholds
- [ ] §5.8.4: **EXPERT ADVICE NEEDED:** Define noise floor measurement methodology
- [ ] §5.8.5: **EXPERT ADVICE NEEDED:** Define long-term stability validation (drift over time)
- [ ] §5.8.6: **EXPERT ADVICE NEEDED:** Define edge case coverage (silence, clipping, DC offset)
- [ ] §5.8.7: Document all expert-provided criteria in validation test suite

**Source:** User Q1, Q3

---

## §6. User Experience Contract

*Note: For embedded firmware, "user" is the consuming system (visual pipeline) and human operators (developers/CI/CD).*

### §6.1 User Visibility Specification

| ID | Consumer Action | Consumer Sees | Consumer Does NOT See | Timing |
|----|-----------------|---------------|----------------------|--------|
| V-1 | Visual pipeline reads ControlBusFrame | Smoothed frequency bands (0.0-1.0) | Raw Goertzel magnitudes, windowing internals | Every 16ms hop |
| V-2 | Visual pipeline reads tempo output | BPM (60-300), phase (0.0-1.0), confidence | PLL internal state, onset buffer | Every hop |
| V-3 | Visual pipeline reads beat tick | Boolean beat_tick flag | Onset detection internals, threshold calculations | On beat |
| V-4 | Developer reads serial telemetry | Signal quality metrics (SNR, clipping, DC offset) | Raw I2S DMA buffer contents | On demand |
| V-5 | CI/CD runs verification tests | PASS/FAIL + deviation metrics | Internal DSP intermediate buffers | Per test run |
| V-6 | Developer inspects audio pipeline state | Published ControlBusFrame snapshot | Audio Core 0 internal state | Via SnapshotBuffer |

**Source:** `planning/audio-pipeline-redesign/prd.md` §6.1, User Q1, Q4

### §6.2 Timing & Feedback Expectations

| ID | Event | Expected Timing | Success Indicator | Failure Indicator |
|----|-------|-----------------|-------------------|-------------------|
| T-1 | Audio hop captured | Every 8ms (128 samples @ 16kHz) | Fresh samples in ring buffer | CaptureResult error |
| T-2 | Goertzel analysis complete | Within 14ms hop period | Bins updated, timing budget OK | Analysis timeout >14ms |
| T-3 | Beat tracking update | Every hop | Phase advances smoothly | BPM jumps >10 BPM |
| T-4 | ControlBusFrame published | Every hop | SnapshotBuffer updated | Stale frame detected |
| T-5 | Signal quality check | Every hop | Metrics within expected range | SNR/clipping warning |
| T-6 | Verification test run | On demand (CI/CD) | All tests PASS | Any test FAIL |

**Source:** `planning/audio-pipeline-redesign/prd.md` §6.2, User Q2, Q3, Q5

---

## §7. Artifact Ownership

### §7.1 Creation Responsibility

| ID | Artifact | Created By | When | App's Role | Failure Handling |
|----|----------|------------|------|------------|------------------|
| O-1 | Raw audio samples | I2S DMA hardware | Every 8ms | Observe (read from ring buffer) | Skip hop, log error |
| O-2 | DC offset correction | Audio pipeline | Before windowing | Create | Use default offset if calibration missing |
| O-3 | Windowed samples | Audio pipeline | After DC correction | Create | N/A (internal) |
| O-4 | Goertzel bin magnitudes | Audio pipeline | After windowing + Goertzel | Create | Publish zero bins on error |
| O-5 | Smoothed frequency bands | ControlBus state machine | After Goertzel | Create | Use previous frame on error |
| O-6 | Beat/tempo output | Beat tracker (Emotiscope pattern) | Every hop | Create | Maintain last valid BPM |
| O-7 | ControlBusFrame | ControlBus state machine | Every hop | Create | N/A (must succeed) |
| O-8 | SnapshotBuffer update | Audio pipeline | After ControlBusFrame ready | Create | Overwrite previous snapshot |
| O-9 | Signal quality metrics | Audio pipeline | Every hop | Create | Log warning if out of range |
| O-10 | Verification test results | Test harness | On CI/CD trigger | Observe (app is under test) | Test harness reports FAIL |

**Source:** `planning/audio-pipeline-redesign/prd.md` §7.1, User Q1, Q3, Q5

### §7.2 External System Dependencies

| ID | External System | What It Creates | How App Knows | Failure Handling | Recovery |
|----|-----------------|-----------------|---------------|------------------|----------|
| E-1 | I2S DMA | Audio sample buffer (128 samples) | DMA interrupt / polling | Skip hop, log error | Wait for next DMA complete |
| E-2 | FreeRTOS | Task scheduling on Core 0 | Tick callback | Watchdog timeout | System reset |
| E-3 | SPH0645 Microphone | Analog audio signal → digital samples | I2S data valid | Silent samples | Log warning, continue |
| E-4 | Visual Pipeline (Core 1) | LED render frame | Consumes SnapshotBuffer | Stale frame warning | Use previous audio frame |
| E-5 | Domain Expert | Validation criteria for signal quality | Human review of spec | **CURRENTLY BLOCKED** | Document gap, continue with best-effort metrics |

**Source:** `planning/audio-pipeline-redesign/prd.md` §7.2, User Q1, Q3

### §7.3 Derived Ownership Rules

| Source | Rule | Rationale |
|--------|------|----------|
| O-1 | Audio pipeline must NOT generate synthetic samples | I2S DMA is sole source of truth |
| O-2 | Audio pipeline must NOT skip DC offset correction | Mic has fixed DC bias that corrupts DSP |
| O-4 | Visual pipeline must NOT access raw Goertzel bins | Contract layer is the boundary |
| O-7 | Beat tracker must NOT modify ControlBusFrame directly | ControlBus owns the contract |
| O-9 | Signal quality metrics must NOT be silently ignored | User Q1 requires visibility |
| E-5 | Implementation must NOT invent signal quality thresholds | Domain expertise required (User Q3) |

**Source:** `planning/audio-pipeline-redesign/prd.md` §7.3, User Q1, Q3

---

## §8. State Requirements

### §8.1 State Isolation

| ID | Requirement | Rationale | Enforcement |
|----|-------------|-----------|-------------|
| SI-1 | Audio pipeline state isolated to Core 0 | Dual-core architecture, lock-free communication | SnapshotBuffer for cross-core |
| SI-2 | Beat tracker state internal to TempoTracker | Encapsulation, testability | Private members |
| SI-3 | Goertzel coefficients immutable after init | Prevent runtime drift | const arrays |
| SI-4 | Signal quality metrics isolated to audio pipeline | Observability without side effects | Read-only telemetry |
| SI-5 | Verification tests isolated from runtime | Tests must not affect production behavior | Separate test harness |

**Source:** `planning/audio-pipeline-redesign/prd.md` §8.1, User Q2, Q3, Q5

### §8.2 State Lifecycle

| ID | State | Initial | Created When | Cleared When | Persists Across |
|----|-------|---------|--------------|--------------|------------------|
| SL-1 | Goertzel coefficients | From Sensory Bridge reference | AudioNode::onStart() | Never (const) | Forever |
| SL-2 | Beat tracker PLL state | Emotiscope 2.0 defaults | TempoTracker::init() | resetDspState() | Audio session |
| SL-3 | ControlBus smoothing state | Zero | ControlBus::Reset() | ControlBus::Reset() | Audio session |
| SL-4 | Ring buffer samples | Zero | First hop | Overwritten continuously | Rolling window |
| SL-5 | DC offset calibration | Default value | AudioNode::calibrateDCOffset() | Never (persists) | Power cycle |
| SL-6 | Signal quality history | Zero | First hop | resetDspState() | Audio session |
| SL-7 | Verification test state | Clean slate | Test harness init | After each test | Test run |

**Source:** `planning/audio-pipeline-redesign/prd.md` §8.2, User Q1, Q3, Q5

---

## §9. Technical Considerations

### §9.1 Architecture Decisions

| ID | Decision | Rationale | Source |
|----|----------|-----------|--------|
| AD-1 | Use Goertzel, not FFT, for frequency analysis | Musically-aligned bins, variable window, lower memory | User Q1, Q2, Sensory Bridge |
| AD-2 | Remove FFT from main DSP feed entirely | FFT was accidental contamination | User Q1 |
| AD-3 | 4-layer spec for all components | Prevent agent drift that corrupted previous implementation | User Q5, prd.md §9.1 |
| AD-4 | Phase 1 = Audio + Contract only | Avoid scope creep, get foundation right first | User Q4, prd.md §9.1 |
| AD-5 | Audio pipeline is source of truth | Contract and visual pipeline adapt to audio output | prd.md §9.1 |
| AD-6 | Incremental build + parallel multi-agent swarm | Build Goertzel → test → Beat tracking → test → Contract | User Q5 |
| AD-7 | Automated tests first, expert validation second | Hierarchy: tests catch drift → expert validates quality | User Q3, Q5 |
| AD-8 | Expose signal quality metrics at every stage | User Q1 requires ZERO blind spots in audio chain | User Q1 |
| AD-9 | Hard deadlines for core, iterative for advanced | Core blocks commercial deployment (mission-critical) | User Q4 |

**Source:** `planning/audio-pipeline-redesign/prd.md` §9.1, User Q1, Q2, Q3, Q4, Q5

### §9.2 Canonical Reference Files

**Sensory Bridge 4.1.1 (Goertzel/Frequency):**
```
/Users/spectrasynq/Workspace_Management/Software/K1.node1/references/
  Sensorybridge.sourcecode/SensoryBridge-4.1.1/SENSORY_BRIDGE_FIRMWARE/
  ├── constants.h      # DSP parameters, bin frequencies
  ├── GDFT.h           # Goertzel DFT implementation
  ├── globals.h        # State management patterns
  └── i2s_audio.h      # Audio capture patterns
```

**Emotiscope 2.0 (Beat Tracking):**
```
/Users/spectrasynq/Workspace_Management/Software/K1.node1/references/
  Emotiscope.sourcecode/Emotiscope-2.0/main/
  └── [beat tracking implementation files]
```

**Source:** `planning/audio-pipeline-redesign/prd.md` §9.2

### §9.3 Integration Points

| Current File | Action | Reference | Phase |
|--------------|--------|-----------|-------|
| `src/audio/AudioNode.cpp` | Rewrite processHop() | Sensory Bridge pattern | Phase 1 |
| `src/audio/GoertzelAnalyzer.cpp` | Replace with Sensory Bridge GDFT | GDFT.h | Phase 1 |
| `src/audio/tempo/TempoTracker.cpp` | Replace entirely with Emotiscope | Emotiscope 2.0 | Phase 1 |
| `src/audio/contracts/ControlBus.h` | Update fields to canonical output | New design | Phase 1 |
| `src/audio/k1/` | **REMOVE** - K1 FFT path | Delete | Phase 1 |
| Visual effects (45+ files) | Update to consume new contract | New ControlBusFrame | Phase 2 (iterative) |

**Source:** `planning/audio-pipeline-redesign/prd.md` §9.3, User Q4

### §9.4 Constraints

| ID | Constraint | Impact | Source |
|----|------------|--------|--------|
| C-1 | 16ms hop period (62.5 Hz) | All processing must complete within budget | User Q2, MAGIC_NUMBERS.md |
| C-2 | <14ms processing budget @ 240MHz | 6ms DSP + 2ms margin for OS jitter = 1.92M cycles | User Q2, MAGIC_NUMBERS.md |
| C-3 | ~20KB RAM budget for audio | No large intermediate buffers | User Q2, technical_blueprint.md |
| C-4 | ~50KB Flash budget for audio code | Minimal code size | User Q2, technical_blueprint.md |
| C-5 | ESP32-S3 @ 240MHz, dual-core | Fixed CPU speed, cannot overclock | User Q2 |
| C-6 | SPH0645 I2S mic @ 16kHz | Fixed sample rate, cannot change | User Q2, MAGIC_NUMBERS.md |
| C-7 | Core 0 affinity for audio | Cannot move audio to Core 1 (rendering) | User Q2, prd.md §9.4 |
| C-8 | Lock-free cross-core communication | Must use SnapshotBuffer pattern only | User Q2, prd.md §9.4 |
| C-9 | No heap allocation in render loops | Performance requirement (120 FPS target) | User Q2, repo constraints |
| C-10 | Centre origin only (LEDs 79/80) | Hardware LGP design constraint | User Q2, repo constraints |
| C-11 | Mark DO NOT MODIFY on canonical code | Prevent future agent corruption | User Q2, prd.md §4.1 |
| C-12 | 16kHz/128-sample enforced | Non-negotiable DSP foundation | User Q2, MAGIC_NUMBERS.md |

**Source:** `planning/audio-pipeline-redesign/prd.md` §9.4, `MAGIC_NUMBERS.md`, User Q2

### §9.5 Expert Advice Gaps

**🚨 CRITICAL: The following require professional domain expertise:**

| Gap ID | Topic | Questions | Impact | Status |
|--------|-------|-----------|--------|--------|
| EXP-1 | SPH0645 Signal Quality | What SNR thresholds validate good signal @ 16kHz? | Cannot validate mic correctness | **BLOCKED - User not qualified (Q3)** |
| EXP-2 | SPL Range Validation | What dB SPL range should music/speech produce? | Cannot validate input level correctness | **BLOCKED - User not qualified (Q3)** |
| EXP-3 | Clipping Detection | What sample amplitude threshold indicates clipping? | Cannot detect audio overload | **BLOCKED - User not qualified (Q3)** |
| EXP-4 | Noise Floor Measurement | How to measure baseline noise floor for SNR calc? | Cannot validate quiet environment behavior | **BLOCKED - User not qualified (Q3)** |
| EXP-5 | Long-Term Stability | What drift/variance over time indicates problem? | Cannot validate production reliability | **BLOCKED - User not qualified (Q3)** |
| EXP-6 | Edge Case Coverage | What edge cases must be tested for robustness? | Risk missing critical failure modes | **BLOCKED - User not qualified (Q3)** |

**Mitigation Strategy:**
1. Implement best-effort signal quality metrics in Phase 1
2. Document all assumptions and limitations
3. Flag EXP-1 through EXP-6 for domain expert review
4. Iterate validation criteria in Phase 2 after expert guidance

**Source:** User Q1, Q3

---

## §10. Success Metrics

| ID | Metric | Target | Measurement Method | Phase |
|----|--------|--------|--------------------|-------|
| M-1 | Goertzel bin frequency match | 100% match to Sensory Bridge | Automated comparison test | Phase 1 |
| M-2 | Goertzel windowing match | 100% match to Sensory Bridge GDFT | Window output comparison test | Phase 1 |
| M-3 | Beat tracking algorithm match | 100% match to Emotiscope 2.0 | Code review + line-by-line diff | Phase 1 |
| M-4 | BPM accuracy | ±1 BPM of ground truth | Test with known-BPM audio samples | Phase 1 |
| M-5 | Agent drift detection | 0 undetected deviations | Verification test suite (100% coverage) | Phase 1 |
| M-6 | Specification coverage | 4 layers for all components | Documentation audit | Phase 1 |
| M-7 | Timing budget compliance | <14ms processing per hop | Runtime profiling | Phase 1 |
| M-8 | Memory budget compliance | ~20KB RAM for audio | Build-time analysis | Phase 1 |
| M-9 | Signal quality visibility | All stages instrumented | Telemetry inspection | Phase 1 |
| M-10 | **Signal quality validation** | **Thresholds TBD by expert** | **EXPERT ADVICE NEEDED (EXP-1 to EXP-6)** | Phase 2 |
| M-11 | CI/CD test reliability | 100% reproducible test results | CI/CD pipeline execution | Phase 1 |
| M-12 | Commercial deployment readiness | Zero crashes, predictable behavior | User acceptance testing | Phase 1 completion |

**Source:** `planning/audio-pipeline-redesign/prd.md` §10, User Q1, Q3, Q4, Q5

---

## §11. Implementation Strategy

### §11.1 Phased Approach

**Phase 1 (Hard Deadlines - Blocking Commercial Deployment):**
1. **Goertzel Implementation** (Sensory Bridge pattern)
2. **Beat Tracking** (Emotiscope pattern)
3. **Contract Layer** (ControlBusFrame redesign)
4. **Signal Quality Instrumentation** (telemetry at all stages)
5. **Automated Verification Tests** (first line of defense)
6. **4-Layer Specification Documentation** (prevent future drift)

**Phase 2 (Iterative):**
1. **Domain Expert Validation** (EXP-1 to EXP-6 criteria)
2. **Visual Pipeline Migration** (45+ effects to new contract)
3. **Advanced Features** (edge case handling, optimization)
4. **Long-Term Stability Testing** (production validation)

**Source:** User Q4, Q5

### §11.2 Parallel Multi-Agent Swarm Execution

**Cadence:** Build → Test → Commit (per component)

**Agent Assignment:**
- **Agent 1:** Goertzel implementation (Sensory Bridge port)
- **Agent 2:** Beat tracking implementation (Emotiscope port)
- **Agent 3:** Contract layer redesign (ControlBusFrame)
- **Agent 4:** Signal quality instrumentation
- **Agent 5:** Verification test suite
- **Agent 6:** Documentation (4-layer spec)

**Synchronization Points:**
- After Goertzel complete: Agents 3, 4 can consume Goertzel output
- After Beat Tracking complete: Agent 3 can integrate tempo data
- After Contract complete: Agent 5 can write contract validation tests
- Agent 6 runs continuously: Document all changes in real-time

**Source:** User Q5

### §11.3 Validation Hierarchy

**First Line (Automated Tests):**
- Run after every component completion
- Must PASS before moving to next component
- Catch drift from canonical references immediately

**Second Line (Domain Expert):**
- Review signal quality metrics against EXP-1 to EXP-6 criteria
- Validate SPH0645 @ 16kHz performance
- Approve commercial deployment readiness

**Source:** User Q3, Q5

---

## Appendix A: Answer Traceability

| User Answer | Summary | Captured In |
|-------------|---------|-------------|
| Q1 Core Requirements | ZERO visibility into mic→DSP→contract chain, need bulletproof commercial-grade audio | §1.1, §2.1, §2.2 (PP-5, PP-6), §3, §4.1 (FR-7, FR-8), §5.3, §7.3, §9.1 (AD-8) |
| Q2 Technical Scope | ESP32-S3 @ 240MHz, 16kHz/128-sample/8ms, <14ms budget, ~20KB RAM, dual-core, lock-free | §4.1 (FR-3, FR-12), §5.1, §5.2, §8.1, §9.4 (C-1 to C-12) |
| Q3 Success Criteria | Automated tests first, domain expert second, NEED EXPERT ADVICE for signal quality | §4.1 (FR-8, FR-10), §4.2 (NFR-3), §5.7, §5.8, §7.2 (E-5), §9.5 (EXP-1 to EXP-6), §10 (M-10), §11.3 |
| Q4 Priority & Timeline | BLOCKING commercial deployment/CI/CD, hard deadlines for core, iterative for advanced | §1.3, §2.1 (impact chain), §2.2 (PP-7), §3, §9.1 (AD-4, AD-9), §9.3, §11.1 |
| Q5 Implementation Strategy | Incremental build with parallel multi-agent swarm, Build→Test→Commit cadence | §4.2 (NFR-1), §5.7, §8.1 (SI-5), §9.1 (AD-3, AD-6, AD-7), §11.2, §11.3 |

**Validation:** ✅ All 5 user answers have been captured across multiple sections. No information lost.

---

## Appendix B: Related Documents

| Document | Purpose | Read Order | Location |
|----------|---------|------------|----------|
| **AGENT_ONBOARDING.md** | **START HERE** - Mandatory checklist before ANY work | 1st | `planning/audio-pipeline-redesign/` |
| **HARDENED_SPEC.md** | Verbatim code extracts from canonical references (WHAT to implement) | 2nd | `planning/audio-pipeline-redesign/` |
| **MAGIC_NUMBERS.md** | Complete derivations for every constant (WHY these values) | 3rd | `planning/audio-pipeline-redesign/` |
| **prd.md (existing)** | Original comprehensive PRD with §1-§11 | 4th | `planning/audio-pipeline-redesign/` |
| **prd.md (this file)** | Context Engineer synthesized PRD (enhanced traceability) | 4th | `context_engineering_runs/2026-01-12_audio_pipeline/` |
| **technical_blueprint.md** | Architecture diagrams, state transitions, file mapping | 5th | `planning/audio-pipeline-redesign/` |
| **tasks.md** | 6-phase implementation task breakdown with dependencies | 6th | `planning/audio-pipeline-redesign/` |

**Document Flow:**
```
AGENT_ONBOARDING.md (context)
    → HARDENED_SPEC.md (what to build)
    → MAGIC_NUMBERS.md (why these values)
    → prd.md (why this design)
    → technical_blueprint.md (architecture)
    → tasks.md (work items)
```

**Source:** `planning/audio-pipeline-redesign/prd.md` Appendix C

---

## Appendix C: Expert Advice Protocol

**When Domain Expertise Required:**

1. **Identify Gap:** Document specific question (EXP-1 to EXP-6 format)
2. **Flag Explicitly:** Mark as "**EXPERT ADVICE NEEDED**" in all relevant sections
3. **Document Assumptions:** If implementing without expert guidance, document ALL assumptions
4. **Request Guidance:** Provide context, constraints, and specific questions
5. **Iterate:** Update validation criteria when expert input received

**Current Expert Advice Requests:**

| Gap ID | Section | Question | Status |
|--------|---------|----------|--------|
| EXP-1 | §4.1 (FR-8), §5.8.1, §9.5 | SPH0645 SNR validation thresholds @ 16kHz? | **PENDING** |
| EXP-2 | §5.8.2, §9.5 | SPL range validation for music/speech? | **PENDING** |
| EXP-3 | §5.8.3, §9.5 | Clipping detection threshold? | **PENDING** |
| EXP-4 | §5.8.4, §9.5 | Noise floor measurement methodology? | **PENDING** |
| EXP-5 | §5.8.5, §9.5 | Long-term stability validation? | **PENDING** |
| EXP-6 | §5.8.6, §9.5 | Edge case coverage requirements? | **PENDING** |

**Source:** User Q1, Q3, Q5

---

## Appendix D: Agent Instructions

**FOR ALL FUTURE AI AGENTS (INCLUDING PARALLEL SWARM):**

1. **READ HARDENED_SPEC.md FIRST** - Contains VERBATIM CODE you must implement
2. **READ THIS PRD SECOND** - Explains WHY the spec is designed this way
3. **DO NOT** invent implementations - use canonical references (Sensory Bridge, Emotiscope)
4. **DO NOT** modify lines marked "DO NOT MODIFY" without explicit user approval
5. **RUN** verification tests after ANY change - they must PASS
6. **ASK** if unsure whether a change aligns with this specification
7. **DOCUMENT** changes with reference to this PRD section numbers (§X.Y)
8. **FLAG** when domain expertise needed (EXP-1 to EXP-6 format)
9. **COMMIT** after each component passes tests (Build→Test→Commit cadence)
10. **SYNC** with other agents at synchronization points (§11.2)

**RED FLAGS - If thinking these thoughts, STOP:**
- "I'll just improve this a bit" → That's how implementation got corrupted
- "This could be better if..." → Canonical implementations are battle-tested
- "I'll add an optimization" → Sensory Bridge runs on weaker hardware than ESP32-S3
- "I don't understand why they did it this way" → Read canonical source comments
- "I'll invent signal quality thresholds" → Domain expertise required (EXP-1 to EXP-6)

**THE HARDENED_SPEC.md IS YOUR IMPLEMENTATION GUIDE. PORT IT FAITHFULLY.**

**Source:** `planning/audio-pipeline-redesign/prd.md` Appendix D

---

*Document Version: 1.0 (Context Engineer Synthesis)*  
*Created: 2026-01-12*  
*Synthesized From: planning/audio-pipeline-redesign/* comprehensive documentation*  
*Status: READY FOR PARALLEL MULTI-AGENT IMPLEMENTATION*
