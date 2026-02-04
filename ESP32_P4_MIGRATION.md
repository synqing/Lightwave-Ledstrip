# ESP32-S3 to ESP32-P4 Migration Plan

**Project:** LightwaveOS v2 Firmware
**Target:** ESP32-P4 Function EV Board v1.4
**Current Platform:** ESP32-S3-DevKitC-1
**Status:** Phase 1 In Progress
**Branch:** `feat/esp32-p4-migration`

---

## Executive Summary

This document outlines the migration of LightwaveOS v2 LED control firmware from ESP32-S3 (Xtensa LX7 @ 240MHz) to ESP32-P4 (RISC-V HP @ 400MHz). Analysis by 10 specialist agents confirms the migration is **FEASIBLE** with moderate effort.

| Metric | Value |
|--------|-------|
| **Feasibility** | FEASIBLE with significant work |
| **Code Portability** | 70% portable without modification |
| **Estimated Effort** | 82-125 hours (12 weeks calendar) |
| **Risk Level** | MEDIUM-HIGH |

### Critical Blockers

1. **No network on P4**: P4 has **NO integrated WiFi** and we will **not** use Ethernet. Network and web control must be removed or gated off for P4.
2. **I2S Audio Driver**: Uses legacy driver with S3-specific registers - complete rewrite required
3. **Toolchain**: Must pin to ESP-IDF **v5.5.2** with hard gates (reuse proven P4 toolchain)

---

## Hardware Comparison

| Feature | ESP32-S3 | ESP32-P4 | Impact |
|---------|----------|----------|--------|
| **CPU** | Dual Xtensa LX7 @ 240 MHz | Dual RISC-V HP @ 400 MHz | 66% faster |
| **Architecture** | Xtensa (proprietary) | RISC-V (open) | Recompile needed |
| **SRAM** | 384 KB | 768 KB | 2x more RAM |
| **PSRAM** | 8 MB (OPI) | 32 MB (OPI) | 4x more |
| **WiFi** | Integrated 802.11 b/g/n | **None** | BLOCKING |
| **Bluetooth** | BLE 5.0 | **None** | Lost feature |
| **Ethernet** | None | 10/100 MAC | Not used |
| **GPIO** | 45 pins | 55 pins | More I/O |
| **RMT Channels** | 8 | 4 | Fewer LED channels |
| **USB** | USB 2.0 FS | USB 2.0 HS | Faster transfers |

---

## Architecture Decision

### Selected: Unified Codebase with Compile-Time HAL

```
firmware/v2/
├── src/
│   ├── hal/
│   │   ├── interface/
│   │   │   ├── IAudioCapture.h      # Audio capture abstraction
│   │   │   ├── ILedDriver.h         # LED driver abstraction
│   │   │   └── INetworkDriver.h     # Network abstraction
│   │   ├── HalFactory.h             # Compile-time type selection
│   │   ├── esp32s3/                 # S3 implementations (future)
│   │   │   ├── AudioCapture_S3.cpp
│   │   │   ├── LedDriver_S3.cpp
│   │   │   └── WiFiDriver_S3.cpp
│   │   └── esp32p4/                 # P4 implementations (future)
│   │       ├── AudioCapture_P4.cpp
│   │       ├── LedDriver_P4.cpp
│   │       └── NetworkDisabled_P4.cpp
│   ├── config/
│   │   ├── chip_config.h            # Platform auto-detection
│   │   ├── chip_esp32s3.h           # S3-specific constants
│   │   └── chip_esp32p4.h           # P4-specific constants
│   └── ... (existing code unchanged)
```

**Rationale:**
- Single codebase reduces maintenance burden
- Shared bug fixes and features across platforms
- Zero runtime overhead (compile-time abstraction)
- Future-proof for ESP32-C6, P5, etc.

---

## Migration Phases

### Phase Gates (Hard vs Nice-to-have)

**Hard gates (must pass before proceeding):**
- Phase 0 → Phase 1: Blink + serial OK; ESP-IDF v5.5.2 boot log confirmed; S3 baseline metrics recorded.
- Phase 1 → Phase 2: P4 build completes with v5.5.2 gate; toolchain scripts in place; no v6.0-dev contamination.
- Phase 2 → Phase 3: 320 LEDs stable at target frame rate; LED output does not block render loop.
- Phase 3 → Phase 4: P4 build runs with network disabled; S3 networking unaffected.
- Phase 4 → Phase 5: Audio capture stable; no WDT; hop rate and overruns within gate.

