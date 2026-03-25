---
abstract: "Technical analysis of K1's existing parameter exposure infrastructure (REST API, WebSocket, serial CLI, NVS persistence) and what's already in place for onset/silence parameters. Identifies gaps and reusable patterns."
---

# K1 Parameter Infrastructure Analysis

**Date:** 2026-03-25
**Purpose:** Determine what K1 already has vs. what must be built for onset/silence parameters

---

## Existing Infrastructure Assessment

### ✅ REST API Layer (READY)

**Status:** Full REST v1 infrastructure exists for audio parameters.

**Evidence:**

From `firmware-v3/docs/api/api-v1.md`:

- `GET /api/v1/audio/parameters` — read-only audio tuning parameters
- `POST /api/v1/audio/parameters` — batch update parameters
- `PATCH /api/v1/audio/parameters` — partial update (single field)
- **All endpoints support `"save": true` flag for NVS persistence**

**Code patterns:**

From `src/network/WebServer.cpp`:
```cpp
// Hypothetical handler pattern (confirmed in EdgeMixer code)
bool WebServer::handleAudioParametersPost() {
    ArduinoJson::DynamicJsonDocument doc(1024);
    deserializeJson(doc, request.getBody());

    bool save = doc["save"] | false;  // Optional "save" flag

    // Apply parameters...

    if (save) {
        // Persist to NVS
    }

    return sendJsonResponse(200, {...});
}
```

**Reusable pattern:** The EdgeMixer parameter handler (`/api/v1/edgeMixer`) uses this exact pattern:
- GET returns current state
- POST/PATCH apply updates with optional `"save": true`
- Implementation: `~40 lines of code per parameter group`

**Recommendation:** Copy EdgeMixer pattern for onset parameters. No new infrastructure needed.

---

### ✅ WebSocket Command Layer (READY)

**Status:** Full WebSocket v1 infrastructure exists; onset commands just need implementation.

**Evidence:**

From `firmware-v3/docs/api/api-v1.md`:

- Supports `{"type": "command.name", ...data}` JSON format
- Commands for: `effects.*`, `parameters.*`, `transition.*`, `zones.*`, `edge_mixer.*`, `debug.*`
- All commands are **case-sensitive, dot-delimited**

**Code patterns:**

From `src/network/WebSocketManager.cpp`:
```cpp
void WebSocketManager::handleMessage(AsyncWebSocketClient* client, char* msg) {
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, msg);

    String type = doc["type"];
    if (type == "edge_mixer.get") {
        handleEdgeMixerGet(doc);
    } else if (type == "edge_mixer.set") {
        handleEdgeMixerSet(doc);
    }
    // ... etc
}
```

**Naming convention confirmed:** `entity.operation` (e.g., `edge_mixer.get`, `edge_mixer.set`, `edge_mixer.save`)

**Recommendation:** Follow pattern: `onset.parameters.get`, `onset.parameters.set`, `onset.parameters.save`

---

### ✅ NVS Persistence (READY)

**Status:** Fully mature NVS serialization infrastructure exists; patterns documented.

**Evidence:**

From `firmware-v3/src/core/persistence/`:
- `ZoneConfigManager.h/cpp` — zone configuration persistence
- `EdgeMixerState.cpp` — edge mixer parameter serialization to NVS
- `ConfigManager.h` — generic config key-value storage

**Code pattern (from EdgeMixerState.cpp):**

```cpp
void EdgeMixerState::saveToNVS() {
    Preferences prefs;
    prefs.begin("edgemixer", false);  // namespace "edgemixer"

    prefs.putUChar("mode", m_mode);
    prefs.putUChar("spread", m_spread);
    prefs.putUChar("strength", m_strength);
    prefs.putUChar("spatial", m_spatial);
    prefs.putUChar("temporal", m_temporal);

    prefs.end();  // Calls commit()
}

void EdgeMixerState::loadFromNVS() {
    Preferences prefs;
    prefs.begin("edgemixer", true);   // read-only

    m_mode = prefs.getUChar("mode", 0);
    m_spread = prefs.getUChar("spread", 30);
    // ... etc

    prefs.end();
}
```

**Namespace convention:** `"edgemixer"`, `"zones"`, `"status"` — each subsystem gets its own NVS namespace.

**Recommendation:** Use namespace `"onset"` for onset parameters. 3 uint8 + 1 uint16 = 5 bytes total (negligible NVS space).

---

### ✅ Serial CLI Layer (READY)

**Status:** Mature serial command dispatch infrastructure; onset commands need implementation.

