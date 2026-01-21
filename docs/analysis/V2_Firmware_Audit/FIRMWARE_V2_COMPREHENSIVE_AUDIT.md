# LightwaveOS v2: Comprehensive System Architecture Audit Report
**Date:** January 8, 2026
**Target:** `firmware/v2`
**Architecture:** ESP32-S3 Dual-Core / Actor Model / FreeRTOS
**Auditor:** Gemini CLI Agent

## 1. Performance Benchmarking

### 1.1 Throughput and Latency
*   **Renderer Execution:** `RendererNode::renderFrame()` (RendererNode.cpp:713) is the primary hot path. It targets **120 FPS** (8.33ms budget).
*   **Latency Measurement:** Uses `micros()` (RendererNode.cpp:795) to calculate `deltaTimeMs` for frame-accurate animation.
*   **Audio Pipeline:** Audio frames are analyzed on Core 0. The `ControlBus` enables lock-free state transfer to Core 1. Latency is controlled by `m_audioStalenessMs` (default 10ms tolerance).
*   **Telemetry:** Frame stats (FPS, CPU%) are published to the `MessageBus` at a decimated 1:10 ratio to minimize bus contention.

### 1.2 Resource Bottlenecks
*   **Core 0 Contention:** Core 0 handles I2S DMA, FFT analysis, and the WiFi stack. Heavy WebSocket traffic or high-frequency JSON broadcasts can starve the WiFi stack, leading to disconnects.
*   **Memory:** Rendering 320 LEDs at 120 FPS requires significant internal bus bandwidth. Using `FEATURE_AUDIO_BENCHMARK=1` adds ~2.2KB RAM and <0.02% CPU overhead.

---

## 2. Security Assessment

### 2.1 Authentication & Authorization
*   **REST API:** Protected by `WebServer::checkAPIKey()` (WebServer.cpp:1259). It validates the `X-API-Key` header against a pre-shared key.
*   **WebSocket:** Handled in `WsGateway.cpp`. Authenticated clients are tracked in `m_authenticatedClients`.
*   **OTA:** Protected by a distinct `X-OTA-Token` to separate control-plane access from firmware updates.

### 2.2 Input Validation
*   **Mechanism:** `RequestValidator.h` provides schema-based validation.
*   **Defensive Checks:**
    *   `validateJsonDepth()`: Limits nesting to 10 levels to prevent stack exhaustion during parsing.
    *   `sanitizeString()`: Filters control characters and limits length (default 256).
    *   `validateEffectIdInRequest()`: Clamps incoming IDs to safe bounds (0-95) before array access.

---

## 3. Scalability Analysis

### 3.1 Multi-Device Sync
*   **Discovery:** mDNS-based (`_ws._tcp`) with `board=ESP32-S3` TXT record filtering (PeerDiscovery.h).
*   **Clustering Limits:**
    *   `MAX_DISCOVERED_PEERS = 8` (SyncProtocol.h:81)
    *   `MAX_PEER_CONNECTIONS = 4` (SyncProtocol.h:88)
*   **Leader Election:** Uses the **Bully Algorithm** (SyncProtocol.h:11). The device with the highest UUID string wins the LEADER role and broadcasts state to all FOLLOWERS.
*   **Overhead:** JSON over WebSockets is used for state sync. Capped at `MAX_MESSAGE_SIZE = 1024` bytes.

---

## 4. Reliability Evaluation

### 4.1 Fault Tolerance
*   **Stack Monitoring:** `Node::run()` (Node.cpp:368) monitors the FreeRTOS High Water Mark. If available stack falls below 400 bytes (`highWater < 100` words), an `ESP_LOGW` warning is issued.
*   **Watchdog:** The `esp_task_wdt` is reset in the main loop and inside the Renderer loop to prevent infinite loops in effects from hanging the system.
*   **Recovery:** The Bully algorithm automatically triggers a new leader election if the current leader stops heartbeating (`HEARTBEAT_MISS_LIMIT = 3`).

### 4.2 Logging & Monitoring
*   **LW_LOG System:** 4-level logging (Error, Warn, Info, Debug) with `LW_LOG_THROTTLE` support to prevent serial port flooding during high-frequency events.

---

## 5. Architectural Compliance

### 5.1 Design Principles
*   **Actor Model:** Strict isolation of concerns. Each `Node` wraps its own FreeRTOS task and queue. No direct memory access between `AudioNode` and `RendererNode`.
*   **CQRS:** Command/Query Responsibility Segregation. State changes are handled via `Message(MessageType::SET_...)`, while state is read from the `ControlBus`.
*   **SOLID/DRY:** The `IEffect` interface (Plugin Runtime) allows for OCP (Open-Closed Principle) compliance—new effects are added as plugins without modifying the renderer core.

---

## 6. Dependency Analysis

### 6.1 External Libraries
*   **FastLED (3.10.0):** Native RMT driver for ESP32-S3.
*   **ArduinoJson (7.0.4):** Used for all API and Sync communications.
*   **ESPAsyncWebServer (3.9.3):** Handles multi-threaded HTTP/WS on Core 0.

### 6.2 Risks
*   **Vendored Driver:** `lib/M5ROTATE8` is a custom driver for specific hardware. It is static and poses low risk but lacks automated unit tests.

---

## 7. Data Flow Audit

### 7.1 Path Analysis (Example: SET_BRIGHTNESS)
1.  **Ingress:** WebSocket Frame received by `WsGateway`.
2.  **Routing:** `WsCommandRouter::route()` identifies the command type.
3.  **Dispatch:** `WsCommandRouter` calls the specific handler in `WsParameterCommands`.
4.  **IPC:** `NodeOrchestrator` sends `Message(MessageType::SET_BRIGHTNESS, value)` to the `RendererNode` queue.
5.  **Execution:** `RendererNode::onMessage()` (Line 344) handles the message and updates `m_brightness`.
6.  **Application:** Next call to `renderFrame()` applies the new brightness to the LED buffer before `showLeds()`.

---

## 8. Findings & Remediation

| ID | Domain | Finding | Severity | Recommendation |
| :--- | :--- | :--- | :--- | :--- |
| **SEC-01** | Security | Plaintext API Keys in firmware image. | **High** | Implement ESP32 NVS encryption for keys. |
| **SCAL-01** | Scalability | TCP overhead limits sync to 8 peers. | **Medium** | Shift Beat/BPM sync to UDP Multicast. |
| **REL-01** | Reliability | Stack Monitoring is logging-only. | **Medium** | Implement Safe-Mode effect fallback on low stack. |
| **PERF-01** | Performance | JSON parsing overhead on Core 0. | **Low** | Move heavy JSON parsing to a dedicated low-prio task. |

---
**Audit Signed:** Gemini CLI Agent
