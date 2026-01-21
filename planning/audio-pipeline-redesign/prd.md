# Product Requirements Document: Audio Pipeline Redesign

> **Traceability Note:** This PRD uses section numbers (§1-§10) that are referenced by the Technical Blueprint and Task List. All user answers have been mapped to specific sections.

> **CRITICAL: This document is the CANONICAL SPECIFICATION for the LightwaveOS audio pipeline. Future agents MUST NOT deviate from this specification without explicit user approval.**

---

## §1. Overview & Vision

### §1.1 Executive Summary

This PRD specifies a **complete redesign** of the LightwaveOS v2 audio pipeline, replacing the current corrupted implementation with a clean, well-documented architecture based on two canonical references:

1. **Sensory Bridge 4.1.1** - For Goertzel/frequency analysis (musically-aligned bins, proper windowing)
2. **Emotiscope 2.0** - For beat/tempo tracking (onset detection, PLL phase locking)

The current audio pipeline has been corrupted by **agent session discontinuity** - where multiple AI agents, lacking persistent context, made incremental changes that drifted from the original design intent. This resulted in:
- FFT code accidentally pulled into the main DSP feed (was only meant for failed beat tracking experiment)
- Beat tracking implementation that's 80% correct but 20% broken
- Multiple overlapping analysis paths (legacy Goertzel, K1 FFT, dual-bank) creating confusion

### §1.2 Key Outcomes

- **O-1**: Clean audio pipeline based 100% on Sensory Bridge 4.1.1 Goertzel implementation
- **O-2**: Working beat tracking based 100% on Emotiscope 2.0 tempo detection
- **O-3**: 4-layer specification (Algorithm → Pseudocode → Reference Code → Tests) preventing future agent drift
- **O-4**: Updated contract layer (ControlBusFrame) exposing canonical audio features to visual pipeline
- **O-5**: Documentation so thorough that no future agent can accidentally corrupt the implementation

### §1.3 Scope

**Phase 1 (This PRD):**
- Audio pipeline redesign
- Contract layer (ControlBusFrame) update
- 4-layer specification documentation
- Verification tests

**Phase 2 (Future):**
- Visual pipeline updates (45+ effects)
- Not in scope for this PRD

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

### §2.2 Current Pain Points

| ID | Pain Point | Impact | Source |
|----|------------|--------|--------|
| PP-1 | FFT accidentally in main DSP feed | Duplicate processing, architectural confusion | User Answer Q1 |
| PP-2 | Beat tracking 80% correct, 20% agent-invented | Unreliable tempo detection | User Answer Q5 |
| PP-3 | Multiple overlapping analysis paths | CPU waste, maintenance burden | User Answer Q1 |
| PP-4 | No specification to prevent drift | Future agents will corrupt again | User Answer Q6 |
| PP-5 | Agents don't understand WHAT/HOW/WHY | Implementation diverges from intent | User Answer Q2 |

### §2.3 Why This Requires a Full Redesign

The current codebase cannot be incrementally fixed because:
1. No clear boundary between "correct" and "corrupted" code
2. Agent-invented code is interleaved with reference implementations
3. Multiple attempts to fix beat tracking have made it worse
4. The only solution is to start fresh from canonical references

---

## §3. Target Users

| User Type | Needs | Key Actions |
|-----------|-------|-------------|
| Future AI Agents | Unambiguous specification they cannot misinterpret | Read spec, implement exactly, run verification tests |
| Human Developers | Understanding of design intent and constraints | Review spec, audit agent implementations |
| The Audio Pipeline | Correctly processed audio features for visual effects | Receive I2S audio, produce ControlBusFrame |
| The Visual Pipeline | Reliable, well-documented audio contract | Consume ControlBusFrame, drive LED effects |

---

## §4. Core Requirements

### §4.1 Functional Requirements

| ID | Requirement | Priority | Source | Spec Layer |
|----|-------------|----------|--------|------------|
| FR-1 | Implement Goertzel frequency analysis exactly as Sensory Bridge 4.1.1 | CRITICAL | Q1, Q7 | Line-level |
| FR-2 | Use 64/96 musically-aligned bins (semitone-spaced, not arbitrary FFT bins) | CRITICAL | Q1 | Algorithm |
| FR-3 | Use 16kHz sample rate, 128-sample chunks, 8ms window | CRITICAL | Q1 | Algorithm |
| FR-4 | Implement proper windowing (Sensory Bridge pattern) | CRITICAL | Q1 | Line-level |
| FR-5 | Implement beat tracking exactly as Emotiscope 2.0 | CRITICAL | Q5, Q7 | Line-level |
| FR-6 | Remove all FFT code from main audio DSP feed | CRITICAL | Q1 | Architecture |
| FR-7 | Define new ControlBusFrame contract based on canonical output | HIGH | Q9 | Function-level |
| FR-8 | Create verification tests that catch implementation drift | HIGH | Q6 | Tests |
| FR-9 | Document WHY each design decision was made | HIGH | Q2 | Algorithm |
| FR-10 | Mark sacred code sections as "DO NOT MODIFY" | HIGH | Q6 | Line-level |

