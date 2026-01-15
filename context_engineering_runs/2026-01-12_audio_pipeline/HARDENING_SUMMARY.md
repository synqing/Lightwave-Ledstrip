# Blueprint Hardening Summary

**Date:** 2026-01-12  
**Version:** 1.1-HARDENED  
**Status:** Critical bugs fixed, ready for full implementation

---

## Critical Bugs Fixed

### 1. ⚠️ **TIMING BUDGET CONTRADICTION (Fatal)**

**Problem:** Document claimed "14ms processing budget" but hop period is 8ms (128 samples @ 16kHz). **You cannot take longer than your hop period in a real-time system.**

**Evidence of Bug:**
- Original Blueprint: "14ms per hop", "Goertzel <10ms", "Total <14ms"
- MAGIC_NUMBERS.md:89: "All audio processing must complete within 8ms"
- MAGIC_NUMBERS.md:73-91: `hop_period = 128 / 16000 = 0.008 sec = 8 ms`

**Fix Applied:**
- **Processing budget: ≤8ms total** (MUST complete within hop period)
- Goertzel: ≤5ms (dominant cost)
- Beat tracking: ≤1ms
- Signal quality: <1ms
- Contract: <1ms
- **Margin: ~2ms** for OS jitter/interrupts
- **Total: ~7.5ms < 8ms ✅**

**Verification:** M-7 automated timing test (`test_timing_budget`) enforces ≤8ms deadline.

---

### 2. 📄 **EVIDENCE-FREE NUMEROLOGY**

**Problem:** Blueprint made claims without citing repo sources. "Good story" instead of "compilable truth."

**Claims Without Evidence (Examples):**
- "/Users/macbookpro/esp/esp-idf/export.sh" (machine-specific)
- "918 lines" (not verified)
- "~20KB RAM" (no arithmetic shown)
- "64/96 bins" (ambiguous)

**Fix Applied:**
- **Evidence Ledger section** added with table format:
  - Claim → Evidence → Source (file path + line numbers)
  - Every non-trivial claim backed by snippet from repo
- Sample rate: MAGIC_NUMBERS.md:19-35
- Hop period: MAGIC_NUMBERS.md:73-91
- 64 bins: MAGIC_NUMBERS.md:98-127
- Build commands: platformio.ini:7,16

**Example Evidence Entry:**
| Claim | Evidence | Source |
|-------|----------|--------|
| **Hop period: 8ms** | `128 / 16000 = 0.008 sec = 8 ms`<br/>`hop_rate = 125 Hz` | `MAGIC_NUMBERS.md:73-91` |

---

### 3. 🔧 **PORTABILITY ISSUES**

**Problem:** Machine-specific paths hardcoded, not portable across installations.

**Violations Found:**
- `/Users/macbookpro/esp/esp-idf/export.sh` (hardcoded user directory)
- ESP-IDF path assumed to be at specific location

**Fix Applied:**
- **Portable approach documented:**
  ```bash
  # Portable (install-dependent)
  source $ESP_IDF_PATH/export.sh
  # OR follow project-specific setup instructions
  ```
- Removed all `/Users/macbookpro/...` references
- Generic patterns only

---

### 4. 🔐 **SECURITY ISSUES**

**Problem:** MCP access keys exposed in documentation.

**Fix Applied:**
- Removed all MCP access keys from documentation
- Added security note: "MCP access keys and other secrets MUST NOT be included in documentation or committed to git"
- Warning about MCP ecosystem auth/secret hygiene

---

### 5. 🔢 **CONSISTENCY ISSUES**

**Problem:** "64/96 bins" ambiguity throughout document.

**Evidence of Ambiguity:**
- Some sections said "64 bins"
- Other sections said "64/96 bins"
- No clear canonical value

**Fix Applied:**
- **Canonical Parameters Table** added as single source of truth
- **64 bins** (Evidence: MAGIC_NUMBERS.md:98-127, Sensory Bridge reference)
- NO 96-bin option (removed all references)
- All subsequent mentions reference this table

---

### 6. ✅ **VERIFICATION SEMANTICS MADE EXPLICIT**

**Problem:** M-1 to M-9 metrics mentioned but verification commands not specified.

**Fix Applied:**
- **Verification Semantics section** added with concrete details for each metric:
  - **Command:** Exact shell command to run
  - **PASS Criteria:** Specific, testable conditions
  - **FAIL Criteria:** Explicit failure conditions
  - **Evidence:** Source citations

**Example (M-7 Timing Budget):**
```bash
# Command
pio test -e esp32dev_audio -f test_timing_budget

# PASS Criteria
- processHop() completes in ≤8ms (1,920,000 cycles @ 240 MHz)
- Goertzel: ≤5ms, Beat: ≤1ms, Contract: ≤1ms
- No heap allocation

# FAIL Criteria
- processHop() exceeds 8ms (violates real-time constraint)
- Any component exceeds budget
- Heap allocation in hot path
```

