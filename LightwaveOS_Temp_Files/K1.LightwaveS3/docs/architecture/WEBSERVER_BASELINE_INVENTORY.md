# WebServer Baseline Inventory

**Purpose**: Complete contract inventory of all HTTP routes and WebSocket commands before modular refactor. This document serves as the compatibility baseline.

**Generated**: Pre-refactor baseline for v2 WebServer modularisation  
**File**: `v2/src/network/WebServer.cpp` (6633 LOC)

---

## HTTP Routes Inventory

### Static Asset Routes

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| GET | `/` | No | Serves `index.html` from LittleFS or fallback HTML |
| GET | `/favicon.ico` | No | Serves favicon from LittleFS or 204 No Content |
| GET | `/app.js` | No | Serves app.js from LittleFS |
| GET | `/styles.css` | No | Serves styles.css from LittleFS |
| GET | `/assets/app.js` | No | Serves dashboard assets/app.js |
| GET | `/assets/styles.css` | No | Serves dashboard assets/styles.css |
| GET | `/vite.svg` | No | Serves vite.svg logo |
| * | `*` (404 handler) | No | SPA fallback: serves index.html for non-API paths without extensions |

### Legacy API Routes (`/api/*`)

| Method | Path | Auth | Handler | Description |
|--------|------|------|---------|-------------|
| GET | `/api/status` | Yes | `handleLegacyStatus` | Returns effect, brightness, speed, palette, heap, network info |
| POST | `/api/effect` | Yes | `handleLegacySetEffect` | Sets current effect (body: `{"effect": N}`) |
| POST | `/api/brightness` | Yes | `handleLegacySetBrightness` | Sets brightness (body: `{"brightness": N}`) |
| POST | `/api/speed` | Yes | `handleLegacySetSpeed` | Sets speed (body: `{"speed": N}`) |
| POST | `/api/palette` | Yes | `handleLegacySetPalette` | Sets palette (body: `{"palette": N}`) |
| POST | `/api/zone/count` | Yes | `handleLegacyZoneCount` | Sets zone count (body: `{"count": N}`) |
| POST | `/api/zone/effect` | Yes | `handleLegacyZoneEffect` | Sets zone effect (body: `{"zone": N, "effect": M}`) |
| POST | `/api/zone/config/save` | Yes | Inline | Saves zone config + system state to NVS |
| POST | `/api/zone/config/load` | Yes | Inline | Loads zone config + system state from NVS |
| POST | `/api/zone/preset/load` | Yes | Inline | Loads zone preset (body: `{"preset": 0-4}`) |
| GET | `/api/network/status` | Yes | Inline | Returns WiFi state, SSID, IP, RSSI, MAC, channel, stats |
| GET | `/api/network/scan` | Yes | Inline | Returns cached WiFi scan results |
| POST | `/api/network/connect` | Yes | Inline | Connects to WiFi (body: `{"ssid": "...", "password": "..."}`) |
| POST | `/api/network/disconnect` | Yes | Inline | Disconnects from WiFi |
| POST | `/update` | Yes (X-OTA-Token) | Inline | OTA firmware update endpoint |

### V1 API Routes (`/api/v1/*`)

#### Discovery & Health

| Method | Path | Auth | Handler | Description |
|--------|------|------|---------|-------------|
| GET | `/api/v1/` | No | `handleApiDiscovery` | API discovery endpoint (HATEOAS links) |
| GET | `/api/v1/health` | No | `handleHealth` | Health check endpoint |

#### Device

| Method | Path | Auth | Handler | Description |
|--------|------|------|---------|-------------|
| GET | `/api/v1/device/status` | Yes | `DeviceHandlers::handleStatus` | Device status (uptime, heap, FPS, network) |
| GET | `/api/v1/device/info` | Yes | `DeviceHandlers::handleInfo` | Device info (chip model, flash size, etc.) |

#### Sync (FEATURE_MULTI_DEVICE)

| Method | Path | Auth | Handler | Description |
|--------|------|------|---------|-------------|
| GET | `/api/v1/sync/status` | Yes | Inline | Sync status with device UUID |

#### Effects

