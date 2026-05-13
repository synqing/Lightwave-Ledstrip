---
abstract: "SSA-N07 read-only naming extraction of the network top-level + routing infrastructure on feature/synqmatrix-rename-2026-05-13. Covers WebServer.{h,cpp}, WebServerBroadcast.cpp, webserver/WebServerContext.h, webserver/V1ApiRoutes.{h,cpp}, webserver/StaticAssetRoutes.{h,cpp}, webserver/WsGateway.{h,cpp}, and webserver/WsCommandRouter.{h,cpp}. Lists every public name, every REST route (174) and every WS command-group registrar call (33), with anomalies for Captain's SynqMatrix rename review."
type: reference
---

# SSA-N07 — Network Top-Level + Routing Infrastructure (Naming Surface)

**Branch:** `feature/synqmatrix-rename-2026-05-13`
**Scope:** `firmware-v3/src/network/WebServer.{h,cpp}`, `firmware-v3/src/network/WebServerBroadcast.cpp`, plus the routing infrastructure under `firmware-v3/src/network/webserver/` excluding `handlers/` and `ws/` (those are SSA-N08).
**Method:** clangd not required for header-level naming surface; direct file read with cross-grep for REST and WS registrations. Read-only — no edits.

## Files inspected (10)

1. `firmware-v3/src/network/WebServer.h` — 837 lines
2. `firmware-v3/src/network/WebServer.cpp` — top-level (constructor through `setupWebSocket`); broadcast TU is split off
3. `firmware-v3/src/network/WebServerBroadcast.cpp` — broadcast methods + subscription accessors + rate/auth helpers
4. `firmware-v3/src/network/webserver/WebServerContext.h` — 191 lines (note: lives under `webserver/`, not `network/` root — task brief had wrong path)
5. `firmware-v3/src/network/webserver/V1ApiRoutes.h` — 56 lines
6. `firmware-v3/src/network/webserver/V1ApiRoutes.cpp` — 2119 lines, 174 route registrations
7. `firmware-v3/src/network/webserver/StaticAssetRoutes.h` — 34 lines
8. `firmware-v3/src/network/webserver/StaticAssetRoutes.cpp` — 165 lines, 3 routes
9. `firmware-v3/src/network/webserver/WsGateway.{h,cpp}` — 211 + 957 lines
10. `firmware-v3/src/network/webserver/WsCommandRouter.{h,cpp}` — 94 + 131 lines

---

## 1. Namespaces (top-level)

| Namespace | Members in scope |
|---|---|
| `lightwaveos::network` | `WebServer` (class), `webServerInstance` (extern pointer), `WebServerConfig` (constexpr block), `LedStreamConfig` and `RateLimitConfig` re-exports, validation ring/encoder externs |
| `lightwaveos::network::webserver` | `WebServerContext` (struct), `V1ApiRoutes`, `StaticAssetRoutes`, `WsGateway`, `WsCommandRouter`, `WsCommandEntry`, `WsCommandHandler` (type alias) |
| `lightwaveos::nodes` (alias namespace) | `using NodeOrchestrator = actors::ActorSystem;` and `using RendererNode = actors::RendererActor;` — legacy compat aliases declared in `WebServer.h` lines 82-86 |

---

## 2. Configuration constants (file-local, header-visible)

`WebServer.h` lines 120-179, namespace `lightwaveos::network::WebServerConfig`:

- `HTTP_PORT` (alias of `config::NetworkConfig::WEB_SERVER_PORT`)
- `MDNS_HOSTNAME` (alias of `config::NetworkConfig::MDNS_HOSTNAME`)
- `AP_SSID_PREFIX` (`"LightwaveOS-"`)
- `AP_PASSWORD` (alias of `config::NetworkConfig::AP_PASSWORD`)
- `WIFI_CONNECT_TIMEOUT_MS`
- `STATUS_BROADCAST_INTERVAL_MS` (5000)
- `MAX_WS_CLIENTS` (8 PSRAM / 2 non-PSRAM)
- `MAX_BATCH_OPERATIONS` (10)

Heap-shed preprocessor knobs (overridable per PIO env):
- `LW_INTERNAL_HEAP_SHED_BELOW_BYTES` (12 KiB)
- `LW_INTERNAL_HEAP_RESUME_ABOVE_BYTES` (28 KiB)
- `LW_INTERNAL_HEAP_SHED_LOG_INTERVAL_MS` (15000)
- `LW_INTERNAL_HEAP_SHED_PROBE_INTERVAL_MS` (300)
- `LW_INTERNAL_HEAP_LARGEST_BLOCK_NEAR_THRESHOLD_MARGIN` (8 KiB)
- `LW_INTERNAL_HEAP_LARGEST_BLOCK_RECOVERY_BYTES` (4 KiB)

