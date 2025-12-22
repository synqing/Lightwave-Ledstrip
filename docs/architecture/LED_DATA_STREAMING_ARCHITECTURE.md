# LED Data Streaming Architecture

## Real-Time LED Visualization over WebSocket

**Document Version:** 2.0
**Date:** December 2024
**Status:** Implemented
**Branch:** main

---

## Executive Summary

This document specifies the implemented architecture for real-time LED data streaming from the ESP32-S3 firmware to the web dashboard. The system enables high-fidelity live preview of all 320 LEDs using efficient binary WebSocket frames.

### Key Implementation Details

| Aspect | Implementation | Rationale |
|--------|----------------|-----------|
| **Protocol** | Binary WebSocket | Minimal overhead, low latency |
| **Sample Rate** | 20 FPS | Smooth visual, constrained bandwidth |
| **Resolution** | Full (320 LEDs) | No visual artifacts from downsampling |
| **Frame Size** | 961 bytes | `[MAGIC_BYTE] + [RGB Data]` |
| **Bandwidth** | ~154 Kbps | < 1% of WiFi capacity |
| **Concurrency** | Thread-Safe Snapshot | Uses `RendererActor::getBufferCopy` |

---

## Table of Contents

1. [Implementation Status](#1-implementation-status)
2. [Protocol Specification](#2-protocol-specification)
3. [Firmware Architecture](#3-firmware-architecture)
4. [Client Implementation](#4-client-implementation)
5. [Performance Analysis](#5-performance-analysis)

---

## 1. Implementation Status

### 1.1 Firmware Components

| Component | Status | Details |
|-----------|--------|---------|
| WebSocket Server | ✅ Active | `AsyncWebSocket` on `/ws` |
| Streaming Logic | ✅ Implemented | `WebServer::broadcastLEDFrame` |
| Subscription | ✅ Implemented | `ledStream.subscribe` with client tracking |
| Thread Safety | ✅ Secured | Critical sections + `getBufferCopy` |

### 1.2 Capability
- **Max Subscribers:** 4 concurrent clients (configurable via `MAX_WS_CLIENTS`)
- **Rate Limit:** 20 FPS broadcast loop (separate from 120 FPS render loop)
- **Delivery:** Direct pointer-to-client binary send (avoids full broadcast overhead)

---

## 2. Protocol Specification

### 2.1 Subscription Model
Clients must explicitly subscribe to receive the binary stream.

**Subscribe:**
```json
{ "type": "ledStream.subscribe", "requestId": "..." }
```

**Unsubscribe:**
```json
{ "type": "ledStream.unsubscribe" }
```

### 2.2 Binary Frame Format
The server sends raw binary frames to subscribed clients.

**Structure:**
```
[MAGIC_BYTE (1 byte)] [RGB DATA (320 * 3 bytes)]
```

- **Magic Byte:** `0xFE` (254) - Used for simple frame validation.
- **RGB Data:** 960 bytes of raw color data.
  - Order: `R0, G0, B0, R1, G1, B1, ... R319, G319, B319`
  - Total Length: 1 + 960 = 961 bytes.

### 2.3 Rejection Policy
If the subscriber table is full (max 4), the server responds with:
```json
{
  "type": "ledStream.rejected",
  "success": false,
  "error": { "code": "RESOURCE_EXHAUSTED", "message": "Subscriber table full" }
}
```

---

## 3. Firmware Architecture

### 3.1 Data Flow
1. **Render Loop (Core 0/1):** `RendererActor` updates the internal `CRGB leds[320]` buffer at ~120 FPS.
2. **Web Task (Core 1):** `WebServer::broadcastLEDFrame()` is called periodically (every 50ms).
3. **Snapshot:** `RENDERER->getBufferCopy(localBuffer)` copies data safely using a mutex/semaphore.
4. **Formatting:** `localBuffer` is packed into `m_ledFrameBuffer` with the magic byte.
5. **Distribution:** The buffer is sent to each ID in `m_ledStreamSubscribers`.

### 3.2 Thread Safety
- **Subscriber List:** Protected by `portENTER_CRITICAL(&m_ledStreamMux)` during modification (subscribe/unsubscribe) and iteration (broadcast).
- **LED Data:** Protected by `RendererActor`'s internal locks during copy.

---

## 4. Client Implementation

### 4.1 Dashboard Logic
The React dashboard (`useLedStream` hook) handles the stream:
1. Connects to WebSocket.
2. Sends `ledStream.subscribe`.
3. Listens for binary messages.
4. Validates the first byte is `0xFE`.
5. Updates the canvas state with the remaining 960 bytes.

---

## 5. Performance Analysis

### 5.1 Bandwidth Usage
- **Frame Size:** 961 bytes
- **Rate:** 20 FPS
- **Per Client:** 19,220 bytes/sec (~19 KB/s)
- **Max Load (4 clients):** ~76 KB/s (~608 Kbps)
- **WiFi Impact:** Negligible (typical ESP32 throughput > 5 Mbps).

### 5.2 CPU Usage
- **Copy:** `memcpy` of 960 bytes is microsecond-scale.
- **Network Stack:** AsyncTCP handles transmission asynchronously.
- **Impact:** Profiling shows < 1% CPU usage increase when streaming to 1 client.
