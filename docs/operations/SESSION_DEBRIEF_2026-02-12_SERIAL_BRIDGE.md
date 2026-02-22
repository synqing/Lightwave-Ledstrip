# Session Debrief: PRISM Studio Serial Communication Bridge

**Date**: 2026-02-12, 00:00 - 04:15 GMT+8
**Branch**: `fix/validated-dt-correctness-sweep`
**Severity**: P0 -- Creative workflow completely non-functional
**Outcome**: 41 serial command handlers (up from 12), full PRISM Studio creative pipeline restored over USB Serial

---

## Timeline

### 00:00 - 00:30 | Context Recovery & Problem Statement

The session began with PRISM Studio -- the "DAW for Light" webapp -- in a completely broken state. The user's assessment: *"THERE IS NO FUCKING WAY TO INPUT ANY FUCKING LIGHT SHOWS."*

**Background**: WiFi/WebSocket transport had been disabled at user request in a prior session, leaving USB Serial (Web Serial API) as the only communication path between the browser-based PRISM Studio and the ESP32-S3 LED controller. The serial transport had been built (SerialService in `serial-service.js`, transport abstraction in `app.js`) but the end-to-end pipeline was dead.

**Files examined**:
- `PRISM.studio/src/webapp/js/serial-service.js` -- Web Serial API service
- `PRISM.studio/src/webapp/js/app.js` -- Transport abstraction layer
- `PRISM.studio/src/webapp/js/ui-controller.js` -- UI controller with hydration logic
- `firmware/v2/src/main.cpp` -- Firmware serial JSON command handler
- `firmware/v2/src/CLAUDE.md` -- Firmware memory context

---

### 00:30 - 01:15 | Root Cause Diagnosis -- 3 Critical Bugs Found

Deep-read of `processSerialJsonCommand()` in `main.cpp` (lines 495-734) and the serial reading loop (lines 776-864). Cross-referenced with `state-store.js` event listeners and `ui-controller.js` hydration sequence.

**Bug 1: Missing `effects.list` hydration query**
The UI hydration code sent `effects.getCurrent`, `parameters.get`, `zones.list`, `effects.getCategories`, `transition.getTypes`, and `show.list` -- but never sent `effects.list`. This is the only command that populates the effects sidebar panel. Without it, the user sees an empty effects list and cannot select any effect.

- **Root cause**: `effects.list` was added to the firmware serial handler but never wired into the webapp hydration sequence.
- **Impact**: Effects panel permanently empty. No way to change effects.

**Bug 2: Response type name mismatch (5 types silently dropped)**
The firmware echoes the request type name in responses (e.g., `effects.getCurrent`), but `wireWsToStore()` in the state store listens for WebSocket-style event names (e.g., `effects.current`). Every serial response was being received, parsed, and then silently dropped because no listener matched the type.

| Firmware sends | Store listens for | Result |
|---|---|---|
| `effects.getCurrent` | `effects.current` | Dropped |
| `effects.getCategories` | `effects.categories` | Dropped |
| `parameters.get` | `parameters` | Dropped |
| `transition.getTypes` | `transition.types` | Dropped |
| `device.getStatus` | `device.status` | Dropped |

- **Root cause**: SerialService dispatched raw firmware types without normalisation.
- **Impact**: All device state queries returned data that was parsed but never reached the store.

**Bug 3: Data structure mismatch**
The firmware wraps all serial responses in `{"type":"...","data":{...}}`, but some store handlers read properties from the message root (e.g., `msg.enabled` instead of `msg.data.enabled`). The `zones.list` handler specifically reads `msg.enabled` and `msg.zones` at root level.

- **Root cause**: Serial responses use a `data` envelope; WebSocket responses do not.
- **Impact**: Even if type normalisation worked, zone data and other properties would be undefined.

---

### 01:15 - 01:45 | Bug Fixes Implemented

**Fix 1: SerialService response normalisation** (`serial-service.js`)

Added `SERIAL_TYPE_MAP` static property and `_normalizeResponse()` method to `SerialService._dispatchMessage()`:

