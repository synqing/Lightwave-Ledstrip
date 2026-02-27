# LightwaveOS Recovery Strategy

**Date:** 2026-01-17
**Based on:** Forensic Investigation Analysis
**Status:** ACTIONABLE

---

## Executive Summary

This document synthesizes the forensic investigation findings into an actionable recovery strategy. The investigation revealed **two independent regressions** that require different approaches:

| Issue | Location | Fix Strategy |
|-------|----------|--------------|
| **LED Non-functionality** | Current (Actor) | Port Node architecture from rollback OR binary search differences |
| **Tempo Tracking** | Both have issues | Current has Goertzel (correct); rollback has onset-timing (flawed) |

---

## Finding 1: Architecture Divergence

### Node vs Actor Naming

The codebase diverged into two architectures:

| Component | Rollback (Working LEDs) | Current (Broken LEDs) |
|-----------|-------------------------|----------------------|
| Base class | `Node` | `Actor` |
| Orchestrator | `NodeOrchestrator` | `ActorSystem` |
| Renderer | `RendererNode` | `RendererActor` |
| ShowDirector | `ShowNode` | `ShowDirectorActor` |

### Core Implementation: IDENTICAL

Analysis of `run()` methods shows they are **functionally identical**:
- Same queue draining logic
- Same tick timing pattern
- Same message handling

### The Real Difference: main.cpp Features

The rollback version has additional features NOT in current:

```cpp
// Rollback has these, current does not:
#include "effects/modifiers/ModifierStack.h"
#include "effects/modifiers/SpeedModifier.h"
#include "effects/modifiers/IntensityModifier.h"
// ... 7 more modifiers

#include "core/persistence/PresetManager.h"
```

And critically different error handling:
```cpp
// Rollback: continues for debugging
if (!orchestrator.start()) {
    LW_LOGE("Node Orchestrator start failed! Running in diagnostic mode.");
    // Continue to loop() for serial debugging access
}

// Current: HALTS completely
if (!actors.start()) {
    LW_LOGE("Actor System start failed!");
    while(1) delay(1000);  // Halt
}
```

---

## Finding 2: TempoTracker Architecture

### Surprising Discovery

The implementations are **swapped** from what might be expected:

| Version | Lines | Architecture | Status per Forensic Plan |
|---------|-------|--------------|--------------------------|
| **Rollback** | 1964 | Onset-timing | FUNDAMENTALLY FLAWED |
| **Current** | 617 | Goertzel | CORRECT approach |

### Implications

1. **Rollback has working LEDs but flawed tempo**
2. **Current has correct tempo but broken LEDs**
3. **Neither version is fully functional**

---

## Finding 3: Tab5 WiFi Regression

### Status: DOCUMENTED AND WORKAROUNDED

The ESP32-P4 Tab5 WiFi regression is caused by incorrect SDIO pin mappings in Arduino-ESP32's pre-compiled ESP-Hosted library.

**Workaround Applied:** `ENABLE_WIFI=0` in `firmware/Tab5.encoder/src/config/Config.h`

**Future Fix:** Wait for Arduino-ESP32 v3.3.0+ which adds `WiFi.setPins()` support.

---

## Recovery Options

### Option A: Port Node Architecture to Current (Recommended)

**Effort:** Medium (rename classes, update imports)
**Risk:** Low (mostly mechanical changes)

1. Copy Node.cpp/h, NodeOrchestrator.cpp/h, RendererNode.cpp/h from rollback
2. Replace Actor* with Node* in current codebase
3. Keep current Goertzel TempoTracker
4. Keep current main.cpp structure but add missing modifiers

**Result:** Working LEDs + Correct tempo + All features

### Option B: Port Goertzel TempoTracker to Rollback

**Effort:** Low (single component)
**Risk:** Medium (may have integration issues)

1. Copy TempoTracker.cpp/h from current to rollback
2. Verify AudioActor/AudioNode compatibility
3. Test tempo tracking

**Result:** Working LEDs + Correct tempo (but missing recent features)

### Option C: Binary Search for LED Failure

**Effort:** High (systematic testing)
**Risk:** Low (diagnostic approach)

1. Start with rollback firmware
2. Replace components one at a time with current versions
3. Test LEDs after each replacement
4. Identify exact file causing failure

**Result:** Root cause identification

---

## Recommended Path Forward

### Phase 1: Immediate (Today)

1. **Verify rollback builds and runs** with working LEDs
2. **Copy Goertzel TempoTracker** to rollback location
3. **Build and test** tempo tracking in rollback

### Phase 2: Short-term (This Week)

1. **Evaluate Option A vs B** based on feature requirements
2. **Implement chosen option**
3. **Document architectural decisions**

### Phase 3: Medium-term

1. **Consolidate to single codebase**
2. **Add systematic debugging** to renderer (as recommended in DEBUGGING_FAILURE_REPORT.md)
3. **Create automated tests** for LED output

---

## Key Files Reference

### Rollback Location (Working LEDs)
```
/Users/spectrasynq/Downloads/Lightwave-Ledstrip-feat-tempo-fft-redesign/firmware/v2/
├── src/core/actors/
│   ├── Node.cpp/h           # Working base class
│   ├── NodeOrchestrator.cpp/h
│   └── RendererNode.cpp/h   # Working LED rendering
└── src/audio/tempo/
    └── TempoTracker.cpp/h   # Onset-timing (FLAWED)
```

### Current Location (Broken LEDs, Correct Tempo)
```
/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware/v2/
├── src/core/actors/
│   ├── Actor.cpp/h          # Non-working base class?
│   ├── ActorSystem.cpp/h
│   └── RendererActor.cpp/h  # LED rendering fails
└── src/audio/tempo/
    └── TempoTracker.cpp/h   # Goertzel (CORRECT)
```

---

## Emotiscope Reference Architecture

The correct audio pipeline parameters from Emotiscope docs:

| Parameter | Correct Value | Notes |
|-----------|---------------|-------|
| Sample Rate | 12,800 Hz | NOT 16kHz |
| Chunk Size | 64 samples | ~5ms per chunk |
| Goertzel Bins | 64 | Semitone-spaced, A1-C7 |
| Novelty Buffer | 1024 @ 50Hz | ~20.48s history |
| Tempo Bins | 96 | 48-143 BPM |

Reference docs:
- `/Users/spectrasynq/Workspace_Management/Software/Emotiscope.HIL/docs/EMOTISCOPE_AUDIO_PIPELINE_COMPLETE_REFERENCE.md`
- `/Users/spectrasynq/Workspace_Management/Software/Emotiscope.HIL/docs/EMOTISCOPE_DSP_ALGORITHMS.md`

---

*Recovery strategy document generated from forensic investigation findings.*
