# Zone Mixer Input Layer Rebuild — Claude Code CLI Prompt

**Status:** READY FOR EXECUTION
**Date:** 2026-04-02
**Scope:** Rebuild the I2C input layer (4 files). Networking, protocol, parameter mapping, LED feedback — all untouched.

---

## Context for the Agent

You are rebuilding the I2C input layer of the Zone Mixer, a physical controller for the K1 LightwaveOS LED system. The Zone Mixer is an AtomS3 (ESP32-S3) connected to 6 I2C devices via a PaHub v2.1 (PCA9548A multiplexer): 2× M5Rotate8 8-encoder modules and 4× M5Stack Unit-Scroll encoders.

**The input layer is completely broken.** The root cause is a fundamental I2C protocol violation in the M5ROTATE8 library: its `read8()`, `read24()`, and `read32()` methods call `Wire.endTransmission()` (which sends a STOP condition) before `Wire.requestFrom()`. On Arduino Core 3.x (pioarduino 54.03.21, IDF 5.x) with a PCA9548A multiplexer, the STOP corrupts the I2C-ng driver state machine, causing `ESP_ERR_INVALID_STATE` on all subsequent transactions.

**The fix has two parts:**
1. Patch the M5ROTATE8 library — 4 surgical `endTransmission()` → `endTransmission(false)` changes in read functions
2. Rebuild the input manager with proper PaHub initialisation, error recovery, and the proven architectural pattern from a validated reference project

**Everything above the input layer works.** WiFi, WebSocket, parameter mapping, echo guard, LED feedback, message handlers — all solid and tested. Do NOT touch them.

---

## MANDATORY Pre-Reading (in this order)

1. `zone-mixer/docs/DESIGN.md` — Hardware design, 8-phase roadmap, physical layout
2. `zone-mixer/lib/M5ROTATE8/m5rotate8.h` — M5ROTATE8 class interface (91 lines)
3. `zone-mixer/lib/M5ROTATE8/m5rotate8.cpp` — **THE BUG IS HERE.** Read all `endTransmission()` calls (318 lines)
4. `zone-mixer/lib/M5UnitScroll/src/M5UnitScroll.h` — M5UnitScroll class interface (52 lines)
5. `zone-mixer/lib/M5UnitScroll/src/M5UnitScroll.cpp` — **THE CORRECT PATTERN.** Line 42: `endTransmission(false)` (173 lines)
6. `zone-mixer/src/config.h` — Hardware constants, pin definitions, I2C addresses, PaHub channels
7. `zone-mixer/src/input/InputManager.h` — Current broken implementation (read to understand the interface contract — callbacks, input IDs, poll architecture)
8. `zone-mixer/src/input/PaHub.h` — Current broken implementation
9. `zone-mixer/src/input/ScrollReader.h` — Current broken implementation (uses raw register access instead of M5UnitScroll library)
10. `zone-mixer/src/input/Rotate8Reader.h` — Current broken M5ROTATE8 wrapper
11. `zone-mixer/src/input/DetentDebounce.h` — Encoder step normalisation filter (keep — still needed)
12. `zone-mixer/src/main.cpp` — Focus on `setup()` and `InputManager` usage (the I2C init section needs rewriting)
13. `zone-mixer/src/parameters/ParameterMapper.h` — Understand how it consumes input events (inputId, delta, isButton)

---

## Hardware Topology (VERIFIED on physical hardware, Feb 19, 2026)

```
AtomS3 (ESP32-S3, GPIO 2 = SDA, GPIO 1 = SCL, 400 kHz)
  └── PaHub v2.1 (PCA9548A, addr 0x70, 6 channels)
       ├── CH0: M5Rotate8  (addr 0x42) — "EncA" Zone Selection Bank
       ├── CH1: M5Rotate8  (addr 0x41) — "EncB" Mix & Config
       ├── CH2: Unit-Scroll (addr 0x40) — Zone 1 brightness (Cyan)
       ├── CH3: Unit-Scroll (addr 0x40) — Zone 2 brightness (Green)
       ├── CH4: Unit-Scroll (addr 0x40) — Zone 3 brightness (Orange)
       └── CH5: Unit-Scroll (addr 0x40) — Master brightness (White)
```

