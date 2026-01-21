# Phase 4: ShowDirectorActor Implementation

**Status:** ✅ **COMPLETE**

## Overview

ShowDirector has been successfully ported to the v2 Actor system as `ShowDirectorActor`. The implementation maintains full compatibility with the original ShowDirector functionality while leveraging the Actor-based architecture for cross-core communication.

## Architecture

### Actor Configuration
- **Core:** Core 0 (background processing)
- **Priority:** 2 (lower than RendererActor)
- **Update Rate:** 20Hz (50ms tick interval)
- **Stack Size:** 2048 words (8KB)

### Integration Points

1. **ActorSystem Integration**
   - Created during `ActorSystem::init()`
   - Started after RendererActor (dependency order)
   - Accessible via `ActorSystem::instance().getShowDirector()`

2. **NarrativeEngine Integration**
   - Uses merged v2 `NarrativeEngine` (unified with v1 compatibility)
   - Automatically modulates narrative phase/tempo based on show chapters
   - Supports manual phase control via `setNarrativePhase()`

3. **RendererActor Communication**
   - Direct message-based communication
   - Sends commands: `SET_EFFECT`, `SET_BRIGHTNESS`, `SET_SPEED`, `SET_PALETTE`, etc.
   - Calls `startTransition()` for smooth effect transitions
   - Reads current state: `getBrightness()`, `getSpeed()`

## Message/Command Interface

### Commands (Incoming to ShowDirectorActor)

All commands are sent via `Actor::send()` to the ShowDirectorActor's message queue.

| Message Type | Parameters | Description |
|--------------|------------|-------------|
| `SHOW_LOAD` | `param1` = showId (0-9) | Load a builtin show by ID |
| `SHOW_START` | (none) | Start playback from beginning |
| `SHOW_STOP` | (none) | Stop playback and reset |
| `SHOW_PAUSE` | (none) | Pause playback (maintains position) |
| `SHOW_RESUME` | (none) | Resume from pause |
| `SHOW_SEEK` | `param4` = timeMs | Seek to specific time position |
| `SHOW_UNLOAD` | (none) | Unload current show |
| `SHUTDOWN` | (none) | Stop show during system shutdown |

**Example Usage:**
```cpp
// Load and start show #0 (Dawn)
auto& system = ActorSystem::instance();
auto* director = system.getShowDirector();

Message loadMsg(MessageType::SHOW_LOAD, 0);
director->send(loadMsg);

Message startMsg(MessageType::SHOW_START);
director->send(startMsg);
```

### Events (Published by ShowDirectorActor)

Events are published to the `MessageBus` for any subscribers to receive.

| Event Type | Parameters | Description |
|------------|------------|-------------|
| `SHOW_STARTED` | `param1` = showId | Show playback started |
| `SHOW_STOPPED` | `param1` = showId | Show playback stopped |
| `SHOW_PAUSED` | `param1` = showId | Show playback paused |
| `SHOW_RESUMED` | `param1` = showId | Show playback resumed |
| `SHOW_CHAPTER_CHANGED` | `param1` = chapterIndex, `param2` = showId | Chapter transition occurred |
| `SHOW_COMPLETED` | `param1` = showId | Show reached end (non-looping) |

**Example Subscription:**
```cpp
// In NetworkActor or other subscriber
bus::MessageBus::instance().subscribe(MessageType::SHOW_CHAPTER_CHANGED, this);

// In onMessage():
if (msg.type == MessageType::SHOW_CHAPTER_CHANGED) {
    uint8_t chapter = msg.param1;
    uint8_t showId = msg.param2;
    // Handle chapter change...
}
```

### Commands (Outgoing from ShowDirectorActor)

ShowDirectorActor sends commands to RendererActor via direct message queue.

