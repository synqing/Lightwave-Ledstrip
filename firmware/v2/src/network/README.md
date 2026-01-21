# Network Subsystem

The network subsystem provides HTTP REST API, WebSocket real-time control, WiFi management, and streaming services for LightwaveOS v2.

**Version:** 2.0.0

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Directory Structure](#directory-structure)
3. [WebServer Architecture](#webserver-architecture)
4. [API v1 Endpoints Reference](#api-v1-endpoints-reference)
5. [WebSocket Protocol](#websocket-protocol)
6. [WiFiManager State Machine](#wifimanager-state-machine)
7. [Rate Limiting](#rate-limiting)
8. [Streaming Subsystem](#streaming-subsystem)
9. [Request Validation](#request-validation)
10. [Thread Safety](#thread-safety)
11. [Related Documentation](#related-documentation)

---

## Architecture Overview

```
+------------------------------------------------------------------+
|                        WebServer (Port 80)                        |
|  - AsyncWebServer for HTTP                                        |
|  - AsyncWebSocket for real-time events (/ws)                      |
|  - mDNS: lightwaveos.local                                        |
+------------------------------------------------------------------+
           |                    |                    |
           v                    v                    v
+------------------+  +------------------+  +------------------+
| V1ApiRoutes      |  | WsGateway        |  | StaticAssetRoutes|
| /api/v1/*        |  | WebSocket Events |  | SPIFFS Files     |
+------------------+  +------------------+  +------------------+
           |                    |
           v                    v
+------------------+  +------------------+
| Handler Classes  |  | WsCommandRouter  |
| EffectHandlers   |  | Table-driven     |
| ZoneHandlers     |  | dispatch         |
| etc.             |  +------------------+
+------------------+           |
           |                   v
           v           +------------------+
+------------------+   | WsXxxCommands    |
| RequestValidator |   | 15+ command      |
| ApiResponse      |   | handler modules  |
+------------------+   +------------------+
           |                   |
           +--------+----------+
                    v
+------------------------------------------------------------------+
|                     WebServerContext                              |
|  - ActorSystem reference (thread-safe state changes)              |
|  - RendererActor pointer (read-only state cache)                  |
|  - ZoneComposer pointer                                           |
|  - Streaming broadcasters                                         |
+------------------------------------------------------------------+
                    |
                    v
+------------------------------------------------------------------+
|                      WiFiManager                                  |
|  - FreeRTOS task on Core 0                                        |
|  - Event-driven state machine                                     |
|  - Automatic reconnection with exponential backoff                |
|  - Soft-AP fallback mode                                          |
+------------------------------------------------------------------+
```

**Key Design Principles:**

- **WebServer runs on Core 0** with the WiFi stack
- **RendererActor runs on Core 1** with LED rendering
- **Never directly access LED buffers** from network handlers
- **All state changes** go through ActorSystem message passing
- **CachedRendererState** provides thread-safe read-only access

---

## Directory Structure

```
src/network/
|-- WebServer.h              # Main WebServer class with Actor integration
|-- WebServer.cpp            # HTTP/WebSocket setup and event handling
|-- WiFiManager.h            # WiFi state machine (PROTECTED FILE)
|-- WiFiManager.cpp          # FreeRTOS task implementation
|-- ApiResponse.h            # Standardized JSON response helpers
|-- RequestValidator.h       # Schema-based request validation
|-- SubscriptionManager.h    # Generic subscription list manager
|
+-- webserver/               # Modular WebServer components
    |-- HttpRouteRegistry.h  # Route registration abstraction
    |-- WebServerContext.h   # Shared context for handlers
    |-- V1ApiRoutes.h/cpp    # API v1 route registration
    |-- StaticAssetRoutes.h  # SPIFFS file serving
    |
    |-- WsGateway.h/cpp      # WebSocket connection management
    |-- WsCommandRouter.h    # Table-driven command dispatch
    |
    |-- RateLimiter.h        # Per-IP token bucket rate limiter
    |
    |-- LedFrameEncoder.h    # Binary LED frame encoding
    |-- LedStreamBroadcaster.h/cpp
    |
    |-- AudioFrameEncoder.h  # Binary audio frame encoding
    |-- AudioStreamConfig.h
    |-- AudioStreamBroadcaster.h/cpp
    |
    |-- BenchmarkFrameEncoder.h
    |-- BenchmarkStreamConfig.h
    |-- BenchmarkStreamBroadcaster.h/cpp
    |
    +-- handlers/            # REST API handler classes
    |   |-- DeviceHandlers.h/cpp
    |   |-- EffectHandlers.h/cpp
    |   |-- ParameterHandlers.h/cpp
    |   |-- TransitionHandlers.h/cpp
    |   |-- ZoneHandlers.h/cpp
    |   |-- PaletteHandlers.h/cpp
    |   |-- BatchHandlers.h/cpp
    |   |-- AudioHandlers.h/cpp
    |   |-- NarrativeHandlers.h/cpp
    |   |-- SystemHandlers.h/cpp
    |   +-- DebugHandlers.h/cpp
    |
    +-- ws/                  # WebSocket command handlers
        |-- WsDeviceCommands.h/cpp
        |-- WsEffectsCommands.h/cpp
        |-- WsTransitionCommands.h/cpp
        |-- WsZonesCommands.h/cpp
        |-- WsPaletteCommands.h/cpp
        |-- WsColorCommands.h/cpp
        |-- WsMotionCommands.h/cpp
        |-- WsStreamCommands.h/cpp
        |-- WsBatchCommands.h/cpp
        |-- WsAudioCommands.h/cpp
        |-- WsNarrativeCommands.h/cpp
        +-- WsDebugCommands.h/cpp
```

---

## WebServer Architecture

### WebServer Class

The `WebServer` class (`WebServer.h/cpp`) is the main entry point:

```cpp
class WebServer {
public:
    WebServer(ActorSystem& actors, RendererActor* renderer);

    bool begin();           // Start server, WiFi, mDNS
    void stop();            // Stop server
    void update();          // Call from loop() for periodic tasks

    // Status
    bool isRunning() const;
    bool isConnected() const;
    bool isAPMode() const;
    size_t getClientCount() const;

    // Broadcasting
    void broadcastStatus();
    void broadcastZoneState();
    void broadcastLEDFrame();
    void notifyEffectChange(uint8_t effectId, const char* name);

    // Cached state (thread-safe read)
    const CachedRendererState& getCachedRendererState() const;
};
```

### CachedRendererState

To avoid cross-core access issues, the WebServer maintains a cached copy of renderer state:

```cpp
struct CachedRendererState {
    uint8_t effectCount;
    uint8_t currentEffect;
    uint8_t brightness;
    uint8_t speed;
    uint8_t paletteIndex;
    // ... additional fields
    const char* effectNames[98];  // Pointers to stable strings
};
```

Updated in `update()` every 100ms. All request handlers use this cached state.

### Handler Architecture

REST API handlers are organized into domain-specific classes:

```cpp
// Example: EffectHandlers
class EffectHandlers {
public:
    static void registerRoutes(HttpRouteRegistry& registry);

    static void handleList(AsyncWebServerRequest* request, RendererActor* renderer);
    static void handleCurrent(AsyncWebServerRequest* request, RendererActor* renderer);
    static void handleSet(AsyncWebServerRequest* request, uint8_t* data, size_t len,
                          ActorSystem& actors, const CachedRendererState& cachedState,
                          std::function<void()> broadcastStatus);
    static void handleMetadata(AsyncWebServerRequest* request, RendererActor* renderer);
};
```

### WebServerContext

Shared context passed to all handlers:

```cpp
struct WebServerContext {
    ActorSystem& actorSystem;
    RendererActor* renderer;
    ZoneComposer* zoneComposer;
    RateLimiter& rateLimiter;

    // Streaming broadcasters
    LedStreamBroadcaster* ledBroadcaster;
    AudioStreamBroadcaster* audioBroadcaster;     // FEATURE_AUDIO_SYNC
    BenchmarkStreamBroadcaster* benchmarkBroadcaster;  // FEATURE_AUDIO_BENCHMARK

    // Callbacks
    std::function<void()> broadcastStatus;
    std::function<void()> broadcastZoneState;
    AsyncWebSocket* ws;
};
```

---

## API v1 Endpoints Reference

**Base URL:** `http://lightwaveos.local/api/v1/`

All responses follow the standardized format:

```json
{
  "success": true,
  "data": { ... },
  "timestamp": 123456789,
  "version": "2.0"
}
```

### Device Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/` | API discovery with HATEOAS links |
| GET | `/api/v1/device/status` | Runtime status (uptime, heap, network) |
| GET | `/api/v1/device/info` | Hardware/firmware information |
| GET | `/api/v1/ping` | Health check endpoint |

### Effects Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/effects` | List all effects (paginated) |
| GET | `/api/v1/effects/current` | Current effect with parameters |
| GET | `/api/v1/effects/metadata?id=N` | Detailed effect metadata |
| POST | `/api/v1/effects/set` | Set active effect |

**POST /api/v1/effects/set:**
```json
{
  "effectId": 12
}
```

### Parameters Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/parameters` | Get all parameters |
| POST | `/api/v1/parameters` | Update parameters |

**POST /api/v1/parameters:**
```json
{
  "brightness": 200,
  "speed": 25,
  "paletteId": 5
}
```

### Transitions Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/transitions/types` | Available transition types |
| GET | `/api/v1/transitions/config` | Current transition config |
| POST | `/api/v1/transitions/config` | Update transition settings |
| POST | `/api/v1/transitions/trigger` | Trigger transition to effect |

### Zones Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/zones` | All zone configurations |
| GET | `/api/v1/zones/{id}` | Specific zone config |
| POST | `/api/v1/zones/layout` | Set zone layout |
| POST | `/api/v1/zones/{id}/effect` | Set zone effect |
| POST | `/api/v1/zones/{id}/brightness` | Set zone brightness |
| POST | `/api/v1/zones/{id}/speed` | Set zone speed |
| POST | `/api/v1/zones/{id}/palette` | Set zone palette |
| POST | `/api/v1/zones/{id}/blend` | Set zone blend mode |
| POST | `/api/v1/zones/{id}/enabled` | Enable/disable zone |
| POST | `/api/v1/zones/enabled` | Enable/disable zone system |

### Batch Operations

| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/api/v1/batch` | Execute up to 10 operations |

**POST /api/v1/batch:**
```json
{
  "operations": [
    {"action": "setBrightness", "value": 200},
    {"action": "setEffect", "effectId": 12},
    {"action": "setSpeed", "value": 25}
  ]
}
```

**Supported Actions:**
- `setBrightness` - value: 0-255
- `setSpeed` - value: 1-100
- `setEffect` - effectId: 0-N
- `setPalette` - paletteId: 0-N
- `setZoneEffect` - zoneId, effectId
- `setZoneBrightness` - zoneId, brightness
- `setZoneSpeed` - zoneId, speed

For complete API documentation, see: [docs/api/api-v1.md](../../../docs/api/api-v1.md)

---

## WebSocket Protocol

**Endpoint:** `ws://lightwaveos.local/ws`

### Connection Management

The `WsGateway` class handles:
- Connection/disconnection events
- Rate limiting per client
- Connection thrash protection (2-second cooldown per IP)
- JSON message parsing and routing

### Command Routing

`WsCommandRouter` uses table-driven dispatch:

```cpp
// Registration (in WsXxxCommands.cpp)
WsCommandRouter::registerCommand("effects.getCurrent", handleGetCurrent);
WsCommandRouter::registerCommand("effects.set", handleSet);

// Routing
bool handled = WsCommandRouter::route(client, doc, ctx);
```

**Optimization:** Uses string length + first character for fast filtering.

### Command Categories

| Category | Commands |
|----------|----------|
| Device | `device.getStatus`, `device.getInfo` |
| Effects | `effects.getCurrent`, `effects.set`, `effects.getMetadata`, `effects.getCategories` |
| Parameters | `parameters.get`, `parameters.set` |
| Transitions | `transition.getTypes`, `transition.trigger`, `transition.config` |
| Zones | `zones.list`, `zones.update`, `zones.setLayout`, `zone.setEffect`, `zone.setPalette`, `zone.setBlend` |
| Streaming | `stream.led.subscribe`, `stream.led.unsubscribe`, `stream.audio.subscribe` |
| Batch | `batch` |

### Message Format

**Request:**
```json
{
  "type": "effects.set",
  "effectId": 12,
  "requestId": "optional-for-correlation"
}
```

**Response:**
```json
{
  "type": "effects.current",
  "success": true,
  "data": {
    "effectId": 12,
    "name": "Dual Wave Collision"
  },
  "requestId": "optional-for-correlation"
}
```

**Error:**
```json
{
  "type": "error",
  "success": false,
  "error": {
    "code": "OUT_OF_RANGE",
    "message": "Effect ID exceeds maximum"
  }
}
```

---

## WiFiManager State Machine

**PROTECTED FILE:** WiFiManager contains critical FreeRTOS synchronization. See CLAUDE.md for modification requirements.

### State Diagram

```
                     +-------------+
                     | STATE_INIT  |
                     +------+------+
                            |
            +---------------+---------------+
            | (has credentials)             | (no credentials)
            v                               v
    +---------------+               +---------------+
    | STATE_SCANNING|               | STATE_AP_MODE |<-------+
    +-------+-------+               +---------------+        |
            |                                                |
            v                                                |
    +---------------+                                        |
    | STATE_CONNECTING|-------(timeout)-------->+------------+
    +-------+-------+                           |
            |                                   |
    (success)                           +-------v-------+
            |                           | STATE_FAILED  |
            v                           +---------------+
    +---------------+
    | STATE_CONNECTED|
    +-------+-------+
            |
    (disconnect event)
            |
            v
    +----------------+
    |STATE_DISCONNECTED|----(retry)---> STATE_CONNECTING
    +----------------+
```

### WiFi States

| State | Description |
|-------|-------------|
| `STATE_WIFI_INIT` | Initial state, checking credentials |
| `STATE_WIFI_SCANNING` | Async network scan in progress |
| `STATE_WIFI_CONNECTING` | Attempting connection |
| `STATE_WIFI_CONNECTED` | Successfully connected |
| `STATE_WIFI_FAILED` | Connection failed, will retry |
| `STATE_WIFI_AP_MODE` | Running in Soft-AP fallback mode |
| `STATE_WIFI_DISCONNECTED` | Disconnected, will reconnect |

### FreeRTOS EventGroup Bits

```cpp
static constexpr uint32_t EVENT_SCAN_COMPLETE     = BIT0;
static constexpr uint32_t EVENT_CONNECTED         = BIT1;
static constexpr uint32_t EVENT_DISCONNECTED      = BIT2;
static constexpr uint32_t EVENT_GOT_IP            = BIT3;
static constexpr uint32_t EVENT_CONNECTION_FAILED = BIT4;
static constexpr uint32_t EVENT_AP_START          = BIT5;
static constexpr uint32_t EVENT_AP_STACONNECTED   = BIT6;
```

### CRITICAL: EventGroup Cleanup

**LANDMINE WARNING:** EventGroup bits persist across interrupted connections. The `setState()` method MUST clear stale bits when entering `STATE_WIFI_CONNECTING`:

```cpp
if (newState == STATE_WIFI_CONNECTING) {
    if (m_wifiEventGroup) {
        xEventGroupClearBits(m_wifiEventGroup,
            EVENT_CONNECTED | EVENT_GOT_IP | EVENT_CONNECTION_FAILED);
    }
}
```

Failing to clear these bits causes the "Connected! IP: 0.0.0.0" bug.

### Task Configuration

```cpp
static constexpr size_t TASK_STACK_SIZE = 4096;
static constexpr UBaseType_t TASK_PRIORITY = 1;
static constexpr BaseType_t TASK_CORE = 0;  // Core 0 with WiFi stack
```

### Timing Configuration

| Parameter | Value | Description |
|-----------|-------|-------------|
| `SCAN_INTERVAL_MS` | 60000 | Re-scan every minute |
| `CONNECT_TIMEOUT_MS` | 10000 | 10s connection timeout |
| `RECONNECT_DELAY_MS` | 5000 | Initial retry delay |
| `MAX_RECONNECT_DELAY_MS` | 60000 | Max backoff (1 minute) |

### AP Mode Configuration

When WiFi connection fails, device enters AP mode:

| Setting | Value |
|---------|-------|
| SSID | `LightwaveOS-XXXX` (last 4 of MAC) |
| Password | `lightwave123` |
| IP | `192.168.4.1` |
| Channel | Auto-selected (least congested) |

---

## Rate Limiting

### Configuration

```cpp
namespace RateLimitConfig {
    constexpr uint8_t MAX_TRACKED_IPS = 8;
    constexpr uint16_t HTTP_LIMIT = 20;          // 20 requests/second
    constexpr uint16_t WS_LIMIT = 50;            // 50 messages/second
    constexpr uint32_t WINDOW_SIZE_MS = 1000;    // 1-second sliding window
    constexpr uint32_t BLOCK_DURATION_MS = 5000; // 5-second block on limit breach
    constexpr uint8_t RETRY_AFTER_SECONDS = 5;
}
```

### Implementation

The `RateLimiter` class uses a per-IP token bucket with sliding window:

```cpp
class RateLimiter {
public:
    bool checkHTTP(IPAddress ip);
    bool checkWebSocket(IPAddress ip);
    bool isBlocked(IPAddress ip) const;
    uint32_t getRetryAfterSeconds(IPAddress ip) const;
};
```

**Features:**
- LRU eviction when tracking table is full
- Injectable time source for testing
- Separate limits for HTTP and WebSocket
- Automatic unblocking after block duration

### Rate Limit Response

HTTP 429 response includes `Retry-After` header:

```json
{
  "success": false,
  "error": {
    "code": "RATE_LIMITED",
    "message": "Too many requests. Please wait before retrying.",
    "retryAfter": 5
  }
}
```

---

## Streaming Subsystem

### LED Frame Streaming

**Frame Format v1:**
```
[MAGIC:1][VERSION:1][NUM_STRIPS:1][LEDS_PER_STRIP:1]
[STRIP0_ID:1][RGB x 160]
[STRIP1_ID:1][RGB x 160]
= 966 bytes total
```

**Configuration:**
```cpp
namespace LedStreamConfig {
    constexpr uint16_t LEDS_PER_STRIP = 160;
    constexpr uint8_t NUM_STRIPS = 2;
    constexpr uint16_t TOTAL_LEDS = 320;
    constexpr uint8_t FRAME_VERSION = 1;
    constexpr uint8_t MAGIC_BYTE = 0xFE;
    constexpr uint8_t TARGET_FPS = 20;  // ~50ms interval
}
```

**Usage:**
```cpp
// Subscribe via WebSocket
ws.send(JSON.stringify({ type: "stream.led.subscribe" }));

// Receive binary frames
ws.onmessage = (event) => {
    if (event.data instanceof ArrayBuffer) {
        const frame = new Uint8Array(event.data);
        if (frame[0] === 0xFE) {
            // Parse LED frame
        }
    }
};
```

### Audio Frame Streaming

**Requires:** `FEATURE_AUDIO_SYNC`

**Frame Format:**
```
[MAGIC:1][VERSION:1][TIMESTAMP:4]
[BPM:4][PHASE:4][CONFIDENCE:4]
[BEAT_TICK:1][DOWNBEAT_TICK:1][BAR_POS:2]
[BASS:4][MID:4][HIGH:4]
... additional audio metrics
```

**Configuration:**
```cpp
namespace AudioStreamConfig {
    constexpr uint8_t TARGET_FPS = 30;      // Match audio hop rate
    constexpr uint8_t MAX_CLIENTS = 4;
    constexpr uint8_t MAGIC_BYTE = 0xA1;    // Audio frame magic
}
```

### Benchmark Streaming

**Requires:** `FEATURE_AUDIO_BENCHMARK`

Streams audio pipeline timing metrics at 10 Hz for performance analysis.

---

## Request Validation

### Schema-Based Validation

The `RequestValidator` class provides declarative validation:

```cpp
// Define schema
constexpr FieldSchema SetEffectSchema[] = {
    {"effectId", FieldType::UINT8, true, 0, 255}
};

// Validate request
JsonDocument doc;
auto result = RequestValidator::parseAndValidate(data, len, doc, SetEffectSchema);
if (!result.valid) {
    sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                      result.errorCode, result.errorMessage, result.fieldName);
    return;
}
```

### Field Types

| Type | Range | Description |
|------|-------|-------------|
| `UINT8` | 0-255 | Unsigned 8-bit integer |
| `UINT16` | 0-65535 | Unsigned 16-bit integer |
| `UINT32` | 0-4294967295 | Unsigned 32-bit integer |
| `INT32` | -2B to +2B | Signed 32-bit integer |
| `BOOL` | true/false | Boolean |
| `STRING` | - | String with min/max length |
| `ARRAY` | - | JSON array with min/max size |
| `OBJECT` | - | JSON object |

### Error Codes

| Code | HTTP Status | Description |
|------|-------------|-------------|
| `INVALID_JSON` | 400 | Malformed JSON body |
| `MISSING_FIELD` | 400 | Required field missing |
| `INVALID_TYPE` | 400 | Wrong data type |
| `OUT_OF_RANGE` | 400 | Value exceeds bounds |
| `RATE_LIMITED` | 429 | Too many requests |
| `INTERNAL_ERROR` | 500 | Server-side error |

### Defensive Validation Helpers

```cpp
// Prevent LoadProhibited crashes from corrupted IDs
uint8_t safeEffectId = validateEffectIdInRequest(effectId);
uint8_t safePaletteId = validatePaletteIdInRequest(paletteId);
uint8_t safeZoneId = validateZoneIdInRequest(zoneId);
```

---

## Thread Safety

### Core Assignment

| Component | Core | Reason |
|-----------|------|--------|
| WiFiManager | Core 0 | WiFi stack runs on Core 0 |
| WebServer | Core 0 | Network handlers |
| RendererActor | Core 1 | LED rendering |
| Main loop | Core 1 | Effect updates |

### Cross-Core Communication

```
[Core 0: WebServer]                     [Core 1: Renderer]
        |                                       |
        |  1. API request received              |
        |                                       |
        v                                       |
  Parse & validate                              |
        |                                       |
        v                                       |
  Send message to ActorSystem  -------->  Receive message
        |                                       |
        |                                       v
        |                                 Apply state change
        |                                       |
        |                                       v
        |                                 Render LEDs
        |                                       |
  Read CachedRendererState  <--------  State updated
        |
        v
  Send response to client
```

### Mutex Protection

- **WiFiManager:** `m_stateMutex` protects state transitions
- **RateLimiter:** Caller must ensure thread safety
- **LedStreamBroadcaster:** Uses `portMUX_TYPE` for ESP32 spinlock
- **SubscriptionManager:** Caller manages thread safety

### Safe Patterns

**DO:**
```cpp
// Use cached state in handlers
const auto& state = server.getCachedRendererState();
uint8_t effect = state.currentEffect;
```

**DO NOT:**
```cpp
// Direct renderer access from network handler (RACE CONDITION!)
uint8_t effect = renderer->getCurrentEffect();  // BAD!
```

---

## Related Documentation

| Document | Path | Description |
|----------|------|-------------|
| API v1 Reference | `docs/api/api-v1.md` | Complete REST API documentation |
| CLAUDE.md | `CLAUDE.md` | Protected files and modification rules |
| WiFi Credentials | `src/config/network_config.h` | Network configuration |
| Feature Flags | `src/config/features.h` | Compile-time feature toggles |

---

## Key Files Summary

| File | Lines | Purpose |
|------|-------|---------|
| `WebServer.cpp` | ~1500 | Main server implementation |
| `WebServer.h` | ~520 | WebServer class definition |
| `WiFiManager.cpp` | ~600 | WiFi state machine (PROTECTED) |
| `WiFiManager.h` | ~445 | WiFiManager class |
| `ApiResponse.h` | ~244 | Response helpers |
| `RequestValidator.h` | ~832 | Schema validation |
| `RateLimiter.h` | ~280 | Rate limiting |
| `WsGateway.cpp` | ~200 | WebSocket gateway |
| `WsCommandRouter.cpp` | ~100 | Command dispatch |

---

**Last Updated:** 2025-01-02
**Subsystem Version:** 2.0.0