| Method | Path | Auth | Handler | Description |
|--------|------|------|---------|-------------|
| GET | `/api/v1/effects/metadata?id=N` | Yes | `EffectHandlers::handleMetadata` | Effect metadata by ID |
| GET | `/api/v1/effects/parameters?id=N` | Yes | `EffectHandlers::handleParametersGet` | Effect parameters by ID |
| POST | `/api/v1/effects/parameters` | Yes | `EffectHandlers::handleParametersSet` | Set effect parameters |
| PATCH | `/api/v1/effects/parameters` | Yes | `EffectHandlers::handleParametersSet` | Set effect parameters (compat) |
| GET | `/api/v1/effects/families` | Yes | `EffectHandlers::handleFamilies` | List effect families |
| GET | `/api/v1/effects` | Yes | `EffectHandlers::handleList` | List all effects |
| GET | `/api/v1/effects/current` | Yes | `EffectHandlers::handleCurrent` | Get current effect |
| POST | `/api/v1/effects/set` | Yes | `EffectHandlers::handleSet` | Set effect |
| PUT | `/api/v1/effects/current` | Yes | `EffectHandlers::handleSet` | Set effect (compat) |

#### Parameters

| Method | Path | Auth | Handler | Description |
|--------|------|------|---------|-------------|
| GET | `/api/v1/parameters` | Yes | `handleParametersGet` | Get global parameters |
| POST | `/api/v1/parameters` | Yes | `handleParametersSet` | Set global parameters |
| PATCH | `/api/v1/parameters` | Yes | `handleParametersSet` | Set global parameters (compat) |

#### Audio (FEATURE_AUDIO_SYNC)

| Method | Path | Auth | Handler | Description |
|--------|------|------|---------|-------------|
| GET | `/api/v1/audio/parameters` | Yes | `handleAudioParametersGet` | Get audio parameters |
| POST | `/api/v1/audio/parameters` | Yes | `handleAudioParametersSet` | Set audio parameters |
| PATCH | `/api/v1/audio/parameters` | Yes | `handleAudioParametersSet` | Set audio parameters (compat) |
| POST | `/api/v1/audio/control` | Yes | `handleAudioControl` | Pause/resume audio |
| GET | `/api/v1/audio/state` | Yes | `handleAudioStateGet` | Get audio state |
| GET | `/api/v1/audio/tempo` | Yes | `handleAudioTempoGet` | Get tempo/BPM |
| GET | `/api/v1/audio/presets` | Yes | `handleAudioPresetsList` | List audio presets |
| POST | `/api/v1/audio/presets` | Yes | `handleAudioPresetSave` | Save audio preset |
| GET | `/api/v1/audio/presets/get?id=X` | Yes | `handleAudioPresetGet` | Get preset by ID |
| POST | `/api/v1/audio/presets/apply?id=X` | Yes | `handleAudioPresetApply` | Apply preset |
| DELETE | `/api/v1/audio/presets/delete?id=X` | Yes | `handleAudioPresetDelete` | Delete preset |
| GET | `/api/v1/audio/mappings/sources` | Yes | `handleAudioMappingsListSources` | List audio source types |
| GET | `/api/v1/audio/mappings/targets` | Yes | `handleAudioMappingsListTargets` | List visual target types |
| GET | `/api/v1/audio/mappings/curves` | Yes | `handleAudioMappingsListCurves` | List curve types |
| GET | `/api/v1/audio/mappings` | Yes | `handleAudioMappingsList` | List all effect mappings |
| GET | `/api/v1/audio/mappings/effect?id=X` | Yes | `handleAudioMappingsGet` | Get mapping for effect |
| POST | `/api/v1/audio/mappings/effect?id=X` | Yes | `handleAudioMappingsSet` | Set mapping for effect |
| DELETE | `/api/v1/audio/mappings/effect?id=X` | Yes | `handleAudioMappingsDelete` | Delete mapping |
| POST | `/api/v1/audio/mappings/enable?id=X` | Yes | `handleAudioMappingsEnable` | Enable mapping |
| POST | `/api/v1/audio/mappings/disable?id=X` | Yes | `handleAudioMappingsEnable` | Disable mapping |
| GET | `/api/v1/audio/mappings/stats` | Yes | `handleAudioMappingsStats` | Get mapping statistics |
| GET | `/api/v1/audio/zone-agc` | Yes | `handleAudioZoneAGCGet` | Get Zone AGC state |
| POST | `/api/v1/audio/zone-agc` | Yes | `handleAudioZoneAGCSet` | Set Zone AGC settings |
| GET | `/api/v1/audio/spike-detection` | Yes | `handleAudioSpikeDetectionGet` | Get spike detection stats |
| POST | `/api/v1/audio/spike-detection/reset` | Yes | `handleAudioSpikeDetectionReset` | Reset spike stats |
| GET | `/api/v1/audio/calibrate` | Yes | `handleAudioCalibrateStatus` | Get calibration status |
| POST | `/api/v1/audio/calibrate/start` | Yes | `handleAudioCalibrateStart` | Start calibration |
| POST | `/api/v1/audio/calibrate/cancel` | Yes | `handleAudioCalibrateCancel` | Cancel calibration |
| POST | `/api/v1/audio/calibrate/apply` | Yes | `handleAudioCalibrateApply` | Apply calibration |

