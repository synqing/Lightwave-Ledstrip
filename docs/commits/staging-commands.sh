#!/bin/bash
# Git Staging Commands for Comprehensive Commit Strategy
# Execute each section sequentially to create atomic commits

set -e  # Exit on error

echo "=== Commit 1: Unified Logging System ==="
git add v2/src/utils/Log.h
git add v2/platformio.ini
git add v2/src/config/features.h
git commit -m "feat(logging): Add unified logging system with colored output

Provides consistent, colored logging with automatic timestamps and component tags.
Preserves existing ANSI color coding conventions from the codebase.

- Add utils/Log.h with LW_LOGI, LW_LOGE, LW_LOGW, LW_LOGD macros
- Add LW_LOG_LEVEL build flag (0=None, 1=Error, 2=Warn, 3=Info, 4=Debug)
- Add FEATURE_UNIFIED_LOGGING feature flag (default: enabled)
- Support both Arduino/ESP-IDF and native builds
- Zero overhead when logs disabled (compile-time filtering)
- Preserve existing color constants (LW_CLR_GREEN, LW_CLR_YELLOW, etc.)

Technical:
- Header-only implementation (no runtime memory allocation)
- Format strings in flash/code section
- Platform detection for Arduino vs native builds
- Title-only coloring pattern for high-frequency logs

Dependencies: None (foundational infrastructure)

Testing:
- Build: pio run -e esp32dev (should compile with LW_LOG_LEVEL=3)
- Runtime: Verify log format [timestamp][LEVEL][TAG] message
- Colors: Verify ANSI colors appear correctly in terminal

Refs: docs/commits/commit-1-unified-logging.md"

echo ""
echo "=== Commit 2: AudioActor Logging Migration ==="
git add v2/src/audio/AudioActor.cpp
git add v2/src/audio/AudioActor.h
git commit -m "refactor(logging): Migrate AudioActor to unified logging system

Replace ESP-IDF ESP_LOG* macros with LW_LOG* equivalents.
Preserve existing colored output conventions (yellow for hardware, cyan for spectral).

- Replace ESP_LOGI/LOGE/LOGW/LOGD with LW_LOG* macros
- Remove esp_log.h include and TAG constant
- Preserve inline ANSI color codes using LW_CLR_* constants
- Maintain log frequency and verbosity (behavior-preserving)

Technical:
- No performance impact (same log frequency)
- Native builds no longer require ESP-IDF logging stubs
- Compile-time log level filtering now available

Dependencies: Commit 1 (unified logging system)

Testing:
- Build: pio run -e esp32dev (should compile without ESP-IDF logging)
- Runtime: Verify audio initialization logs appear with correct format
- Colors: Verify yellow for DMA diagnostics, cyan for spectral analysis

Refs: docs/commits/commit-2-3-audio-logging.md"

echo ""
echo "=== Commit 3: AudioCapture Logging Migration ==="
git add v2/src/audio/AudioCapture.cpp
git commit -m "refactor(logging): Migrate AudioCapture to unified logging system

Replace ESP-IDF ESP_LOG* macros with LW_LOG* equivalents.
Preserve existing colored output conventions (yellow for DMA diagnostics).

- Replace ESP_LOGI/LOGE/LOGW/LOGD with LW_LOG* macros
- Remove esp_log.h include and TAG constant
- Preserve inline ANSI color codes using LW_CLR_* constants
- Maintain log frequency and verbosity (behavior-preserving)

Technical:
- No performance impact (same log frequency)
- Native builds no longer require ESP-IDF logging stubs
- Compile-time log level filtering now available

Dependencies: Commit 1 (unified logging system)

Testing:
- Build: pio run -e esp32dev (should compile without ESP-IDF logging)
- Runtime: Verify I2S initialization logs appear with correct format
- Colors: Verify yellow for DMA diagnostics

Refs: docs/commits/commit-2-3-audio-logging.md"

echo ""
echo "=== Commit 4: Audio Debug Verbosity ==="
git add v2/src/audio/AudioDebugConfig.h
git add v2/src/audio/AudioDebugConfig.cpp
# Note: AudioActor.cpp and AudioCapture.cpp changes for verbosity gating
# Note: main.cpp changes for adbg CLI commands
git add v2/src/audio/AudioActor.cpp
git add v2/src/audio/AudioCapture.cpp
git add v2/src/main.cpp
git commit -m "feat(audio): Add runtime-configurable audio debug verbosity

Provides tiered debug levels (0-5) with CLI control via 'adbg' command.
Allows adjusting verbosity without recompiling.

