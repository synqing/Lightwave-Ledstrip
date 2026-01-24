# Serial Debug Redesign Proposal

**Date:** 2026-01-22
**Status:** PROPOSAL — Ready for Review

---

## Goals

1. **Sane defaults** — No runtime spam with default build
2. **Clear hierarchy** — Each level has distinct, logical purpose
3. **One verbosity system** — Consolidate LW_LOG, ESP_LOG, adbg
4. **On-demand diagnostics** — Commands to print status without changing level
5. **Domain separation** — Audio, render, network each controllable independently

---

## Proposed Architecture

### Unified Debug Config

Replace the fragmented system with a single `DebugConfig` struct:

```cpp
// src/config/DebugConfig.h
namespace lightwaveos {
namespace config {

struct DebugConfig {
    // Global verbosity (affects all domains unless overridden)
    uint8_t globalLevel = 2;  // Default: WARN

    // Domain-specific overrides (-1 = use global)
    int8_t audioLevel = -1;
    int8_t renderLevel = -1;
    int8_t networkLevel = -1;
    int8_t actorLevel = -1;

    // Periodic output intervals (0 = disabled)
    uint16_t statusIntervalSec = 0;   // 0 = off by default
    uint16_t spectrumIntervalSec = 0; // 0 = off by default

    // Effective level for a domain
    uint8_t effectiveLevel(Domain domain) const;
};

enum class Domain : uint8_t {
    AUDIO,
    RENDER,
    NETWORK,
    ACTOR,
    SYSTEM
};

// Levels with CLEAR semantics
enum class Level : uint8_t {
    OFF = 0,      // Nothing
    ERROR = 1,    // Actual errors (failures, corruption)
    WARN = 2,     // Actionable warnings (low stack, high latency)
    INFO = 3,     // Significant events (effect change, connection)
    DEBUG = 4,    // Diagnostic detail (timing, values)
    TRACE = 5     // Everything (raw samples, DMA, per-frame)
};

DebugConfig& getDebugConfig();

} // namespace config
} // namespace lightwaveos
```

### New Serial Commands

```
dbg                    - Show all debug config
dbg <0-5>              - Set global level
dbg audio <0-5>        - Set audio domain level
dbg render <0-5>       - Set render domain level
dbg network <0-5>      - Set network domain level
dbg actor <0-5>        - Set actor domain level

dbg status             - Print health summary NOW (one-shot)
dbg spectrum           - Print spectrum NOW (one-shot)
dbg beat               - Print beat tracking NOW (one-shot)
dbg memory             - Print heap/stack NOW (one-shot)

dbg interval status <N>    - Auto-print status every N seconds (0=off)
dbg interval spectrum <N>  - Auto-print spectrum every N seconds (0=off)
```

### Backwards Compatibility

Keep `adbg` as alias:
```
adbg         -> dbg audio
adbg <0-5>   -> dbg audio <0-5>
adbg status  -> dbg status
```

---

## Proposed Level Definitions

### Level 0: OFF
- **Prints:** Nothing from that domain
- **Use case:** Production, minimal logging

### Level 1: ERROR
- **Prints:** Actual failures
- **Audio:** Capture failure, AGC saturation, DMA timeout
- **Render:** Frame drop, effect init failure
- **Network:** Connection failure, rate limit exceeded
- **Actor:** Queue full, task creation failure

### Level 2: WARN (DEFAULT)
- **Prints:** Errors + actionable warnings
- **Audio:** Spike correction, noise floor change
- **Render:** Slow frame (>12ms), transition abort
- **Network:** Client disconnect, retry
- **Actor:** Stack < 20%, queue > 80%

### Level 3: INFO
- **Prints:** Warn + significant events
- **Audio:** Calibration complete, AGC adjustment
- **Render:** Effect change, brightness change
- **Network:** Client connect, API request
- **Actor:** Actor start/stop

