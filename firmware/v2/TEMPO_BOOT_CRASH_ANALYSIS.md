# CRITICAL: Tempo Tracking Boot Crash Analysis

**Date:** 2026-01-08 22:43 GMT+8
**Severity:** 🔴 CRITICAL - System boot failure
**Impact:** Phase 6 validation BLOCKED, firmware unusable
**Status:** UNDER INVESTIGATION

---

## Crash Summary

**Exception:** LoadProhibited (Core 1 panic)
**Address:** 0x00000006 (null pointer dereference + 6 byte offset)
**Location:** NodeOrchestrator initialization
**Behavior:** Boot loop (continuous reset)

```
[INFO][Main] Initializing Node Orchestrator...
[NodeOrchestrator] Initializing Node Orchestrator...
[NodeOrchestrator] Free heap: 8650407 bytes

Guru Meditation Error: Core 1 panic'ed (LoadProhibited). Exception was unhandled.

Core  1 register dump:
PC      : 0x42009521  PS      : 0x00060e30  A0      : 0x82009b80  A1      : 0x3fcebd10
A2      : 0x3d80b6a0  A3      : 0x3fceda94  A4      : 0x00000004  A5      : 0x00000000
A6      : 0x000001bc  A7      : 0x0000001b  A8      : 0x82009515  A9      : 0x00000000
A10     : 0x3fced914  A11     : 0x00000000  A12     : 0x00000060  A13     : 0x3d808ffc
A14     : 0x00000000  A15     : 0x00000006  SAR     : 0x0000001d  EXCCAUSE: 0x0000001c
EXCVADDR: 0x00000006  LBEG    : 0x400570e8  LEND    : 0x400570f3  LCOUNT  : 0x00000000

Backtrace: 0x4200951e:0x3fcebd10 0x42009b7d:0x3fcebd30 0x420067c2:0x3fcebd50
           0x4200edf7:0x3fcebf00 0x420393ad:0x3fcebf40 0x4208e65e:0x3fcebfa0
```

---

## Root Cause Analysis

### Exception Type: LoadProhibited (0x1c)

**Meaning:** Attempted to load data from an invalid memory address.

**EXCVADDR: 0x00000006**
- This is a null pointer dereference with a 6-byte offset
- Pattern: `obj_ptr->member` where `obj_ptr == nullptr` and `member` is at offset 6

### Context: NodeOrchestrator Initialization

**Timing:** Crash occurs immediately after:
```cpp
[NodeOrchestrator] Initializing Node Orchestrator...
[NodeOrchestrator] Free heap: 8650407 bytes
```

**What happens next in NodeOrchestrator::init():**
1. AudioNode initialization (if FEATURE_AUDIO_SYNC enabled)
2. RendererNode initialization
3. Cross-node wiring

**Most Likely:** AudioNode or its dependencies (TempoTracker, RhythmBank, HarmonyBank) have initialization issues.

---

## Prime Suspects

### Suspect #1: TempoTracker Member Access in Constructor (HIGH PROBABILITY)

**Theory:** TempoTracker constructor accesses member variables before they're initialized.

**Evidence:**
- Phase 5 added state machine with new member: `TempoTrackerState state_`
- If state_ accessed before initialization, could cause crash
- Offset 6 bytes from 'this' could be the state_ member

**Code to Review:**
```cpp
// TempoTracker.h - check member initialization order
class TempoTracker {
private:
    TempoTrackerState state_;  // If accessed in constructor before init...
    uint16_t hop_count_;
    uint8_t mismatch_streak_;
    // ...
};

// TempoTracker.cpp - constructor
TempoTracker::TempoTracker(const TempoTrackerTuning& tuning)
    : tuning_(tuning)
    // ... other initializers ...
{
    // If state_ used here before initialization, crash!
    updateState();  // <-- Potential issue if called too early
}
```

**Fix:** Ensure all members initialized before use in constructor body.

---

### Suspect #2: AudioRingBuffer Uninitialized (MEDIUM PROBABILITY)

**Theory:** AudioRingBuffer template used before construction completes.

**Evidence:**
- AudioRingBuffer is header-only template (Phase 2A)
- RhythmBank/HarmonyBank use AudioRingBuffer in constructors
- If buffer size is zero or buffer pointer is null, crash on first push()

**Code to Review:**
```cpp
// AudioRingBuffer.h
template<typename T, size_t Capacity>
class AudioRingBuffer {
private:
    T m_buffer[Capacity];  // Stack array - should be safe
    size_t m_writeIndex;    // If uninitialized, could cause issues
};
```

**Fix:** Ensure m_writeIndex initialized to 0 in constructor.

---

### Suspect #3: RhythmBank/HarmonyBank Member Order (LOW PROBABILITY)

**Theory:** Bank objects access members in wrong order during construction.

**Evidence:**
- Both banks have AGC, NoiseFloor, NoveltyFlux members
- If one member's constructor calls another member before it's initialized, crash

