# ShowDirector - Choreographed Light Show System

The ShowDirector is LightwaveOS's orchestration engine for multi-minute LED light shows. It provides chapter-based narrative structure, timed cue execution, parameter sweeps, and integration with the NarrativeEngine and TransitionEngine systems.

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Architecture Design](#2-architecture-design)
3. [Implementation Details](#3-implementation-details)
4. [Installation & Setup](#4-installation--setup)
5. [Usage Instructions](#5-usage-instructions)
6. [Extension & Customization](#6-extension--customization)
7. [Improvement & Contribution](#7-improvement--contribution)
8. [Troubleshooting](#8-troubleshooting)

---

## 1. Project Overview

### Purpose

The ShowDirector system enables choreographed, multi-minute LED light shows that tell visual stories. Each show is divided into **chapters** (narrative segments) containing **cues** (timed actions) that change effects, sweep parameters, and modulate the lighting atmosphere.

### Key Features

- **10 Builtin Shows**: Pre-defined shows ranging from 2-10 minutes
  - Dawn, Storm, Meditation, Celebration, Cosmos, Forest, Heartbeat, Ocean, Energy, Ambient
- **7 Cue Types**: Effect changes, parameter sweeps, palette changes, transitions, narrative modulation, zone configuration, markers
- **6 Sweepable Parameters**: Brightness, Speed, Intensity, Saturation, Complexity, Variation
- **8 Concurrent Sweeps**: Smooth interpolation of multiple parameters simultaneously
- **Chapter-Based Narrative**: 4 narrative phases (Build, Hold, Release, Rest) with tension levels
- **Looping Support**: Shows can run continuously (e.g., Meditation, Ambient)
- **Full API Control**: REST endpoints and WebSocket for remote control

### Target Audience

- **Developers**: Extending the system with custom shows or new cue types
- **Integrators**: Connecting ShowDirector to external controllers or automation systems
- **Users**: Understanding how to use the web interface for show playback

### Use Cases

- **Ambient Lighting**: Continuous background evolution (Ambient show, 10 min loop)
- **Events**: Dramatic story-driven sequences (Storm, Celebration)
- **Wellness**: Breathing-synchronized patterns (Meditation, Heartbeat)
- **Installations**: Long-running artistic displays

---

## 2. Architecture Design

### System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      Web Interface                          │
│     data/index.html (Shows Panel) + data/app.js             │
└─────────────────────────────────────────────────────────────┘
                              │
                    REST API + WebSocket
                              │
┌─────────────────────────────────────────────────────────────┐
│                     WebServer.cpp                            │
│   /api/v1/shows/* endpoints + show.* WebSocket handlers     │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                     ShowDirector                             │
│               (Singleton Orchestrator)                       │
│                                                              │
│  ┌──────────────────┐  ┌────────────────────────────────┐   │
│  │   CueScheduler   │  │     ParameterSweeper           │   │
│  │  (time-sorted    │  │  (8 concurrent interpolations) │   │
│  │   PROGMEM queue) │  │                                │   │
│  └──────────────────┘  └────────────────────────────────┘   │
│                                                              │
│  Integrates with:                                            │
│  ├── NarrativeEngine (tempo/phase modulation)               │
│  ├── TransitionEngine (12 visual transition types)          │
│  └── ZoneComposer (zone-specific effects - future)          │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                     Effect System                            │
│   currentEffect, currentPalette, brightnessVal, etc.        │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    LED Buffer + FastLED                     │
│                    leds[320] → WS2812                        │
└─────────────────────────────────────────────────────────────┘
```

### Component Responsibilities

| Component | Purpose | RAM | Location |
|-----------|---------|-----|----------|
| **ShowDirector** | Main orchestrator, playback control, cue execution | 20 bytes | `src/core/shows/ShowDirector.h/cpp` |
| **CueScheduler** | Time-sorted PROGMEM queue traversal | 8 bytes | `src/core/shows/CueScheduler.h` |
| **ParameterSweeper** | Concurrent parameter interpolations | 80 bytes | `src/core/shows/ParameterSweeper.h/cpp` |
| **ShowTypes** | Data structures (ShowCue, ShowChapter, ShowDefinition) | N/A | `src/core/shows/ShowTypes.h` |
| **BuiltinShows** | 10 PROGMEM show definitions | ~2KB Flash | `src/core/shows/BuiltinShows.h` |

### Data Flow

1. **User starts show** → REST/WebSocket → `ShowDirector::start()`
2. **Each frame** → `ShowDirector::update()` called from main loop
3. **Cue execution** → `CueScheduler::getReadyCues()` returns due cues
4. **Effect change** → `executeCue()` sets `currentEffect`, triggers transitions
5. **Parameter sweep** → `ParameterSweeper::update()` interpolates values
6. **Narrative modulation** → `NarrativeEngine::setTempo()` adjusts breathing cycle
7. **Status broadcast** → WebSocket pushes progress to connected clients

### Technology Stack

- **Platform**: ESP32-S3 @ 240MHz, 8MB Flash
- **LED Control**: FastLED 3.10.0 with RMT driver
- **JSON**: ArduinoJson 7.0.4
- **Storage**: PROGMEM for show definitions (Flash, not RAM)
- **Web**: ESPAsyncWebServer 3.6.0

---

## 3. Implementation Details

### Core Data Structures

#### CueType Enum

```cpp
enum CueType : uint8_t {
    CUE_EFFECT = 0,           // Change effect on zone or global
    CUE_PARAMETER_SWEEP,      // Interpolate parameter over time
    CUE_ZONE_CONFIG,          // Configure zone settings
    CUE_TRANSITION,           // Trigger TransitionEngine type
    CUE_NARRATIVE,            // Modulate NarrativeEngine tempo/phase
    CUE_PALETTE,              // Change color palette
    CUE_MARKER                // Sync point marker (no action)
};
```

#### ParamId Enum (Sweepable Parameters)

```cpp
enum ParamId : uint8_t {
    PARAM_BRIGHTNESS = 0,     // 0-255 LED brightness
    PARAM_SPEED,              // 1-255 animation speed
    PARAM_INTENSITY,          // Effect intensity
    PARAM_SATURATION,         // Color saturation
    PARAM_COMPLEXITY,         // Effect complexity
    PARAM_VARIATION,          // Effect variation
    PARAM_COUNT               // Sentinel (6 total)
};
```

#### ShowCue Struct (10 bytes)

```cpp
struct ShowCue {
    uint32_t timeMs;          // Execution time from show start
    CueType type;             // Action type
    uint8_t targetZone;       // Zone ID (ZONE_GLOBAL = 0xFF for all)
    uint8_t data[4];          // Cue-specific data

    // Accessor methods for type safety:
    uint8_t effectId() const;           // CUE_EFFECT
    uint8_t effectTransition() const;   // CUE_EFFECT
    uint8_t sweepParamId() const;       // CUE_PARAMETER_SWEEP
    uint8_t sweepTargetValue() const;   // CUE_PARAMETER_SWEEP
    uint16_t sweepDurationMs() const;   // CUE_PARAMETER_SWEEP
    // ... more accessors for each type
};
```

**Data Layout by Cue Type**:

| Type | data[0] | data[1] | data[2] | data[3] |
|------|---------|---------|---------|---------|
| `CUE_EFFECT` | effectId | transitionType | - | - |
| `CUE_PARAMETER_SWEEP` | paramId | targetValue | durLow | durHigh |
| `CUE_PALETTE` | paletteId | - | - | - |
| `CUE_NARRATIVE` | phase | tempoLow | tempoHigh | - |
| `CUE_TRANSITION` | transitionType | durLow | durHigh | - |

#### ShowChapter Struct (20 bytes)

```cpp
struct ShowChapter {
    const char* name;             // PROGMEM string pointer
    uint32_t startTimeMs;         // When chapter starts
    uint32_t durationMs;          // Chapter duration
    uint8_t narrativePhase;       // ShowNarrativePhase (Build/Hold/Release/Rest)
    uint8_t tensionLevel;         // 0-255 (influences tempo/intensity)
    uint8_t cueStartIndex;        // First cue index for this chapter
    uint8_t cueCount;             // Number of cues in this chapter
};
```

#### ShowDefinition Struct (PROGMEM)

```cpp
struct ShowDefinition {
    const char* id;               // Short identifier (e.g., "dawn")
    const char* name;             // Display name (e.g., "Dawn")
    uint32_t totalDurationMs;     // Total show length
    uint8_t chapterCount;         // Number of chapters
    uint8_t totalCues;            // Total cues across all chapters
    bool looping;                 // Whether show loops
    const ShowChapter* chapters;  // PROGMEM array of chapters
    const ShowCue* cues;          // PROGMEM array of all cues
};
```

### Memory Budget

| Component | RAM | Flash |
|-----------|-----|-------|
| ShowPlaybackState | 20 bytes | - |
| CueScheduler | 8 bytes | - |
| ParameterSweeper (8 sweeps) | 80 bytes | - |
| ShowDirector cue buffer | 40 bytes | - |
| **Total RAM** | **~148 bytes** | - |
| 10 Builtin Shows | - | ~2KB |
| Show strings (PROGMEM) | - | ~500 bytes |
| **Total Flash** | - | **~2.5KB** |

### Design Decisions

1. **Singleton Pattern**: `ShowDirector::getInstance()` ensures single orchestrator
2. **PROGMEM Storage**: All show data in Flash to minimize RAM usage
3. **Linear Cue Scan**: Simple sequential cue processing (no heap allocation)
4. **Accessor Methods**: Type-safe data access instead of C++ unions
5. **Header-Only CueScheduler**: Reduced compilation complexity

---

## 4. Installation & Setup

### Prerequisites

- PlatformIO with ESP32 platform
- LightwaveOS codebase with WiFi build environment

### Feature Flag

Enable ShowDirector in `src/config/features.h`:

```cpp
#define FEATURE_SHOWS 1
```

This flag is typically defined in the `esp32dev_audio` build environment.

### Build Command

```bash
# Full build with WiFi and Shows
pio run -e esp32dev_audio -t upload

# Monitor serial output
pio device monitor -b 115200
```

### Dependencies

No additional dependencies required. ShowDirector uses:
- FastLED (already included)
- ArduinoJson (already included)
- ESPAsyncWebServer (already included)

### Integration Point

ShowDirector is updated in the main loop (`src/main.cpp`):

```cpp
void loop() {
    // ... other processing ...

    #if FEATURE_SHOWS
    ShowDirector::getInstance().update();  // Call BEFORE effect render
    #endif

    // ... effect rendering ...
}
```

---

## 5. Usage Instructions

### REST API Endpoints

All endpoints use the v1 API prefix: `/api/v1/shows/*`

| Endpoint | Method | Description | Request Body |
|----------|--------|-------------|--------------|
| `/api/v1/shows` | GET | List all available shows | - |
| `/api/v1/shows/status` | GET | Get current playback status | - |
| `/api/v1/shows/start` | POST | Start a show | `{"showId": 0}` |
| `/api/v1/shows/stop` | POST | Stop current show | - |
| `/api/v1/shows/pause` | POST | Toggle pause/resume | - |
| `/api/v1/shows/seek` | POST | Seek to position | `{"timeMs": 30000}` |

#### Example: List Shows

```bash
curl http://lightwaveos.local/api/v1/shows
```

**Response**:
```json
{
  "success": true,
  "data": {
    "shows": [
      {"id": 0, "name": "Dawn", "duration": 180000, "looping": false},
      {"id": 1, "name": "Storm", "duration": 240000, "looping": false},
      {"id": 2, "name": "Meditation", "duration": 300000, "looping": true},
      // ... 7 more shows
    ]
  },
  "timestamp": 12345,
  "version": "1.0"
}
```

#### Example: Start Show

```bash
curl -X POST http://lightwaveos.local/api/v1/shows/start \
  -H "Content-Type: application/json" \
  -d '{"showId": 0}'
```

**Response**:
```json
{
  "success": true,
  "data": {
    "showId": 0,
    "showName": "Dawn",
    "duration": 180000,
    "action": "started"
  },
  "timestamp": 12346
}
```

#### Example: Get Status

```bash
curl http://lightwaveos.local/api/v1/shows/status
```

**Response**:
```json
{
  "success": true,
  "data": {
    "playing": true,
    "paused": false,
    "showId": 0,
    "showName": "Dawn",
    "duration": 180000,
    "elapsed": 45123,
    "remaining": 134877,
    "progress": 0.25,
    "chapter": 1,
    "chapterName": "First Light",
    "tension": 75,
    "looping": false
  }
}
```

### WebSocket Commands

Connect to `ws://lightwaveos.local/ws` and send JSON commands:

```javascript
// Start a show
ws.send(JSON.stringify({type: "show.start", showId: 2}));

// Stop the show
ws.send(JSON.stringify({type: "show.stop"}));

// Pause/resume
ws.send(JSON.stringify({type: "show.pause"}));

// Request status
ws.send(JSON.stringify({type: "show.status"}));

// List available shows
ws.send(JSON.stringify({type: "show.list"}));
```

**WebSocket Events** (server → client):

| Event | Description |
|-------|-------------|
| `show.progress` | Progress update with elapsed, remaining, chapter info |
| `show.started` | Show has started |
| `show.stopped` | Show has stopped |
| `show.paused` | Show paused or resumed |

### Web Interface

The Shows panel is located in the web UI at `http://lightwaveos.local`:

1. **Shows Panel**: Click to expand the Shows section
2. **Show Grid**: Click any show card to start it
3. **Now Playing Bar**: Shows current show name and chapter
4. **Progress Bar**: Visual progress indicator
5. **Playback Controls**: Pause/Stop buttons when a show is playing

---

## 6. Extension & Customization

### Creating Custom Shows

Add new shows in `src/core/shows/BuiltinShows.h`:

#### Step 1: Define PROGMEM Strings

```cpp
static const char PROGMEM MYSHOW_ID[] = "myshow";
static const char PROGMEM MYSHOW_NAME[] = "My Custom Show";
static const char PROGMEM MYSHOW_CH0_NAME[] = "Introduction";
static const char PROGMEM MYSHOW_CH1_NAME[] = "Climax";
static const char PROGMEM MYSHOW_CH2_NAME[] = "Conclusion";
```

#### Step 2: Define Cues Array

```cpp
static const ShowCue PROGMEM MYSHOW_CUES[] = {
    // Introduction (0-30s): Start with Fire effect, low brightness
    {0,     CUE_EFFECT,          ZONE_GLOBAL, {0, 0, 0, 0}},      // Fire, no transition
    {0,     CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_BRIGHTNESS, 80, 0, 0}},  // Instant
    {0,     CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_BUILD, DUR_LO(6000), DUR_HI(6000), 0}},

    // Climax (30s-60s): Switch to Collision, max brightness
    {30000, CUE_EFFECT,          ZONE_GLOBAL, {9, 2, 0, 0}},      // Collision with fade
    {30000, CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_BRIGHTNESS, 255, DUR_LO(10000), DUR_HI(10000)}},
    {30000, CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_HOLD, DUR_LO(2000), DUR_HI(2000), 0}},

    // Conclusion (60s-90s): Fade to Ocean, gentle
    {60000, CUE_EFFECT,          ZONE_GLOBAL, {2, 1, 0, 0}},      // Ocean with cross-fade
    {60000, CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_BRIGHTNESS, 60, DUR_LO(30000), DUR_HI(30000)}},
    {60000, CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_RELEASE, DUR_LO(8000), DUR_HI(8000), 0}},
};
```

#### Step 3: Define Chapters Array

```cpp
static const ShowChapter PROGMEM MYSHOW_CHAPTERS[] = {
    // name,            startMs, durationMs, phase,             tension, cueStart, cueCount
    {MYSHOW_CH0_NAME,   0,       30000,      SHOW_PHASE_BUILD,   100,     0,        3},
    {MYSHOW_CH1_NAME,   30000,   30000,      SHOW_PHASE_HOLD,    255,     3,        3},
    {MYSHOW_CH2_NAME,   60000,   30000,      SHOW_PHASE_RELEASE, 75,      6,        3},
};
```

#### Step 4: Add to BUILTIN_SHOWS Array

```cpp
static const ShowDefinition PROGMEM BUILTIN_SHOWS[] = {
    // ... existing shows ...

    // Show 10: My Custom Show
    {MYSHOW_ID, MYSHOW_NAME, 90000, 3, 9, false, MYSHOW_CHAPTERS, MYSHOW_CUES},
};
```

### Cue Type Reference

#### CUE_EFFECT

Changes the current effect with optional transition.

```cpp
{timeMs, CUE_EFFECT, ZONE_GLOBAL, {effectId, transitionType, 0, 0}}
```

- `effectId`: Index into effects[] array (0-45)
- `transitionType`: TransitionType enum (0=None, 1=CrossFade, 2=Wipe, etc.)

#### CUE_PARAMETER_SWEEP

Smooth interpolation of a parameter over time.

```cpp
{timeMs, CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {paramId, targetValue, DUR_LO(ms), DUR_HI(ms)}}
```

- `paramId`: ParamId enum (0-5)
- `targetValue`: Target value (0-255)
- Duration encoded as low/high bytes (max 65535ms)

#### CUE_NARRATIVE

Modulates the NarrativeEngine breathing cycle.

```cpp
{timeMs, CUE_NARRATIVE, ZONE_GLOBAL, {phase, DUR_LO(tempoMs), DUR_HI(tempoMs), 0}}
```

- `phase`: ShowNarrativePhase (Build=0, Hold=1, Release=2, Rest=3)
- `tempoMs`: Breathing cycle period (2000-15000ms typical)

### API Integration for External Control

For automation systems, create a simple controller:

```python
import requests
import time

BASE_URL = "http://lightwaveos.local/api/v1/shows"

def start_show(show_id):
    response = requests.post(f"{BASE_URL}/start", json={"showId": show_id})
    return response.json()

def get_status():
    response = requests.get(f"{BASE_URL}/status")
    return response.json()

def wait_for_show():
    while True:
        status = get_status()
        if not status["data"]["playing"]:
            break
        print(f"Progress: {status['data']['progress']*100:.1f}%")
        time.sleep(1)

# Run Dawn show and wait for completion
start_show(0)
wait_for_show()
print("Show complete!")
```

---

## 7. Improvement & Contribution

### Known Limitations

1. **Global Parameters Only**: Parameter sweeps affect global state, not per-zone (ZoneComposer integration pending)
2. **Fixed 1-Second Effect Transitions**: CUE_EFFECT uses hardcoded 1000ms transition duration
3. **Sweeps Cancelled on Seek**: Active parameter sweeps are cancelled when seeking to prevent stale values
4. **No Runtime Show Loading**: Shows must be compiled into firmware (no JSON file loading)

### Roadmap

| Feature | Priority | Status |
|---------|----------|--------|
| JSON show file loading from SPIFFS | High | Planned |
| User-defined shows via web UI | High | Planned |
| Zone-aware parameter sweeps | Medium | Pending ZoneComposer update |
| Show export/import | Medium | Planned |
| Configurable effect transition duration | Low | Planned |
| Beat-synced cue timing | Low | Exploration |

### Testing Approach

Currently, testing is manual:

1. **Web UI Testing**: Use the Shows panel to start/stop/pause shows
2. **API Validation**: Use curl or Postman to test REST endpoints
3. **WebSocket Testing**: Browser dev tools or wscat for real-time events
4. **Serial Monitoring**: Watch for errors at 115200 baud

### Performance Characteristics

- **Frame Budget**: ShowDirector::update() < 0.5ms per call
- **Cue Processing**: Up to 4 cues per frame (MAX_CUES_PER_FRAME)
- **Sweep Updates**: Linear scan of 8 sweep slots
- **Memory Stable**: No heap allocations during playback
- **Compatible with 120 FPS**: Well under 8.33ms frame budget

### Contributing Guidelines

1. Follow CENTER ORIGIN constraint for any effects referenced by shows
2. Use PROGMEM for all show data to preserve RAM
3. Test with the WiFi build environment
4. Document any new cue types in ShowTypes.h

---

## 8. Troubleshooting

### Common Issues

#### Show Not Starting

**Symptom**: Clicking a show in the web UI does nothing.

**Causes**:
1. `FEATURE_SHOWS` not enabled - Check `src/config/features.h`
2. Build environment mismatch - Use `esp32dev_audio` environment
3. API rate limiting - Wait and retry

**Verification**:
```bash
curl http://lightwaveos.local/api/v1/shows
# Should return list of shows, not 404
```

#### Progress Not Updating

**Symptom**: Show starts but progress bar doesn't move.

**Causes**:
1. WebSocket disconnected - Check browser console
2. JavaScript error - Check for errors in console
3. Status polling not running - Verify `startShowStatusPolling()` called

**Verification**:
```bash
# Check status directly
curl http://lightwaveos.local/api/v1/shows/status
# elapsed should increase each call
```

#### Effect Not Changing

**Symptom**: Cues fire but effect doesn't change visually.

**Causes**:
1. Invalid effectId - Must be 0-45 (number of registered effects)
2. Zone not global - Check targetZone is ZONE_GLOBAL (0xFF)
3. Transition blocking - Previous transition still in progress

**Verification**:
Check Serial output for effect change logs:
```
[ShowDirector] executeCue: CUE_EFFECT id=6
```

### Debug Logging

Serial output at 115200 baud shows:
- Show load/start/stop events
- Cue execution
- Chapter transitions
- Parameter sweep activity

### Log Interpretation

```
[ShowDirector] loadShowById(0)          // Show loaded
[ShowDirector] start()                   // Playback started
[ShowDirector] executeCue: CUE_EFFECT    // Effect change
[ShowDirector] updateChapter(1)          // Chapter transition
[ParameterSweeper] startSweep BRIGHTNESS // Sweep started
[ShowDirector] handleShowEnd()           // Show completed
```

### Recovery Steps

1. **Stop and restart**: `POST /api/v1/shows/stop`, then start again
2. **Clear WebSocket**: Refresh the browser page
3. **Power cycle**: Sometimes needed if ESP32 is in bad state
4. **Rebuild firmware**: `pio run -e esp32dev_audio -t clean && pio run -e esp32dev_audio -t upload`

---

## File Reference

| File | Purpose |
|------|---------|
| `src/core/shows/ShowTypes.h` | Data structures (ShowCue, ShowChapter, ShowDefinition, enums) |
| `src/core/shows/ShowDirector.h` | Class interface and documentation |
| `src/core/shows/ShowDirector.cpp` | Implementation (~467 lines) |
| `src/core/shows/CueScheduler.h` | Time-sorted PROGMEM queue |
| `src/core/shows/ParameterSweeper.h` | Parameter interpolation interface |
| `src/core/shows/ParameterSweeper.cpp` | Sweep implementation |
| `src/core/shows/BuiltinShows.h` | 10 show definitions in PROGMEM |
| `src/network/WebServer.cpp` | REST API handlers (lines 1915-1958, 2833-2956) |
| `src/network/WebServer.h` | Handler declarations |
| `data/index.html` | Shows panel HTML |
| `data/app.js` | Shows JavaScript (state, API, WebSocket, UI) |
| `data/styles.css` | Show card styling |

---

*Document Version: 1.0*
*Last Updated: December 21, 2025*
*ShowDirector System: LightwaveOS v2*