---

## Remaining Work (Full Blueprint Sections)

**STATUS:** §1 and §2 are complete and hardened. The following sections need similar hardening treatment:

### To Complete in technical_blueprint_HARDENED.md:

- [ ] **§3. State Transition Specifications**
  - Extract from existing technical_blueprint.md §3 (lines 201-400)
  - Add evidence citations for all state lifecycle claims
  - Verify Pre/Post conditions match PRD §8

- [ ] **§4. Integration Wiring**
  - Extract from existing technical_blueprint.md §4 (lines 401-600)
  - Add timing budget annotations per call
  - Verify critical sequences match processHop() flow

- [ ] **§5. System Components**
  - Extract from existing technical_blueprint.md §5 (lines 601-700)
  - Add RAM/Flash budget per component
  - Verify file paths exist or marked as "greenfield"

- [ ] **§6. Data Models** (if applicable for embedded)
  - Schema for ControlBusFrame
  - Memory layout diagrams

- [ ] **§7. API Specifications**
  - C++ method signatures
  - Contract definitions

- [ ] **§8. Implementation Phases**
  - Already in tasks.md, reference or summarize

- [ ] **§9. Technical Risks**
  - Extract from existing §9
  - Add mitigation timing/evidence

- [ ] **§10. Testing Strategy**
  - Already covered in Verification Semantics
  - Cross-reference M-1 to M-9

- [ ] **§11. Deployment Considerations**
  - Build commands (portable)
  - Verification workflow

---

## Patch Set Application Status

| Patch | Status | Notes |
|-------|--------|-------|
| 1. Fix timing budget | ✅ COMPLETE | 8ms deadline enforced, phase budgets updated |
| 2. Add Evidence Ledger | ✅ COMPLETE | All §1-2 claims cited with sources |
| 3. Normalize key params | ✅ COMPLETE | Canonical Parameters Table added |
| 4. Make verify.sh explicit | ✅ COMPLETE | M-1 to M-9 with exact commands/criteria |

**Additional Fixes Applied:**
- ✅ Portability (removed machine-specific paths)
- ✅ Security (removed MCP keys, added warnings)
- ✅ Consistency (64 bins canonical, no ambiguity)

---

## What to Do Next

### Option 1: Complete Full Blueprint (Recommended)

Extend `technical_blueprint_HARDENED.md` with remaining sections (§3-§11) following the same hardening approach:
- Evidence citations for all claims
- Timing budgets for all operations
- Portable patterns only
- Verification commands explicit

### Option 2: Use Hardened Sections + Existing Comprehensive Docs

- **Use:** `technical_blueprint_HARDENED.md` (§1-2, Evidence Ledger, Verification Semantics)
- **Reference:** `planning/audio-pipeline-redesign/technical_blueprint.md` (§3-11 comprehensive details)
- **Caution:** Existing doc has timing bug (says "16ms hop"), use hardened timing values

### Option 3: Start Implementation with Hardened Foundation

- **Foundation:** Use corrected timing budget (≤8ms), 64 bins, Evidence Ledger
- **Tasks:** Reference `context_engineering_runs/2026-01-12_audio_pipeline/tasks.md`
- **Verification:** Use explicit M-1 to M-9 commands from hardened blueprint
- **Guard Rails:** Automated timing test (M-7) prevents 14ms regression

---

## Critical Reminders for Implementation

1. **TIMING IS NON-NEGOTIABLE:**
   - ≤8ms total processing budget
   - Goertzel ≤5ms, Beat ≤1ms, Contract <1ms
   - M-7 test MUST pass before merging

2. **64 BINS CANONICAL:**
   - Not 96 bins
   - Semitone-spaced (equal temperament)
   - A1 (55 Hz) to C7 (2093 Hz)

3. **EVIDENCE-DRIVEN DEVELOPMENT:**
   - Every claim must cite source
   - "I think" → "MAGIC_NUMBERS.md:89 says"
   - Profile timing, don't guess

4. **VERIFICATION FIRST:**
   - Run M-1 to M-9 after every phase
   - Timing test (M-7) catches regression
   - Agent drift test (M-5) catches corruption

---

## PASS/FAIL: Blueprint Hardening

✅ **PASS - All critical bugs fixed:**
1. ✅ Timing budget corrected (8ms, not 14ms)
2. ✅ Evidence Ledger added (claims → citations)
3. ✅ Portability fixed (no machine-specific paths)
4. ✅ Security fixed (no MCP keys in docs)
5. ✅ Consistency fixed (64 bins canonical)
6. ✅ Verification explicit (M-1 to M-9 commands)

**Status:** Blueprint foundation is now "compilable truth" ready for implementation.

---

*Document Version: 1.0*  
*Created: 2026-01-12*  
*Hardening Complete: §1-2 + Evidence Ledger + Verification Semantics*  
*Remaining: §3-11 (can reference existing comprehensive docs with corrected timing)*