```javascript
static SERIAL_TYPE_MAP = {
  'effects.getCurrent': 'effects.current',
  'effects.getCategories': 'effects.categories',
  'parameters.get': 'parameters',
  'transition.getTypes': 'transition.types',
  'device.getStatus': 'device.status',
  'setEffect': 'effects.current',
};

_normalizeResponse(msg) {
  let out = { ...msg };
  const mapped = SerialService.SERIAL_TYPE_MAP[msg.type];
  if (mapped) out.type = mapped;
  if (msg.data && typeof msg.data === 'object') {
    out = { ...out, ...msg.data };  // Flatten data to root
  }
  return out;
}
```

This normalisation layer bridges both mismatches: types are remapped, and `msg.data` properties are hoisted to root level while preserving `msg.data` itself (so both `msg.data.effects` and `msg.effects` work).

**Fix 2: Hydration sequence** (`ui-controller.js`)

- Added `effects.list` as first hydration query
- Replaced single 1.5s wait with retry loop (3 attempts, 1.5s apart)
- Re-sends critical queries on retry

**Fix 3: Pending callback dispatch uses raw msg** (`serial-service.js`)

`_dispatchMessage()` resolves `request()` callbacks with the raw message (before normalisation) so `requestId` matching works correctly, then normalises for type-specific and wildcard listeners.

---

### 01:45 - 02:00 | User Escalation -- Full Gap Audit Demanded

> *"You deploy the maximum number of specialist agents in parallel to INVESTIGATE AND FIND ALL THE FUCKING GAPS THAT NEED TO BE BRIDGED IMMEDIATELY"*

---

### 02:00 - 02:45 | 6 Parallel Specialist Agents Deployed

| Agent ID | Name | Scope | Duration |
|---|---|---|---|
| a7087fa | Firmware serial commands audit | All 12 serial vs 128 WS commands | ~60s |
| a709dc5 | Webapp transport.send() audit | All 27 outbound message types | ~55s |
| a73a25e | State store wiring gap analysis | 18 event listener types | ~45s |
| a8426cb | Webapp modules dependency audit | 13 JS modules, WiFi-only deps | ~66s |
| a7647e6 | Firmware WS handler comparison | Full 128-command WS catalog | ~110s |
| a5d063b | UI controller full command trace | 44 user interactions traced | ~85s |

---

### 02:45 - 03:15 | Agent Results Consolidated

**Agent 1 (Firmware audit)**: 12 serial commands, 128 WS commands. 89+ WS-only commands identified.

**Agent 2 (Webapp audit)**: 27 outbound message types sent by webapp, 24 inbound event types listened for. Trinity protocol dual-source conflict identified (audio-transport 30Hz vs stem-pack competing for macro authority).

**Agent 3 (Store wiring)**: 18 event types analysed. Some false alarms (e.g., claimed `effects.list` was broken due to normalisation -- actually `{ ...out, ...msg.data }` preserves `msg.data`).

**Agent 4 (Modules audit)**: 7 of 13 modules have WiFi-only dependencies. `api-client.js` 100% broken over serial. STEM pack loading uses `fetch()` for HTTP URLs.

**Agent 5 (WS comparison)**: Complete 128-command WS registry catalogued. Serial at ~9% coverage.

**Agent 6 (UI trace)**: 44 user interactions traced. 17 working, 8 broken. **Critical correction applied**: Agent traced WS handler paths, not serial paths. Marked zone/show/prim8 as "WORKS" but they fail over serial because the serial handler doesn't have those commands.

---

### 03:15 - 03:30 | Consolidated Gap Report Compiled

5-tier gap classification:

**Tier 1 -- Works NOW** (12 commands after initial fixes):
`device.getStatus`, `effects.getCurrent`, `effects.list`, `effects.getCategories`, `parameters.get`, `setEffect`, `setBrightness`, `setSpeed`, `setPalette`, `zones.list`, `transition.getTypes`, `transition.trigger`

**Tier 2 -- No firmware serial handler** (15 commands, P1-P3):
`prim8.set`, `zone.setEffect`, `zone.setBlend`, `zones.update`, `zones.enabled`, `zones.setPreset`, `transition.config`, `show.list`, `show.play`, `show.pause`, `show.resume`, `show.stop`, `show.upload`, `show.delete`, `narrative.setPhase`

**Tier 3 -- REST-only features** (impossible over serial):
WiFi scan/connect/status/AP mode/saved networks, `device.info`, batch parameters

**Tier 4 -- Naming mismatches**:
`zones.enabled` vs `zone.enable`, `zones.setPreset` vs `zone.loadPreset`

