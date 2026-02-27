# WiFi STA Fix Proposal

## Summary

Fix the state-agnostic event bit setting bug that causes false FAILED transitions when WiFi successfully reconnects after transient disconnects.

## Root Cause

The `STA_DISCONNECTED` event handler sets both `EVENT_DISCONNECTED` and `EVENT_CONNECTION_FAILED` unconditionally, regardless of current state. When WiFi auto-reconnects after a transient disconnect (AUTH_EXPIRE, ASSOC_LEAVE), the `EVENT_CONNECTION_FAILED` bit can cause `handleStateConnecting()` to transition to FAILED even though connection succeeded.

## Minimal Fix Set

### Fix 1: State-Aware Disconnect Handler

**Location**: `v2/src/network/WiFiManager.cpp:720-735`

Make the `STA_DISCONNECTED` handler check current state before setting bits:

```cpp
case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
    {
        WiFiState currentState = manager.getState();
        LW_LOGW("Event: Disconnected from AP (state: %s)", 
                manager.getStateString().c_str());
        
        if (manager.m_wifiEventGroup) {
            if (currentState == STATE_WIFI_CONNECTING) {
                // In CONNECTING state: this is a connection failure
                xEventGroupSetBits(manager.m_wifiEventGroup, EVENT_CONNECTION_FAILED);
            } else if (currentState == STATE_WIFI_CONNECTED) {
                // In CONNECTED state: this is a disconnection (will reconnect)
                xEventGroupSetBits(manager.m_wifiEventGroup, EVENT_DISCONNECTED);
            } else {
                // In other states: set both for safety, but log it
                LW_LOGD("Disconnect in state %s, setting both bits", 
                        manager.getStateString().c_str());
                xEventGroupSetBits(manager.m_wifiEventGroup,
                    EVENT_DISCONNECTED | EVENT_CONNECTION_FAILED);
            }
        }
    }
    break;
```

### Fix 2: Clear Stale Bits on State Entry

**Location**: `v2/src/network/WiFiManager.cpp:520-534`

Clear `EVENT_CONNECTION_FAILED` when entering CONNECTING state to prevent stale bits:

```cpp
void WiFiManager::setState(WiFiState newState) {
    if (xSemaphoreTake(m_stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Reset state-specific flags on entry to avoid persistence bugs
        if (newState == STATE_WIFI_SCANNING) {
            m_scanStarted = false;
        } else if (newState == STATE_WIFI_CONNECTING) {
            m_connectStarted = false;
            m_connectStartTime = 0;
            // Clear stale failure bits when starting new connection attempt
            if (m_wifiEventGroup) {
                xEventGroupClearBits(m_wifiEventGroup, EVENT_CONNECTION_FAILED);
            }
        }

        m_currentState = newState;
        xSemaphoreGive(m_stateMutex);
    }
}
```

### Fix 3: Defensive Check Before FAILED Transition

**Location**: `v2/src/network/WiFiManager.cpp:264-269`

Add defensive WiFi.status() check before transitioning to FAILED:

```cpp
} else if (bits & EVENT_CONNECTION_FAILED) {
    // Explicit failure event - but verify we're not actually connected
    // This handles the case where disconnect happened but WiFi auto-reconnected
    if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
        // We're actually connected! Clear the failure bit and transition to CONNECTED
        LW_LOGI("Connected! IP: %s (detected after disconnect event)", 
                WiFi.localIP().toString().c_str());
        if (m_wifiEventGroup) {
            xEventGroupClearBits(m_wifiEventGroup, EVENT_CONNECTION_FAILED);
            xEventGroupSetBits(m_wifiEventGroup, EVENT_GOT_IP);
        }
        m_connectStarted = false;
        m_successfulConnections++;
        m_lastConnectionTime = millis();
        m_reconnectDelay = RECONNECT_DELAY_MS;
        m_attemptsOnCurrentNetwork = 0;
        setState(STATE_WIFI_CONNECTED);
    } else {
        // Genuine failure
        m_connectStarted = false;
        m_connectionAttempts++;
        LW_LOGW("Connection failed (attempt %d)", m_connectionAttempts);
        setState(STATE_WIFI_FAILED);
    }
}
```

### Fix 4: Diagnostic Logging

**Location**: `v2/src/network/WiFiManager.cpp:244-250`

Add diagnostic logging to help debug future issues:

```cpp
// Wait for connection events with longer timeout to handle slow connections
EventBits_t bits = xEventGroupWaitBits(
    m_wifiEventGroup,
    EVENT_CONNECTED | EVENT_GOT_IP | EVENT_CONNECTION_FAILED,
    pdTRUE,   // Clear on exit
    pdFALSE,  // Wait for any bit
    pdMS_TO_TICKS(500)
);

// Diagnostic: log which bits were set
if (bits != 0) {
    LW_LOGD("Connection wait returned: bits=0x%02x, WiFi.status=%d, IP=%s",
            bits, WiFi.status(), WiFi.localIP().toString().c_str());
}
```

## Testing

After applying fixes, verify:
1. Connection succeeds after AUTH_EXPIRE disconnect
2. Connection succeeds after ASSOC_LEAVE disconnect
3. No false FAILED transitions when WiFi is actually connected
4. Network switching only happens when genuinely failed (not connected)

## Risk Assessment

**Low risk**: All fixes are defensive and preserve existing behavior for genuine failures. The state-aware disconnect handler is the most critical fix.

