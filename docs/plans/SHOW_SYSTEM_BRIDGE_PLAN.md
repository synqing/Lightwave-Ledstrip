# Show System Bridge: Firmware ↔ Dashboard Integration Plan

**Status:** Planning  
**Priority:** High  
**Created:** 2025-01-03  
**Last Updated:** 2025-01-03

## Executive Summary

This document outlines a comprehensive plan to bridge the gap between the firmware's cue-based show system (`ShowNode`) and the dashboard's scene-based timeline editor (`ShowsTab.tsx`). The goal is to enable full bidirectional synchronization, allowing users to create, edit, and play shows through the drag-and-drop UI while the firmware executes them in real-time.

---

## 1. Gap Analysis

### 1.1 Firmware Model (ShowNode)

**Data Structures:**
- `ShowDefinition`: Complete show with chapters and cues (stored in PROGMEM)
- `ShowCue`: Time-stamped action (10 bytes)
  - Types: `CUE_EFFECT`, `CUE_PARAMETER_SWEEP`, `CUE_ZONE_CONFIG`, `CUE_TRANSITION`, `CUE_NARRATIVE`, `CUE_PALETTE`, `CUE_MARKER`
  - Time: Absolute milliseconds from show start
  - Target: Zone ID (0xFF = global) or specific zone (0-3)
- `ShowChapter`: Narrative structure (20 bytes)
  - Name, start time, duration, narrative phase, tension level
  - Cue index range (cueStartIndex, cueCount)
- `ShowPlaybackState`: Runtime state (20 bytes RAM)
  - Current show ID, chapter index, cue index
  - Playing/paused status, timing information

**Control Interface:**
- Message-based: `SHOW_LOAD`, `SHOW_START`, `SHOW_STOP`, `SHOW_PAUSE`, `SHOW_RESUME`, `SHOW_SEEK`, `SHOW_UNLOAD`
- Built-in shows: 10 predefined shows in PROGMEM
- No REST API endpoints currently exist
- No WebSocket commands for shows

**Limitations:**
- Shows are read-only (compiled into firmware)
- No dynamic show creation/editing
- No persistence of custom shows
- No API for show management

### 1.2 Dashboard Model (ShowsTab.tsx)

**Data Structures:**
- `TimelineScene`: Visual scene block
  - `id`: Unique identifier (string)
  - `zoneId`: 0=Global, 1-4=Zones (number)
  - `effectName`: Display name (string)
  - `startTimePercent`: Position 0-100 (number)
  - `durationPercent`: Width 0-100 (number)
  - `accentColor`: Tailwind color class (string)
- `Show`: Complete show definition
  - `id`: Unique identifier (string)
  - `name`: Display name (string)
  - `durationSeconds`: Total duration (number)
  - `scenes`: Array of `TimelineScene`
  - `isSaved`: Save status (boolean)

**UI Features:**
- Drag-and-drop scene arrangement
- Visual timeline with zone tracks
- Scene editing (start time, duration, zone)
- Transport controls (play, pause, skip)
- Keyboard navigation
- Scene deletion

**Limitations:**
- Completely local state (no persistence)
- No connection to firmware
- No real-time playback synchronization
- No show save/load functionality

### 1.3 Key Differences

| Aspect | Firmware | Dashboard |
|--------|----------|-----------|
| **Time Model** | Absolute milliseconds | Percentage (0-100) |
| **Data Model** | Cue-based (actions) | Scene-based (blocks) |
| **Storage** | PROGMEM (read-only) | Local state (ephemeral) |
| **Granularity** | Individual actions | Continuous blocks |
| **Zones** | 0xFF=Global, 0-3=Zones | 0=Global, 1-4=Zones |
| **Effects** | Effect ID (uint8_t) | Effect name (string) |
| **Persistence** | None (built-in only) | None |
| **API** | Message-based (internal) | None |

---

## 2. Design Strategy

### 2.1 Translation Model: Scenes ↔ Cues

**Core Concept:** A `TimelineScene` represents a continuous effect application over a time range. This must be translated into one or more `ShowCue` entries.

**Translation Rules:**

1. **Scene → Cues:**
   - **Start Cue**: `CUE_EFFECT` at scene start time
   - **End Cue** (optional): `CUE_EFFECT` to next scene's effect, or `CUE_MARKER` for scene boundary
   - **Parameter Sweeps**: If scene has parameter changes, add `CUE_PARAMETER_SWEEP` cues
   - **Transitions**: If scene has transition type, use `CUE_TRANSITION` or effect transition parameter

2. **Cues → Scenes:**
   - Group consecutive `CUE_EFFECT` cues with same effect into a scene
   - Calculate scene duration from cue time differences
   - Extract zone from cue target
   - Map effect ID to effect name via PatternRegistry