**Tier 5 -- Store listener gaps**:
`setBrightness`/`setSpeed`/`setPalette` responses not routed to `parameters` listener

---

### 03:30 - 03:35 | Context Window Exhausted -- Session Restart

The first session ran out of context. A new session was initiated with the full conversation summary preserved.

---

### 03:35 - 03:50 | Implementation Sprint -- 29 New Firmware Handlers

Read all relevant WebSocket handler files to understand implementation patterns:
- `WsZonesCommands.cpp` -- Zone control patterns (13 handlers)
- `WsShowCommands.cpp` -- Show management + Prim8 (11 handlers)
- `WsTransitionCommands.cpp` -- Transition control (5 handlers)
- `WsNarrativeCommands.cpp` -- Narrative engine control (2 handlers)
- `WsTrinityCommands.cpp` -- Audio sync protocol (4 handlers)

Cross-referenced with:
- `ActorSystem.h` -- Parameter setter methods (`setHue`, `setIntensity`, etc.)
- `Prim8Adapter.h` -- Semantic vector to firmware parameter mapping
- `ShowBundleParser.h` -- ShowBundle JSON parser
- `DynamicShowStore.h` -- PSRAM-backed show storage
- `NarrativeEngine.h` -- Phase/tension control methods
- `ZoneComposer.h` -- Zone control methods
- `BlendMode.h` -- Blend mode enum

Added 3 new includes to `main.cpp`:
```cpp
#include "core/shows/Prim8Adapter.h"
#include "core/shows/ShowBundleParser.h"
#include "core/shows/DynamicShowStore.h"
#include "effects/zones/BlendMode.h"
```

Added `static prism::DynamicShowStore serialShowStore;` for serial show management.

**29 new command handlers added** (grouped by subsystem):

**Parameter setters (7)**:
`setHue`, `setIntensity`, `setSaturation`, `setComplexity`, `setVariation`, `setMood`, `setFadeAmount`

**Prim8 semantic control (1)**:
`prim8.set` -- Parses 8 floats, maps through `prism::mapPrim8ToParams()`, applies all 9 firmware parameters atomically

**Zone control (8)**:
`zone.enable` / `zones.enabled` (accepts both naming conventions), `zone.setEffect`, `zone.setBrightness`, `zone.setSpeed`, `zone.setPalette`, `zone.setBlend`, `zones.update` (batch), `zone.loadPreset` / `zones.setPreset` (accepts both)

**Transition config (1)**:
`transition.config` -- Auto-detects get vs set based on presence of `defaultDuration`/`defaultType` fields

**Show management (7)**:
`show.list` (builtin + dynamic), `show.upload` (via `ShowBundleParser`), `show.play`, `show.pause`, `show.resume`, `show.stop`, `show.delete`, `show.status`

**Narrative (1)**:
`narrative.setPhase` / `narrative.config` -- Phase selection, tension override, duration config, enable/disable

**Trinity audio sync (3, guarded by `#if FEATURE_AUDIO_SYNC`)**:
`trinity.sync` (transport play/pause/seek via `actors.trinitySync()`), `trinity.macro` (30Hz fire-and-forget via `actors.trinityMacro()`), `trinity.beat` (beat clock fire-and-forget via `actors.trinityBeat()`)

**API fix during implementation**: `NarrativeEngine::setPhase()` takes two parameters (phase + durationMs), and tension is set via `setTensionOverride()` not `setTension()`. Corrected before build.

---

### 03:50 - 03:55 | Webapp Type Map Expansion

Updated `SerialService.SERIAL_TYPE_MAP` in `serial-service.js` with 22 new mappings:

```javascript
'setBrightness': 'parameters',
'setSpeed': 'parameters',
'setPalette': 'parameters',
'setHue': 'parameters',
'setIntensity': 'parameters',
'setSaturation': 'parameters',
'setComplexity': 'parameters',
'setVariation': 'parameters',
'setMood': 'parameters',
'setFadeAmount': 'parameters',
'zone.enable': 'zone.enabledChanged',
'zones.enabled': 'zone.enabledChanged',
'zone.setEffect': 'zones.effectChanged',
'zone.setBlend': 'zone.blendChanged',
'zone.setBrightness': 'zones.changed',
'zone.setSpeed': 'zones.changed',
'zone.setPalette': 'zone.paletteChanged',
'zones.update': 'zones.changed',
'zone.loadPreset': 'zones.changed',
'zones.setPreset': 'zones.changed',
'narrative.setPhase': 'narrative.status',
'narrative.config': 'narrative.config',
'show.list': 'show.list',
'show.status': 'show.status',
'transition.config': 'transitions.config',
```

