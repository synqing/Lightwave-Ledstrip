# LightwaveOS API v1 Reference

**Version:** 1.0.0
**Base URL:** `http://lightwaveos.local` or `http://<device-ip>`
**Protocol:** HTTP REST + WebSocket

## Table of Contents

1. [Introduction](#introduction)
2. [Response Format](#response-format)
3. [Error Codes](#error-codes)
4. [Rate Limiting](#rate-limiting)
5. [REST API Endpoints](#rest-api-endpoints)
6. [WebSocket Commands](#websocket-commands)
7. [Batch Operations](#batch-operations)
8. [Effect Metadata](#effect-metadata)
9. [Examples](#examples)

---

## Introduction

The LightwaveOS API v1 provides programmatic control over an ESP32-S3 LED control system featuring:

- **320 LEDs** across dual 160-LED WS2812 strips
- **47 visual effects** organized into 11 categories
- **Multi-zone composition** (up to 4 independent zones)
- **12 transition types** with 15 easing curves
- **CENTER ORIGIN constraint** - all effects originate from LED 79/80

This API is fully backward compatible with v0 endpoints and adds structured responses, comprehensive metadata, and WebSocket v1 parity.

---

## Response Format

All v1 API responses follow a standardized JSON structure.

### Success Response

```json
{
  "success": true,
  "data": {
    // Endpoint-specific data
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
```

### Error Response

```json
{
  "success": false,
  "error": {
    "code": "RATE_LIMITED",
    "message": "Too many requests, please slow down",
    "field": "effectId"  // Optional - field that caused error
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
```

---

## Error Codes

| Code | HTTP Status | Description |
|------|-------------|-------------|
| `INVALID_JSON` | 400 | Request body is not valid JSON |
| `MISSING_FIELD` | 400 | Required field is missing from request |
| `OUT_OF_RANGE` | 400 | Parameter value exceeds valid range |
| `SCHEMA_MISMATCH` | 400 | Request doesn't match expected schema |
| `RATE_LIMITED` | 429 | Too many requests, client is rate limited |
| `INTERNAL_ERROR` | 500 | Server-side error occurred |

---

## Rate Limiting

To protect device stability, the API enforces rate limits per client IP address.

| Protocol | Limit | Window | Penalty |
|----------|-------|--------|---------|
| **HTTP** | 20 requests/sec | 1 second | 429 status |
| **WebSocket** | 50 messages/sec | 1 second | Error message |
| **Block Duration** | N/A | 5 seconds | After limit breach |

**Rate Limit Headers:**
- Not currently exposed, but client should implement exponential backoff on 429 responses

**Best Practices:**
- Use WebSocket for real-time updates (higher rate limit)
- Use batch operations to reduce request count
- Implement client-side debouncing for UI controls

---

## REST API Endpoints

### API Discovery

#### `GET /api/v1/`

Get API metadata and HATEOAS links.

**Response:**
```json
{
  "success": true,
  "data": {
    "name": "LightwaveOS",
    "apiVersion": "1.0.0",
    "description": "ESP32-S3 LED Control System",
    "hardware": {
      "ledsTotal": 320,
      "strips": 2,
      "centerPoint": 79,
      "maxZones": 4
    },
    "_links": {
      "self": "/api/v1/",
      "device": "/api/v1/device/status",
      "effects": "/api/v1/effects",
      "parameters": "/api/v1/parameters",
      "transitions": "/api/v1/transitions/types",
      "batch": "/api/v1/batch",
      "websocket": "ws://lightwaveos.local/ws"
    }
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
```

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v1/
```

---

### Device Endpoints

#### `GET /api/v1/device/status`

Get device runtime status.

**Response:**
```json
{
  "success": true,
  "data": {
    "uptime": 3600,
    "freeHeap": 245000,
    "heapSize": 327680,
    "cpuFreq": 240,
    "network": {
      "connected": true,
      "apMode": false,
      "ip": "192.168.1.100",
      "rssi": -45
    },
    "wsClients": 2
  }
}
```

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v1/device/status
```

---

#### `GET /api/v1/device/info`

Get device hardware and firmware information.

**Response:**
```json
{
  "success": true,
  "data": {
    "firmware": "1.0.0",
    "board": "ESP32-S3-DevKitC-1",
    "sdk": "v4.4.2",
    "flashSize": 8388608,
    "sketchSize": 1234567,
    "freeSketch": 6666666
  }
}
```

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v1/device/info
```

---

### Effects Endpoints

#### `GET /api/v1/effects`

List all available effects with optional pagination and metadata.

**Query Parameters:**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `start` | int | 0 | Starting effect ID |
| `count` | int | 20 | Number of effects to return |
| `details` | bool | false | Include full metadata (description, feature flags) |

**Response:**
```json
{
  "success": true,
  "data": {
    "total": 47,
    "start": 0,
    "count": 5,
    "effects": [
      {
        "id": 0,
        "name": "Fire",
        "category": "Classic",
        "categoryId": 0,
        "centerOrigin": true,
        "description": "Realistic fire simulation radiating from center",
        "usesSpeed": true,
        "usesPalette": true,
        "zoneAware": true,
        "dualStrip": false,
        "physicsBased": false
      }
    ],
    "categories": [
      {"id": 0, "name": "Classic"},
      {"id": 1, "name": "Shockwave"},
      {"id": 2, "name": "LGP Interference"}
    ]
  }
}
```

**cURL Examples:**
```bash
# Get first 20 effects (basic info)
curl http://lightwaveos.local/api/v1/effects

# Get effects 10-15 with full details
curl "http://lightwaveos.local/api/v1/effects?start=10&count=5&details=true"
```

---

#### `GET /api/v1/effects/current`

Get currently active effect with parameters.

**Response:**
```json
{
  "success": true,
  "data": {
    "effectId": 5,
    "name": "Shockwave",
    "brightness": 200,
    "speed": 15,
    "paletteId": 3
  }
}
```

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v1/effects/current
```

---

#### `GET /api/v1/effects/metadata`

Get detailed metadata for a specific effect.

**Query Parameters:**

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `id` | int | Yes | Effect ID (0-46) |

**Response:**
```json
{
  "success": true,
  "data": {
    "effectId": 9,
    "name": "Holographic Interference",
    "category": "LGP Interference",
    "categoryId": 2,
    "description": "Holographic interference patterns",
    "features": {
      "centerOrigin": true,
      "usesSpeed": true,
      "usesPalette": true,
      "zoneAware": false,
      "dualStrip": true,
      "physicsBased": true,
      "audioReactive": false
    },
    "featureFlags": 55
  }
}
```

**cURL Example:**
```bash
curl "http://lightwaveos.local/api/v1/effects/metadata?id=9"
```

---

#### `POST /api/v1/effects/set`

Set the active effect (triggers transition if enabled).

**Request Body:**
```json
{
  "effectId": 12
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "effectId": 12,
    "name": "Dual Wave Collision"
  }
}
```

**cURL Example:**
```bash
curl -X POST http://lightwaveos.local/api/v1/effects/set \
  -H "Content-Type: application/json" \
  -d '{"effectId": 12}'
```

---

### Parameters Endpoints

#### `GET /api/v1/parameters`

Get all current parameter values.

**Response:**
```json
{
  "success": true,
  "data": {
    "brightness": 200,
    "speed": 20,
    "paletteId": 5,
    "intensity": 128,
    "saturation": 255,
    "complexity": 64,
    "variation": 32
  }
}
```

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v1/parameters
```

---

#### `POST /api/v1/parameters`

Set one or more parameters. Only include fields you want to update.

**Request Body:**
```json
{
  "brightness": 180,
  "speed": 25,
  "paletteId": 7
}
```

**Parameter Ranges:**

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| `brightness` | uint8 | 0-255 | Global brightness |
| `speed` | uint8 | 1-50 | Animation speed |
| `paletteId` | uint8 | 0-N | Color palette (see `/api/palettes`) |
| `intensity` | uint8 | 0-255 | Effect intensity |
| `saturation` | uint8 | 0-255 | Color saturation |
| `complexity` | uint8 | 0-255 | Pattern complexity |
| `variation` | uint8 | 0-255 | Effect variation |

**Response:**
```json
{
  "success": true,
  "data": {}
}
```

**cURL Example:**
```bash
curl -X POST http://lightwaveos.local/api/v1/parameters \
  -H "Content-Type: application/json" \
  -d '{"brightness": 180, "speed": 25}'
```

---

### Transitions Endpoints

#### `GET /api/v1/transitions/types`

Get available transition types and easing curves.

**Response:**
```json
{
  "success": true,
  "data": {
    "types": [
      {
        "id": 0,
        "name": "FADE",
        "description": "CENTER ORIGIN crossfade - radiates from center",
        "centerOrigin": true
      },
      {
        "id": 1,
        "name": "WIPE_OUT",
        "description": "Wipe from center outward",
        "centerOrigin": true
      },
      {
        "id": 3,
        "name": "DISSOLVE",
        "description": "Random pixel transition",
        "centerOrigin": false
      }
    ],
    "easingCurves": [
      {"id": 0, "name": "LINEAR"},
      {"id": 1, "name": "IN_QUAD"},
      {"id": 3, "name": "IN_OUT_QUAD"},
      {"id": 7, "name": "IN_ELASTIC"},
      {"id": 8, "name": "OUT_ELASTIC"}
    ]
  }
}
```

**Transition Types:**

| ID | Name | Description | Center Origin |
|----|------|-------------|---------------|
| 0 | FADE | Crossfade radiating from center | Yes |
| 1 | WIPE_OUT | Wipe from center to edges | Yes |
| 2 | WIPE_IN | Wipe from edges to center | Yes |
| 3 | DISSOLVE | Random pixel transition | No |
| 4 | PHASE_SHIFT | Frequency-based morph | No |
| 5 | PULSEWAVE | Concentric energy pulses | Yes |
| 6 | IMPLOSION | Particles collapse to center | Yes |
| 7 | IRIS | Mechanical aperture from center | Yes |
| 8 | NUCLEAR | Chain reaction from center | Yes |
| 9 | STARGATE | Event horizon portal at center | Yes |
| 10 | KALEIDOSCOPE | Symmetric crystal patterns | Yes |
| 11 | MANDALA | Sacred geometry from center | Yes |

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v1/transitions/types
```

---

#### `GET /api/v1/transitions/config`

Get current transition configuration.

**Response:**
```json
{
  "success": true,
  "data": {
    "enabled": true,
    "defaultDuration": 1000,
    "defaultType": 0,
    "defaultEasing": 3
  }
}
```

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v1/transitions/config
```

---

#### `POST /api/v1/transitions/config`

Configure transition behavior.

**Request Body:**
```json
{
  "enabled": true
}
```

**Response:**
```json
{
  "success": true,
  "data": {}
}
```

**cURL Example:**
```bash
curl -X POST http://lightwaveos.local/api/v1/transitions/config \
  -H "Content-Type: application/json" \
  -d '{"enabled": true}'
```

---

#### `POST /api/v1/transitions/trigger`

Manually trigger a transition to a new effect.

**Request Body:**
```json
{
  "toEffect": 15,
  "type": 5,
  "duration": 2000
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `toEffect` | uint8 | Yes | Target effect ID (0-46) |
| `type` | uint8 | No | Transition type (0-11), default 0 |
| `duration` | uint32 | No | Duration in ms, default 1000 |

**Response:**
```json
{
  "success": true,
  "data": {
    "effectId": 15,
    "name": "Radial Star",
    "transitionType": 5,
    "duration": 2000
  }
}
```

**cURL Example:**
```bash
curl -X POST http://lightwaveos.local/api/v1/transitions/trigger \
  -H "Content-Type: application/json" \
  -d '{"toEffect": 15, "type": 5, "duration": 2000}'
```

---

### Batch Operations

#### `POST /api/v1/batch`

Execute multiple operations in a single request (max 10 operations).

**Request Body:**
```json
{
  "operations": [
    {
      "action": "setBrightness",
      "value": 200
    },
    {
      "action": "setSpeed",
      "value": 30
    },
    {
      "action": "setEffect",
      "effectId": 9
    }
  ]
}
```

**Supported Actions:**

| Action | Parameters | Description |
|--------|------------|-------------|
| `setBrightness` | `value` (0-255) | Set global brightness |
| `setSpeed` | `value` (1-50) | Set animation speed |
| `setEffect` | `effectId` (0-46) | Change effect |
| `setPalette` | `paletteId` | Change color palette |
| `setZoneEffect` | `zoneId`, `effectId` | Set zone-specific effect |
| `setZoneBrightness` | `zoneId`, `brightness` | Set zone brightness |
| `setZoneSpeed` | `zoneId`, `speed` | Set zone speed |

**Response:**
```json
{
  "success": true,
  "data": {
    "processed": 3,
    "failed": 0,
    "results": [
      {"action": "setBrightness", "success": true},
      {"action": "setSpeed", "success": true},
      {"action": "setEffect", "success": true}
    ]
  }
}
```

**cURL Example:**
```bash
curl -X POST http://lightwaveos.local/api/v1/batch \
  -H "Content-Type: application/json" \
  -d '{
    "operations": [
      {"action": "setBrightness", "value": 200},
      {"action": "setSpeed", "value": 30},
      {"action": "setEffect", "effectId": 9}
    ]
  }'
```

---

## WebSocket Commands

**WebSocket URL:** `ws://lightwaveos.local/ws`

All WebSocket messages use JSON format with a `type` field.

### Connection

```javascript
const ws = new WebSocket('ws://lightwaveos.local/ws');

ws.onopen = () => {
  console.log('Connected to LightwaveOS');
};

ws.onmessage = (event) => {
  const data = JSON.parse(event.data);
  console.log('Received:', data);
};
```

---

### Command: `transition.getTypes`

Get available transition types (same as REST endpoint).

**Send:**
```json
{
  "type": "transition.getTypes"
}
```

**Receive:**
```json
{
  "type": "transition.types",
  "success": true,
  "types": [
    {"id": 0, "name": "FADE"},
    {"id": 1, "name": "WIPE_OUT"}
  ]
}
```

**JavaScript Example:**
```javascript
ws.send(JSON.stringify({
  type: "transition.getTypes"
}));
```

---

### Command: `transition.trigger`

Trigger a transition to a new effect.

**Send:**
```json
{
  "type": "transition.trigger",
  "toEffect": 12
}
```

**Receive:**
Status broadcast to all clients.

**JavaScript Example:**
```javascript
ws.send(JSON.stringify({
  type: "transition.trigger",
  toEffect: 12
}));
```

---

### Command: `transition.config`

Configure transition settings.

**Send:**
```json
{
  "type": "transition.config",
  "enabled": true
}
```

**Receive:**
Status broadcast to all clients.

**JavaScript Example:**
```javascript
ws.send(JSON.stringify({
  type: "transition.config",
  enabled: true
}));
```

---

### Command: `device.getStatus`

Get device status over WebSocket.

**Send:**
```json
{
  "type": "device.getStatus"
}
```

**Receive:**
```json
{
  "type": "device.status",
  "success": true,
  "data": {
    "uptime": 3600,
    "freeHeap": 245000,
    "cpuFreq": 240,
    "wsClients": 2
  }
}
```

**JavaScript Example:**
```javascript
ws.send(JSON.stringify({
  type: "device.getStatus"
}));
```

---

### Command: `effects.getCurrent`

Get current effect over WebSocket.

**Send:**
```json
{
  "type": "effects.getCurrent"
}
```

**Receive:**
```json
{
  "type": "effects.current",
  "success": true,
  "data": {
    "effectId": 5,
    "name": "Shockwave",
    "brightness": 200,
    "speed": 15,
    "paletteId": 3
  }
}
```

**JavaScript Example:**
```javascript
ws.send(JSON.stringify({
  type: "effects.getCurrent"
}));
```

---

### Command: `effects.getMetadata`

Get effect metadata over WebSocket.

**Send:**
```json
{
  "type": "effects.getMetadata",
  "effectId": 9
}
```

**Receive:**
```json
{
  "type": "effects.metadata",
  "success": true,
  "data": {
    "effectId": 9,
    "name": "Holographic Interference",
    "category": "LGP Interference",
    "categoryId": 2,
    "description": "Holographic interference patterns",
    "features": {
      "centerOrigin": true,
      "usesSpeed": true,
      "usesPalette": true,
      "zoneAware": false,
      "dualStrip": true,
      "physicsBased": true,
      "audioReactive": false
    }
  }
}
```

**JavaScript Example:**
```javascript
ws.send(JSON.stringify({
  type: "effects.getMetadata",
  effectId: 9
}));
```

---

### Command: `effects.getCategories`

Get effect category list.

**Send:**
```json
{
  "type": "effects.getCategories"
}
```

**Receive:**
```json
{
  "type": "effects.categories",
  "success": true,
  "categories": [
    {"id": 0, "name": "Classic"},
    {"id": 1, "name": "Shockwave"},
    {"id": 2, "name": "LGP Interference"}
  ]
}
```

**JavaScript Example:**
```javascript
ws.send(JSON.stringify({
  type: "effects.getCategories"
}));
```

---

### Command: `parameters.get`

Get all current parameters.

**Send:**
```json
{
  "type": "parameters.get"
}
```

**Receive:**
```json
{
  "type": "parameters",
  "success": true,
  "data": {
    "brightness": 200,
    "speed": 20,
    "paletteId": 5,
    "intensity": 128,
    "saturation": 255,
    "complexity": 64,
    "variation": 32
  }
}
```

**JavaScript Example:**
```javascript
ws.send(JSON.stringify({
  type: "parameters.get"
}));
```

---

### Command: `batch`

Execute batch operations over WebSocket.

**Send:**
```json
{
  "type": "batch",
  "operations": [
    {"action": "setBrightness", "value": 200},
    {"action": "setEffect", "effectId": 12}
  ]
}
```

**Receive:**
```json
{
  "type": "batch.result",
  "success": true,
  "processed": 2,
  "failed": 0
}
```

**JavaScript Example:**
```javascript
ws.send(JSON.stringify({
  type: "batch",
  operations: [
    {action: "setBrightness", value: 200},
    {action: "setEffect", effectId: 12}
  ]
}));
```

---

## Effect Metadata

### Effect Categories

| ID | Name | Description |
|----|------|-------------|
| 0 | Classic | Traditional LED effects (Fire, Ocean, Wave) |
| 1 | Shockwave | Energy pulses from center |
| 2 | LGP Interference | Wave interference patterns |
| 3 | LGP Geometric | Geometric patterns (Diamond, Ring, Star) |
| 4 | LGP Advanced | Advanced optical effects (Moire, Vortex, Fresnel) |
| 5 | LGP Organic | Nature-inspired patterns (Aurora, Bioluminescence) |
| 6 | LGP Quantum | Quantum physics simulations |
| 7 | LGP Color Mixing | Chromatic effects |
| 8 | LGP Physics | Physics-based simulations |
| 9 | LGP Novel Physics | Experimental physics effects |
| 10 | Audio Reactive | Audio-responsive effects |

### Feature Flags

| Flag | Bit | Description |
|------|-----|-------------|
| `centerOrigin` | 0x01 | Originates from LED 79/80 (center) |
| `usesSpeed` | 0x02 | Responds to speed parameter |
| `usesPalette` | 0x04 | Uses current color palette |
| `zoneAware` | 0x08 | Compatible with zone system |
| `dualStrip` | 0x10 | Designed for dual-strip hardware |
| `physicsBased` | 0x20 | Uses physics simulation |
| `audioReactive` | 0x40 | Responds to audio input |

### Complete Effect List

| ID | Name | Category | Center Origin |
|----|------|----------|---------------|
| 0 | Fire | Classic | Yes |
| 1 | Ocean | Classic | Yes |
| 2 | Wave | Classic | Yes |
| 3 | Ripple | Classic | Yes |
| 4 | Sinelon | Classic | Yes |
| 5 | Shockwave | Shockwave | Yes |
| 6 | Collision | Shockwave | Yes |
| 7 | Gravity Well | Shockwave | Yes |
| 9 | Holographic Interference | LGP Interference | Yes |
| 10 | Standing Wave | LGP Interference | Yes |
| 11 | Interference Beam | LGP Interference | Yes |
| 12 | Dual Wave Collision | LGP Interference | Yes |
| 13 | Diamond Lattice | LGP Geometric | Yes |
| 14 | Concentric Ring | LGP Geometric | Yes |
| 15 | Radial Star | LGP Geometric | Yes |
| 16 | Moire Pattern | LGP Advanced | Yes |
| 17 | Radial Ripple | LGP Advanced | Yes |
| 18 | Vortex Spiral | LGP Advanced | Yes |
| 19 | Chromatic Shear | LGP Advanced | Yes |
| 20 | Fresnel Zone | LGP Advanced | Yes |
| 21 | Photonic Crystal | LGP Advanced | Yes |
| 22 | Aurora | LGP Organic | Yes |
| 23 | Bioluminescence | LGP Organic | Yes |
| 24 | Plasma Membrane | LGP Organic | Yes |
| 25-33 | Quantum effects | LGP Quantum | Yes |
| 34-35 | Color mixing | LGP Color Mixing | Yes |
| 36-41 | Physics effects | LGP Physics | Yes |
| 42-46 | Novel physics | LGP Novel Physics | Yes |

---

## Examples

### Change Effect with Transition

```bash
# Set effect with automatic transition
curl -X POST http://lightwaveos.local/api/v1/effects/set \
  -H "Content-Type: application/json" \
  -d '{"effectId": 9}'

# Trigger custom transition
curl -X POST http://lightwaveos.local/api/v1/transitions/trigger \
  -H "Content-Type: application/json" \
  -d '{
    "toEffect": 9,
    "type": 5,
    "duration": 2000
  }'
```

### Batch Update Parameters

```bash
curl -X POST http://lightwaveos.local/api/v1/batch \
  -H "Content-Type: application/json" \
  -d '{
    "operations": [
      {"action": "setEffect", "effectId": 13},
      {"action": "setBrightness", "value": 220},
      {"action": "setSpeed", "value": 25},
      {"action": "setPalette", "paletteId": 4}
    ]
  }'
```

### WebSocket Real-Time Control

```javascript
const ws = new WebSocket('ws://lightwaveos.local/ws');

ws.onopen = () => {
  // Get current status
  ws.send(JSON.stringify({
    type: "effects.getCurrent"
  }));

  // Change effect
  ws.send(JSON.stringify({
    type: "setEffect",
    effectId: 15
  }));

  // Adjust brightness
  ws.send(JSON.stringify({
    type: "setBrightness",
    value: 200
  }));
};

ws.onmessage = (event) => {
  const data = JSON.parse(event.data);

  if (data.type === "effects.current") {
    console.log("Current effect:", data.data.name);
  }

  if (data.type === "status") {
    console.log("Status update:", data);
  }
};
```

### Get Effect Metadata

```bash
# Get detailed info for effect 9
curl "http://lightwaveos.local/api/v1/effects/metadata?id=9"

# List all effects with details
curl "http://lightwaveos.local/api/v1/effects?details=true"
```

### Monitor Device Status

```javascript
const ws = new WebSocket('ws://lightwaveos.local/ws');

ws.onmessage = (event) => {
  const data = JSON.parse(event.data);

  // Periodic status broadcasts
  if (data.type === "status") {
    console.log("Uptime:", data.uptime);
    console.log("Free heap:", data.freeHeap);
    console.log("Current effect:", data.effect);
  }
};
```

---

## Notes

### CENTER ORIGIN Constraint

All effects must originate from LED 79/80 (center point). This is a hardware requirement for the Light Guide Plate (LGP) design.

Effects propagate:
- **OUTWARD** from center (79/80) to edges (0/159)
- **INWARD** from edges (0/159) to center (79/80)

Linear left-to-right or right-to-left patterns are not supported.

### Backward Compatibility

v1 API endpoints coexist with v0 endpoints:

- `/api/v1/*` - New structured API
- `/api/*` - Legacy v0 endpoints (still functional)
- WebSocket supports both `type` (v1) and `cmd` (v0) formats

### Performance Considerations

- Target frame rate: 120 FPS
- WebSocket recommended for real-time updates
- Batch operations reduce HTTP overhead
- Rate limiting protects device stability

---

**Documentation Version:** 1.0.0
**Last Updated:** 2025-12-16
**API Base:** LightwaveOS v1.0.0