#### Audio Benchmark (FEATURE_AUDIO_BENCHMARK)

| Method | Path | Auth | Handler | Description |
|--------|------|------|---------|-------------|
| GET | `/api/v1/audio/benchmark` | Yes | `handleBenchmarkGet` | Get benchmark metrics |
| POST | `/api/v1/audio/benchmark/start` | Yes | `handleBenchmarkStart` | Start benchmark |
| POST | `/api/v1/audio/benchmark/stop` | Yes | `handleBenchmarkStop` | Stop benchmark |
| GET | `/api/v1/audio/benchmark/history` | Yes | `handleBenchmarkHistory` | Get benchmark history |

#### Transitions

| Method | Path | Auth | Handler | Description |
|--------|------|------|---------|-------------|
| GET | `/api/v1/transitions/types` | Yes | `handleTransitionTypes` | List transition types |
| POST | `/api/v1/transitions/trigger` | Yes | `handleTransitionTrigger` | Trigger transition |
| GET | `/api/v1/transitions/config` | Yes | `handleTransitionConfigGet` | Get transition config |
| POST | `/api/v1/transitions/config` | Yes | `handleTransitionConfigSet` | Set transition config |

#### Palettes

| Method | Path | Auth | Handler | Description |
|--------|------|------|---------|-------------|
| GET | `/api/v1/palettes` | Yes | `handlePalettesList` | List all palettes |
| GET | `/api/v1/palettes/current` | Yes | `handlePalettesCurrent` | Get current palette |
| POST | `/api/v1/palettes/set` | Yes | `handlePalettesSet` | Set palette |

#### Batch Operations

| Method | Path | Auth | Handler | Description |
|--------|------|------|---------|-------------|
| POST | `/api/v1/batch` | Yes | `handleBatch` | Execute batch operations |

#### Narrative

| Method | Path | Auth | Handler | Description |
|--------|------|------|---------|-------------|
| GET | `/api/v1/narrative/status` | Yes | `handleNarrativeStatus` | Get narrative status |
| GET | `/api/v1/narrative/config` | Yes | `handleNarrativeConfigGet` | Get narrative config |
| POST | `/api/v1/narrative/config` | Yes | `handleNarrativeConfigSet` | Set narrative config |

#### Zones

| Method | Path | Auth | Handler | Description |
|--------|------|------|---------|-------------|
| GET | `/api/v1/zones` | Yes | `ZoneHandlers::handleList` | List all zones |
| POST | `/api/v1/zones/layout` | Yes | `ZoneHandlers::handleLayout` | Set zone layout |
| GET | `/api/v1/zones/:id` | Yes | `ZoneHandlers::handleGet` | Get zone by ID (0-3) |
| POST | `/api/v1/zones/:id/effect` | Yes | `ZoneHandlers::handleSetEffect` | Set zone effect |
| POST | `/api/v1/zones/:id/brightness` | Yes | `ZoneHandlers::handleSetBrightness` | Set zone brightness |
| POST | `/api/v1/zones/:id/speed` | Yes | `ZoneHandlers::handleSetSpeed` | Set zone speed |
| POST | `/api/v1/zones/:id/palette` | Yes | `ZoneHandlers::handleSetPalette` | Set zone palette |
| POST | `/api/v1/zones/:id/blend` | Yes | `ZoneHandlers::handleSetBlend` | Set zone blend mode |
| POST | `/api/v1/zones/:id/enabled` | Yes | `ZoneHandlers::handleSetEnabled` | Set zone enabled |
| POST | `/api/v1/zones/enabled` | Yes | Inline | Set global zone mode enabled |