Re-exported namespaces for legacy compatibility: `lightwaveos::network::LedStreamConfig` (mirrors `webserver::LedStreamConfig`); `lightwaveos::network::RateLimitConfig` (mirrors `webserver::RateLimitConfig`).

---

## 3. `class lightwaveos::network::WebServer`

`WebServer.h:221`. The single largest networking class.

### 3.1 Lifecycle / control surface

| Member | Signature |
|---|---|
| `WebServer` (ctor) | `WebServer(lightwaveos::nodes::NodeOrchestrator& orchestrator, lightwaveos::nodes::RendererNode* renderer)` |
| `~WebServer` | `~WebServer()` |
| `begin` | `bool begin()` |
| `stop` | `void stop()` |
| `update` | `void update()` |
| `isRunning` | `bool isRunning() const` |
| `isConnected` | `bool isConnected() const` |
| `isAPMode` | `bool isAPMode() const` |
| `getClientCount` | `size_t getClientCount() const` |
| `getWebSocket` | `AsyncWebSocket* getWebSocket() const` |
| `getWsGateway` | `webserver::WsGateway* getWsGateway() const` |
| `isLittleFSMounted` | `bool isLittleFSMounted() const` |
| `mountLittleFS` | `bool mountLittleFS()` |
| `unmountLittleFS` | `bool unmountLittleFS()` |
| `isClientAuthenticated` | `bool isClientAuthenticated(uint32_t clientId) const` (FEATURE_API_AUTH) |
| `getApiKeyManager` | `ApiKeyManager& getApiKeyManager()` (FEATURE_API_AUTH) |

### 3.2 Cached renderer state

Nested struct `WebServer::CachedRendererState` (lines 325-382). Audio sub-struct `audioTuning` is FEATURE_AUDIO_SYNC-gated.

- `getCachedRendererState() const -> const CachedRendererState&`
- `findEffectName(EffectId id) const -> const char*` (member of `CachedRendererState`)
- Constant: `MAX_CACHED_EFFECTS = limits::MAX_EFFECTS`

### 3.3 Broadcasting and subscriptions

| Member | Signature |
|---|---|
| `shouldDeferTextAll` | `bool shouldDeferTextAll() const` |
| `broadcastStatus` | `void broadcastStatus()` |
| `doBroadcastStatus` | `void doBroadcastStatus()` |
| `broadcastZoneState` | `void broadcastZoneState()` |
| `broadcastSingleZoneState` | `void broadcastSingleZoneState(uint8_t zoneId)` |
| `broadcastLEDFrame` | `void broadcastLEDFrame()` |
| `setLEDStreamSubscription` | `bool setLEDStreamSubscription(AsyncWebSocketClient*, bool)` |
| `hasLEDStreamSubscribers` | `bool hasLEDStreamSubscribers() const` |
| `setLogStreamSubscription` | `bool setLogStreamSubscription(AsyncWebSocketClient*, bool)` |
| `hasLogStreamSubscribers` | `bool hasLogStreamSubscribers() const` |
| `setStatusSubscription` | `bool setStatusSubscription(AsyncWebSocketClient*, bool)` |
| `hasStatusSubscribers` | `bool hasStatusSubscribers() const` |
| `getStatusSubscriberCount` | `size_t getStatusSubscriberCount() const` |
| `broadcastAudioFrame` | `void broadcastAudioFrame()` (FEATURE_AUDIO_SYNC) |
| `broadcastBeatEvent` | `void broadcastBeatEvent()` (FEATURE_AUDIO_SYNC) |
| `setAudioStreamSubscription` | `bool setAudioStreamSubscription(AsyncWebSocketClient*, bool)` (FEATURE_AUDIO_SYNC) |
| `hasAudioStreamSubscribers` | `bool hasAudioStreamSubscribers() const` (FEATURE_AUDIO_SYNC) |
| `broadcastFftFrame` | `void broadcastFftFrame()` (FEATURE_AUDIO_SYNC) |
| `broadcastStmFrame` | `void broadcastStmFrame()` (FEATURE_AUDIO_SYNC) |
| `setStmStreamSubscription` | `bool setStmStreamSubscription(AsyncWebSocketClient*, bool)` (FEATURE_AUDIO_SYNC) |
| `hasStmStreamSubscribers` | `bool hasStmStreamSubscribers() const` (FEATURE_AUDIO_SYNC) |
| `broadcastBenchmarkStats` | `void broadcastBenchmarkStats()` (FEATURE_AUDIO_BENCHMARK) |
| `setBenchmarkStreamSubscription` | `bool setBenchmarkStreamSubscription(AsyncWebSocketClient*, bool)` (FEATURE_AUDIO_BENCHMARK) |
| `hasBenchmarkStreamSubscribers` | `bool hasBenchmarkStreamSubscribers() const` (FEATURE_AUDIO_BENCHMARK) |
| `notifyEffectChange` | `void notifyEffectChange(EffectId effectId, const char* name)` |
| `notifyParameterChange` | `void notifyParameterChange()` |