Levels:
- 0 = Off (no debug output)
- 1 = Errors only
- 2 = Status (10-second health checks)
- 3 = Low (+ DMA diagnostics ~5s interval)
- 4 = Medium (+ 64-bin Goertzel ~2s interval)
- 5 = High (+ 8-band Goertzel ~1s interval)

Implementation:
- AudioDebugConfig singleton with verbosity and baseInterval
- Verbosity gating in AudioActor and AudioCapture
- CLI commands: adbg, adbg <0-5>, adbg interval <N>
- Derived intervals: DMA=5x, 64-bin=0.5x, 8-band=1x base interval

Technical:
- Minimal overhead: Single struct access per log check
- No string formatting when logs disabled
- Complements compile-time LW_LOG_LEVEL (runtime override)

Dependencies: Commits 1-3 (unified logging, audio migration)

Testing:
- CLI: adbg (show settings), adbg 2 (status only), adbg 5 (full verbosity)
- Verify log frequency changes with verbosity level
- Verify intervals scale correctly

Refs: docs/commits/commit-4-audio-debug-verbosity.md"

echo ""
echo "=== Commit 5: K1 Debug Infrastructure ==="
git add v2/src/audio/k1/K1DebugMetrics.h
git add v2/src/audio/k1/K1DebugRing.h
git add v2/src/audio/k1/K1DebugMacros.h
git add v2/src/config/features.h
git add v2/src/audio/k1/K1BeatClock.h
git add v2/src/audio/k1/K1Pipeline.h
git add v2/src/audio/k1/K1Pipeline.cpp
git add v2/src/audio/AudioActor.h
git add v2/src/audio/AudioActor.cpp
git commit -m "feat(k1): Add K1 beat tracker debug infrastructure

Provides cross-core debug visibility for K1-Lightwave beat tracker pipeline.
Enables real-time state capture without blocking audio processing.

Components:
- K1DebugMetrics: 64-byte cache-aligned debug sample (fixed-point encoding)
- K1DebugRing: Lock-free SPSC ring buffer (32 samples × 64 bytes = 2KB)
- K1DebugMacros: Zero-overhead instrumentation macros (expand to nothing when disabled)
- Integration: K1Pipeline instrumented, AudioActor holds ring buffer

Data Encoding:
- BPM: uint16_t bpm_x10 (60-180 BPM → 600-1800)
- Magnitudes: uint16_t magnitude_x1k (0.0-1.0 → 0-1000)
- Phases: int16_t phase_x100 (radians × 100)
- Top-3 candidates, tactus state, beat clock (PLL) state, flags

Technical:
- Lock-free SPSC queue (no mutexes, no blocking)
- Cache-aligned samples (64-byte alignment prevents false sharing)
- Producer (Core 0) never blocks (drops samples if ring full)
- Zero bytes when FEATURE_K1_DEBUG disabled (compile-time elimination)

Dependencies: Commit 1 (unified logging), utils/LockFreeQueue.h

Testing:
- Build: pio run -e esp32dev_audio (FEATURE_K1_DEBUG enabled)
- Runtime: Verify ring buffer doesn't overflow during audio processing
- Verify debug macros expand to nothing when disabled

Refs: docs/commits/commit-5-6-k1-debug.md"

echo ""
echo "=== Commit 6: K1 Debug CLI ==="
git add v2/src/audio/k1/K1DebugCli.h
git add v2/src/audio/k1/K1DebugCli.cpp
git add v2/src/main.cpp
git commit -m "feat(k1): Add K1 beat tracker CLI debug commands

Provides Serial CLI interface for debugging K1-Lightwave beat tracker.
Output formats match Tab5.DSP for consistency.

Commands:
- k1: Full diagnostic (BPM, confidence, phase, lock, top-3 candidates)
- k1s: Stats summary (all 4 stages, internal metrics)
- k1spec: ASCII resonator spectrum (121 bins, every 5 BPM)
- k1nov: Recent novelty z-scores with visual bars
- k1reset: Reset pipeline state
- k1c: Compact output (for continuous monitoring)

Implementation:
- K1DebugCli.h/cpp: Print functions and command handler
- main.cpp: Command handler integration (gated by FEATURE_K1_DEBUG)
- Accesses K1Pipeline via AudioActor

Technical:
- CLI output only when command issued (no continuous overhead)
- Tab5.DSP-compatible format for consistency
- ~1KB code size (only included when FEATURE_K1_DEBUG enabled)

Dependencies: Commit 5 (K1 debug infrastructure), Commit 1 (unified logging)

Testing:
- Play music and wait for beat lock
- Test: k1 (show current state), k1s (full stats), k1spec (spectrum)
- Verify output matches Tab5.DSP format
- Verify k1reset unlocks and re-locks