### §4.2 Non-Functional Requirements

| ID | Category | Requirement | Source |
|----|----------|-------------|--------|
| NFR-1 | Maintainability | 4-layer specification for every component | Q6 |
| NFR-2 | Correctness | 100% match to canonical references (no agent inventions) | Q5 |
| NFR-3 | Verifiability | Automated tests that fail on drift from reference | Q6 |
| NFR-4 | Documentation | Every function has WHAT/HOW/WHY documented | Q2 |
| NFR-5 | Separation | Audio pipeline produces, contract exposes, visual consumes | Q3, Q9 |

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

#### Story §5.2: Proper Windowing Implementation
**As a** future AI agent implementing Goertzel analysis
**I want** line-level reference code for the windowing function
**So that** I cannot accidentally implement a different window

**Acceptance Criteria:**
- [ ] §5.2.1: Window function matches Sensory Bridge `GDFT.h` exactly
- [ ] §5.2.2: Window coefficients are hardcoded (not computed) to prevent drift
- [ ] §5.2.3: Verification test compares window output to reference output
- [ ] §5.2.4: Code section marked "// FROM SENSORY BRIDGE 4.1.1 - DO NOT MODIFY"

### Epic 2: Beat Tracking (Emotiscope Pattern)

#### Story §5.3: Onset Detection
**As a** future AI agent implementing beat tracking
**I want** the complete Emotiscope 2.0 onset detection algorithm
**So that** I cannot accidentally invent my own version

**Acceptance Criteria:**
- [ ] §5.3.1: Onset detection matches Emotiscope 2.0 `main/` implementation exactly
- [ ] §5.3.2: Threshold calculation uses reference formula (not agent interpretation)
- [ ] §5.3.3: All magic numbers are documented with their source
- [ ] §5.3.4: Verification test with known input produces known output

#### Story §5.4: PLL Phase Tracking
**As a** future AI agent implementing tempo tracking
**I want** the exact PLL implementation from Emotiscope 2.0
**So that** the 20% that was previously corrupted is now correct

**Acceptance Criteria:**
- [ ] §5.4.1: PLL gains match Emotiscope 2.0 values exactly
- [ ] §5.4.2: Phase correction logic is verbatim from reference
- [ ] §5.4.3: State machine transitions match reference
- [ ] §5.4.4: Verification test confirms BPM output within ±1 BPM of reference

### Epic 3: Contract Layer

#### Story §5.5: ControlBusFrame Redesign
**As a** the visual pipeline
**I want** a clean contract that exposes canonical audio features
**So that** I receive only correct, documented data

**Acceptance Criteria:**
- [ ] §5.5.1: ControlBusFrame contains only fields produced by canonical pipeline
- [ ] §5.5.2: Each field is documented with its source (Goertzel bin X, tempo phase, etc.)
- [ ] §5.5.3: No legacy/corrupted fields remain in the contract
- [ ] §5.5.4: Contract update does not break visual pipeline (Phase 2 handles that)

---

## §6. User Experience Contract

*Note: For embedded firmware, "user" is the consuming system (visual pipeline) not a human.*

### §6.1 User Visibility Specification

| ID | Consumer Action | Consumer Sees | Consumer Does NOT See | Timing |
|----|-----------------|---------------|----------------------|--------|
| V-1 | Read ControlBusFrame | Smoothed frequency bands (0.0-1.0) | Raw Goertzel magnitudes, windowing internals | Every 16ms hop |
| V-2 | Read tempo output | BPM (60-300), phase (0.0-1.0), confidence | PLL internal state, onset buffer | Every hop |
| V-3 | Read beat tick | Boolean beat_tick flag | Onset detection internals, threshold calculations | On beat |

### §6.2 Timing & Feedback Expectations

| ID | Event | Expected Timing | Success Indicator | Failure Indicator |
|----|-------|-----------------|-------------------|-------------------|
| T-1 | Audio hop captured | Every 8ms (128 samples @ 16kHz) | Fresh samples in buffer | CaptureResult error |
| T-2 | Goertzel analysis complete | Within 16ms hop period | Bins updated | Analysis timeout |
| T-3 | Beat tracking update | Every hop | Phase advances smoothly | BPM jumps > 10 BPM |
| T-4 | ControlBusFrame published | Every hop | SnapshotBuffer updated | Stale frame detected |

---

## §7. Artifact Ownership

### §7.1 Creation Responsibility