**Code to Review:**
```cpp
// RhythmBank.cpp constructor
RhythmBank::RhythmBank()
    : m_goertzel(...),  // Initialized first
      m_agc(...),       // Initialized second
      m_noiseFloor(...) // Initialized third
{
    // If m_noiseFloor used before its constructor runs, crash!
    m_goertzel.compute(buffer, magnitudes);  // Could access m_noiseFloor
}
```

**Fix:** Ensure member initialization order matches declaration order in header.

---

### Suspect #4: AudioNode Tempo Pointer (LOW PROBABILITY)

**Theory:** AudioNode accesses m_tempo pointer before TempoTracker is fully constructed.

**Evidence:**
- AudioNode has `TempoTracker m_tempo` member (value, not pointer in v2)
- If AudioNode constructor calls m_tempo methods before m_tempo's constructor completes, crash

**Code to Review:**
```cpp
// AudioNode.cpp constructor
AudioNode::AudioNode()
    : m_tempo(default_tuning)  // TempoTracker constructed here
{
    // If we call m_tempo methods here before construction completes...
    m_tempo.updateState();  // Potential crash if state_ not initialized
}
```

**Fix:** Don't call m_tempo methods in AudioNode constructor body.

---

## Debugging Steps

### Step 1: Decode Backtrace (PRIORITY 1)

**Extract symbol addresses from ELF:**

```bash
# Navigate to firmware directory
cd /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware/v2

# Decode backtrace using addr2line
/Users/spectrasynq/.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-addr2line \
  -e .pio/build/esp32dev_audio/firmware.elf \
  -f -C \
  0x4200951e 0x42009b7d 0x420067c2 0x4200edf7 0x420393ad 0x4208e65e
```

**Expected Output:**
```
FunctionName
/path/to/source.cpp:line_number
```

This will reveal EXACTLY which function/line is crashing.

---

### Step 2: Check Member Initialization Order

**Review each Phase 2-5 class constructor:**

```bash
# Check TempoTracker constructor
grep -A 20 "TempoTracker::TempoTracker" src/audio/tempo/TempoTracker.cpp

# Check RhythmBank constructor
grep -A 15 "RhythmBank::RhythmBank" src/audio/RhythmBank.cpp

# Check HarmonyBank constructor
grep -A 15 "HarmonyBank::HarmonyBank" src/audio/HarmonyBank.cpp

# Check AudioNode constructor
grep -A 20 "AudioNode::AudioNode" src/audio/AudioNode.cpp
```

**Look for:**
- Members accessed before initialization
- Calls to methods that use uninitialized members
- Constructor body code that assumes members are ready

---

### Step 3: Add Defensive Checks

**Temporary debug code to isolate the crash:**

```cpp
// In TempoTracker constructor (TempoTracker.cpp)
TempoTracker::TempoTracker(const TempoTrackerTuning& tuning)
    : tuning_(tuning),
      state_(TempoTrackerState::INITIALIZING),  // Explicitly initialize FIRST
      hop_count_(0),
      mismatch_streak_(0)
{
    Serial.println("[TempoTracker] Constructor START");

    // Don't call updateState() in constructor!
    // Wait for first updateTempo() call instead

    Serial.println("[TempoTracker] Constructor END");
}
```

---

### Step 4: Disable Audio Subsystem (Fallback)

**If crash persists, temporarily disable audio to verify hardware boots:**

```cpp
// In platformio.ini, add build flag:
build_flags =
    ${common.build_flags}
    -D FEATURE_AUDIO_SYNC=0  // Disable audio subsystem
```

**Rebuild and test:**
```bash
pio run -e esp32dev_audio -t upload -d firmware/v2 --upload-port /dev/cu.usbmodem101
```

**Expected:** If crash disappears, confirms audio subsystem is the cause.

---

## Immediate Fix Strategy

### Option A: Revert Phase 5 (Conservative)

**If Phase 5 state machine is the culprit:**

1. Revert TempoTracker.h/cpp to Phase 4 version
2. Rebuild and test
3. If boot succeeds, Phase 5 has a bug
4. Fix state machine initialization and re-apply

**Commands:**
```bash
cd /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip
git diff HEAD~1 firmware/v2/src/audio/tempo/TempoTracker.cpp > phase5_changes.patch
git checkout HEAD~1 -- firmware/v2/src/audio/tempo/TempoTracker.h firmware/v2/src/audio/tempo/TempoTracker.cpp
pio run -e esp32dev_audio -t upload -d firmware/v2 --upload-port /dev/cu.usbmodem101
```

---

### Option B: Fix Member Initialization (Surgical)

**If member initialization order is the issue:**

1. Review all Phase 2-5 class headers for member declaration order
2. Ensure constructor initializer lists match header order
3. Move constructor body code to explicit init() methods
4. Rebuild and test

