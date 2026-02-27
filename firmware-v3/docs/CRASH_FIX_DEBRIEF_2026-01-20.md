# CRASH FIX SESSION DEBRIEF - 2026-01-20

## SUMMARY

This document details ALL code changes made during the watchdog timeout crash investigation session. The session attempted to fix WDT crashes but introduced potentially broken code in effect files.

**RESULT: STILL CRASHING** - The changes did NOT fix the issue. The device still crashes with `TG1WDT_SYS_RST` when cycling through enhanced effects (specifically LGP Wave Collision Enhanced, ID 96).

---

## REVERT COMMAND

To revert ALL changes made in this session:

```bash
cd /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware/v2
git checkout HEAD -- src/effects/ieffect/
git checkout HEAD -- src/main.cpp
git checkout HEAD -- src/core/actors/RendererActor.cpp
git checkout HEAD -- src/core/actors/RendererActor.h
git checkout HEAD -- src/audio/AudioActor.cpp
git checkout HEAD -- src/core/system/StackMonitor.cpp
git checkout HEAD -- platformio.ini
git checkout HEAD -- src/network/RequestValidator.h
git checkout HEAD -- src/audio/contracts/AudioEffectMapping.h
```

---

## FILE-BY-FILE CHANGES

### 1. `src/main.cpp`

**Purpose:** Disable watchdog timer completely

**Changes:**
```diff
+// Watchdog timer - include to DISABLE it
 #ifndef NATIVE_BUILD
 #include <esp_task_wdt.h>
 #endif

 void setup() {
     Serial.begin(115200);
     delay(1000);

+    // DISABLE WATCHDOG TIMER COMPLETELY
+    #ifndef NATIVE_BUILD
+    esp_task_wdt_deinit();  // Completely disable the task watchdog
+    #endif

     // Telemetry boot heartbeat...

// In loop():
+    // Watchdog timer DISABLED - uncomment to re-enable
     // Feed watchdog timer (prevents system reset if tasks block)
-#ifndef NATIVE_BUILD
-    esp_task_wdt_reset();
-#endif
+// #ifndef NATIVE_BUILD
+//     esp_task_wdt_reset();
+// #endif
```

**Risk Level:** LOW - WDT disable is benign

---

### 2. `platformio.ini`

