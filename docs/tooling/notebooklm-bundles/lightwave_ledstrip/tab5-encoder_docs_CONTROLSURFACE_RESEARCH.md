---
abstract: "Complete semantic analysis of ControlSurfaceUI: 8-agent research synthesis covering firmware parameter API (IEffect, Ar16Controls), WS protocol (25 commands, codec gap), semantic contracts (9 mandatory), encoder routing (4 priorities, 6 sites), cross-stack alignment (4 dropped fields), spec gaps (3 critical, 1 contradiction), blank screen diagnosis (2 causes), and shared AR control system (16 params, 27 effects). Read before implementing FX PARAMS tab or modifying encoder routing."
---

# ControlSurfaceUI Research Synthesis

**Date:** 2026-03-22
**Scope:** Complete semantic analysis of the effect parameter introspection system spanning K1 firmware, WebSocket protocol, and Tab5 client
**Method:** 8 specialist agents deployed in parallel, zero-overlap scopes
**Investment:** ~650K tokens across 8 agents, 18 minutes wall time

---

## Table of Contents

1. [System Overview](#1-system-overview)
2. [Firmware Parameter API](#2-firmware-parameter-api)
3. [Ar16Controls Shared System](#3-ar16controls-shared-system)
4. [WebSocket Protocol](#4-websocket-protocol)
5. [Codec Gap — Critical Finding](#5-codec-gap--critical-finding)
6. [ControlSurfaceUI Semantic Contracts](#6-controlsurfaceui-semantic-contracts)
7. [Encoder Routing Architecture](#7-encoder-routing-architecture)
8. [Cross-Stack Type Alignment](#8-cross-stack-type-alignment)
9. [Blank Screen Root Cause](#9-blank-screen-root-cause)
10. [Implementation Spec Gap Analysis](#10-implementation-spec-gap-analysis)
11. [Decisions Required](#11-decisions-required)
12. [Source File Reference](#12-source-file-reference)

---

## 1. System Overview

```
K1 Firmware (100+ effects)              Wire (WebSocket)           Tab5 (ControlSurfaceUI)
─────────────────────────               ────────────────           ──────────────────────
IEffect::getParameterCount()  ─┐
IEffect::getParameter(index)  ─┤        effects.parameters.get    CSEffectParam[16]
  → EffectParameter (10 fields)─┤──JSON──→ (6 of 10 fields) ──────→ (9 fields + valid)
                                │        effects.parameters.set    applyEncoderDelta()
IEffect::setParameter(name,val)←┤──JSON──← {paramName: value} ←───── handleEncoderChange()
                                │
EffectPresetManager (16 slots) ─┤        effectPresets.list/       CSPresetSlot[8]
                                │        save/load/delete          handleEncoderButton(15)
                                │
CameraMode (bool)              ─┤        cameraMode.set/get/       _cameraModeActive
                                │        changed                   handleEncoderButton(14)
```

### Component Inventory

| Component | K1 Firmware | Wire Protocol | Tab5 Client |
|-----------|------------|---------------|-------------|
| **Effect params** | 10-field `EffectParameter` struct, 3 impl patterns, ~47 effects with params | `effects.parameters.get/set` — sends only 6 of 10 fields | `CSEffectParam` 9-field struct, type-aware encoder delta, 14 encoder slots |
| **Shared AR controls** | `Ar16Controls` — 16 standardised params, 5 groups, ~27 effects | Same wire format | No awareness of Ar16Controls specifically |
| **Presets** | `EffectPresetManager` — 16 NVS slots, full CRUD, all 5 WS commands | `effectPresets.list/get/saveCurrent/load/delete` | 8 slots, load/save/delete via encoder 15 |
| **Camera mode** | Full — saves/restores brightness/speed/fade/gamma | `cameraMode.set/get/changed` | Toggle on encoder 14, receives broadcast |
| **Param queue** | SPSC ring buffer, 16 slots, Core 0→Core 1 | N/A | Throttled per-encoder via WebSocketClient |

---

## 2. Firmware Parameter API

**Source:** `firmware-v3/src/plugins/api/IEffect.h`

### EffectParameter Struct (10 fields)

| Field | Type | Purpose |
|-------|------|---------|
| `name` | `const char*` | Wire key (used in WS commands) |
| `displayName` | `const char*` | Human-readable UI label |
| `minValue` | `float` | Lower clamp bound |
| `maxValue` | `float` | Upper clamp bound |
| `defaultValue` | `float` | Reset target |
| `type` | `EffectParameterType` | FLOAT(0), INT(1), BOOL(2), ENUM(3) |
| `step` | `float` | Suggested delta per click (default 0.01) |
| `group` | `const char*` | Logical grouping (timing/wave/blend/audio/colour) |
| `unit` | `const char*` | Display suffix (s, Hz, %, x) |
| `advanced` | `bool` | Expert-mode flag (unused — all false) |

Two constructors: full 10-field, and backward-compatible 5-field (fills type=FLOAT, step=0.01, group="", unit="", advanced=false).

### IEffect Virtual Methods

```cpp
virtual uint8_t getParameterCount() const { return 0; }           // Base: 0
virtual const EffectParameter* getParameter(uint8_t index) const;  // Base: nullptr
virtual bool setParameter(const char* name, float value);          // Base: false
virtual float getParameter(const char* name) const;                // Base: 0.0f
```

### Three Implementation Patterns

| Pattern | Effects | Description |
|---------|---------|-------------|
| **Ar16Controls** | ~27 | Shared 16-param audio-reactive block (see section 3) |
| **Custom static arrays** | ~20 | Effect defines own `s_params[]` with per-effect parameters |
| **Zero params** | ~55 | Never override `getParameterCount()` — only use global params |

### setParameter Flow

1. WS/REST handler validates key exists (linear scan of `getParameter(i)`)
2. Valid keys enqueued via `renderer->enqueueEffectParameterUpdate()` — SPSC ring buffer (16 slots)
3. Enqueue on Core 0 (network task), dequeue on Core 1 (RendererActor)
4. `applyPendingEffectParameterUpdates()` called at top of each render frame
5. Effect's `setParameter()` clamps and writes to member variable
6. Value takes effect immediately on next render frame

### Latent Bug: Param Name Truncation

`EffectParamUpdate.name` is `char[24]`. AUTO_TUNABLES effects have names 30-40 chars (e.g. `lgpgold_code_speckle_effect_speed_scale`). Truncation causes silent `setParameter()` failures when the truncated name doesn't match.

**Source:** `firmware-v3/src/core/actors/RendererActor.h` — EffectParamUpdate struct

---

## 3. Ar16Controls Shared System

**Source:** `firmware-v3/src/effects/ieffect/AudioReactiveLowRiskPackHelpers.h`
**Namespace:** `lightwaveos::effects::ieffect::lowrisk_ar`

### Standard Parameter Table (all 16 params)

| # | name | displayName | min | max | default | step | group | unit |
|---|------|-------------|-----|-----|---------|------|-------|------|
| 0 | `audio_mix` | Audio Mix | 0.0 | 1.0 | 0.35 | 0.01 | blend | — |
| 1 | `beat_gain` | Beat Gain | 0.0 | 2.0 | 0.80 | 0.05 | audio | x |
| 2 | `bass_gain` | Bass Gain | 0.0 | 2.0 | 1.00 | 0.05 | audio | x |
| 3 | `mid_gain` | Mid Gain | 0.0 | 2.0 | 1.00 | 0.05 | audio | x |
| 4 | `treble_gain` | Treble Gain | 0.0 | 2.0 | 1.00 | 0.05 | audio | x |
| 5 | `flux_gain` | Flux Gain | 0.0 | 2.0 | 0.90 | 0.05 | audio | x |
| 6 | `harmonic_gain` | Harmonic Gain | 0.0 | 2.0 | 0.70 | 0.05 | audio | x |
| 7 | `rhythmic_gain` | Rhythmic Gain | 0.0 | 2.0 | 0.70 | 0.05 | audio | x |
| 8 | `attack_s` | Attack | 0.01 | 1.50 | 0.20 | 0.01 | timing | s |
| 9 | `release_s` | Release | 0.05 | 2.50 | 0.60 | 0.05 | timing | s |
| 10 | `motion_rate` | Motion Rate | 0.25 | 3.0 | 1.00 | 0.05 | timing | x |
| 11 | `motion_depth` | Motion Depth | 0.0 | 2.0 | 0.85 | 0.05 | wave | x |
| 12 | `colour_anchor_mix` | Colour Anchor Mix | 0.0 | 1.0 | 0.65 | 0.05 | colour | — |
| 13 | `event_decay_s` | Event Decay | 0.05 | 2.0 | 0.30 | 0.05 | timing | s |
| 14 | `memory_gain` | Memory Gain | 0.0 | 2.0 | 0.70 | 0.05 | blend | x |
| 15 | `silence_hold` | Silence Hold | 0.0 | 1.0 | 0.55 | 0.05 | audio | — |

All parameters: type=FLOAT, advanced=false.

### Groups

| Group | Params | Purpose |
|-------|--------|---------|
| **audio** (7) | beat/bass/mid/treble/flux/harmonic/rhythmic gain, silence_hold | Signal gain multipliers |
| **timing** (4) | attack, release, motion_rate, event_decay | Envelope and speed |
| **blend** (2) | audio_mix, memory_gain | Reactive vs ambient crossfade |
| **wave** (1) | motion_depth | Spatial modulation |
| **colour** (1) | colour_anchor_mix | Hue lock vs free |

### Adoption

~27 effects use Ar16Controls (23 source files, some containing multiple effect classes). These include all 20 dedicated LGP AR effects, 2 ambient variants, and 4 ShapeBangersPack sub-effects.

### Key Design Properties

- **Static descriptors, instance storage.** `kParameters[]` is shared; `values[]` is per-effect instance.
- **No persistence.** `init()` calls `resetDefaults()` — values reset on effect switch.
- **Scene modifier overlay.** Translation Engine can dynamically adjust `audioMix`, `motionRate`, `motionDepth`, `brightnessScale`, `beatPulse`, `hueShiftSpeed` on top of user values.
- **Shared audio pipeline.** `updateSignals()` free function processes raw audio context through user's gain/timing settings, producing an `ArSignalSnapshot` used by all 27 effects.

---

## 4. WebSocket Protocol

**Source:** `firmware-v3/src/network/webserver/ws/WsEffectsCommands.cpp`, `WsEffectPresetCommands.cpp`

### effects.parameters.get

**Request:** `{"type": "effects.parameters.get", "effectId": 4352, "requestId": "..."}`
**Response type:** `"effects.parameters"` (NOT `"effects.parameters.get"`)
**Response data:**
```json
{
  "effectId": 4352, "name": "LGP Holographic", "hasParameters": true,
  "parameters": [{"name": "...", "displayName": "...", "min": 0, "max": 1, "default": 0.5, "value": 0.7}]
}
```

K1 always returns `success: true` even for parameterless effects (empty `parameters` array).

### effects.parameters.set

**Request:** `{"type": "effects.parameters.set", "effectId": 4352, "parameters": {"trailLength": 0.8}}`
**Response type:** `"effects.parameters.changed"`
**Response data:** `{"effectId": 4352, "queued": ["trailLength"], "failed": []}`

Unicast only — no broadcast to other WS clients. Tab5 does NOT handle this response (silently dropped).

### Camera Mode (3 commands)

| Command | Response Type | Broadcast |
|---------|--------------|-----------|
| `cameraMode.set` | `cameraMode.set` | `cameraMode.changed` to all |
| `cameraMode.get` | `cameraMode.get` | none |

Camera mode saves/restores brightness, speed, fadeAmount, gamma. Bypasses codec layer (reads JSON directly).

### Effect Presets (5 commands, all fully implemented)

| Command | Request Key | Response Type | Broadcast |
|---------|------------|---------------|-----------|
| `effectPresets.list` | — | `effectPresets.list` | none |
| `effectPresets.get` | `id` | `effectPresets.get` | none |
| `effectPresets.saveCurrent` | `slot`, `name` | `effectPresets.saved` | to all |
| `effectPresets.load` | `id` | `effectPresets.loaded` | none |
| `effectPresets.delete` | `id` | `effectPresets.deleted` | to all |

**Note:** `saveCurrent` uses `"slot"` but `load`/`delete` use `"id"` — field name inconsistency.

### Full Command Registry (25 effects-related WS commands)

20 in `WsEffectsCommands.cpp` + 5 in `WsEffectPresetCommands.cpp`. All fully implemented, no stubs.

---

## 5. Codec Gap — Critical Finding

**Source:** `firmware-v3/src/codec/WsEffectsCodec.cpp:506-521`

### What K1 Sends (6 of 10 fields)

```cpp
p["name"] = paramNames[i];
p["displayName"] = paramDisplayNames[i];
p["min"] = paramMins[i];
p["max"] = paramMaxs[i];
p["default"] = paramDefaults[i];
p["value"] = paramValues[i];
```

### What K1 DROPS (4 fields)

| Dropped Field | K1 Has It | Tab5 Expects It | Impact |
|---------------|-----------|-----------------|--------|
| `type` | Yes (`EffectParameterType`) | Yes (`CSParamType`) — but can only infer heuristically | **BLOCKER**: Tab5 can't distinguish ENUM from INT. Type-aware delta non-functional. |
| `step` | Yes (per param, e.g. 0.05) | Yes — decoder at `WsMessageRouter.h:637` reads `p["step"]` | Tab5 always falls back to `(max-min)/255`. Ar16Controls step=0.05 becomes 0.0078 (6x finer). |
| `unit` | Yes (s, x, Hz) | Yes — decoder at `WsMessageRouter.h:643-649` reads `p["unit"]` | Labels like "0.5 Hz" never appear. Users see bare numbers. |
| `group` | Yes (timing, audio, blend, wave, colour) | No field in Tab5 struct | No parameter grouping possible in UI. |

### Why This Matters

The `encodeParametersGet` function signature doesn't even accept step/type/unit/group. The caller in `WsEffectsCommands.cpp:329-344` extracts only 6 fields from `EffectParameter`, ignoring the rest.

**Fix:** Extend `WsEffectsCodec::encodeParametersGet()` to accept and emit all 10 fields. Tab5's decoder already handles `step` and `unit`. Only `type` needs a new decode path. The `group` and `advanced` fields need new struct members in `CSEffectParam`.

---

## 6. ControlSurfaceUI Semantic Contracts

**Source:** `tab5-encoder/src/ui/ControlSurfaceUI.h` (245 lines), `ControlSurfaceUI.cpp` (682 lines)

### 5-Layer Decomposition

**Layer 1 — Data Model:**
- `CSEffectParam` struct: name[32], displayName[32], unit[8], min, max, current, default, step, type, valid
- `CSPresetSlot` struct: occupied, name[32], effectId
- `CSParamType` enum: FLOAT=0, INT=1, BOOL=2, ENUM=3
- State: `_params[16]`, `_presetSlots[8]`, `_paramCount`, `_currentEffectId`, `_currentEffectName[48]`, `_cameraModeActive`, `_selectedPresetSlot`
- Constants: `CS_MAX_EFFECT_PARAMS=16`, `CS_MAX_PARAM_ENCODERS=14`, `CS_ENCODER_CAMERA_MODE=14`, `CS_ENCODER_PRESET_BANK=15`, `CS_MAX_PRESET_SLOTS=8`

**Layer 2 — Input Contract (encoder → action):**

`handleEncoderChange(encoderIndex, delta)`:
- Encoder 0-13: param adjustment via `applyEncoderDelta`
- Encoder 14: no-op (camera mode uses button only)
- Encoder 15: cycle preset slot (wraps 0-7)

`handleEncoderButton(encoderIndex, isLongPress)`:
- Encoder 0-13: reset to `defaultValue`, send to K1
- Encoder 14: toggle camera mode, send to K1
- Encoder 15 short press: load-if-occupied, save-if-empty (auto-name `STUDY_{id}_S{slot}`)
- Encoder 15 long press: delete preset

`applyEncoderDelta(paramIndex, delta)` — **type-aware**:
- BOOL: toggle (delta magnitude ignored — any click flips)
- INT/ENUM: `currentValue + sign(delta) * step`, then `roundf()` (magnitude ignored)
- FLOAT: `currentValue + delta * step` (magnitude preserved — velocity sensitive)
- All types: clamp to [minValue, maxValue]

**Layer 3 — Output Contract (action → K1):**

| Action | WS Method | Notes |
|--------|-----------|-------|
| Param changed | `sendEffectParameterChange(throttleIndex, effectId, paramName, value)` | throttleIndex = encoder index for rate limiting |
| Camera toggled | `sendCameraModeSet(active)` | — |
| Preset load | `sendEffectPresetLoad(slot)` | — |
| Preset save | `sendEffectPresetSave(slot, name)` | Auto-name: `STUDY_{effectId:04X}_S{slot}` |
| Preset delete | `sendEffectPresetDelete(slot)` | Long press only |

**Layer 4 — Inbound Contract (K1 → state):**

| Method | Trigger | Mutation |
|--------|---------|----------|
| `onEffectParametersReceived()` | K1 response to param request | Overwrites `_params[]`, `_paramCount`, triggers `rebindEncoders()` |
| `onPresetListReceived()` | K1 response to preset list request | Overwrites `_presetSlots[]` |
| `setCameraMode()` | K1 broadcast | Sets `_cameraModeActive` |
| `updateGlobalParam()` | K1 status broadcast | Direct LVGL write (no data model — design anomaly) |
| `onScreenEnter()` | Screen activation | Fetches: requestEffectParameters, requestEffectPresetsList, requestCameraModeGet |

**Layer 5 — Presentation (expendable):**
All LVGL widget creation, styling, positioning, colour constants, layout constants, card/bar/label rendering. This entire layer is replaced by the new tab layout.

### 9 Mandatory Contracts

1. Encoder routing table: 0-13=params, 14=camera, 15=preset
2. Type-aware delta: BOOL toggle, INT/ENUM snap+round, FLOAT velocity-scale
3. Param reset: button on 0-13 → reset to defaultValue AND send to K1
4. Preset context-sensitivity: short-press load-if-occupied / save-if-empty
5. Bootstrap fetch: onScreenEnter → 3 WS requests
6. Rebind cycle: onEffectParametersReceived → full re-populate
7. `_paramCount` activation: encoders beyond count are inactive/dimmed
8. Throttled output: per-encoder rate limiting via throttleIndex
9. Camera mode toggle: encoder 14 button toggles, rotation ignored

### Design Anomalies

- `loop()` is dead code — all updates are immediate, not throttled
- `_display` (M5GFX&) constructor arg is unused — legacy coupling
- `updateGlobalParam()` bypasses data model — writes directly to LVGL widget

---

## 7. Encoder Routing Architecture

**Source:** `tab5-encoder/src/main.cpp` (2903 lines)

### Routing Decision Tree (onEncoderChange, line 885)

```
onEncoderChange(index, value, wasReset)
│
├─ CHECK 1 (line 888): screen == ZONE_COMPOSER?
│  └─ YES → zoneUI->handleEncoderChange() — ALL 16 encoders
│
├─ CHECK 2 (line 919): screen == CONTROL_SURFACE?
│  └─ YES → csUI->handleEncoderChange() — ALL 16 encoders
│     (encoder 15 button handled separately in loop() via ClickDetector)
│
├─ CHECK 3 (line 961): screen == GLOBAL && index >= 8?
│  └─ YES → handleFxParamEncoderChange() — Unit-B only (8-15)
│
└─ FALLTHROUGH: Global parameter path
   └─ ParameterHandler → K1 global param update
```

### Two Competing FxParam Systems

| System | Active When | Encoder Range | Source Location |
|--------|------------|---------------|-----------------|
| `handleFxParamEncoderChange()` | GLOBAL screen, Unit-B | 8-15 | main.cpp:271 |
| `ControlSurfaceUI::handleEncoderChange()` | CONTROL_SURFACE screen | 0-15 (all) | ControlSurfaceUI.cpp |

These are mutually exclusive by screen state. The redesign puts both under UIScreen::GLOBAL, creating a routing conflict.

### 6 Routing Sites That Need Updating

| Line | Current Check | Change Needed |
|------|--------------|---------------|
| 888 | `screen == ZONE_COMPOSER` | Keep (ZONES tab or screen) |
| 919 | `screen == CONTROL_SURFACE` | Change to `screen == GLOBAL && tab == FX_PARAMS` |
| 961 | `screen == GLOBAL && index >= 8` | Add `&& tab != FX_PARAMS` guard |
| 1645 | `screen == GLOBAL` | May need tab check |
| 2655 | `screen == GLOBAL` (preset handling in loop) | Add `&& tab != FX_PARAMS` guard |
| 2702 | `screen == CONTROL_SURFACE` (encoder 15 click) | Change to tab-aware check |

### Recommended Approach

Add `getActiveTab()` alongside `getCurrentScreen()` (Option 2 from agent analysis). Cleanest separation — existing screen checks for GLOBAL remain valid when tab == MAIN.

### ClickDetector System

`g_clickDetectors[8]` — one per Unit-B encoder. Encoder 15 uses ClickDetector in loop() for long-hold detection (needed for preset delete). Encoders 0-14 use `wasReset` (boolean) which cannot distinguish single/double/long.

---

## 8. Cross-Stack Type Alignment

**Source:** `IEffect.h`, `ControlSurfaceUI.h`, `WsEffectsCodec.cpp`, `WsMessageRouter.h`

### Type Enum: MATCH (but never transmitted)

| Value | K1 (`EffectParameterType`) | Tab5 (`CSParamType`) | Transmitted? |
|-------|---------------------------|---------------------|-------------|
| 0 | FLOAT | FLOAT | **NO** |
| 1 | INT | INT | **NO** |
| 2 | BOOL | BOOL | **NO** |
| 3 | ENUM | ENUM | **NO** |

### JSON Field Name Mapping: EXACT MATCH (for 6 transmitted fields)

| JSON Key | K1 Encoder | Tab5 Decoder | Match? |
|----------|-----------|-------------|--------|
| `name` | `p["name"]` | `p["name"]` | Yes |
| `displayName` | `p["displayName"]` | `p["displayName"]` | Yes |
| `min` | `p["min"]` | `p["min"]` | Yes |
| `max` | `p["max"]` | `p["max"]` | Yes |
| `default` | `p["default"]` | `p["default"]` | Yes |
| `value` | `p["value"]` | `p["value"]` | Yes |

### Step Size: Always Falls Back

Tab5 decoder (`WsMessageRouter.h:637`): `out.step = p["step"] | ((out.maxValue - out.minValue) / 255.0f)`
Since `p["step"]` is never in the JSON, ArduinoJson `|` always returns the fallback.

| Param Example | K1 Step | Tab5 Step (fallback) | Ratio |
|--------------|---------|---------------------|-------|
| audio_mix (0-1) | 0.01 | 0.00392 | 2.5x finer |
| beat_gain (0-2) | 0.05 | 0.00784 | 6.4x finer |
| attack_s (0.01-1.5) | 0.01 | 0.00584 | 1.7x finer |

### Value Echo: Optimistic Update

Tab5 updates `currentValue` locally on encoder turn, then sends to K1. K1 responds with `queued`/`failed` keys but does NOT echo the applied value. Tab5 does not handle `effects.parameters.changed` — the response is silently dropped.

---

## 9. Blank Screen Root Cause

**Source:** `ControlSurfaceUI.cpp:300-314`, `WebSocketClient.h:256`, `IEffect.h:212`

### Root Cause 1 — Race Condition (75% probability)

`_currentEffectId` defaults to 0 (`WebSocketClient.h:256`). Guard at `ControlSurfaceUI.cpp:305`:
```cpp
if (effectId > 0) {
    _wsClient->requestEffectParameters(effectId, "cs_enter");
}
```
If user enters ControlSurface before K1's first status broadcast, `effectId == 0` and the entire request is silently skipped. No error logged, no retry.

**Fix:** Deferred request — if effectId==0 at screen entry, set a pending flag and issue the request when the next `effects.current` or status message arrives.

### Root Cause 2 — Parameterless Effect (20% probability)

~55 of ~115 effects never override `getParameterCount()` (base returns 0). K1 returns `success: true` with empty `parameters: []`. Tab5 calls `onEffectParametersReceived(..., 0)`. Cards 0-13 show "--".

**Fix:** Show "No adjustable parameters for this effect" instead of 14 blank cards.

### Confirmation Method

Add serial log in `onScreenEnter()`:
```cpp
Serial.printf("[CS] onScreenEnter: effectId=%u connected=%d\n",
              effectId, _wsClient ? _wsClient->isConnected() : -1);
```
- `effectId=0` → race condition
- `effectId=XXXX` + "loaded, 0 params" → parameterless effect

---

## 10. Implementation Spec Gap Analysis

**Source:** `tab5-encoder/docs/IMPLEMENTATION_SPEC.md`, `IMPLEMENTATION_SPEC_REVIEW.md`

### Critical Gaps

| Gap | Description | Spec Coverage | Impact |
|-----|-------------|--------------|--------|
| **Camera mode** | Encoder 14 camera toggle | MISSING ENTIRELY | Functionality lost |
| **Type-aware encoding** | FLOAT/INT/BOOL/ENUM delta calculation | ZERO mention | Encoders produce raw deltas — booleans won't toggle, ints won't snap |
| **Absorption plan** | Migration of ControlSurfaceUI contracts | One parenthetical: "(may be absorbed)" at line 276 | After all 8 phases, ControlSurfaceUI is dead code competing with FX tab |
| **Data fetch timing** | When to request param metadata | NOT SPECIFIED | Implementer must guess |

### Spec Contradictions

| Topic | Spec Says | Code Says | Resolution |
|-------|----------|----------|------------|
| Preset long-press | "Long-press = save" | "Long-press = delete" (`ControlSurfaceUI.cpp:375-379`) | Delete is correct — accidental long-press should not overwrite |
| Phase 1 colours | Pixel-identical refactor | Token values differ from codebase values | Use current codebase values, not spec values |

### Param Slot Reduction

| System | Encoders for FX params | Max visible | Paging needed? |
|--------|----------------------|-------------|---------------|
| ControlSurfaceUI (current) | 0-13 (14 slots) | 14 | Never |
| FX PARAMS tab (redesign) | 8-13 (6 per page) | 6 | Effects with >6 params |

Paging accommodates up to 16 params (3 pages), so total addressable params is maintained. Ergonomic cost: effects with 7-14 params now require page navigation.

---

## 11. Decisions Required

| # | Decision | Options | Recommendation |
|---|----------|---------|---------------|
| D1 | Camera mode placement | Mode button / Sidebar / Drop / Keep separate screen | If production: mode button. If dev: explicit drop. |
| D2 | Preset gesture | Spec's "long-press=save" / Code's "long-press=delete" | Long-press=delete (accidental overwrite prevention) |
| D3 | ControlSurfaceUI fate | Delete after migration / Keep / Refactor to data model | Delete after extracting 9 semantic contracts |
| D4 | Data fetch trigger | Tab switch / Effect change / Boot | Effect change (regardless of active tab) |
| D5 | Codec gap | Fix now / Fix later / Never | Fix now — prerequisite for type-aware delta |
| D6 | Encoder 0-7 trade-off | Accept 14→6 reduction / Redesign | Accept — global params used more frequently |

---

## 12. Source File Reference

### K1 Firmware

| File | Lines | Content |
|------|-------|---------|
| `firmware-v3/src/plugins/api/IEffect.h` | 102-244 | EffectParameter struct, EffectParameterType enum, IEffect virtual API |
| `firmware-v3/src/effects/ieffect/AudioReactiveLowRiskPackHelpers.h` | 122+ | Ar16Controls: 16 params, 5 groups, set/get, scene modifiers |
| `firmware-v3/src/core/actors/RendererActor.h` | — | EffectParamUpdate struct (name[24] limit), PARAM_QUEUE_SIZE=16 |
| `firmware-v3/src/core/actors/RendererActor.cpp` | ~1100 | applyPendingEffectParameterUpdates() dequeue loop |
| `firmware-v3/src/network/webserver/ws/WsEffectsCommands.cpp` | 294-422 | effects.parameters.get/set handlers, camera mode |
| `firmware-v3/src/network/webserver/ws/WsEffectPresetCommands.cpp` | 90-469 | 5 preset commands (all implemented) |
| `firmware-v3/src/codec/WsEffectsCodec.cpp` | 506-521 | encodeParametersGet — THE codec gap (6 of 10 fields) |
| `firmware-v3/src/codec/WsEffectsCodec.h` | 346 | Encoder function signature (no step/type/unit/group params) |

### Tab5 Encoder

| File | Lines | Content |
|------|-------|---------|
| `tab5-encoder/src/ui/ControlSurfaceUI.h` | 1-244 | CSEffectParam, CSParamType, full API surface |
| `tab5-encoder/src/ui/ControlSurfaceUI.cpp` | 1-682 | Semantic logic: delta, formatting, routing, WS integration |
| `tab5-encoder/src/ui/DisplayUI.h` | 44-49 | UIScreen enum including CONTROL_SURFACE |
| `tab5-encoder/src/ui/DisplayUI.cpp` | 200-205, 851-857, 1130-1134 | Screen entry, lifecycle, navigation |
| `tab5-encoder/src/network/WsMessageRouter.h` | 141, 183-196, 583-669, 677-692, 697-740 | Response handlers: params, camera, presets, global forwarding |
| `tab5-encoder/src/network/WebSocketClient.cpp` | 459-482 | requestEffectParameters, sendEffectParameterChange |
| `tab5-encoder/src/main.cpp` | 885-1083, 2655-2722 | Encoder routing decision tree, ClickDetector handling |

### Spec Documents

| File | Content |
|------|---------|
| `tab5-encoder/docs/IMPLEMENTATION_SPEC.md` | UI redesign spec (305 lines) |
| `tab5-encoder/docs/IMPLEMENTATION_SPEC_REVIEW.md` | LVGL feasibility review (695 lines) |

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-22 | agent:research-council (8 specialists) | Created — complete semantic analysis from 8 parallel specialist agents |