#### OpenAPI

| Method | Path | Auth | Handler | Description |
|--------|------|------|---------|-------------|
| GET | `/api/v1/openapi.json` | No | `handleOpenApiSpec` | OpenAPI 3.0 specification |

---

## WebSocket Commands Inventory

**Endpoint**: `/ws`  
**Message Format**: JSON with `type` field (required), optional `requestId`  
**Response Format**: JSON with `type`, `success`, `data`/`error`, optional `requestId`

### Authentication (FEATURE_API_AUTH)

| Command | Auth Required | Description |
|---------|---------------|-------------|
| `auth` | No (first message) | Authenticate with API key: `{"type":"auth","apiKey":"..."}` |

### Effect Control

| Command | Response Type | Description |
|---------|---------------|-------------|
| `setEffect` | (broadcast) | Set current effect: `{"type":"setEffect","effectId":N}` |
| `nextEffect` | (broadcast) | Cycle to next effect |
| `prevEffect` | (broadcast) | Cycle to previous effect |
| `effects.getCurrent` | `effects.current` | Get current effect info |
| `effects.list` | `effects.list` | List all effects |
| `effects.setCurrent` | `effects.set` | Set current effect (with response) |
| `effects.getMetadata` | `effects.metadata` | Get effect metadata by ID |
| `effects.parameters.get` | `effects.parameters` | Get effect parameters |
| `effects.parameters.set` | `effects.parameters.set` | Set effect parameters |
| `effects.getCategories` | `effects.categories` | Get effect categories/families |
| `effects.getByFamily` | `effects.byFamily` | Get effects by family ID |

### Parameter Control

| Command | Response Type | Description |
|---------|---------------|-------------|
| `setBrightness` | (broadcast) | Set brightness: `{"type":"setBrightness","value":N}` |
| `setSpeed` | (broadcast) | Set speed: `{"type":"setSpeed","value":N}` |
| `setPalette` | (broadcast) | Set palette: `{"type":"setPalette","paletteId":N}` |
| `parameters.get` | `parameters` | Get global parameters |
| `parameters.set` | `parameters.set` | Set global parameters |

### Zone Control

| Command | Response Type | Description |
|---------|---------------|-------------|
| `zone.enable` | (event) | Enable/disable zone mode: `{"type":"zone.enable","enable":true}` |
| `zone.setEffect` | (event) | Set zone effect: `{"type":"zone.setEffect","zone":N,"effect":M}` |
| `zone.setBrightness` | (event) | Set zone brightness |
| `zone.setSpeed` | (event) | Set zone speed |
| `zone.setPalette` | (event) | Set zone palette |
| `zone.loadPreset` | (event) | Load zone preset: `{"type":"zone.loadPreset","preset":0-4}` |
| `getZoneState` | `zone.state` | Get current zone state |
| `zones.get` | `zones.get` | Get zone by ID |
| `zones.list` | `zones.list` | List all zones |
| `zones.update` | `zones.update` | Update zone (multi-field) |
| `zones.setEffect` | `zones.setEffect` | Set zone effect (with response) |

### Transition Control

| Command | Response Type | Description |
|---------|---------------|-------------|
| `transition.trigger` | (broadcast) | Trigger transition: `{"type":"transition.trigger","toEffect":N,"transitionType":M,"random":false}` |
| `transition.getTypes` | `transition.types` | Get available transition types |
| `transition.config` | `transition.config` | Get/set transition config (conditional on fields) |
| `transitions.list` | `transitions.list` | List transitions |
| `transitions.trigger` | `transitions.trigger` | Trigger transition (with response) |

### Audio Control (FEATURE_AUDIO_SYNC)