**Input ID mapping (20 total controls):**
- IDs 0–7: EncA encoders (CH0, addr 0x42)
- IDs 8–15: EncB encoders (CH1, addr 0x41)
- IDs 16–18: Scroll wheels — Zone 1/2/3 brightness (CH2/3/4, addr 0x40)
- ID 19: Scroll wheel — Master brightness (CH5, addr 0x40)

**All 4 Unit-Scrolls share addr 0x40.** They are differentiated ONLY by PaHub channel. The M5Rotate8 units have distinct addresses (0x42 and 0x41) but are also on separate PaHub channels.

---

## Deliverables

### Phase 0: Patch M5ROTATE8 Library (SURGICAL — 4 lines)

**File: `zone-mixer/lib/M5ROTATE8/m5rotate8.cpp`**

The bug: `read8()`, `read24()`, and `read32()` call `_wire->endTransmission()` (sends STOP) before `_wire->requestFrom()`. On Arduino Core 3.x with PCA9548A, the STOP between write-address and read-data corrupts the I2C-ng state machine. The fix: use `endTransmission(false)` to send a REPEATED START instead.

**Exact changes (4 READ functions only — do NOT change write functions or isConnected):**

| Line | Current | Patched |
|------|---------|---------|
| 224 | `_error = _wire->endTransmission();` | `_error = _wire->endTransmission(false);` |
| 255 | `_error = _wire->endTransmission();` | `_error = _wire->endTransmission(false);` |
| 297 | `_error = _wire->endTransmission();` | `_error = _wire->endTransmission(false);` |

These are in `read8()` (line 224), `read24()` (line 255), and `read32()` (line 297).

**Do NOT patch:**
- Line 45 (`isConnected()`) — This is a probe, not a read. STOP is correct here.
- Lines 215, 246, 288 (`write8()`, `write24()`, `write32()`) — These are writes. STOP after write is correct.

**Why `endTransmission(false)` is correct:** It sends a REPEATED START condition instead of STOP. The I2C transaction becomes: START → write(addr) → write(reg) → REPEATED START → read(addr) → read(data) → STOP. This is the standard I2C register-read pattern. The M5UnitScroll library already does this correctly (line 42 of M5UnitScroll.cpp).

### Phase 1: Rebuild PaHub.h

**File: `zone-mixer/src/input/PaHub.h`**

Complete rewrite. The current implementation never verifies the PCA9548A is present, ignores `selectChannel()` return values, and has no `disableAll()` on init.

**Requirements:**
- `init()` — Probe addr 0x70 with `Wire.beginTransmission(0x70); Wire.endTransmission()`. If not 0, log error and return false. Then call `disableAll()`. Add 10ms settle delay after init.
- `selectChannel(uint8_t ch)` — Write `(1 << ch)` to addr 0x70. Check return value of `endTransmission()`. Return bool. Add 100µs settle delay after channel switch (STM32F030 in the PaHub needs this — validated on hardware).
- `disableAll()` — Write 0x00 to addr 0x70. Check return value.
- All functions take `TwoWire&` as parameter (do not store a global Wire reference).
- Log all errors via `Serial.printf()` or `ESP_LOGE`.

**Pattern from validated reference project:**
```cpp
// Channel selection is a direct byte write to 0x70
Wire.beginTransmission(0x70);
Wire.write(1 << channel);  // Enable exactly one channel
uint8_t err = Wire.endTransmission();
delayMicroseconds(100);     // PaHub settle time
```

### Phase 2: Rebuild ScrollReader.h

**File: `zone-mixer/src/input/ScrollReader.h`**

Replace the current raw-register implementation with a wrapper around the official M5UnitScroll library (already in `zone-mixer/lib/M5UnitScroll/`).

**Requirements:**
- Constructor takes `M5UnitScroll&` reference (shared instance, not owned)
- `poll()` — Reads incremental encoder value via `getIncEncoderValue()` and button state via `getButtonStatus()`
- Delta callback: fires when `getIncEncoderValue()` returns non-zero
- Button callback: fires on press (transition from not-pressed to pressed)
- LED control: `setColour(uint32_t rgb)` wrapping `setLEDColor()`
- No direct Wire/register access — ALL I2C goes through M5UnitScroll library methods

**Key M5UnitScroll API (from the library):**
```cpp
int16_t getIncEncoderValue();  // Returns delta since last read, auto-resets
bool getButtonStatus();         // true = pressed
void setLEDColor(uint32_t color, uint8_t index = 0);
void resetEncoder();
```

