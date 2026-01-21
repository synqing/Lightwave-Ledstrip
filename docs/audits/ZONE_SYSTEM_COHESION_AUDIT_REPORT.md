# Zone System Cohesion Audit Report

**Date**: 2025-01-27  
**Scope**: Zone Composer system across firmware (v2), web apps (data/app.js, lightwave-simple), and Tab5.encoder  
**Status**: Complete

---

## Executive Summary

This audit examined the Zone Composer system across three major components to identify inconsistencies, missing features, and alignment issues. The system demonstrates **good overall cohesion** with some **critical gaps** in feature parity and data structure alignment.

### Key Findings

- ✅ **Data Structures**: ZoneSegment structures are well-aligned across systems
- ⚠️ **ZoneState**: Multiple ZoneState definitions with different fields
- ⚠️ **Feature Parity**: Significant gaps in audio config and beat trigger support
- ✅ **Validation Logic**: Consistent validation rules, but web apps missing ordering check
- ⚠️ **State Synchronization**: lightwave-simple missing audio config fields
- ✅ **API Protocol**: WebSocket commands well-aligned
- ⚠️ **Presets**: Preset definitions match, but Tab5.encoder has hardcoded duplicates

---

## 1. Data Structure Alignment

### 1.1 ZoneSegment Structure

**Status**: ✅ **ALIGNED**

| System | Location | Fields | Namespace |
|--------|----------|--------|-----------|
| Firmware v2 | `firmware/v2/src/effects/zones/ZoneDefinition.h` | `zoneId`, `s1LeftStart`, `s1LeftEnd`, `s1RightStart`, `s1RightEnd`, `totalLeds` | `lightwaveos::zones` |
| Tab5.encoder | `firmware/Tab5.encoder/src/zones/ZoneDefinition.h` | `zoneId`, `s1LeftStart`, `s1LeftEnd`, `s1RightStart`, `s1RightEnd`, `totalLeds` | `zones` |
| Web Apps | JavaScript objects | Same field names | N/A |

**Findings**:
- ✅ Field names match exactly
- ✅ Field types match (uint8_t)
- ⚠️ Namespace difference (`lightwaveos::zones` vs `zones`) - acceptable for Tab5.encoder compatibility

### 1.2 ZoneState Structure

**Status**: ⚠️ **MISALIGNED - Multiple Definitions**

#### Firmware ZoneComposer.h
```cpp
struct ZoneState {
    uint8_t effectId;
    uint8_t brightness;
    uint8_t speed;
    uint8_t paletteId;
    BlendMode blendMode;
    bool enabled;
    ZoneAudioConfig audio;  // Full audio config
};
```

#### Firmware SystemState.h
```cpp
struct ZoneState {
    uint8_t effectId;
    uint8_t paletteId;
    uint8_t brightness;
    uint8_t speed;
    bool enabled;
    // Missing: blendMode, audio config
};
```

#### Tab5.encoder ZoneComposerUI.h
```cpp
struct ZoneState {
    uint8_t effectId = 0;
    char effectName[48] = {0};
    uint8_t speed = 25;
    uint8_t paletteId = 0;
    char paletteName[48] = {0};
    uint8_t blendMode = 0;
    char blendModeName[32] = {0};
    bool enabled = false;
    uint8_t ledStart = 0;  // Display-only
    uint8_t ledEnd = 0;    // Display-only
    // Missing: brightness, audio config
};
```

#### Web Apps (firmware/v2/data/app.js)
```javascript
state.zones.zones[zoneId] = {
    effectId, brightness, speed, paletteId, blendMode,
    tempoSync, audioBand, beatTriggerEnabled, beatTriggerInterval,
    beatModulation, tempoSpeedScale, beatDecay
}
```

**Findings**:
- ❌ **SystemState.h ZoneState missing `blendMode` and `audio` config** - This is a critical gap
- ⚠️ **Tab5.encoder ZoneState missing `brightness`** - Display-only structure, acceptable
- ✅ **Web apps have full audio config support** in state structure
- ⚠️ **lightwave-simple missing audio config fields** in state updates (see Section 5)

### 1.3 Constants Alignment

**Status**: ✅ **ALIGNED**

