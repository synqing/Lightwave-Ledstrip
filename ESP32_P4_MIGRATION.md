# ESP32-S3 to ESP32-P4 Migration Plan

**Project:** LightwaveOS v2 Firmware
**Target:** ESP32-P4 Function EV Board v1.4
**Current Platform:** ESP32-S3-DevKitC-1
**Status:** Phase 1 Complete
**Branch:** `feat/esp32-p4-migration`

---

## Executive Summary

This document outlines the migration of LightwaveOS v2 LED control firmware from ESP32-S3 (Xtensa LX7 @ 240MHz) to ESP32-P4 (RISC-V HP @ 400MHz). Analysis by 10 specialist agents confirms the migration is **FEASIBLE** with moderate effort.

| Metric | Value |
|--------|-------|
| **Feasibility** | FEASIBLE with significant work |
| **Code Portability** | 70% portable without modification |
| **Estimated Effort** | 88-135 hours (12 weeks calendar) |
| **Risk Level** | MEDIUM-HIGH |

### Critical Blockers

1. **WiFi**: P4 has **NO integrated WiFi** - requires Ethernet (built-in MAC) or external module
2. **I2S Audio Driver**: Uses legacy driver with S3-specific registers - complete rewrite required
3. **PlatformIO**: No official P4 support - requires pioarduino community fork

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
| **Ethernet** | None | 10/100 MAC | New capability |
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
│   │       └── EthernetDriver_P4.cpp
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

### Phase 0: Pre-Migration Validation (Week 1)
- [ ] Acquire ESP32-P4 Function EV Board (×2 units recommended)
- [ ] Flash minimal test firmware (LED blink)
- [ ] Verify serial communication (115200 baud)
- [ ] Archive S3 branch: `git checkout -b archive/s3-production`
- [ ] Capture baseline metrics from S3 (120 FPS, 20ms audio latency)

### Phase 1: Build System Setup (Week 2) ✅ COMPLETE
- [x] Create feature branch: `feat/esp32-p4-migration`
- [x] Create HAL interface directory structure
- [x] Define platform-agnostic interfaces (IAudioCapture, ILedDriver, INetworkDriver)
- [x] Create platform detection headers (chip_config.h, chip_esp32s3.h, chip_esp32p4.h)
- [x] Add `[env:esp32p4_idf]` to platformio.ini
- [ ] Test compilation with pioarduino fork (requires hardware)

### Phase 2: Core LED Output (Weeks 3-4)
- [ ] Test FastLED 3.10.0 compatibility with P4 RMT driver
- [ ] If FastLED fails, implement ESP-IDF `led_strip` component wrapper
- [ ] Create `LedDriver_S3.cpp` (extract from existing code)
- [ ] Create `LedDriver_P4.cpp` (new implementation)
- [ ] Port `RendererActor` to use HAL
- [ ] Validate 320 LEDs @ 120 FPS (8.33ms frame budget)

### Phase 3: Network Stack (Weeks 5-6)
- [ ] Implement `EthernetDriver_P4.cpp` using ESP-IDF Ethernet driver
- [ ] Create `INetworkDriver` abstraction for WiFi/Ethernet
- [ ] Port WebServer initialization for Ethernet
- [ ] Test ESPAsyncWebServer compatibility (may need sync fallback)
- [ ] Validate mDNS (`lightwaveos.local`)
- [ ] Test all 20+ REST endpoints and WebSocket commands

### Phase 4: Audio Processing (Weeks 7-8)
- [ ] **CRITICAL**: Rewrite I2S driver from legacy to std mode
- [ ] Remove S3-specific register manipulation
- [ ] Port SPH0645 microphone configuration
- [ ] Test 12.8kHz sample rate, 256-sample hops
- [ ] Validate Goertzel analyzer, TempoTracker, ChromaAnalyzer
- [ ] Test audio-reactive effects

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
- [ ] Ethernet reconnection testing
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
| `platformio.ini` | ~360 | Add P4 environment |
| `src/network/WiFiManager.cpp` | ~957 | Replace with Ethernet |
| `src/network/WiFiManager.h` | ~543 | Interface redesign |

### HIGH PRIORITY (Should Change)

| File | Lines | Change Type |
|------|-------|-------------|
| `src/core/actors/RendererActor.cpp` | ~900 | HAL abstraction |
| `src/hal/led/FastLedDriver.cpp` | ~500 | GPIO remapping |
| `src/core/actors/Actor.h` | ~600 | Increase stack sizes 12-25% |
| `src/sync/SyncProtocol.h` | ~250 | Board identification |
| `src/network/WebServer.cpp` | ~1480 | Ethernet initialization |

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

## PlatformIO Configuration

### New Environment: `[env:esp32p4_idf]`