**Architecture note:** The reference project used a single M5UnitScroll instance reused across channels. PaHub channel selection happens BEFORE each device read in InputManager — ScrollReader does not know about channels. It just reads from whatever device is currently selected on the I2C bus.

### Phase 3: Rebuild Rotate8Reader.h

**File: `zone-mixer/src/input/Rotate8Reader.h`**

Wrapper around the now-patched M5ROTATE8 library.

**Requirements:**
- Constructor takes `M5ROTATE8&` reference
- `poll()` — Reads all 8 encoders via `getRelCounter(ch)` and buttons via `getKeyPressed(ch)`
- Uses `DetentDebounce` (from `DetentDebounce.h`, keep as-is) to normalise M5Rotate8's ±2 detent steps (with occasional ±1 noise) into clean ±1 output per step
- Delta callback: fires per-encoder when normalised delta is non-zero
- Button callback: fires per-encoder on button press
- LED control: `setLED(uint8_t ch, uint8_t r, uint8_t g, uint8_t b)` wrapping `writeRGB()`
- After reading relative counters, reset them: `resetCounter(ch)` — the M5ROTATE8 relative counter is cumulative, not auto-resetting like the Unit-Scroll incremental register

**Key M5ROTATE8 API (from the patched library):**
```cpp
int32_t getRelCounter(uint8_t channel);  // Cumulative since last reset
bool resetCounter(uint8_t channel);       // Reset relative counter to 0
bool getKeyPressed(uint8_t channel);      // true = currently pressed
bool writeRGB(uint8_t channel, uint8_t R, uint8_t G, uint8_t B);
```

**DetentDebounce contract (keep existing implementation):**
- Input: raw encoder delta (±1 or ±2 per physical detent)
- Output: normalised ±1 per physical detent
- 60ms minimum interval between emits
- Pairs half-detents (two ±1 arrivals) into single ±1 output

### Phase 4: Rebuild InputManager.h

**File: `zone-mixer/src/input/InputManager.h`**

The orchestrator. Owns device instances, manages PaHub channel switching, polls all 6 devices at 100 Hz.

**Requirements:**
- Owns: 1× M5UnitScroll instance (shared across 4 scroll channels), 2× M5ROTATE8 instances (EncA at 0x42, EncB at 0x41), 4× ScrollReader wrappers, 2× Rotate8Reader wrappers
- `begin()`:
  1. Do NOT call `Wire.begin()` — let `M5UnitScroll::begin()` handle it (line 15 of M5UnitScroll.cpp calls `_wire->begin(_sda, _scl)`)
  2. Call `PaHub::init()` to probe and reset the multiplexer
  3. For each of the 6 PaHub channels, select the channel and probe the device (M5ROTATE8::isConnected() for CH0/CH1, M5UnitScroll::getDevStatus() for CH2–5)
  4. Log which devices were found. If any are missing, log error but continue (partial operation is better than no operation)
  5. Set initial LED colours per the zone colour scheme (defined in config.h)
  6. Call `PaHub::disableAll()` after init
- `poll()`:
  1. Select CH0 → poll EncA (8 encoders) → 100µs settle
  2. Select CH1 → poll EncB (8 encoders) → 100µs settle
  3. Select CH2 → poll Scroll 0 (Zone 1) → 100µs settle
  4. Select CH3 → poll Scroll 1 (Zone 2) → 100µs settle
  5. Select CH4 → poll Scroll 2 (Zone 3) → 100µs settle
  6. Select CH5 → poll Scroll 3 (Master) → 100µs settle
  7. Call `esp_task_wdt_reset()` or `yield()` after every 2 channels to prevent WDT panic (full cycle is ~3.9ms, WDT threshold is 5s, but consecutive I2C without yield can trigger Core 0 WDT on ESP32-S3)
- Callback: `void onInputChange(uint8_t inputId, int16_t delta, bool isButton)` — fires for every encoder delta or button press, with the correct input ID (0–19)
- Error recovery: If any I2C transaction returns error, log it and skip that device for this poll cycle. Do NOT abort the entire poll. Do NOT retry immediately (retry on next poll cycle). Count consecutive errors per device — if 10 consecutive errors, attempt device re-init.

**The M5UnitScroll reuse pattern:**
```cpp
// Single M5UnitScroll instance, reused across 4 PaHub channels
M5UnitScroll m_scroll;  // begin() called once at startup

// In poll(), for each scroll channel:
PaHub::selectChannel(ch);
delayMicroseconds(100);
int16_t delta = m_scroll.getIncEncoderValue();  // Reads from whatever device is on the bus
bool btn = m_scroll.getButtonStatus();
```