### Level 4: DEBUG
- **Prints:** Info + diagnostic values
- **Audio:** Periodic spectrum (every 10s), beat confidence
- **Render:** Frame timing breakdown
- **Network:** Message routing, WebSocket frames
- **Actor:** Message delivery timing

### Level 5: TRACE
- **Prints:** Everything
- **Audio:** Per-hop values, raw DMA, all Goertzel bins
- **Render:** Per-frame LED values
- **Network:** Raw packet bytes
- **Actor:** Every message, every state change

---

## Migration Plan

### Phase 1: Add New Infrastructure (Non-Breaking)
1. Create `src/config/DebugConfig.h` with new struct
2. Create `src/config/DebugConfig.cpp` with singleton
3. Add `dbg` command handler in main.cpp (parallel to `adbg`)
4. Add one-shot methods: `printStatus()`, `printSpectrum()`, etc.

### Phase 2: Migrate Audio Domain
1. Replace `AudioDebugConfig` with `DebugConfig` domain
2. Update AudioActor.cpp verbosity checks
3. Update AudioCapture.cpp verbosity checks
4. Keep `adbg` as alias

### Phase 3: Migrate Other Domains
1. Wrap ESP_LOG* calls with domain-aware gating
2. Add render domain gating to RendererActor
3. Add network domain gating to WebServer
4. Add actor domain gating to ActorSystem

### Phase 4: Change Default to WARN
1. Set `globalLevel = 2` (WARN)
2. Remove periodic prints from Level 2
3. Move periodic prints to on-demand commands

### Phase 5: Cleanup
1. Remove `AudioDebugConfig.h/cpp`
2. Update documentation
3. Update REST API endpoints

---

## Implementation Details

### Macro Design

```cpp
// src/utils/Log.h additions

// Domain-aware logging
#define LW_LOG_DOMAIN(domain, level, fmt, ...) \
    do { \
        if (config::getDebugConfig().effectiveLevel(domain) >= level) { \
            LW_LOG_PRINTF(LW_LOG_FORMAT(#level, LW_CLR_##level, fmt), \
                          (unsigned long)LW_LOG_MILLIS(), ##__VA_ARGS__); \
        } \
    } while(0)

// Convenience macros per domain
#define LW_AUDIO_LOGE(fmt, ...) LW_LOG_DOMAIN(Domain::AUDIO, Level::ERROR, fmt, ##__VA_ARGS__)
#define LW_AUDIO_LOGW(fmt, ...) LW_LOG_DOMAIN(Domain::AUDIO, Level::WARN, fmt, ##__VA_ARGS__)
#define LW_AUDIO_LOGI(fmt, ...) LW_LOG_DOMAIN(Domain::AUDIO, Level::INFO, fmt, ##__VA_ARGS__)
#define LW_AUDIO_LOGD(fmt, ...) LW_LOG_DOMAIN(Domain::AUDIO, Level::DEBUG, fmt, ##__VA_ARGS__)
#define LW_AUDIO_LOGT(fmt, ...) LW_LOG_DOMAIN(Domain::AUDIO, Level::TRACE, fmt, ##__VA_ARGS__)

// Same for RENDER, NETWORK, ACTOR
```

### One-Shot Status Methods

```cpp
// src/audio/AudioActor.h additions
class AudioActor {
public:
    // One-shot debug output (ignores verbosity level)
    void printStatus();      // Health summary
    void printSpectrum();    // Current 8-band and 64-bin
    void printBeat();        // Beat tracking state
    void printCalibration(); // Noise floor, AGC state
};
```

### Example Refactored AudioActor

**Before (current mess):**
```cpp
// Every 10 seconds, regardless of what level means
if (dbgCfg.verbosity >= 2 && (m_stats.tickCount % 620) == 0) {
    LW_LOGI("Audio alive: ...");
    LW_LOGI("Spike stats: ...");
    LW_LOGI("Saliency: ...");
    LW_LOGI("Style: ...");
    LW_LOGI("Beat: ...");
}
```