**Nice-to-have (non-blocking, can slip):**
- Optional ESP-DSP acceleration once baseline audio capture is stable.
- Additional profiling and frame-time instrumentation beyond minimum.

### Phase 0: Pre-Migration Validation (Week 1)
- [ ] Acquire ESP32-P4 Function EV Board (×2 units recommended)
- [ ] Flash minimal test firmware (LED blink)
- [ ] Verify serial communication (115200 baud)
- [ ] Archive S3 branch: `git checkout -b archive/s3-production`
- [ ] Capture baseline metrics from S3 (120 FPS, 20ms audio latency)
- [ ] Confirm LED data pins: GPIO 20 and GPIO 21 (P4)
- [x] Document I2S pin map for P4 onboard audio front end (BCK 12, WS 10, DIN 11, DOUT 9, MCLK 13; I2C SCL 8, SDA 7; PA_EN 53)
- [ ] Install ESP-IDF v5.5.2 and verify `IDF_PATH`
- [ ] Run P4 toolchain gate: boot log must show `ESP-IDF v5.5.2` (no v6.0-dev)
- [ ] Confirm P4 flash size and PSRAM config (align with 32MB/OPI as applicable)
- [ ] Confirm onboard codec/init path (ES8311 or direct I2S mic) and required I2C init sequence

### Phase 1: Build System Setup (Week 2)
- [x] Create feature branch: `feat/esp32-p4-migration`
- [x] Create HAL interface directory structure
- [x] Define platform-agnostic interfaces (IAudioCapture, ILedDriver, INetworkDriver)
- [x] Create platform detection headers (chip_config.h, chip_esp32s3.h, chip_esp32p4.h)
- [x] Add `[env:esp32p4_idf]` to platformio.ini
- [ ] Pin ESP-IDF v5.5.2 and add hard build gates (reuse P4 toolchain scripts)
- [x] Import P4 toolchain scripts to `LightwaveOS-P4` (`build_with_idf55.sh`, `idf_version_pin.sh`, `clean_idf6.sh`, `verify_build.sh`)
- [ ] Test compilation with ESP-IDF v5.5.2 on P4 hardware

### Phase 2: Core LED Output (Weeks 3-4)
- [ ] Test FastLED 3.10.0 compatibility with P4 RMT driver
- [ ] If FastLED fails, implement ESP-IDF `led_strip` component wrapper
- [ ] Create `LedDriver_S3.cpp` (extract from existing code)
- [ ] Create `LedDriver_P4.cpp` (new implementation)
- [ ] Port `RendererActor` to use HAL
- [ ] Map LED outputs to GPIO 20 and GPIO 21 (P4)
- [ ] Validate 320 LEDs @ 120 FPS (8.33ms frame budget)
- [ ] Document LED send time (WS2812 @ 800 kHz) and confirm render loop is non-blocking

### Phase 3: Network Removal (Weeks 5-6)
- [ ] Disable network features for P4 builds (compile-time flags)
- [ ] Remove P4 network bring-up and web server dependencies
- [ ] Ensure S3 networking remains intact and unchanged
- [ ] Define P4 control path (serial/USB only) and document behaviour

### Phase 4: Audio Processing (Weeks 7-8)
- [ ] **CRITICAL**: Rewrite I2S driver from legacy to std mode
- [ ] Remove S3-specific register manipulation
- [ ] Port SPH0645 microphone configuration
- [ ] Test 12.8kHz sample rate, 256-sample hops
- [ ] Validate Goertzel analyzer, TempoTracker, ChromaAnalyzer
- [ ] Test audio-reactive effects
- [ ] Add hop-rate/overrun/WDT gates (from P4 audio producer) to boot log checks:
      hop_rate capture=125 Hz ±2, fast=125 Hz ±2; overruns=0; WDT triggers=0 over 10-minute run

### Phase 5: Full Feature Parity (Weeks 9-10)
- [ ] Test all 76 effects (visual verification)
- [ ] Verify CENTER ORIGIN compliance
- [ ] Test zone system (up to 4 zones)
- [ ] Validate transition engine (12 types)
- [ ] Test M5ROTATE8 encoder (I2C)
- [ ] Test OTA updates
- [ ] Performance benchmarking (target: ≥120 FPS)

