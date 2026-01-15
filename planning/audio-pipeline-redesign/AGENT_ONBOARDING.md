# Agent Onboarding Checklist: Audio Pipeline

**Version:** 1.0.0
**Status:** MANDATORY PROTOCOL
**Last Updated:** 2026-01-12

---

## Purpose

This checklist ensures every AI agent working on the LightwaveOS audio pipeline has the necessary context before making any changes. **Skipping steps will result in architectural drift.**

**TIME INVESTMENT:** ~15 minutes to complete checklist, saves hours of debugging drift.

---

# PRE-WORK CHECKLIST

## Phase 1: Document Reading (Required)

Complete these steps IN ORDER. Check each box before proceeding.

### 1.1 Core Documents

| # | Document | Purpose | Est. Time | Verified |
|---|----------|---------|-----------|----------|
| 1 | [ ] **HARDENED_SPEC.md** | Verbatim code to implement | 10 min | |
| 2 | [ ] **MAGIC_NUMBERS.md** | Derivations for all constants | 5 min | |
| 3 | [ ] **prd.md** (§1-§4) | Why this design exists | 5 min | |
| 4 | [ ] **technical_blueprint.md** (§1-§3) | Architecture diagrams | 5 min | |
| 5 | [ ] **tasks.md** (Quick Reference) | Ownership & boundaries | 3 min | |

### 1.2 Verification Questions

After reading, you MUST be able to answer these questions. If you cannot, re-read the relevant document.

**From HARDENED_SPEC.md:**
- [ ] Q1: What is the Goertzel coefficient formula? → `coeff = 2 × cos(2π × k / N)`
- [ ] Q2: Why does phase wrap at PI instead of 2×PI? → Half-beat tracking (downbeat/offbeat)
- [ ] Q3: What is the smoothing time constant at 50 Hz? → ~0.79 seconds (39.5 frames)
- [ ] Q4: Why Hamming window (0.54) instead of Hann (0.5)? → Better sidelobe suppression

**From MAGIC_NUMBERS.md:**
- [ ] Q5: Why 64 frequency bins? → 5+ octaves of semitone resolution (A1-C7)
- [ ] Q6: Why 16 kHz sample rate? → PDM mic optimal rate on ESP32-S3
- [ ] Q7: What is Q14 fixed-point? → Multiply by 16384, enables integer-only DSP
- [ ] Q8: Why 96 tempo bins? → 1 BPM resolution from 60-156 BPM

**From prd.md:**
- [ ] Q9: What caused the current corruption? → Agent session discontinuity
- [ ] Q10: What are the two canonical references? → Sensory Bridge 4.1.1 + Emotiscope 2.0

---

## Phase 2: Context Verification

### 2.1 Confirm Understanding

Before writing ANY code, verbally confirm:

```
I understand that:
- [ ] The current codebase is CORRUPTED and being redesigned
- [ ] I must PORT verbatim from canonical references, not invent
- [ ] Lines marked "DO NOT MODIFY" are load-bearing
- [ ] All magic numbers have documented derivations
- [ ] Changes must pass verification tests
- [ ] Phase wrapping at PI (not 2×PI) is intentional
- [ ] FFT should NOT be in the main DSP feed
- [ ] The audio pipeline is source of truth (not visual)
```

### 2.2 Red Flag Recognition

You MUST recognize these thoughts as drift indicators:

| Thought Pattern | Reality Check |
|-----------------|---------------|
| "I'll just improve this a bit" | STOP - that's how we got here |
| "This could be optimized" | Sensory Bridge runs on weaker hardware |
| "I don't understand why" | Read the source comments |
| "Let me add a feature" | Not in spec = not implemented |
| "This seems redundant" | It's probably load-bearing |
| "I'll use FFT instead" | NO - Goertzel is intentional |
| "I'll round these values" | NO - precision is required |
| "I'll change the smoothing" | NO - these are tuned constants |

---

## Phase 3: Environment Verification

### 3.1 Build Verification

Before making changes, verify the build works:

```bash
# Run from repository root
pio run -e esp32dev_audio

# Expected: BUILD SUCCESS
# If fails: DO NOT proceed, fix build first
```

### 3.2 File Locations

Confirm you know where key files are:

| Component | Expected Location | Status |
|-----------|-------------------|--------|
| Goertzel implementation | `src/audio/GoertzelBank.cpp` | [ ] Found |
| Tempo tracking | `src/audio/tempo/TempoTracker.cpp` | [ ] Found |
| Control bus contract | `src/audio/ControlBusFrame.h` | [ ] Found |
| Audio node entry | `src/audio/AudioNode.cpp` | [ ] Found |

---

# DURING-WORK PROTOCOL

## Phase 4: Implementation Rules

### 4.1 Before Writing Code

For EVERY function you implement:

- [ ] Find the corresponding section in HARDENED_SPEC.md
- [ ] Copy the verbatim code extract
- [ ] Note the source annotation (e.g., "FROM Sensory Bridge GDFT.h:59-119")
- [ ] Add the "DO NOT MODIFY" comment

### 4.2 Code Annotation Template

Every critical function must have this header:

```cpp
/**
 * @brief [Description]
 *
 * FROM: [Canonical Source] [file:lines]
 * HARDENED_SPEC: Section [X.Y]
 *
 * DO NOT MODIFY without PRD approval.
 * Magic numbers documented in MAGIC_NUMBERS.md
 */
```