| ID | Artifact | Created By | When | App's Role |
|----|----------|------------|------|------------|
| O-1 | Raw audio samples | I2S DMA hardware | Every 8ms | Observe (read from buffer) |
| O-2 | Goertzel bin magnitudes | Audio pipeline | After windowing + Goertzel | Create |
| O-3 | Beat/tempo output | Beat tracker (Emotiscope pattern) | Every hop | Create |
| O-4 | ControlBusFrame | ControlBus state machine | Every hop | Create |
| O-5 | SnapshotBuffer update | Audio pipeline | After ControlBusFrame ready | Create |

### §7.2 External System Dependencies

| ID | External System | What It Creates | How App Knows | Failure Handling |
|----|-----------------|-----------------|---------------|------------------|
| E-1 | I2S DMA | Audio sample buffer | DMA interrupt / polling | Skip hop, log error |
| E-2 | FreeRTOS | Task scheduling | Tick callback | Watchdog timeout |

### §7.3 Derived Ownership Rules

| Source | Rule | Rationale |
|--------|------|----------|
| O-1 | Audio pipeline must NOT generate synthetic samples | I2S DMA is sole source of truth |
| O-2 | Visual pipeline must NOT access raw Goertzel bins | Contract layer is the boundary |
| O-4 | Beat tracker must NOT modify ControlBusFrame directly | ControlBus owns the contract |

---

## §8. State Requirements

### §8.1 State Isolation

| ID | Requirement | Rationale | Enforcement |
|----|-------------|-----------|-------------|
| SI-1 | Audio pipeline state isolated to Core 0 | Dual-core architecture, lock-free communication | SnapshotBuffer for cross-core |
| SI-2 | Beat tracker state internal to TempoTracker | Encapsulation, testability | Private members |
| SI-3 | Goertzel coefficients immutable after init | Prevent runtime drift | const arrays |

### §8.2 State Lifecycle

| ID | State | Initial | Created When | Cleared When | Persists Across |
|----|-------|---------|--------------|--------------|------------------|
| SL-1 | Goertzel coefficients | From Sensory Bridge reference | AudioNode::onStart() | Never (const) | Forever |
| SL-2 | Beat tracker PLL state | Emotiscope 2.0 defaults | TempoTracker::init() | resetDspState() | Audio session |
| SL-3 | ControlBus smoothing state | Zero | ControlBus::Reset() | ControlBus::Reset() | Audio session |
| SL-4 | Ring buffer samples | Zero | First hop | Overwritten continuously | Rolling window |

---

## §9. Technical Considerations

### §9.1 Architecture Decisions

| ID | Decision | Rationale | Source |
|----|----------|-----------|--------|
| AD-1 | Use Goertzel, not FFT, for frequency analysis | Musically-aligned bins, variable window, lower memory | Q1, Sensory Bridge |
| AD-2 | Remove FFT from main DSP feed entirely | FFT was accidental contamination | Q1 |
| AD-3 | 4-layer spec for all components | Prevent agent drift that corrupted previous implementation | Q6 |
| AD-4 | Phase 1 = Audio + Contract only | Avoid scope creep, get foundation right | Q10 |
| AD-5 | Audio pipeline is source of truth | Contract and visual pipeline adapt to audio output | Q9 |

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

### §9.3 Integration Points

| Current File | Action | Reference |
|--------------|--------|-----------|
| `src/audio/AudioNode.cpp` | Rewrite processHop() | Sensory Bridge pattern |
| `src/audio/GoertzelAnalyzer.cpp` | Replace with Sensory Bridge GDFT | GDFT.h |
| `src/audio/tempo/TempoTracker.cpp` | Replace entirely with Emotiscope | Emotiscope 2.0 |
| `src/audio/contracts/ControlBus.h` | Update fields to canonical output | New design |
| `src/audio/k1/` | REMOVE - K1 FFT path | Delete |

### §9.4 Constraints

| ID | Constraint | Impact |
|----|------------|--------|
| C-1 | 16ms hop period (62.5 Hz) | All processing must complete within budget |
| C-2 | ESP32-S3 memory limits | No large intermediate buffers |
| C-3 | Core 0 affinity for audio | Cannot move audio to Core 1 |
| C-4 | Lock-free cross-core | Must use SnapshotBuffer pattern |

---

## §10. Success Metrics

| ID | Metric | Target | Measurement Method |
|----|--------|--------|--------------------|
| M-1 | Goertzel bin match | 100% match to Sensory Bridge | Automated comparison test |
| M-2 | Beat tracking match | 100% match to Emotiscope 2.0 | Known-input test vectors |
| M-3 | BPM accuracy | ±1 BPM of ground truth | Test with known-BPM audio |
| M-4 | Agent drift detection | 0 undetected deviations | Verification test suite |
| M-5 | Specification coverage | 4 layers for all components | Documentation audit |

