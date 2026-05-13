# LightwaveOS FSM Reference

> Agent quick-reference. Sources: statemachine-editor diagrams + codebase.

---

### 1. ConnectionState
**File:** `ViewModels/AppViewModel.swift:25`
**States:** `disconnected`, `discovering`, `connecting`, `connected`, `error(String)`

| From | To | Trigger |
|------|----|---------|
| disconnected | discovering | `startDiscovery` |
| disconnected | connecting | manual connect (IP entry) |
| discovering | disconnected | `stopDiscovery` |
| discovering | connecting | device selected |
| connecting | connected | REST + WS established |
| connecting | error | timeout |
| connected | disconnected | disconnect |
| connected | error | session error |
| error | disconnected | retry (auto) |
| error | connecting | user retries |

---

### 2. WebSocketService.Event
**File:** `Network/WebSocketService.swift:44`
**Variants:** `connected`, `disconnected(Error?)`, `status`, `beat`, `zoneUpdate`, `parameterUpdate`, `ledData`, `audioMetrics`

| Event | Payload | AppViewModel handler |
|-------|---------|---------------------|
| connected | -- | `wsConnected=true`, start subs + UDP health |
| disconnected | `Error?` | `wsConnected=false`, cancel streams, reset UDP |
| status | `WebSocketPayload` | update effect, palette, params, BPM |
| beat | `WebSocketPayload` | `audio.handleBeatEvent()` |
| zoneUpdate | `WebSocketPayload` | `zones.handleZoneUpdate()` |
| parameterUpdate | `WebSocketPayload` | `parameters.updateFromStatus()` |
| ledData | `Data` (960B) | write `ledData` array |
| audioMetrics | `AudioMetricsFrame` | `audio.handleMetricsFrame()` |

---

### 3. WebSocketMessageType (19 cases)
**File:** `Network/WebSocketService.swift:15`

| Case | Raw value |
|------|-----------|
| status | `"status"` |
| beatEvent | `"beat.event"` |
| zonesList | `"zones.list"` |
| zonesChanged | `"zones.changed"` |
| zonesStateChanged | `"zones.stateChanged"` |
| zonesEffectChanged | `"zones.effectChanged"` |
| zonesLayoutChanged | `"zones.layoutChanged"` |
| zoneEnabledChanged | `"zone.enabledChanged"` |
| zonesEnabledChanged | `"zones.enabledChanged"` |
| parametersChanged | `"parameters.changed"` |
| effectsChanged | `"effects.changed"` |
| effectsList | `"effects.list"` |
| palettesList | `"palettes.list"` |
| deviceStatus | `"device.status"` |
| colourCorrectionConfig | `"colorCorrection.getConfig"` |
| audioSubscribed | `"audio.subscribed"` |
| audioUnsubscribed | `"audio.unsubscribed"` |
| ledStreamSubscribed | `"ledStream.subscribed"` |
| unknown | (fallback) |

---

### 4. NWBrowser.State (Bonjour Discovery)
**File:** `Network/DeviceDiscoveryService.swift:100`
**States:** `ready`, `failed(NWError)`, `cancelled`

| From | To | Trigger |
|------|----|---------|
| (init) | ready | browser starts, Bonjour available |
| (init) | failed | network unavailable |
| ready | cancelled | `stopDiscovery()` |

---

### 5. NWConnection.State (Endpoint Resolution)
**File:** `Network/DeviceDiscoveryService.swift:206`
**States:** `ready`, `failed`, `cancelled`

| From | To | Trigger |
|------|----|---------|
| (init) | ready | TCP handshake OK -> extract device info |
| (init) | failed | connection refused / unreachable |
| (any) | cancelled | 5s timeout fires |

---

### 6. NWListener.State (UDP Receiver)
**File:** `Network/UDPStreamReceiver.swift:50`
**States:** `ready`, `failed(NWError)`, `cancelled`

| From | To | Trigger |
|------|----|---------|
| (init) | ready | UDP port bound |
| (init) | failed | port in use / permission denied |
| ready | cancelled | `stop()` |

---

### 7. UDP Fallback
**File:** `ViewModels/AppViewModel.swift:348`
**States:** `UDP_ACTIVE` -> `UDP_STALLED` -> `WS_FALLBACK`

| From | To | Trigger |
|------|----|---------|
| UDP_ACTIVE | UDP_STALLED | no frames for 3s |
| UDP_STALLED | WS_FALLBACK | health monitor calls `subscribeLEDStreamWS()` + `subscribeAudioStreamWS()` |

Resets to UDP_ACTIVE on WS reconnect or disconnect (`udpFallbackActive = false`).

---

### 8. AppTab -- `Views/ContentView.swift:16` -- `play`, `zones`, `audio`, `device` -- TabView selection binding.

### 9. ConnectionSheet -- `Views/ContentView.swift:14,77` -- `Bool` -- auto-shows on `disconnected`, auto-dismisses on `connected`.

---

### Cross-FSM Flow

```
Launch -> ConnectionSheet(auto)
  |-- NWBrowser.State -> ready -> browse results
  |-- device selected -> NWConnection.State -> ready -> extract IP
  `-- or manual IP entry
  v
ConnectionState: connecting -> REST + WS + UDP start
  v
ConnectionState: connected
  |-- WS Event stream active
  |-- UDP_ACTIVE -> (stall 3s) -> WS_FALLBACK
  `-- ConnectionSheet auto-dismiss
```