This works because M5UnitScroll methods use the addr stored at `begin()` time (0x40), and the PaHub physically routes that address to the correct physical device. One instance, four channels.

### Phase 5: Update main.cpp Setup

**File: `zone-mixer/src/main.cpp`**

Minimal changes — only the I2C init section in `setup()`.

**Remove:**
- Any `Wire.begin()` call (M5UnitScroll::begin() handles this)
- Any direct PaHub channel selection in setup
- Any raw I2C probing code

**Replace with:**
```cpp
// I2C initialisation — M5UnitScroll::begin() calls Wire.begin() internally
// PaHub init and device probing handled by InputManager::begin()
if (!inputManager.begin()) {
    Serial.println("[ERROR] InputManager init failed — check I2C wiring");
    // Continue anyway — WiFi/WS still useful for diagnostics
}
```

**Keep everything else in main.cpp untouched:** WiFi, WebSocket, message handlers, display, loop timing, parameter routing.

### Phase 6: Update platformio.ini

**File: `zone-mixer/platformio.ini`**

Remove the PIO registry dependency for M5ROTATE8 since we're using the patched local copy:

```ini
lib_deps =
    m5stack/M5Unified@^0.2.2
    bblanchon/ArduinoJson@^7.0.0
    links2004/WebSockets@^2.4.0
    ; M5UnitScroll — local lib/ copy (official M5Stack, uses endTransmission(false) correctly)
    ; M5ROTATE8 — local lib/ copy (patched: endTransmission(false) in read functions)
```

The local `lib/` copies take precedence over PIO registry when both exist, but removing the registry dep prevents PIO from downloading the unpatched version and creating confusion.

### Phase 7: Build and Verify

```bash
cd zone-mixer
pio run -e atoms3 2>&1
```

**Pass criteria:**
- Zero errors
- Zero warnings
- M5ROTATE8 library compiles from local `lib/` (not from `.pio/libdeps/`)
- Binary size within AtomS3 flash budget (~1.3 MB available)

---

## Hard Constraints (NON-NEGOTIABLE)

1. **K1 is AP-ONLY.** The Zone Mixer connects TO the K1's WiFi AP at 192.168.4.1. NEVER attempt to make the K1 connect to an external network.
2. **British English** in all comments, docs, logs, and UI strings: centre, colour, initialise, serialise, behaviour, normalise.
3. **Zero warnings** on build.
4. **No heap allocation in the poll loop.** `InputManager::poll()` runs at 100 Hz. All buffers static. No `String`, `new`, or `malloc` in the hot path.
5. **Do NOT touch networking or protocol code.** WiFiManager.h, WebSocketClient.h, ParameterMapper.h, EchoGuard.h, LedFeedback.h, and main.cpp message handlers are working and tested. Hands off.
6. **Input ID contract is locked.** IDs 0–7 = EncA, 8–15 = EncB, 16–18 = Scrolls Z1/Z2/Z3, 19 = Master. ParameterMapper depends on this mapping. Do not change it.
7. **PaHub channel assignments are locked.** CH0=EncA, CH1=EncB, CH2–5=Scrolls. Hardware wired, cannot change.
8. **100µs settle delay after every PaHub channel switch.** The STM32F030 in the PaHub needs this. Validated on hardware.
9. **M5UnitScroll::begin() handles Wire.begin().** Do NOT call Wire.begin() separately — double-init causes silent bus corruption on Arduino Core 3.x.
10. **Keep DetentDebounce.h as-is.** It works. Ported from the Tab5 project, validated there.

---

## Escalation Triggers

Stop and report (do not attempt to fix) if:

1. M5ROTATE8 with the `endTransmission(false)` patch still produces I2C errors through the PaHub — this means the bug is deeper than the STOP/START issue
2. M5UnitScroll::begin() fails on the first call — may indicate Wire/I2C-ng driver init issue with M5Unified
3. PaHub probe at 0x70 returns error — hardware wiring issue, cannot fix in software
4. Build pulls M5ROTATE8 from PIO registry instead of local `lib/` — dependency resolution conflict
5. Any change required to ParameterMapper.h or WebSocketClient.h — scope violation, escalate for approval