### Phase 6: Stabilization (Weeks 11-12)
- [ ] 72-hour stress test (monitor heap, crashes, thermal)
- [ ] Memory leak analysis (<1KB/hour acceptable)
- [ ] Update documentation (CLAUDE.md, API docs)
- [ ] CI/CD pipeline for dual-platform builds

---

## Files Requiring Modification

### BLOCKING (Must Change)

| File | Lines | Change Type |
|------|-------|-------------|
| `src/audio/AudioCapture.cpp` | ~250 | Complete I2S rewrite |
| `src/audio/AudioCapture.h` | ~80 | New I2S handle types |
| `src/config/audio_config.h` | ~146 | New I2S config structs |
| `platformio.ini` | ~360 | Keep S3 build only; P4 uses ESP-IDF toolchain |
| `src/network/WiFiManager.cpp` | ~957 | Gate out for P4 |
| `src/network/WiFiManager.h` | ~543 | Gate out for P4 |

### HIGH PRIORITY (Should Change)

| File | Lines | Change Type |
|------|-------|-------------|
| `src/core/actors/RendererActor.cpp` | ~900 | HAL abstraction |
| `src/hal/led/FastLedDriver.cpp` | ~500 | GPIO remapping |
| `src/core/actors/Actor.h` | ~600 | Increase stack sizes 12-25% |
| `src/sync/SyncProtocol.h` | ~250 | Board identification |
| `src/network/WebServer.cpp` | ~1480 | Gate out for P4 |

### NEW FILES (Created in Phase 1)

| File | Purpose |
|------|---------|
| `src/hal/interface/IAudioCapture.h` | Audio HAL interface |
| `src/hal/interface/ILedDriver.h` | LED HAL interface |
| `src/hal/interface/INetworkDriver.h` | Network HAL interface |
| `src/hal/HalFactory.h` | Compile-time type selection |
| `src/config/chip_config.h` | Platform auto-detection |
| `src/config/chip_esp32s3.h` | S3 constants |
| `src/config/chip_esp32p4.h` | P4 constants |

---

## ESP-IDF Toolchain (P4)

P4 builds must match the proven toolchain from `p4_audio_producer`.
Scripts live in `LightwaveOS-P4`:
`build_with_idf55.sh`, `idf_version_pin.sh`, `clean_idf6.sh`, `verify_build.sh`.

### Required Version
- ESP-IDF **v5.5.2** only (hard gate)

### Build Commands (P4)

```bash
cd firmware/v2

# Ensure IDF_PATH points to ESP-IDF v5.5.2
export IDF_PATH=$HOME/esp/esp-idf-v5.5.2

# Verify version gate (fails if not v5.5.2)
../LightwaveOS-P4/idf_version_pin.sh

# Source ESP-IDF and build (recommended script)
. $IDF_PATH/export.sh
./p4_build.sh
```

### Version Gate (P4)
Boot log must show:
- `boot: ESP-IDF v5.5.2 ...`
- `app_init: ESP-IDF: v5.5.2 ...`

---

## Effort Estimates

| Component | Hours | Risk |
|-----------|-------|------|
| HAL Interface Definition | 8-12 | LOW |
| I2S Audio Driver Rewrite | 20-30 | **HIGH** |
| Network removal / gating | 12-20 | MEDIUM |
| FastLED/RMT Configuration | 10-15 | MEDIUM |
| FreeRTOS Task Tuning | 8-12 | MEDIUM |
| Build System | 4-6 | LOW |
| Testing & Validation | 20-30 | MEDIUM |
| **TOTAL** | **82-125** | **MEDIUM-HIGH** |

---

## Risk Matrix

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| FastLED P4 incompatibility | MEDIUM | CRITICAL | Fallback to ESP-IDF `led_strip` |
| Loss of network features on P4 | HIGH | MEDIUM | Accept reduced feature set on P4 |
| I2S driver timing problems | MEDIUM | CRITICAL | Use new std mode driver API |
| ESP-IDF version drift | MEDIUM | HIGH | Enforce v5.5.2 gate at build and boot |
| Performance regression | LOW | HIGH | Profile and optimize |

---

## Acceptance Criteria

### Hard Gates
- [ ] Phase 0 gate met (blink + serial + IDF v5.5.2 + S3 baseline)
- [ ] LED output does not block render loop (measured)
- [ ] Audio capture stable: hop_rate capture=125 Hz ±2, fast=125 Hz ±2; overruns=0; WDT triggers=0 over 10 minutes