### 3.4 Private setup, dispatch and helpers

| Member | Signature |
|---|---|
| `setupCORS` | `void setupCORS()` |
| `setupRoutes` | `void setupRoutes()` |
| `setupWebSocket` | `void setupWebSocket()` |
| `startMDNS` | `void startMDNS()` |
| `onWsEvent` | `static void onWsEvent(AsyncWebSocket*, AsyncWebSocketClient*, AwsEventType, void*, uint8_t*, size_t)` |
| `handleWsConnect` | `void handleWsConnect(AsyncWebSocketClient*)` |
| `handleWsDisconnect` | `void handleWsDisconnect(AsyncWebSocketClient*)` |
| `handleWsMessage` | `void handleWsMessage(AsyncWebSocketClient*, uint8_t*, size_t)` |
| `executeBatchAction` | `bool executeBatchAction(const String& action, JsonVariant params)` |
| `checkRateLimit` | `bool checkRateLimit(AsyncWebServerRequest*)` |
| `checkWsRateLimit` | `bool checkWsRateLimit(AsyncWebSocketClient*)` |
| `checkAPIKey` | `bool checkAPIKey(AsyncWebServerRequest*)` |
| `updateCachedRendererState` | `void updateCachedRendererState()` |
| `updateLowHeapShedState` | `void updateLowHeapShedState(uint32_t nowMs)` |

### 3.5 Private state (named only)

`m_orchestrator`, `m_renderer`, `m_server`, `m_ws`, `m_rateLimiter`, `m_wsGateway`, `m_wsClientIpMap[16]` (struct `WsClientIpMapEntry { clientId, ipKey }`), `m_running`, `m_apMode`, `m_apClientDisconnected`, `m_lastApReinitMs`, `m_mdnsStarted`, `m_littleFSMounted`, `m_lastBroadcast`, `m_startTime`, `m_lastRegisteredIP`, `m_lastImmediateBroadcast`, `m_broadcastPending`, `m_lastClientConnectMs`, `m_lowHeapShed`, `m_lastHeapShedLogMs`, `m_lastHeapShedProbeMs`, `m_lastLargestInternalHeap`, `m_shedActivatedAtMs`, `m_shedClearedAtMs`, `m_ledBroadcaster`, `m_ledFrameScratch`, `m_statusSubscribers` (`SubscriptionManager<8>`), `m_statusSubscribersMux`, `m_udpStreamer`, `m_logBroadcaster`, `m_audioBroadcaster` (FEATURE_AUDIO_SYNC), `m_stmBroadcaster` (FEATURE_AUDIO_SYNC), `m_audioFrameScratch`, `m_audioGridScratch`, `m_benchmarkBroadcaster` (FEATURE_AUDIO_BENCHMARK), `m_authenticatedClients` (FEATURE_API_AUTH), `m_apiKeyManager` (FEATURE_API_AUTH), `m_authRateLimiter` (FEATURE_API_AUTH), `m_zoneComposer`, `m_cachedRendererState`, `m_lastStateCacheUpdate`.

Class-internal constants: `WS_CLIENT_IP_MAP_SLOTS=16`, `BROADCAST_COALESCE_MS=50`, `CONNECT_STABILISE_MS=600`, `MAX_STATUS_SUBSCRIBERS=8`, `STATE_CACHE_TTL_MS=100`, `INTERNAL_HEAP_SHED_MAX_LATCH_MS=10000`, `INTERNAL_HEAP_SHED_POST_CLEAR_GRACE_MS=500`.