### 4.3 Change Tracking

For every change, log:

```markdown
## Change Log Entry

**Date:** [YYYY-MM-DD]
**Component:** [GoertzelBank / TempoTracker / etc.]
**Type:** [New / Modified / Removed]
**Source:** HARDENED_SPEC.md Section [X.Y]
**Verification:** [Test name that validates this change]

**Description:**
[What was changed and why]

**Canonical Reference:**
[File:line from Sensory Bridge or Emotiscope]
```

---

## Phase 5: Verification Protocol

### 5.1 After Every Change

Run verification tests:

```bash
# Unit tests
pio test -e native

# Build verification
pio run -e esp32dev_audio

# If either fails: REVERT and investigate
```

### 5.2 Verification Checklist

After completing a component:

- [ ] All functions have source annotations
- [ ] All magic numbers are documented
- [ ] Verification test exists and passes
- [ ] No FFT code in main DSP path
- [ ] No invented algorithms (only ported)
- [ ] Build succeeds without warnings

---

# POST-WORK CHECKLIST

## Phase 6: Completion Verification

### 6.1 Self-Audit

Before marking work complete:

- [ ] Re-read HARDENED_SPEC.md section for the component
- [ ] Compare your implementation line-by-line
- [ ] Verify all constants match MAGIC_NUMBERS.md
- [ ] Run full test suite
- [ ] Check for accidental FFT inclusion

### 6.2 Documentation Update

If you added anything:

- [ ] Update HARDENED_SPEC.md if new code patterns
- [ ] Update MAGIC_NUMBERS.md if new constants
- [ ] Update tasks.md with completion status
- [ ] Add to change log

### 6.3 Handoff Notes

Leave clear notes for next agent:

```markdown
## Session Handoff

**Completed:**
- [List of completed work with HARDENED_SPEC references]

**Verified:**
- [List of tests that pass]

**Next Steps:**
- [What should be done next, with spec references]

**Warnings:**
- [Any gotchas or non-obvious decisions]
```

---

# EMERGENCY PROCEDURES

## If You Suspect Drift

1. **STOP** immediately
2. **DO NOT** commit partial changes
3. **READ** HARDENED_SPEC.md for the component
4. **COMPARE** your code line-by-line with spec
5. **REVERT** if significant deviation found
6. **DOCUMENT** what went wrong

## If Tests Fail

1. **DO NOT** modify tests to pass
2. **READ** the test to understand what it validates
3. **COMPARE** your implementation with spec
4. **FIX** the implementation, not the test
5. **RE-RUN** tests after fix

## If You Don't Understand Something

1. **DO NOT** guess or improvise
2. **READ** the canonical source comments
3. **CHECK** MAGIC_NUMBERS.md for derivations
4. **ASK** the user for clarification if still unclear
5. **DOCUMENT** your question for future agents

---

# QUICK REFERENCE CARD

```
╔═══════════════════════════════════════════════════════════════╗
║              AUDIO PIPELINE ONBOARDING QUICK REF              ║
╠═══════════════════════════════════════════════════════════════╣
║                                                               ║
║  BEFORE CODING:                                               ║
║  1. Read HARDENED_SPEC.md (verbatim code)                     ║
║  2. Read MAGIC_NUMBERS.md (derivations)                       ║
║  3. Read prd.md §1-§4 (why this design)                       ║
║  4. Verify build: pio run -e esp32dev_audio                   ║
║                                                               ║
║  WHILE CODING:                                                ║
║  - Copy from HARDENED_SPEC, don't invent                      ║
║  - Add "DO NOT MODIFY" comments                               ║
║  - Reference source: "FROM [file:line]"                       ║
║  - Test after every component                                 ║
║                                                               ║
║  RED FLAGS (STOP IF YOU THINK):                               ║
║  - "I'll improve this" → Drift alert                          ║
║  - "I'll use FFT" → Architecture violation                    ║
║  - "I'll round this" → Precision violation                    ║
║  - "I don't get why" → Read source comments                   ║
║                                                               ║
║  CANONICAL SOURCES:                                           ║
║  - Goertzel: Sensory Bridge 4.1.1                             ║
║  - Tempo: Emotiscope 2.0                                      ║
║                                                               ║
║  KEY FORMULAS:                                                ║
║  - Goertzel coeff: 2×cos(2πk/N)                               ║
║  - Q14 scale: multiply by 16384                               ║
║  - Phase wrap: at PI (not 2×PI)                               ║
║  - Smoothing: 0.975 retain, 0.025 update                      ║
║                                                               ║
╚═══════════════════════════════════════════════════════════════╝
```

---

# CERTIFICATION

Before starting work, copy and complete this certification:

```markdown
## Agent Onboarding Certification

I, [Agent Session ID], certify that:

1. [ ] I have read HARDENED_SPEC.md completely
2. [ ] I have read MAGIC_NUMBERS.md completely
3. [ ] I have read prd.md sections §1-§4
4. [ ] I can answer all 10 verification questions
5. [ ] I understand the red flag patterns
6. [ ] I verified the build succeeds
7. [ ] I will not invent algorithms
8. [ ] I will not modify DO NOT MODIFY code
9. [ ] I will run tests after every change
10. [ ] I will document my changes

Date: [YYYY-MM-DD]
Starting task: [Reference to tasks.md item]
```

---

**END OF ONBOARDING CHECKLIST**

*Every minute spent on this checklist saves ten minutes debugging drift.*
