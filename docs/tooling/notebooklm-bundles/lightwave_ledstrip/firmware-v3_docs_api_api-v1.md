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
10. [Related Documentation](#related-documentation)

**Subsection index (REST API Endpoints):**
- [API Discovery](#api-discovery)
- [Device Endpoints](#device-endpoints)
- [Effects Endpoints](#effects-endpoints)
- [Parameters Endpoints](#parameters-endpoints)
- [Transitions Endpoints](#transitions-endpoints)
- [Zones Endpoints](#zones-endpoints)
- [Batch Operations](#batch-operations)
- [Palettes Endpoints](#palettes-endpoints)
- [Audio Endpoints](#audio-endpoints)
- [Presets Endpoints](#presets-endpoints)
- [Debug Endpoints](#debug-endpoints)

---

## Introduction

The LightwaveOS API v1 provides programmatic control over an ESP32-S3 LED control system featuring:

- **320 LEDs** across dual 160-LED WS2812 strips
- **47 visual effects** organized into 11 categories
- **Multi-zone composition** (up to 3 independent zones)
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
      "maxZones": 3
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
      "apMode": true,
      "ip": "192.168.4.1",
      "rssi": 0
    },
    "wsClients": 2
  }
}
```

**cURL Example:**
```bash
curl http://192.168.4.1/api/v1/device/status
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
| `page` | int | 1 | Page number (1-based) |
| `limit` | int | 20 | Effects per page (max 150) |
| `offset` | int | - | Alternative to `page`: skip this many effects (converted to page internally) |
| `category` | int | - | Filter by category ID |
| `details` | bool | false | Include full metadata (description, feature flags) |

**Response:**
```json
{
  "success": true,
  "data": {
    "total": 47,
    "page": 1,
    "limit": 20,
    "pages": 3,
    "effects": [
      {
        "id": 0,
        "name": "Fire",
        "category": "Classic",
        "categoryId": 0,
        "centerOrigin": true,
        "description": "Realistic fire simulation radiating from centre",
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
curl http://192.168.4.1/api/v1/effects

# Get page 2 with 5 per page, full details
curl "http://192.168.4.1/api/v1/effects?page=2&limit=5&details=true"
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
    "hue": 0,
    "intensity": 128,
    "saturation": 255,
    "complexity": 64,
    "variation": 32,
    "mood": 160,
    "fadeAmount": 128
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
| `speed` | uint8 | 1-100 | Animation speed |
| `paletteId` | uint8 | 0-N | Colour palette (see `/api/v1/palettes`) |
| `hue` | uint8 | 0-255 | Hue offset |
| `intensity` | uint8 | 0-255 | Effect intensity |
| `saturation` | uint8 | 0-255 | Colour saturation |
| `complexity` | uint8 | 0-255 | Pattern complexity |
| `variation` | uint8 | 0-255 | Effect variation |
| `mood` | uint8 | 0-255 | Mood (0=reactive, 255=smooth). REST only. |
| `fadeAmount` | uint8 | 0-255 | Fade amount. REST only. |

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
| `toEffect` | uint16 | Yes | Target effect ID |
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

### Zones Endpoints

#### `GET /api/v1/zones`

Get all zone configurations.

**Response:**
```json
{
  "success": true,
  "data": {
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
      },
      {
        "zoneId": 1,
        "s1LeftStart": 20,
        "s1LeftEnd": 64,
        "s1RightStart": 95,
        "s1RightEnd": 139,
        "totalLeds": 90
      },
      {
        "zoneId": 2,
        "s1LeftStart": 0,
        "s1LeftEnd": 19,
        "s1RightStart": 140,
        "s1RightEnd": 159,
        "totalLeds": 40
      }
    ],
    "zones": [
      {
        "id": 0,
        "enabled": true,
        "effectId": 5,
        "effectName": "Shockwave",
        "brightness": 200,
        "speed": 20,
        "paletteId": 0,
        "blendMode": 0,
        "blendModeName": "Overwrite"
      }
    ],
    "presets": [
      {"id": 0, "name": "Unified"},
      {"id": 1, "name": "Dual Split"},
      {"id": 2, "name": "Triple Rings"},
      {"id": 3, "name": "Heartbeat Focus"}
    ]
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
```

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v1/zones
```

---

#### `POST /api/v1/zones/layout`

Set zone layout using custom segment definitions. This allows full runtime control over zone boundaries.

**Request Body:**
```json
{
  "zones": [
    {
      "zoneId": 0,
      "s1LeftStart": 65,
      "s1LeftEnd": 79,
      "s1RightStart": 80,
      "s1RightEnd": 94
    },
    {
      "zoneId": 1,
      "s1LeftStart": 20,
      "s1LeftEnd": 64,
      "s1RightStart": 95,
      "s1RightEnd": 139
    },
    {
      "zoneId": 2,
      "s1LeftStart": 0,
      "s1LeftEnd": 19,
      "s1RightStart": 140,
      "s1RightEnd": 159
    }
  ]
}
```

**Segment Field Definitions:**

| Field | Type | Range | Description |
|-------|------|-------|-------------|
| `zoneId` | uint8 | 0-2 | Zone identifier (must be sequential, starting from 0) |
| `s1LeftStart` | uint8 | 0-79 | Left segment start LED (toward LED 0) |
| `s1LeftEnd` | uint8 | 0-79 | Left segment end LED (inclusive, must be ≥ start) |
| `s1RightStart` | uint8 | 80-159 | Right segment start LED (toward LED 159) |
| `s1RightEnd` | uint8 | 80-159 | Right segment end LED (inclusive, must be ≥ start) |

**Validation Rules:**

1. **Centre-Origin Symmetry**: Left and right segments must be symmetric around the centre pair (LEDs 79/80):
   - Segment sizes must match: `(s1LeftEnd - s1LeftStart) == (s1RightEnd - s1RightStart)`
   - Distance from centre must match: `(79 - s1LeftEnd) == (s1RightStart - 80)`

2. **No Overlaps**: Zone segments must not overlap with each other.

3. **Complete Coverage**: All LEDs from 0-159 (per strip) must be assigned to exactly one zone.

4. **Centre-Outward Ordering**: Zones must be ordered from centre outward:
   - Zone 1 = innermost (zoneId: 0, closest to centre pair)
   - Zone N = outermost (at edges)

5. **Minimum Zone Size**: At least 1 LED per segment (left and right), minimum 2 LEDs total per zone.

6. **Centre Pair Inclusion**: At least one zone must include LED 79 or 80.

**Response:**
```json
{
  "success": true,
  "data": {
    "zoneCount": 3
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
```

**Error Response (Validation Failed):**
```json
{
  "success": false,
  "error": {
    "code": "INVALID_VALUE",
    "message": "Layout validation failed"
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
```

**cURL Example:**
```bash
curl -X POST http://lightwaveos.local/api/v1/zones/layout \
  -H "Content-Type: application/json" \
  -d '{
    "zones": [
      {"zoneId": 0, "s1LeftStart": 65, "s1LeftEnd": 79, "s1RightStart": 80, "s1RightEnd": 94},
      {"zoneId": 1, "s1LeftStart": 20, "s1LeftEnd": 64, "s1RightStart": 95, "s1RightEnd": 139},
      {"zoneId": 2, "s1LeftStart": 0, "s1LeftEnd": 19, "s1RightStart": 140, "s1RightEnd": 159}
    ]
  }'
```

---

#### `GET /api/v1/zones/{id}`

Get configuration for a specific zone.

**Path Parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `id` | uint8 | Zone ID (0-2) |

**Response:**
```json
{
  "success": true,
  "data": {
    "id": 0,
    "enabled": true,
    "effectId": 5,
    "effectName": "Shockwave",
    "brightness": 200,
    "speed": 20,
    "paletteId": 0,
    "blendMode": 0,
    "blendModeName": "OVERWRITE"
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
```

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v1/zones/0
```

---

#### `POST /api/v1/zones/{id}/effect`

Set the effect for a specific zone.

**Path Parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `id` | uint8 | Zone ID (0-2) |

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
    "zoneId": 0,
    "effectId": 12,
    "effectName": "Dual Wave Collision"
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
```

**cURL Example:**
```bash
curl -X POST http://lightwaveos.local/api/v1/zones/0/effect \
  -H "Content-Type: application/json" \
  -d '{"effectId": 12}'
```

---

#### `POST /api/v1/zones/{id}/brightness`

Set brightness for a specific zone.

**Request Body:**
```json
{
  "brightness": 220
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "zoneId": 0,
    "brightness": 220
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
```

---

#### `POST /api/v1/zones/{id}/speed`

Set speed for a specific zone.

**Request Body:**
```json
{
  "speed": 25
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "zoneId": 0,
    "speed": 25
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
```

---

#### `POST /api/v1/zones/{id}/palette`

Set palette for a specific zone.

**Request Body:**
```json
{
  "paletteId": 5
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "zoneId": 0,
    "paletteId": 5,
    "paletteName": "Sunset"
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
```

---

#### `POST /api/v1/zones/{id}/blend`

Set blend mode for a specific zone.

**Request Body:**
```json
{
  "blendMode": 1
}
```

**Blend Modes:**

| ID | Name | Description |
|----|------|-------------|
| 0 | Overwrite | Replace pixels in zone |
| 1 | Additive | Add to existing pixels (light accumulation) |
| 2 | Multiply | Multiply with existing pixels |
| 3 | Screen | Screen blend mode (lighten) |
| 4 | Overlay | Overlay blend (multiply if dark, screen if light) |
| 5 | Alpha | 50/50 blend |
| 6 | Lighten | Take brighter pixel |
| 7 | Darken | Take darker pixel |

**Response:**
```json
{
  "success": true,
  "data": {
    "zoneId": 0,
    "blendMode": 1,
    "blendModeName": "Additive"
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
```

---

#### `POST /api/v1/zones/{id}/enabled`

Enable or disable a specific zone.

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
  "data": {
    "zoneId": 0,
    "enabled": true
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
```

---

#### `POST /api/v1/zones/enabled`

Enable or disable the entire zone system.

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
  "data": {
    "enabled": true
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
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

### Palettes Endpoints

#### `GET /api/v1/palettes`

List all available colour palettes with optional filtering and pagination.

**Query Parameters:**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `category` | string | — | Filter by category: `artistic`, `scientific`, or `lgpOptimized` |
| `warm` | bool | — | When `true`, return only warm-toned palettes |
| `cool` | bool | — | When `true`, return only cool-toned palettes |
| `calm` | bool | — | When `true`, return only calm palettes |
| `vivid` | bool | — | When `true`, return only vivid palettes |
| `cvd` | bool | — | When `true`, return only colour-vision-deficiency-friendly palettes |
| `page` | int | 1 | Page number (1-based) |
| `limit` | int | 20 | Items per page (1–100) |
| `offset` | int | — | Alternative to `page`: zero-based item offset |

**Response:**
```json
{
  "success": true,
  "data": {
    "total": 75,
    "offset": 0,
    "limit": 20,
    "count": 20,
    "categories": {
      "artistic": 50,
      "scientific": 15,
      "lgpOptimized": 10
    },
    "palettes": [
      {
        "id": 0,
        "name": "Fire",
        "category": "artistic",
        "flags": {
          "warm": true,
          "cool": false,
          "calm": false,
          "vivid": true,
          "cvdFriendly": false,
          "whiteHeavy": false
        },
        "avgBrightness": 180,
        "maxBrightness": 255
      }
    ],
    "pagination": {
      "page": 1,
      "limit": 20,
      "total": 75,
      "pages": 4
    }
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
```

**cURL Examples:**
```bash
# List all palettes (first page)
curl http://lightwaveos.local/api/v1/palettes

# Filter to warm, vivid palettes only
curl "http://lightwaveos.local/api/v1/palettes?warm=true&vivid=true"

# Fetch all scientific palettes
curl "http://lightwaveos.local/api/v1/palettes?category=scientific&limit=100"
```

---

#### `GET /api/v1/palettes/current`

Get the currently active colour palette.

**Response:**
```json
{
  "success": true,
  "data": {
    "paletteId": 3,
    "name": "Ocean",
    "category": "artistic",
    "flags": {
      "warm": false,
      "cool": true,
      "calm": true,
      "vivid": false,
      "cvdFriendly": true
    },
    "avgBrightness": 140,
    "maxBrightness": 220
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
```

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v1/palettes/current
```

---

#### `POST /api/v1/palettes/set`

Set the active colour palette.

**Request Body:**
```json
{
  "paletteId": 5
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `paletteId` | uint8 | Yes | Palette ID (0–74) |

**Response:**
```json
{
  "success": true,
  "data": {
    "paletteId": 5,
    "name": "Aurora",
    "category": "artistic"
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
```

**Error Responses:**

| Condition | HTTP | Code | Description |
|-----------|------|------|-------------|
| `paletteId` out of range | 400 | `OUT_OF_RANGE` | Palette ID must be 0–74 |
| Invalid JSON | 400 | `INVALID_JSON` | Request body is not valid JSON |

**cURL Example:**
```bash
curl -X POST http://lightwaveos.local/api/v1/palettes/set \
  -H "Content-Type: application/json" \
  -d '{"paletteId": 5}'
```

---

### Audio Endpoints

The audio endpoints require the firmware to be built with `FEATURE_AUDIO_SYNC` enabled (all production builds). Requests to these endpoints when audio is unavailable return HTTP 503 with error code `FEATURE_DISABLED` or `AUDIO_UNAVAILABLE`.

#### `GET /api/v1/audio/parameters`

Get current audio DSP pipeline tuning and contract parameters. The response shape differs by audio backend.

**Response (PipelineCore backend — `esp32dev_audio_pipelinecore`):**
```json
{
  "success": true,
  "data": {
    "pipeline": {
      "dcAlpha": 0.995,
      "agcTargetRms": 0.25,
      "agcMinGain": 0.5,
      "agcMaxGain": 8.0,
      "agcAttack": 0.05,
      "agcRelease": 0.002,
      "agcClipReduce": 0.8,
      "agcIdleReturnRate": 0.001,
      "noiseFloorMin": 0.01,
      "noiseFloorRise": 0.1,
      "noiseFloorFall": 0.005,
      "gateStartFactor": 1.5,
      "gateRangeFactor": 3.0,
      "gateRangeMin": 0.05,
      "rmsDbFloor": -60.0,
      "rmsDbCeil": 0.0,
      "bandDbFloor": -60.0,
      "bandDbCeil": 0.0,
      "chromaDbFloor": -60.0,
      "chromaDbCeil": 0.0,
      "fluxScale": 1.0,
      "bandAttack": 0.3,
      "bandRelease": 0.05,
      "heavyBandAttack": 0.5,
      "heavyBandRelease": 0.02,
      "usePerBandNoiseFloor": false,
      "silenceHysteresisMs": 200,
      "silenceThreshold": 0.005,
      "perBandGains": [1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0],
      "perBandNoiseFloors": [0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01],
      "bins64Adaptive": {
        "scale": 1.0,
        "floor": 0.0,
        "rise": 0.3,
        "fall": 0.05,
        "decay": 0.98
      },
      "novelty": {
        "useSpectralFlux": true,
        "spectralFluxScale": 1.0
      }
    },
    "controlBus": {
      "alphaFast": 0.3,
      "alphaSlow": 0.05
    },
    "contract": {
      "audioStalenessMs": 500,
      "bpmMin": 60.0,
      "bpmMax": 200.0,
      "bpmTau": 4.0,
      "confidenceTau": 8.0,
      "phaseCorrectionGain": 0.1,
      "barCorrectionGain": 0.05,
      "beatsPerBar": 4,
      "beatUnit": 4
    },
    "state": {
      "rmsRaw": 0.12,
      "rmsMapped": 0.35,
      "rmsPreGain": 0.10,
      "fluxMapped": 0.28,
      "agcGain": 2.5,
      "dcEstimate": 0.001,
      "noiseFloor": 0.015,
      "minSample": -0.42,
      "maxSample": 0.45,
      "peakCentered": 0.87,
      "meanSample": 0.0002,
      "clipCount": 0
    },
    "capabilities": {
      "sampleRate": 44100,
      "hopSize": 256,
      "fftSize": 512,
      "goertzelWindow": 64,
      "bandCount": 8,
      "chromaCount": 12,
      "waveformPoints": 64
    }
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
```

**Response (ESV11 backend — `esp32dev_audio_esv11`):**
```json
{
  "success": true,
  "data": {
    "backend": "esv11",
    "seq": 4821,
    "contract": {
      "audioStalenessMs": 500,
      "bpmMin": 60.0,
      "bpmMax": 200.0,
      "bpmTau": 4.0,
      "confidenceTau": 8.0,
      "phaseCorrectionGain": 0.1,
      "barCorrectionGain": 0.05,
      "beatsPerBar": 4,
      "beatUnit": 4
    },
    "latest": {
      "rms": 0.22,
      "flux": 0.15,
      "bpm": 124.0,
      "tempoConfidence": 0.85,
      "phase01AtAudioT": 0.42,
      "beatTick": 183,
      "downbeatTick": 46,
      "beatInBar": 2,
      "beatStrength": 0.91
    },
    "capabilities": {
      "sampleRate": 32000,
      "hopSize": 256,
      "bins64": 64,
      "bandCount": 8,
      "chromaCount": 12,
      "waveformPoints": 64
    }
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
```

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v1/audio/parameters
```

---

#### `POST /api/v1/audio/parameters`

Replace audio DSP tuning parameters. All fields are optional; only supplied fields are applied.

**Request Body (PipelineCore backend):**
```json
{
  "pipeline": {
    "agcTargetRms": 0.3,
    "noiseFloorMin": 0.008,
    "bandAttack": 0.25,
    "bandRelease": 0.04,
    "perBandGains": [1.2, 1.0, 0.9, 0.9, 1.0, 1.1, 1.1, 1.2]
  },
  "contract": {
    "bpmMin": 70.0,
    "bpmMax": 180.0,
    "beatsPerBar": 4
  },
  "resetState": false
}
```

**Request Body (ESV11 backend):**
```json
{
  "contract": {
    "bpmMin": 70.0,
    "bpmMax": 180.0,
    "beatsPerBar": 4,
    "beatUnit": 4
  }
}
```

| Field | Type | Description |
|-------|------|-------------|
| `pipeline` | object | DSP pipeline tuning (PipelineCore only) |
| `contract` | object | Beat-tracking / tempo contract |
| `resetState` | bool | When `true`, resets DSP statistics (PipelineCore only) |

**Response:**
```json
{
  "success": true,
  "data": {
    "updated": ["pipeline", "contract"]
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
```

The `updated` array lists which top-level groups were modified (`pipeline`, `contract`, `state`).

**Error Responses:**

| Condition | HTTP | Code | Description |
|-----------|------|------|-------------|
| Invalid JSON | 400 | `INVALID_JSON` | Request body is not valid JSON |
| Missing `contract` key (ESV11) | 400 | `MISSING_FIELD` | ESV11 requires the `contract` wrapper |
| Audio unavailable | 503 | `SYSTEM_NOT_READY` | Audio actor not initialised |

**cURL Example:**
```bash
curl -X POST http://lightwaveos.local/api/v1/audio/parameters \
  -H "Content-Type: application/json" \
  -d '{"contract": {"bpmMin": 70.0, "bpmMax": 180.0}}'
```

---

#### `PATCH /api/v1/audio/parameters`

Partial update to audio DSP tuning parameters. Identical request/response contract to `POST /api/v1/audio/parameters`; both methods accept the same body and apply only the supplied fields.

**cURL Example:**
```bash
curl -X PATCH http://lightwaveos.local/api/v1/audio/parameters \
  -H "Content-Type: application/json" \
  -d '{"pipeline": {"agcTargetRms": 0.2}}'
```

---

#### `GET /api/v1/audio/state`

Get the current operational state of the audio actor.

**Response:**
```json
{
  "success": true,
  "data": {
    "state": "RUNNING",
    "capturing": true,
    "hopCount": 9234,
    "sampleIndex": 2367744,
    "stats": {
      "tickCount": 9234,
      "captureSuccess": 9230,
      "captureFail": 4
    },
    "controlBus": {
      "silentScale": 1.0,
      "isSilent": false,
      "tempoLocked": true,
      "tempoConfidence": 0.87,
      "style": 2,
      "styleConfidence": 0.72
    }
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
```

**State Values:**

| Value | Description |
|-------|-------------|
| `UNINITIALIZED` | Actor has not yet started |
| `INITIALIZING` | I2S and DSP pipeline starting up |
| `RUNNING` | Capturing and processing audio |
| `PAUSED` | Capture suspended via `POST /api/v1/audio/control` |
| `ERROR` | Unrecoverable audio error; device restart required |

The `controlBus` object is present only when `FEATURE_AUDIO_SYNC` is enabled and the renderer has a cached audio frame.

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v1/audio/state
```

---

#### `GET /api/v1/audio/tempo`

Get the current beat-tracking and tempo grid snapshot.

**Response:**
```json
{
  "success": true,
  "data": {
    "bpm": 124.0,
    "confidence": 0.87,
    "beat_phase": 0.42,
    "bar_phase": 0.11,
    "beat_in_bar": 2,
    "beats_per_bar": 4
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
```

| Field | Type | Range | Description |
|-------|------|-------|-------------|
| `bpm` | float | 30–300 | Smoothed tempo estimate in beats per minute |
| `confidence` | float | 0–1 | Beat-detection confidence (0 = no signal, 1 = locked) |
| `beat_phase` | float | 0–1 | Position within current beat cycle |
| `bar_phase` | float | 0–1 | Position within current bar |
| `beat_in_bar` | uint8 | 0–N | Current beat index within the bar (zero-based) |
| `beats_per_bar` | uint8 | 1–16 | Time signature numerator |

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v1/audio/tempo
```

---

#### `GET /api/v1/audio/fft`

Get the latest audio analysis frame containing spectral data. Returns a snapshot of the most recent ControlBus frame.

**Response:**
```json
{
  "success": true,
  "data": {
    "backend": "lwls",
    "seq": 9234,
    "rms": 0.35,
    "flux": 0.22,
    "bands": [0.82, 0.61, 0.45, 0.30, 0.18, 0.12, 0.08, 0.04],
    "chroma": [0.1, 0.0, 0.3, 0.0, 0.2, 0.1, 0.0, 0.4, 0.0, 0.2, 0.0, 0.1],
    "bins64": [0.02, 0.05, 0.12, 0.41, 0.38, 0.21, 0.14, 0.08],
    "bins64Adaptive": [0.03, 0.07, 0.18, 0.55, 0.50, 0.28, 0.19, 0.11]
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
```

| Field | Type | Description |
|-------|------|-------------|
| `backend` | string | Audio backend identifier: `"lwls"` (PipelineCore) or `"esv11"` |
| `seq` | uint32 | Monotonically increasing frame sequence number |
| `rms` | float | Smoothed RMS energy level (0–1) |
| `flux` | float | Spectral flux / onset strength (0–1) |
| `bands` | float[8] | Octave energy bands (60 Hz to ~8 kHz, each 0–1) |
| `chroma` | float[12] | Pitch-class energy (C through B, each 0–1) |
| `bins64` | float[64] | Raw 64-bin spectral magnitude array |
| `bins64Adaptive` | float[64] | Adaptively normalised 64-bin magnitude array |

**Band Centre Frequencies:**

| Index | Frequency | Label |
|-------|-----------|-------|
| 0 | ~60 Hz | Sub-bass |
| 1 | ~120 Hz | Bass |
| 2 | ~250 Hz | Low-mid |
| 3 | ~500 Hz | Mid |
| 4 | ~1000 Hz | High-mid |
| 5 | ~2000 Hz | Presence |
| 6 | ~4000 Hz | Brilliance |
| 7 | ~7800 Hz | Air |

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v1/audio/fft
```

---

### Presets Endpoints

Zone-layout presets let you save and restore complete multi-zone configurations (effect assignments, per-zone parameters, blend modes, and zone layout).

#### `GET /api/v1/presets`

List all saved zone-configuration presets.

**Response:**
```json
{
  "success": true,
  "data": {
    "presets": [],
    "count": 0,
    "status": "stub"
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
```

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v1/presets
```

---

#### `GET /api/v1/presets/{name}`

Download a named preset as a JSON document.

**Path Parameters:**

| Parameter | Description |
|-----------|-------------|
| `name` | URL-encoded preset name (matches the `name` field used when saving) |

**Response:** Returns the stored preset JSON document on success.

**Error Responses:**

| Condition | HTTP | Code |
|-----------|------|------|
| Not yet implemented | 404 | `NOT_IMPLEMENTED` |

---

#### `POST /api/v1/presets`

Upload a new zone-configuration preset.

**Request Body:** A preset JSON document (structure defined when the feature is released).

**Error Responses:**

| Condition | HTTP | Code |
|-----------|------|------|
| Not yet implemented | 404 | `NOT_IMPLEMENTED` |

---

#### `PUT /api/v1/presets/{name}`

Update an existing preset by name.

**Error Responses:**

| Condition | HTTP | Code |
|-----------|------|------|
| Not yet implemented | 404 | `NOT_IMPLEMENTED` |

---

#### `DELETE /api/v1/presets/{name}`

Delete a preset by name.

**Error Responses:**

| Condition | HTTP | Code |
|-----------|------|------|
| Not yet implemented | 404 | `NOT_IMPLEMENTED` |

---

#### `POST /api/v1/presets/{name}/rename`

Rename an existing preset.

**Request Body:**
```json
{
  "newName": "My Live Set"
}
```

**Error Responses:**

| Condition | HTTP | Code |
|-----------|------|------|
| Not yet implemented | 404 | `NOT_IMPLEMENTED` |

---

#### `POST /api/v1/presets/{name}/load`

Apply a saved preset to the active zone configuration.

**Error Responses:**

| Condition | HTTP | Code |
|-----------|------|------|
| Not yet implemented | 404 | `NOT_IMPLEMENTED` |

---

#### `POST /api/v1/presets/save-current`

Save the current zone configuration as a new named preset.

**Request Body:**
```json
{
  "name": "My Live Set"
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `name` | string | Yes | Human-readable preset name |

**Error Responses:**

| Condition | HTTP | Code |
|-----------|------|------|
| Not yet implemented | 404 | `NOT_IMPLEMENTED` |

**cURL Example:**
```bash
curl -X POST http://lightwaveos.local/api/v1/presets/save-current \
  -H "Content-Type: application/json" \
  -d '{"name": "My Live Set"}'
```

---

### Debug Endpoints

#### `GET /api/v1/debug/udp`

Get UDP streaming counters and backoff state for field diagnostics.

**Response:**
```json
{
  "success": true,
  "data": {
    "started": true,
    "subscribers": 1,
    "suppressed": 0,
    "consecutiveFailures": 0,
    "lastFailureMs": 123456,
    "lastFailureAgoMs": 250,
    "cooldownRemainingMs": 0,
    "socketResets": 0,
    "lastSocketResetMs": 0,
    "led": {"attempts": 100, "success": 100, "failures": 0},
    "audio": {"attempts": 100, "success": 99, "failures": 1}
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
```

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v1/debug/udp
```

---

### Edge Mixer

Composable 3-stage post-processor for Light Guide Plate colour differentiation. Applies hue/saturation transforms to Strip 2 via three independent stages: Colour (7 transform modes), Spatial (positional mask), and Temporal (audio-reactive modulation).

Effective pixel strength = `scale8(scale8(globalStrength, temporalAmount), spatialAmount)`.

**Parameters:**

| Field | Type | Range | Description |
|-------|------|-------|-------------|
| `mode` | uint8 | 0-6 | Colour transform: 0=mirror, 1=analogous, 2=complementary, 3=split_complementary, 4=saturation_veil, 5=triadic, 6=tetradic |
| `spread` | uint8 | 0-60 | Hue spread in degrees (affects modes 1, 4, 5, 6) |
| `strength` | uint8 | 0-255 | Global mix strength: 0=no effect, 255=full shift |
| `spatial` | uint8 | 0-1 | Spatial mask: 0=uniform, 1=centre_gradient (0 at LED 79, 255 at edges) |
| `temporal` | uint8 | 0-1 | Temporal modulation: 0=static, 1=rms_gate (strength tracks audio RMS) |
| `save` | bool | - | Optional. If `true`, persist current settings to NVS after applying changes (REST only) |

**Mode details:**

| Mode | Name | Description |
|------|------|-------------|
| 0 | mirror | No processing (zero overhead) |
| 1 | analogous | Shift hue by +spread degrees |
| 2 | complementary | +180° hue shift with ~15% desaturation |
| 3 | split_complementary | +150° hue shift with ~10% desaturation |
| 4 | saturation_veil | Desaturate by spread amount (spread 0 = no change, spread 60 = near-greyscale) |
| 5 | triadic | +120° hue shift (colour triad) |
| 6 | tetradic | +90° hue shift (colour tetrad) |

#### REST: `GET /api/v1/edgeMixer`

Returns current edge mixer state.

```json
{
  "success": true,
  "data": {
    "mode": 1,
    "modeName": "analogous",
    "spread": 30,
    "strength": 255,
    "spatial": 0,
    "spatialName": "uniform",
    "temporal": 0,
    "temporalName": "static"
  },
  "timestamp": 123456,
  "version": "2.0"
}
```

#### REST: `POST /api/v1/edgeMixer`

Set one or more edge mixer parameters. All fields are optional; omitted fields remain unchanged. Include `"save": true` to persist to NVS.

```bash
curl -X POST -H "Content-Type: application/json" \
  -d '{"mode":3,"spatial":1,"temporal":1,"save":true}' \
  http://192.168.4.1/api/v1/edgeMixer
```

#### WebSocket: `edge_mixer.get`

```json
{"type":"edge_mixer.get"}
```

Response includes `mode`, `modeName`, `spread`, `strength`, `spatial`, `spatialName`, `temporal`, `temporalName`.

#### WebSocket: `edge_mixer.set`

```json
{"type":"edge_mixer.set","mode":3,"spatial":1,"temporal":1}
```

All fields optional. Validates all before applying any.

#### WebSocket: `edge_mixer.save`

```json
{"type":"edge_mixer.save"}
```

Persists current settings (including spatial and temporal) to NVS.

#### Serial JSON: `getEdgeMixer` / `setEdgeMixer` / `saveEdgeMixer`

```json
{"type":"getEdgeMixer"}
{"type":"setEdgeMixer","mode":3,"spatial":1,"temporal":1}
{"type":"saveEdgeMixer"}
```

#### Broadcast Fields

The periodic WebSocket status broadcast includes:

| Field | Type | Description |
|-------|------|-------------|
| `edgeMixerMode` | uint8 | Current mode (0-6) |
| `edgeMixerModeName` | string | Human-readable mode name |
| `edgeMixerSpread` | uint8 | Current spread (0-60) |
| `edgeMixerStrength` | uint8 | Current strength (0-255) |
| `edgeMixerSpatial` | uint8 | Current spatial mode (0-1) |
| `edgeMixerSpatialName` | string | Human-readable spatial name |
| `edgeMixerTemporal` | uint8 | Current temporal mode (0-1) |
| `edgeMixerTemporalName` | string | Human-readable temporal name |

---

## WebSocket Commands

**WebSocket URL:** `ws://lightwaveos.local/ws`

All WebSocket messages use JSON format with a `type` field.

### Command Categories

| Category | Commands |
|----------|----------|
| Effects | `effects.getCurrent`, `effects.getMetadata`, `effects.getCategories`, `setEffect` |
| Parameters | `parameters.get`, `setBrightness`, `setSpeed`, `setPalette` |
| Transitions | `transition.getTypes`, `transition.trigger`, `transition.config` |
| Zones | `zones.list`, `zones.update`, `zones.setLayout`, `zone.setEffect`, `zone.setPalette`, `zone.setBlend` |
| Device | `device.getStatus` |
| Batch | `batch` |
| Streaming | `ledStream.subscribe`, `ledStream.unsubscribe`, `audio.subscribe`, `audio.unsubscribe` |
| Edge Mixer | `edge_mixer.get`, `edge_mixer.set`, `edge_mixer.save` |
| Debug | `debug.audio.get`, `debug.audio.set`, `debug.udp.get` |

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

### Command: `debug.udp.get`

Get UDP streaming counters and backoff state.

**Send:**
```json
{
  "type": "debug.udp.get",
  "requestId": "udp-1"
}
```

**Receive:**
```json
{
  "type": "debug.udp.stats",
  "success": true,
  "data": {
    "started": true,
    "subscribers": 1,
    "suppressed": 0,
    "consecutiveFailures": 0,
    "lastFailureMs": 123456,
    "lastFailureAgoMs": 250,
    "cooldownRemainingMs": 0,
    "socketResets": 0,
    "lastSocketResetMs": 0,
    "led": {"attempts": 100, "success": 100, "failures": 0},
    "audio": {"attempts": 100, "success": 99, "failures": 1}
  }
}
```

**JavaScript Example:**
```javascript
ws.send(JSON.stringify({
  type: "debug.udp.get",
  requestId: "udp-1"
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

### Command: `zones.list`

Get all zone configurations over WebSocket.

**Send:**
```json
{
  "type": "zones.list"
}
```

**Receive:**
```json
{
  "type": "zones.list",
  "success": true,
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
      "effectName": "Shockwave",
      "brightness": 200,
      "speed": 20,
      "paletteId": 0,
      "blendMode": 0,
      "blendModeName": "OVERWRITE"
    }
  ],
  "presets": [
    {"id": 0, "name": "Unified"},
    {"id": 1, "name": "Dual Split"}
  ]
}
```

**JavaScript Example:**
```javascript
ws.send(JSON.stringify({
  type: "zones.list"
}));
```

---

### Command: `zones.update`

Update zone configuration over WebSocket. Supports updating multiple fields in a single request.

**Send:**
```json
{
  "type": "zones.update",
  "zoneId": 0,
  "brightness": 220,
  "speed": 25,
  "paletteId": 5,
  "blendMode": 1
}
```

**Supported Fields:**

| Field | Type | Range | Description |
|-------|------|-------|-------------|
| `effectId` | uint8 | 0-255 | Effect to render in this zone |
| `brightness` | uint8 | 0-255 | Zone brightness level |
| `speed` | uint8 | 1-100 | Zone animation speed |
| `paletteId` | uint8 | 0-74 | Palette ID (0 = use global palette) |
| `blendMode` | uint8 | 0-7 | Blend mode for compositing (see blend modes table) |

**Receive:**
Broadcast to all clients:
```json
{
  "type": "zones.changed",
  "zoneId": 0,
  "updated": ["brightness", "speed", "paletteId", "blendMode"],
  "current": {
    "effectId": 5,
    "brightness": 220,
    "speed": 25,
    "paletteId": 5,
    "blendMode": 1,
    "blendModeName": "Additive"
  },
  "timestamp": 123456789
}
```

**JavaScript Example:**
```javascript
ws.send(JSON.stringify({
  type: "zones.update",
  zoneId: 0,
  brightness: 220,
  speed: 25,
  paletteId: 5,
  blendMode: 1
}));
```

---

### Command: `zones.setLayout`

Set zone layout using custom segment definitions.

**Send:**
```json
{
  "type": "zones.setLayout",
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

**Receive:**
```json
{
  "type": "zones.layoutChanged",
  "success": true,
  "zoneCount": 3
}
```

**JavaScript Example:**
```javascript
ws.send(JSON.stringify({
  type: "zones.setLayout",
  zones: [
    {zoneId: 0, s1LeftStart: 65, s1LeftEnd: 79, s1RightStart: 80, s1RightEnd: 94},
    {zoneId: 1, s1LeftStart: 20, s1LeftEnd: 64, s1RightStart: 95, s1RightEnd: 139}
  ]
}));
```

---

### Command: `zone.setPalette`

Set palette for a specific zone. Palette ID 0 uses the global palette.

**Send:**
```json
{
  "type": "zone.setPalette",
  "zoneId": 0,
  "paletteId": 5
}
```

**Receive:**
```json
{
  "type": "zone.paletteChanged",
  "zoneId": 0,
  "current": {
    "effectId": 5,
    "effectName": "Shockwave",
    "brightness": 200,
    "speed": 20,
    "paletteId": 5,
    "blendMode": 0,
    "blendModeName": "Overwrite"
  }
}
```

**JavaScript Example:**
```javascript
ws.send(JSON.stringify({
  type: "zone.setPalette",
  zoneId: 0,
  paletteId: 5
}));
```

---

### Command: `zone.setEffect`

Set effect for a specific zone.

**Send:**
```json
{
  "type": "zone.setEffect",
  "zoneId": 0,
  "effectId": 9
}
```

**Receive:**
```json
{
  "type": "zone.effectChanged",
  "zoneId": 0,
  "current": {
    "effectId": 9,
    "effectName": "Holographic Interference",
    "brightness": 200,
    "speed": 20,
    "paletteId": 0,
    "blendMode": 0,
    "blendModeName": "Overwrite"
  }
}
```

**JavaScript Example:**
```javascript
ws.send(JSON.stringify({
  type: "zone.setEffect",
  zoneId: 0,
  effectId: 9
}));
```

---

### Command: `zone.setBlend`

Set blend mode for a specific zone.

**Send:**
```json
{
  "type": "zone.setBlend",
  "zoneId": 0,
  "blendMode": 1
}
```

**Blend Modes:**

| ID | Name | Description |
|----|------|-------------|
| 0 | Overwrite | Replace pixels in zone |
| 1 | Additive | Add to existing pixels (light accumulation) |
| 2 | Multiply | Multiply with existing pixels |
| 3 | Screen | Screen blend mode (lighten) |
| 4 | Overlay | Overlay blend (multiply if dark, screen if light) |
| 5 | Alpha | 50/50 blend |
| 6 | Lighten | Take brighter pixel |
| 7 | Darken | Take darker pixel |

**Receive:**
```json
{
  "type": "zone.blendChanged",
  "zoneId": 0,
  "current": {
    "effectId": 5,
    "effectName": "Shockwave",
    "brightness": 200,
    "speed": 20,
    "paletteId": 0,
    "blendMode": 1,
    "blendModeName": "Additive"
  }
}
```

**JavaScript Example:**
```javascript
ws.send(JSON.stringify({
  type: "zone.setBlend",
  zoneId: 0,
  blendMode: 1
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

### Command: `ledStream.subscribe`

Subscribe to live LED frame data. Frames are 966 bytes: 6-byte header + 960 bytes (320 LEDs x 3 RGB).

**Send (WebSocket transport):**
```json
{
  "type": "ledStream.subscribe"
}
```

**Receive:**
```json
{
  "type": "ledStream.subscribed",
  "success": true,
  "transport": "ws"
}
```

After subscription, binary LED frames arrive on the WebSocket connection.

**UDP Transport Negotiation:**

Clients can opt into UDP delivery by including `udpPort` in the subscribe command:

```json
{"type": "ledStream.subscribe", "udpPort": 41234}
```

Response includes transport confirmation:
```json
{"type": "ledStream.subscribed", "success": true, "transport": "udp"}
```

If `udpPort` is omitted, frames are delivered via WebSocket binary (backward compatible).

**Audio UDP subscription follows the same pattern:**
```json
{"type": "audio.subscribe", "udpPort": 41234}
```

---

### Command: `ledStream.unsubscribe`

Stop receiving LED frame data.

**Send:**
```json
{
  "type": "ledStream.unsubscribe"
}
```

**Receive:**
```json
{
  "type": "ledStream.unsubscribed",
  "success": true
}
```

---

### Command: `audio.subscribe`

Subscribe to live audio metrics data. Frames are 464 bytes of binary audio analysis data (30 FPS).

**Send (WebSocket transport):**
```json
{
  "type": "audio.subscribe"
}
```

**Send (UDP transport):**
```json
{
  "type": "audio.subscribe",
  "udpPort": 41234
}
```

**Receive:**
```json
{
  "type": "audio.subscribed",
  "success": true,
  "transport": "udp"
}
```

---

### Command: `audio.unsubscribe`

Stop receiving audio metrics data.

**Send:**
```json
{
  "type": "audio.unsubscribe"
}
```

**Receive:**
```json
{
  "type": "audio.unsubscribed",
  "success": true
}
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

### UDP Streaming

For high-frequency binary data (LED frames, audio metrics), iOS clients use UDP transport to avoid TCP ACK timeout failures on weak WiFi. The WebSocket connection serves as the control channel for subscription negotiation and JSON events. UDP and WebSocket streaming coexist -- clients choose transport per subscription.

UDP frame demultiplexing uses magic bytes:
- LED: first byte `0xFE` (966 bytes)
- Audio: first 4 bytes `0x41 0x55 0x44 0x00` (464 bytes)

### Performance Considerations

- Target frame rate: 120 FPS
- WebSocket recommended for real-time updates
- Batch operations reduce HTTP overhead
- Rate limiting protects device stability

---

## Related Documentation

For audio-specific APIs (requires `esp32dev_audio` build):

- [Audio Stream API](../../v2/docs/AUDIO_STREAM_API.md) - Binary audio metrics streaming (30 FPS)
- [Audio Control API](../../v2/docs/AUDIO_CONTROL_API.md) - Capture control, beat events, tuning presets

---

**Documentation Version:** 1.0.0
**Last Updated:** 2026-02-04
**API Base:** LightwaveOS v1.0.0