| Constant | Firmware v2 | Tab5.encoder | Web Apps |
|----------|-------------|--------------|----------|
| `MAX_ZONES` | 4 | 4 | 4 |
| `STRIP_LENGTH` | 160 | N/A (uses MAX_LED=159) | N/A |
| `CENTER_LEFT` | 79 (hardcoded) | 79 | 79 |
| `CENTER_RIGHT` | 80 (hardcoded) | 80 | 80 |
| `MAX_LED` | 159 | 159 | 159 |
| `TOTAL_LEDS` | 320 | N/A | N/A |

**Findings**:
- ✅ All constants match across systems
- ✅ Centre pair (79/80) consistent everywhere

---

## 2. API/Protocol Alignment

### 2.1 WebSocket Command Types

**Status**: ✅ **WELL ALIGNED**

| Command | Firmware | Tab5.encoder | Web Apps |
|---------|----------|--------------|----------|
| `zone.enable` | ✅ | ✅ | ✅ |
| `zone.setEffect` | ✅ | ✅ | ✅ |
| `zone.setBrightness` | ✅ | ✅ | ✅ |
| `zone.setSpeed` | ✅ | ✅ | ✅ |
| `zone.setPalette` | ✅ | ✅ | ✅ |
| `zone.setBlend` | ✅ | ✅ | ✅ |
| `zone.loadPreset` | ✅ | ❌ | ✅ |
| `zones.get` | ✅ | ❌ | ✅ |
| `zones.list` | ✅ | ✅ | ✅ |
| `zones.update` | ✅ | ❌ | ✅ |
| `zones.setEffect` | ✅ | ❌ | ✅ |
| `zones.setLayout` | ✅ | ✅ | ✅ |
| `zones.getAudio` | ✅ | ❌ | ❌ |
| `zones.setAudio` | ✅ | ❌ | ❌ |
| `zones.getBeatTrigger` | ✅ | ❌ | ❌ |
| `zones.setBeatTrigger` | ✅ | ❌ | ❌ |
| `zones.reorder` | ✅ | ❌ | ✅ (data/app.js only) |

**Findings**:
- ✅ Core zone commands well-supported
- ❌ **Tab5.encoder missing audio config commands** - Feature gap
- ❌ **Tab5.encoder missing beat trigger commands** - Feature gap
- ❌ **Tab5.encoder missing zone reordering** - Feature gap
- ❌ **Web apps missing audio config commands** - UI may exist but commands not used
- ⚠️ **lightwave-simple missing `zones.reorder`** - Feature gap

### 2.2 WebSocket Response Types

**Status**: ✅ **ALIGNED**

| Event Type | Firmware | Tab5.encoder | Web Apps |
|------------|----------|--------------|----------|
| `zones.list` | ✅ | ✅ | ✅ |
| `zones.changed` | ✅ | ✅ | ✅ |
| `zones.effectChanged` | ✅ | ❌ | ✅ |
| `zone.enabledChanged` | ✅ | ❌ | ✅ |
| `zones.layoutChanged` | ✅ | ❌ | ✅ |
| `zone.paletteChanged` | ✅ | ❌ | ✅ |

**Findings**:
- ✅ Core events supported
- ⚠️ **Tab5.encoder only handles `zones.list` and `zones.changed`** - Missing granular events
- ✅ Web apps handle all events correctly

### 2.3 REST API Endpoints

**Status**: ✅ **ALIGNED** (Not used by Tab5.encoder or web apps)

All REST endpoints are properly implemented in firmware. Web apps use WebSocket exclusively.

### 2.4 Field Name Consistency

**Status**: ✅ **CONSISTENT**

- ✅ `zoneId` used consistently (not `zone`)
- ✅ `effectId` used consistently (not `effect`)
- ✅ `paletteId` used consistently (not `palette`)
- ✅ `blendMode` used consistently (with `blendModeName` for display)

---

## 3. Validation Logic Consistency

### 3.1 Zone Layout Validation

**Status**: ⚠️ **MOSTLY ALIGNED - Missing Ordering Check in Web Apps**

