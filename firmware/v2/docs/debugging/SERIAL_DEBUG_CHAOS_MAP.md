# Serial Debug Chaos Map

**Date:** 2026-01-22
**Status:** THE CURRENT STATE IS A DOG'S BREAKFAST

---

## Executive Summary: Why It's a Mess

1. **THREE independent logging systems** with different verbosity models
2. **adbg levels are ADDITIVE, not EXCLUSIVE** — Level 4 includes 2+3+4
3. **DEFAULT verbosity=4** floods serial with 8+ lines every 2-10 seconds
4. **No unified gating** — ESP_LOG, LW_LOG, Serial.print all independent
5. **"Verbosity" means different things** depending on which system you're in

---

## The Three Logging Systems (Collision Course)

### System 1: LW_LOG* (Unified Logging)
| Setting | Location | Values | Default |
|---------|----------|--------|---------|
| `LW_LOG_LEVEL` | platformio.ini | 0-4 | 4 (DEBUG) |

**Behaviour:**
- 0 = None, 1 = Error, 2 = Warn, 3 = Info, 4 = Debug
- **362 usages** across codebase
- Compile-time gating (macros compile out)
- Format: `[12345][INFO][Tag] message`

### System 2: ESP_LOG* (ESP-IDF Native)
| Setting | Location | Values | Default |
|---------|----------|--------|---------|
| `CORE_DEBUG_LEVEL` | platformio.ini | 0-5 | 4 (DEBUG) |

**Behaviour:**
- 0 = None, 1 = Error, 2 = Warn, 3 = Info, 4 = Debug, 5 = Verbose
- **85 usages** in Actor system, MessageBus, ColorCorrection
- Runtime + compile-time gating
- Format: `I (12345) TAG: message`

### System 3: AudioDebugConfig (adbg)
| Setting | Location | Values | Default |
|---------|----------|--------|---------|
| `verbosity` | AudioDebugConfig.h | 0-5 | **4 (Medium)** |
| `baseInterval` | AudioDebugConfig.h | frames | 62 (~1s) |

**Behaviour:**
- Runtime configurable via `adbg` serial command
- **ONLY affects audio debug output**
- **DOES NOT affect LW_LOG or ESP_LOG**

---

## adbg Verbosity Levels: What Each Actually Prints

### The CRITICAL Problem: Levels are ADDITIVE

```
Level 5 = Level 4 + Level 3 + Level 2 + Level 5 specific
Level 4 = Level 3 + Level 2 + Level 4 specific
Level 3 = Level 2 + Level 3 specific
Level 2 = Level 2 specific
Level 1 = Errors only (but what errors?)
Level 0 = Nothing
```

### Level 0: Off
- **Claims:** No audio debug output
- **Actually prints:** Nothing from adbg-gated code
- **Still prints:** All LW_LOGI startup messages, ESP_LOG* actor messages

### Level 1: Minimal (Errors only)
- **Claims:** Errors only
- **Actually prints:** Nothing different from Level 0
- **Problem:** There's no `verbosity >= 1` checks anywhere except the comment

### Level 2: Status (10-second health)
- **Interval:** Every 620 ticks (~10 seconds)
- **Location:** `AudioActor.cpp:262-309`
- **Prints 5 SEPARATE log lines:**

```
[INFO][AudioActor] Audio alive: mic=-12.3dB cap=5000 pk=1234 pkC=567 rms=0.1234->0.567 pre=0.1234 g=1.50 dc=123.4 clip=0 flux=0.345 min=-1000 max=1000 mean=12.3
[INFO][AudioActor] Spike stats: frames=5000 detected=12 corrected=8 avg/frame=0.002 removed=0.15
[INFO][AudioActor] Saliency: overall=0.567 dom=2 H=0.234 R=0.567 T=0.123 D=0.345
[INFO][AudioActor] Style: 1 conf=0.89 [R=0.23 H=0.45 M=0.12 T=0.08 D=0.12]
[INFO][AudioActor] Beat: BPM=120.5 conf=0.85 phase=0.67 lock=YES
```

**That's 5 log lines every 10 seconds just from Level 2**

