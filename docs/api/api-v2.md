# LightwaveOS API v2 Reference

**Version:** 2.0.0
**Base URL:** `http://lightwaveos.local/api/v2/` or `http://<device-ip>/api/v2/`
**Protocol:** HTTP REST + WebSocket

## Table of Contents

1. [Introduction](#introduction)
2. [Response Format](#response-format)
3. [Error Codes](#error-codes)
4. [Rate Limiting](#rate-limiting)
5. [REST API Endpoints](#rest-api-endpoints)
   - [Discovery & Device](#discovery--device-endpoints-5)
   - [Effects](#effects-endpoints-5)
   - [Parameters](#parameters-endpoints-4)
   - [Transitions](#transitions-endpoints-4)
   - [Zones](#zones-endpoints-10)
   - [Enhancements](#enhancement-endpoints-8)
   - [Batch](#batch-endpoints-1)
6. [WebSocket Commands](#websocket-commands)
7. [Examples](#examples)
8. [Migration from v1](#migration-from-v1)

---

## Introduction

The LightwaveOS API v2 provides programmatic control over an ESP32-S3 LED control system featuring:

- **320 LEDs** across dual 160-LED WS2812 strips
- **47+ visual effects** organized into 11 categories
- **Multi-zone composition** (up to 4 independent zones)
- **12 transition types** with 15 easing curves
- **CENTER ORIGIN constraint** - all effects originate from LED 79/80
- **Advanced color engine** - cross-palette blending, diffusion, temporal rotation
- **Per-effect custom parameters** - semantic control (e.g., "Flame Height")

### What's New in v2

- **REST-first design** - All operations available via REST endpoints
- **Resource-oriented routes** - `/api/v2/effects/{id}` instead of query params
- **Full CRUD for zones** - Create, read, update, delete zone configurations
- **Custom effect parameters** - Per-effect semantic controls beyond global params
- **Color & Motion engines** - Access to advanced enhancement systems
- **Consistent HTTP verbs** - GET, POST, PUT, PATCH, DELETE where appropriate
- **Improved error handling** - Granular error codes with field-level validation

---

## Response Format

All v2 API responses follow a standardized JSON structure.

### Success Response

```json
{
  "success": true,
  "data": {
    // Endpoint-specific data
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

### Error Response

```json
{
  "success": false,
  "error": {
    "code": "OUT_OF_RANGE",
    "message": "Effect ID must be between 0 and 46",
    "field": "effectId",
    "details": {
      "provided": 99,
      "min": 0,
      "max": 46
    }
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

---

## Error Codes

| Code | HTTP Status | Description |
|------|-------------|-------------|
| `INVALID_JSON` | 400 | Request body is not valid JSON |
| `MISSING_FIELD` | 400 | Required field is missing from request |
| `OUT_OF_RANGE` | 400 | Parameter value exceeds valid range |
| `INVALID_VALUE` | 400 | Value is of wrong type or format |
| `RESOURCE_NOT_FOUND` | 404 | Requested resource does not exist |
| `METHOD_NOT_ALLOWED` | 405 | HTTP method not supported for endpoint |
| `CONFLICT` | 409 | Resource already exists or state conflict |
| `RATE_LIMITED` | 429 | Too many requests, client is rate limited |
| `INTERNAL_ERROR` | 500 | Server-side error occurred |
| `NOT_IMPLEMENTED` | 501 | Feature not available in current build |

---

## Rate Limiting

To protect device stability, the API enforces rate limits per client IP address.

| Protocol | Limit | Window | Penalty |
|----------|-------|--------|---------|
| **HTTP** | 20 requests/sec | 1 second | 429 status |
| **WebSocket** | 50 messages/sec | 1 second | Error message |
| **Block Duration** | N/A | 5 seconds | After limit breach |

**Rate Limit Headers (v2):**
```
X-RateLimit-Limit: 20
X-RateLimit-Remaining: 15
X-RateLimit-Reset: 1640000000
```

**Best Practices:**
- Use WebSocket for real-time updates (higher rate limit)
- Use batch operations to reduce request count
- Implement client-side debouncing for UI controls
- Check `X-RateLimit-Remaining` header before bulk operations

---

## REST API Endpoints

### Discovery & Device Endpoints (5)

#### `GET /api/v2/`

Get API metadata and HATEOAS links.

**Response:**
```json
{
  "success": true,
  "data": {
    "name": "LightwaveOS",
    "apiVersion": "2.0.0",
    "description": "ESP32-S3 LED Control System",
    "hardware": {
      "ledsTotal": 320,
      "strips": 2,
      "ledsPerStrip": 160,
      "centerPoint": 79,
      "maxZones": 4
    },
    "features": {
      "webServer": true,
      "zoneComposer": true,
      "transitions": true,
      "colorEngine": true,
      "motionEngine": false,
      "rotate8Encoder": false
    },
    "_links": {
      "self": "/api/v2/",
      "openapi": "/api/v2/openapi.json",
      "device": "/api/v2/device",
      "effects": "/api/v2/effects",
      "parameters": "/api/v2/parameters",
      "transitions": "/api/v2/transitions",
      "zones": "/api/v2/zones",
      "batch": "/api/v2/batch",
      "websocket": "ws://lightwaveos.local/ws"
    }
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v2/
```

---

#### `GET /api/v2/openapi.json`

Get OpenAPI 3.0 specification for the entire API.

**Response:**
```json
{
  "openapi": "3.0.3",
  "info": {
    "title": "LightwaveOS API",
    "version": "2.0.0",
    "description": "ESP32-S3 LED Control System"
  },
  "servers": [
    {"url": "http://lightwaveos.local/api/v2"}
  ],
  "paths": { /* ... */ }
}
```

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v2/openapi.json
```

---

#### `GET /api/v2/device`

Get complete device information (status + info combined).

**Response:**
```json
{
  "success": true,
  "data": {
    "status": {
      "uptime": 3600,
      "freeHeap": 245000,
      "heapSize": 327680,
      "cpuFreq": 240,
      "network": {
        "connected": true,
        "apMode": false,
        "ip": "192.168.1.100",
        "ssid": "MyNetwork",
        "rssi": -45
      },
      "wsClients": 2
    },
    "info": {
      "firmware": "1.0.0",
      "board": "ESP32-S3-DevKitC-1",
      "sdk": "v4.4.2",
      "flashSize": 8388608,
      "sketchSize": 1234567,
      "freeSketch": 6666666,
      "chipModel": "ESP32-S3",
      "chipRevision": 0,
      "chipCores": 2
    }
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v2/device
```

---

#### `GET /api/v2/device/status`

Get device runtime status only.

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
      "ssid": "MyNetwork",
      "rssi": -45
    },
    "wsClients": 2,
    "currentEffect": 5,
    "zoneMode": false,
    "brightness": 200
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v2/device/status
```

---

#### `GET /api/v2/device/info`

Get device hardware and firmware information only.

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
    "freeSketch": 6666666,
    "chipModel": "ESP32-S3",
    "chipRevision": 0,
    "chipCores": 2
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v2/device/info
```

---

### Effects Endpoints (5)

#### `GET /api/v2/effects`

List all available effects with pagination and filtering.

**Query Parameters:**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `page` | int | 1 | Page number (1-indexed) |
| `limit` | int | 20 | Items per page (1-50) |
| `category` | string | null | Filter by category ID (0-10) |
| `details` | bool | false | Include full metadata |

**Response:**
```json
{
  "success": true,
  "data": {
    "effects": [
      {
        "id": 0,
        "name": "Fire",
        "category": "Classic",
        "categoryId": 0,
        "description": "Realistic fire simulation radiating from center",
        "features": {
          "centerOrigin": true,
          "usesSpeed": true,
          "usesPalette": true,
          "zoneAware": true,
          "dualStrip": false,
          "physicsBased": false
        },
        "customParams": [
          {
            "name": "Flame Height",
            "min": 0,
            "max": 255,
            "default": 180,
            "target": "intensity"
          },
          {
            "name": "Spark Rate",
            "min": 0,
            "max": 255,
            "default": 100,
            "target": "variation"
          }
        ]
      }
    ],
    "pagination": {
      "page": 1,
      "limit": 20,
      "total": 47,
      "pages": 3
    },
    "categories": [
      {"id": 0, "name": "Classic"},
      {"id": 1, "name": "Shockwave"},
      {"id": 2, "name": "LGP Interference"}
    ]
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

**cURL Examples:**
```bash
# Get first 20 effects
curl http://lightwaveos.local/api/v2/effects

# Get page 2 with 10 items, full details
curl "http://lightwaveos.local/api/v2/effects?page=2&limit=10&details=true"

# Filter by category
curl "http://lightwaveos.local/api/v2/effects?category=2"
```

---

#### `GET /api/v2/effects/current`

Get currently active effect with all parameters.

**Response:**
```json
{
  "success": true,
  "data": {
    "effectId": 5,
    "name": "Shockwave",
    "category": "Shockwave",
    "parameters": {
      "brightness": 200,
      "speed": 15,
      "paletteId": 3,
      "intensity": 150,
      "saturation": 255,
      "complexity": 128,
      "variation": 180
    }
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v2/effects/current
```

---

#### `PUT /api/v2/effects/current`

Set the active effect (triggers transition if enabled).

**Request Body:**
```json
{
  "effectId": 12,
  "transition": {
    "enabled": true,
    "type": 5,
    "duration": 2000
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `effectId` | uint8 | Yes | Effect ID (0-46) |
| `transition.enabled` | bool | No | Override global transition setting |
| `transition.type` | uint8 | No | Transition type (0-11) |
| `transition.duration` | uint32 | No | Duration in ms (100-10000) |

**Response:**
```json
{
  "success": true,
  "data": {
    "effectId": 12,
    "name": "Dual Wave Collision",
    "transitionActive": true,
    "transitionType": 5,
    "duration": 2000
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
curl -X PUT http://lightwaveos.local/api/v2/effects/current \
  -H "Content-Type: application/json" \
  -d '{"effectId": 12, "transition": {"type": 5, "duration": 2000}}'
```

---

#### `GET /api/v2/effects/{id}`

Get detailed metadata for a specific effect.

**Path Parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `id` | uint8 | Effect ID (0-46) |

**Response:**
```json
{
  "success": true,
  "data": {
    "id": 9,
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
    "featureFlags": 55,
    "customParams": [
      {
        "name": "Fringe Width",
        "min": 0,
        "max": 255,
        "default": 128,
        "target": "intensity"
      },
      {
        "name": "Phase Shift",
        "min": 0,
        "max": 255,
        "default": 100,
        "target": "variation"
      }
    ]
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v2/effects/9
```

---

#### `GET /api/v1/effects/parameters` (compat)
#### `POST /api/v1/effects/parameters` (compat)
#### `PATCH /api/v1/effects/parameters` (compat)

Per-effect tunable discovery and updates currently ship on the v1 compatibility route.  
v2 clients can call these routes directly without changing command names.

**Typed tunable metadata fields:**

| Field | Type | Description |
|------|------|-------------|
| `type` | `float\\|int\\|bool\\|enum` | Runtime value type |
| `step` | float | Suggested UI increment |
| `group` | string | Stable grouping key (`timing`, `wave`, `intro`, etc.) |
| `unit` | string | Display unit (`s`, `Hz`, `%`, etc.) |
| `advanced` | bool | True for dense/internal controls |

**Persistence status fields:**

| Field | Type | Description |
|------|------|-------------|
| `persistence.mode` | `nvs\\|volatile` | Active persistence mode |
| `persistence.dirty` | bool | Pending debounced write |
| `persistence.lastError` | string | Optional last persistence error |

**Coverage snapshot (as of February 26, 2026):**

- Active effects: `170`
- Effects with API tunables: `170`
- Total exposed per-effect parameters: `1162`
- Missing named tunables: `0`
- Coverage gate: `python firmware/v2/tools/effect_tunables/generate_manifest.py --validate --enforce-all-families` passes

All active families now expose tunables (Wave A, Wave B, and remaining active families).

For per-effect key lists used by dashboard tuners, refer to:
- v1 list in `docs/api/api-v1.md` under `POST/PATCH /api/v1/effects/parameters`
- generated manifest `firmware/v2/docs/effects/tunable_manifest.json`

---

#### `GET /api/v2/effects/categories`

Get list of all effect categories with counts.

**Response:**
```json
{
  "success": true,
  "data": {
    "categories": [
      {
        "id": 0,
        "name": "Classic",
        "description": "Traditional LED effects",
        "effectCount": 5
      },
      {
        "id": 1,
        "name": "Shockwave",
        "description": "Energy pulses from center",
        "effectCount": 4
      },
      {
        "id": 2,
        "name": "LGP Interference",
        "description": "Wave interference patterns",
        "effectCount": 4
      }
    ]
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v2/effects/categories
```

---

### Parameters Endpoints (4)

#### `GET /api/v2/parameters`

Get all current global parameter values.

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
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v2/parameters
```

---

#### `PATCH /api/v2/parameters`

Update one or more global parameters. Only include fields you want to update.

**Request Body:**
```json
{
  "brightness": 180,
  "speed": 25,
  "intensity": 150
}
```

**Parameter Ranges:**

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| `brightness` | uint8 | 0-255 | Global brightness |
| `speed` | uint8 | 1-50 | Animation speed |
| `paletteId` | uint8 | 0-35 | Color palette index |
| `intensity` | uint8 | 0-255 | Effect intensity |
| `saturation` | uint8 | 0-255 | Color saturation |
| `complexity` | uint8 | 0-255 | Pattern complexity |
| `variation` | uint8 | 0-255 | Effect variation |

**Response:**
```json
{
  "success": true,
  "data": {
    "updated": ["brightness", "speed", "intensity"],
    "current": {
      "brightness": 180,
      "speed": 25,
      "paletteId": 5,
      "intensity": 150,
      "saturation": 255,
      "complexity": 64,
      "variation": 32
    }
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
curl -X PATCH http://lightwaveos.local/api/v2/parameters \
  -H "Content-Type: application/json" \
  -d '{"brightness": 180, "speed": 25}'
```

---

#### `GET /api/v2/parameters/{name}`

Get a specific parameter value.

**Path Parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `name` | string | Parameter name (brightness, speed, etc.) |

**Response:**
```json
{
  "success": true,
  "data": {
    "name": "brightness",
    "value": 200,
    "min": 0,
    "max": 255,
    "description": "Global brightness level"
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v2/parameters/brightness
```

---

#### `PUT /api/v2/parameters/{name}`

Set a specific parameter value.

**Path Parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `name` | string | Parameter name (brightness, speed, etc.) |

**Request Body:**
```json
{
  "value": 180
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "name": "brightness",
    "value": 180,
    "previousValue": 200
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
curl -X PUT http://lightwaveos.local/api/v2/parameters/brightness \
  -H "Content-Type: application/json" \
  -d '{"value": 180}'
```

---

### Transitions Endpoints (4)

#### `GET /api/v2/transitions`

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
        "centerOrigin": true,
        "duration": {
          "min": 100,
          "max": 10000,
          "default": 1000
        }
      },
      {
        "id": 1,
        "name": "WIPE_OUT",
        "description": "Wipe from center outward",
        "centerOrigin": true,
        "duration": {
          "min": 100,
          "max": 10000,
          "default": 1000
        }
      }
    ],
    "easingCurves": [
      {"id": 0, "name": "LINEAR"},
      {"id": 1, "name": "IN_QUAD"},
      {"id": 2, "name": "OUT_QUAD"},
      {"id": 3, "name": "IN_OUT_QUAD"},
      {"id": 7, "name": "IN_ELASTIC"},
      {"id": 8, "name": "OUT_ELASTIC"}
    ]
  },
  "timestamp": 123456789,
  "version": "2.0.0"
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
curl http://lightwaveos.local/api/v2/transitions
```

---

#### `GET /api/v2/transitions/config`

Get current transition configuration.

**Response:**
```json
{
  "success": true,
  "data": {
    "enabled": true,
    "defaultDuration": 1000,
    "defaultType": 0,
    "defaultEasing": 3,
    "autoTransition": true
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v2/transitions/config
```

---

#### `PATCH /api/v2/transitions/config`

Update transition configuration. Only include fields you want to update.

**Request Body:**
```json
{
  "enabled": true,
  "defaultDuration": 1500,
  "defaultType": 5
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "updated": ["enabled", "defaultDuration", "defaultType"],
    "current": {
      "enabled": true,
      "defaultDuration": 1500,
      "defaultType": 5,
      "defaultEasing": 3
    }
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
curl -X PATCH http://lightwaveos.local/api/v2/transitions/config \
  -H "Content-Type: application/json" \
  -d '{"enabled": true, "defaultDuration": 1500}'
```

---

#### `POST /api/v2/transitions/trigger`

Manually trigger a transition to a new effect.

**Request Body:**
```json
{
  "toEffect": 15,
  "type": 5,
  "duration": 2000,
  "easing": 3
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `toEffect` | uint8 | Yes | Target effect ID (0-46) |
| `type` | uint8 | No | Transition type (0-11), default from config |
| `duration` | uint32 | No | Duration in ms (100-10000), default from config |
| `easing` | uint8 | No | Easing curve (0-14), default from config |

**Response:**
```json
{
  "success": true,
  "data": {
    "fromEffect": 5,
    "toEffect": 15,
    "toEffectName": "Radial Star",
    "transitionType": 5,
    "transitionName": "PULSEWAVE",
    "duration": 2000,
    "easing": 3,
    "estimatedCompletionMs": 124458789
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
curl -X POST http://lightwaveos.local/api/v2/transitions/trigger \
  -H "Content-Type: application/json" \
  -d '{"toEffect": 15, "type": 5, "duration": 2000}'
```

---

### Zones Endpoints (10)

#### `GET /api/v2/zones`

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
        "blendModeName": "OVERWRITE"
      }
    ],
    "presets": [
      {"id": 0, "name": "Unified"},
      {"id": 1, "name": "Dual Split"},
      {"id": 2, "name": "Triple Rings"},
      {"id": 3, "name": "Quad Active"},
      {"id": 4, "name": "Heartbeat Focus"}
    ]
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v2/zones
```

---

#### `POST /api/v2/zones`

Enable or disable the zone system.

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
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
curl -X POST http://lightwaveos.local/api/v2/zones \
  -H "Content-Type: application/json" \
  -d '{"enabled": true}'
```

---

#### `POST /api/v2/zones/layout`

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
| `zoneId` | uint8 | 0-3 | Zone identifier (must be sequential, starting from 0) |
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
   - Zone 0 = innermost (closest to centre pair)
   - Zone N-1 = outermost (at edges)

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
  "version": "2.0.0"
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
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
curl -X POST http://lightwaveos.local/api/v2/zones/layout \
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

#### `GET /api/v2/zones/{id}`

Get configuration for a specific zone.

**Path Parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `id` | uint8 | Zone ID (0-3) |

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
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v2/zones/0
```

---

#### `PATCH /api/v2/zones/{id}`

Update zone configuration. Only include fields you want to update.

**Path Parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `id` | uint8 | Zone ID (0-3) |

**Request Body:**
```json
{
  "enabled": true,
  "brightness": 220,
  "speed": 25
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "zoneId": 0,
    "updated": ["enabled", "brightness", "speed"],
    "current": {
      "enabled": true,
      "brightness": 220,
      "speed": 25
    }
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
curl -X PATCH http://lightwaveos.local/api/v2/zones/0 \
  -H "Content-Type: application/json" \
  -d '{"brightness": 220, "speed": 25}'
```

---

#### `DELETE /api/v2/zones/{id}`

Disable a specific zone.

**Path Parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `id` | uint8 | Zone ID (0-3) |

**Response:**
```json
{
  "success": true,
  "data": {
    "zoneId": 0,
    "enabled": false,
    "message": "Zone 0 disabled"
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
curl -X DELETE http://lightwaveos.local/api/v2/zones/0
```

---

#### `GET /api/v2/zones/{id}/effect`

Get effect assigned to a specific zone.

**Response:**
```json
{
  "success": true,
  "data": {
    "zoneId": 0,
    "effectId": 5,
    "effectName": "Shockwave"
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v2/zones/0/effect
```

---

#### `PUT /api/v2/zones/{id}/effect`

Set effect for a specific zone.

**Request Body:**
```json
{
  "effectId": 9
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "zoneId": 0,
    "effectId": 9,
    "effectName": "Holographic Interference"
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
curl -X PUT http://lightwaveos.local/api/v2/zones/0/effect \
  -H "Content-Type: application/json" \
  -d '{"effectId": 9}'
```

---

#### `GET /api/v2/zones/{id}/parameters`

Get all parameters for a specific zone.

**Response:**
```json
{
  "success": true,
  "data": {
    "zoneId": 0,
    "brightness": 200,
    "speed": 20,
    "paletteId": 0,
    "visualParams": {
      "intensity": 128,
      "saturation": 255,
      "complexity": 64,
      "variation": 32
    }
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v2/zones/0/parameters
```

---

#### `PATCH /api/v2/zones/{id}/parameters`

Update parameters for a specific zone.

**Request Body:**
```json
{
  "brightness": 220,
  "intensity": 150
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "zoneId": 0,
    "updated": ["brightness", "intensity"],
    "current": {
      "brightness": 220,
      "intensity": 150
    }
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
curl -X PATCH http://lightwaveos.local/api/v2/zones/0/parameters \
  -H "Content-Type: application/json" \
  -d '{"brightness": 220, "intensity": 150}'
```

---

#### `GET /api/v2/zones/presets`

Get available zone presets (built-in + user).

**Response:**
```json
{
  "success": true,
  "data": {
    "presets": [
      {"id": 0, "name": "Unified"},
      {"id": 1, "name": "Dual Split"},
      {"id": 2, "name": "Triple Rings"},
      {"id": 3, "name": "Quad Active"},
      {"id": 4, "name": "Heartbeat Focus"}
    ]
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v2/zones/presets
```

---

#### `POST /api/v2/zones/presets/{id}/load`

Load a zone preset (built-in or user).

**Path Parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `id` | uint8 | Preset ID (built-in) or slot (user) |

**Query Parameters:**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `type` | string | builtin | Preset type: `builtin` or `user` |

**Response:**
```json
{
  "success": true,
  "data": {
    "presetId": 3,
    "presetName": "Quad Active",
    "zoneCount": 4,
    "message": "Preset loaded successfully"
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
# Load built-in preset
curl -X POST http://lightwaveos.local/api/v2/zones/presets/3/load

# Load user preset
curl -X POST "http://lightwaveos.local/api/v2/zones/presets/0/load?type=user"
```

---

### Enhancement Endpoints (8)

These endpoints control advanced enhancement systems (Color Engine, Motion Engine).

#### `GET /api/v2/enhancements/color`

Get color engine status and configuration.

**Response:**
```json
{
  "success": true,
  "data": {
    "enabled": true,
    "crossBlend": {
      "enabled": true,
      "palette1": 3,
      "palette2": 7,
      "palette3": null,
      "blend1": 128,
      "blend2": 127,
      "blend3": 0
    },
    "temporalRotation": {
      "enabled": true,
      "speed": 0.5,
      "phase": 123.45
    },
    "diffusion": {
      "enabled": false,
      "amount": 0
    }
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v2/enhancements/color
```

---

#### `PATCH /api/v2/enhancements/color`

Update color engine configuration.

**Request Body:**
```json
{
  "enabled": true,
  "crossBlend": {
    "enabled": true,
    "palette1": 3,
    "palette2": 7
  },
  "temporalRotation": {
    "enabled": true,
    "speed": 0.5
  }
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "updated": ["enabled", "crossBlend", "temporalRotation"],
    "current": {
      "enabled": true
    }
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
curl -X PATCH http://lightwaveos.local/api/v2/enhancements/color \
  -H "Content-Type: application/json" \
  -d '{"crossBlend": {"enabled": true, "palette1": 3, "palette2": 7}}'
```

---

#### `POST /api/v2/enhancements/color/reset`

Reset color engine to default state.

**Response:**
```json
{
  "success": true,
  "data": {
    "message": "Color engine reset to defaults"
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
curl -X POST http://lightwaveos.local/api/v2/enhancements/color/reset
```

---

#### `GET /api/v2/enhancements/motion`

Get motion engine status and configuration.

**Response:**
```json
{
  "success": true,
  "data": {
    "enabled": false,
    "message": "Motion engine not available in current build"
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v2/enhancements/motion
```

---

#### `PATCH /api/v2/enhancements/motion`

Update motion engine configuration.

**Request Body:**
```json
{
  "enabled": true,
  "warpStrength": 128,
  "warpFrequency": 64
}
```

**Response:**
```json
{
  "success": false,
  "error": {
    "code": "NOT_IMPLEMENTED",
    "message": "Motion engine not available in current build"
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

---

#### `GET /api/v2/enhancements`

Get status of all enhancement systems.

**Response:**
```json
{
  "success": true,
  "data": {
    "colorEngine": {
      "available": true,
      "enabled": true
    },
    "motionEngine": {
      "available": false,
      "enabled": false
    }
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v2/enhancements
```

---

#### `GET /api/v1/palettes`

Get list of all available color palettes.

**Query Parameters:**
- `offset` (optional): Starting offset (default: 0)
- `limit` (optional): Number of palettes to return (default: 20, max: 100)
- `category` (optional): Filter by category (`artistic`, `scientific`, `lgpOptimized`)
- `warm` (optional): Filter warm palettes (`true`/`false`)
- `cool` (optional): Filter cool palettes (`true`/`false`)
- `calm` (optional): Filter calm palettes (`true`/`false`)
- `vivid` (optional): Filter vivid palettes (`true`/`false`)
- `cvd` (optional): Filter CVD-friendly palettes (`true`/`false`)

**Response:**
```json
{
  "success": true,
  "data": {
    "total": 75,
    "offset": 0,
    "limit": 100,
    "count": 75,
    "palettes": [
      {
        "id": 0,
        "name": "Sunset Real",
        "category": "artistic",
        "flags": {
          "warm": true,
          "cool": false,
          "calm": false,
          "vivid": true,
          "cvdFriendly": false,
          "whiteHeavy": false
        },
        "avgBrightness": 128,
        "maxBrightness": 255
      }
    ],
    "categories": {
      "artistic": 25,
      "scientific": 30,
      "lgpOptimized": 20
    },
    "pagination": {
      "page": 1,
      "limit": 100,
      "total": 75,
      "pages": 1
    }
  },
  "timestamp": 123456789,
  "version": "2.0"
}
```

**Note:** The response includes both flat pagination fields (`total`, `offset`, `limit`, `count`) for V2 API compatibility and a nested `pagination` object for backward compatibility.

**Response:**
```json
{
  "success": true,
  "data": {
    "palettes": [
      {
        "id": 0,
        "name": "Rainbow",
        "colors": ["#FF0000", "#FF7F00", "#FFFF00", "#00FF00"]
      },
      {
        "id": 1,
        "name": "Ocean",
        "colors": ["#001F3F", "#0074D9", "#39CCCC", "#7FDBFF"]
      }
    ],
    "total": 36
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v2/palettes
```

---

#### `GET /api/v2/palettes/{id}`

Get specific palette details.

**Response:**
```json
{
  "success": true,
  "data": {
    "id": 3,
    "name": "Forest",
    "colors": [
      "#001F00", "#003F00", "#005F00", "#007F00",
      "#009F00", "#00BF00", "#00DF00", "#00FF00"
    ]
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
curl http://lightwaveos.local/api/v2/palettes/3
```

---

### Batch Endpoints (1)

#### `POST /api/v2/batch`

Execute multiple operations in a single request (max 10 operations).

**Request Body:**
```json
{
  "operations": [
    {
      "method": "PATCH",
      "path": "/api/v2/parameters",
      "body": {
        "brightness": 200,
        "speed": 30
      }
    },
    {
      "method": "PUT",
      "path": "/api/v2/effects/current",
      "body": {
        "effectId": 9
      }
    },
    {
      "method": "PATCH",
      "path": "/api/v2/zones/0",
      "body": {
        "brightness": 220
      }
    }
  ]
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "processed": 3,
    "succeeded": 3,
    "failed": 0,
    "results": [
      {
        "operation": 0,
        "method": "PATCH",
        "path": "/api/v2/parameters",
        "success": true,
        "statusCode": 200
      },
      {
        "operation": 1,
        "method": "PUT",
        "path": "/api/v2/effects/current",
        "success": true,
        "statusCode": 200
      },
      {
        "operation": 2,
        "method": "PATCH",
        "path": "/api/v2/zones/0",
        "success": true,
        "statusCode": 200
      }
    ]
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

**cURL Example:**
```bash
curl -X POST http://lightwaveos.local/api/v2/batch \
  -H "Content-Type: application/json" \
  -d '{
    "operations": [
      {"method": "PATCH", "path": "/api/v2/parameters", "body": {"brightness": 200}},
      {"method": "PUT", "path": "/api/v2/effects/current", "body": {"effectId": 9}}
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
  console.log('Connected to LightwaveOS v2');
};

ws.onmessage = (event) => {
  const data = JSON.parse(event.data);
  console.log('Received:', data);
};

ws.onerror = (error) => {
  console.error('WebSocket error:', error);
};
```

---

### Command: `ledStream.subscribe`

Subscribe to real-time LED data streaming.

**Send:**
```json
{
  "type": "ledStream.subscribe",
  "requestId": "optional-id"
}
```

**Receive (Success):**
```json
{
  "type": "ledStream.subscribed",
  "requestId": "optional-id",
  "success": true,
  "data": {
    "clientId": 1,
    "frameSize": 961,
    "targetFps": 20,
    "magicByte": 254,
    "accepted": true
  }
}
```

**Receive (Failure):**
```json
{
  "type": "ledStream.rejected",
  "requestId": "optional-id",
  "success": false,
  "error": {
    "code": "RESOURCE_EXHAUSTED",
    "message": "Subscriber table full"
  }
}
```

**Binary Stream:**
Upon successful subscription, the server sends binary WebSocket frames at ~20 FPS.
- **Size:** 961 bytes
- **Format:** `[MAGIC_BYTE (0xFE)] [R0][G0][B0] ... [R319][G319][B319]`
- **Encoding:** Raw binary (Uint8Array)

**Send Unsubscribe:**
```json
{
  "type": "ledStream.unsubscribe"
}
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
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

---

### Command: `device.getInfo`

Get device hardware info over WebSocket.

**Send:**
```json
{
  "type": "device.getInfo"
}
```

**Receive:**
```json
{
  "type": "device.info",
  "success": true,
  "data": {
    "firmware": "1.0.0",
    "board": "ESP32-S3-DevKitC-1",
    "sdk": "v4.4.2"
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

---

### Command: `effects.list`

Get effects list over WebSocket.

**Send:**
```json
{
  "type": "effects.list",
  "page": 1,
  "limit": 20,
  "details": true
}
```

**Receive:**
```json
{
  "type": "effects.list",
  "success": true,
  "data": {
    "effects": [ /* ... */ ],
    "pagination": {
      "page": 1,
      "limit": 20,
      "total": 47
    }
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
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
    "parameters": {
      "brightness": 200,
      "speed": 15
    }
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

---

### Command: `effects.setCurrent`

Set current effect over WebSocket.

**Send:**
```json
{
  "type": "effects.setCurrent",
  "effectId": 12,
  "transition": {
    "type": 5,
    "duration": 2000
  }
}
```

**Receive:**
Broadcast to all clients:
```json
{
  "type": "effects.changed",
  "effectId": 12,
  "name": "Dual Wave Collision",
  "timestamp": 123456789
}
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
    "features": { /* ... */ }
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

---

### Command: `effects.parameters.get` (compat)

Get per-effect tunables over WebSocket.

**Send:**
```json
{
  "type": "effects.parameters.get",
  "effectId": 1201
}
```

**Receive:**
```json
{
  "type": "effects.parameters",
  "success": true,
  "data": {
    "effectId": 1201,
    "name": "Time Reversal Mirror Mod3",
    "hasParameters": true,
    "persistence": {
      "mode": "nvs",
      "dirty": false
    },
    "parameters": [
      {
        "name": "forward_sec",
        "type": "float",
        "step": 0.05,
        "group": "timing",
        "unit": "s",
        "advanced": false
      }
    ]
  }
}
```

---

### Command: `effects.parameters.set` (compat)

Set per-effect tunables over WebSocket.

**Send:**
```json
{
  "type": "effects.parameters.set",
  "effectId": 1201,
  "parameters": {
    "forward_sec": 1.4,
    "ridge_count": 12
  }
}
```

**Receive:**
```json
{
  "type": "effects.parameters.changed",
  "success": true,
  "data": {
    "effectId": 1201,
    "queued": ["forward_sec", "ridge_count"],
    "failed": []
  }
}
```

---

### Command: `parameters.get`

Get all parameters over WebSocket.

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
    "paletteId": 5
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

---

### Command: `parameters.set`

Set parameters over WebSocket.

**Send:**
```json
{
  "type": "parameters.set",
  "brightness": 180,
  "speed": 25
}
```

**Receive:**
Broadcast to all clients:
```json
{
  "type": "parameters.changed",
  "updated": ["brightness", "speed"],
  "current": {
    "brightness": 180,
    "speed": 25
  },
  "timestamp": 123456789
}
```

---

### Command: `transitions.list`

Get transition types over WebSocket.

**Send:**
```json
{
  "type": "transitions.list"
}
```

**Receive:**
```json
{
  "type": "transitions.list",
  "success": true,
  "data": {
    "types": [ /* ... */ ],
    "easingCurves": [ /* ... */ ]
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

---

### Command: `transitions.trigger`

Trigger transition over WebSocket.

**Send:**
```json
{
  "type": "transitions.trigger",
  "toEffect": 15,
  "type": 5,
  "duration": 2000
}
```

**Receive:**
Broadcast to all clients:
```json
{
  "type": "transition.started",
  "fromEffect": 5,
  "toEffect": 15,
  "transitionType": 5,
  "duration": 2000,
  "timestamp": 123456789
}
```

---

### Command: `zones.list`

Get all zones over WebSocket.

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
      "blendModeName": "OVERWRITE"
    }
  ],
  "presets": [
    {"id": 0, "name": "Unified"},
    {"id": 1, "name": "Dual Split"}
  ],
  "timestamp": 123456789
}
```

---

### Command: `zones.get`

Get specific zone over WebSocket.

**Send:**
```json
{
  "type": "zones.get",
  "zoneId": 0
}
```

**Receive:**
```json
{
  "type": "zones.zone",
  "success": true,
  "data": {
    "id": 0,
    "enabled": true,
    "effectId": 5
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
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

---

### Command: `zones.setEffect`

Set zone effect over WebSocket.

**Send:**
```json
{
  "type": "zones.setEffect",
  "zoneId": 0,
  "effectId": 9
}
```

**Receive:**
Broadcast to all clients:
```json
{
  "type": "zones.effectChanged",
  "zoneId": 0,
  "effectId": 9,
  "effectName": "Holographic Interference",
  "timestamp": 123456789
}
```

---

### Command: `zones.setLayout`

Set zone layout using custom segment definitions over WebSocket.

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

**Receive:**
```json
{
  "type": "zones.layoutChanged",
  "success": true,
  "zoneCount": 3,
  "timestamp": 123456789
}
```

**Error Response (Validation Failed):**
```json
{
  "type": "error",
  "success": false,
  "code": "INVALID_VALUE",
  "message": "Layout validation failed",
  "timestamp": 123456789
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

**Validation Rules:** Same as REST API endpoint (see `POST /api/v2/zones/layout`).

---

### Command: `batch`

Execute batch operations over WebSocket.

**Send:**
```json
{
  "type": "batch",
  "operations": [
    {
      "method": "PATCH",
      "path": "/api/v2/parameters",
      "body": {"brightness": 200}
    },
    {
      "method": "PUT",
      "path": "/api/v2/effects/current",
      "body": {"effectId": 12}
    }
  ]
}
```

**Receive:**
```json
{
  "type": "batch.result",
  "success": true,
  "data": {
    "processed": 2,
    "succeeded": 2,
    "failed": 0
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

---

### Command: `enhancements.get`

Get summary of all enhancement engines over WebSocket.

**Send:**
```json
{
  "type": "enhancements.get"
}
```

**Receive:**
```json
{
  "type": "enhancements.summary",
  "success": true,
  "data": {
    "colorEngine": {
      "available": true,
      "enabled": true
    },
    "motionEngine": {
      "available": true,
      "enabled": false
    }
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

---

### Command: `enhancements.color.get`

Get ColorEngine configuration over WebSocket.

**Send:**
```json
{
  "type": "enhancements.color.get"
}
```

**Receive:**
```json
{
  "type": "enhancements.color",
  "success": true,
  "data": {
    "enabled": true,
    "crossBlend": {
      "enabled": true,
      "palette1": 3,
      "palette2": 7,
      "palette3": null,
      "blend1": 128,
      "blend2": 127,
      "blend3": 0
    },
    "temporalRotation": {
      "enabled": true,
      "speed": 0.5,
      "phase": 0.0
    },
    "diffusion": {
      "enabled": false,
      "amount": 64
    }
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

---

### Command: `enhancements.color.set`

Update ColorEngine configuration over WebSocket.

**Send:**
```json
{
  "type": "enhancements.color.set",
  "enabled": true,
  "crossBlend": {
    "enabled": true,
    "palette1": 3,
    "palette2": 7,
    "palette3": null,
    "blend1": 128,
    "blend2": 127
  },
  "temporalRotation": {
    "enabled": true,
    "speed": 0.5
  },
  "diffusion": {
    "enabled": false,
    "amount": 64
  }
}
```

**Receive:**
```json
{
  "type": "enhancements.color",
  "success": true,
  "data": {
    "updated": ["enabled", "crossBlend", "temporalRotation"],
    "current": {
      "enabled": true,
      "crossBlend": {
        "enabled": true,
        "palette1": 3,
        "palette2": 7,
        "palette3": null
      }
    }
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

---

### Command: `enhancements.color.reset`

Reset ColorEngine to defaults over WebSocket.

**Send:**
```json
{
  "type": "enhancements.color.reset"
}
```

**Receive:**
```json
{
  "type": "enhancements.color",
  "success": true,
  "data": {
    "message": "Color engine reset to defaults"
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

---

### Command: `enhancements.motion.get`

Get MotionEngine configuration over WebSocket.

**Send:**
```json
{
  "type": "enhancements.motion.get"
}
```

**Receive:**
```json
{
  "type": "enhancements.motion",
  "success": true,
  "data": {
    "enabled": true,
    "warpStrength": 128,
    "warpFrequency": 64
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

---

### Command: `enhancements.motion.set`

Update MotionEngine configuration over WebSocket.

**Send:**
```json
{
  "type": "enhancements.motion.set",
  "enabled": true,
  "warpStrength": 128,
  "warpFrequency": 64
}
```

**Receive:**
```json
{
  "type": "enhancements.motion",
  "success": true,
  "data": {
    "updated": ["enabled", "warpStrength", "warpFrequency"],
    "current": {
      "enabled": true,
      "warpStrength": 128,
      "warpFrequency": 64
    }
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

---

### Command: `palettes.list`

Get list of all available color palettes over WebSocket.

**Send:**
```json
{
  "type": "palettes.list"
}
```

**Receive:**
```json
{
  "type": "palettes.list",
  "success": true,
  "data": {
    "palettes": [
      {
        "id": 0,
        "name": "Sunset Real",
        "colors": [
          {"r": 120, "g": 0, "b": 0},
          {"r": 166, "g": 21, "b": 17},
          {"r": 54, "g": 0, "b": 118},
          {"r": 112, "g": 0, "b": 10}
        ]
      },
      {
        "id": 1,
        "name": "Rivendell",
        "colors": [
          {"r": 1, "g": 14, "b": 5},
          {"r": 13, "g": 33, "b": 12}
        ]
      }
    ],
    "total": 75
  },
  "timestamp": 123456789,
  "version": "2.0.0"
}
```

---

### Command: `palettes.get`

Get specific palette details over WebSocket.

**Send:**
```json
{
  "type": "palettes.get",
  "requestId": "req-123",
  "paletteId": 0
}
```

**Receive:**
```json
{
  "type": "palettes.get",
  "requestId": "req-123",
  "success": true,
  "data": {
    "palette": {
      "id": 0,
      "name": "Sunset Real",
      "category": "artistic",
      "flags": {
        "warm": true,
        "cool": false,
        "calm": false,
        "vivid": true,
        "cvdFriendly": false,
        "whiteHeavy": false
      },
      "avgBrightness": 128,
      "maxBrightness": 255
    }
  },
  "timestamp": 123456789,
  "version": "2.0"
}
```

---

### Command: `palettes.set`

Set the current color palette over WebSocket.

**Send:**
```json
{
  "type": "palettes.set",
  "requestId": "req-124",
  "paletteId": 5
}
```

**Receive:**
```json
{
  "type": "palettes.set",
  "requestId": "req-124",
  "success": true,
  "data": {
    "paletteId": 5,
    "name": "Ocean",
    "category": "artistic"
  },
  "timestamp": 123456789,
  "version": "2.0"
}
```

**Note:** The legacy `setPalette` command is still supported for backward compatibility but `palettes.set` is the preferred command.

---

## Examples

### Complete Effect Setup

Set effect with custom parameters and transition:

```bash
# Set effect with transition
curl -X PUT http://lightwaveos.local/api/v2/effects/current \
  -H "Content-Type: application/json" \
  -d '{
    "effectId": 9,
    "transition": {
      "type": 5,
      "duration": 2000,
      "easing": 3
    }
  }'

# Update custom parameters for Holographic effect
curl -X PATCH http://lightwaveos.local/api/v2/parameters \
  -H "Content-Type: application/json" \
  -d '{
    "intensity": 150,
    "variation": 120
  }'
```

---

### Multi-Zone Configuration

Configure 4-zone setup with different effects:

```bash
# Enable zone system
curl -X POST http://lightwaveos.local/api/v2/zones \
  -H "Content-Type: application/json" \
  -d '{"enabled": true}'

# Configure each zone
curl -X PUT http://lightwaveos.local/api/v2/zones/0/effect \
  -H "Content-Type: application/json" \
  -d '{"effectId": 5}'

curl -X PUT http://lightwaveos.local/api/v2/zones/1/effect \
  -H "Content-Type: application/json" \
  -d '{"effectId": 9}'

curl -X PUT http://lightwaveos.local/api/v2/zones/2/effect \
  -H "Content-Type: application/json" \
  -d '{"effectId": 13}'

curl -X PUT http://lightwaveos.local/api/v2/zones/3/effect \
  -H "Content-Type: application/json" \
  -d '{"effectId": 22}'

# Set different brightness per zone
curl -X PATCH http://lightwaveos.local/api/v2/zones/0/parameters \
  -H "Content-Type: application/json" \
  -d '{"brightness": 255}'

curl -X PATCH http://lightwaveos.local/api/v2/zones/1/parameters \
  -H "Content-Type: application/json" \
  -d '{"brightness": 200}'
```

---

### Batch Operation for Scene Change

Change multiple settings atomically:

```bash
curl -X POST http://lightwaveos.local/api/v2/batch \
  -H "Content-Type: application/json" \
  -d '{
    "operations": [
      {
        "method": "PUT",
        "path": "/api/v2/effects/current",
        "body": {"effectId": 22}
      },
      {
        "method": "PATCH",
        "path": "/api/v2/parameters",
        "body": {
          "brightness": 220,
          "speed": 25,
          "paletteId": 7
        }
      },
      {
        "method": "PATCH",
        "path": "/api/v2/transitions/config",
        "body": {
          "enabled": true,
          "defaultType": 5
        }
      }
    ]
  }'
```

---

### Color Engine Configuration

Enable cross-palette blending:

```bash
curl -X PATCH http://lightwaveos.local/api/v2/enhancements/color \
  -H "Content-Type: application/json" \
  -d '{
    "enabled": true,
    "crossBlend": {
      "enabled": true,
      "palette1": 3,
      "palette2": 7,
      "blend1": 128,
      "blend2": 127
    },
    "temporalRotation": {
      "enabled": true,
      "speed": 0.5
    }
  }'
```

---

### WebSocket Real-Time Control

Full-featured WebSocket client:

```javascript
const ws = new WebSocket('ws://lightwaveos.local/ws');

ws.onopen = () => {
  console.log('Connected to LightwaveOS v2');

  // Get current status
  ws.send(JSON.stringify({
    type: "effects.getCurrent"
  }));

  // Subscribe to parameter changes
  ws.send(JSON.stringify({
    type: "parameters.get"
  }));
};

ws.onmessage = (event) => {
  const data = JSON.parse(event.data);

  switch (data.type) {
    case 'effects.current':
      console.log('Current effect:', data.data.name);
      updateUI(data.data);
      break;

    case 'effects.changed':
      console.log('Effect changed to:', data.name);
      break;

    case 'parameters.changed':
      console.log('Parameters updated:', data.updated);
      break;

    case 'zones.changed':
      console.log('Zone updated:', data.zoneId);
      break;

    case 'error':
      console.error('Error:', data.error.message);
      break;
  }
};

// Change effect with transition
function setEffect(effectId) {
  ws.send(JSON.stringify({
    type: "effects.setCurrent",
    effectId: effectId,
    transition: {
      type: 5,
      duration: 2000
    }
  }));
}

// Update brightness in real-time
function setBrightness(value) {
  ws.send(JSON.stringify({
    type: "parameters.set",
    brightness: value
  }));
}

// Batch update for scene
function loadScene(sceneData) {
  ws.send(JSON.stringify({
    type: "batch",
    operations: [
      {
        method: "PUT",
        path: "/api/v2/effects/current",
        body: {effectId: sceneData.effectId}
      },
      {
        method: "PATCH",
        path: "/api/v2/parameters",
        body: sceneData.parameters
      }
    ]
  }));
}
```

---

### Filter Effects by Category

Get all LGP Interference effects:

```bash
curl "http://lightwaveos.local/api/v2/effects?category=2&details=true"
```

---

### Get Custom Effect Parameters

Discover custom parameters for an effect:

```bash
curl http://lightwaveos.local/api/v2/effects/0

# Response shows custom params for Fire effect:
# {
#   "customParams": [
#     {"name": "Flame Height", "target": "intensity", "default": 180},
#     {"name": "Spark Rate", "target": "variation", "default": 100}
#   ]
# }

# Set "Flame Height" by updating intensity:
curl -X PATCH http://lightwaveos.local/api/v2/parameters \
  -H "Content-Type: application/json" \
  -d '{"intensity": 200}'
```

---

## Migration from v1

### URL Changes

| v1 | v2 |
|----|-----|
| `GET /api/v1/effects/metadata?id=9` | `GET /api/v2/effects/9` |
| `POST /api/v1/effects/set` | `PUT /api/v2/effects/current` |
| `POST /api/v1/parameters` | `PATCH /api/v2/parameters` |
| `POST /api/v1/transitions/config` | `PATCH /api/v2/transitions/config` |

### Request Body Changes

**v1 - Set Effect:**
```json
{"effectId": 12}
```

**v2 - Set Effect:**
```json
{
  "effectId": 12,
  "transition": {"type": 5, "duration": 2000}
}
```

### Response Format

v1 and v2 use the same response wrapper, but v2 includes `version: "2.0.0"`.

### Zone API

v1 had limited zone support. v2 provides full REST CRUD:

```bash
# v1 (limited)
POST /api/v1/zone/effect

# v2 (full CRUD)
GET /api/v2/zones
GET /api/v2/zones/{id}
PATCH /api/v2/zones/{id}
DELETE /api/v2/zones/{id}
PUT /api/v2/zones/{id}/effect
GET /api/v2/zones/{id}/parameters
PATCH /api/v2/zones/{id}/parameters
```

### WebSocket Commands

v2 adds namespaced commands:
- `device.getStatus`, `device.getInfo`
- `effects.list`, `effects.getCurrent`, `effects.setCurrent`
- `parameters.get`, `parameters.set`
- `zones.list`, `zones.get`, `zones.update`

v1 commands still work for backward compatibility.

---

## Notes

### CENTER ORIGIN Constraint

All effects must originate from LED 79/80 (center point). This is a hardware requirement for the Light Guide Plate (LGP) design.

Effects propagate:
- **OUTWARD** from center (79/80) to edges (0/159)
- **INWARD** from edges (0/159) to center (79/80)

Linear left-to-right or right-to-left patterns are not supported.

### Custom Effect Parameters

Effects with `customParams` provide semantic control labels:
- Fire: "Flame Height", "Spark Rate"
- Ocean: "Wave Height", "Turbulence", "Foam"
- Shockwave: "Pulse Width", "Expansion"

These map to underlying `VisualParams` (intensity, saturation, complexity, variation).

### Performance Considerations

- Target frame rate: 120 FPS
- WebSocket recommended for real-time updates
- Batch operations reduce HTTP overhead
- Rate limiting protects device stability
- Color engine adds ~2ms per frame when enabled

### Backward Compatibility

v2 API coexists with v1:
- `/api/v2/*` - New REST-first API
- `/api/v1/*` - Legacy v1 API (still functional)
- WebSocket supports both v1 and v2 command formats

---

**Documentation Version:** 2.0.0
**Last Updated:** 2025-12-21
**API Base:** LightwaveOS v2.0.0