### 3.6 Globals (file scope, `lightwaveos::network`)

- `extern WebServer* webServerInstance;` (declaration; assigned in main setup)
- `extern lightwaveos::validation::EffectValidationRing<128> g_validationRing;` (FEATURE_EFFECT_VALIDATION)
- `extern lightwaveos::validation::ValidationFrameEncoder g_validationEncoder;` (FEATURE_EFFECT_VALIDATION)

---

## 4. `struct lightwaveos::network::webserver::WebServerContext`

`webserver/WebServerContext.h:60`. Non-owning aggregator passed by const ref into every routing registrar.

| Field | Type | Notes |
|---|---|---|
| `actorSystem` | `actors::ActorSystem&` | Canonical name |
| `orchestrator` | `actors::ActorSystem&` | Alias of `actorSystem` for V1ApiRoutes compat |
| `renderer` | `actors::RendererActor*` | |
| `zoneComposer` | `zones::ZoneComposer*` | |
| `webServer` | `network::WebServer*` | `this` from `WebServer` |
| `pluginManager` | `plugins::PluginManagerActor*` | nullable |
| `rateLimiter` | `RateLimiter&` | |
| `ledBroadcaster` | `LedStreamBroadcaster*` | |
| `logBroadcaster` | `LogStreamBroadcaster*` | |
| `audioBroadcaster` | `AudioStreamBroadcaster*` | FEATURE_AUDIO_SYNC |
| `stmBroadcaster` | `StmStreamBroadcaster*` | FEATURE_AUDIO_SYNC |
| `benchmarkBroadcaster` | `BenchmarkStreamBroadcaster*` | FEATURE_AUDIO_BENCHMARK |
| `udpStreamer` | `UdpStreamer*` | |
| `startTime`, `apMode` | `uint32_t`, `bool` | |
| `broadcastStatus`, `broadcastZoneState` | `std::function<void()>` | |
| `ws` | `AsyncWebSocket*` | |
| `setLEDStreamSubscription`, `setLogStreamSubscription` | `std::function<bool(AsyncWebSocketClient*, bool)>` | |
| `setAudioStreamSubscription`, `setStmStreamSubscription` | `std::function<bool(AsyncWebSocketClient*, bool)>` | FEATURE_AUDIO_SYNC |
| `setValidationStreamSubscription` | `std::function<bool(AsyncWebSocketClient*, bool)>` | FEATURE_EFFECT_VALIDATION |
| `setBenchmarkStreamSubscription` | `std::function<bool(AsyncWebSocketClient*, bool)>` | FEATURE_AUDIO_BENCHMARK |
| `executeBatchAction` | `std::function<bool(const String&, JsonVariant)>` | |

Single multi-arg constructor with feature-gated parameters.

---

## 5. `class lightwaveos::network::webserver::V1ApiRoutes`

`webserver/V1ApiRoutes.h:29`. One static member:

```
static void registerRoutes(
    HttpRouteRegistry& registry,
    const WebServerContext& ctx,
    WebServer* server,
    std::function<bool(AsyncWebServerRequest*)> checkRateLimit,
    std::function<bool(AsyncWebServerRequest*)> checkAPIKey,
    std::function<void()> broadcastStatus,
    std::function<void()> broadcastZoneState);
```

### 5.1 Complete REST route inventory (174 registrations)

Wire surface — every URL path is naming-relevant to the SynqMatrix rename. Grouped by domain. `*` regex routes carry capture groups `(\d+)` or `([1-3])` for zone/preset IDs.

**Discovery / health / device / filesystem / sync** (10): `GET /api/v1/ping`, `GET /api/v1/`, `GET /api/v1/health`, `GET /api/v1/device/status`, `GET /api/v1/device/info`, `GET /api/v1/filesystem/status`, `POST /api/v1/filesystem/mount`, `POST /api/v1/filesystem/unmount`, `POST /api/v1/filesystem/restart`, `GET /api/v1/sync/status`.

**Effects** (10): `GET /api/v1/effects/metadata`, `GET /api/v1/effects/parameters`, `POST /api/v1/effects/parameters`, `GET /api/v1/effects/families`, `GET /api/v1/effects`, `GET /api/v1/effects/current`, `POST /api/v1/effects/set`, `PUT /api/v1/effects/current`, `GET /api/v1/parameters`, `POST /api/v1/parameters`.

