# WebServer Refactor Migration Guide

**Purpose**: Guide for developers working with the refactored WebServer architecture  
**Target Audience**: Developers adding new routes/commands or migrating existing code

## What Changed

### Public API (Stable)

✅ **No changes** - All public `WebServer` methods remain unchanged:
- `WebServer::begin()`
- `WebServer::stop()`
- `WebServer::broadcastStatus()`
- `WebServer::broadcastZoneState()`
- All HTTP endpoints (routes, payloads, responses)
- All WebSocket message types and formats

### Internal Structure (Refactored)

The monolithic `WebServer.cpp` (6633 LOC) has been split into:

1. **Route registration modules**: `StaticAssetRoutes`, `LegacyApiRoutes`, `V1ApiRoutes`
2. **WebSocket gateway**: `WsGateway` for connection/message handling
3. **Command modules**: Domain-specific WS command handlers in `ws/` directory
4. **Shared context**: `WebServerContext` for dependency injection

## Adding New HTTP Routes

### V1 API Routes (`/api/v1/*`)

**Location**: `v2/src/network/webserver/V1ApiRoutes.cpp`

**Example**: Adding `/api/v1/new/endpoint`

```cpp
// In V1ApiRoutes::registerRoutes()
registry.onGet("/api/v1/new/endpoint", [&ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
    if (!checkRateLimit(request)) return;
    if (!checkAPIKey(request)) return;
    
    // Use handler class if available
    handlers::NewHandler::handle(request, ctx);
    
    // Or inline handler
    sendSuccessResponse(request, [&ctx](JsonObject& data) {
        data["value"] = ctx.renderer->getBrightness();
    });
});
```

**Best Practice**: Create a handler class in `v2/src/network/webserver/handlers/` if the logic is substantial (>50 LOC).

### Legacy API Routes (`/api/*`)

**Location**: `v2/src/network/webserver/LegacyApiRoutes.cpp`

**Pattern**: Same as V1 routes, but use `sendLegacySuccess()` / `sendLegacyError()` for responses.

### Static Asset Routes

**Location**: `v2/src/network/webserver/StaticAssetRoutes.cpp`

**Example**: Adding a new static file route

```cpp
// In StaticAssetRoutes::registerRoutes()
registry.onGet("/new-asset.js", [](AsyncWebServerRequest* request) {
    if (LittleFS.exists("/new-asset.js")) {
        request->send(LittleFS, "/new-asset.js", "application/javascript");
        return;
    }
    request->send(HttpStatus::NOT_FOUND, "text/plain", "Missing /new-asset.js");
});
```

## Adding New WebSocket Commands

### Step 1: Determine Command Category

- **Device**: `WsDeviceCommands`
- **Effects**: `WsEffectsCommands`
- **Zones**: `WsZonesCommands` (create if needed)
- **Audio**: `WsAudioCommands` (create if needed)
- **Network**: `WsNetworkCommands` (create if needed)
- **Validation**: `WsValidationCommands` (create if needed)

### Step 2: Create Handler Function

**Pattern**:
```cpp
// In appropriate WsXxxCommands.cpp
static void handleNewCommand(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    
    // Validate input
    if (!doc.containsKey("requiredField")) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "requiredField required", requestId));
        return;
    }
    
    // Business logic
    uint8_t value = doc["requiredField"];
    ctx.actorSystem.setSomeValue(value);
    
    // Response
    String response = buildWsResponse("new.response", requestId, [&ctx, value](JsonObject& data) {
        data["value"] = value;
        data["current"] = ctx.renderer->getSomeValue();
    });
    client->text(response);
}
```

### Step 3: Register Command

```cpp
// In registerWsXxxCommands()
void registerWsXxxCommands(const WebServerContext& ctx) {
    WsCommandRouter::registerCommand("new.command", handleNewCommand);
    // ... other commands ...
}
```

### Step 4: Call Registration

**Location**: `v2/src/network/WebServer.cpp` in `setupWebSocket()`

```cpp
webserver::ws::registerWsXxxCommands(ctx);
```

## Migrating Existing Commands

### From `processWsCommand` to Module

