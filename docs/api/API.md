# LightwaveOS Web API Reference

Complete reference for REST and WebSocket APIs.

**Base URL**: `http://lightwaveos.local` or `http://<device-ip>`

---

## REST API Endpoints

### System Status

#### GET /api/status
Get current system state.

**Response**:
```json
{
  "effect": 0,
  "effectName": "Fire",
  "brightness": 128,
  "palette": 5,
  "paletteName": "Ocean",
  "speed": 25,
  "fps": 120,
  "freeHeap": 180000,
  "uptime": 3600,
  "wifi": {
    "ssid": "MyNetwork",
    "rssi": -45
  }
}
```

---

### Effect Control

#### POST /api/effect
Set the current effect.

**Request Body** (JSON):
```json
{
  "effect": 5
}
```

**Response**: 200 OK on success

#### GET /api/effects
Get list of available effects (paginated).

**Query Parameters**:
- `start` (optional): Starting index (default: 0)
- `count` (optional): Number of effects to return (default: 20)

**Response**:
```json
{
  "effects": [
    { "id": 0, "name": "Fire", "type": "standard" },
    { "id": 1, "name": "Ocean", "type": "standard" }
  ],
  "total": 45,
  "start": 0,
  "count": 20
}
```

---

### Brightness & Speed

#### POST /api/brightness
Set global brightness.

**Request Body** (JSON):
```json
{
  "brightness": 128
}
```

**Range**: 0-255

#### POST /api/speed
Set animation speed.

**Request Body** (JSON):
```json
{
  "speed": 25
}
```

**Range**: 1-50

---

### Palette Control

#### POST /api/palette
Set current color palette.

**Request Body** (JSON):
```json
{
  "palette": 5
}
```

#### GET /api/palettes
Get list of available palettes.

**Response**:
```json
{
  "palettes": [
    { "id": 0, "name": "Lava" },
    { "id": 1, "name": "Ocean" },
    { "id": 2, "name": "Forest" }
  ]
}
```

---

### Pipeline Parameters

#### POST /api/pipeline
Set effect pipeline parameters.

**Request Body** (JSON):
```json
{
  "intensity": 200,
  "saturation": 255,
  "complexity": 128,
  "variation": 64
}
```

All parameters are optional. Range: 0-255.

---

### Transition Control

#### POST /api/transitions
Configure effect transitions.

**Request Body** (JSON):
```json
{
  "enabled": true,
  "type": "crossfade"
}
```

**Transition Types**: `crossfade`, `wipe`, `dissolve`, `instant`

---

### Zone Composer

#### GET /api/zone/status
Get zone system status.

**Response**:
```json
{
  "enabled": true,
  "zoneCount": 3,
  "segments": [
    {
      "zoneId": 0,
      "s1LeftStart": 65,
      "s1LeftEnd": 79,
      "s1RightStart": 80,
      "s1RightEnd": 94,
      "totalLeds": 30
    }
  ],
  "zones": [
    {
      "id": 0,
      "enabled": true,
      "effectId": 5,
      "effectName": "Fire",
      "brightness": 200,
      "speed": 25
    }
  ]
}
```

#### POST /api/zone/enable
Enable or disable zone system.

**Request Body**:
```json
{
  "enabled": true
}
```

#### POST /api/v1/zones/layout
Set zone layout using custom segment definitions. **Note**: This replaces the deprecated `/api/zone/count` endpoint.

**Request Body**:
```json
{
  "zones": [
    {
      "zoneId": 0,
      "s1LeftStart": 65,
      "s1LeftEnd": 79,
      "s1RightStart": 80,
      "s1RightEnd": 94
    }
  ]
}
```

**See**: [API_V1.md](API_V1.md) for full documentation of zone layout endpoints and validation rules.

#### POST /api/zone/config
Configure a specific zone.

**Request Body**:
```json
{
  "zoneId": 0,
  "effectId": 5,
  "enabled": true
}
```

#### POST /api/zone/brightness
Set zone-specific brightness.

**Request Body**:
```json
{
  "zoneId": 0,
  "brightness": 200
}
```

#### POST /api/zone/speed
Set zone-specific speed.

**Request Body**:
```json
{
  "zoneId": 0,
  "speed": 30
}
```

#### GET /api/zone/presets
Get available zone presets.

**Response**:
```json
{
  "presets": [
    { "id": 0, "name": "Single" },
    { "id": 1, "name": "Dual" },
    { "id": 2, "name": "Triple" },
    { "id": 3, "name": "Quad" },
    { "id": 4, "name": "Alternating" }
  ]
}
```

#### POST /api/zone/preset/load
Load a zone preset.

**Request Body**:
```json
{
  "presetId": 2
}
```

#### POST /api/zone/config/save
Save current zone configuration to NVS.

#### POST /api/zone/config/load
Load zone configuration from NVS.

---

### Enhancement Engines

*Requires `FEATURE_ENHANCEMENT_ENGINES=1`*

#### POST /api/enhancement/color/blend
Set palette cross-blending.

**Request Body**:
```json
{
  "blend": 0.5
}
```

**Range**: 0.0-1.0

