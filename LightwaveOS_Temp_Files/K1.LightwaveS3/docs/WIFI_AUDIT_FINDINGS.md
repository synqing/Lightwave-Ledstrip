# WiFi STA Comprehensive Audit Findings

## Critical Bug Identified

### Root Cause: State-Agnostic Event Bit Setting

**Location**: `v2/src/network/WiFiManager.cpp:726-735`

The `STA_DISCONNECTED` event handler sets **both** `EVENT_DISCONNECTED` and `EVENT_CONNECTION_FAILED` unconditionally, regardless of current state:

```cpp
case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
    xEventGroupSetBits(manager.m_wifiEventGroup,
        EVENT_DISCONNECTED | EVENT_CONNECTION_FAILED);
```

### Failure Scenario (from logs)

1. **State: CONNECTING** - WiFi stack attempts connection
2. **Event: STA_DISCONNECTED (AUTH_EXPIRE)** → Sets `EVENT_CONNECTION_FAILED`
3. **WiFi stack auto-reconnects** (WiFi.setAutoReconnect(true))
4. **Event: STA_CONNECTED** → Sets `EVENT_CONNECTED`
5. **Event: STA_GOT_IP** → Sets `EVENT_GOT_IP`
6. **handleStateConnecting() wait returns** with `EVENT_CONNECTION_FAILED` (if it was set first)
7. **Code checks**: `if (bits & EVENT_GOT_IP)` → **FALSE** (because wait returned CONNECTION_FAILED first)
8. **Code checks**: `else if (bits & EVENT_CONNECTION_FAILED)` → **TRUE**
9. **Result**: Transitions to `STATE_WIFI_FAILED` even though connection succeeded!

### Additional Issues

1. **Event bit priority**: `xEventGroupWaitBits` with `pdFALSE` (wait for any) returns ALL bits that are set, but the order of checks matters. If both `EVENT_CONNECTION_FAILED` and `EVENT_GOT_IP` are set, the code should prioritize `EVENT_GOT_IP`.

2. **No state check in event handler**: The `STA_DISCONNECTED` handler doesn't check current state before setting bits. It should:
   - If state is CONNECTING: set `EVENT_CONNECTION_FAILED` only
   - If state is CONNECTED: set `EVENT_DISCONNECTED` only
   - If state is other: discard or handle appropriately

3. **Stale bit risk**: If `EVENT_CONNECTION_FAILED` is set while in CONNECTING, then WiFi auto-reconnects successfully, the bit might still be present when we check, causing false failure.

## State Machine Flow Analysis

### Paths to FAILED/INIT while connected

1. **handleStateConnecting() line 264-269**: If `EVENT_CONNECTION_FAILED` bit is set (even if connection succeeded), transitions to FAILED
2. **handleStateConnecting() line 270-292**: Timeout path - but has defensive check for `WiFi.status() == WL_CONNECTED`
3. **handleStateFailed() line 346**: After network switch, transitions to INIT
4. **handleStateFailed() line 366**: After backoff, transitions to INIT
5. **handleStateDisconnected() line 401**: After disconnect, transitions to INIT

### Network Switching Logic

**Location**: `v2/src/network/WiFiManager.cpp:583-602`

The `switchToNextNetwork()` function does NOT check if we're currently connected before switching. This could cause:
- Switching networks while connected to primary
- Clearing cached channel info unnecessarily
- Triggering reconnection when already connected

## Event Group Semantics

### Current Implementation

- `EVENT_CONNECTED`: Set by `STA_CONNECTED` event
- `EVENT_GOT_IP`: Set by `STA_GOT_IP` event
- `EVENT_DISCONNECTED`: Set by `STA_DISCONNECTED` event (unconditionally)
- `EVENT_CONNECTION_FAILED`: Set by `STA_DISCONNECTED` event (unconditionally)

### Problem

Both `EVENT_DISCONNECTED` and `EVENT_CONNECTION_FAILED` are set for EVERY disconnect, regardless of state. This causes:
- CONNECTING state to see failure when it should see "disconnected, will reconnect"
- CONNECTED state to see failure when it should see "disconnected, will reconnect"
- Stale failure bits persisting across state transitions

## Build Configuration (esp32dev_audio)

**Location**: `v2/platformio.ini:60-74`