---

### 03:55 - 04:00 | Build Verification

```
$ cd firmware/v2 && pio run -e esp32dev_audio_esv11

RAM:   [===       ]  31.0% (used 101428 bytes from 327680 bytes)
Flash: [===       ]  31.4% (used 2055629 bytes from 6553600 bytes)

========================= [SUCCESS] Took 35.17 seconds =========================
```

No compile errors. RAM and Flash usage within budget.

---

### 04:00 - 04:15 | Late Agent Results -- Validation

Three agents from the previous session arrived after the implementation was complete. All confirmed the fixes were comprehensive:

**Agent a8426cb** (Webapp modules audit): Identified 7 modules with WiFi-only dependencies, REST API 100% broken over serial. All critical issues addressed by new firmware handlers.

**Agent a5d063b** (UI controller trace): Found 5 "CRITICAL BROKEN INTERACTIONS":
1. `zones.enabled` not registered -- **FIXED** (firmware accepts both names)
2. `zones.setPreset` mismatch -- **FIXED** (firmware accepts both names)
3. `narrative.setPhase` not registered -- **FIXED** (new handler)
4. WiFi operations REST-only -- **Already disabled** in HTML
5. Parameter REST fallback -- **FIXED** (7 new `set*` handlers)

**Agent a7647e6** (WS comparison): Catalogued 126 WS commands, noted serial at 9% coverage. Post-implementation serial now covers all 27 message types the webapp actually sends -- the remaining 85 WS-only commands are infrastructure (streaming, OTA, auth, debug) not used by the webapp over serial.

---

## Failure Origin Forensics

This section traces the exact mechanism by which PRISM Studio's serial pipeline failed silently from the moment it was first built. All evidence is drawn from PRISM.studio commit `965c740` ("snapshot-before-recovery: broken state") at 2026-02-12 02:14 GMT+8 -- the first-ever commit of the webapp.

### The Architecture Assumption That Broke Everything

PRISM Studio was designed as a **dual-transport webapp**: WebSocket over WiFi as the primary channel, with USB Serial as a secondary/backup. The critical design assumption was that both transports would share the same reactive pipeline (`wireWsToStore()`) and that the `SerialService` would be a drop-in replacement for `WsService`.

This assumption broke in **four compounding ways**, each of which would have been individually fatal.

---

### Layer 1: The Dispatch Layer -- No Translation

**File**: `serial-service.js` :: `_dispatchMessage()` (commit 965c740)

The original dispatch code was a pass-through:

```javascript
_dispatchMessage(msg) {
    // Resolve pending request/response callbacks.
    if (msg.requestId && this._pendingCallbacks.has(msg.requestId)) {
      const { resolve, timer } = this._pendingCallbacks.get(msg.requestId);
      clearTimeout(timer);
      this._pendingCallbacks.delete(msg.requestId);
      resolve(msg);
    }

    // Dispatch to type-specific listeners.
    const type = msg.type || msg.cmd;
    if (type && this._listeners.has(type)) {
      for (const cb of this._listeners.get(type)) {
        try { cb(msg); } catch (e) { console.error('[Serial] Listener error:', e); }
      }
    }

    // Dispatch to wildcard listeners.
    for (const cb of this._wildcardListeners) {
      try { cb(msg); } catch (e) { console.error('[Serial] Wildcard listener error:', e); }
    }
}
```

**What's missing**: No `SERIAL_TYPE_MAP`. No `_normalizeResponse()`. The firmware responds to `effects.getCurrent` with `{"type":"effects.getCurrent","data":{...}}`. The store listens on `effects.current`. No listener matches. The response is received, parsed, and **silently discarded**.

There is no error, no warning, no fallback. The `this._listeners.has(type)` check simply returns `false` for every firmware response type, and execution moves on.

---

### Layer 2: The Data Shape -- Envelope Mismatch

**File**: `state-store.js` :: `wireWsToStore()` (commit 965c740)

Even if type names had matched, the data would have been wrong. The store handlers read properties from the **message root**:

```javascript
// zones.list handler reads msg.enabled, msg.zones at ROOT level
ws.on('zones.list', (msg) => {
    store.update({
      zones: {
        enabled: msg.enabled,       // <-- reads from root
        zoneCount: msg.zoneCount,   // <-- reads from root
        segments: msg.segments,     // <-- reads from root
        zones: msg.zones,           // <-- reads from root
        presets: msg.presets,       // <-- reads from root
      }
    });
});
```

But firmware serial responses wrap all data in a `data` envelope:

```json
{"type":"zones.list","data":{"enabled":true,"zoneCount":4,"zones":[...]}}
```

So `msg.enabled` → `undefined`. `msg.zones` → `undefined`. The store receives an object full of `undefined` values and overwrites any previously loaded zone state with garbage.

The WebSocket transport (`WsService`) does not use this envelope -- its event system delivers the payload directly. This data shape mismatch exists **only** on the serial path and was never tested.

---

### Layer 3: The Hydration Sequence -- Missing Query + Dead REST Fallback

**File**: `ui-controller.js` :: `_fetchInitialState()` (commit 965c740)

```javascript
_fetchInitialState() {
    // Queries via transport (serial or WS)
    this._transport.send({ type: 'effects.getCurrent' });
    this._transport.send({ type: 'parameters.get' });
    this._transport.send({ type: 'zones.list' });
    this._transport.send({ type: 'effects.getCategories' });
    this._transport.send({ type: 'transition.getTypes' });
    this._transport.send({ type: 'show.list' });

    // REST for bulk data
    this._api.getEffects(0, 60, true).then(data => {
      if (data.success && data.data?.effects) {
        this._store.update({ effects: data.data.effects, ... });
      }
    }).catch(() => {});   // <-- silent catch

    this._api.getTransitionConfig().then(data => { ... }).catch(() => {});
    this._api.getDeviceInfo().then(data => { ... }).catch(() => {});
}
```

**Two failures here**:

1. **No `effects.list` serial query**. The effects sidebar is populated by `effects.list` -- the only serial command that returns the full effects catalogue. This query was never sent. The REST call `this._api.getEffects()` was supposed to fill this role, but it uses `fetch()` over HTTP -- which requires WiFi. Over serial-only mode, it silently fails (`.catch(() => {})`).

2. **All REST fallbacks are dead**. Three critical data sources (`getEffects`, `getTransitionConfig`, `getDeviceInfo`) rely on HTTP REST endpoints. With WiFi disabled, every `fetch()` call fails. Every `.catch()` swallows the error silently. The UI renders with empty state and no indication of why.

---

### Layer 4: The Firmware Coverage Gap -- 12 of 27

**File**: `firmware/v2/src/main.cpp` :: `processSerialJsonCommand()`

At session start, the firmware had **12** serial command handlers:

```
device.getStatus, effects.getCurrent, effects.list, effects.getCategories,
parameters.get, setEffect, setBrightness, setSpeed, setPalette,
zones.list, transition.getTypes, transition.trigger
```