#### Firmware (`ZoneComposer::validateLayout()`)
Checks:
1. ✅ Boundary ranges (0-79 left, 80-159 right)
2. ✅ Minimum zone size
3. ✅ Symmetry (leftSize == rightSize, distance from centre)
4. ✅ Centre pair inclusion (Zone 0 must include 79/80)
5. ✅ Overlap detection
6. ✅ Complete coverage (all LEDs 0-159)
7. ✅ **Centre-outward ordering** (Zone 0 innermost)

#### Web Apps (`validateZoneLayout()`)
Checks:
1. ✅ Boundary ranges
2. ✅ Minimum zone size
3. ✅ Symmetry
4. ✅ Centre pair inclusion
5. ✅ Overlap detection
6. ✅ Complete coverage
7. ❌ **Missing centre-outward ordering check**

**Findings**:
- ⚠️ **Web apps missing ordering validation** - This is acceptable because firmware reorders automatically (see Section 3.2)
- ✅ All other validation rules match
- ✅ Error messages are user-friendly in web apps

### 3.2 Zone Reordering

**Status**: ✅ **IMPLEMENTED**

- ✅ Reordering function exists in WebSocket handler (`reorderZonesByCentreDistance()`)
- ✅ Reordering function exists in REST handler
- ✅ Distance calculation: `min(79 - s1LeftEnd, s1RightStart - 80)` - **CORRECT**
- ✅ ZoneId reassignment after reordering - **CORRECT**

**Note**: Web apps don't need ordering validation because firmware automatically reorders before validation.

---

## 4. Feature Completeness

### 4.1 Audio Configuration

**Status**: ⚠️ **PARTIAL SUPPORT**

| Feature | Firmware | data/app.js | lightwave-simple | Tab5.encoder |
|---------|----------|-------------|------------------|--------------|
| `tempoSync` | ✅ | ✅ | ❌ | ❌ |
| `beatModulation` | ✅ | ✅ | ❌ | ❌ |
| `tempoSpeedScale` | ✅ | ✅ | ❌ | ❌ |
| `beatDecay` | ✅ | ✅ | ❌ | ❌ |
| `audioBand` | ✅ | ✅ | ❌ | ❌ |
| UI Controls | ✅ | ✅ | ❌ | ❌ |
| WebSocket Commands | ✅ | ❌ | ❌ | ❌ |

**Findings**:
- ✅ **firmware/v2/data/app.js has full audio config support** in UI and state
- ❌ **lightwave-simple completely missing audio config** - Feature gap
- ❌ **Tab5.encoder missing audio config** - Feature gap
- ⚠️ **Web apps don't use `zones.setAudio` command** - They use `zones.update` instead

### 4.2 Beat Trigger

**Status**: ⚠️ **PARTIAL SUPPORT**

| Feature | Firmware | data/app.js | lightwave-simple | Tab5.encoder |
|---------|----------|-------------|------------------|--------------|
| `beatTriggerEnabled` | ✅ | ✅ | ❌ | ❌ |
| `beatTriggerInterval` | ✅ | ✅ | ❌ | ❌ |
| `effectList` | ✅ | ✅ | ❌ | ❌ |
| UI Controls | ✅ | ✅ | ❌ | ❌ |
| WebSocket Commands | ✅ | ❌ | ❌ | ❌ |

**Findings**:
- ✅ **firmware/v2/data/app.js has full beat trigger support** in UI and state
- ❌ **lightwave-simple completely missing beat trigger** - Feature gap
- ❌ **Tab5.encoder missing beat trigger** - Feature gap
- ⚠️ **Web apps don't use `zones.setBeatTrigger` command** - They use `zones.update` instead

### 4.3 Zone Reordering

**Status**: ⚠️ **PARTIAL SUPPORT**

| Feature | Firmware | data/app.js | lightwave-simple | Tab5.encoder |
|---------|----------|-------------|------------------|--------------|
| `zones.reorder` command | ✅ | ✅ | ❌ | ❌ |
| UI Controls | ✅ | ✅ | ❌ | ❌ |

**Findings**:
- ✅ **firmware/v2/data/app.js supports reordering** with UI controls
- ❌ **lightwave-simple missing reordering** - Feature gap
- ❌ **Tab5.encoder missing reordering** - Feature gap

