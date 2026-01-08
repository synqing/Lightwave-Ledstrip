# Modifiers API Specification

**Version**: 1.0.0
**Status**: PLANNED (Phase A - A2)
**Base URL**: `/api/v1/modifiers`

---

## Overview

The Modifiers API provides RESTful endpoints and WebSocket commands for managing effect modifiers. Modifiers are post-processing layers that transform LED output after effect rendering.

---

## REST API Endpoints

### 1. Add Modifier

**Endpoint**: `POST /api/v1/modifiers/add`

**Description**: Add a new modifier to the active stack.

**Request Body**:
```json
{
  "type": "speed|intensity|color_shift|mirror|glitch",
  "params": {
    // Type-specific parameters (see below)
  }
}
```

**Success Response** (200):
```json
{
  "success": true,
  "data": {
    "modifierIndex": 0,
    "type": "speed",
    "memoryUsage": 48,
    "stackCount": 1
  },
  "timestamp": 1234567890,
  "version": "1.0"
}
```

**Error Response** (400):
```json
{
  "success": false,
  "error": "Stack full (max 8 modifiers)",
  "timestamp": 1234567890,
  "version": "1.0"
}
```

---

### 2. Remove Modifier

**Endpoint**: `POST /api/v1/modifiers/remove`

**Description**: Remove a modifier from the active stack.

**Request Body**:
```json
{
  "type": "speed"
}
```

**Success Response** (200):
```json
{
  "success": true,
  "data": {
    "removed": true,
    "stackCount": 0
  },
  "timestamp": 1234567890,
  "version": "1.0"
}
```

---

### 3. List Modifiers

**Endpoint**: `GET /api/v1/modifiers/list`

**Description**: Get all active modifiers and their parameters.

**Success Response** (200):
```json
{
  "success": true,
  "data": {
    "modifiers": [
      {
        "type": "speed",
        "name": "Speed",
        "enabled": true,
        "params": {
          "multiplier": 2.0
        }
      },
      {
        "type": "intensity",
        "name": "Intensity",
        "enabled": true,
        "params": {
          "source": "audio_rms",
          "baseIntensity": 0.8,
          "depth": 0.5
        }
      }
    ],
    "count": 2,
    "maxCount": 8,
    "memoryUsage": 96
  },
  "timestamp": 1234567890,
  "version": "1.0"
}
```

---

### 4. Clear All Modifiers

**Endpoint**: `POST /api/v1/modifiers/clear`

**Description**: Remove all modifiers from the stack.

**Success Response** (200):
```json
{
  "success": true,
  "data": {
    "cleared": 2,
    "stackCount": 0
  },
  "timestamp": 1234567890,
  "version": "1.0"
}
```

---

### 5. Update Modifier Parameters

**Endpoint**: `POST /api/v1/modifiers/update`

**Description**: Update parameters of an existing modifier.

**Request Body**:
```json
{
  "type": "speed",
  "params": {
    "multiplier": 1.5
  }
}
```

**Success Response** (200):
```json
{
  "success": true,
  "data": {
    "updated": true,
    "type": "speed",
    "params": {
      "multiplier": 1.5
    }
  },
  "timestamp": 1234567890,
  "version": "1.0"
}
```

---

## Type-Specific Parameters

### SpeedModifier

```json
{
  "type": "speed",
  "params": {
    "multiplier": 2.0  // Range: 0.1 - 3.0
  }
}
```

**Parameters**:
- `multiplier` (float): Speed scaling factor
  - `< 1.0`: Slow motion
  - `= 1.0`: Normal speed
  - `> 1.0`: Fast forward

---

### IntensityModifier

```json
{
  "type": "intensity",
  "params": {
    "source": "audio_rms",      // "constant" | "audio_rms" | "audio_beat_phase" | "sine_wave" | "triangle_wave"
    "baseIntensity": 0.8,       // Range: 0.0 - 1.0
    "depth": 0.5,               // Range: 0.0 - 1.0
    "frequency": 1.0            // Hz (for wave modes)
  }
}
```

**Parameters**:
- `source`: Modulation source
  - `constant`: Fixed scaling
  - `audio_rms`: Modulated by RMS energy
  - `audio_beat_phase`: Pulsing on beat phase
  - `sine_wave`: Time-based sine wave
  - `triangle_wave`: Time-based triangle wave
- `baseIntensity`: Base brightness scaling (0.0 = off, 1.0 = full)
- `depth`: Modulation amount (0.0 = no modulation, 1.0 = full modulation)
- `frequency`: Oscillation rate for wave modes (Hz)

---

### ColorShiftModifier

```json
{
  "type": "color_shift",
  "params": {
    "mode": "auto_rotate",      // "fixed" | "auto_rotate" | "audio_chroma" | "beat_pulse"
    "hueOffset": 128,           // Range: 0 - 255
    "rotationSpeed": 30.0       // Hue units per second
  }
}
```

**Parameters**:
- `mode`: Color shift mode
  - `fixed`: Static hue offset
  - `auto_rotate`: Continuously rotating hue
  - `audio_chroma`: Driven by audio chromagram (requires FEATURE_AUDIO_SYNC)
  - `beat_pulse`: Hue pulses on beats (requires FEATURE_AUDIO_SYNC)
