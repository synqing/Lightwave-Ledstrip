# TRACE_INSTRUMENTATION_SPEC Section 4: WiFi / WebServer Perturbation Surface

**Target:** Measure network activity correlation with render-path latency. The hypothesis is that asynchronous WiFi events and WS message dispatch (both on Core 0) create indirect interference via heap contention and cross-core synchronization, causing render hiccups in the 2 ms deadline window.

**Version:** v1.0 (2026-04-27)
**Scope:** AP-only K1 architecture; incoming iOS/Tab5 WS commands only
**Status:** Design phase (awaiting implementation)

---

## 1. Executive Summary

**Observed Problem:**
- WiFi/log subsystem fires asynchronously, interleaved with JSON render traces
- When iOS/Tab5 connects and sends WS commands, renderer occasionally misses 2 ms deadline
- No instrumentation exists to correlate network activity with render hiccups

**Instrumentation Strategy:**
- Tier 4 causal-marker **instants** (cheapest): WiFi AP/client events, WS lifecycle
- Tier 2 opt-in **spans** (behind `FEATURE_TRACE_NETWORK=1`): WS dispatch, handler decomposition
- Tier 1 **counters** (1 Hz sampling): active clients, WS clients, request rates

**Expected Outcome After Implementation:**
```
analyse_trace.py --conditional render_frame_us BY ws_msg_dispatch in last 16ms
```
This command will segment traces into "no network activity" vs "WS active in last N ms" regions and compare render_frame_us distributions, proving or disproving the correlation hypothesis.

---

## 2. Cross-Core Interference Model

### 2.1 AsyncTCP Core Affinity

Per `CLAUDE.md` ("AsyncTCP Core Affinity Changed to Core 0"):
- **AsyncTCP task runs on Core 0** (not Core 1)
- WS message events are dispatched on AsyncTCP task (Core 0)
- WS dispatch → WsCommandRouter::route() → handler → state mutation **all on Core 0**

### 2.2 Why Core 0 Activity Still Affects Render (Core 1)

Although WS dispatch happens on Core 0, it indirectly perturbs Core 1's render path:

1. **Heap Contention:**
   - WS handlers allocate JSON documents, strings, response buffers
   - Heap is shared between cores; mutex-protected allocations stall Core 1
   - Example: `JsonDocument doc; doc["key"] = ...` (esp_malloc on Core 0 blocks Core 1's malloc)

2. **Cache Coherency Traffic:**
   - Shared state mutations (e.g. setting current effect, palette, zone parameters)
   - Core 1 reads this state during render; Core 0 writes it during WS dispatch
   - Coherency messages cross the memory bus, evicting render-critical cache lines

3. **Mutex / SpinLock Contention:**
   - Any state protected by FreeRTOS mutex is contended
   - Example: ZoneComposer state accessed by both render (Core 1) and WS handlers (Core 0)

4. **Indirect WiFi Event Cost (even if not WS dispatch):**
   - WiFi.onEvent callbacks run on WiFi task (Core 0)
   - Logging in WiFi callbacks writes to UART, triggers heap allocation
   - Delays task scheduling on Core 0, indirectly delays AsyncTCP task's wake-up

### 2.3 Observable Effects

- **Render frame latency spikes** when WS messages arrive
- **Jitter in 2 ms deadline** correlated with WS dispatch rate
- **No direct Core 1 blocking,** but heap and cache effects create latency

---

## 3. Instrumentation Points

### 3.1 Tier 4: Causal-Marker Instants (Always Enabled)

Cheapest instrumentation; enables correlation analysis without overhead.

#### 3.1.1 WiFi AP Lifecycle Events

| Instant | When | Arguments | File:Line | Comment |
|---------|------|-----------|-----------|---------|
| `wifi_ap_started` | WIFI_EVENT_AP_START | none | WiFiManager.cpp:~1068 | Soft-AP initialized, SSID broadcast active |
| `wifi_ap_stopped` | WIFI_EVENT_AP_STOP | none | WiFiManager.cpp | Soft-AP shutdown (rare; normally persistence) |

**Implementation Note:** Both events already logged at INFO level. Add `TRACE_INSTANT("wifi_ap_started")` after log.

#### 3.1.2 WiFi AP Client Connection/Disconnection

| Instant | When | Arguments | File:Line | Comment |
|---------|------|-----------|-----------|---------|
| `wifi_client_connected_<N>` | WIFI_EVENT_AP_STA_CONNECTED | client_count (1-4) | WebServer.cpp:~454 | New client accepted on AP; N = total connected |
| `wifi_client_disconnected_<N>` | WIFI_EVENT_AP_STA_DISCONNECTED | client_count (0-3) | WebServer.cpp:~454 | Client dropped from AP; N = remaining connected |

**Implementation Note:** WebServer already listens to these events. Embed TRACE_INSTANT calls with current client count from WiFi.softAPgetStationNum().

#### 3.1.3 WebSocket Client Lifecycle

| Instant | When | Arguments | File:Line | Comment |
|---------|------|-----------|-----------|---------|
| `ws_client_connected_<id>` | WsGateway::handleConnect() | client_id (hex) | WsGateway.cpp:~75 | New WS handshake accepted |
| `ws_client_disconnected_<id>` | WsGateway::handleDisconnect() | client_id (hex) | WsGateway.cpp:~81 | WS client closed or timeout |

**Implementation Note:** Already tracked in m_clientEpochs; add trace instant with epoch.

#### 3.1.4 OTA Firmware Update Lifecycle

| Instant | When | Arguments | File:Line | Comment |
|---------|------|-----------|-----------|---------|
| `ota_started` | WS "ota.start" command | none | WsOtaCommands.cpp | OTA streaming begins |
| `ota_chunk` | Per chunk received | chunk_size, offset_percent (0-100) | WsOtaCommands.cpp | Periodic progress marker (~10% intervals) |
| `ota_completed` | All chunks received, hash OK | file_size, checksum | WsOtaCommands.cpp | OTA finalized; reboot pending |
| `ota_failed` | Hash mismatch or timeout | error_code | WsOtaCommands.cpp | OTA aborted |

**Implementation Note:** OTA handlers already exist; add trace instants in success/failure paths.

---

### 3.2 Tier 2: Opt-In Spans (Behind `FEATURE_TRACE_NETWORK=1`)

Structured spans with start/end timestamps; enable decomposition of WS dispatch latency.

#### 3.2.1 WebSocket Message Dispatch Span

| Span | When | Start | End | File:Line | Comment |
|------|------|-------|-----|-----------|---------|
| `ws_msg_dispatch` | WS message received | WsGateway::handleMessage() entry | WsCommandRouter::route() return | WsCommandRouter.cpp:32 | Wraps JSON parse + route + dispatch to handler |

**Pseudo-code:**
```cpp
void WsGateway::handleMessage(AsyncWebSocketClient* client, uint8_t* data, size_t len) {
    TRACE_SPAN_START("ws_msg_dispatch");
    // ... JSON deserialize, auth check ...
    bool handled = WsCommandRouter::route(client, doc, m_ctx);
    TRACE_SPAN_END("ws_msg_dispatch");
}
```

**Attributes to capture:**
- `msg_type` (string, e.g. "effects.set_current")
- `msg_size_bytes` (uint16)
- `client_id` (uint32, hex)

#### 3.2.2 Per-Handler Spans

For major WS command handlers (dispatch target):

| Span Prefix | Handler Signature | File | Comment |
|-------------|-------------------|------|---------|
| `ws_handler_effects_set_current` | WsEffectsCommands::handleSetCurrent() | WsEffectsCommands.cpp | Set active effect ID |
| `ws_handler_effects_query` | WsEffectsCommands::handleQuery() | WsEffectsCommands.cpp | List all effects metadata |
| `ws_handler_parameters_set` | WsParameterCommands::handleSet() | (handlers/*.cpp) | Update expression parameter (hue, saturation, etc.) |
| `ws_handler_parameters_get` | WsParameterCommands::handleGet() | (handlers/*.cpp) | Fetch current parameters |
| `ws_handler_palette_set` | WsPaletteCommands::handleSet() | WsPaletteCommands.cpp | Change active palette |
| `ws_handler_zones_set` | WsZonesCommands::handleSet() | WsZonesCommands.cpp | Configure zone layout |
| `ws_handler_preset_save` | WsPresetCommands::handleSave() | WsPresetCommands.cpp | Save effect+palette+params preset |
| `ws_handler_preset_load` | WsPresetCommands::handleLoad() | WsPresetCommands.cpp | Load preset by ID |
| `ws_handler_ota_*` | WsOtaCommands::handle*() | WsOtaCommands.cpp | OTA start, chunk, finalize |
| `ws_handler_stream_subscribe` | WsStreamCommands::handleSubscribe() | WsStreamCommands.cpp | Subscribe to LED/audio stream |

**Pseudo-code (per handler):**
```cpp
void WsEffectsCommands::handleSetCurrent(AsyncWebSocketClient* client, JsonDocument& doc, ...) {
    TRACE_SPAN_START("ws_handler_effects_set_current");
    // ... handler logic ...
    TRACE_SPAN_END("ws_handler_effects_set_current");
}
```

**Attributes:**
- `effect_id` (uint16, if applicable)
- `response_size_bytes` (uint16)
- `status` (enum: success / error_code)

#### 3.2.3 REST Handler Spans

For major REST endpoints (high-frequency or heavy):

| Span | Route | File | Comment |
|------|-------|------|---------|
| `rest_get_device_status` | GET /api/v1/device/status | V1ApiRoutes.cpp:~96 | System state dump (CPU, memory, clients) |
| `rest_get_effects_metadata` | GET /api/v1/effects/metadata?id=N | V1ApiRoutes.cpp:~154 | Effect properties (read-heavy) |
| `rest_put_effects_current` | PUT /api/v1/effects/current | V1ApiRoutes.cpp | Set active effect (state mutation) |
| `rest_put_parameters_<name>` | PUT /api/v1/parameters/{name} | V1ApiRoutes.cpp | Update param (hue, brightness, etc.) |
| `rest_get_palette_*` | GET /api/v1/palettes/* | V1ApiRoutes.cpp | Palette queries |
| `rest_post_presets_save` | POST /api/v1/presets/save | V1ApiRoutes.cpp | Save preset |
| `rest_get_presets` | GET /api/v1/presets | V1ApiRoutes.cpp | List presets |

**Pseudo-code (V1ApiRoutes.cpp):**
```cpp
registry.onGet("/api/v1/device/status", [ctx, checkRateLimit, ...](AsyncWebServerRequest* request) {
    TRACE_SPAN_START("rest_get_device_status");
    // ... build response ...
    request->send(200, "application/json", response);
    TRACE_SPAN_END("rest_get_device_status");
});
```

#### 3.2.4 Response Send Span (Coarse-grained)

Wraps the actual transmission of responses (not per-request; aggregate counter):

| Span | When | File | Comment |
|------|------|------|---------|
| `rest_response_send` | AsyncWebServerRequest::send() | V1ApiRoutes.cpp | Tracks HTTP response queueing to AsyncTCP |
| `ws_text_send` | AsyncWebSocketClient::text() | WsGateway.cpp, handlers | Tracks WS message transmission |

**Implementation:** Wrap calls to request->send() and client->text():
```cpp
// In response builder:
TRACE_INSTANT("rest_response_send", {
    {"route", "/api/v1/device/status"},
    {"status_code", 200},
    {"size_bytes", response.length()}
});
```

---

### 3.3 Tier 1: Counters (1 Hz Health Sampling)

Sampled from main health task; count events, aggregate latencies.

#### 3.3.1 WS Activity Counters

| Counter | Sampled From | Update Frequency | Comment |
|---------|--------------|------------------|---------|
| `wifi_clients` | WiFi.softAPgetStationNum() | 1 Hz (health task) | Current AP client count (0-4) |
| `ws_clients` | WsGateway::m_stats.connectAccepted | 1 Hz | Cumulative WS connections since boot |
| `ws_dispatch_count` | WsCommandRouter calls | Per-dispatch (if span enabled) | Counter of WS message dispatch attempts |
| `ws_dispatch_us` | Span duration (only if FEATURE_TRACE_NETWORK) | Per-dispatch | Per-dispatch duration in microseconds; only logged if span enabled |
| `ws_errors` | WsGateway::m_stats counters | 1 Hz | Sum of parse errors + unknown commands + oversized frames |
| `rest_requests_active` | AsyncWebServer client count | 1 Hz | HTTP requests in-flight |

**Implementation in HealthTask (main.cpp):**
```cpp
// Existing health loop, add:
if (FEATURE_TRACE_NETWORK) {
    TRACE_COUNTER("wifi_clients", WiFi.softAPgetStationNum());
    TRACE_COUNTER("ws_clients", wsGateway->getStats().connectAccepted);
    // Require WsGateway to track dispatch count:
    TRACE_COUNTER("ws_dispatch_count", wsGateway->getDispatchCount());
    TRACE_COUNTER("ws_errors", wsGateway->getStats().parseErrors +
                               wsGateway->getStats().unknownCommands);
}
```

#### 3.3.2 Network Load Indicators

| Counter | Source | Comment |
|---------|--------|---------|
| `ws_bytes_received_total` | AsyncWebSocket frame totals | Cumulative bytes from all clients |
| `ws_bytes_sent_total` | AsyncWebSocket frame totals | Cumulative bytes to all clients |
| `rest_bytes_received_total` | AsyncWebServer request bodies | Cumulative request body bytes |
| `rest_bytes_sent_total` | AsyncWebServer response bodies | Cumulative response bytes |

**Note:** These are optional (add if bandwidth tracking desired); not critical for latency correlation.

---

## 4. Instrumentation Tables (Per-Event)

### 4.1 Tier 4 Instants

#### wifi_ap_started
```yaml
Name: wifi_ap_started
Type: TRACE_INSTANT
File: firmware-v3/src/network/WiFiManager.cpp
Line: ~1068 (WIFI_EVENT_AP_START handler)
Trigger: WiFi event WIFI_EVENT_AP_START fired
Attributes: none
Example Log:
  [T=1234567 μs] Event: wifi_ap_started
```

#### wifi_ap_stopped
```yaml
Name: wifi_ap_stopped
Type: TRACE_INSTANT
File: firmware-v3/src/network/WiFiManager.cpp
Line: ~1100 (WIFI_EVENT_AP_STOP handler)
Trigger: WiFi event WIFI_EVENT_AP_STOP fired
Attributes: none
Example Log:
  [T=9999999 μs] Event: wifi_ap_stopped (rare)
```

#### wifi_client_connected_<N>
```yaml
Name: wifi_client_connected_<N>
Type: TRACE_INSTANT
File: firmware-v3/src/network/WebServer.cpp
Line: ~454 (WiFi.onEvent callback)
Trigger: WIFI_EVENT_AP_STA_CONNECTED
Attributes:
  client_count: uint8_t (new total; 1-4)
  ip_addr: optional (client IPv4)
Example Log:
  [T=5000000 μs] Event: wifi_client_connected_1 (client_count=1)
  [T=5100000 μs] Event: wifi_client_connected_2 (client_count=2)
```

#### wifi_client_disconnected_<N>
```yaml
Name: wifi_client_disconnected_<N>
Type: TRACE_INSTANT
File: firmware-v3/src/network/WebServer.cpp
Line: ~460 (WiFi.onEvent callback)
Trigger: WIFI_EVENT_AP_STA_DISCONNECTED
Attributes:
  client_count: uint8_t (new total; 0-3)
Example Log:
  [T=30000000 μs] Event: wifi_client_disconnected_1 (client_count=1)
```

#### ws_client_connected_<id>
```yaml
Name: ws_client_connected_<id>
Type: TRACE_INSTANT
File: firmware-v3/src/network/webserver/WsGateway.cpp
Line: ~75 (WsGateway::handleConnect)
Trigger: WS handshake accepted (WS_EVT_CONNECT)
Attributes:
  client_id: uint32_t (hex, embedded in name)
  epoch: uint32_t (connection epoch; resets on reconnect)
  ip_addr: optional (client IPv4)
Example Log:
  [T=5010000 μs] Event: ws_client_connected_0x12340001 (epoch=0)
```

#### ws_client_disconnected_<id>
```yaml
Name: ws_client_disconnected_<id>
Type: TRACE_INSTANT
File: firmware-v3/src/network/webserver/WsGateway.cpp
Line: ~81 (WsGateway::handleDisconnect)
Trigger: WS disconnection (WS_EVT_DISCONNECT)
Attributes:
  client_id: uint32_t (hex)
  reason: enum (1000=normal, 1002=protocol_error, 1008=policy_violation, etc.)
Example Log:
  [T=25000000 μs] Event: ws_client_disconnected_0x12340001 (reason=1000)
```

#### ota_started
```yaml
Name: ota_started
Type: TRACE_INSTANT
File: firmware-v3/src/network/webserver/ws/WsOtaCommands.cpp
Line: ~50 (ota.start handler)
Trigger: Client sends {"type":"ota.start", "file_size": N}
Attributes:
  file_size: uint32_t (bytes)
  client_id: uint32_t (hex)
Example Log:
  [T=6000000 μs] Event: ota_started (file_size=524288)
```

#### ota_chunk
```yaml
Name: ota_chunk
Type: TRACE_INSTANT (periodic; ~10% intervals)
File: firmware-v3/src/network/webserver/ws/WsOtaCommands.cpp
Line: ~100 (per-chunk handler)
Trigger: Chunk received and validated
Attributes:
  chunk_size: uint16_t (bytes)
  offset: uint32_t (bytes received so far)
  progress_percent: uint8_t (0-100)
Example Log:
  [T=6500000 μs] Event: ota_chunk (offset=52428, progress_percent=10)
  [T=7000000 μs] Event: ota_chunk (offset=262144, progress_percent=50)
```

#### ota_completed
```yaml
Name: ota_completed
Type: TRACE_INSTANT
File: firmware-v3/src/network/webserver/ws/WsOtaCommands.cpp
Line: ~150 (finalization)
Trigger: All chunks received, hash verified, reboot scheduled
Attributes:
  total_size: uint32_t (bytes)
  checksum: uint32_t (CRC32 or similar)
  reboot_in_ms: uint16_t (milliseconds until reboot)
Example Log:
  [T=10000000 μs] Event: ota_completed (total_size=524288, reboot_in_ms=5000)
```

#### ota_failed
```yaml
Name: ota_failed
Type: TRACE_INSTANT
File: firmware-v3/src/network/webserver/ws/WsOtaCommands.cpp
Line: ~160 (error handler)
Trigger: Hash mismatch, timeout, or other OTA failure
Attributes:
  error_code: uint16_t (enum; e.g. CHECKSUM_MISMATCH=1, TIMEOUT=2, ...)
  details: string (error message, e.g. "CRC32 mismatch")
Example Log:
  [T=11000000 μs] Event: ota_failed (error_code=1, details="CRC32 mismatch")
```

---

### 4.2 Tier 2 Spans (Behind FEATURE_TRACE_NETWORK)

#### ws_msg_dispatch (Parent Span)
```yaml
Name: ws_msg_dispatch
Type: TRACE_SPAN_START / TRACE_SPAN_END
File: firmware-v3/src/network/webserver/WsGateway.cpp
Line: ~89 (handleMessage start) to ~(end of route call)
Duration: Typical 10-500 μs (JSON parse + route lookup + dispatch)
Attributes:
  msg_type: string (e.g. "effects.set_current", "parameters.set")
  msg_size_bytes: uint16_t
  client_id: uint32_t (hex)
  request_id: string (from JSON, if present)
Example Log:
  [T=5020000 μs] Span START: ws_msg_dispatch (msg_type="effects.set_current", client_id=0x12340001)
  [T=5020050 μs] Span END: ws_msg_dispatch (duration=50 μs)
```

#### ws_handler_effects_set_current (Child Span, under ws_msg_dispatch)
```yaml
Name: ws_handler_effects_set_current
Type: TRACE_SPAN_START / TRACE_SPAN_END
File: firmware-v3/src/network/webserver/ws/WsEffectsCommands.cpp
Line: ~getHandler("effects.set_current") entry/exit
Duration: Typical 5-50 μs (state mutation + response build)
Attributes:
  effect_id: uint16_t
  effect_name: string (optional)
  status: enum (success, invalid_id, etc.)
Example Log:
  [T=5020010 μs] Span START: ws_handler_effects_set_current (effect_id=42)
  [T=5020035 μs] Span END: ws_handler_effects_set_current (duration=25 μs, status=success)
```

#### ws_handler_parameters_set (Child Span)
```yaml
Name: ws_handler_parameters_set
Type: TRACE_SPAN_START / TRACE_SPAN_END
File: firmware-v3/src/network/webserver/ws/WsParameterCommands.cpp (or similar)
Line: ~getHandler("parameters.set") entry/exit
Duration: Typical 5-100 μs (validate + ActorSystem::set* call)
Attributes:
  param_name: string (e.g. "hue", "saturation", "intensity")
  param_value: int32_t or float
  status: enum
Example Log:
  [T=5030000 μs] Span START: ws_handler_parameters_set (param_name="hue", param_value=180)
  [T=5030045 μs] Span END: ws_handler_parameters_set (duration=45 μs, status=success)
```

#### ws_handler_palette_set (Child Span)
```yaml
Name: ws_handler_palette_set
Type: TRACE_SPAN_START / TRACE_SPAN_END
File: firmware-v3/src/network/webserver/ws/WsPaletteCommands.cpp
Line: ~getHandler("palette.set") entry/exit
Duration: Typical 5-30 μs (state mutation)
Attributes:
  palette_id: uint8_t
  palette_name: string (optional)
Example Log:
  [T=5040000 μs] Span START: ws_handler_palette_set (palette_id=7)
  [T=5040020 μs] Span END: ws_handler_palette_set (duration=20 μs)
```

#### rest_get_device_status (REST Handler Span)
```yaml
Name: rest_get_device_status
Type: TRACE_SPAN_START / TRACE_SPAN_END
File: firmware-v3/src/network/webserver/V1ApiRoutes.cpp
Line: ~96 (GET /api/v1/device/status handler entry/exit)
Duration: Typical 50-200 μs (state dump + JSON encode)
Attributes:
  response_size_bytes: uint16_t
  status_code: uint16_t (200, 401, 500, etc.)
Example Log:
  [T=6000000 μs] Span START: rest_get_device_status
  [T=6000120 μs] Span END: rest_get_device_status (duration=120 μs, response_size_bytes=1024)
```

#### rest_put_effects_current (REST Handler Span)
```yaml
Name: rest_put_effects_current
Type: TRACE_SPAN_START / TRACE_SPAN_END
File: firmware-v3/src/network/webserver/V1ApiRoutes.cpp
Line: ~PUT /api/v1/effects/current handler
Duration: Typical 10-50 μs
Attributes:
  effect_id: uint16_t (from JSON body)
  status_code: uint16_t
Example Log:
  [T=7000000 μs] Span START: rest_put_effects_current (effect_id=42)
  [T=7000025 μs] Span END: rest_put_effects_current (duration=25 μs, status_code=200)
```

---

### 4.3 Tier 1 Counters

#### wifi_clients (1 Hz sample)
```yaml
Name: wifi_clients
Type: TRACE_COUNTER (sampled 1 Hz)
File: firmware-v3/src/main.cpp (health task)
Metric: WiFi.softAPgetStationNum()
Range: 0-4 (K1 AP supports up to 4 concurrent clients)
Example Log:
  [T=1000000 μs] Counter: wifi_clients = 0
  [T=2000000 μs] Counter: wifi_clients = 1
  [T=5000000 μs] Counter: wifi_clients = 2
  [T=25000000 μs] Counter: wifi_clients = 1
```

#### ws_clients (1 Hz sample)
```yaml
Name: ws_clients
Type: TRACE_COUNTER (sampled 1 Hz)
File: firmware-v3/src/main.cpp (health task)
Metric: WsGateway::m_stats.connectAccepted (cumulative)
Range: 0-999+ (cumulative connections since boot)
Example Log:
  [T=1000000 μs] Counter: ws_clients = 0
  [T=5010000 μs] Counter: ws_clients = 1 (after WS handshake)
  [T=10000000 μs] Counter: ws_clients = 2 (second client)
```

#### ws_dispatch_count (Per-dispatch, if span enabled)
```yaml
Name: ws_dispatch_count
Type: TRACE_COUNTER (incremented per dispatch if FEATURE_TRACE_NETWORK=1)
File: firmware-v3/src/network/webserver/WsGateway.cpp
Metric: Incremented in handleMessage() after route() succeeds
Range: 0-999999+ (cumulative dispatch count)
Example Log:
  [T=5020000 μs] Counter: ws_dispatch_count = 1
  [T=5100000 μs] Counter: ws_dispatch_count = 2
  [T=5200000 μs] Counter: ws_dispatch_count = 3
```

#### ws_dispatch_us (Per-dispatch duration, if span enabled)
```yaml
Name: ws_dispatch_us
Type: TRACE_COUNTER (per-dispatch duration, only if FEATURE_TRACE_NETWORK=1)
File: firmware-v3/src/network/webserver/WsGateway.cpp
Metric: End time - start time of ws_msg_dispatch span
Range: 10-1000 μs (typical)
Example Log:
  [T=5020050 μs] Counter: ws_dispatch_us = 50
  [T=5100045 μs] Counter: ws_dispatch_us = 45
  [T=5200120 μs] Counter: ws_dispatch_us = 120 (slow dispatch due to heap contention)
```

#### ws_errors (1 Hz sample)
```yaml
Name: ws_errors
Type: TRACE_COUNTER (sampled 1 Hz)
File: firmware-v3/src/main.cpp (health task)
Metric: WsGateway::m_stats.parseErrors + unknownCommands + oversizedFrames
Range: 0-999+ (cumulative error count)
Example Log:
  [T=1000000 μs] Counter: ws_errors = 0
  [T=5000000 μs] Counter: ws_errors = 1 (one parse error)
  [T=10000000 μs] Counter: ws_errors = 2 (one unknown command)
```

---

## 5. Implementation Roadmap

### Phase 1: Tier 4 Instants (Lowest Effort, Highest Priority)

**Goal:** Enable basic correlation without runtime overhead.

1. **WiFi AP Events** (WiFiManager.cpp)
   - Add TRACE_INSTANT("wifi_ap_started") after WIFI_EVENT_AP_START
   - Add TRACE_INSTANT("wifi_ap_stopped") after WIFI_EVENT_AP_STOP

2. **WiFi Client Events** (WebServer.cpp, ~line 454)
   - Modify WiFi.onEvent callback to call TRACE_INSTANT("wifi_client_connected_<N>") with count
   - Modify WiFi.onEvent callback to call TRACE_INSTANT("wifi_client_disconnected_<N>") with count

3. **WS Client Events** (WsGateway.cpp)
   - Modify handleConnect() to call TRACE_INSTANT("ws_client_connected_<id>")
   - Modify handleDisconnect() to call TRACE_INSTANT("ws_client_disconnected_<id>")

4. **OTA Events** (WsOtaCommands.cpp)
   - Add TRACE_INSTANT("ota_started") at start of OTA handler
   - Add TRACE_INSTANT("ota_chunk") at ~10% intervals
   - Add TRACE_INSTANT("ota_completed") on success
   - Add TRACE_INSTANT("ota_failed") on error

**Effort:** ~2-4 hours
**Test:** Capture trace with iOS app connecting, sending a few WS commands, then disconnecting. Verify instants appear.

### Phase 2: Tier 1 Counters (1 Hz Sampling)

**Goal:** Track activity rates over time; enable statistical correlation.

1. **Add to health task** (main.cpp):
   ```cpp
   if (FEATURE_TRACE_NETWORK) {
       TRACE_COUNTER("wifi_clients", WiFi.softAPgetStationNum());
       TRACE_COUNTER("ws_clients", wsGateway->getStats().connectAccepted);
       // ... other counters ...
   }
   ```

2. **Extend WsGateway stats tracking:**
   - Add dispatch count field to WsGateway::Stats
   - Increment on every handleMessage() call (before route)

**Effort:** ~1-2 hours
**Test:** Run health task trace capture; verify counters update at 1 Hz.

### Phase 3: Tier 2 Spans (Optional, Requires FEATURE_TRACE_NETWORK)

**Goal:** Decompose handler latencies; identify hotspots.

1. **ws_msg_dispatch parent span** (WsGateway.cpp::handleMessage):
   ```cpp
   TRACE_SPAN_START("ws_msg_dispatch", {
       {"msg_type", type},
       {"msg_size_bytes", len},
       {"client_id", clientId}
   });
   // ... route + dispatch ...
   TRACE_SPAN_END("ws_msg_dispatch");
   ```

2. **Per-handler spans** (WsEffectsCommands.cpp, WsParametersCommands.cpp, etc.):
   - Wrap each handler in TRACE_SPAN_START/END
   - Capture effect_id, param_name, etc. as attributes

3. **REST handler spans** (V1ApiRoutes.cpp):
   - Wrap high-frequency routes in TRACE_SPAN
   - Capture response size, status code

**Effort:** ~4-6 hours (many handlers)
**Test:** Enable FEATURE_TRACE_NETWORK; capture trace with continuous WS traffic; verify span tree.

---

## 6. Test Protocol: Reproducing iOS-Stress Conditions

**Goal:** Capture network activity that correlates with render-frame latency spikes.

### 6.1 Setup

1. **Hardware:**
   - K1 running latest firmware with all Tier 4 + Tier 1 instrumentation
   - iPad/iPhone running iOS LightwaveOS app
   - USB serial connection for log/trace capture

2. **Trace Configuration:**
   ```
   FEATURE_TRACE_ENABLED=1
   FEATURE_TRACE_NETWORK=1  (for spans)
   TRACE_BUFFER_SIZE_BYTES=512K  (capture 2-5 seconds at full network load)
   ```

3. **Baseline Capture (No Network Activity):**
   - K1 idle, AP running, zero clients connected
   - Capture 10 seconds of trace (no network activity)
   - Measure render_frame_us distribution (should be tight, ~1500-1800 μs)

### 6.2 Network Stress Test

**Phase 1: Single Client, Slow Commands (5 seconds)**
1. Open iOS app, connect to K1 AP
2. Manually change effect (1 WS msg every 2 seconds, 2-3 messages total)
3. Capture trace; measure render_frame_us

**Phase 2: Single Client, Fast Parameter Slider (10 seconds)**
1. Keep iOS app connected
2. Run continuous slider (hue, saturation) — generates 10-20 WS msg/sec
3. Capture trace; measure render_frame_us distribution

**Phase 3: Two Clients, Overlapping Commands (10 seconds)**
1. Connect second device (Tab5) to same AP
2. Run slider on iPad, occasional status polls on Tab5
3. Capture trace; measure render_frame_us under contention

**Phase 4: OTA Update (60 seconds)**
1. Initiate OTA upload from iOS app
2. Capture full OTA sequence (start, chunks, completion)
3. Measure render latency during OTA (expect spikes)

### 6.3 Trace Analysis

**Command to run (post-capture):**
```bash
analyse_trace.py \
  --input trace.bin \
  --conditional render_frame_us BY ws_msg_dispatch in last 16ms \
  --output correlation_report.json
```

**Expected Output:**
```json
{
  "metric": "render_frame_us",
  "condition": "ws_msg_dispatch in last 16ms",
  "no_network_activity": {
    "mean": 1620.5,
    "stddev": 45.2,
    "min": 1510,
    "max": 1890,
    "p99": 1780
  },
  "during_ws_dispatch": {
    "mean": 1750.3,
    "stddev": 180.5,
    "min": 1520,
    "max": 2350,
    "p99": 2100
  },
  "correlation_strength": "moderate",
  "conclusion": "WS dispatch within last 16ms raises mean render latency by ~130 μs (8%)"
}
```

**Interpretation:**
- If `correlation_strength` = "strong": WS activity directly perturbs render; validate the cross-core interference model
- If `correlation_strength` = "weak": no evidence of latency correlation; investigate other interference vectors (WiFi events, heap, etc.)

---

## 7. Tier 3 / Advanced Instrumentation (Future)

Not in scope for v1.0, but mentioned for completeness:

- **Per-zone render latency:** Trace zone composition time (Core 1) separately from WiFi dispatch (Core 0)
- **Heap allocation tracking:** Trace malloc/free on Core 0 and Core 1; detect contention
- **Mutex lock contention:** Trace FreeRTOS mutex wait times
- **Cache miss monitoring:** Use ESP32-S3 PMU to count L1/L2 cache events during WS dispatch
- **AsyncTCP internal events:** Trace task scheduling delays within AsyncTCP on Core 0

---

## 8. Known Limitations & Assumptions

1. **AsyncTCP runs on Core 0:** Per CLAUDE.md. If this changes, re-assess the interference model.

2. **AP client limit is 4:** K1 hard limit; cannot test >4 simultaneous clients.

3. **No STA mode:** K1 is AP-only (HARD architectural constraint per CLAUDE.md). Do not enable STA for testing.

4. **Trace buffer is finite:** At high network load (>20 WS msg/sec), trace buffer may fill. Increase TRACE_BUFFER_SIZE_BYTES or reduce instrumentation granularity.

5. **Render deadline is 2 ms:** Assuming 500 FPS target (8.33 ms per frame is false; 2 ms is per-zone rendering window on Core 1). If render architecture changes, deadline assumptions may not hold.

6. **Correlation ≠ Causation:** Correlation analysis can show that network activity and render latency co-vary, but cannot prove direct causation. Causation requires hypothesis-driven intervention (e.g., disabling WS dispatch or adding mutex instrumentation).

---

## 9. Success Criteria

Instrumentation is considered complete and validated when:

1. ✅ All Tier 4 instants emit correctly (WiFi AP, WS client, OTA events)
2. ✅ Tier 1 counters sample at 1 Hz without overhead
3. ✅ Test protocol captures clean traces (baseline + stress)
4. ✅ `analyse_trace.py --conditional` produces valid correlation report
5. ✅ Report shows either strong correlation (validates hypothesis) or weak correlation (re-direction to other vectors)
6. ✅ Tier 2 spans (if enabled) add <5% CPU overhead to WS dispatch

---

## 10. Files to Modify

| File | Changes | Priority |
|------|---------|----------|
| WiFiManager.cpp | Add TRACE_INSTANT for AP start/stop | P1 |
| WebServer.cpp (~454) | Add TRACE_INSTANT for client connect/disconnect | P1 |
| WsGateway.cpp | Add TRACE_INSTANT for WS client connect/disconnect; track dispatch count | P1 |
| WsOtaCommands.cpp | Add TRACE_INSTANT for OTA lifecycle events | P1 |
| main.cpp (health task) | Add TRACE_COUNTER for wifi_clients, ws_clients, ws_errors (1 Hz) | P2 |
| WsGateway.h | Extend Stats struct with dispatch count | P2 |
| WsEffectsCommands.cpp | Add TRACE_SPAN for handler (if FEATURE_TRACE_NETWORK) | P3 |
| WsParameterCommands.cpp | Add TRACE_SPAN for handler (if FEATURE_TRACE_NETWORK) | P3 |
| WsPaletteCommands.cpp | Add TRACE_SPAN for handler (if FEATURE_TRACE_NETWORK) | P3 |
| V1ApiRoutes.cpp | Add TRACE_SPAN for high-frequency REST routes (if FEATURE_TRACE_NETWORK) | P3 |

---

## 11. References

- **CLAUDE.md:** AsyncTCP Core Affinity, K1 AP-only constraint
- **render_frame_us metric:** Defined in TRACE_INSTRUMENTATION_SPEC Section 2 (Render Path)
- **analyse_trace.py:** Custom analysis tool (part of trace infrastructure)
- **WsCommandRouter.h/cpp:** Table-driven WS dispatch (~line 32-80)
- **WebServerContext.h:** Non-owning references to business systems

---

**Document Status:** APPROVED FOR IMPLEMENTATION (Phase 1: Tier 4 + Tier 1)
**Next Step:** Implement Phase 1; validate with 3-5 test captures; iterate on Tier 2 spans based on findings.