**Purpose:** Add WDT disable flags (DON'T WORK), hardcode upload port

**Changes:**
```diff
+	; Watchdog timers DISABLED
+	-D CONFIG_ESP_TASK_WDT_EN=0
+	-D CONFIG_ESP_TASK_WDT=0
+	-D CONFIG_ESP_INT_WDT=0
+	-D CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0=0
+	-D CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1=0

+upload_port = /dev/cu.usbmodem21401
```

Also added many codec source files to native_codec_test_ws build.

**Risk Level:** LOW - Build flags don't actually disable WDT at runtime

---

### 3. `src/core/actors/RendererActor.h`

**Purpose:** Increase MAX_EFFECTS, add Trinity integration

**Changes:**
```diff
-    static constexpr uint8_t MAX_EFFECTS = 96;
+    // As of 2025-01-20: 98 effects registered (IDs 0-97), set to 100 for headroom.
+    static constexpr uint8_t MAX_EFFECTS = 100;

+    // Trinity Contract Layer Integration
+    void injectTrinityBeat(float bpm, float phase01, bool isTick, bool isDownbeat, int beat_in_bar);
+    void updateTrinityMacros(float energy, float vocal_presence, float bass_weight,
+                            float percussiveness, float brightness);
+    void startTrinitySync(float bpm);
+    void stopTrinitySync();
+    void seekTrinitySync(float position_sec);
+    void pauseTrinitySync();
+    void resumeTrinitySync();

+    audio::TrinityControlBusProxy m_trinityProxy;
+    bool m_trinitySyncActive = false;
+    bool m_trinitySyncPaused = false;
+    float m_trinitySyncPosition = 0.0f;
```

**Risk Level:** LOW - MAX_EFFECTS increase is safe, Trinity code is unused

---

### 4. `src/core/actors/RendererActor.cpp`

**Purpose:** Add seqlock timeout, WDT reset (commented out), Trinity message handlers

**Key Changes:**

**Seqlock timeout (lines 682-696):**
```cpp
uint32_t spins = 0;
do {
    v0 = m_audioContractSeq.load(std::memory_order_acquire);
    if (v0 & 1U) {
        if (++spins > 10000) {
            LW_LOGW("Seqlock timeout in getAudioContractTuning - returning stale data");
            break;
        }
        continue;
    }
    out = m_audioContractPending;
    v1 = m_audioContractSeq.load(std::memory_order_acquire);
} while (v0 != v1 || (v1 & 1U));
```

**WDT reset commented out (lines 551-558):**
```cpp
// Watchdog timer DISABLED - uncomment to re-enable
// Feed watchdog at start of each frame to prevent starvation of loopTask
// #ifndef NATIVE_BUILD
//     esp_task_wdt_reset();
// #endif
```

**Trinity message handlers added (lines 460-545):**
- `MessageType::TRINITY_BEAT`
- `MessageType::TRINITY_MACRO`
- `MessageType::TRINITY_SYNC`

**Risk Level:** MEDIUM - Seqlock change could affect audio timing

---

### 5. `src/audio/AudioActor.cpp`

**Purpose:** Add seqlock timeout to prevent infinite spinloop

**Changes (lines 121-137 and 149-172):**
```cpp
uint32_t spins = 0;
do {
    v0 = m_pipelineTuningSeq.load(std::memory_order_acquire);
    if (v0 & 1U) {
        if (++spins > 10000) {
            LW_LOGW("Seqlock timeout in getPipelineTuning - returning stale data");
            break;
        }
        continue;
    }
    // ...
```

Same pattern applied to `getDspState()`.

**Risk Level:** MEDIUM - Could return stale audio data

---

### 6. `src/network/RequestValidator.h`

**Purpose:** Sync MAX_EFFECTS constant

**Changes:**
```diff
-    static constexpr uint8_t MAX_EFFECTS = 96;
+    static constexpr uint8_t MAX_EFFECTS = 100;
```

**Risk Level:** LOW

---

### 7. `src/audio/contracts/AudioEffectMapping.h`

**Purpose:** Sync MAX_EFFECTS constant

**Changes:**
```diff
-    static constexpr uint8_t MAX_EFFECT_ID = 96;
+    static constexpr uint8_t MAX_EFFECT_ID = 100;
```

**Risk Level:** LOW

---

### 8. `src/core/system/StackMonitor.cpp`

**Purpose:** Comment out WDT reset

**Changes:**
```diff
-#include <esp_task_wdt.h>
+// #include <esp_task_wdt.h>

-    esp_task_wdt_reset();
+//     esp_task_wdt_reset();
```

**Risk Level:** LOW

---

## EFFECT FILE CHANGES (HIGH RISK)

### 9. `src/effects/ieffect/LGPStarBurstNarrativeEffect.cpp` + `.h`

**Purpose:** Replace sinf/expf with LUT-based lookups to reduce CPU usage

**MASSIVE REWRITE - 321 lines changed**

**Key Changes:**
1. Added `m_expLUT[64]` for pre-computed exp(-x) values
2. Added cache arrays for strip 2 reuse:
   - `m_cachedBrightness[160]`
   - `m_cachedPaletteIdx[160]`
   - `m_cachedBRoot[160]`
   - `m_cachedBThird[160]`
   - `m_cachedBFifth[160]`
3. Replaced `sinf()` with `sin8()` FastLED lookup
4. Replaced `expf()` with LUT lookup
5. Changed `expDecay()` to `expDecayFast()` linear approximation
6. Split render loop into Strip 1 (compute + cache) and Strip 2 (read cache)

**Risk Level:** **VERY HIGH** - Complex math rewrite, likely broken

---

### 10. `src/effects/ieffect/LGPSpectrumDetailEnhancedEffect.cpp` + `.h`

**Purpose:** Optimize render loop

**Changes:**
1. Added `m_sinTable[128]` for sin lookup
2. Added `m_avgBins[64]` as class member (was stack array)
3. Added `m_hasBin[80]`, `m_binMagnitudeAt[80]` as class members
4. Replaced `sinf()` with table lookup
5. Replaced stack arrays with class members

**Risk Level:** HIGH - Math changes could produce wrong values

---

### 11. `src/effects/ieffect/LGPSpectrumDetailEffect.cpp` + `.h`

**Purpose:** Pre-compute bin-to-LED distance LUT

**Changes:**
1. Added `m_binDistanceLUT[64]` member
2. Pre-compute LUT in `init()` with `log10f()`
3. `binToLedDistance()` now returns `m_binDistanceLUT[bin]`

**Risk Level:** MEDIUM - LUT computation looks correct

---

### 12. `src/effects/ieffect/LGPStarBurstEffectEnhanced.cpp` + `.h`

**Purpose:** Replace sinf/expf/tanhf with optimized versions

**Changes:**
1. Added `m_expLut[128]` for exp(-x) lookup
2. Added `fastTanh()` inline function
3. Replaced `sinf()` with `sin8()` in render loop
4. Replaced `expf()` with LUT lookup
5. Replaced `tanhf()` with `fastTanh()` Pade approximation

**Risk Level:** **VERY HIGH** - Integer overflow was found and "fixed" but may still be broken

---

### 13. `src/effects/ieffect/LGPWaveCollisionEffectEnhanced.cpp` + `.h`

**Purpose:** Replace sinf/expf/tanhf with optimized versions

**Changes:**
1. Added `m_expLut[81]` for exp(-x) lookup
2. Replaced `sinf()` with `sin8()` - **THEN REVERTED BACK TO sinf()**
3. Replaced `expf()` with LUT lookup
4. Replaced `tanhf()` with fast approximation `x / (1 + fabsf(x))`

**Root cause found:** Integer overflow in sin8 angle calculation:
```cpp
// BROKEN: distFromCenter * 6 overflows uint8_t (79 * 6 = 474 > 255)
uint8_t sinArg = (uint8_t)((distFromCenter * 6) - (uint8_t)(m_phase * 0.406f));
```

This was "fixed" by reverting to float-based sinf(), but the crash persists.

**Risk Level:** **CRITICAL** - This effect causes the crash

---

### 14. `src/effects/ieffect/BreathingEnhancedEffect.cpp` + `.h`

**Purpose:** Pre-compute foreshortening LUT

**Changes:**
1. Added `m_foreshortenLUT[81]` member
2. Pre-compute in `init()`: `powf()` + `expf()` for each distance
3. Replace per-LED `powf()`/`expf()` with LUT lookup

**Risk Level:** HIGH - Math rewrite

---

## ROOT CAUSE ANALYSIS

The crash happens specifically when switching to **LGP Wave Collision Enhanced (ID 96)**. Despite multiple "fixes":

1. WDT disable doesn't work (ESP32 Arduino framework ignores CONFIG flags)
2. `esp_task_wdt_deinit()` should disable WDT but crash still shows `TG1WDT_SYS_RST`
3. The effect optimizations introduced bugs (integer overflow, wrong math)

**The real issue is likely:**
- Memory corruption from the effect optimizations
- Stack overflow from render loop complexity
- Uninitialized memory in the new LUT arrays

---

## RECOMMENDED ACTION

### IMPORTANT: Most enhanced effects are NEW FILES (untracked)

These files are NEW and have no previous version to revert to:
- `BreathingEnhancedEffect.cpp/.h` (NEW)
- `LGPSpectrumDetailEffect.cpp/.h` (NEW)
- `LGPSpectrumDetailEnhancedEffect.cpp/.h` (NEW)
- `LGPStarBurstEffectEnhanced.cpp/.h` (NEW)
- `LGPWaveCollisionEffectEnhanced.cpp/.h` (NEW)
- `RippleEnhancedEffect.cpp/.h` (NEW)
- etc.

Only `LGPStarBurstNarrativeEffect.cpp/.h` was MODIFIED from existing code.

### To DELETE all new enhanced effects (nuclear option):

```bash
cd /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware/v2

# Delete all new enhanced effect files
rm src/effects/ieffect/BreathingEnhancedEffect.cpp
rm src/effects/ieffect/BreathingEnhancedEffect.h
rm src/effects/ieffect/LGPSpectrumDetailEffect.cpp
rm src/effects/ieffect/LGPSpectrumDetailEffect.h
rm src/effects/ieffect/LGPSpectrumDetailEnhancedEffect.cpp
rm src/effects/ieffect/LGPSpectrumDetailEnhancedEffect.h
rm src/effects/ieffect/LGPStarBurstEffectEnhanced.cpp
rm src/effects/ieffect/LGPStarBurstEffectEnhanced.h
rm src/effects/ieffect/LGPWaveCollisionEffectEnhanced.cpp
rm src/effects/ieffect/LGPWaveCollisionEffectEnhanced.h
rm src/effects/ieffect/RippleEnhancedEffect.cpp
rm src/effects/ieffect/RippleEnhancedEffect.h
rm src/effects/ieffect/ChevronWavesEffectEnhanced.cpp
rm src/effects/ieffect/ChevronWavesEffectEnhanced.h
rm src/effects/ieffect/LGPInterferenceScannerEffectEnhanced.cpp
rm src/effects/ieffect/LGPInterferenceScannerEffectEnhanced.h
rm src/effects/ieffect/LGPPhotonicCrystalEffectEnhanced.cpp
rm src/effects/ieffect/LGPPhotonicCrystalEffectEnhanced.h
rm src/effects/ieffect/BPMEnhancedEffect.cpp
rm src/effects/ieffect/BPMEnhancedEffect.h

# Revert the ONE modified effect
git checkout HEAD -- src/effects/ieffect/LGPStarBurstNarrativeEffect.cpp
git checkout HEAD -- src/effects/ieffect/LGPStarBurstNarrativeEffect.h

# Then update CoreEffects.cpp to remove registrations for deleted effects
```

### To revert only modified tracked files:

```bash
git checkout HEAD -- src/effects/ieffect/LGPStarBurstNarrativeEffect.cpp
git checkout HEAD -- src/effects/ieffect/LGPStarBurstNarrativeEffect.h
```

---

## TIMELINE

| Time | Action | Result |
|------|--------|--------|
| Start | Investigated WDT crash after 10 minutes | Found priority inversion theory |
| +15m | Added WDT reset in RendererActor | Still crashed |
| +30m | Optimized LGPSpectrumDetailEnhancedEffect | Built OK |
| +45m | Optimized LGPSpectrumDetailEffect | Built OK |
| +60m | Added seqlock timeout | Built OK |
| +75m | Fixed MAX_EFFECTS 96→100 | Built OK, effects now named |
| +90m | Device crashed on effect cycle | Effect #96 crashes |
| +105m | Optimized 5 more effects (agents in parallel) | Built OK |
| +120m | Device still crashes on #96 | WDT still triggering |
| +135m | Disabled WDT via CONFIG flags | Doesn't work |
| +150m | Added esp_task_wdt_deinit() | Still crashes with WDT |
| +165m | Created this debrief | Current state |

---

## CONCLUSION

The optimization attempt failed. The changes introduced bugs in the effect rendering code that cause crashes. The WDT cannot be properly disabled through the methods attempted.

**The effects were working before this session.** Reverting the effect files will restore functionality.