Refs: docs/commits/commit-5-6-k1-debug.md"

echo ""
echo "=== Commit 7: RendererActor Logging Migration ==="
git add v2/src/core/actors/RendererActor.cpp
git commit -m "refactor(logging): Migrate RendererActor to unified logging system

Replace ESP-IDF ESP_LOG* macros and Serial.println with LW_LOG* equivalents.
Preserve existing colored output conventions (green for effect selection).

- Replace ESP_LOGI/LOGD with LW_LOG* macros
- Remove esp_log.h include and NATIVE_BUILD guards
- Preserve green color for effect names (LW_CLR_GREEN)
- Maintain log frequency and verbosity (behavior-preserving)

Technical:
- No performance impact (same log frequency)
- Native builds no longer require ESP-IDF logging stubs
- Compile-time log level filtering now available

Dependencies: Commit 1 (unified logging system)

Testing:
- Build: pio run -e esp32dev (should compile without ESP-IDF logging)
- Runtime: Verify renderer initialization logs appear
- Colors: Verify green for effect selection feedback

Refs: docs/commits/commit-7-10-logging-migration.md"

echo ""
echo "=== Commit 8: Hardware Components Logging Migration ==="
git add v2/src/hardware/EncoderManager.cpp
git add v2/src/hardware/PerformanceMonitor.cpp
git commit -m "refactor(logging): Migrate hardware components to unified logging system

Replace Serial.println/Serial.print with LW_LOG* equivalents in hardware components.
Preserve existing log frequency and verbosity.

Components:
- EncoderManager: I2C encoder initialization, connection status, metrics
- PerformanceMonitor: FPS, CPU usage, frame timing, memory metrics

Changes:
- Replace Serial.println/print with LW_LOG* macros
- Use LW_LOG_TAG for component identification (Encoder, PERF)
- Maintain log frequency and verbosity (behavior-preserving)

Technical:
- No performance impact (same log frequency)
- Compile-time log level filtering now available
- Consistent formatting across hardware components

Dependencies: Commit 1 (unified logging system)

Testing:
- Build: pio run -e esp32dev (should compile)
- Runtime: Verify encoder initialization logs appear
- Runtime: Verify performance monitor output (FPS, CPU, heap)

Refs: docs/commits/commit-7-10-logging-migration.md"

echo ""
echo "=== Commit 9: Network Components Logging Migration ==="
git add v2/src/network/WebServer.cpp
git add v2/src/network/WiFiManager.cpp
git commit -m "refactor(logging): Migrate network components to unified logging system

Replace Serial.printf/Serial.println with LW_LOG* equivalents in network components.
Preserve existing log frequency and verbosity.

Components:
- WebServer: Server initialization, mDNS, WebSocket events, OTA updates
- WiFiManager: WiFi state machine, connection status, AP mode

Changes:
- Replace Serial.printf/println with LW_LOG* macros
- Use LW_LOG_TAG for component identification (WebServer, WiFi)
- Maintain log frequency and verbosity (behavior-preserving)

Technical:
- No performance impact (same log frequency)
- Compile-time log level filtering now available
- Consistent formatting across network components

Dependencies: Commit 1 (unified logging system)

Testing:
- Build: pio run -e esp32dev_audio (should compile)
- Runtime: Verify WiFi connection logs appear
- Runtime: Verify WebServer initialization logs appear
- Runtime: Verify WebSocket connection/disconnection logs

Refs: docs/commits/commit-7-10-logging-migration.md"

echo ""
echo "=== Commit 10: Main Application Logging Migration ==="
git add v2/src/main.cpp
git commit -m "refactor(logging): Migrate main.cpp to unified logging system

Replace Serial.println/Serial.printf with LW_LOG* equivalents in main application.
Preserve existing colored output conventions (green for effect selection).

Changes:
- Replace Serial.println/printf with LW_LOG* macros
- Use LW_LOG_TAG \"Main\" for component identification
- Preserve green color for effect selection (LW_CLR_GREEN)
- Maintain log frequency and verbosity (behavior-preserving)

Note: CLI command additions (adbg, k1) were already committed in Commits 4 and 6.
This commit only includes logging migration changes.

Technical:
- No performance impact (same log frequency)
- Compile-time log level filtering now available
- Consistent formatting with other components

Dependencies: Commit 1 (unified logging system)

Testing:
- Build: pio run -e esp32dev (should compile)
- Runtime: Verify startup sequence logs appear
- Runtime: Verify effect selection uses green color
- Runtime: Verify all CLI commands still work

Refs: docs/commits/commit-7-10-logging-migration.md"

echo ""
echo "=== All commits completed successfully! ==="
echo "Review commits with: git log --oneline -10"

