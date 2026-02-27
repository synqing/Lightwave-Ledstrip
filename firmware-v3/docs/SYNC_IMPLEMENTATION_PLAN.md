# LightwaveOS v2 Sync Subsystem Implementation Plan

## Overview

This document outlines the implementation plan for enabling multi-device synchronization in LightwaveOS v2 using the ESP-IDF native `esp_websocket_client` component.

## Decision Summary

**Selected Library:** `esp_websocket_client` (ESP-IDF native component)

**Alternatives Evaluated:**
- ESPAsyncWebServer: SERVER-ONLY, cannot create outgoing connections
- links2004/WebSockets: WiFi.h header detection fails on ESP32-S3
- gilmaimon/ArduinoWebsockets: Viable alternative, but external dependency

**Why esp_websocket_client:**
1. Pre-compiled in framework (zero additional dependencies)
2. Official Espressif component with proven stability
3. Built-in auto-reconnection with exponential backoff
4. Event-driven callback model matches Actor pattern
5. Native SSL/TLS support for future security hardening
6. ~10KB per connection (40KB total for 4 peers)

## Current State

- Sync subsystem exists in `v2/src/sync/`
- Currently **excluded from build** (`-<sync/*>` in platformio.ini)
- PeerManager has placeholder implementation using `AsyncWebSocketClient`
- Message format, protocol, and serializers are complete

## Implementation Phases

### Phase 1: PeerManager Refactor (Core Change)

**Goal:** Replace `AsyncWebSocketClient` with `esp_websocket_client`

**Files to Modify:**
- `src/sync/PeerManager.h` - Change connection struct
- `src/sync/PeerManager.cpp` - Implement ESP-IDF WebSocket client

**Key Changes:**

1. Replace connection struct:
```cpp
// BEFORE
AsyncWebSocketClient* client;

// AFTER
esp_websocket_client_handle_t client;
```

2. Implement connection lifecycle:
```cpp
esp_websocket_client_config_t config = {
    .uri = "ws://192.168.1.100:80/ws",
    .disable_auto_reconnect = true,  // We handle reconnect with backoff
    .task_prio = 5,
    .task_stack = 4096,
    .buffer_size = 2048,
    .ping_interval_sec = 10,
};

esp_websocket_client_handle_t client = esp_websocket_client_init(&config);
esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, ws_event_handler, this);
esp_websocket_client_start(client);
```

3. Implement static event handler (required for C callback):
```cpp
static void ws_event_handler(void* handler_args, esp_event_base_t base,
                             int32_t event_id, void* event_data);
```

**API Mapping:**

| Current API | ESP-IDF Equivalent |
|-------------|-------------------|
| `client.connect(url)` | `esp_websocket_client_init()` + `start()` |
| `client.text(msg)` | `esp_websocket_client_send_text()` |
| `client.close()` | `esp_websocket_client_close()` + `destroy()` |
| `client.isConnected()` | `esp_websocket_client_is_connected()` |
| `onMessage callback` | `WEBSOCKET_EVENT_DATA` |
| `onConnect callback` | `WEBSOCKET_EVENT_CONNECTED` |
| `onDisconnect callback` | `WEBSOCKET_EVENT_DISCONNECTED` |

### Phase 2: Include Header and Build Configuration

**Files to Modify:**
- `platformio.ini` - Re-enable sync subsystem

**Changes:**

1. Add include for ESP-IDF component:
```cpp
#include "esp_websocket_client.h"
```

2. Update platformio.ini build filter:
```ini
; BEFORE
build_src_filter = +<*> -<sync/*>

; AFTER
build_src_filter = +<*>
```

### Phase 3: Static Callback Wrapper

ESP-IDF uses C callbacks, but PeerManager is C++. Need static wrapper:

```cpp
class PeerManager {
private:
    // Static callback wrapper
    static void wsEventHandler(void* handler_args, esp_event_base_t base,
                               int32_t event_id, void* event_data) {
        PeerManager* self = static_cast<PeerManager*>(handler_args);
        self->handleWebSocketEvent(event_id, event_data);
    }

    // Instance method for actual processing
    void handleWebSocketEvent(int32_t event_id, void* event_data);
};
```

### Phase 4: Connection Pool Management

Replace current slot management with ESP-IDF handles:

```cpp
struct PeerConnection {
    char uuid[16];
    uint8_t ip[4];
    uint16_t port;
    esp_websocket_client_handle_t client;  // Changed from AsyncWebSocketClient*
    uint32_t lastActivityMs;
    uint32_t lastPingMs;
    uint32_t reconnectDelayMs;
    uint8_t missedPings;
    bool connecting;
    bool connected;
};

PeerConnection m_connections[MAX_PEER_CONNECTIONS];  // 4 slots
```

### Phase 5: Message Handling

Handle fragmented messages (esp_websocket_client delivers chunks):

```cpp
void handleWebSocketEvent(int32_t event_id, void* event_data) {
    esp_websocket_event_data_t* data = (esp_websocket_event_data_t*)event_data;

    switch (event_id) {
        case WEBSOCKET_EVENT_DATA:
            // Check if this is a complete message or fragment
            if (data->payload_offset == 0 &&
                data->data_len == data->payload_len) {
                // Complete message - process directly
                processMessage(data->data_ptr, data->data_len);
            } else {
                // Fragmented - accumulate in buffer
                accumulateFragment(data);
            }
            break;
    }
}
```

### Phase 6: Integration Testing

1. Enable sync subsystem in build
2. Flash to single device, verify no crashes
3. Flash to two devices on same network
4. Verify mDNS discovery finds peers
5. Verify WebSocket client connects to peer server
6. Verify state sync works (leader broadcasts to followers)
7. Test reconnection on WiFi disconnect

## Memory Budget

| Component | Estimated Size |
|-----------|---------------|
| esp_websocket_client (per connection) | ~10 KB |
| 4 peer connections | ~40 KB |
| Message buffer (1024 bytes) | ~1 KB |
| PeerConnection structs | ~0.4 KB |
| **Total Sync Overhead** | **~42 KB** |

ESP32-S3 has 512KB SRAM. Current v2 build uses ~180KB. Sync adds ~42KB = ~222KB total (43% utilization).

## Configuration Constants

From `SyncProtocol.h`:
```cpp
constexpr uint8_t  MAX_PEER_CONNECTIONS     = 4;
constexpr uint8_t  MAX_DISCOVERED_PEERS     = 8;
constexpr uint16_t MAX_MESSAGE_SIZE         = 1024;
constexpr uint32_t HEARTBEAT_INTERVAL_MS    = 10000;
constexpr uint8_t  HEARTBEAT_MISS_LIMIT     = 3;
constexpr uint32_t RECONNECT_INITIAL_MS     = 1000;
constexpr uint32_t RECONNECT_MAX_MS         = 16000;
constexpr uint32_t PEER_SCAN_INTERVAL_MS    = 30000;
constexpr uint32_t PEER_TIMEOUT_MS          = 90000;
```

## Risk Mitigation

| Risk | Mitigation |
|------|-----------|
| esp_websocket_client not linked | Verify symbol exists with `nm libesp_websocket_client.a` |
| C callback context loss | Use static wrapper with `this` pointer as handler_args |
| Message fragmentation | Accumulate fragments before processing |
| Memory exhaustion | Limit to 4 connections, 1KB message buffer |
| WiFi instability | Built-in reconnection with exponential backoff |

## Files Created/Modified

| File | Status | Changes |
|------|--------|---------|
| `src/sync/PeerManager.h` | Modify | Replace AsyncWebSocketClient with esp_websocket_client_handle_t |
| `src/sync/PeerManager.cpp` | Modify | Implement ESP-IDF WebSocket client API |
| `platformio.ini` | Modify | Remove `-<sync/*>` exclusion |
| `docs/SYNC_IMPLEMENTATION_PLAN.md` | Create | This document |

## Success Criteria

1. Build completes with sync subsystem enabled
2. Two ESP32-S3 devices discover each other via mDNS
3. Devices establish WebSocket connections to each other
4. Leader election completes (highest UUID wins)
5. State changes propagate from leader to followers
6. Followers update their LED display within 100ms of leader
7. Reconnection works after brief WiFi outage

## Next Steps

1. Read current PeerManager.cpp implementation
2. Create esp_websocket_client wrapper methods
3. Update PeerConnection struct
4. Implement static callback handler
5. Test with single device (no crashes)
6. Test with two devices (full sync)

---

*Generated: 2025-12-21*
*Based on specialist agent research findings*