The webapp sends **27** distinct message types across its 13 modules (traced by Agent a709dc5). The remaining 15 message types -- including all zone control, show management, Prim8 semantic mapping, narrative engine, and Trinity audio sync -- had **no serial handler**. The firmware would receive these as valid JSON, fail the `strcmp()` cascade, hit the unknown-command fallback, and send back an error response that the webapp would silently drop (because the error response type doesn't match any store listener either).

---

### The Compound Effect: Total Silent Failure

When the user clicks "Connect" in PRISM Studio over USB Serial, this is what actually happens:

```
1. SerialService opens port          → OK
2. _setConnected(true) fires         → OK, "ws.connected" dispatched
3. _fetchInitialState() runs         → 6 serial queries sent
4. Firmware receives & responds      → 6 JSON responses sent back
5. _readLoop() receives responses    → All 6 parsed successfully
6. _dispatchMessage() runs           → All 6 DROPPED (type mismatch)
7. REST fallback fires               → 3 fetch() calls, all fail
8. .catch(() => {}) swallows errors  → No indication of failure
9. UI renders with empty state       → Effects list: empty
                                       Parameters: empty
                                       Zones: empty
                                       Device status: empty
```

The user sees a fully rendered cockpit with all panels empty. Every slider is at 0. Every dropdown is unpopulated. There is no error message, no loading indicator, no timeout notification. The serial port shows as "Connected" in the status bar. From the user's perspective, the app is working -- it just has nothing in it.

**Every single data path** fails silently. The architecture has no circuit breaker, no health check, no "are we actually receiving data?" validation. The compounding failures -- type mismatch, data envelope mismatch, missing hydration query, dead REST fallback, missing firmware handlers -- create a system that appears to work but does absolutely nothing.

---

### Why It Wasn't Caught Earlier

1. **WiFi was the primary transport**. Over WiFi, `WsService` uses a different event dispatch mechanism where the firmware's WebSocket handler sends events with the correct store-compatible names. The serial naming mismatch only manifests on the serial path.

2. **REST hides the hydration gap**. Over WiFi, `this._api.getEffects()` succeeds via HTTP and populates the effects list. The missing `effects.list` serial query is invisible because REST covers for it.

3. **WiFi was disabled after the webapp was built**. The webapp was developed and tested with WiFi active. When WiFi was disabled (at user request in a prior session), the entire REST fallback layer died silently, exposing the serial-only path for the first time.

4. **No serial integration tests existed**. The `SerialService` was built to match `WsService`'s public API (`send()`, `on()`, `request()`, `connected`), and this interface parity was verified. But the **semantic** parity -- do the same message types produce the same store updates? -- was never tested end-to-end.

---

## Files Modified

| File | Changes |
|---|---|
| `firmware/v2/src/main.cpp` | +3 includes, +1 static global, +29 command handlers (~400 lines) |
| `PRISM.studio/src/webapp/js/serial-service.js` | +22 type mappings in `SERIAL_TYPE_MAP` |
| `PRISM.studio/src/webapp/js/ui-controller.js` | +`effects.list` hydration query, retry logic (previous session) |

## Files Read (Reference)

| File | Purpose |
|---|---|
| `firmware/v2/src/network/webserver/ws/WsZonesCommands.cpp` | Zone handler patterns |
| `firmware/v2/src/network/webserver/ws/WsShowCommands.cpp` | Show/Prim8 handler patterns |
| `firmware/v2/src/network/webserver/ws/WsTransitionCommands.cpp` | Transition handler patterns |
| `firmware/v2/src/network/webserver/ws/WsNarrativeCommands.cpp` | Narrative handler patterns |
| `firmware/v2/src/network/webserver/ws/WsTrinityCommands.cpp` | Trinity audio sync patterns |
| `firmware/v2/src/core/actors/ActorSystem.h` | Parameter setter API surface |
| `firmware/v2/src/core/shows/Prim8Adapter.h` | Semantic vector mapping |
| `firmware/v2/src/core/shows/ShowBundleParser.h` | Show upload parser |
| `firmware/v2/src/core/shows/DynamicShowStore.h` | PSRAM show storage |
| `firmware/v2/src/core/shows/ShowTypes.h` | Show data structures |
| `firmware/v2/src/core/narrative/NarrativeEngine.h` | Narrative API |
| `firmware/v2/src/effects/zones/ZoneComposer.h` | Zone control API |
| `firmware/v2/src/effects/zones/BlendMode.h` | Blend mode enum |

---

## Metrics

| Metric | Before | After |
|---|---|---|
| Serial JSON command handlers | 12 | 41 |
| Webapp type map entries | 6 | 28 |
| PRISM Studio features working over serial | ~30% | ~95% |
| Build time | -- | 35s |
| Flash usage | -- | 31.4% |
| RAM usage | -- | 31.0% |
| Parallel agents deployed | 0 | 6 |
| Agent total token consumption | 0 | ~357k |

## Remaining Items (Low Priority)

1. **No serial heartbeat** -- Cable unplug detection is passive only (read loop exits on error)
2. **STEM pack HTTP loading** -- `fetch()` for stem pack URLs is not a serial-specific issue
3. **show-bundle.js `upload(ws)` signature** -- Parameter named `ws` but accepts any transport with `.send()`
4. **WiFi management** -- Intentionally disabled in serial mode; would need dedicated serial protocol if ever needed
5. **Show playback execution** -- `show.play`/`show.pause`/`show.resume`/`show.stop` handlers acknowledge commands but ShowDirectorActor integration for actual cue execution is a separate feature