**SongAware (rename target)** (5): `GET /api/v1/songAware/config`, `POST /api/v1/songAware/config`, `GET /api/v1/songAware/status`, `GET /api/v1/songAware/allowlist`, `POST /api/v1/songAware/allowlist/reset`. **All five contain the legacy `songAware` path segment that the SynqMatrix rename targets.**

**Factory presets** (2): `GET /api/v1/factoryPresets`, `POST /api/v1/factoryPresets/load`.

**Audio** (28): parameters/control/agc/state/tempo/fft/stm/presets (5 subroutes incl. delete), mappings (10 subroutes), zone-agc, spike-detection (+ reset), mic-gain, calibrate (4 subroutes), benchmark (4 subroutes).

**Stimulus** (4): `POST /api/v1/stimulus/mode`, `POST /api/v1/stimulus/patch`, `POST /api/v1/stimulus/clear`, `GET /api/v1/stimulus/status`.

**Debug** (4): `GET /api/v1/vrms`, `GET /api/v1/debug/audio`, `POST /api/v1/debug/audio`, `GET /api/v1/debug/memory/zones`, `GET /api/v1/debug/udp`.

**Transitions** (4): `GET /api/v1/transitions/types`, `POST /api/v1/transitions/trigger`, `GET /api/v1/transitions/config`, `POST /api/v1/transitions/config`.

**Batch** (1): `POST /api/v1/batch`.

**Palettes** (3): `GET /api/v1/palettes`, `GET /api/v1/palettes/current`, `POST /api/v1/palettes/set`.

**Narrative** (3): `GET /api/v1/narrative/status`, `GET /api/v1/narrative/config`, `POST /api/v1/narrative/config`.

**Shows** (5): `GET /api/v1/shows/current`, `GET /api/v1/shows`, `POST /api/v1/shows`, `PUT /api/v1/shows`, `DELETE /api/v1/shows`, `POST /api/v1/shows/control`.

**OpenAPI** (1): `GET /api/v1/openapi.json`.

**Zones** (28): list `GET /api/v1/zones`; layout `POST /api/v1/zones/layout`; per-zone (regex `[1-3]`) — get state, plus `POST .../effect`, `.../brightness`, `.../speed`, `.../palette`, `.../blend`, `.../enabled` (each x3 zones via regex), `.../audio` (GET+POST), `.../beat-trigger` (GET+POST); collection ops `POST /api/v1/zones/enabled`, `GET /api/v1/zones/config`, `POST /api/v1/zones/config/save`, `POST /api/v1/zones/config/load`, `GET /api/v1/zones/timing`, `POST /api/v1/zones/timing/reset`, `POST /api/v1/zones/reorder`.

**Presets — generic by name** (8): `GET /api/v1/presets`, `GET /api/v1/presets/{name}`, `POST /api/v1/presets`, `PUT /api/v1/presets/{name}`, `DELETE /api/v1/presets/{name}`, `POST /api/v1/presets/{name}/rename`, `POST /api/v1/presets/{name}/load`, `POST /api/v1/presets/save-current`.

**Effect presets (legacy `effect-presets` shape)** (5): `GET /api/v1/effect-presets`, `POST /api/v1/effect-presets`, `GET /api/v1/effect-presets/get`, `POST /api/v1/effect-presets/apply`, `DELETE /api/v1/effect-presets/delete`.

**Zone presets (legacy `zone-presets` shape)** (5): `GET /api/v1/zone-presets`, `POST /api/v1/zone-presets`, `GET /api/v1/zone-presets/get`, `POST /api/v1/zone-presets/apply`, `DELETE /api/v1/zone-presets/delete`.

**Presets — by numeric slot** (10): `GET /api/v1/presets/effects`, `GET /api/v1/presets/effects/{n}`, `POST /api/v1/presets/effects/save-current`, `POST /api/v1/presets/effects/{n}/load`, `DELETE /api/v1/presets/effects/{n}`, `GET /api/v1/presets/zones`, `GET /api/v1/presets/zones/{n}`, `POST /api/v1/presets/zones/save-current`, `POST /api/v1/presets/zones/{n}/load`, `DELETE /api/v1/presets/zones/{n}`.

**Firmware / OTA** (5): `GET /api/v1/firmware/version`, `POST /api/v1/firmware/update`, `POST /api/v1/firmware/filesystem`, `POST /update` (legacy), `GET /api/v1/device/ota-token`, `POST /api/v1/device/ota-token`.