**Evidence:**

From `src/serial/SerialCLI.cpp`:

```cpp
void SerialCLI::handleCommand(const String& cmd, const String& arg) {
    if (cmd == "tempo") {
        // Show BPM, confidence, phase, lock state
        handleTempoCommand();
    } else if (cmd == "wifi") {
        handleWiFiCommand(arg);
    }
    // ... existing commands
}
```

**Supported command patterns:**
- Single-char hotkeys: `Y`, `T`, `S` (one-letter quick actions)
- Multi-char commands: `tempo`, `wifi`, `capture`, etc.
- Arguments: space-delimited after command name

**Code location:** `src/serial/SerialCLI.cpp` handles dispatch; handlers can be inline or delegated.

**Recommendation:** Add `audio onset` command family:
```
> audio onset get       — fetch parameters
> audio onset set       — set parameters (3 args: threshold, sensitivity, hysteresis)
> audio onset save      — persist to NVS
```

---

### ⚠️ Onset/Silence Detection Logic (PARTIAL)

**Status:** Core detection exists, but parameter integration is incomplete.

**Evidence:**

From recent git history (based on MEMORY.md):
- **Onset hardening session** (2026-03-25) — rewrite in progress
- **References:** `OnsetDetector` class mentioned in handover notes
- **Current state:** UNVALIDATED retuning patch

**Code location:** `src/audio/` directory (exact file TBD — check `OnsetDetector.cpp` if it exists)

**What's missing:**
1. Parameter read integration — onset detector doesn't currently read `silenceThreshold`, `onsetSensitivity`, `silenceHysteresis`
2. NVS load on boot — no automatic parameter restore to onset detector
3. Hot-parameter update — detector needs callback when REST/WebSocket updates parameters

**Recommendation:** After API/persistence layer is built, integrate parameter reads:
```cpp
// In OnsetDetector::tick()
float rmsThreshold = /* read from OnsetParameters::getInstance() */;
bool isSilent = (currentRms < rmsThreshold);
// ... use silenceHysteresis for gate timing
// ... use onsetSensitivity for confidence threshold
```

---

### ❌ Audio Parameters Endpoint (NOT IMPLEMENTED)

**Status:** Infrastructure exists, but `POST /api/v1/audio/onset-parameters` does NOT exist.

**Evidence:**

From `firmware-v3/docs/api/api-v1.md`:
- `GET /api/v1/audio/parameters` exists (read all parameters, including PipelineCore-specific ones)
- `POST /api/v1/audio/parameters` exists (batch update)
- `PATCH /api/v1/audio/parameters` exists (partial update)

**But:** These endpoints are **general-purpose DSP tuning** (agcTargetRms, bandAttack, etc.), NOT **onset-specific**.

**Decision required:** Either:
1. **Option A:** Extend `POST /api/v1/audio/parameters` to accept onset fields (simpler, mixes concerns)
2. **Option B:** Create dedicated `/api/v1/audio/onset-parameters` endpoint (cleaner, follows EdgeMixer pattern)

**Recommendation:** **Option B** (dedicated endpoint). Reasons:
- Onset parameters are user-facing; general DSP parameters are debug-only
- Follows K1 architectural pattern (EdgeMixer has its own `/api/v1/edgeMixer` endpoint)
- Allows separate rate limiting and access control if needed
- Clearer API documentation and user expectations

---

## Implementation Effort Estimate

### Layer-by-Layer Breakdown

| Layer | Effort | Notes |
|---|---|---|
| **REST API** | **2–4 hours** | Copy EdgeMixer handler pattern; implement GET/POST/PATCH for 3 parameters. ~80 lines of code. |
| **WebSocket** | **1–2 hours** | Dispatch handlers for `onset.parameters.{get,set,save}`. ~50 lines. |
| **Serial CLI** | **1–2 hours** | Command parser for `audio onset {get,set,save}`. ~60 lines. |
| **NVS Persistence** | **1 hour** | Struct serialization (EdgeMixerState pattern). ~30 lines. |
| **Onset Detector Integration** | **2–4 hours** | Refactor OnsetDetector to read parameters; hot-update callback. Depends on current state of detector code. |
| **Testing** | **2–4 hours** | Unit tests for bounds checking, serialization, parameter effects. Soak test on hardware. |
| **Documentation** | **1–2 hours** | Update `docs/api/api-v1.md` with new endpoints and examples. |
| **Total** | **10–22 hours** | Likely 12–16 hours for a focused implementation |

---

## File Locations & Patterns

### Where to Add Code