**Before** (in `WebServer::processWsCommand`):
```cpp
else if (type == "my.command") {
    const char* requestId = doc["requestId"] | "";
    // ... handler logic using m_renderer, m_actorSystem, etc. ...
    client->text(response);
}
```

**After** (in `WsXxxCommands.cpp`):
```cpp
static void handleMyCommand(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    // ... same logic, but use ctx.renderer, ctx.actorSystem instead of m_renderer, m_actorSystem ...
    client->text(response);
}

// In registerWsXxxCommands():
WsCommandRouter::registerCommand("my.command", handleMyCommand);
```

**Key Changes**:
- Replace `m_renderer` → `ctx.renderer`
- Replace `m_actorSystem` → `ctx.actorSystem`
- Replace `m_zoneComposer` → `ctx.zoneComposer`
- Replace `m_startTime` → `ctx.startTime`
- Replace `m_apMode` → `ctx.apMode`
- Remove `this->` references

## Accessing WebServer-Specific State

Some handlers may need access to WebServer-specific state not in `WebServerContext`:

### Option 1: Add to Context (Preferred)

If the state is needed by multiple modules, add it to `WebServerContext`:
```cpp
// In WebServerContext.h
struct WebServerContext {
    // ... existing fields ...
    size_t wsClientCount;  // New field
};
```

### Option 2: Use Callback (Temporary)

For one-off needs, pass a callback:
```cpp
// In command handler registration
void registerWsXxxCommands(const WebServerContext& ctx, std::function<size_t()> getClientCount) {
    // Use getClientCount() in handlers
}
```

### Option 3: Keep in WebServer (Last Resort)

If the state is truly WebServer-specific and not needed elsewhere, keep the handler in `WebServer` and call it via fallback.

## Testing New Routes/Commands

### Unit Test Pattern

```cpp
// test_native/test_ws_router.cpp
void test_new_command_routing() {
    // Setup
    WebServerContext ctx = createMockContext();
    registerWsXxxCommands(ctx);
    
    // Test
    JsonDocument doc;
    doc["type"] = "new.command";
    doc["requiredField"] = 42;
    
    MockWebSocketClient client;
    bool handled = WsCommandRouter::route(&client, doc, ctx);
    
    // Assert
    TEST_ASSERT_TRUE(handled);
    // Verify response...
}
```

## Common Pitfalls

### ❌ Don't Cache Context References

```cpp
// BAD: Context may be temporary
static const WebServerContext* s_ctx;
void registerCommands(const WebServerContext& ctx) {
    s_ctx = &ctx;  // DANGEROUS!
}
```

### ❌ Don't Allocate in Hot Paths

```cpp
// BAD: Heap allocation in render loop
String response = "prefix" + value + "suffix";

// GOOD: Use buildWsResponse or fixed buffer
String response = buildWsResponse("type", requestId, [value](JsonObject& data) {
    data["value"] = value;
});
```

### ✅ Do Use Request Validation

```cpp
// GOOD: Validate before processing
if (!doc.containsKey("requiredField")) {
    client->text(buildWsError(ErrorCodes::MISSING_FIELD, "requiredField required", requestId));
    return;
}
```

### ✅ Do Include requestId in Responses

```cpp
// GOOD: Echo requestId for request/response correlation
String response = buildWsResponse("response.type", requestId, [](JsonObject& data) {
    // ...
});
```

## Backward Compatibility Checklist

When adding/modifying routes or commands:

- [ ] HTTP endpoint path unchanged (or new endpoint, not modifying existing)
- [ ] Request payload format unchanged
- [ ] Response format unchanged (JSON structure, field names)
- [ ] Error codes unchanged
- [ ] WebSocket message `type` unchanged
- [ ] Authentication requirements unchanged
- [ ] Rate limiting behaviour unchanged

## Module Size Guidelines

- **Target**: Each module ≤1000 LOC
- **If exceeding**: Split into sub-modules or extract helper functions
- **Example**: `V1ApiRoutes` delegates to handler classes to stay under limit

## Getting Help

- **Architecture questions**: See `docs/architecture/WEB_SERVER_MODULAR_ARCHITECTURE.md`
- **Route inventory**: See `docs/architecture/WEBSERVER_BASELINE_INVENTORY.md`
- **Code examples**: Check existing modules in `v2/src/network/webserver/`