**After (proposed):**
```cpp
// Only at DEBUG level, with controllable interval
auto& cfg = getDebugConfig();
if (cfg.statusIntervalSec > 0 &&
    cfg.effectiveLevel(Domain::AUDIO) >= Level::DEBUG) {
    if ((m_stats.tickCount % (cfg.statusIntervalSec * 62)) == 0) {
        printStatus();  // Single consolidated method
    }
}

// Warnings print immediately when they happen
if (spikesCorrected > SPIKE_THRESHOLD) {
    LW_AUDIO_LOGW("High spike rate: %u corrected this frame", spikesCorrected);
}
```

---

## REST API Changes

### New Endpoints

```
GET  /api/v1/debug/config     - Get full debug config
POST /api/v1/debug/config     - Update debug config
POST /api/v1/debug/status     - Trigger one-shot status (returns JSON)
POST /api/v1/debug/spectrum   - Trigger one-shot spectrum (returns JSON)
```

### Example Response

```json
// GET /api/v1/debug/config
{
    "globalLevel": 2,
    "domains": {
        "audio": -1,
        "render": -1,
        "network": -1,
        "actor": -1
    },
    "intervals": {
        "status": 0,
        "spectrum": 0
    },
    "levels": ["OFF", "ERROR", "WARN", "INFO", "DEBUG", "TRACE"]
}
```

---

## Summary of Changes

| What | Now | Proposed |
|------|-----|----------|
| Default level | 4 (floods serial) | 2 (quiet) |
| Level 1 | Unused | Errors only |
| Level 2 | 5 lines every 10s | Warnings only |
| Level 3 | + DMA | + Significant events |
| Level 4 | + 64-bin spectrum | + Diagnostics |
| Level 5 | + 8-band spectrum | + Everything |
| Domain control | None | Per-domain levels |
| On-demand | None | `dbg status`, `dbg spectrum` |
| Periodic | Always on at Level 2+ | Opt-in with `dbg interval` |

---

## Swarm Implementation Tasks

### Task 1: Create DebugConfig Infrastructure
```bash
npx claude-flow@v3alpha agent_spawn \
  --agent embedded \
  --task "Create unified DebugConfig system:
    - src/config/DebugConfig.h (struct, enum, singleton)
    - src/config/DebugConfig.cpp (implementation)
    - Domain-aware macros in Log.h
    Files: src/config/DebugConfig.{h,cpp}, src/utils/Log.h"
```

### Task 2: Add Serial Commands
```bash
npx claude-flow@v3alpha agent_spawn \
  --agent embedded \
  --task "Add dbg serial commands in main.cpp:
    - dbg (show config)
    - dbg <level> (set global)
    - dbg <domain> <level> (set domain)
    - dbg status/spectrum/beat/memory (one-shot)
    - dbg interval <type> <seconds>
    - Keep adbg as backwards-compatible alias"
```

### Task 3: Migrate AudioActor
```bash
npx claude-flow@v3alpha agent_spawn \
  --agent embedded \
  --task "Migrate AudioActor to new debug system:
    - Replace AudioDebugConfig usage with DebugConfig
    - Add printStatus(), printSpectrum(), printBeat() methods
    - Fix verbosity level semantics
    - Remove periodic prints from Level 2"
```

### Task 4: Add REST Endpoints
```bash
npx claude-flow@v3alpha agent_spawn \
  --agent api \
  --task "Add debug REST endpoints:
    - GET/POST /api/v1/debug/config
    - POST /api/v1/debug/status (returns JSON)
    - POST /api/v1/debug/spectrum (returns JSON)
    - Update DebugHandlers.{h,cpp}"
```

### Task 5: Documentation
```bash
npx claude-flow@v3alpha agent_spawn \
  --agent documentation \
  --task "Document new debug system:
    - Update CLAUDE.md with new commands
    - Create docs/debugging/DEBUG_SYSTEM.md
    - Update API documentation"
```