3. **Time Conversion:**
   - **UI → Firmware**: `timeMs = (startTimePercent / 100) * durationSeconds * 1000`
   - **Firmware → UI**: `startTimePercent = (timeMs / (durationSeconds * 1000)) * 100`

4. **Zone Mapping:**
   - **UI → Firmware**: `0 → 0xFF (ZONE_GLOBAL)`, `1-4 → 0-3`
   - **Firmware → UI**: `0xFF → 0`, `0-3 → 1-4`

### 2.2 Data Flow Architecture

```
┌─────────────────┐         ┌──────────────────┐         ┌─────────────────┐
│   Dashboard     │         │   Translation     │         │    Firmware      │
│  (ShowsTab)    │◄────────┤     Layer         │────────►│   (ShowNode)     │
│                 │         │                   │         │                  │
│ TimelineScene[] │         │  Scene ↔ Cue      │         │  ShowCue[]       │
│ Show (local)    │         │  Converter        │         │  ShowDefinition  │
└─────────────────┘         └──────────────────┘         └─────────────────┘
       │                              │                              │
       │                              │                              │
       ▼                              ▼                              ▼
┌─────────────────┐         ┌──────────────────┐         ┌─────────────────┐
│  REST API       │         │  WebSocket       │         │  Message Bus    │
│  /api/v1/shows  │         │  show.* commands │         │  SHOW_* events   │
└─────────────────┘         └──────────────────┘         └─────────────────┘
```

### 2.3 Storage Strategy

**Option A: NVS Storage (Recommended)**
- Store custom shows in NVS (Non-Volatile Storage)
- Limit: ~10-20 custom shows (memory constraints)
- Format: JSON or binary encoding
- Pros: Persistent, survives reboots
- Cons: Limited capacity, requires serialization

**Option B: RAM-Only (Temporary)**
- Store shows in RAM during session
- Lost on reboot
- Pros: Simple, no storage limits
- Cons: Not persistent

**Option C: Hybrid**
- Built-in shows: PROGMEM (read-only)
- Custom shows: NVS (read-write)
- Active show: RAM (runtime state)

**Recommendation:** Option C (Hybrid)

---

## 3. API Design

### 3.1 REST API Endpoints

**Base Path:** `/api/v1/shows`

#### 3.1.1 List Shows
```
GET /api/v1/shows
```

**Response:**
```json
{
  "success": true,
  "data": {
    "builtin": [
      {
        "id": 0,
        "name": "Dawn",
        "durationMs": 180000,
        "durationSeconds": 180,
        "chapterCount": 4,
        "cueCount": 8,
        "looping": false,
        "type": "builtin"
      }
    ],
    "custom": [
      {
        "id": "show-abc123",
        "name": "My Custom Show",
        "durationMs": 120000,
        "durationSeconds": 120,
        "sceneCount": 5,
        "isSaved": true,
        "type": "custom"
      }
    ]
  }
}
```

#### 3.1.2 Get Show Details
```
GET /api/v1/shows/{id}
```

**Parameters:**
- `id`: Show ID (number for builtin, string for custom)
- `format`: `"cues"` (firmware format) or `"scenes"` (UI format) - default: `"scenes"`

**Response (scenes format):**
```json
{
  "success": true,
  "data": {
    "id": "show-abc123",
    "name": "My Custom Show",
    "durationSeconds": 120,
    "durationMs": 120000,
    "type": "custom",
    "scenes": [
      {
        "id": "scene-1",
        "zoneId": 1,
        "effectId": 12,
        "effectName": "Center Pulse",
        "startTimeMs": 0,
        "startTimePercent": 0,
        "durationMs": 30000,
        "durationPercent": 25,
        "accentColor": "accent-cyan"
      }
    ],
    "chapters": []  // Optional: if show has chapters
  }
}
```

**Response (cues format):**
```json
{
  "success": true,
  "data": {
    "id": 0,
    "name": "Dawn",
    "type": "builtin",
    "cues": [
      {
        "timeMs": 0,
        "type": "CUE_EFFECT",
        "targetZone": 255,
        "effectId": 6,
        "transitionType": 0
      }
    ],
    "chapters": [...]
  }
}
```

#### 3.1.3 Create/Update Custom Show
```
POST /api/v1/shows
PUT /api/v1/shows/{id}
```

**Request Body:**
```json
{
  "name": "My Custom Show",
  "durationSeconds": 120,
  "scenes": [
    {
      "id": "scene-1",
      "zoneId": 1,
      "effectId": 12,
      "startTimePercent": 0,
      "durationPercent": 25
    }
  ]
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "id": "show-abc123",
    "name": "My Custom Show",
    "message": "Show saved successfully"
  }
}
```

#### 3.1.4 Delete Custom Show
```
DELETE /api/v1/shows/{id}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "id": "show-abc123",
    "message": "Show deleted successfully"
  }
}
```