**Network** (12): `GET /api/v1/network/status`, `POST /api/v1/network/sta/enable`, `POST /api/v1/network/ap/enable`, `GET /api/v1/network/networks`, `POST /api/v1/network/networks`, `DELETE /api/v1/network/networks/{ssid}`, `POST /api/v1/network/connect`, `POST /api/v1/network/disconnect`, `GET /api/v1/network/scan`, `GET /api/v1/network/scan/status`, `GET /api/v1/network/scan/status/{id}`.

**Modifiers** (5): `GET /api/v1/modifiers/list`, `POST /api/v1/modifiers/add`, `POST /api/v1/modifiers/remove`, `POST /api/v1/modifiers/clear`, `POST /api/v1/modifiers/update`.

**Colour correction** (6): `GET /api/v1/colorCorrection/config`, `POST /api/v1/colorCorrection/mode`, `POST /api/v1/colorCorrection/config`, `POST /api/v1/colorCorrection/save`, `GET /api/v1/colorCorrection/presets`, `POST /api/v1/colorCorrection/preset`.

**EdgeMixer** (2): `GET /api/v1/edgeMixer`, `POST /api/v1/edgeMixer`.

**Render** (2): `GET /api/v1/render/dithering`, `POST /api/v1/render/dithering`.

**Auth** (3): `GET /api/v1/auth/status`, `POST /api/v1/auth/rotate`, `DELETE /api/v1/auth/key`.

Total: 174 (verified by grep count). All routes go through `HttpRouteRegistry::onGet/onPost/onPut/onDelete` plus regex variants `onGetRegex/onPostRegex/onPutRegex/onDeleteRegex`.

---

## 6. `class lightwaveos::network::webserver::StaticAssetRoutes`

`webserver/StaticAssetRoutes.h:21`. One static member:

```
static void registerRoutes(HttpRouteRegistry& registry);
```

### 6.1 Routes registered (3)

| Method | Path | Behaviour |
|---|---|---|
| `GET` | `/` | Renders embedded `LAUNCHER_HTML` (PROGMEM) into a PSRAM-preferred 3 KB buffer (`getLauncherBuffer`). Substitutes WiFi SSID, `FIRMWARE_VERSION_STRING`, `config::NetworkConfig::MDNS_HOSTNAME`. |
| `GET` | `/favicon.ico` | `HttpStatus::NO_CONTENT` |
| (404) | `registry.onNotFound` | CORS-OPTIONS passthrough; structured JSON error for `/api*` and `/ws*`; plain-text 404 otherwise |

File-local constants: `kLauncherBufferSize = 3072`. Internal helper `getLauncherBuffer()` (static-local PSRAM-first allocator).

---

## 7. `class lightwaveos::network::webserver::WsGateway`

`webserver/WsGateway.h:26`.

### 7.1 Public API

| Member | Signature |
|---|---|
| `WsGateway` (ctor) | `WsGateway(AsyncWebSocket*, const WebServerContext&, std::function<bool(AsyncWebSocketClient*)> checkRateLimit, std::function<bool(AsyncWebSocketClient*, JsonDocument&)> checkAuth, std::function<void(AsyncWebSocketClient*)> onConnect, std::function<void(AsyncWebSocketClient*)> onDisconnect, std::function<void(AsyncWebSocketClient*, JsonDocument&)> fallbackHandler = nullptr)` |
| `onEvent` | `static void onEvent(AsyncWebSocket*, AsyncWebSocketClient*, AwsEventType, void*, uint8_t*, size_t)` |
| `handleConnect` | `void handleConnect(AsyncWebSocketClient*)` |
| `handleDisconnect` | `void handleDisconnect(AsyncWebSocketClient*)` |
| `handleMessage` | `void handleMessage(AsyncWebSocketClient*, uint8_t*, size_t)` |
| `cleanupStaleConnections` | `void cleanupStaleConnections()` |
| `closeClientsInSubnet` | `void closeClientsInSubnet(uint8_t a, uint8_t b, uint8_t c, uint16_t code, const char* reason)` |
| `getStats` | `Stats getStats() const` |

Nested `struct WsGateway::Stats`: `connectAccepted`, `connectRejectedCooldown`, `connectRejectedOverlap`, `connectRejectedLimit`, `disconnects`, `parseErrors`, `oversizedFrames`, `unknownCommands`, `dispatchCount`.

### 7.2 Private state and constants