| Message Type | Parameters | Description |
|--------------|------------|-------------|
| `SET_EFFECT` | `param1` = effectId | Change current effect |
| `SET_BRIGHTNESS` | `param1` = brightness (0-255) | Set global brightness |
| `SET_SPEED` | `param1` = speed (1-50) | Set animation speed |
| `SET_PALETTE` | `param1` = paletteId | Change color palette |
| `SET_INTENSITY` | `param1` = intensity (0-255) | Set effect intensity (future) |
| `SET_SATURATION` | `param1` = saturation (0-255) | Set color saturation (future) |
| `SET_COMPLEXITY` | `param1` = complexity (0-255) | Set pattern complexity (future) |
| `SET_VARIATION` | `param1` = variation (0-255) | Set pattern variation (future) |

**Note:** Intensity, Saturation, Complexity, and Variation are placeholders for future RendererActor functionality. Currently return default values.

## Cue Execution

ShowDirectorActor executes cues from the show definition:

### Cue Types

1. **CUE_EFFECT** - Change effect
   - Uses `RendererActor::startTransition()` if transition type specified
   - Otherwise sends `SET_EFFECT` message

2. **CUE_PARAMETER_SWEEP** - Smooth parameter interpolation
   - Uses `ParameterSweeper` for smooth transitions
   - Supports: BRIGHTNESS, SPEED, INTENSITY, SATURATION, COMPLEXITY, VARIATION

3. **CUE_PALETTE** - Change color palette
   - Sends `SET_PALETTE` message

4. **CUE_NARRATIVE** - Modulate NarrativeEngine
   - Sets narrative phase and tempo
   - Integrates with chapter tension levels

5. **CUE_TRANSITION** - Trigger transition without effect change
   - Future: Direct transition trigger (currently placeholder)

6. **CUE_ZONE_CONFIG** - Configure zone settings
   - Future: ZoneComposer integration

7. **CUE_MARKER** - Sync point marker
   - No action, used for timing reference

## NarrativeEngine Integration

### Automatic Modulation

ShowDirectorActor automatically modulates NarrativeEngine based on show chapters:

```cpp
// Chapter defines:
ShowChapter {
    narrativePhase: SHOW_PHASE_BUILD,  // Maps to NarrativePhase::PHASE_BUILD
    tensionLevel: 128,                  // 0-255, influences tempo
    ...
}

// ShowDirectorActor converts:
NarrativePhase phase = PHASE_BUILD;
float tempo = 8000.0f - (tension / 255.0f) * 6000.0f;  // 2s-8s range
uint32_t duration = 30000 - (tension / 255.0f) * 25000;  // 5s-30s range

m_narrative->setPhase(phase, duration);
m_narrative->setTempo(tempo);
```

### Manual Control

ShowDirectorActor also supports manual narrative control via cues:

```cpp
// CUE_NARRATIVE cue data:
// data[0] = phase (0-3)
// data[1-2] = tempoMs (little-endian, 2000-8000ms)

void modulateNarrative(uint8_t phase, uint8_t tension);
void setNarrativePhase(NarrativePhase phase, uint32_t durationMs);
```

## State Access

### Read-Only Accessors

```cpp
bool hasShow() const;                    // Is a show loaded?
bool isPlaying() const;                  // Is show actively playing?
bool isPaused() const;                   // Is show paused?
float getProgress() const;               // 0.0-1.0 progress
uint8_t getCurrentChapter() const;       // Current chapter index
uint8_t getCurrentShowId() const;        // Current show ID (0-9)
uint32_t getElapsedMs() const;           // Elapsed time in ms
uint32_t getRemainingMs() const;         // Remaining time in ms
```

**Example:**
```cpp
auto* director = ActorSystem::instance().getShowDirector();
if (director && director->hasShow()) {
    Serial.printf("Show: %d, Chapter: %d, Progress: %.1f%%\n",
                  director->getCurrentShowId(),
                  director->getCurrentChapter(),
                  director->getProgress() * 100.0f);
}
```

## Implementation Details