#### 3.1.5 Get Current Show State
```
GET /api/v1/shows/current
```

**Response:**
```json
{
  "success": true,
  "data": {
    "showId": 0,
    "showName": "Dawn",
    "isPlaying": true,
    "isPaused": false,
    "progress": 0.45,
    "elapsedMs": 81000,
    "remainingMs": 99000,
    "currentChapter": 2,
    "currentChapterName": "Sunrise"
  }
}
```

#### 3.1.6 Control Show Playback
```
POST /api/v1/shows/control
```

**Request Body:**
```json
{
  "action": "start",  // "start", "stop", "pause", "resume", "seek"
  "showId": 0,        // Required for "start"
  "timeMs": 45000     // Required for "seek"
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "action": "start",
    "showId": 0,
    "message": "Show started"
  }
}
```

### 3.2 WebSocket Commands

**Command Pattern:** `show.{action}`

#### 3.2.1 List Shows
```json
{
  "type": "show.list",
  "requestId": "req-123"
}
```

**Response:**
```json
{
  "type": "show.list",
  "success": true,
  "data": {
    "builtin": [...],
    "custom": [...]
  },
  "requestId": "req-123"
}
```

#### 3.2.2 Get Show
```json
{
  "type": "show.get",
  "id": 0,
  "format": "scenes",
  "requestId": "req-124"
}
```

#### 3.2.3 Save Show
```json
{
  "type": "show.save",
  "name": "My Show",
  "durationSeconds": 120,
  "scenes": [...],
  "requestId": "req-125"
}
```

#### 3.2.4 Delete Show
```json
{
  "type": "show.delete",
  "id": "show-abc123",
  "requestId": "req-126"
}
```

#### 3.2.5 Control Playback
```json
{
  "type": "show.control",
  "action": "start",
  "showId": 0,
  "requestId": "req-127"
}
```

**Actions:**
- `start`: Start show (requires `showId`)
- `stop`: Stop current show
- `pause`: Pause current show
- `resume`: Resume paused show
- `seek`: Seek to time (requires `timeMs`)

#### 3.2.6 Get Current State
```json
{
  "type": "show.state",
  "requestId": "req-128"
}
```

### 3.3 WebSocket Events (Broadcast)

**Event Pattern:** `show.{event}`

#### 3.3.1 Show Started
```json
{
  "type": "show.started",
  "data": {
    "showId": 0,
    "showName": "Dawn",
    "timestamp": 1234567890
  }
}
```

#### 3.3.2 Show Stopped
```json
{
  "type": "show.stopped",
  "data": {
    "showId": 0,
    "timestamp": 1234567890
  }
}
```

#### 3.3.3 Show Paused/Resumed
```json
{
  "type": "show.paused",
  "data": {
    "showId": 0,
    "timestamp": 1234567890
  }
}
```

#### 3.3.4 Chapter Changed
```json
{
  "type": "show.chapterChanged",
  "data": {
    "showId": 0,
    "chapterIndex": 2,
    "chapterName": "Sunrise",
    "timestamp": 1234567890
  }
}
```

#### 3.3.5 Progress Update (Periodic)
```json
{
  "type": "show.progress",
  "data": {
    "showId": 0,
    "progress": 0.45,
    "elapsedMs": 81000,
    "remainingMs": 99000,
    "timestamp": 1234567890
  }
}
```

**Frequency:** Every 100ms during playback (10 Hz)

---

## 4. Implementation Plan

### 4.1 Phase 1: Core Translation Layer

**Files to Create:**
- `firmware/v2/src/core/shows/ShowTranslator.h`
- `firmware/v2/src/core/shows/ShowTranslator.cpp`

**Responsibilities:**
- Convert `TimelineScene[]` → `ShowCue[]`
- Convert `ShowCue[]` → `TimelineScene[]`
- Time conversion (ms ↔ percent)
- Zone mapping (UI ↔ firmware)
- Effect ID ↔ name mapping

**Key Functions:**
```cpp
namespace lightwaveos {
namespace shows {

class ShowTranslator {
public:
    // UI → Firmware
    static bool scenesToCues(
        const TimelineScene* scenes,
        uint8_t sceneCount,
        uint32_t totalDurationMs,
        ShowCue* outCues,
        uint8_t& outCueCount,
        uint8_t maxCues
    );

    // Firmware → UI
    static bool cuesToScenes(
        const ShowCue* cues,
        uint8_t cueCount,
        uint32_t totalDurationMs,
        TimelineScene* outScenes,
        uint8_t& outSceneCount,
        uint8_t maxScenes
    );

    // Time conversions
    static uint32_t percentToMs(float percent, uint32_t totalDurationMs);
    static float msToPercent(uint32_t timeMs, uint32_t totalDurationMs);

    // Zone conversions
    static uint8_t uiZoneToFirmware(uint8_t uiZoneId);
    static uint8_t firmwareZoneToUi(uint8_t firmwareZoneId);
};

} // namespace shows
} // namespace lightwaveos
```

