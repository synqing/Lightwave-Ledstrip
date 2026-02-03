# LightwaveOS API — Full Canonical Reference

**Version:** 1.0.0  
**Base URL:** `http://lightwaveos.local` or `http://<device-ip>`  
**WebSocket URL:** `ws://lightwaveos.local/ws`  
**Source:** Firmware v2 (`firmware/v2/src/network`), Tab5.encoder, lightwave-controller

---

## Table of Contents

1. [Overview](#overview)
2. [Response Format](#response-format)
3. [Rate Limiting](#rate-limiting)
4. [REST API](#rest-api)
5. [WebSocket Protocol](#websocket-protocol)
6. [Client Usage Matrix](#client-usage-matrix)
7. [Enums & Constants](#enums--constants)

---

## Overview

The LightwaveOS API controls an ESP32-S3 dual 320-LED strip system (Light Guide Plate). It provides:

- **REST** — HTTP JSON endpoints for discovery, configuration, and batch operations
- **WebSocket** — Real-time commands and status broadcasts

All responses use camelCase. Optional `X-API-Key` header for authenticated endpoints.

---

## Response Format

### Success

```json
{
  "success": true,
  "data": { ... },
  "timestamp": 123456789,
  "version": "1.0"
}
```

### Error

```json
{
  "success": false,
  "error": {
    "code": "RATE_LIMITED",
    "message": "Too many requests",
    "field": "effectId"
  },
  "timestamp": 123456789,
  "version": "1.0"
}
```

### Error Codes

| Code | HTTP | Description |
|------|------|-------------|
| `INVALID_JSON` | 400 | Invalid request body |
| `MISSING_FIELD` | 400 | Required field missing |
| `OUT_OF_RANGE` | 400 | Value out of valid range |
| `SCHEMA_MISMATCH` | 400 | Request doesn't match schema |
| `RATE_LIMITED` | 429 | Too many requests |
| `INTERNAL_ERROR` | 500 | Server error |

---

## Rate Limiting

| Protocol | Limit | Window |
|----------|-------|--------|
| HTTP | 20 req/s | 1 second |
| WebSocket | 50 msg/s | 1 second |

Exceeding returns 429 (HTTP) or error message (WS).

---

## REST API

### Discovery & Health

| Method | Path | Description | Auth |
|--------|------|-------------|------|
| GET | `/api/v1/ping` | `{"pong":true}` | No |
| GET | `/api/v1/` | API metadata, HATEOAS links | No |
| GET | `/api/v1/health` | Health status | No |

---

### Device

| Method | Path | Description | Auth |
|--------|------|-------------|------|
| GET | `/api/v1/device/status` | Runtime status | Yes |
| GET | `/api/v1/device/info` | Hardware/firmware info | Yes |
| GET | `/api/v1/device/ota-token` | OTA auth token | Yes |
| POST | `/api/v1/device/ota-token` | Rotate OTA token | Yes |

**GET /api/v1/device/status** response `data`:

| Field | Type | Description |
|-------|------|-------------|
| uptime | uint32 | Seconds since boot |
| freeHeap | uint32 | Free heap (bytes) |
| heapSize | uint32 | Total heap |
| cpuFreq | uint8 | CPU MHz |
| fps | float | Current FPS |
| cpuPercent | float | CPU usage |
| framesRendered | uint32 | Total frames |
| network.connected | bool | WiFi connected |
| network.apMode | bool | AP mode active |
| network.ip | string | IP address |
| network.rssi | int | WiFi signal |
| wsClients | size_t | WebSocket client count |

**GET /api/v1/device/info** response `data`:

| Field | Type | Description |
|-------|------|-------------|
| firmware | string | Version string |
| firmwareVersionNumber | uint32 | Version number |
| board | string | e.g. ESP32-S3-DevKitC-1 |
| sdk | string | ESP-IDF version |
| flashSize | uint32 | Flash size (bytes) |
| sketchSize | uint32 | App size |
| freeSketch | uint32 | Free flash |
| architecture | string | e.g. "Actor System v2" |

---

### Effects

| Method | Path | Description | Auth |
|--------|------|-------------|------|
| GET | `/api/v1/effects` | List effects (paginated) | Yes |
| GET | `/api/v1/effects/current` | Current effect | Yes |
| GET | `/api/v1/effects/metadata` | Effect metadata by id | Yes |
| GET | `/api/v1/effects/parameters` | Effect params by id | Yes |
| POST | `/api/v1/effects/set` | Set effect | Yes |
| PUT | `/api/v1/effects/current` | Set current effect | Yes |
| GET | `/api/v1/effects/families` | Effect families | Yes |

**GET /api/v1/effects** query params:

| Param | Type | Default | Description |
|-------|------|---------|-------------|
| page | int | 1 | Page number |
| limit | int | 20 | Items per page (max 150) |
| offset | int | 0 | Alternative to page |
| category | int | - | Filter by categoryId |
| details | bool | false | Include feature flags |

Response `data`:

| Field | Type | Description |
|-------|------|-------------|
| total | int | Total effects |
| offset | int | Current offset |
| limit | int | Limit used |
| effects | array | Effect objects |
| pagination | object | page, limit, total, pages |

Effect object:

| Field | Type | Description |
|-------|------|-------------|
| id | uint8 | Effect ID |
| name | string | Display name |
| category | string | Category name |
| categoryId | int | Category ID |
| isAudioReactive | bool | Audio-reactive flag |
| centerOrigin | bool | Centre-origin compliant |
| usesSpeed | bool | Uses speed param |
| usesPalette | bool | Uses palette |
| zoneAware | bool | Zone compatible |
| dualStrip | bool | Dual-strip aware |
| physicsBased | bool | Physics-based |

**POST /api/v1/effects/set** body: `{"effectId": N}`

---

### Parameters (Visual)

| Method | Path | Description | Auth |
|--------|------|-------------|------|
| GET | `/api/v1/parameters` | All params | Yes |
| POST | `/api/v1/parameters` | Set params | Yes |
| PATCH | `/api/v1/parameters` | Partial update | Yes |

Parameter fields (all 0-255 unless noted):

| Field | Range | Description |
|-------|-------|-------------|
| brightness | 0-255 | Global brightness |
| speed | 1-50 | Animation speed |
| paletteId | 0-N | Palette ID |
| mood | 0-255 | Mood parameter |
| fadeAmount | 0-255 | Fade amount |
| intensity | 0-255 | Intensity |
| saturation | 0-255 | Saturation |
| complexity | 0-255 | Complexity |
| variation | 0-255 | Variation |

---

### Palettes

| Method | Path | Description | Auth |
|--------|------|-------------|------|
| GET | `/api/v1/palettes` | List palettes | Yes |
| GET | `/api/v1/palettes/current` | Current palette | Yes |
| POST | `/api/v1/palettes/set` | Set palette | Yes |

**GET /api/v1/palettes** query params: `page`, `limit`, `offset` (same as effects).

Response `data.palettes[]`:

| Field | Type | Description |
|-------|------|-------------|
| id | uint8 | Palette ID |
| name | string | Display name |
| category | string | e.g. CPT-City, Crameri |
| flags | uint8 | Flags |
| avgBrightness | uint8 | Avg brightness |
| maxBrightness | uint8 | Max brightness |

**POST /api/v1/palettes/set** body: `{"paletteId": N}`

---

### Zones

| Method | Path | Description | Auth |
|--------|------|-------------|------|
| GET | `/api/v1/zones` | Full zone state | Yes |
| POST | `/api/v1/zones/layout` | Set layout | Yes |
| POST | `/api/v1/zones/enabled` | Enable/disable zones | Yes |
| GET | `/api/v1/zones/{0-3}` | Single zone | Yes |
| POST | `/api/v1/zones/{0-3}/effect` | Set zone effect | Yes |
| POST | `/api/v1/zones/{0-3}/brightness` | Set zone brightness | Yes |
| POST | `/api/v1/zones/{0-3}/speed` | Set zone speed | Yes |
| POST | `/api/v1/zones/{0-3}/palette` | Set zone palette | Yes |
| POST | `/api/v1/zones/{0-3}/blend` | Set zone blend | Yes |
| POST | `/api/v1/zones/{0-3}/enabled` | Enable/disable zone | Yes |
| GET | `/api/v1/zones/config` | Zone config status | Yes |
| POST | `/api/v1/zones/config/save` | Save to NVS | Yes |
| POST | `/api/v1/zones/config/load` | Load from NVS | Yes |

**GET /api/v1/zones** response `data`:

| Field | Type | Description |
|-------|------|-------------|
| enabled | bool | Zone system enabled |
| zoneCount | uint8 | Number of zones (1-4) |
| segments | array | LED segment defs |
| zones | array | Per-zone state |
| presets | array | Built-in presets |

Segment object:

| Field | Type | Description |
|-------|------|-------------|
| zoneId | uint8 | 0-3 |
| s1LeftStart | uint8 | Left segment start (0-79) |
| s1LeftEnd | uint8 | Left segment end |
| s1RightStart | uint8 | Right segment start (80-159) |
| s1RightEnd | uint8 | Right segment end |
| totalLeds | uint8 | LEDs in zone |

Zone object:

| Field | Type | Description |
|-------|------|-------------|
| id | uint8 | Zone ID |
| enabled | bool | Zone enabled |
| effectId | uint8 | Effect ID |
| effectName | string | Effect display name |
| brightness | uint8 | Zone brightness |
| speed | uint8 | Zone speed |
| paletteId | uint8 | Palette ID |
| blendMode | uint8 | 0-7 |
| blendModeName | string | Blend mode name |

Preset object: `{"id": 0-4, "name": "Unified"|"Dual Split"|...}`

**POST /api/v1/zones/layout** body:

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

Layout validation: centre-origin symmetry, no overlaps, complete coverage, zones ordered centre-outward.

**POST /api/v1/zones/enabled** body: `{"enabled": true|false}`

**POST /api/v1/zones/{id}/effect** body: `{"effectId": N}`  
**POST /api/v1/zones/{id}/speed** body: `{"speed": N}`  
**POST /api/v1/zones/{id}/palette** body: `{"paletteId": N}`  
**POST /api/v1/zones/{id}/blend** body: `{"blendMode": N}`

---

### Transitions

| Method | Path | Description | Auth |
|--------|------|-------------|------|
| GET | `/api/v1/transitions/types` | Available types | Yes |
| GET | `/api/v1/transitions/config` | Current config | Yes |
| POST | `/api/v1/transitions/config` | Set config | Yes |
| POST | `/api/v1/transitions/trigger` | Trigger transition | Yes |

**POST /api/v1/transitions/trigger** body: `{"toEffect": N, "type": 0-11, "duration": ms}`

---

### Batch

| Method | Path | Description | Auth |
|--------|------|-------------|------|
| POST | `/api/v1/batch` | Execute up to 10 ops | Yes |

Body: `{"operations": [{"action": "setBrightness", "value": 200}, ...]}`

Actions: `setBrightness`, `setSpeed`, `setEffect`, `setPalette`, `setZoneEffect`, `setZoneBrightness`, `setZoneSpeed`

---

### Network (Tab5 Connectivity UI)

| Method | Path | Description | Auth |
|--------|------|-------------|------|
| GET | `/api/v1/network/status` | Network status | No |
| GET | `/api/v1/network/networks` | Saved networks | Yes |
| POST | `/api/v1/network/networks` | Add network | Yes |
| DELETE | `/api/v1/network/networks/{ssid}` | Delete network | Yes |
| POST | `/api/v1/network/connect` | Connect to network | Yes |
| POST | `/api/v1/network/disconnect` | Disconnect | Yes |
| GET | `/api/v1/network/scan` | Scan networks | Yes |

---

### Color Correction

| Method | Path | Description | Auth |
|--------|------|-------------|------|
| GET | `/api/v1/colorCorrection/config` | Get config | Yes |
| POST | `/api/v1/colorCorrection/config` | Set config | Yes |
| POST | `/api/v1/colorCorrection/mode` | Set mode | Yes |
| GET | `/api/v1/colorCorrection/presets` | Presets | Yes |
| POST | `/api/v1/colorCorrection/preset` | Apply preset | Yes |

---

### Firmware / OTA

| Method | Path | Description | Auth |
|--------|------|-------------|------|
| GET | `/api/v1/firmware/version` | Version string | No |
| POST | `/api/v1/firmware/update` | OTA update (multipart) | X-OTA-Token |
| POST | `/update` | Legacy OTA | X-OTA-Token |

---

## WebSocket Protocol

Connect to `ws://lightwaveos.local/ws`. All messages are JSON with `type` field.

### Client → Server (Commands)

| type | Params | Description |
|------|--------|-------------|
| `getStatus` | — | Request full status (legacy alias) |
| `device.getStatus` | — | Device status |
| `device.getInfo` | — | Device info |
| `effects.list` | page, limit, details, requestId | Effect list |
| `effects.getCurrent` | — | Current effect |
| `effects.getMetadata` | effectId | Effect metadata |
| `effects.setCurrent` | effectId | Set effect |
| `effects.getCategories` | — | Categories |
| `effects.parameters.get` | id | Effect params |
| `effects.parameters.set` | id, params | Set effect params |
| `parameters.get` | — | All params |
| `parameters.set` | brightness, speed, paletteId, mood, fadeAmount, complexity, variation | Set params |
| `palettes.list` | page, limit, requestId | Palette list |
| `palettes.get` | — | Palettes |
| `palettes.set` | paletteId | Set palette |
| `zones.get` | — | Zone state (segments + zones + presets) |
| `zone.enable` | enable | Enable/disable zone system |
| `zone.setEffect` | zoneId, effectId | Zone effect |
| `zone.setBrightness` | zoneId, brightness | Zone brightness |
| `zone.setSpeed` | zoneId, speed | Zone speed |
| `zone.setPalette` | zoneId, paletteId | Zone palette |
| `zone.setBlend` | zoneId, blendMode | Zone blend (0-7) |
| `zone.loadPreset` | presetId | Load built-in preset (0-4) |
| `zones.setLayout` | zones: [{zoneId, s1LeftStart, s1LeftEnd, s1RightStart, s1RightEnd}, ...] | Custom layout |
| `zones.update` | zoneId, effectId?, brightness?, speed?, paletteId?, blendMode? | Multi-field zone update |
| `colorCorrection.getConfig` | — | Color correction state |
| `colorCorrection.setConfig` | gammaEnabled, gammaValue, autoExposureEnabled, autoExposureTarget, brownGuardrailEnabled, mode | Full config |
| `colorCorrection.setGamma` | enabled, value | Gamma only |
| `colorCorrection.setAutoExposure` | enabled, target | AE only |
| `colorCorrection.setBrownGuardrail` | enabled | Brown guardrail |
| `colorCorrection.setMode` | mode | 0=OFF, 1=HSV, 2=RGB, 3=BOTH |
| `transition.trigger` | toEffect, type?, duration? | Trigger transition |
| `transition.getTypes` | — | Transition types |
| `transition.config` | enabled | Transition config |
| `batch` | operations: [{action, ...}] | Batch ops |
| `ledStream.subscribe` | — | LED binary stream |
| `ledStream.unsubscribe` | — | Unsubscribe |
| `validation.subscribe` | — | Validation events |
| `validation.unsubscribe` | — | Unsubscribe |
| `audio.parameters.get` | — | Audio params |
| `audio.parameters.set` | ... | Audio params |
| `audio.subscribe` | — | Audio events |
| `audio.unsubscribe` | — | Unsubscribe |
| `auth.status` | — | Auth status |
| `auth.rotate` | — | Rotate API key |
| `sys.capabilities` | — | Capabilities |
| `ota.check` | — | OTA check |
| `ota.begin` | — | OTA begin |
| `ota.chunk` | — | OTA chunk |
| `ota.abort` | — | OTA abort |
| `ota.verify` | — | OTA verify |

### Server → Client (Messages)

| type | Description |
|------|-------------|
| `status` | Full parameter sync (uptime, effectId, brightness, speed, paletteId, mood, fadeAmount, complexity, variation, paletteName) |
| `device.status` | Device info (uptime, etc.) |
| `parameters.changed` | Params changed notification |
| `zone.status` | Zone sync (zones array with effectId, speed, paletteId) |
| `zones.changed` | Zone config changed (triggers refresh) |
| `zones.list` | Zone state response: enabled, zoneCount, segments, zones, presets |
| `effects.changed` | Effects changed |
| `effects.list` | Effect list response |
| `palettes.list` | Palette list response |
| `colorCorrection.getConfig` | Color correction response |
| `zones.effectChanged` | Zone effect changed |
| `zones.changed` | Zone updated |
| `zones.layoutChanged` | Layout changed |
| `zone.enabledChanged` | Zone system enabled/disabled |

### zones.list Response Structure

```json
{
  "type": "zones.list",
  "success": true,
  "enabled": true,
  "zoneCount": 3,
  "segments": [
    {"zoneId": 0, "s1LeftStart": 65, "s1LeftEnd": 79, "s1RightStart": 80, "s1RightEnd": 94, "totalLeds": 30}
  ],
  "zones": [
    {"id": 0, "enabled": true, "effectId": 5, "effectName": "Shockwave", "speed": 20, "paletteId": 0, "paletteName": "Sunset Real", "blendMode": 0, "blendModeName": "Overwrite"}
  ],
  "presets": [{"id": 0, "name": "Unified"}, {"id": 1, "name": "Dual Split"}, ...]
}
```

---

## Client Usage Matrix

| Capability | Tab5.encoder | lightwave-controller | lightwave-ios |
|------------|--------------|----------------------|---------------|
| Device discovery | GET /api/v1/device/info | mDNS + device/info | mDNS + manual |
| Status sync | getStatus (WS) | REST + WS status | getStatus (WS) |
| Effects list | effects.list (WS) | GET /api/v1/effects | REST |
| Palettes list | palettes.list (WS) | GET /api/v1/palettes | Hardcoded (gap) |
| Parameters | parameters.set (WS) | POST /api/v1/parameters | parameters.set (WS) |
| Zones | zones.get, zone.* (WS) | GET/POST /api/v1/zones/* | REST + WS (segments gap) |
| Zone layout | zones.setLayout (WS) | POST /api/v1/zones/layout | — |
| Zone preset | zone.loadPreset (WS) | Layout push | — |
| Color correction | colorCorrection.* (WS) | — | — |
| Network config | REST /api/v1/network/* | — | — |

---

## Enums & Constants

### Zone Presets (Built-in)

| ID | Name |
|----|------|
| 0 | Unified |
| 1 | Dual Split |
| 2 | Triple Rings |
| 3 | Quad Active |
| 4 | Heartbeat Focus |

### Blend Modes

| ID | Name |
|----|------|
| 0 | Overwrite |
| 1 | Additive |
| 2 | Multiply |
| 3 | Screen |
| 4 | Overlay |
| 5 | Alpha |
| 6 | Lighten |
| 7 | Darken |

### LED Topology

- Total: 320 LEDs (160 per strip)
- Centre: LEDs 79/80 (between strip 1 and 2)
- Strip 1: 0-159 (left), Strip 2: 160-319 (logical) / s1 right 80-159
- All effects must originate from centre (79/80) and propagate outward or inward

### Colour Correction Mode

| ID | Name |
|----|------|
| 0 | OFF |
| 1 | HSV |
| 2 | RGB |
| 3 | BOTH |

---

### Additional REST Endpoints

These exist in firmware but are not covered in full detail here. See `docs/api/api-v1.md` and `firmware/v2/src/network/webserver/V1ApiRoutes.cpp` for implementation:

- **Audio:** `/api/v1/audio/parameters`, `/api/v1/audio/state`, `/api/v1/audio/tempo`, `/api/v1/audio/fft`, `/api/v1/audio/presets`, `/api/v1/audio/mappings`, `/api/v1/audio/zone-agc`, `/api/v1/audio/spike-detection`, `/api/v1/audio/calibrate`, `/api/v1/audio/benchmark`, `/api/v1/debug/audio`
- **Narrative:** `/api/v1/narrative/status`, `/api/v1/narrative/config`
- **Shows:** `/api/v1/shows`, `/api/v1/shows/current`, `/api/v1/shows/control`
- **Presets:** `/api/v1/presets`, `/api/v1/presets/{name}/load`, `/api/v1/presets/save-current`
- **Effect/Zone Presets:** `/api/v1/effect-presets`, `/api/v1/zone-presets`
- **Modifiers:** `/api/v1/modifiers/list`, add, remove, update, clear
- **Filesystem:** `/api/v1/filesystem/status`, mount, unmount, restart
- **Auth:** `/api/v1/auth/status`, rotate, key (when `FEATURE_API_AUTH` enabled)

---

## References

- Firmware: `firmware/v2/src/network/webserver/`
- Tab5: `firmware/Tab5.encoder/src/network/WebSocketClient.cpp`, `HttpClient.cpp`, `WsMessageRouter.h`
- Dashboard: `lightwave-controller/static/app.js`
- Extended docs: `docs/api/api-v1.md`

---

*Generated from firmware v2, Tab5.encoder, and lightwave-controller. Last updated: 2026-02-01.*