### Level 3: Low (+ DMA diagnostics)
- **Interval:** Every `baseInterval * 5` ticks (~5 seconds @ default)
- **Location:** `AudioCapture.cpp:131-143`
- **Adds 1 line:**

```
[INFO][AudioCapture] DMA: hop=15000 R=[FFFFFFFE..00000001] >>10=[-2..0]
```

**Total at Level 3: 5 lines every 10s + 1 line every 5s = 7 lines per 10s**

### Level 4: Medium (+ 64-bin Goertzel) — THE DEFAULT
- **Interval:** Every `baseInterval / 2` ticks (~0.5s @ default, but counter-based)
- **Location:** `AudioActor.cpp:802-808`
- **Adds 1 line (as LW_LOGD, which requires LW_LOG_LEVEL >= 4):**

```
[DEBUG][AudioActor] 64-bin Goertzel: [0.123 0.234 0.345 0.456 0.567 0.678 0.789 0.890]
```

**Total at Level 4: ~8+ lines per 10 seconds**

### Level 5: High (+ 8-band Goertzel)
- **Interval:** Every `baseInterval` ticks (~1 second)
- **Location:** `AudioActor.cpp:673-686`
- **Adds 1 MASSIVE line (as LW_LOGD):**

```
[DEBUG][AudioActor] Goertzel: -12.3dB raw=[0.1234 0.2345 0.3456 0.4567 0.5678 0.6789 0.7890 0.8901] map=[0.123 0.234 0.345 0.456 0.567 0.678 0.789 0.890] rms=0.1234->0.567 pre=0.1234 g=1.50 dc=123.4 clip=0 pk=1234 pkC=567 min=-1000 max=1000 mean=12.3
```

**Total at Level 5: ~12+ lines per 10 seconds**

---

## What Prints by Default (Out of the Box)

### Build: `esp32dev_audio_esv11` with defaults
| Setting | Value |
|---------|-------|
| `LW_LOG_LEVEL` | 4 (DEBUG) |
| `CORE_DEBUG_LEVEL` | 4 (DEBUG) |
| `adbg verbosity` | 4 (Medium) |

### At Boot (One-Time)
~50+ lines of LW_LOGI from setup():
- Actor system init
- Effect registration
- WiFi connection
- Plugin manager
- Zone composer
- Help menu

### During Runtime (Every 10 Seconds)
From adbg Level 2 (gated by `verbosity >= 2`):
```
[INFO] Audio alive: mic=-12.3dB ...
[INFO] Spike stats: frames=5000 ...
[INFO] Saliency: overall=0.567 ...
[INFO] Style: 1 conf=0.89 ...
[INFO] Beat: BPM=120.5 ...
```

### During Runtime (Every ~5 Seconds)
From adbg Level 3 (gated by `verbosity >= 3`):
```
[INFO] DMA: hop=15000 R=[FFFFFFFE..00000001] ...
```

### During Runtime (Every ~2 Seconds)
From adbg Level 4 (gated by `verbosity >= 4`):
```
[DEBUG] 64-bin Goertzel: [0.123 0.234 0.345 ...]
```

### From ESP_LOG (Actor System)
Random ESP_LOGI/LOGW from:
- MessageBus subscriptions
- Actor start/stop
- Queue utilization warnings
- Stack watermark warnings

### From Effect Changes
Every time you change effect:
```
[INFO] IEffect cleanup: OldEffect (ID 5)
[INFO] IEffect init: NewEffect (ID 12)
[INFO] Effect changed: 5 (OldEffect) -> 12 (NewEffect)
```

---

## The Root Problems

### Problem 1: adbg Levels Don't Make Sense

| Level | Name | Actual Behaviour |
|-------|------|------------------|
| 0 | Off | Works |
| 1 | Minimal | **DOES NOTHING** — no `>= 1` checks exist |
| 2 | Status | Prints 5 lines every 10s (way too verbose for "status") |
| 3 | Low | Adds DMA (why is DMA higher priority than status?) |
| 4 | Medium | Adds 64-bin (what makes this "medium"?) |
| 5 | High | Adds 8-band (why is 8-band less important than 64-bin?) |