**TypeScript Types (Dashboard):**
```typescript
// Add to lightwave-dashboard/src/types/timeline.ts

export interface ShowCue {
  timeMs: number;
  type: 'CUE_EFFECT' | 'CUE_PARAMETER_SWEEP' | 'CUE_TRANSITION' | 'CUE_PALETTE' | 'CUE_MARKER';
  targetZone: number;  // 255 = global
  data: {
    effectId?: number;
    transitionType?: number;
    paramId?: number;
    targetValue?: number;
    durationMs?: number;
    paletteId?: number;
  };
}

export interface ShowDefinition {
  id: string | number;
  name: string;
  durationMs: number;
  durationSeconds: number;
  type: 'builtin' | 'custom';
  cues?: ShowCue[];
  scenes?: TimelineScene[];
  chapters?: ShowChapter[];
}
```

### 4.2 Phase 2: Show Storage System

**Files to Create:**
- `firmware/v2/src/core/persistence/ShowStorage.h`
- `firmware/v2/src/core/persistence/ShowStorage.cpp`

**Responsibilities:**
- Save custom shows to NVS
- Load custom shows from NVS
- List available shows (builtin + custom)
- Delete custom shows
- Manage storage limits

**Storage Format:**
```cpp
// NVS Keys: "show_0", "show_1", ..., "show_9" (max 10 custom shows)
// Format: Binary encoding of ShowDefinition (compressed)

struct StoredShow {
    char name[32];
    uint32_t durationMs;
    uint8_t sceneCount;
    // Followed by scene data (variable length)
};
```

**Key Functions:**
```cpp
namespace lightwaveos {
namespace persistence {

class ShowStorage {
public:
    static constexpr uint8_t MAX_CUSTOM_SHOWS = 10;
    
    static bool saveShow(const char* id, const ShowDefinition& show);
    static bool loadShow(const char* id, ShowDefinition& outShow);
    static bool deleteShow(const char* id);
    static uint8_t listShows(ShowInfo* outShows, uint8_t maxShows);
    static uint8_t getCustomShowCount();
    static bool hasSpace();
};

} // namespace persistence
} // namespace lightwaveos
```

### 4.3 Phase 3: REST API Handlers

**Files to Create:**
- `firmware/v2/src/network/webserver/handlers/ShowHandlers.h`
- `firmware/v2/src/network/webserver/handlers/ShowHandlers.cpp`

**Handlers:**
```cpp
namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

class ShowHandlers {
public:
    // GET /api/v1/shows
    static void handleList(AsyncWebServerRequest* request,
                          NodeOrchestrator& orchestrator);

    // GET /api/v1/shows/{id}
    static void handleGet(AsyncWebServerRequest* request,
                         const String& showId,
                         const String& format,
                         NodeOrchestrator& orchestrator);

    // POST /api/v1/shows
    static void handleCreate(AsyncWebServerRequest* request,
                             uint8_t* data, size_t len,
                             NodeOrchestrator& orchestrator);

    // PUT /api/v1/shows/{id}
    static void handleUpdate(AsyncWebServerRequest* request,
                            const String& showId,
                            uint8_t* data, size_t len,
                            NodeOrchestrator& orchestrator);

    // DELETE /api/v1/shows/{id}
    static void handleDelete(AsyncWebServerRequest* request,
                            const String& showId,
                            NodeOrchestrator& orchestrator);

    // GET /api/v1/shows/current
    static void handleCurrent(AsyncWebServerRequest* request,
                             NodeOrchestrator& orchestrator);

    // POST /api/v1/shows/control
    static void handleControl(AsyncWebServerRequest* request,
                             uint8_t* data, size_t len,
                             NodeOrchestrator& orchestrator);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
```

**Route Registration:**
Add to `firmware/v2/src/network/webserver/V1ApiRoutes.cpp`:
```cpp
// Show routes
registry.onGet("/api/v1/shows", [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
    if (!checkRateLimit(request)) return;
    if (!checkAPIKey(request)) return;
    handlers::ShowHandlers::handleList(request, ctx.orchestrator);
});

registry.onGet("/api/v1/shows/current", [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
    if (!checkRateLimit(request)) return;
    if (!checkAPIKey(request)) return;
    handlers::ShowHandlers::handleCurrent(request, ctx.orchestrator);
});

// ... additional routes
```

### 4.4 Phase 4: WebSocket Commands

**Files to Create:**
- `firmware/v2/src/network/webserver/ws/WsShowCommands.cpp`

**Command Handlers:**
```cpp
static void handleShowList(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx);
static void handleShowGet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx);
static void handleShowSave(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx);
static void handleShowDelete(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx);
static void handleShowControl(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx);
static void handleShowState(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx);
```