| Command | Response Type | Description |
|---------|---------------|-------------|
| `audio.parameters.get` | `audio.parameters` | Get audio parameters |
| `audio.parameters.set` | `audio.parameters.set` | Set audio parameters |
| `audio.subscribe` | `audio.subscribed` | Subscribe to audio stream |
| `audio.unsubscribe` | `audio.unsubscribed` | Unsubscribe from audio stream |
| `audio.zone-agc.get` | `audio.zone-agc.state` | Get Zone AGC state |
| `audio.zone-agc.set` | `audio.zone-agc.updated` | Set Zone AGC settings |
| `audio.spike-detection.get` | `audio.spike-detection.state` | Get spike detection stats |
| `audio.spike-detection.reset` | `audio.spike-detection.reset` | Reset spike stats |

### Device & Status

| Command | Response Type | Description |
|---------|---------------|-------------|
| `getStatus` | `status` | Get device status (legacy format) |
| `device.getStatus` | `device.status` | Get device status (v1 format) |
| `device.getInfo` | `device.info` | Get device info |

### Streaming Subscriptions

| Command | Response Type | Description |
|---------|---------------|-------------|
| `ledStream.subscribe` | `ledStream.subscribed` | Subscribe to LED frame stream (binary) |
| `ledStream.unsubscribe` | `ledStream.unsubscribed` | Unsubscribe from LED stream |
| `benchmark.subscribe` | `benchmark.subscribed` | Subscribe to benchmark stream (FEATURE_AUDIO_BENCHMARK) |
| `benchmark.unsubscribe` | `benchmark.unsubscribed` | Unsubscribe from benchmark stream |
| `validation.subscribe` | `validation.subscribed` | Subscribe to validation stream (FEATURE_EFFECT_VALIDATION) |
| `validation.unsubscribe` | `validation.unsubscribed` | Unsubscribe from validation stream |

### Benchmark (FEATURE_AUDIO_BENCHMARK)

| Command | Response Type | Description |
|---------|---------------|-------------|
| `benchmark.start` | `benchmark.started` | Start benchmark |
| `benchmark.stop` | `benchmark.stopped` | Stop benchmark |
| `benchmark.get` | `benchmark.state` | Get benchmark metrics |

### Narrative

| Command | Response Type | Description |
|---------|---------------|-------------|
| `narrative.getStatus` | `narrative.status` | Get narrative status |
| `narrative.config` | `narrative.config` | Get/set narrative config (conditional on fields) |

### Palettes

| Command | Response Type | Description |
|---------|---------------|-------------|
| `palettes.list` | `palettes.list` | List all palettes |
| `palettes.get` | `palettes.get` | Get palette by ID |

### Motion Engine

| Command | Response Type | Description |
|---------|---------------|-------------|
| `motion.getStatus` | `motion.status` | Get motion engine status |
| `motion.enable` | `motion.enabled` | Enable motion engine |
| `motion.disable` | `motion.disabled` | Disable motion engine |
| `motion.phase.setOffset` | `motion.phase.offset` | Set phase offset |
| `motion.phase.enableAutoRotate` | `motion.phase.autoRotate` | Enable auto-rotate |
| `motion.phase.getPhase` | `motion.phase.state` | Get current phase |
| `motion.speed.setModulation` | `motion.speed.modulation` | Set speed modulation |
| `motion.speed.setBaseSpeed` | `motion.speed.baseSpeed` | Set base speed |
| `motion.momentum.getStatus` | `motion.momentum.status` | Get momentum status |
| `motion.momentum.addParticle` | `motion.momentum.particle` | Add particle |
| `motion.momentum.applyForce` | `motion.momentum.force` | Apply force |
| `motion.momentum.getParticle` | `motion.momentum.particle` | Get particle |
| `motion.momentum.reset` | `motion.momentum.reset` | Reset momentum |
| `motion.momentum.update` | `motion.momentum.updated` | Update momentum |

### Color Engine

| Command | Response Type | Description |
|---------|---------------|-------------|
| `color.getStatus` | `color.status` | Get color engine status |
| `color.enableBlend` | `color.blend.enabled` | Enable palette blending |
| `color.setBlendPalettes` | `color.blend.palettes` | Set blend palettes |
| `color.setBlendFactors` | `color.blend.factors` | Set blend factors |
| `color.enableRotation` | `color.rotation.enabled` | Enable palette rotation |
| `color.setRotationSpeed` | `color.rotation.speed` | Set rotation speed |
| `color.enableDiffusion` | `color.diffusion.enabled` | Enable colour diffusion |
| `color.setDiffusionAmount` | `color.diffusion.amount` | Set diffusion amount |
| `colorCorrection.getConfig` | `colorCorrection.config` | Get colour correction config |
| `colorCorrection.setMode` | `colorCorrection.mode` | Set correction mode |
| `colorCorrection.setConfig` | `colorCorrection.config` | Set correction config |
| `colorCorrection.save` | `colorCorrection.saved` | Save correction config |