### Parameter Sweeper Callbacks

ShowDirectorActor provides static callbacks for `ParameterSweeper`:

- `applyParamValue()` - Sends appropriate message to RendererActor
- `getParamValue()` - Reads current value from RendererActor

**Note:** Currently only BRIGHTNESS and SPEED are fully implemented. Other parameters return default values until RendererActor supports them.

### Cue Buffer

Uses a fixed-size buffer for cue processing:
```cpp
ShowCue m_cueBuffer[CueScheduler::MAX_CUES_PER_FRAME];  // 4 cues max per frame
```

### PROGMEM Show Definitions

All show data stored in PROGMEM (flash) to minimize RAM usage:
- Show definitions: ~200 bytes each
- Cues: 10 bytes each
- Total flash usage: ~2KB for 10 builtin shows

## Testing

### Manual Testing

```cpp
// In main loop or test code:
auto& system = ActorSystem::instance();
auto* director = system.getShowDirector();

// Load show #0
Message load(MessageType::SHOW_LOAD, 0);
director->send(load);

// Start playback
Message start(MessageType::SHOW_START);
director->send(start);

// Monitor progress
if (director->isPlaying()) {
    Serial.printf("Progress: %.1f%%, Chapter: %d\n",
                  director->getProgress() * 100.0f,
                  director->getCurrentChapter());
}
```

### Event Monitoring

```cpp
// Subscribe to show events
bus::MessageBus::instance().subscribe(MessageType::SHOW_CHAPTER_CHANGED, this);

// In onMessage():
if (msg.type == MessageType::SHOW_CHAPTER_CHANGED) {
    Serial.printf("Chapter changed: %d\n", msg.param1);
}
```

## Future Enhancements

1. **Full Parameter Support**
   - Implement `getIntensity()`, `getSaturation()`, `getComplexity()`, `getVariation()` in RendererActor
   - Complete ParameterSweeper integration for all parameters

2. **Zone System Integration**
   - Support `CUE_ZONE_CONFIG` for multi-zone shows
   - Integrate with ZoneComposer

3. **Transition-Only Cues**
   - Implement `CUE_TRANSITION` for transition effects without effect change
   - Add `TRIGGER_TRANSITION` message type

4. **Show Loading from Storage**
   - Support loading shows from SPIFFS/SD card
   - Dynamic show compilation from JSON

5. **Show Recording**
   - Record manual effect changes as cues
   - Export shows for sharing

## Migration Notes

### From v1 ShowDirector

The v2 ShowDirectorActor maintains API compatibility where possible:

| v1 Method | v2 Equivalent |
|-----------|---------------|
| `ShowDirector::getInstance().loadShowById(id)` | `Message(MessageType::SHOW_LOAD, id)` → `director->send()` |
| `ShowDirector::getInstance().start()` | `Message(MessageType::SHOW_START)` → `director->send()` |
| `ShowDirector::getInstance().update()` | Automatic (20Hz tick) |
| `ShowDirector::getInstance().isPlaying()` | `director->isPlaying()` |
| `ShowDirector::getInstance().getProgress()` | `director->getProgress()` |

### Key Differences

1. **No Singleton Pattern** - Access via `ActorSystem::instance().getShowDirector()`
2. **Message-Based Control** - All control via messages, not direct method calls
3. **Automatic Updates** - No need to call `update()` manually
4. **Event Publishing** - Subscribe to events instead of polling

## Conclusion

Phase 4 is **COMPLETE**. ShowDirectorActor is fully implemented, integrated, and ready for use. The message/command interface is well-defined and documented. The system maintains compatibility with the original ShowDirector while leveraging the Actor architecture for improved cross-core communication and scalability.

---

**Next Steps:**
- Phase 5: Additional Actor implementations (NetworkActor, HmiActor, etc.)
- Phase 6: Plugin system integration
- Phase 7: Full v1 → v2 migration