**Registration:**
```cpp
// In WsShowCommands.cpp constructor or init function
WsCommandRouter::registerCommand("show.list", handleShowList);
WsCommandRouter::registerCommand("show.get", handleShowGet);
WsCommandRouter::registerCommand("show.save", handleShowSave);
WsCommandRouter::registerCommand("show.delete", handleShowDelete);
WsCommandRouter::registerCommand("show.control", handleShowControl);
WsCommandRouter::registerCommand("show.state", handleShowState);
```

### 4.5 Phase 5: WebSocket Event Broadcasting

**Files to Modify:**
- `firmware/v2/src/core/actors/ShowNode.cpp`

**Add Event Publishing:**
```cpp
void ShowNode::publishShowEvent(MessageType type, uint8_t showId, ...) {
    // Existing implementation publishes to MessageBus
    
    // NEW: Also broadcast via WebSocket
    if (m_wsBroadcastCallback) {
        m_wsBroadcastCallback(type, showId, ...);
    }
}
```

**Integration Point:**
- Add WebSocket broadcast callback to `ShowNode`
- Set callback from `WebServer` during initialization
- Broadcast events: `show.started`, `show.stopped`, `show.paused`, `show.resumed`, `show.chapterChanged`, `show.progress`

**Progress Updates:**
- Use `onTick()` (20Hz) to check if progress update needed
- Throttle to 10Hz (every 2 ticks = 100ms)
- Only broadcast if progress changed significantly (>1%)

### 4.6 Phase 6: Dashboard Integration

**Files to Modify:**
- `lightwave-dashboard/src/components/tabs/ShowsTab.tsx`
- `lightwave-dashboard/src/hooks/useShows.ts` (new)
- `lightwave-dashboard/src/services/showService.ts` (new)

**New Hook: `useShows`**
```typescript
export function useShows() {
  const { ws, http } = useV2();
  const [shows, setShows] = useState<Show[]>([]);
  const [currentShow, setCurrentShow] = useState<ShowState | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // Load shows from firmware
  const loadShows = useCallback(async () => {
    setLoading(true);
    try {
      const response = await http.get('/api/v1/shows');
      setShows(response.data.builtin.concat(response.data.custom));
    } catch (err) {
      setError(err.message);
    } finally {
      setLoading(false);
    }
  }, [http]);

  // Save show to firmware
  const saveShow = useCallback(async (show: Show) => {
    try {
      const response = await http.post('/api/v1/shows', show);
      return response.data;
    } catch (err) {
      setError(err.message);
      throw err;
    }
  }, [http]);

  // Control playback
  const controlShow = useCallback(async (action: string, showId?: number, timeMs?: number) => {
    try {
      await http.post('/api/v1/shows/control', { action, showId, timeMs });
    } catch (err) {
      setError(err.message);
      throw err;
    }
  }, [http]);

  // WebSocket event handlers
  useEffect(() => {
    if (!ws) return;

    const handlers = {
      'show.started': (data: any) => {
        setCurrentShow(prev => ({ ...prev, isPlaying: true, isPaused: false }));
      },
      'show.stopped': (data: any) => {
        setCurrentShow(null);
      },
      'show.progress': (data: any) => {
        setCurrentShow(prev => prev ? { ...prev, progress: data.progress } : null);
      },
      // ... other handlers
    };

    Object.entries(handlers).forEach(([type, handler]) => {
      ws.on(type, handler);
    });

    return () => {
      Object.keys(handlers).forEach(type => {
        ws.off(type);
      });
    };
  }, [ws]);

  return {
    shows,
    currentShow,
    loading,
    error,
    loadShows,
    saveShow,
    controlShow,
  };
}
```

**Update ShowsTab.tsx:**
- Replace local state with `useShows()` hook
- Add save/load functionality
- Connect transport controls to firmware
- Sync playhead with firmware progress
- Handle WebSocket events for real-time updates

---

## 5. Data Translation Details

### 5.1 Scene → Cue Conversion Algorithm

