# Execution Plan: Protocol Contract + LVGL Reference + Bug Fixes

## Dependency Graph

```
Phase 1: Protocol Contract (YAML)        Phase 2: LVGL Reference (MD)
  ├─ 1A: Extract K1 WS commands            ├─ 2A: Write lvgl-component-reference.md
  ├─ 1B: Extract K1 REST routes            └─ (no dependencies)
  ├─ 1C: Extract K1 broadcasts
  ├─ 1D: Extract Tab5 consumers
  ├─ 1E: Write k1-ws-contract.yaml
  └─ 1F: Write k1-rest-contract.yaml
          │
          ▼
Phase 3: Bug Fixes (CODE)                Phase 4: Integration (DOCS)
  ├─ 3A: Fix WsMessageRouter names          ├─ 4A: Update CLAUDE.md (root)
  ├─ 3B: Remove dead WS commands            ├─ 4B: Update tab5-encoder/CLAUDE.md
  └─ 3C: Build + flash Tab5                 └─ 4C: Commit all + update CONCERNS.md

Parallelism:
  - Phase 1 (1A-1D) and Phase 2 run in PARALLEL (independent)
  - Phase 1E/1F depend on 1A-1D completing
  - Phase 3 depends on Phase 1E (contract informs the fix)
  - Phase 4 depends on all prior phases
```

## Phase 1: Protocol Contract

### 1A: Extract K1 WebSocket Commands

**READ (25 files):**
```
firmware-v3/src/network/webserver/ws/WsEffectsCommands.cpp
firmware-v3/src/network/webserver/ws/WsParameterCommands.cpp
firmware-v3/src/network/webserver/ws/WsZoneCommands.cpp
firmware-v3/src/network/webserver/ws/WsAudioCommands.cpp
firmware-v3/src/network/webserver/ws/WsColorCommands.cpp
firmware-v3/src/network/webserver/ws/WsEdgeMixerCommands.cpp
firmware-v3/src/network/webserver/ws/WsTransitionCommands.cpp
firmware-v3/src/network/webserver/ws/WsShowCommands.cpp
firmware-v3/src/network/webserver/ws/WsEffectPresetCommands.cpp
firmware-v3/src/network/webserver/ws/WsOtaCommands.cpp
firmware-v3/src/network/webserver/ws/WsStreamCommands.cpp
firmware-v3/src/network/webserver/ws/WsBatchCommands.cpp
firmware-v3/src/network/webserver/ws/WsDeviceCommands.cpp
firmware-v3/src/network/webserver/ws/WsStimulusCommands.cpp
firmware-v3/src/network/webserver/ws/WsNarrativeCommands.cpp
firmware-v3/src/network/webserver/ws/WsTrinityCommands.cpp
firmware-v3/src/network/webserver/ws/WsMotionCommands.cpp
firmware-v3/src/network/webserver/ws/WsModifierCommands.cpp
firmware-v3/src/network/webserver/ws/WsZonePresetCommands.cpp
firmware-v3/src/network/webserver/ws/WsPluginCommands.cpp
firmware-v3/src/network/webserver/ws/WsPaletteCommands.cpp
firmware-v3/src/network/webserver/ws/WsColorCorrectionCommands.cpp
firmware-v3/src/network/webserver/ws/WsControlLeaseCommands.cpp
firmware-v3/src/network/webserver/ws/WsCapabilitiesCommands.cpp
firmware-v3/src/network/webserver/ws/WsNetworkCommands.cpp
```

**Extract:** Every `registerCommand("name", ...)` call → command name, handler signature, request fields, response type.

**Method:** `grep -n 'registerCommand' firmware-v3/src/network/webserver/ws/*.cpp` to get names, then read each handler to extract field types.

### 1B: Extract K1 REST Routes

**READ (1 file):**
```
firmware-v3/src/network/webserver/V1ApiRoutes.cpp
```

**Extract:** Every `server->on(...)` route registration → method, path, handler, request/response fields.

**Method:** `grep -n 'server->on' V1ApiRoutes.cpp` for routes, then cross-reference with `firmware-v3/docs/api/api-v1.md` for existing documentation.

### 1C: Extract K1 Broadcasts

**READ (2 files):**
```
firmware-v3/src/network/WebServerBroadcast.cpp
firmware-v3/src/network/ApiResponse.h
```

**Extract:** Every broadcast type string, trigger condition, field names.

### 1D: Extract Tab5 Consumers

**READ (3 files):**
```
tab5-encoder/src/network/WebSocketClient.cpp   — outbound commands (30)
tab5-encoder/src/network/WsMessageRouter.h     — inbound handlers (12)
tab5-encoder/src/network/HttpClient.cpp         — REST calls (8)
```