- WiFi credentials: OPTUS_738CC0N (primary), VX220-013F (secondary)
- `LW_LOG_LEVEL=4` (debug level)
- `FEATURE_EFFECT_VALIDATION=0`
- No WiFi-specific build flags that would affect timing

## Task Scheduling Analysis

**WiFiManager Task** (`v2/src/network/WiFiManager.h:307-309`):
- Core: 0 (same as WiFi stack)
- Priority: 1 (low)
- Stack: 4096 bytes

**AudioActor Task** (`v2/src/audio/AudioActor.h:11`):
- Core: 0 (same as WiFiManager)
- Priority: 4 (higher than WiFiManager)
- Tick interval: 16ms

**Analysis**:
- AudioActor has **higher priority** than WiFiManager, so it can preempt WiFiManager
- Both run on Core 0, which is correct (WiFi stack requires Core 0)
- WiFiManager's `xEventGroupWaitBits` calls are blocking, so they will yield to higher-priority tasks
- **No scheduling issue identified**: The blocking waits should work correctly even with audio preemption

**Conclusion**: Task scheduling is not the root cause. The bug is in event bit handling logic.

## EventGroup Bit Clearing Semantics

### Current Behavior

All `xEventGroupWaitBits` calls use `pdTRUE` (clear on exit), which clears **all bits in the wait mask** that are currently set, not just the ones that caused the wait to return.

**Example**:
- Wait mask: `EVENT_CONNECTED | EVENT_GOT_IP | EVENT_CONNECTION_FAILED`
- Bits set: `EVENT_CONNECTION_FAILED | EVENT_GOT_IP`
- Wait returns: Both bits (because `pdFALSE` = wait for any)
- Bits cleared: Both `EVENT_CONNECTION_FAILED` and `EVENT_GOT_IP`

### Stale Bit Risk

**Scenario**: 
1. In CONNECTING state
2. `STA_DISCONNECTED` event sets `EVENT_CONNECTION_FAILED`
3. WiFi auto-reconnects, `STA_GOT_IP` sets `EVENT_GOT_IP`
4. `handleStateConnecting()` wait returns with both bits
5. Code checks `if (bits & EVENT_GOT_IP)` → should be TRUE
6. But if the wait already cleared both bits, we should still see EVENT_GOT_IP in the return value

**Actually, the wait clears bits AFTER returning**, so the return value should contain both bits. The issue is the **order of checks** in the code.

### Bit Priority Issue

**Location**: `v2/src/network/WiFiManager.cpp:254-269`

The code checks `EVENT_GOT_IP` first, then `EVENT_CONNECTION_FAILED`. This is correct, BUT:

If `xEventGroupWaitBits` returns with BOTH bits set, the code will:
1. Check `if (bits & EVENT_GOT_IP)` → TRUE → transition to CONNECTED ✅
2. Never reach `else if (bits & EVENT_CONNECTION_FAILED)`

**However**, there's a subtle bug: if the wait times out (returns 0), and EVENT_CONNECTION_FAILED was set but EVENT_GOT_IP wasn't set yet, we'll go to the timeout path. The timeout path has a defensive check, but it only runs if the wait returned 0.

**The real bug**: If EVENT_CONNECTION_FAILED is set, and we wait, and EVENT_GOT_IP arrives AFTER the wait returns (but before we check), we won't see EVENT_GOT_IP in the return value, and we'll transition to FAILED.

## Recommendations

1. **Make STA_DISCONNECTED handler state-aware**: Check current state before setting bits
   - If state is CONNECTING: set `EVENT_CONNECTION_FAILED` only
   - If state is CONNECTED: set `EVENT_DISCONNECTED` only
   - If state is other: discard or handle appropriately

2. **Prioritize EVENT_GOT_IP in handleStateConnecting()**: Check for GOT_IP first, even if CONNECTION_FAILED is also set (already done, but verify)

3. **Clear stale bits on state entry**: When entering CONNECTING, clear `EVENT_CONNECTION_FAILED` to prevent false failures

4. **Add defensive WiFi.status() check**: Before transitioning to FAILED, verify we're not actually connected (already done in timeout path, but add to CONNECTION_FAILED path too)

5. **Guard network switching**: Don't switch networks if currently connected with valid IP

6. **Add diagnostic logging**: Log which bits are set when wait returns, and current WiFi.status()