---

## §11. 4-Layer Specification Requirement

Every component in the audio pipeline MUST have all 4 layers documented:

### Layer 1: Algorithm Specification
- High-level description of WHAT the component does
- WHY it's designed this way (not another way)
- References to canonical source

### Layer 2: Function-Level Pseudocode
- Input/output for each function
- Logic flow in language-agnostic pseudocode
- Edge cases and error handling

### Layer 3: Line-Level Reference Code
- Actual C++ code from canonical reference
- Marked with "// FROM [SOURCE] - DO NOT MODIFY"
- Magic numbers documented with source

### Layer 4: Verification Tests
- Test vectors with known input → expected output
- Automated tests that fail on drift
- Comparison tests against reference implementation

---

## Appendix A: Answer Traceability

| User Answer | Summary | Captured In |
|-------------|---------|-------------|
| Q1 | Original intent was Goertzel-only, FFT was accidental contamination | §2.1, §4.1 (FR-1 to FR-6), §9.1 (AD-1, AD-2) |
| Q2 | Real issue is agents not understanding WHAT/HOW/WHY | §2.2 (PP-5), §4.2 (NFR-4), §9.1 |
| Q3 | Two pipelines + contract, focus on audio only | §1.3, §3, §7.3 |
| Q4 | Redesign from scratch using Sensory Bridge | §1.1, §4.1 (FR-1), §9.2 |
| Q5 | Emotiscope 80% correct, 20% agent-invented | §2.1, §2.2 (PP-2), §5.3, §5.4 |
| Q6 | All 4 layers of specification required | §4.1 (FR-8, FR-10), §4.2 (NFR-1 to NFR-3), §11 |
| Q7 | Sensory Bridge for Goertzel, Emotiscope for beat | §9.2 |
| Q8 | Emotiscope 2.0 path provided | §9.2 |
| Q9 | Audio pipeline is source of truth | §9.1 (AD-5), §7.3 |
| Q10 | Phase 1 = Audio + Contract | §1.3, §9.1 (AD-4) |
| Q11 | Full spec package required | §11, §10 |

**Validation:** ✅ All 11 user answers have been captured. No information lost.

---

## Appendix B: Files to Remove

The following files/directories should be DELETED as part of the redesign:

```
src/audio/k1/                    # K1 FFT path - accidental contamination
src/audio/K1_GoertzelTables_16k.h  # Wrong approach
src/audio/fft/                   # FFT should not be in main DSP feed
```

---

## Appendix C: Related Documents

| Document | Purpose | Read Order |
|----------|---------|------------|
| **AGENT_ONBOARDING.md** | **START HERE** - Mandatory checklist before ANY work. Ensures context. | 1st |
| **HARDENED_SPEC.md** | Verbatim code extracts from canonical references. WHAT to implement. | 2nd |
| **MAGIC_NUMBERS.md** | Complete derivations for every constant. WHY these values. | 3rd |
| prd.md (this file) | Problem statement, requirements, constraints. WHY this design. | 4th |
| technical_blueprint.md | Architecture diagrams, state transitions, file mapping. | 5th |
| tasks.md | Implementation task breakdown with dependencies. | 6th |

**Document Flow:**
```
AGENT_ONBOARDING.md (context)
    → HARDENED_SPEC.md (what to build)
    → MAGIC_NUMBERS.md (why these values)
    → prd.md (why this design)
    → technical_blueprint.md (architecture)
    → tasks.md (work items)
```

---

## Appendix D: Agent Instructions

**FOR ALL FUTURE AI AGENTS WORKING ON THIS CODEBASE:**

1. **READ HARDENED_SPEC.md FIRST** - It contains the VERBATIM CODE you must implement
2. **READ THIS PRD SECOND** - It explains WHY the spec is designed this way
3. **DO NOT** invent your own implementations - use the canonical references
4. **DO NOT** modify lines marked "DO NOT MODIFY" without explicit user approval
5. **RUN** verification tests after any change - they must pass
6. **ASK** if you're unsure whether a change aligns with this specification
7. **DOCUMENT** your changes with reference to this PRD section numbers

**RED FLAGS - If you're thinking these thoughts, STOP:**
- "I'll just improve this a bit" → That's how the implementation got corrupted
- "This could be better if..." → The canonical implementations are battle-tested
- "I'll add an optimization" → Sensory Bridge runs on weaker hardware than ESP32-S3
- "I don't understand why they did it this way" → Read the canonical source comments

**THE HARDENED_SPEC.md IS YOUR IMPLEMENTATION GUIDE. PORT IT FAITHFULLY.**

---

*Document Version: 1.1*
*Created: 2026-01-12*
*Updated: 2026-01-12 - Added HARDENED_SPEC.md reference*
*Status: READY FOR IMPLEMENTATION*