**Extract:** Every command Tab5 sends, every message type Tab5 handles, every REST endpoint Tab5 calls. Cross-reference with 1A/1B/1C to identify gaps and mismatches.

### 1E: Write k1-ws-contract.yaml

**CREATE:**
```
docs/protocol/k1-ws-contract.yaml
```

**Structure:**
```yaml
version: "1.0"
transport: "WebSocket JSON text frames"
endpoint: "ws://<k1-ip>:80/ws"

envelope:
  request: { type, requestId? }
  response: { type, success, data, requestId? }
  broadcast: { type, ... }

commands:
  <name>:
    direction: client -> K1 | K1 -> client | bidirectional
    description: string
    request: { field: { type, required, range? } }
    response: { type, data: { ... } } | null
    broadcast: { type, fields } | null
    consumers: [tab5, ios]
    known_issues: [] | null
    deprecated: bool | null

broadcasts:
  <name>:
    direction: K1 -> clients
    trigger: string
    fields: { ... }
    consumers: [tab5, ios]
```

**Content:** All 141 WS commands + all broadcast types. Each command has direction, fields with types, response format, and consumer list.

### 1F: Write k1-rest-contract.yaml

**CREATE:**
```
docs/protocol/k1-rest-contract.yaml
```

**Structure:**
```yaml
version: "1.0"
base_url: "http://<k1-ip>/api/v1"

endpoints:
  <path>:
    method: GET | POST | PUT | DELETE
    description: string
    request: { params: {}, body: {} }
    response: { status, body: {} }
    consumers: [tab5, ios]
```

**Content:** All 93 REST routes with method, path, request/response shapes.

### 1G: Write docs/protocol/README.md

**CREATE:**
```
docs/protocol/README.md
```

**Content:** How to use the contracts, rules for updating them, future lint script plans.

---

## Phase 2: LVGL Component Reference

### 2A: Write lvgl-component-reference.md

**CREATE:**
```
tab5-encoder/docs/reference/lvgl-component-reference.md
```

**READ (for content — already analysed by research agent, but verify):**
```
tab5-encoder/src/ui/DisplayUI.h              — screen enum, widget pointers
tab5-encoder/src/ui/DisplayUI.cpp            — widget tree, styles, patterns
tab5-encoder/src/ui/ZoneComposerUI.h/cpp     — complex screen example
tab5-encoder/src/ui/ConnectivityTab.h/cpp    — network screen example
tab5-encoder/src/ui/ControlSurfaceUI.h/cpp   — cleanest screen (template source)
tab5-encoder/src/ui/lvgl_bridge.cpp          — display init, flush, touch
tab5-encoder/src/ui/Theme.h                  — legacy colours (anti-pattern reference)
tab5-encoder/src/ui/fonts/experimental_fonts.h — font macro inventory
tab5-encoder/src/ui/widgets/*.h              — legacy widgets (anti-pattern reference)
```

**Sections (~300 lines):**
1. Architecture Overview (display, render loop, thread model)
2. Screen Lifecycle Protocol (create once, never destroy)
3. Design System Colours (hex table, NOT Theme.h)
4. Font Table (family/weight/size/macro/usage)
5. Widget Tree Diagrams (GLOBAL, Zone, Connectivity, ControlSurface)
6. Layout Patterns (grid, flex, card factory, transparent container)
7. Event Handling (static callback, user_data, touch bubbling)
8. Anti-Patterns — 12 explicit MUST NOT rules
9. Copy-Paste Templates (6 templates for common operations)
10. Watchdog and Memory rules

---

## Phase 3: Bug Fixes

### 3A: Fix WsMessageRouter Broadcast Names

**MODIFY:**
```
tab5-encoder/src/network/WsMessageRouter.h
```

**Change:** Add handler registrations for K1's actual broadcast type names:
- `"effectChanged"` → route to existing effects changed handler
- `"zones.stateChanged"` → route to existing zones changed handler
- `"zones.enabledChanged"` → route to zones changed handler (or new handler)

**Injection point:** The `registerHandlers()` method or equivalent where type→handler mappings are defined.

**Risk:** LOW — adding new string matches to the router. Existing handlers stay.

### 3B: Remove Dead WebSocket Commands

**MODIFY:**
```
tab5-encoder/src/network/WebSocketClient.h    — remove 3 method declarations
tab5-encoder/src/network/WebSocketClient.cpp  — remove 3 method implementations
```