### 4.4 Zone Presets

**Status**: ✅ **ALIGNED** (with minor issue)

| Preset | Firmware | data/app.js | lightwave-simple | Tab5.encoder |
|--------|----------|-------------|------------------|--------------|
| Preset 0 (Unified) | ✅ | ✅ | ✅ | ✅ |
| Preset 1 (Dual Split) | ✅ | ✅ | ✅ | ✅ |
| Preset 2 (Triple Rings) | ✅ | ✅ | ✅ | ✅ |
| Preset 3 (Quad Active) | ✅ | ✅ | ✅ | ✅ |
| Preset 4 (Heartbeat Focus) | ✅ | ✅ | ✅ | ✅ |

**Findings**:
- ✅ All preset definitions match across systems
- ⚠️ **Tab5.encoder has hardcoded preset definitions** instead of using shared constants - Acceptable but not ideal
- ✅ Preset segment values match exactly

### 4.5 Zone Layout Customization

**Status**: ✅ **FULLY SUPPORTED**

- ✅ Web apps support custom layout creation
- ✅ Tab5.encoder supports custom layouts (via UI)
- ✅ Layout persistence via `zones.setLayout` command
- ✅ Validation ensures centre-origin compliance

---

## 5. State Synchronization

### 5.1 Initial State Loading

**Status**: ✅ **ALIGNED**

| System | Method | Command |
|--------|--------|---------|
| data/app.js | On WebSocket connect | `{"type":"zones.list"}` |
| lightwave-simple | On WebSocket connect | `{"type":"zones.list"}` |
| Tab5.encoder | On WebSocket connect | `requestZonesState()` → `{"type":"zones.list"}` |

**Findings**:
- ✅ All systems request zone state on connect
- ✅ State structure matches between request and response

### 5.2 Event Handling

**Status**: ⚠️ **PARTIAL ALIGNMENT**

#### firmware/v2/data/app.js
Handles:
- ✅ `zones.list`
- ✅ `zones.changed` (with full audio config fields)
- ✅ `zones.effectChanged`
- ✅ `zone.enabledChanged`
- ✅ `zones.layoutChanged`

#### lightwave-simple/app.js
Handles:
- ✅ `zones.list`
- ✅ `zones.changed` (but **missing audio config fields**)
- ✅ `zones.effectChanged`
- ✅ `zone.enabledChanged`
- ❌ `zones.layoutChanged` (not explicitly handled)

#### Tab5.encoder
Handles:
- ✅ `zones.list`
- ✅ `zones.changed`
- ❌ Other events (not needed for encoder UI)

**Findings**:
- ❌ **lightwave-simple missing audio config field updates** in `zones.changed` handler
- ⚠️ **lightwave-simple missing `zones.layoutChanged` handler** - May cause UI desync

### 5.3 State Update Logic

**Status**: ✅ **FUNCTIONAL**

- ✅ Web apps update local state correctly on events
- ✅ State updates trigger UI refreshes
- ✅ Tab5.encoder updates UI on state changes

---

## 6. Error Handling

### 6.1 Error Codes

**Status**: ✅ **CONSISTENT**

| Error Code | Usage | Consistency |
|------------|-------|-------------|
| `INVALID_VALUE` | Layout validation, missing fields | ✅ Consistent |
| `OUT_OF_RANGE` | Invalid zoneId, effectId, speed | ✅ Consistent |
| `FEATURE_DISABLED` | Zone system not available | ✅ Consistent |
| `MISSING_FIELD` | Required field missing | ✅ Consistent |

**Findings**:
- ✅ Error codes used consistently across handlers
- ✅ Error messages are descriptive

### 6.2 Validation Error Display

**Status**: ✅ **GOOD**

- ✅ Web apps display validation errors in UI
- ✅ Error messages are user-friendly
- ⚠️ Tab5.encoder doesn't display validation errors (hardware limitation)

---

## 7. Documentation Alignment

### 7.1 API Documentation

**Status**: ✅ **GOOD**

- ✅ Zone API documented in `docs/api/api-v1.md`
- ✅ Zone API documented in `docs/api/api-v2.md`
- ✅ WebSocket command examples match implementation
- ⚠️ Audio config and beat trigger commands not fully documented