#### POST /api/enhancement/color/diffusion
Set color diffusion strength.

**Request Body**:
```json
{
  "diffusion": 128
}
```

#### POST /api/enhancement/color/rotation
Enable temporal palette rotation.

**Request Body**:
```json
{
  "enabled": true,
  "speed": 1.0
}
```

#### POST /api/enhancement/motion/phase
Set phase offset.

**Request Body**:
```json
{
  "phase": 0.5
}
```

#### POST /api/enhancement/motion/auto-rotate
Enable auto-rotation.

**Request Body**:
```json
{
  "enabled": true,
  "speed": 1.0
}
```

#### GET /api/enhancement/status
Get enhancement engines status.

**Response**:
```json
{
  "colorEngineEnabled": true,
  "motionEngineEnabled": true,
  "blendingEngineEnabled": false
}
```

---

### Network Management

#### GET /api/network/scan
Scan for available WiFi networks.

**Response**:
```json
{
  "networks": [
    { "ssid": "MyNetwork", "rssi": -45, "secure": true }
  ]
}
```

#### POST /api/network/connect
Connect to a WiFi network.

**Request Body**:
```json
{
  "ssid": "MyNetwork",
  "password": "secret123"
}
```

---

### Firmware Update

#### POST /update
Upload new firmware (OTA update).

**Request**: `multipart/form-data` with firmware binary

**Response**: Device reboots on success

---

## WebSocket API

Connect to: `ws://lightwaveos.local/ws`

### Message Format

All messages use JSON with a `type` field:
```json
{
  "type": "commandType",
  "param": "value"
}
```

### Commands

#### Effect Control

```json
// Set specific effect
{ "type": "setEffect", "effectId": 5 }

// Next/Previous effect
{ "type": "nextEffect" }
{ "type": "prevEffect" }
```

#### Parameters

```json
// Set brightness
{ "type": "setBrightness", "value": 128 }

// Set speed
{ "type": "setSpeed", "value": 25 }

// Set palette
{ "type": "setPalette", "paletteId": 3 }

// Set pipeline parameters
{
  "type": "setPipeline",
  "intensity": 200,
  "saturation": 255,
  "complexity": 128,
  "variation": 64
}

// Toggle transitions
{ "type": "toggleTransitions", "enabled": true }
```

#### Zone Control

```json
// Enable/disable zone system
{ "type": "zone.enable", "enable": true }

// Set zone count
{ "type": "zone.setCount", "count": 4 }

// Set zone effect
{ "type": "zone.setEffect", "zoneId": 0, "effectId": 5 }

// Enable/disable specific zone
{ "type": "zone.enableZone", "zoneId": 0, "enabled": true }

// Load preset
{ "type": "zone.loadPreset", "presetId": 2 }

// Save/load config
{ "type": "zone.save" }
{ "type": "zone.load" }

// Zone brightness/speed
{ "type": "zone.setBrightness", "zoneId": 0, "brightness": 200 }
{ "type": "zone.setSpeed", "zoneId": 0, "speed": 30 }
```

### Legacy Format (Deprecated)

For backward compatibility, the legacy `cmd` format is still supported:

```json
{ "cmd": "effect", "value": 5 }
{ "cmd": "brightness", "value": 128 }
{ "cmd": "palette", "value": 3 }
```

---

## Status Updates

The server broadcasts status updates over WebSocket when parameters change:

```json
{
  "type": "status",
  "effect": 5,
  "brightness": 128,
  "palette": 3,
  "zoneEnabled": true,
  "zoneCount": 4
}
```

---

## Error Handling

### REST Errors

| Status Code | Meaning |
|-------------|---------|
| 200 | Success |
| 400 | Bad Request (invalid JSON or parameters) |
| 404 | Endpoint not found |
| 500 | Server error |

### WebSocket Errors

Invalid messages are silently ignored. Check serial output for debugging.

---

## Rate Limits

- REST: No hard limit, but avoid more than 10 requests/second
- WebSocket: No limit, designed for real-time control

---

## CORS

For cross-origin requests (external dashboard), the server returns appropriate headers:

```
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, POST, OPTIONS
Access-Control-Allow-Headers: Content-Type
```

---

## Examples

### cURL Examples

```bash
# Get status
curl http://lightwaveos.local/api/status

# Set effect
curl -X POST http://lightwaveos.local/api/effect \
  -H "Content-Type: application/json" \
  -d '{"effect": 5}'

# Set brightness
curl -X POST http://lightwaveos.local/api/brightness \
  -H "Content-Type: application/json" \
  -d '{"brightness": 200}'
```

### JavaScript Example

```javascript
// WebSocket connection
const ws = new WebSocket('ws://lightwaveos.local/ws');

ws.onopen = () => {
  // Set effect
  ws.send(JSON.stringify({ type: 'setEffect', effectId: 5 }));

  // Set zone config
  ws.send(JSON.stringify({
    type: 'zone.setEffect',
    zoneId: 0,
    effectId: 10
  }));
};

ws.onmessage = (event) => {
  const data = JSON.parse(event.data);
  console.log('Status update:', data);
};
```
