# tab5-encoder FSM Reference

9 state machines governing input, connectivity, UI feedback, and presets.

## 1. ClickDetector (4 states) -- `input/ClickDetector.h`

Detects single-click, double-click, long-hold. Outputs: SINGLE_CLICK, DOUBLE_CLICK, LONG_HOLD.

| State | Transition | Target | Emit |
|-------|-----------|--------|------|
| **IDLE** | justPressed | PRESSED | - |
| **PRESSED** | released >= 20ms | WAIT_FOR_DOUBLE | - |
| **PRESSED** | released < 20ms | IDLE | - |
| **PRESSED** | held >= 1000ms | COOLDOWN | LONG_HOLD |
| **WAIT_FOR_DOUBLE** | 2nd press <= 300ms | COOLDOWN | DOUBLE_CLICK |
| **WAIT_FOR_DOUBLE** | 2nd press > 300ms | PRESSED | SINGLE_CLICK |
| **WAIT_FOR_DOUBLE** | 300ms timeout | IDLE | SINGLE_CLICK |
| **COOLDOWN** | released + 100ms | IDLE | - |

## 2. WiFiConnectionStatus (6 states) -- `network/WiFiManager.h`

Non-blocking WiFi + mDNS resolution lifecycle.

| State | Transition | Target |
|-------|-----------|--------|
| **DISCONNECTED** | backoff timeout | CONNECTING |
| **CONNECTING** | WL_CONNECTED | CONNECTED |
| **CONNECTING** | failed | DISCONNECTED |
| **CONNECTED** | resolve mDNS | MDNS_RESOLVING |
| **CONNECTED** | WiFi lost | DISCONNECTED |
| **MDNS_RESOLVING** | success | MDNS_RESOLVED |
| **MDNS_RESOLVING** | timeout | CONNECTED |
| **ERROR** | 10s delay | DISCONNECTED |

## 3. WebSocketStatus (4 states) -- `network/WebSocketClient.h`

WebSocket client with exponential backoff (1s initial, 30s max).

| State | Transition | Target |
|-------|-----------|--------|
| **DISCONNECTED** | connect attempt | CONNECTING |
| **CONNECTING** | connected | CONNECTED |
| **CONNECTING** | failed | ERROR |
| **CONNECTED** | disconnected | DISCONNECTED |
| **ERROR** | backoff (1s..30s) | DISCONNECTED |

## 4. ConnectionState / LED Feedback (6 states) -- `ui/LedFeedback.h`

Drives M5ROTATE8 status LED (Unit A, LED 8). Breathing: 30-100% sine, 1500ms period.

| State | Color | Animation |
|-------|-------|-----------|
| **WIFI_DISCONNECTED** | Red | Solid |
| **WIFI_CONNECTING** | Blue | Breathing |
| **WIFI_CONNECTED** | Blue | Solid |
| **WS_CONNECTING** | Yellow | Breathing |
| **WS_CONNECTED** | Green | Solid |
| **WS_RECONNECTING** | Orange | Breathing |

## 5. SpeedPaletteMode (2-state toggle) -- `input/ButtonHandler.h`

Per-zone encoder function: **SPEED** (default) / **PALETTE**. Trigger: Enc1/3/5/7 button. 4 instances.

## 6. Zone Mode (2-state toggle) -- `input/ButtonHandler.h`

Global: **disabled** (default) / **enabled**. Trigger: Enc0 button (Unit B idx 8). Sends WS command.

## 7. PresetSlotState (5 states) -- `ui/widgets/PresetSlotWidget.h`

Per-slot lifecycle with save/delete visual feedback.

| State | Transition | Target |
|-------|-----------|--------|
| **EMPTY** | setOccupied | OCCUPIED |
| **OCCUPIED** | setActive | ACTIVE |
| **OCCUPIED** | cleared | EMPTY |
| **OCCUPIED** | save feedback | SAVING |
| **OCCUPIED** | delete feedback | DELETING |
| **ACTIVE** | deactivated | EMPTY |
| **SAVING** | 500ms timeout | ACTIVE |
| **DELETING** | 500ms timeout | EMPTY |

## 8. ConnectivityState (4 states) -- `ui/ConnectivityTab.h`

Network management UI screen state.

| State | Description |
|-------|-------------|
| **IDLE** | Default, showing network lists |
| **SCANNING** | WiFi scan in progress (15s timeout) |
| **CONNECTING** | Connecting to selected network |
| **ERROR** | Error with message display |

## 9. DiscoveryState (4 states) -- `network/HttpClient.h`

mDNS server discovery (FreeRTOS task, mutex-protected).

| State | Transition | Target |
|-------|-----------|--------|
| **IDLE** | startDiscovery() | RUNNING |
| **RUNNING** | mDNS resolved | SUCCESS |
| **RUNNING** | timeout/failure | FAILED |
| **SUCCESS** | (terminal) | - |
| **FAILED** | retry | IDLE |