```ini
[env:esp32p4_idf]
platform = https://github.com/pioarduino/platform-espressif32.git
board = esp32-p4-function-ev-board
framework = espidf
board_build.f_cpu = 400000000L
board_build.flash_mode = qio
board_upload.flash_size = 8MB
build_flags =
    -D CHIP_ESP32_P4=1
    -D CONFIG_IDF_TARGET_ESP32P4=1
    -D FEATURE_WIFI=0
    -D FEATURE_ETHERNET=1
    -D FEATURE_WEB_SERVER=1
    ; ... (see platformio.ini for full configuration)
```

### Build Commands

```bash
# ESP32-S3 build (existing)
pio run -e esp32dev_wifi

# ESP32-P4 build (new)
pio run -e esp32p4_idf

# List all environments
grep -E "^\[env:" platformio.ini
```

---

## Effort Estimates

| Component | Hours | Risk |
|-----------|-------|------|
| HAL Interface Definition | 8-12 | LOW |
| I2S Audio Driver Rewrite | 20-30 | **HIGH** |
| WiFi→Ethernet Migration | 40-60 | **HIGH** |
| FastLED/RMT Configuration | 10-15 | MEDIUM |
| FreeRTOS Task Tuning | 8-12 | MEDIUM |
| Build System | 4-6 | LOW |
| Testing & Validation | 20-30 | MEDIUM |
| **TOTAL** | **88-135** | **MEDIUM-HIGH** |

---

## Risk Matrix

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| FastLED P4 incompatibility | MEDIUM | CRITICAL | Fallback to ESP-IDF `led_strip` |
| ESPAsyncWebServer issues | MEDIUM | HIGH | Use ESP-IDF native HTTP server |
| I2S driver timing problems | MEDIUM | CRITICAL | Use new std mode driver API |
| Pioarduino fork instability | MEDIUM | MEDIUM | Pin to specific version |
| Performance regression | LOW | HIGH | Profile and optimize |

---

## Acceptance Criteria

### Performance Targets
- [ ] Frame rate ≥120 FPS sustained
- [ ] Audio latency ≤25ms
- [ ] Memory leak rate <1KB/hour
- [ ] Zero crashes in 72h stress test

### Feature Parity
- [ ] All 76 effects render identically to S3
- [ ] Web dashboard fully functional
- [ ] All REST API endpoints operational
- [ ] All WebSocket commands functional
- [ ] Zone system operational (4 zones)
- [ ] Transition engine functional (12 types)

### Hardware Verification
- [ ] 320 LEDs illuminate correctly (GPIO 4/5)
- [ ] Ethernet connectivity (`http://lightwaveos.local`)
- [ ] Audio-reactive effects respond to music
- [ ] Serial monitor shows 120+ FPS

---

## Rollback Plan

If migration fails at any phase:

**Abort Triggers:**
- Unable to achieve 100 FPS in Phase 2
- ESPAsyncWebServer incompatible with no viable alternative in Phase 3
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
| Hardware Compatibility | P4: 400MHz RISC-V, no WiFi, has Ethernet MAC |
| C++ Code Analysis | 70% portable, only I2S needs rewrite |
| Architecture Strategy | Unified codebase with compile-time HAL recommended |
| Network Layer | WiFi blocking; use built-in Ethernet |
| Deep Technical Audit | 102K LOC, well-architected for migration |
| Build System | PlatformIO extensible, pioarduino required |
| Audio Subsystem | I2S legacy→std mode driver migration critical |
| Effects System | All 76 effects are pure computation, fully portable |
| Toolchain Investigation | ESP-IDF 5.3+ required, Arduino limited |
| Migration Strategy | 6-phase, 12-week roadmap with gates |

---

## Quick Reference

### Chip Detection in Code

```cpp
#include "config/chip_config.h"

#if CHIP_ESP32_P4
    // P4-specific code
    #include "hal/esp32p4/EthernetDriver_P4.h"
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
    constexpr bool HAS_ETHERNET = true;
    constexpr uint8_t RMT_CHANNELS = 4;

    namespace task {
        constexpr float STACK_MULTIPLIER = 1.2f;  // RISC-V needs more stack
    }
}
```

---

## Next Steps

1. **Hardware**: Acquire ESP32-P4 Function EV Board (×2)
2. **Environment**: Install pioarduino fork and test build
3. **Validate**: Flash minimal firmware, verify serial output
4. **Proceed**: Begin Phase 2 (LED output) upon successful boot

---

## References

- [ESP32-P4 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32-p4_technical_reference_manual_en.pdf)
- [ESP32-P4 Function EV Board User Guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/user_guide.html)
- [pioarduino Platform Fork](https://github.com/pioarduino/platform-espressif32)
- [FastLED ESP32-P4 Support](https://github.com/FastLED/FastLED/releases/tag/3.10.0)
- [ESP-IDF I2S Standard Mode Driver](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/api-reference/peripherals/i2s.html)

---

*Document generated: 2026-01-24*
*Last updated: Phase 1 Complete*