```cpp
bool ShowTranslator::scenesToCues(
    const TimelineScene* scenes,
    uint8_t sceneCount,
    uint32_t totalDurationMs,
    ShowCue* outCues,
    uint8_t& outCueCount,
    uint8_t maxCues
) {
    outCueCount = 0;
    
    // Sort scenes by start time
    TimelineScene sorted[sceneCount];
    memcpy(sorted, scenes, sizeof(TimelineScene) * sceneCount);
    qsort(sorted, sceneCount, sizeof(TimelineScene), compareScenesByTime);
    
    // Convert each scene
    for (uint8_t i = 0; i < sceneCount && outCueCount < maxCues - 1; i++) {
        const TimelineScene& scene = sorted[i];
        
        // Calculate absolute times
        uint32_t startMs = percentToMs(scene.startTimePercent, totalDurationMs);
        uint32_t endMs = startMs + percentToMs(scene.durationPercent, totalDurationMs);
        
        // Convert zone
        uint8_t firmwareZone = uiZoneToFirmware(scene.zoneId);
        
        // Get effect ID from name (via PatternRegistry)
        uint8_t effectId = getEffectIdByName(scene.effectName);
        if (effectId == 0xFF) continue; // Invalid effect
        
        // Create effect cue at scene start
        ShowCue& cue = outCues[outCueCount++];
        cue.timeMs = startMs;
        cue.type = CUE_EFFECT;
        cue.targetZone = firmwareZone;
        cue.data[0] = effectId;
        cue.data[1] = 0; // Default transition
        cue.data[2] = 0;
        cue.data[3] = 0;
        
        // Optional: Add marker cue at scene end
        if (outCueCount < maxCues) {
            ShowCue& marker = outCues[outCueCount++];
            marker.timeMs = endMs;
            marker.type = CUE_MARKER;
            marker.targetZone = firmwareZone;
            // Marker data unused
        }
    }
    
    // Sort cues by time
    qsort(outCues, outCueCount, sizeof(ShowCue), compareCuesByTime);
    
    return outCueCount > 0;
}
```

### 5.2 Cue → Scene Conversion Algorithm

```cpp
bool ShowTranslator::cuesToScenes(
    const ShowCue* cues,
    uint8_t cueCount,
    uint32_t totalDurationMs,
    TimelineScene* outScenes,
    uint8_t& outSceneCount,
    uint8_t maxScenes
) {
    outSceneCount = 0;
    
    // Group consecutive CUE_EFFECT cues with same effect and zone
    uint8_t i = 0;
    while (i < cueCount && outSceneCount < maxScenes) {
        // Find next CUE_EFFECT
        while (i < cueCount && cues[i].type != CUE_EFFECT) {
            i++;
        }
        if (i >= cueCount) break;
        
        const ShowCue& startCue = cues[i];
        uint8_t effectId = startCue.effectId();
        uint8_t zone = startCue.targetZone;
        uint32_t sceneStartMs = startCue.timeMs;
        
        // Find scene end (next CUE_EFFECT with different effect/zone, or CUE_MARKER, or show end)
        uint32_t sceneEndMs = totalDurationMs;
        uint8_t j = i + 1;
        while (j < cueCount) {
            if (cues[j].type == CUE_EFFECT) {
                if (cues[j].effectId() != effectId || cues[j].targetZone != zone) {
                    sceneEndMs = cues[j].timeMs;
                    break;
                }
            } else if (cues[j].type == CUE_MARKER && cues[j].targetZone == zone) {
                sceneEndMs = cues[j].timeMs;
                break;
            }
            j++;
        }
        
        // Create scene
        TimelineScene& scene = outScenes[outSceneCount++];
        scene.id = generateSceneId(outSceneCount - 1);
        scene.zoneId = firmwareZoneToUi(zone);
        scene.effectName = getEffectNameById(effectId);
        scene.startTimePercent = msToPercent(sceneStartMs, totalDurationMs);
        scene.durationPercent = msToPercent(sceneEndMs - sceneStartMs, totalDurationMs);
        scene.accentColor = getZoneColor(scene.zoneId);
        
        i = j;
    }
    
    return outSceneCount > 0;
}
```

### 5.3 Edge Cases

1. **Overlapping Scenes:**
   - UI allows overlapping scenes on different zones
   - Firmware executes cues in time order
   - Solution: Preserve all cues, let firmware handle execution order

2. **Gaps Between Scenes:**
   - UI may have gaps (no scene at certain time)
   - Firmware needs explicit effect at all times
   - Solution: Add default effect cue (effect 0 or last effect) to fill gaps

3. **Scene Boundaries:**
   - UI scenes are continuous blocks
   - Firmware cues are discrete events
   - Solution: Use CUE_MARKER to mark scene boundaries, or infer from next CUE_EFFECT

4. **Effect Name Resolution:**
   - UI uses effect names (strings)
   - Firmware uses effect IDs (uint8_t)
   - Solution: Use PatternRegistry to map names ↔ IDs

---

## 6. Testing Strategy

### 6.1 Unit Tests

**Translation Layer:**
- Test scene → cue conversion with various scenarios
- Test cue → scene conversion with built-in shows
- Test time conversion accuracy
- Test zone mapping correctness
- Test edge cases (overlaps, gaps, boundaries)

**Storage Layer:**
- Test save/load cycle
- Test storage limits
- Test deletion
- Test corruption recovery

### 6.2 Integration Tests

**API Endpoints:**
- Test all REST endpoints with valid/invalid inputs
- Test authentication and rate limiting
- Test error handling

**WebSocket:**
- Test all commands
- Test event broadcasting
- Test multiple clients
- Test reconnection handling