**The hierarchy makes no logical sense:**
- Why does "Status" include Saliency, Style, Beat? Those are analysis details.
- Why is DMA at Level 3 but spectral analysis at Level 4-5?
- Level 1 (Minimal) is literally unused — zero code checks `>= 1`

### Problem 2: Gating is Inconsistent

| Check | Location | What it gates |
|-------|----------|---------------|
| `verbosity >= 2` | AudioActor.cpp:262 | 5 health/analysis lines |
| `verbosity >= 3` | AudioCapture.cpp:131 | DMA diagnostics |
| `verbosity >= 4` | AudioActor.cpp:802 | 64-bin Goertzel |
| `verbosity >= 5` | AudioActor.cpp:673 | 8-band Goertzel |

**Notice:** Nothing uses `verbosity >= 1`. The "Minimal - Errors only" level is a lie.

### Problem 3: Three Verbosity Systems Collide

To get silence you need:
```ini
# platformio.ini
-D LW_LOG_LEVEL=0      # Silence LW_LOG*
-D CORE_DEBUG_LEVEL=0  # Silence ESP_LOG*
```
AND at runtime:
```
adbg 0                  # Silence audio debug
```

**But even then**, direct Serial.printf calls (433 of them) still print.

### Problem 4: No "Clean" Default

A sensible default would be:
- Boot messages: Yes
- Runtime spam: No
- Errors/warnings: Yes
- User-triggered status: Yes (via command)

**Current default (verbosity=4) gives you:**
- 50+ boot messages
- 8+ lines every 10 seconds
- Errors AND info AND debug
- No way to get "just errors" without recompile

---

## What Should Happen

### Proposed adbg Redesign

| Level | Name | Should Print |
|-------|------|--------------|
| 0 | Off | Nothing from audio subsystem |
| 1 | Errors | Only actual errors (capture fail, AGC clip, etc.) |
| 2 | Warnings | Errors + warnings (stack low, spike correction) |
| 3 | Status | Warnings + periodic health (1 line every 30s) |
| 4 | Analysis | Status + Goertzel/Saliency (on demand or every 10s) |
| 5 | Trace | Everything including DMA, raw samples |

### Proposed Default: Level 2 (Warnings)
- No runtime spam
- Errors always visible
- Warnings for actionable issues
- `adbg status` for on-demand health check

### Proposed Serial Command Additions
```
adbg             - Show current level
adbg <0-5>       - Set level
adbg status      - Print health NOW (one-shot, any level)
adbg spectrum    - Print spectrum NOW (one-shot, any level)
adbg config      - Show all debug config
```

---

## Files That Need Changes

| File | Changes Needed |
|------|----------------|
| `src/audio/AudioDebugConfig.h` | Redefine levels, add `printStatus()`, `printSpectrum()` |
| `src/audio/AudioActor.cpp` | Restructure verbosity checks, add one-shot methods |
| `src/audio/AudioCapture.cpp` | Move DMA to appropriate level |
| `src/main.cpp` | Add `adbg status` / `adbg spectrum` commands |
| `src/config/features.h` | Document the three systems clearly |

---

## Appendix: Every Periodic Print Source

| Source | Interval | Level Gate | Lines |
|--------|----------|------------|-------|
| AudioActor "Audio alive" | 10s | `verbosity >= 2` | 1 |
| AudioActor "Spike stats" | 10s | `verbosity >= 2` | 1 |
| AudioActor "Saliency" | 10s | `verbosity >= 2` | 1 |
| AudioActor "Style" | 10s | `verbosity >= 2` | 1 |
| AudioActor "Beat" | 10s | `verbosity >= 2` | 1 |
| AudioCapture "DMA" | 5s | `verbosity >= 3` | 1 |
| AudioActor "64-bin Goertzel" | ~2s | `verbosity >= 4` | 1 |
| AudioActor "Goertzel 8-band" | ~1s | `verbosity >= 5` | 1 |
| Actor "Stack low" | varies | ESP_LOGW | 1 |
| MessageBus | varies | ESP_LOGD | varies |

**Total at default (verbosity=4): ~8 lines every 10 seconds minimum**
