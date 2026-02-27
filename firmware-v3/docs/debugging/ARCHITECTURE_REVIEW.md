# Debugging/Logging Architecture Review

**Date:** 2026-01-22
**Reviewer:** Claude (Manual Review)
**Scope:** firmware/v2 debugging, logging, and serial print systems

---

## Resolution Status (2026-01-22)

The following improvements have been implemented as part of Phase 5 of the serial debug system redesign:

| Issue | Status | Resolution |
|-------|--------|------------|
| Logging fragmentation | **RESOLVED** | Unified `dbg` command system with per-domain control |
| Missing debug endpoints | **RESOLVED** | Added `/api/v1/debug/config` and `/api/v1/debug/status` REST endpoints |
| Serial menu coupling | **PARTIALLY RESOLVED** | Debug commands now use unified `dbg` interface; full refactor deferred |
| No structured log export | **DEFERRED** | WebSocket log streaming planned for future release |

**New Documentation:** See [DEBUG_SYSTEM.md](DEBUG_SYSTEM.md) for comprehensive user guide.

---

## Executive Summary

The firmware/v2 has a **well-designed 12-component debugging architecture** with clear separation of concerns. However, there is **logging fragmentation** across three systems (LW_LOG*, Serial.print*, ESP_LOG*) that could be consolidated for consistency.

### Strengths
- Unified logging macros (LW_LOG*) with timestamps, colors, and level control
- Comprehensive system monitors (Stack, Heap, MemoryLeak, ValidationProfiler)
- Well-structured 5-tier audio verbosity system
- REST API and WebSocket debug endpoints with proper validation
- Feature flags for compile-time debug control

### Areas for Improvement
- **Logging fragmentation**: 3 different logging systems in use
- **Missing debug endpoints**: Performance stats, system monitors not exposed via REST/WS
- **Serial menu coupling**: Debug commands embedded in 200+ line switch statement
- **No structured log export**: No way to stream logs to external tools

---

## Architecture Components

### 1. Unified Logging System (`src/utils/Log.h`)

**Rating: Excellent**

| Aspect | Assessment |
|--------|------------|
| Design | Clean macro-based system with level filtering |
| Colors | Well-thought-out semantic color palette |
| Performance | Zero overhead when level disabled (compiles out) |
| Adoption | 362 usages across 19 files |

**Key Features:**
- 4 log levels: ERROR, WARN, INFO, DEBUG
- Automatic timestamps via `millis()`
- Component tags via `LW_LOG_TAG`
- Throttle helper: `LW_LOG_THROTTLE()`
- Context helper: `LW_LOGE_CTX()` (adds heap + function name)
- Native build support (mock millis for unit tests)

**Code Quality:**
```cpp
// Clean format with color-coded level indicators
[12345][INFO][WebServer] Client connected from 192.168.1.100
[12346][ERROR][WiFiManager] Connection failed (heap=45678, fn=connect)
```

---

### 2. Performance Monitor (`src/hardware/PerformanceMonitor.h`)

**Rating: Excellent**

| Aspect | Assessment |
|--------|------------|
| Timing | Per-section µs precision with EMA smoothing |
| Memory | Heap, fragmentation, min free tracking |
| Alerts | Fragmentation thresholds (30% warn, 50% critical) |
| Output | Serial-friendly ASCII graphs and reports |

**Key Features:**
- Section timing: effect, FastLED.show(), serial, network
- CPU usage percentage calculation
- Dropped frame detection (>1.5x target)
- 60-sample history buffer for trend analysis
- REST-friendly `PerformanceStats` struct

**Gap:** Not exposed via REST API or WebSocket.

---

### 3. System Monitors (`src/core/system/`)

**Rating: Good**

| Monitor | Purpose | API Coverage |
|---------|---------|--------------|
| StackMonitor | FreeRTOS stack overflow detection | Serial only |
| HeapMonitor | Heap corruption detection | Serial only |
| MemoryLeakDetector | Allocation tracking | Serial only |
| ValidationProfiler | Validation overhead measurement | Serial only |

**Strengths:**
- Comprehensive coverage of ESP32 failure modes
- Profiling features with detailed reports
- Proper C linkage for FreeRTOS hooks

**Gap:** None of these are exposed via REST API or WebSocket.

---

### 4. Audio Debug Config (`src/audio/AudioDebugConfig.h`)

**Rating: Excellent**