### 6.3 End-to-End Tests

**Full Workflow:**
1. Create show in UI
2. Save to firmware
3. Load from firmware
4. Start playback
5. Verify real-time updates
6. Pause/resume
7. Seek to position
8. Stop show
9. Delete show

**Edge Cases:**
- Large shows (many scenes)
- Long duration shows
- Rapid start/stop cycles
- Network interruptions
- Concurrent edits

### 6.4 Performance Tests

**Memory:**
- Measure RAM usage for custom shows
- Test storage limits
- Monitor heap fragmentation

**CPU:**
- Measure translation time
- Test cue execution overhead
- Monitor WebSocket broadcast overhead

**Network:**
- Test API response times
- Test WebSocket event frequency
- Test large show payloads

---

## 7. Migration & Backward Compatibility

### 7.1 Built-in Shows

- Built-in shows remain unchanged (PROGMEM)
- Accessible via API with `type: "builtin"`
- Read-only (cannot be modified/deleted)
- ID range: 0-9

### 7.2 Custom Shows

- Custom shows stored in NVS
- ID format: `"show-{uuid}"` or `"show-{timestamp}"`
- Accessible via API with `type: "custom"`
- Full CRUD operations available

### 7.3 API Versioning

- All endpoints under `/api/v1/shows`
- Maintain backward compatibility with existing API
- Future versions: `/api/v2/shows` if breaking changes needed

### 7.4 Dashboard Compatibility

- Dashboard continues to work with local state if firmware unavailable
- Graceful degradation: show UI even if API fails
- Offline mode: allow editing, queue saves for when connected

---

## 8. Security Considerations

### 8.1 Authentication

- All show endpoints require API key authentication
- WebSocket commands require authentication
- Rate limiting on all endpoints

### 8.2 Input Validation

- Validate show duration (min: 1s, max: 3600s)
- Validate scene count (max: 50 scenes per show)
- Validate time ranges (0-100% for percentages)
- Validate zone IDs (0-4 for UI, 0-3 or 0xFF for firmware)
- Validate effect IDs (must exist in PatternRegistry)

### 8.3 Storage Limits

- Maximum custom shows: 10
- Maximum scenes per show: 50
- Maximum show duration: 3600 seconds (1 hour)
- Maximum total storage: ~64KB NVS space

### 8.4 Resource Protection

- Prevent show deletion if currently playing
- Prevent show modification if currently playing (or require confirmation)
- Limit concurrent show operations

---

## 9. Performance Considerations

### 9.1 Memory Usage

**RAM:**
- ShowTranslator: ~1KB (temporary during conversion)
- ShowStorage: ~2KB (NVS read buffer)
- Active show state: 20 bytes (existing ShowPlaybackState)

**Flash:**
- Built-in shows: ~2KB (existing PROGMEM)
- Custom shows: ~6KB per show (NVS, compressed)
- Total custom shows: ~60KB (10 shows × 6KB)

### 9.2 CPU Usage

**Translation:**
- Scene → Cue: O(n log n) for sorting, O(n) for conversion
- Cue → Scene: O(n) for single pass
- Typical show (10 scenes): <1ms conversion time

**Storage:**
- Save: O(n) serialization + NVS write (~10ms)
- Load: NVS read + O(n) deserialization (~5ms)

**WebSocket:**
- Progress updates: 10Hz = 10 messages/second
- Event broadcasts: <1 message/second average
- Total overhead: <1% CPU

### 9.3 Network Usage

**REST API:**
- List shows: ~2KB response
- Get show (scenes): ~5-10KB per show
- Get show (cues): ~2-5KB per show
- Save show: ~5-10KB request

**WebSocket:**
- Progress updates: ~100 bytes per message
- Events: ~50-200 bytes per message
- Total: ~1-2KB/second during playback

---

## 10. Implementation Phases

### Phase 1: Foundation (Week 1)
- [ ] Create ShowTranslator class
- [ ] Implement scene ↔ cue conversion
- [ ] Unit tests for translation
- [ ] Integration with PatternRegistry

### Phase 2: Storage (Week 1-2)
- [ ] Create ShowStorage class
- [ ] Implement NVS save/load
- [ ] Storage limits and management
- [ ] Unit tests for storage

### Phase 3: REST API (Week 2)
- [ ] Create ShowHandlers
- [ ] Implement all REST endpoints
- [ ] Route registration
- [ ] API tests

### Phase 4: WebSocket (Week 2-3)
- [ ] Create WsShowCommands
- [ ] Implement all WebSocket commands
- [ ] Event broadcasting from ShowNode
- [ ] WebSocket tests

### Phase 5: Dashboard Integration (Week 3)
- [ ] Create useShows hook
- [ ] Update ShowsTab.tsx
- [ ] Connect transport controls
- [ ] Real-time progress sync
- [ ] Error handling and loading states