### 7.2 Code Comments

**Status**: ✅ **GOOD**

- ✅ ZoneDefinition.h comments match preset definitions
- ✅ ZoneComposer.h documentation is accurate
- ✅ Web app code has adequate comments

---

## Critical Issues

### Priority 1: Breaking Issues

1. **SystemState.h ZoneState Missing Fields**
   - **Issue**: `SystemState::ZoneState` missing `blendMode` and `audio` config
   - **Impact**: State synchronization may lose data
   - **Recommendation**: Add missing fields to SystemState::ZoneState

### Priority 2: Feature Parity Gaps

2. **lightwave-simple Missing Audio Config**
   - **Issue**: No audio config support in UI or state updates
   - **Impact**: Users can't configure audio-reactive zones
   - **Recommendation**: Port audio config UI from data/app.js

3. **lightwave-simple Missing Beat Trigger**
   - **Issue**: No beat trigger support
   - **Impact**: Users can't configure beat-triggered effect cycling
   - **Recommendation**: Port beat trigger UI from data/app.js

4. **Tab5.encoder Missing Audio Config**
   - **Issue**: No audio config commands or UI
   - **Impact**: Encoder can't control audio-reactive zones
   - **Recommendation**: Add audio config support (if hardware allows)

5. **Tab5.encoder Missing Beat Trigger**
   - **Issue**: No beat trigger commands or UI
   - **Impact**: Encoder can't configure beat triggers
   - **Recommendation**: Add beat trigger support (if hardware allows)

6. **lightwave-simple Missing Zone Reordering**
   - **Issue**: No `zones.reorder` command support
   - **Impact**: Users can't reorder zones
   - **Recommendation**: Port reordering UI from data/app.js

### Priority 3: Minor Issues

7. **Web Apps Missing Ordering Validation**
   - **Issue**: Web apps don't validate centre-outward ordering
   - **Impact**: None (firmware reorders automatically)
   - **Recommendation**: Optional - add validation for better UX

8. **Tab5.encoder Hardcoded Presets**
   - **Issue**: Preset definitions duplicated instead of using shared constants
   - **Impact**: Maintenance burden if presets change
   - **Recommendation**: Consider sharing preset definitions (may not be feasible due to namespace differences)

9. **lightwave-simple Missing Layout Changed Handler**
   - **Issue**: No explicit handler for `zones.layoutChanged`
   - **Impact**: UI may not refresh after layout changes
   - **Recommendation**: Add handler or verify UI refreshes via other events

---

## Recommendations

### Immediate Actions

1. **Fix SystemState::ZoneState** - Add `blendMode` and `audio` config fields
2. **Port audio config to lightwave-simple** - Copy UI and state handling from data/app.js
3. **Port beat trigger to lightwave-simple** - Copy UI and state handling from data/app.js
4. **Add layout changed handler to lightwave-simple** - Ensure UI refreshes correctly

### Future Enhancements

1. **Add audio config to Tab5.encoder** - If hardware/UI space allows
2. **Add beat trigger to Tab5.encoder** - If hardware/UI space allows
3. **Add zone reordering to Tab5.encoder** - If UI space allows
4. **Document audio config and beat trigger APIs** - Complete API documentation
5. **Consider shared preset definitions** - Evaluate feasibility of sharing constants

### Testing Recommendations

1. **Cross-system compatibility tests** - Verify all systems work with same zone layouts
2. **Validation consistency tests** - Ensure all systems reject invalid layouts
3. **State synchronization tests** - Verify state sync across all systems
4. **Feature parity tests** - Test audio config and beat trigger across systems

---

## Conclusion

The Zone Composer system demonstrates **good overall cohesion** with well-aligned data structures and API protocols. The main gaps are in **feature parity**, particularly:

- lightwave-simple missing audio config and beat trigger
- Tab5.encoder missing advanced features (acceptable given hardware constraints)
- SystemState::ZoneState missing fields (needs immediate fix)

The system is **production-ready** for core zone functionality, but **feature parity improvements** would enhance user experience across all platforms.

---

**Report Generated**: 2025-01-27  
**Next Review**: After Priority 1 and 2 fixes are implemented