Structs `ConnectGuardEntry { ipKey, lastMs, lastActivityMs, active, _pad[3] }`, `ClientIpMapEntry { clientId, ipKey }`, `ClientEpochEntry { clientId, connEpoch, connectTs }`.

Fields: `m_ws`, `m_ctx`, `m_checkRateLimit`, `m_checkAuth`, `m_onConnect`, `m_onDisconnect`, `m_fallbackHandler`, `m_connectGuard[8]`, `m_clientIpMap[16]`, `m_clientEpochs[16]`, `m_stats`. Statics: `s_eventSeq`, `s_instance`.

Helpers: `getOrIncrementEpoch(uint32_t clientId) -> uint32_t`, `validateOrigin(AsyncWebServerRequest*) -> bool`.

Constants: `CONNECT_GUARD_SLOTS=8`, `CONNECT_COOLDOWN_MS=2000`, `IDLE_TIMEOUT_MS=15000`, `STALE_ACTIVE_RECOVERY_MS=5000`, `CLIENT_IP_MAP_SLOTS=16`, `MAX_WS_MESSAGE_SIZE=65536`.

File-local helpers (`webserver/WsGateway.cpp` anon namespace): `s_telemetryBuf[320]`, `escapeJsonString(const char*, char*, size_t)`.

---

## 8. `class lightwaveos::network::webserver::WsCommandRouter`

`webserver/WsCommandRouter.h:44`. Table-driven dispatcher.

### 8.1 Public API

| Member | Signature |
|---|---|
| `registerCommand` | `static void registerCommand(const char* type, WsCommandHandler handler)` |
| `route` | `static bool route(AsyncWebSocketClient*, JsonDocument&, const WebServerContext&)` |
| `getHandlerCount` | `static size_t getHandlerCount()` |
| `getMaxHandlers` | `static size_t getMaxHandlers()` |
| `reset` | `static void reset()` (NATIVE_BUILD only) |

Type alias: `using WsCommandHandler = void (*)(AsyncWebSocketClient*, JsonDocument&, const WebServerContext&)`.
Struct `WsCommandEntry { const char* type; uint16_t typeLen; char firstChar; WsCommandHandler handler; }`.
Capacity constant: `MAX_HANDLERS = 192`. Storage `s_handlers` is allocated lazily into PSRAM (or internal heap on non-PSRAM boards) by `ensureStorage()` — see `WsCommandRouter.cpp:24-47`. Inbound envelope key is `"type"` with legacy fallback `"cmd"` and is stripped from the JSON doc before handler dispatch.

### 8.2 WS command-group registrar calls (33 from `WebServer::setupWebSocket`)

These are the seams via which the wire-name surface is populated. Each call is a free function in `lightwaveos::network::webserver::ws::` (declared in `webserver/ws/Ws*Commands.h` — out of scope for SSA-N07 per the brief; covered by SSA-N08).

Order of registration in `WebServer.cpp:1311-1348`:

1. `registerWsDeviceCommands`
2. `registerWsFilesystemCommands`
3. `registerWsEffectsCommands`
4. `registerWsZonesCommands`
5. `registerWsTransitionCommands`
6. `registerWsNarrativeCommands`
7. `registerWsMotionCommands`
8. `registerWsColorCommands`
9. `registerWsEdgeMixerCommands`
10. `registerWsRenderCommands`
11. **`registerWsSynqMatrixCommands`** — already renamed on this branch (was `registerWsSongAwareCommands` on `main`)
12. `registerWsPaletteCommands`
13. `registerWsPresetCommands`
14. `registerWsZonePresetCommands`
15. `registerWsEffectPresetCommands`
16. `registerWsBatchCommands`
17. `registerWsAudioCommands` (FEATURE_AUDIO_SYNC)
18. `registerWsStimulusCommands`
19. `registerWsDebugCommands`
20. `registerWsStreamCommands`
21. `registerWsStatusCommands`
22. `registerWsStmCommands` (FEATURE_AUDIO_SYNC)
23. `registerWsModifierCommands`
24. `registerWsAuthCommands` (FEATURE_API_AUTH)
25. `registerWsSysCommands`
26. `registerWsTrinityCommands`
27. `registerWsShowCommands`
28. `registerWsOtaCommands`
29. `registerWsPluginCommands`

(29 distinct registrar functions; FEATURE-gated count includes audio + auth, but the headline registrar list is 29.)