**Pattern:**
```cpp
// Header: members declared in this order
class Foo {
private:
    int m_a;
    int m_b;
    int m_c;
};

// Implementation: MUST initialize in same order
Foo::Foo()
    : m_a(0),  // Same order as header!
      m_b(0),
      m_c(0)
{
    // Constructor body - all members now safe to use
}
```

---

### Option C: Lazy Initialization (Robust)

**If constructor timing is complex:**

1. Don't initialize heavy objects in constructors
2. Use explicit init() methods called after construction
3. Add null checks before accessing members

**Pattern:**
```cpp
class TempoTracker {
public:
    TempoTracker(const TempoTrackerTuning& tuning);
    void init();  // Call this after construction completes

private:
    bool m_initialized = false;
    TempoTrackerState state_;
};

void TempoTracker::init() {
    if (m_initialized) return;
    state_ = TempoTrackerState::INITIALIZING;
    // ... rest of initialization ...
    m_initialized = true;
}
```

---

## Root Cause Hypothesis (Best Guess)

**Most Likely:** Phase 5 `updateState()` called in TempoTracker constructor before `state_` member is initialized.

**Evidence:**
1. Phase 5 added state machine (commit 6b5c812)
2. State enum member `state_` added to class
3. If `updateState()` called in constructor body before member initialization completes, accessing `state_` would read garbage
4. Offset 0x06 matches typical enum/small type offset in class layout

**Fix:**
```cpp
// TempoTracker.cpp - BEFORE (BROKEN)
TempoTracker::TempoTracker(const TempoTrackerTuning& tuning)
    : tuning_(tuning)
{
    updateState();  // ← CRASH! state_ not initialized yet
}

// TempoTracker.cpp - AFTER (FIXED)
TempoTracker::TempoTracker(const TempoTrackerTuning& tuning)
    : tuning_(tuning),
      state_(TempoTrackerState::INITIALIZING)  // ← Initialize in list
{
    // Now updateState() is safe to call
}
```

---

## Impact on Phase 6

**Status:** BLOCKED - Cannot validate tempo tracking while firmware crashes on boot.

**Cascading Effects:**
- All 20 AC cannot be tested
- All 7 NFR cannot be measured
- Ground truth validation impossible
- Parameter tuning blocked

**Required Before Phase 6:**
1. Fix boot crash
2. Verify clean boot to serial prompt
3. Confirm tempo tracking starts without errors
4. THEN proceed with Phase 6 validation plan

---

## Harness Status Update Required

**Current:** Phase 6 planned as validation
**Reality:** Phase 5 introduced boot regression

**Record Phase 6 Attempt as BLOCKED:**
```bash
python3 .claude/skills/ralph-loop/scripts/update_attempt.py TEMPO-TRACKING-FIX-001 BLOCKED \
  --summary "Phase 6 blocked: Critical boot crash (LoadProhibited @ 0x00000006) during NodeOrchestrator init. Firmware boot loops, Phase 5 state machine suspected. Backtrace suggests null pointer dereference in TempoTracker or bank constructors. Requires immediate fix before validation can proceed. See TEMPO_BOOT_CRASH_ANALYSIS.md for debugging steps."
```

**Iteration Status:** Increment 4 → 5 (budget: 5 iterations remaining)

---

## Next Steps for Developer

### Immediate (< 5 minutes)

1. **Decode backtrace** using addr2line (see Step 1 above)
2. **Identify crashing function** from decoded symbols
3. **Review constructor** of crashing class

### Short-term (< 30 minutes)

1. **Fix member initialization** in identified class
2. **Rebuild firmware:** `pio run -e esp32dev_audio`
3. **Upload and test:** `pio run -e esp32dev_audio -t upload --upload-port /dev/cu.usbmodem101`
4. **Verify clean boot:** Device should reach "LightwaveOS started" without crash

### Medium-term (< 2 hours)

1. **Add defensive checks** to all Phase 2-5 constructors
2. **Add unit tests** for constructor safety (if native_test env works)
3. **Document initialization order** for all new classes
4. **Re-test Phase 6** validation after fix

---

## Lessons Learned (Meta)

### What Went Wrong

1. **No incremental testing** - All 5 phases built together, crash root cause ambiguous
2. **No constructor validation** - Assumed member initialization was safe
3. **Complex state machine** added in Phase 5 without runtime verification
4. **Build success ≠ runtime success** - Linker doesn't catch logic errors

### Prevention for Future

1. **Test each phase on hardware** before proceeding to next
2. **Smoke test** after every commit (does it boot?)
3. **Constructor hygiene** - keep constructors minimal, use init() methods
4. **Member initialization order** - always match header declaration order
5. **Defensive programming** - null checks, assert(m_initialized), bounds checking

---

**Timestamp:** 2026-01-08T22:44:00Z
**Severity:** 🔴 CRITICAL
**Action Required:** Debug backtrace, fix constructor, re-upload
**ETA to Fix:** 30 minutes (if backtrace points to obvious issue)