### Batch Operations

| Command | Response Type | Description |
|---------|---------------|-------------|
| `batch` | `batch.result` | Execute batch operations: `{"type":"batch","actions":[...]}` |

---

## Cross-Cutting Concerns

### Rate Limiting

- **HTTP**: 20 req/sec per IP (via `RateLimiter`)
- **WebSocket**: 50 msg/sec per client (via `RateLimiter`)
- **Response**: 429 Too Many Requests with `Retry-After` header

### Authentication (FEATURE_API_AUTH)

- **HTTP**: API key via `X-API-Key` header or query param `apiKey`
- **WebSocket**: First message must be `{"type":"auth","apiKey":"..."}` to authenticate session
- **Public endpoints**: `/api/v1/`, `/api/v1/health`, `/api/v1/openapi.json`

### Error Response Format

**HTTP (V1)**:
```json
{
  "success": false,
  "error": {
    "code": "ERROR_CODE",
    "message": "Human-readable message",
    "field": "fieldName"  // optional
  },
  "timestamp": 1234567890,
  "version": "2.0"
}
```

**WebSocket**:
```json
{
  "type": "error",
  "success": false,
  "error": {
    "code": "ERROR_CODE",
    "message": "Human-readable message"
  },
  "requestId": "..."  // optional, echoes client requestId
}
```

### Success Response Format

**HTTP (V1)**:
```json
{
  "success": true,
  "data": { ... },
  "timestamp": 1234567890,
  "version": "2.0"
}
```

**WebSocket**:
```json
{
  "type": "responseType",
  "success": true,
  "data": { ... },
  "requestId": "..."  // optional
}
```

---

## Performance Baseline Metrics

**Note**: These will be captured via host-native benchmarks before refactor.

### Target Metrics

1. **WebSocket Command Routing**: Dispatch time for 10k messages (p50, p95, p99)
2. **Status Broadcast Fanout**: Time to broadcast to M clients (M = 1, 4, 8)
3. **Route Registration**: Time to register all HTTP routes
4. **Memory Footprint**: RAM usage of WebServer class + modules

### Baseline Capture

- Run benchmarks on current monolithic implementation
- Store results in `v2/test/baseline/webserver_baseline.json`
- Compare post-refactor to ensure no regression

---

## Dependencies & Responsibilities

### Current WebServer Responsibilities

1. **Lifecycle**: WiFi init, AP fallback, mDNS, LittleFS mount
2. **Transport**: AsyncWebServer, AsyncWebSocket construction/management
3. **Routing**: All HTTP route registration (static, legacy, v1)
4. **WebSocket Gateway**: Connect/disconnect, auth, rate limiting, message parsing
5. **Command Dispatch**: Large if/else chain in `processWsCommand()`
6. **Business Logic**: Direct calls to ActorSystem, RendererActor, ZoneComposer, audio subsystems
7. **Broadcasting**: Status, zone state, LED frames, audio frames, benchmark metrics
8. **Streaming**: Subscription management for LED/audio/benchmark streams

### External Dependencies

- `ActorSystem` - State changes
- `RendererActor` - Effect/parameter queries
- `ZoneComposer` - Zone operations
- `AudioCapture` / `AudioTuning` - Audio operations
- `PatternRegistry` - Effect metadata
- `NarrativeEngine` - Narrative operations
- `ZoneConfigManager` - Persistence
- `WiFiManager` - Network operations
- `RateLimiter` - Rate limiting
- `LedStreamBroadcaster`, `AudioStreamBroadcaster`, `BenchmarkStreamBroadcaster` - Streaming

---

## Migration Notes

- All routes and WS commands must remain **100% backward compatible**
- Response formats must not change
- Error codes must remain stable
- Authentication flow must remain identical
- Rate limiting behaviour must remain identical