---

## File Manifest

### PATCH (1 file — 3 line changes)

| File | What changes |
|------|-------------|
| `zone-mixer/lib/M5ROTATE8/m5rotate8.cpp` | Lines 224, 255, 297: `endTransmission()` → `endTransmission(false)` in read8, read24, read32 |

### REBUILD (4 files — complete rewrite)

| File | What it does |
|------|-------------|
| `zone-mixer/src/input/PaHub.h` | PCA9548A multiplexer: init with probe, selectChannel with error check and settle delay, disableAll |
| `zone-mixer/src/input/ScrollReader.h` | M5UnitScroll wrapper: poll via getIncEncoderValue/getButtonStatus, LED colour, delta + button callbacks |
| `zone-mixer/src/input/Rotate8Reader.h` | M5ROTATE8 wrapper: poll 8 encoders via getRelCounter + resetCounter, DetentDebounce integration, button + LED |
| `zone-mixer/src/input/InputManager.h` | Orchestrator: owns devices, manages PaHub channels, 100 Hz poll loop, error counting, device re-init |

### MODIFY (2 files — minimal changes)

| File | What changes |
|------|-------------|
| `zone-mixer/src/main.cpp` | Remove Wire.begin() and raw I2C init. Replace with InputManager::begin() call. Keep everything else. |
| `zone-mixer/platformio.ini` | Remove `robtillaart/M5ROTATE8@^0.4.2` from lib_deps (use local patched copy) |

### DO NOT TOUCH

| File | Why |
|------|-----|
| `zone-mixer/src/network/WiFiManager.h` | Working — 3-state connection machine with exponential backoff |
| `zone-mixer/src/network/WebSocketClient.h` | Working — WebSocket with mutex, hello sequence, JSON dispatch |
| `zone-mixer/src/parameters/ParameterMapper.h` | Working — all 20 input→WS command mappings verified against K1 API |
| `zone-mixer/src/sync/EchoGuard.h` | Working — per-parameter holdoff preventing snapback |
| `zone-mixer/src/led/LedFeedback.h` | Working — 22 LEDs, dirty-flag flush at 10 Hz |
| `zone-mixer/src/input/DetentDebounce.h` | Working — encoder step normalisation, ported from Tab5 |
| `zone-mixer/lib/M5UnitScroll/` | Working — official library, correct I2C pattern |

### KEEP (reference — do not delete)

| File | Why |
|------|-----|
| `zone-mixer/docs/DESIGN.md` | Hardware design doc and 8-phase roadmap |
| `zone-mixer/docs/USER_GUIDE.md` | End-user guide for all 20 controls |

---

## Validation Checklist

After implementation, verify each item:

- [ ] `pio run -e atoms3` — zero errors, zero warnings
- [ ] M5ROTATE8 compiles from `lib/M5ROTATE8/` (check build log for `lib/M5ROTATE8/m5rotate8.cpp`)
- [ ] M5ROTATE8 `read8()` line 224 uses `endTransmission(false)`
- [ ] M5ROTATE8 `read24()` line 255 uses `endTransmission(false)`
- [ ] M5ROTATE8 `read32()` line 297 uses `endTransmission(false)`
- [ ] No `Wire.begin()` call in `main.cpp` setup
- [ ] PaHub::init() probes addr 0x70 and logs result
- [ ] PaHub::selectChannel() has 100µs settle delay
- [ ] InputManager::begin() initialises M5UnitScroll FIRST (this triggers Wire.begin())
- [ ] InputManager::poll() calls `PaHub::selectChannel()` before each device read
- [ ] InputManager callback signature: `void(uint8_t inputId, int16_t delta, bool isButton)`
- [ ] Input IDs preserved: 0–7=EncA, 8–15=EncB, 16–18=Scrolls, 19=Master
- [ ] ScrollReader uses M5UnitScroll library methods, NOT raw Wire register access
- [ ] Rotate8Reader uses DetentDebounce for step normalisation
- [ ] No `new`, `malloc`, or `String` in poll() hot path
- [ ] All comments and logs use British English
- [ ] No changes to WiFiManager, WebSocketClient, ParameterMapper, EchoGuard, or LedFeedback
- [ ] `robtillaart/M5ROTATE8@^0.4.2` removed from platformio.ini lib_deps

---

## Changelog
- 2026-04-02: Initial draft — input layer rebuild from CC CLI handover analysis