| Aspect | Assessment |
|--------|------------|
| Flexibility | 6-tier verbosity (0-5) with runtime control |
| CLI | `adbg` command with level and interval control |
| REST | `/api/v1/debug/audio` GET/POST |
| WebSocket | `debug.audio.get`, `debug.audio.set` |

**Verbosity Tiers:**
| Level | Name | Output | Interval |
|-------|------|--------|----------|
| 0 | Off | None | - |
| 1 | Minimal | Errors | - |
| 2 | Status | 10s health | ~10s |
| 3 | Low | + DMA | ~5s |
| 4 | Medium | + 64-bin Goertzel | ~2s |
| 5 | High | + 8-band Goertzel | ~1s |

**This is a model pattern** for how debug features should be exposed.

---

### 5. Serial Menu (`src/main.cpp:260-450+`)

**Rating: Fair**

| Aspect | Assessment |
|--------|------------|
| Coverage | 40+ commands covering all subsystems |
| Design | Monolithic switch statement |
| Discoverability | Help text printed at boot |

**Strengths:**
- Comprehensive command coverage
- Immediate single-char hotkeys for common actions
- Line-buffered input for multi-char commands

**Concerns:**
- 200+ line switch statement is hard to maintain
- Debug commands mixed with effect/zone/show commands
- No command grouping or submenus

---

### 6. Network Debug Endpoints

**Rating: Good**

| Endpoint | Method | Function |
|----------|--------|----------|
| `/api/v1/debug/audio` | GET | Get audio verbosity |
| `/api/v1/debug/audio` | POST | Set audio verbosity |
| `debug.audio.get` | WS | Get audio verbosity |
| `debug.audio.set` | WS | Set audio verbosity |

**Strengths:**
- Proper JSON validation with error codes
- Consistent response format
- Feature flag gating (`FEATURE_AUDIO_SYNC`)

**Gaps:**
- No `/api/v1/debug/performance` endpoint
- No `/api/v1/debug/memory` endpoint
- No `/api/v1/debug/stack` endpoint
- No log streaming endpoint

---

## Logging Fragmentation Analysis

### Current State: Three Logging Systems

| System | Occurrences | Files | Purpose |
|--------|-------------|-------|---------|
| `LW_LOG*` | 362 | 19 | Unified app logging |
| `Serial.print*` | 433 | 27 | Menu output, legacy |
| `ESP_LOG*` | 85 | 7 | ESP-IDF native (actors) |

### Impact

1. **Inconsistent format**: Some messages have timestamps, some don't
2. **No unified filtering**: Can't filter by level across all outputs
3. **Mixed colour schemes**: LW_LOG uses different colours than raw Serial
4. **Testing complexity**: Three systems to mock in unit tests

### Files with Mixed Usage (High Priority)

| File | LW_LOG | Serial | ESP_LOG | Recommendation |
|------|--------|--------|---------|----------------|
| `main.cpp` | 49 | 234 | 0 | Migrate menu help to LW_LOGI |
| `ActorSystem.cpp` | 0 | 26 | 32 | Standardise on LW_LOG* |
| `ZoneComposer.cpp` | 0 | 29 | 0 | Migrate to LW_LOG* |
| `NarrativeEngine.cpp` | 0 | 17 | 0 | Migrate to LW_LOG* |
| `NVSManager.cpp` | 0 | 12 | 0 | Migrate to LW_LOG* |

---

## Improvement Recommendations

### Priority 1: Expose System Monitors via REST API

**Effort:** Medium
**Impact:** High (enables remote debugging, dashboards)

Add endpoints:
- `GET /api/v1/debug/performance` - PerformanceMonitor stats
- `GET /api/v1/debug/memory` - HeapMonitor + MemoryLeakDetector
- `GET /api/v1/debug/stack` - StackMonitor profiles

### Priority 2: Consolidate Logging Systems

**Effort:** High
**Impact:** Medium (consistency, testability)

Migration plan:
1. Add `LW_LOG_MENU` macro for menu help text (no timestamp, no level)
2. Migrate Serial.printf in main.cpp menu to `LW_LOG_MENU`
3. Add `LW_LOG_RAW` for data output (graphs, tables)
4. Replace ESP_LOG* in actors with LW_LOG*

### Priority 3: Refactor Serial Menu Architecture

**Effort:** High
**Impact:** Medium (maintainability)