### Nice-to-have
- [ ] ESP-DSP acceleration integrated (optional)
- [ ] Additional profiling instrumentation for render/LED send time

### Performance Targets
- [ ] Frame rate ≥120 FPS sustained
- [ ] Audio latency ≤25ms
- [ ] Memory leak rate <1KB/hour
- [ ] Zero crashes in 72h stress test

### Feature Parity
- [ ] All 76 effects render identically to S3
- [ ] Network and web UI are **disabled** on P4 (S3 retains full web control)
- [ ] Zone system operational (4 zones)
- [ ] Transition engine functional (12 types)

### Hardware Verification
- [ ] 320 LEDs illuminate correctly (GPIO 20/21)
- [ ] Audio-reactive effects respond to music
- [ ] Serial monitor shows 120+ FPS

---

## Rollback Plan

If migration fails at any phase:

**Abort Triggers:**
- Unable to achieve 100 FPS in Phase 2
- Network removal causes P4 build instability
- Audio pipeline unusable in Phase 4
- Schedule exceeds 15 weeks (including 20% buffer)

**Actions:**
1. Document failure mode and root cause
2. Revert to S3 production branch (`archive/s3-production`)
3. Re-evaluate in 6 months (wait for ecosystem maturity)

---

## Agent Analysis Summary

Ten specialist agents conducted comprehensive analysis:

| Agent | Key Finding |
|-------|-------------|
| Hardware Compatibility | P4: 400MHz RISC-V, no WiFi, Ethernet MAC unused |
| C++ Code Analysis | 70% portable, only I2S needs rewrite |
| Architecture Strategy | Unified codebase with compile-time HAL recommended |
| Network Layer | WiFi blocking; remove network on P4 |
| Deep Technical Audit | 102K LOC, well-architected for migration |
| Build System | ESP-IDF v5.5.2 pinned; S3 stays PlatformIO |
| Audio Subsystem | I2S legacy→std mode driver migration critical |
| Effects System | All 76 effects are pure computation, fully portable |
| Toolchain Investigation | ESP-IDF 5.5.2 required; reuse P4 toolchain scripts |
| Migration Strategy | 6-phase, 12-week roadmap with gates |

---

## Quick Reference

### P4 I2S Pin Map (Onboard Audio Front End)

From `p4_audio_producer` reference config:

```cpp
// I2S
BCK = GPIO 12
WS  = GPIO 10
DIN = GPIO 11
DOUT = GPIO 9
MCLK = GPIO 13

// I2C (audio codec control)
SCL = GPIO 8
SDA = GPIO 7

// Audio PA enable
PA_EN = GPIO 53
```

### Chip Detection in Code

```cpp
#include "config/chip_config.h"

#if CHIP_ESP32_P4
    // P4-specific code
    #include "hal/esp32p4/NetworkDisabled_P4.h"
#else
    // S3-specific code (default)
    #include "hal/esp32s3/WiFiDriver_S3.h"
#endif

// Runtime check
if (chip::isESP32P4()) {
    Serial.println("Running on ESP32-P4");
}
```

### Platform Constants

```cpp
// From chip_esp32p4.h
namespace chip {
    constexpr uint32_t CPU_FREQ_MHZ = 400;
    constexpr bool HAS_INTEGRATED_WIFI = false;
    constexpr bool HAS_ETHERNET = false;  // MAC present but unused
    constexpr uint8_t RMT_CHANNELS = 4;

    namespace task {
        constexpr float STACK_MULTIPLIER = 1.2f;  // RISC-V needs more stack
    }
}
```

---

## Next Steps

1. **Hardware**: Acquire ESP32-P4 Function EV Board (×2)
2. **Environment**: Install ESP-IDF v5.5.2 and confirm toolchain gates
3. **Validate**: Flash minimal firmware, verify serial output
4. **Proceed**: Begin Phase 2 (LED output) upon successful boot

---

## References

- [ESP32-P4 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32-p4_technical_reference_manual_en.pdf)
- [ESP32-P4 Function EV Board User Guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/user_guide.html)
- [FastLED ESP32-P4 Support](https://github.com/FastLED/FastLED/releases/tag/3.10.0)
- [ESP-IDF I2S Standard Mode Driver](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/api-reference/peripherals/i2s.html)

---

*Document generated: 2026-01-24*
*Last updated: Phase 1 Complete*