### Phase 6: Testing & Polish (Week 4)
- [ ] End-to-end testing
- [ ] Performance optimization
- [ ] Documentation
- [ ] Bug fixes

---

## 11. Known Limitations & Future Enhancements

### 11.1 Current Limitations

1. **Storage Capacity:**
   - Limited to 10 custom shows
   - ~6KB per show (NVS constraints)
   - No show export/import

2. **Scene Complexity:**
   - Simple effect changes only
   - No parameter sweeps in UI (yet)
   - No transition types in UI (yet)

3. **Chapters:**
   - UI doesn't support chapters
   - Chapters only in built-in shows
   - Future: Add chapter editing to UI

4. **Real-time Sync:**
   - Progress updates at 10Hz (may feel laggy)
   - No frame-perfect sync
   - Network latency affects responsiveness

### 11.2 Future Enhancements

1. **Advanced Scene Editing:**
   - Parameter sweeps in UI
   - Transition types
   - Effect blending modes

2. **Chapter Support:**
   - Chapter editing in UI
   - Narrative phase visualization
   - Tension level controls

3. **Show Templates:**
   - Save show as template
   - Clone and modify shows
   - Share shows between devices

4. **Audio Sync:**
   - Sync shows to audio BPM
   - Beat-aligned cues
   - Audio-reactive scenes

5. **Multi-Device:**
   - Sync shows across devices
   - Master/slave show playback
   - Distributed show execution

---

## 12. Documentation Requirements

### 12.1 API Documentation

- OpenAPI spec updates
- Endpoint documentation
- Request/response examples
- Error codes

### 12.2 Developer Documentation

- Translation algorithm details
- Storage format specification
- Extension points for custom cue types
- Performance tuning guide

### 12.3 User Documentation

- Show creation tutorial
- Timeline editor guide
- Best practices
- Troubleshooting

---

## 13. Success Criteria

### 13.1 Functional Requirements

- ✅ Create custom shows via UI
- ✅ Save shows to firmware
- ✅ Load shows from firmware
- ✅ Play shows from UI
- ✅ Real-time progress updates
- ✅ Transport controls (play, pause, stop, seek)
- ✅ Delete custom shows
- ✅ List all shows (builtin + custom)

### 13.2 Performance Requirements

- Translation: <10ms for 20 scenes
- API response: <100ms for all endpoints
- WebSocket latency: <50ms for events
- Storage: <20ms save/load operations

### 13.3 Quality Requirements

- Zero memory leaks
- Graceful error handling
- Backward compatibility maintained
- Comprehensive test coverage (>80%)

---

## 14. Risk Assessment

### 14.1 Technical Risks

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| NVS storage full | Medium | High | Implement storage management, LRU eviction |
| Translation errors | Low | Medium | Comprehensive unit tests, validation |
| Memory fragmentation | Low | High | Use fixed-size buffers, avoid dynamic allocation |
| WebSocket overload | Low | Medium | Throttle progress updates, limit clients |
| Show corruption | Low | High | Validation on load, checksums |

### 14.2 Schedule Risks

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Translation complexity | Medium | Medium | Start with simple cases, iterate |
| Storage format changes | Low | Low | Version storage format |
| Dashboard integration | Medium | High | Parallel development, mock API |

---

## 15. Dependencies

### 15.1 Firmware Dependencies

- `ShowNode` (existing)
- `PatternRegistry` (for effect ID ↔ name mapping)
- `NodeOrchestrator` (for ShowNode access)
- `NVSManager` (for storage)
- `WebServer` (for API/WebSocket)

### 15.2 Dashboard Dependencies

- `useV2` hook (for WebSocket/HTTP)
- `ShowsTab.tsx` (existing UI)
- `timeline.ts` types (existing)

### 15.3 External Dependencies

- ArduinoJson (firmware)
- ESPAsyncWebServer (firmware)
- React (dashboard)
- TypeScript (dashboard)

---

## 16. Conclusion

This plan provides a comprehensive roadmap for bridging the firmware's cue-based show system with the dashboard's scene-based timeline editor. The implementation is divided into 6 phases over 4 weeks, with clear milestones and success criteria.

**Key Design Decisions:**
1. **Hybrid Storage:** Built-in shows in PROGMEM, custom shows in NVS
2. **Translation Layer:** Bidirectional conversion between scenes and cues
3. **Dual API:** REST for CRUD, WebSocket for real-time control
4. **Progressive Enhancement:** UI works offline, syncs when connected

**Next Steps:**
1. Review and approve this plan
2. Begin Phase 1 implementation
3. Set up testing infrastructure
4. Create tracking issues/tasks

---

**Document Version:** 1.0  
**Author:** AI Assistant  
**Reviewers:** [Pending]  
**Approval:** [Pending]