Proposed design:
```cpp
// Command registry pattern
struct SerialCommand {
    const char* name;
    const char* help;
    void (*handler)(const String& args);
    uint8_t flags; // IMMEDIATE, BUFFERED, DEBUG, etc.
};

// Register commands in setup()
registerCommand("adbg", "Audio debug verbosity", handleAudioDebug, CMD_BUFFERED | CMD_DEBUG);
```

### Priority 4: Add Log Streaming Endpoint

**Effort:** Medium
**Impact:** Medium (enables external log aggregation)

WebSocket endpoint for real-time log streaming:
- `debug.logs.subscribe` - Start streaming logs
- `debug.logs.unsubscribe` - Stop streaming
- Filter by level, tag, or pattern

---

## Swarm Implementation Tasks

For Claude-Flow to implement these improvements:

### Task 1: Debug REST Endpoints (api agent)
```bash
npx claude-flow@v3alpha agent_spawn \
  --agent api \
  --task "Add REST endpoints for PerformanceMonitor, HeapMonitor, StackMonitor:
    - GET /api/v1/debug/performance (fps, cpu, timing, heap)
    - GET /api/v1/debug/memory (heap free/min, fragmentation, leak scan)
    - GET /api/v1/debug/stack (per-task profiles)
    Files: src/network/webserver/handlers/DebugHandlers.{h,cpp}"
```

### Task 2: Debug WebSocket Commands (api agent)
```bash
npx claude-flow@v3alpha agent_spawn \
  --agent api \
  --task "Add WebSocket commands for system monitors:
    - debug.performance.get
    - debug.memory.get
    - debug.stack.get
    Files: src/network/webserver/ws/WsDebugCommands.{h,cpp}"
```

### Task 3: Logging Consolidation (embedded agent)
```bash
npx claude-flow@v3alpha agent_spawn \
  --agent embedded \
  --task "Add LW_LOG_MENU and LW_LOG_RAW macros to Log.h:
    - LW_LOG_MENU: No timestamp, no level prefix (for help text)
    - LW_LOG_RAW: No formatting at all (for data output)
    Then migrate top 5 files with Serial.printf to LW_LOG*"
```

### Task 4: Serial Menu Refactor (embedded agent)
```bash
npx claude-flow@v3alpha agent_spawn \
  --agent embedded \
  --task "Refactor serial menu in main.cpp:
    - Create SerialCommand registry in src/cli/CommandRegistry.{h,cpp}
    - Move command handlers to separate files by category
    - Keep immediate hotkeys, refactor buffered commands to registry"
```

### Task 5: Documentation Update (documentation agent)
```bash
npx claude-flow@v3alpha agent_spawn \
  --agent documentation \
  --task "Update debugging documentation:
    - Add docs/debugging/LOGGING_GUIDE.md
    - Update CLAUDE.md with new debug endpoints
    - Add examples for each debug feature"
```

---

## Verification Checklist

After implementation:

- [ ] `pio run -e esp32dev_audio` compiles cleanly
- [ ] `curl http://lightwaveos.local/api/v1/debug/performance` returns JSON
- [ ] `curl http://lightwaveos.local/api/v1/debug/memory` returns JSON
- [ ] WebSocket `debug.performance.get` returns data
- [ ] Serial `s` command shows performance stats
- [ ] LW_LOG* usage increased, Serial.print* usage decreased
- [ ] All debug features documented

---

## Appendix: File Inventory

| File | Lines | Purpose |
|------|-------|---------|
| `src/utils/Log.h` | 211 | Unified logging macros |
| `src/hardware/PerformanceMonitor.h` | 336 | Performance metrics |
| `src/hardware/PerformanceMonitor.cpp` | ~200 | Implementation |
| `src/core/system/StackMonitor.h` | 142 | Stack monitoring |
| `src/core/system/HeapMonitor.h` | 96 | Heap monitoring |
| `src/core/system/MemoryLeakDetector.h` | 106 | Leak detection |
| `src/core/system/ValidationProfiler.h` | 104 | Validation profiling |
| `src/audio/AudioDebugConfig.h` | 46 | Audio verbosity config |
| `src/network/webserver/handlers/DebugHandlers.h` | 58 | REST debug handlers |
| `src/network/webserver/handlers/DebugHandlers.cpp` | 109 | Implementation |
| `src/network/webserver/ws/WsDebugCommands.h` | 31 | WS debug commands |