`WsCommandRouter::getHandlerCount()` is read by `WebServer::setupWebSocket()` and logged. Live count on main per claude-mem #48645 was 141 commands across 24 modules; current branch adds 5 modules and one rename.

---

## 9. Free functions / file-local helpers in WebServer.cpp + WebServerBroadcast.cpp

`WebServer.cpp` (anon namespace, lines 138-188):
- `uint32_t packIpKey(const IPAddress&)`
- `size_t getFreeInternalHeap()`
- `size_t getLargestInternalHeapBlock()`
- `static lightwaveos::validation::ValidationFrameEncoder* s_validationEncoder`
- `static AsyncWebSocketClient* s_validationSubscribers[4]`
- `static constexpr size_t MAX_VALIDATION_SUBSCRIBERS = 4`
- `static void initValidationEncoder()`

`WebServerBroadcast.cpp` (anon namespace + file-local):
- `size_t getFreeInternalHeap()` (duplicate of WebServer.cpp anon; not deduplicated)
- `static const char* formatKeyName(uint8_t rootNote, audio::ChordType type)`

No additional public names; both TUs share the same `WebServer` class and write into its private members.

---

## 10. Anomalies relevant to SynqMatrix rename

1. **`songAware` REST paths not renamed.** Five `/api/v1/songAware/*` routes remain in `V1ApiRoutes.cpp` (lines 263, 269, 291, 297, 314). The matching WS module registrar (`registerWsSynqMatrixCommands`) has been renamed, creating asymmetric naming: WS layer is `synqMatrix`, REST layer is still `songAware`. Either Captain holds the REST surface for compatibility or it will need a coordinated update in `docs/protocol/k1-rest-contract.yaml` plus iOS/Tab5 clients.

2. **`nodes::NodeOrchestrator` / `nodes::RendererNode` legacy aliases.** Declared in `WebServer.h` lines 82-86 as `using` aliases for `actors::ActorSystem` and `actors::RendererActor`. The `WebServer` constructor signature still references these names. Documented as deliberate compatibility in the header comments. Not a rename target, but a naming artefact worth flagging.

3. **`WebServerContext.orchestrator` field is an alias for `actorSystem`.** Both fields bind to the same reference in the ctor. Comment at `WebServerContext.h:62-65` calls this out for `V1ApiRoutes.cpp` compatibility. Means any rename of `orchestrator` would need to keep the alias or sweep V1ApiRoutes too.

4. **`AP_SSID_PREFIX = "LightwaveOS-"`** (`WebServer.h:166`). String prefix of the Soft-AP SSID; the dash and the product-name token are wire-visible to every iOS/Tab5 client during discovery. If Captain renames the consumer-visible product, this constant is part of the surface to align with brand work.

5. **Launcher HTML carries `LightwaveOS` name and `localhost:8888` CTA URL** (`StaticAssetRoutes.cpp:60-105`). Product name appears three times (`<title>`, `<h1>`, body copy `LightwaveOS Controller`). mDNS host `%s.local` is rendered from `config::NetworkConfig::MDNS_HOSTNAME`. Any product-name change needs to touch this HTML.

6. **mDNS service text records hard-code `version=2.0.0` and `board=ESP32-S3`** (`WebServer.cpp:1133-1138`). Wire-visible to any client doing service discovery on `_ws._tcp` / `_http._tcp`. Not directly a SynqMatrix rename concern but worth flagging because both labels are stable identifiers consumed by Tab5/iOS.

7. **`registerWsSynqMatrixCommands` is the only registrar already renamed on this branch** — the other 28 registrars and 174 REST paths are still in their pre-rename shape relative to the d1d7b807 mechanical pass. This SSA's surface confirms the mechanical pass touched WS registrar names but did not touch REST paths or `WebServerContext`/`WebServer` field names. Captain decides whether the rename is meant to extend further (REST `/api/v1/songAware/*`, `audio/mappings`, brand strings) or stop at the WS registrar names.

8. **`WebServerContext.h` is under `webserver/`, not `network/` root.** The task brief listed it as `firmware-v3/src/network/WebServerContext.h`; the actual path is `firmware-v3/src/network/webserver/WebServerContext.h`. Verified via `find`. Worth correcting in the SSA-N0x index doc.

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-13 | agent:claude-opus-4-7 (1M) | Created — SSA-N07 read-only naming extraction for network top-level + routing infrastructure on feature/synqmatrix-rename-2026-05-13. 174 REST routes, 29 WS command-group registrars, 8 anomalies flagged. |
