# Protected Files

> **When to read this:** Before modifying WiFiManager, WebServer, or any file that uses FreeRTOS synchronisation primitives.

## WiFiManager (`firmware/v2/src/network/WiFiManager.*`)

**Criticality: CRITICAL**

FreeRTOS EventGroup bits **persist across interrupted connections**. This has caused the "Connected! IP: 0.0.0.0" bug multiple times.

### Mandatory Rules

1. `setState()` MUST call `xEventGroupClearBits()` when entering `STATE_WIFI_CONNECTING`
2. Clear bits: `EVENT_CONNECTED | EVENT_GOT_IP | EVENT_CONNECTION_FAILED`
3. All state transitions MUST be mutex-protected via `m_stateMutex`
4. Never assume EventGroup bits are clean -- they persist

### Load-Bearing Fix (WiFiManager.cpp ~line 499)

```cpp
if (newState == STATE_WIFI_CONNECTING) {
    // CRITICAL FIX: Clear stale event bits
    if (m_wifiEventGroup) {
        xEventGroupClearBits(m_wifiEventGroup,
            EVENT_CONNECTED | EVENT_GOT_IP | EVENT_CONNECTION_FAILED);
    }
}
```

Removing or refactoring this code will reintroduce the 0.0.0.0 IP bug. This bug has resurfaced 3+ times.

### Verification

```bash
cd firmware/v2 && pio run -e esp32dev_audio -t upload
# Confirm: WiFi connects WITHOUT "IP: 0.0.0.0" appearing first
```

## WebServer (`firmware/v2/src/network/WebServer.*`)

**Criticality: HIGH**

- Rate limiting state must be thread-safe
- WebSocket handlers run on a different FreeRTOS task than HTTP handlers
- AsyncWebServer callbacks are NOT synchronised -- use mutexes

## Before Modifying Any Protected File

1. Read the ENTIRE file and understand the state machine
2. Read this document for known landmines
3. Run all related tests before AND after changes
4. Understand FreeRTOS synchronisation if the file uses mutexes/semaphores/event groups