- `hueOffset`: Hue offset in HSV color space (0-255)
- `rotationSpeed`: Rotation rate for `auto_rotate` mode (hue units/sec)

---

### MirrorModifier

```json
{
  "type": "mirror",
  "params": {
    "mode": "center_out"  // "left_to_right" | "right_to_left" | "center_out" | "kaleidoscope"
  }
}
```

**Parameters**:
- `mode`: Mirror mode
  - `left_to_right`: Mirror left half (0-79) to right half (80-159)
  - `right_to_left`: Mirror right half to left half
  - `center_out`: Mirror both halves identically from center
  - `kaleidoscope`: Alternating symmetry patterns

---

### GlitchModifier

```json
{
  "type": "glitch",
  "params": {
    "mode": "beat_sync",        // "pixel_flip" | "channel_shift" | "noise_burst" | "beat_sync"
    "intensity": 0.2,           // Range: 0.0 - 1.0
    "channelShift": 3           // Pixel offset (for channel_shift mode)
  }
}
```

**Parameters**:
- `mode`: Glitch mode
  - `pixel_flip`: Random pixel color inversions
  - `channel_shift`: RGB channel displacement
  - `noise_burst`: Random brightness noise
  - `beat_sync`: Glitch triggers on beats (requires FEATURE_AUDIO_SYNC)
- `intensity`: Glitch strength (0.0 = off, 1.0 = maximum chaos)
- `channelShift`: Pixel offset for RGB channel displacement

---

## WebSocket Commands

### Add Modifier

```json
{
  "cmd": "modifiers.add",
  "type": "speed",
  "params": {
    "multiplier": 2.0
  }
}
```

**Response**:
```json
{
  "event": "modifiers.added",
  "data": {
    "type": "speed",
    "stackCount": 1
  }
}
```

---

### Remove Modifier

```json
{
  "cmd": "modifiers.remove",
  "type": "speed"
}
```

**Response**:
```json
{
  "event": "modifiers.removed",
  "data": {
    "type": "speed",
    "stackCount": 0
  }
}
```

---

### Update Modifier

```json
{
  "cmd": "modifiers.update",
  "type": "intensity",
  "params": {
    "depth": 0.7
  }
}
```

**Response**:
```json
{
  "event": "modifiers.updated",
  "data": {
    "type": "intensity",
    "params": {
      "depth": 0.7
    }
  }
}
```

---

### Clear All Modifiers

```json
{
  "cmd": "modifiers.clear"
}
```

**Response**:
```json
{
  "event": "modifiers.cleared",
  "data": {
    "cleared": 2
  }
}
```

---

### Subscribe to Modifier State

```json
{
  "cmd": "modifiers.subscribe"
}
```

**State Broadcast** (sent on every change):
```json
{
  "event": "modifiers.state",
  "data": {
    "modifiers": [
      {
        "type": "speed",
        "enabled": true,
        "params": { "multiplier": 2.0 }
      }
    ],
    "count": 1,
    "memoryUsage": 48
  }
}
```

---

## Rate Limiting

All endpoints respect global rate limits:
- **HTTP**: 20 requests/second
- **WebSocket**: 50 messages/second

Exceeding limits returns `429 Too Many Requests`.

---

## Error Codes

| Code | Meaning | Description |
|------|---------|-------------|
| 400 | Bad Request | Invalid modifier type or parameters |
| 429 | Too Many Requests | Rate limit exceeded |
| 500 | Internal Server Error | Modifier initialization failed |

---

## Example Usage

### Scenario: Audio-Reactive Fire Effect

```bash
# 1. Set effect to Fire
curl -X POST http://lightwaveos.local/api/v1/effects/set \
  -H "Content-Type: application/json" \
  -d '{"effectId": 0}'

# 2. Add speed modifier (2x faster)
curl -X POST http://lightwaveos.local/api/v1/modifiers/add \
  -H "Content-Type: application/json" \
  -d '{
    "type": "speed",
    "params": { "multiplier": 2.0 }
  }'

# 3. Add audio-reactive intensity
curl -X POST http://lightwaveos.local/api/v1/modifiers/add \
  -H "Content-Type: application/json" \
  -d '{
    "type": "intensity",
    "params": {
      "source": "audio_beat_phase",
      "baseIntensity": 0.9,
      "depth": 0.4
    }
  }'

# 4. Add color rotation
curl -X POST http://lightwaveos.local/api/v1/modifiers/add \
  -H "Content-Type: application/json" \
  -d '{
    "type": "color_shift",
    "params": {
      "mode": "auto_rotate",
      "rotationSpeed": 20.0
    }
  }'

# Result: Fire effect at 2x speed, pulsing with beats, rotating hue
```

---

## Implementation Checklist

- [ ] Create `ModifierHandlers.h/cpp` in `src/network/webserver/handlers/`
- [ ] Implement REST endpoints in `ModifierHandlers.cpp`
- [ ] Add routes to `V1ApiRoutes.cpp`
- [ ] Create `WsModifierCommands.h/cpp` in `src/network/webserver/ws/`
- [ ] Implement WebSocket commands
- [ ] Update `WebServer.cpp` to register modifier routes
- [ ] Add modifier state persistence (NVS)
- [ ] Write integration tests

---

**Author**: visual-fx-architect agent
**Date**: 2026-01-07
**Status**: SPECIFICATION COMPLETE (Implementation pending)