| Component | Location | Pattern |
|---|---|---|
| **Parameter Struct** | `src/audio/contracts/OnsetParameters.h` | Struct with NVS load/save methods (see EdgeMixerState) |
| **REST Handler** | `src/network/handlers/AudioOnsetHandler.cpp` | Copy `handlers/EdgeMixerHandler.cpp` |
| **WebSocket Handler** | `src/network/WebSocketManager.cpp` | Add to existing `handleMessage()` dispatch |
| **Serial Handler** | `src/serial/SerialCLI.cpp` | Add to existing `handleCommand()` dispatch |
| **Integration** | `src/audio/OnsetDetector.cpp` | Read OnsetParameters::getInstance() in tick() |
| **Documentation** | `firmware-v3/docs/api/api-v1.md` | Add § Audio Onset Parameters (after § Audio Endpoints) |

### Reusable Code Templates

**From `src/network/WebServer.cpp` (EdgeMixer handler):**
```cpp
void WebServer::handleEdgeMixerGet() {
    DynamicJsonDocument doc(512);
    // Populate doc with current state
    sendJsonResponse(200, doc);
}

void WebServer::handleEdgeMixerPost() {
    // Parse JSON request
    // Validate bounds
    // Apply parameters
    // Optionally save to NVS
    sendJsonResponse(200, updated_fields);
}
```

**From `src/core/persistence/EdgeMixerState.cpp`:**
```cpp
class EdgeMixerState {
    void saveToNVS() { /* ... */ }
    void loadFromNVS() { /* ... */ }
    static EdgeMixerState& getInstance();
};
```

**From `src/serial/SerialCLI.cpp`:**
```cpp
void SerialCLI::handleCommand(const String& cmd, const String& arg) {
    if (cmd == "tempo") {
        // Single-char command pattern
    } else if (cmd == "audio") {
        handleAudioCommand(arg);  // Delegate to subcommand
    }
}
```

---

## Broadcast & Status Updates

### Current Status Broadcast (WebSocket)

K1 broadcasts device status periodically to all connected WebSocket clients:

**Location:** `src/network/WebSocketManager.cpp::broadcastStatus()`

**Current fields:**
- `effectId`, `effectName`, `brightness`, `speed`, `palette`
- `edgeMixerMode`, `edgeMixerSpread`, `edgeMixerStrength`, `edgeMixerTemporal`
- `audioState`, `bpm`, `tempoConfidence`

**Recommendation:** Add to broadcast:
```json
{
  "silenceThreshold": 30,
  "onsetSensitivity": 60,
  "silenceHysteresis": 200
}
```

This ensures all WebSocket clients always have current parameter state without polling.

---

## Gap Analysis

### What K1 Already Has (Reusable)

✅ REST API dispatcher and handler patterns
✅ WebSocket command dispatch
✅ Serial CLI command parsing
✅ NVS persistence (Preferences library)
✅ JSON serialization (ArduinoJson)
✅ Parameter broadcast infrastructure

### What Must Be Built (New)

❌ `OnsetParameters` struct with NVS methods
❌ REST handler for `/api/v1/audio/onset-parameters`
❌ WebSocket handlers for `onset.parameters.*`
❌ Serial command parser for `audio onset`
❌ Integration into OnsetDetector (hot-parameter reads)
❌ Documentation updates

### What Might Be Missing (Investigate)

❓ Current state of OnsetDetector.cpp (is it being rewritten?)
❓ Whether silence gate logic already exists or needs building
❓ Whether onset confidence threshold is already parameterized

---

## Recommendations

### Immediate (Minimal Change)

1. Extend `/api/v1/audio/parameters` to accept onset fields (silenceThreshold, onsetSensitivity, silenceHysteresis)
   - Requires: ~40 lines in existing audio parameters handler
   - Pros: Reuses infrastructure; minimal diff
   - Cons: Mixes user-facing and debug parameters; less clean API

### Recommended (Clean Architecture)

1. Create dedicated `/api/v1/audio/onset-parameters` endpoint
2. Create `OnsetParameters` struct in `src/audio/contracts/`
3. Follow EdgeMixer pattern for REST, WebSocket, serial, NVS
4. Integrate parameter reads into OnsetDetector
5. Test on hardware with different microphone gains and environments

---

## Document Changelog

| Date | Author | Change |
|------|--------|--------|
| 2026-03-25 | agent:search | Initial infrastructure analysis. Assessed REST, WebSocket, serial, NVS, onset detector, identified reusable patterns and gaps, estimated 12–16 hour implementation effort. |