**Remove:**
- `sendColorCorrectionSetGamma()` (declaration + implementation)
- `sendColorCorrectionSetAutoExposure()` (declaration + implementation)
- `sendColorCorrectionSetBrownGuardrail()` (declaration + implementation)

**Verify no callers:** `grep -rn 'setGamma\|setAutoExposure\|setBrownGuardrail' tab5-encoder/src/`

**Risk:** LOW — if callers exist, compiler will catch them immediately.

### 3C: Build + Flash Tab5

**Command:**
```bash
cd tab5-encoder && pio run -e esp32p4
```

**Upload (if Tab5 connected):**
```bash
pio run -e esp32p4 -t upload --upload-port /dev/cu.usbmodem1101
```

**Verify:** Tab5 connects to K1 AP, WebSocket establishes, effect changes from K1 serial are reflected on Tab5 display.

---

## Phase 4: Integration

### 4A: Update Root CLAUDE.md

**MODIFY:**
```
CLAUDE.md  (project root)
```

**Add to Tool Enforcement section:**
```markdown
### Protocol Contract (MANDATORY for network code)
Before adding, modifying, or consuming any WebSocket command or REST
endpoint, read `docs/protocol/k1-ws-contract.yaml`. This is the single
source of truth. If a command is not in the contract, it does not exist.
Update contract FIRST, then implement.
```

### 4B: Update tab5-encoder/CLAUDE.md

**MODIFY:**
```
tab5-encoder/CLAUDE.md
```

**Add:**
```markdown
### LVGL Code (MANDATORY before touching src/ui/)
Read `docs/reference/lvgl-component-reference.md` FIRST. This documents
the exact widget tree, colour system, font assignments, layout patterns,
and 12 anti-patterns that cause visual bugs, memory leaks, and WDT panics.
```

### 4C: Update CONCERNS.md + Commit

**MODIFY:**
```
.planning/codebase/CONCERNS.md
```

**Mark resolved:** The naming mismatch and dead command issues found during protocol research.

**Commit all work as atomic commits per phase.**

---

## Execution Order (with parallelism)

```
WAVE 1 (parallel):
  ├─ Agent A: Phase 1 (protocol contract — reads 30 files, writes 3 YAML/MD)
  └─ Agent B: Phase 2 (LVGL reference — reads 10 files, writes 1 MD)

WAVE 2 (sequential, after Wave 1):
  └─ Phase 3: Bug fixes (3A + 3B + 3C) — 3 files modified, build + flash

WAVE 3 (sequential, after Wave 2):
  └─ Phase 4: Integration (4A + 4B + 4C) — 3 files modified, commit
```

**Estimated token cost:**
- Wave 1 Agent A: ~40K tokens (30 files to grep/read, YAML generation)
- Wave 1 Agent B: ~15K tokens (research already done, just writing)
- Wave 2: ~5K tokens (small targeted edits)
- Wave 3: ~3K tokens (CLAUDE.md edits)

**Total: ~63K tokens across 2 parallel agents + 2 sequential steps.**

---

## File Manifest

### Files CREATED (6):
```
docs/protocol/k1-ws-contract.yaml          — WebSocket protocol contract
docs/protocol/k1-rest-contract.yaml        — REST API protocol contract
docs/protocol/README.md                     — Usage instructions
tab5-encoder/docs/reference/lvgl-component-reference.md  — LVGL harness
```

### Files MODIFIED (6):
```
tab5-encoder/src/network/WsMessageRouter.h  — add broadcast name handlers
tab5-encoder/src/network/WebSocketClient.h   — remove 3 dead declarations
tab5-encoder/src/network/WebSocketClient.cpp — remove 3 dead implementations
CLAUDE.md                                    — add protocol contract mandate
tab5-encoder/CLAUDE.md                       — add LVGL reference mandate
.planning/codebase/CONCERNS.md              — mark issues resolved
```

### Files READ (for extraction, not modified) (~33):
```
firmware-v3/src/network/webserver/ws/Ws*Commands.cpp  (25 files)
firmware-v3/src/network/webserver/V1ApiRoutes.cpp
firmware-v3/src/network/WebServerBroadcast.cpp
firmware-v3/src/network/ApiResponse.h
tab5-encoder/src/network/WebSocketClient.cpp
tab5-encoder/src/network/WsMessageRouter.h
tab5-encoder/src/network/HttpClient.cpp
tab5-encoder/src/ui/DisplayUI.cpp (already analysed)
tab5-encoder/src/ui/fonts/experimental_fonts.h (already analysed)
```

---
*Plan created: 2026-03-21 after eng review (CLEARED)*
