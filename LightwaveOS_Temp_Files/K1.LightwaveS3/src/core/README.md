# Core Module

The `core/` module contains the foundational systems for LightwaveOS v2: the Actor system for cross-core communication, state management via CQRS, persistent storage, show orchestration, and system monitoring.

## Directory Structure

| Directory | Purpose |
|-----------|---------|
| `actors/` | Actor system for thread-safe cross-core messaging |
| `bus/` | Pub/sub MessageBus for Actor communication |
| `state/` | CQRS state management with double-buffering |
| `persistence/` | NVS-backed persistent storage managers |
| `shows/` | Choreographed light show definitions and scheduling |
| `narrative/` | Dramatic timing engine for visual effects |
| `system/` | Heap, stack, and memory monitoring utilities |

## Actor System (actors/, bus/)

The Actor Model provides thread-safe, lock-free communication between ESP32-S3 cores.

**Core Distribution:**
- **Core 0 (Network/Input):** NetworkActor, HmiActor, PluginManagerActor
- **Core 1 (Rendering):** RendererActor, StateStoreActor

### Key Files

| File | Purpose |
|------|---------|
| `actors/Actor.h` | Base Actor class with FreeRTOS task management |
| `actors/ActorSystem.h` | Actor lifecycle and registry |
| `actors/RendererActor.h` | 120 FPS LED rendering on Core 1 |
| `actors/ShowDirectorActor.h` | Show playback orchestration |
| `bus/MessageBus.h` | Pub/sub message routing |

### Message Structure

Messages are 16-byte fixed-size structures for efficient queue transfer:

```cpp
struct Message {
    MessageType type;   // 1 byte
    uint8_t param1-3;   // 3 bytes
    uint32_t param4;    // 4 bytes (duration, flags)
    uint32_t timestamp; // 4 bytes
    uint32_t reserved;  // 4 bytes
};
```

### Usage Example

```cpp
#include "core/actors/Actor.h"
#include "core/bus/MessageBus.h"

// Subscribe to events
MSG_BUS.subscribe(MessageType::EFFECT_CHANGED, this);

// Publish messages
MSG_BUS.publish(Message(MessageType::SET_EFFECT, effectId));

// Send directly to actor
renderer->send(Message(MessageType::SET_BRIGHTNESS, 200));
```

## State Management (state/)

Implements CQRS (Command-Query Responsibility Segregation) with double-buffered immutable state.

**See [state/README.md](state/README.md) for complete documentation.**

### Quick Reference

```cpp
#include "core/state/StateStore.h"
#include "core/state/Commands.h"

// Lock-free read (safe at 120 FPS)
const SystemState& state = stateStore.getState();
uint8_t brightness = state.brightness;

// State mutation via command
SetEffectCommand cmd(5);
stateStore.dispatch(cmd);
```

### Performance

| Operation | Latency | Blocks? |
|-----------|---------|---------|
| `getState()` | ~10ns | Never |
| `dispatch()` | < 1ms | Only other writes |

## Persistence (persistence/)

NVS-backed managers for storing configuration across reboots.

| File | Purpose |
|------|---------|
| `NVSManager.h` | Low-level NVS wrapper with namespacing |
| `AudioTuningManager.h` | Audio sensitivity calibration |
| `ZoneConfigManager.h` | Zone layout and effect assignments |

### Usage Example

```cpp
#include "core/persistence/NVSManager.h"

NVSManager nvs("config");
nvs.setUInt8("brightness", 128);
uint8_t val = nvs.getUInt8("brightness", 128); // Default: 128
```

## Shows/Narratives (shows/, narrative/)

Orchestrates multi-minute choreographed light shows with timed cues and dramatic timing.

### Show System (shows/)

| File | Purpose |
|------|---------|
| `ShowTypes.h` | Cue, chapter, and show data structures |
| `CueScheduler.h` | Timed cue execution |
| `ParameterSweeper.h` | Smooth parameter interpolation |
| `BuiltinShows.h` | Pre-defined show definitions |

**Cue Types:**
- `CUE_EFFECT` - Change effect
- `CUE_PARAMETER_SWEEP` - Interpolate parameter over time
- `CUE_ZONE_CONFIG` - Configure zones
- `CUE_TRANSITION` - Trigger transition type
- `CUE_NARRATIVE` - Modulate narrative timing
- `CUE_PALETTE` - Change color palette

### Narrative Engine (narrative/)

Global temporal conductor for visual drama using BUILD -> HOLD -> RELEASE -> REST phases.

```cpp
#include "core/narrative/NarrativeEngine.h"

// Query global intensity (0-1)
float intensity = NARRATIVE.getIntensity();

// Query with zone offset for spatial choreography
float zoneIntensity = NARRATIVE.getIntensity(zoneId);

// Check phase
if (NARRATIVE.justEntered(PHASE_BUILD)) {
    // Trigger dramatic effect
}
```

## System Monitoring (system/)

Runtime monitoring for heap, stack, and memory profiling.

| File | Purpose |
|------|---------|
| `HeapMonitor.h` | Heap integrity and fragmentation |
| `StackMonitor.h` | Task stack high-water marks |
| `MemoryLeakDetector.h` | Allocation tracking |
| `ValidationProfiler.h` | Performance profiling |

### Usage Example

```cpp
#include "core/system/HeapMonitor.h"

HeapMonitor::init();

// Check heap health
if (!HeapMonitor::checkHeapIntegrity()) {
    // Handle corruption
}

// Monitor memory
size_t freeHeap = HeapMonitor::getFreeHeap();
uint8_t fragmentation = HeapMonitor::getFragmentationPercent();
```

## Architecture Overview

```
                    ┌─────────────────┐
                    │   MessageBus    │
                    │   (pub/sub)     │
                    └────────┬────────┘
                             │
        ┌────────────────────┼────────────────────┐
        │                    │                    │
   ┌────▼─────┐        ┌─────▼─────┐        ┌────▼─────┐
   │ Network  │        │ Renderer  │        │   Hmi    │
   │  Actor   │        │  Actor    │        │  Actor   │
   │ (Core 0) │        │ (Core 1)  │        │ (Core 0) │
   └────┬─────┘        └─────┬─────┘        └────┬─────┘
        │                    │                    │
        │              ┌─────▼─────┐              │
        │              │  State    │              │
        └──────────────►  Store   ◄──────────────┘
                       │ (CQRS)   │
                       └─────┬─────┘
                             │
                       ┌─────▼─────┐
                       │    NVS    │
                       │ Persist   │
                       └───────────┘
```

## Related Documentation

- [state/README.md](state/README.md) - Complete CQRS state management guide
- [state/QUICK_REFERENCE.md](state/QUICK_REFERENCE.md) - Command cheat sheet
- [../../docs/api/API_V1.md](../../docs/api/API_V1.md) - REST/WebSocket API
