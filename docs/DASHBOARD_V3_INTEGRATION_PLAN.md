# LightwaveOS Dashboard V3 Integration Plan

## Executive Summary

This document provides a comprehensive analysis of the V3 Dashboard design and a detailed integration plan for connecting it to the LightwaveOS ESP32-S3 firmware.

---

## 1. Design Analysis

### 1.1 Layout Evaluation

| Component | Rating | Notes |
|-----------|--------|-------|
| **Sidebar Navigation** | Excellent | Clean vertical nav with 7 tabs, status indicator, quick stats |
| **LED Visualization** | Excellent | CENTER ORIGIN compliant canvas with zone coloring |
| **Tab System** | Good | 7 distinct views: Composer, Effects, Palettes, Transitions, Performance, Network, Firmware |
| **Zone Mixer** | Good | 3-zone visualization with sync mode selector |
| **Preset Bar** | Good | P0-P4 quick access with save button and power toggle |

**Layout Strengths:**
- Consistent K1 branding with gold accent (#FFB84D)
- Glass morphism aesthetic with proper backdrop-filter
- Responsive grid system (12-column)
- Clear visual hierarchy with typography tokens

**Layout Concerns:**
- CDN dependencies (Tailwind, Iconify, Google Fonts) - need local bundling
- Fixed 3-zone mixer doesn't match firmware's 4-zone system
- Missing dual-strip (Strip 1/Strip 2) visualization

### 1.2 Functionality Evaluation

| Feature | V3 Dashboard | Firmware Support | Gap |
|---------|--------------|------------------|-----|
| Effect Selection | 50 effects hardcoded | 47 effects dynamic | Minor - sync list |
| Palette Selection | 33 palettes hardcoded | 33 palettes dynamic | None |
| Brightness Control | Slider 0-255 | REST + WS | None |
| Speed Control | Slider 1-50 | REST + WS | None |
| Zone Mixer | 3 zones (Bass/Mid/High) | 4 zones (configurable) | Major - redesign |
| Visual Pipeline | Intensity/Saturation/Complexity/Variation | Enhancement Engine | None |
| Transitions | 12 types | 12 types | None |
| Performance Monitor | FPS/CPU/Heap/Fragmentation | PerformanceMonitor | None |
| Network Settings | Scan/Connect | Partial | Add scan API |
| OTA Updates | File upload | Existing /update | None |
| Presets | P0-P4 static | Zone presets (5) | Map existing |

### 1.3 UX Evaluation

**Positives:**
- Smooth animations with `prefers-reduced-motion` support
- Accessible focus states with visible outlines
- Intuitive slider controls with live value display
- Collapsible sections (Visual Pipeline)
- Category filtering chips

**Issues to Address:**
- No loading states during API calls
- No error feedback UI (toast/snackbar)
- No connection lost recovery UI
- Static data - needs dynamic population
- No debouncing on slider inputs (firmware has 150ms debounce)

---

## 2. Integration Points Inventory

### 2.1 Existing Firmware API Endpoints

```
GET  /api/status           - System status, current effect, brightness, speed
GET  /api/effects          - Effect list with IDs and names
GET  /api/palettes         - Palette list with IDs and names
GET  /api/zone/status      - Zone configuration state
GET  /api/zone/presets     - Available zone presets
GET  /api/enhancement/status - Enhancement engine parameters

POST /api/effect           - Set effect by ID
POST /api/brightness       - Set brightness (0-255)
POST /api/speed            - Set speed (1-50)
POST /api/palette          - Set palette by ID
POST /api/pipeline         - Set enhancement parameters
POST /api/transitions      - Set transition settings
POST /api/zone/enable      - Enable/disable zone system
POST /api/zone/config      - Set zone effect/enabled
POST /api/zone/count       - Set zone count (1-4)
POST /api/zone/brightness  - Set per-zone brightness
POST /api/zone/speed       - Set per-zone speed
POST /api/zone/preset/load - Load zone preset
POST /api/zone/config/save - Save zone config to NVS
POST /api/zone/config/load - Load zone config from NVS
POST /update               - OTA firmware upload
```

### 2.2 WebSocket Message Types (Firmware → Dashboard)

```javascript
// Incoming messages (server → client)
{ type: "status", effectId, brightness, speed, paletteId, ... }
{ type: "effectChange", effectId, name }
{ type: "paletteChange", paletteId }
{ type: "zone.state", enabled, zoneCount, zones: [...] }
{ type: "performance", fps, freeHeap, fragmentation, cpuUsage }
{ type: "error", message }
```

### 2.3 WebSocket Commands (Dashboard → Firmware)

```javascript
// Outgoing commands (client → server)
{ type: "setEffect", effectId }
{ type: "nextEffect" }
{ type: "prevEffect" }
{ type: "setBrightness", value }
{ type: "setSpeed", value }
{ type: "setPalette", paletteId }
{ type: "setPipeline", intensity, saturation, complexity, variation }
{ type: "toggleTransitions", enabled }
{ type: "zone.enable", enable }
{ type: "zone.setCount", count }
{ type: "zone.setEffect", zoneId, effectId }
{ type: "zone.enableZone", zoneId, enabled }
{ type: "zone.loadPreset", presetId }
{ type: "zone.save" }
{ type: "zone.load" }
{ type: "zone.setBrightness", zoneId, brightness }
{ type: "zone.setSpeed", zoneId, speed }
{ type: "zone.setPalette", zoneId, paletteId }
```

### 2.4 Missing Endpoints (Required for V3)

```
GET  /api/network/scan     - Exists but needs enhancement for RSSI
GET  /api/performance      - New: detailed timing metrics
GET  /api/transitions      - New: current transition settings
POST /api/zone/palette     - New: per-zone palette (just added!)
```

---

## 3. Communication Protocol Specifications

### 3.1 Primary Protocol: WebSocket

**Endpoint:** `ws://{hostname}/ws`

**Rationale:** Real-time bidirectional communication for:
- Live LED visualization sync
- Instant slider feedback
- Zone state broadcasting
- Performance metrics streaming

**Connection Lifecycle:**
```
1. Dashboard opens WS connection
2. Server sends initial status + zone.state
3. Bidirectional command/response flow
4. Heartbeat every 30s (optional)
5. Auto-reconnect on disconnect (3s interval)
```

### 3.2 Secondary Protocol: REST API

**Base URL:** `/api`

**Rationale:** Used for:
- Initial data fetch (effects, palettes, presets)
- Stateless operations (OTA upload)
- Fallback when WS unavailable

**Standard Headers:**
```
Content-Type: application/json
Accept: application/json
```

### 3.3 Hybrid Approach

| Operation | Protocol | Reason |
|-----------|----------|--------|
| Initial load | REST | Bulk data fetch |
| Real-time control | WebSocket | Low latency |
| Parameter changes | WebSocket | Instant feedback |
| Status polling | WebSocket broadcast | Efficient |
| OTA upload | REST multipart | File transfer |
| Network scan | REST | One-shot operation |

---

## 4. Data Exchange Format Requirements

### 4.1 JSON Schema Definitions

#### Status Response
```json
{
  "effectId": 15,
  "effectName": "Aurora",
  "brightness": 128,
  "speed": 25,
  "paletteId": 2,
  "paletteName": "Ocean Breeze",
  "fps": 120,
  "freeHeap": 156000,
  "uptime": 16320,
  "wifiRssi": -42,
  "zoneEnabled": true,
  "zoneCount": 3
}
```

#### Effect List Response
```json
{
  "effects": [
    { "id": 0, "name": "Solid Color", "category": "classic" },
    { "id": 1, "name": "Fire", "category": "classic" },
    { "id": 5, "name": "Shockwave", "category": "lgp" }
  ]
}
```

#### Zone State Message
```json
{
  "type": "zone.state",
  "enabled": true,
  "zoneCount": 3,
  "zones": [
    { "id": 0, "enabled": true, "effectId": 2, "brightness": 255, "speed": 25, "paletteId": 0 },
    { "id": 1, "enabled": true, "effectId": 11, "brightness": 220, "speed": 30, "paletteId": 0 },
    { "id": 2, "enabled": true, "effectId": 12, "brightness": 180, "speed": 35, "paletteId": 0 }
  ]
}
```

#### Performance Metrics
```json
{
  "type": "performance",
  "fps": 120,
  "fpsMin": 118,
  "fpsMax": 122,
  "cpuUsage": 42,
  "freeHeap": 156420,
  "minFreeHeap": 85420,
  "fragmentation": 12,
  "effectProcessingUs": 1234,
  "fastledShowUs": 3200,
  "temperature": 42.5,
  "uptime": 16320
}
```

### 4.2 Error Response Format
```json
{
  "type": "error",
  "code": "INVALID_EFFECT_ID",
  "message": "Effect ID 99 out of range (0-46)"
}
```

---

## 5. Authentication and Security Measures

### 5.1 Current State

The firmware currently has **no authentication**. This is acceptable for:
- Local network only (192.168.x.x)
- Single-user device
- No sensitive data

### 5.2 Recommended Security Measures

#### Level 1: Basic (Implement Now)
- [ ] CORS restricted to same-origin
- [ ] Rate limiting on API endpoints (10 req/s)
- [ ] Input validation on all parameters
- [ ] WebSocket origin checking

#### Level 2: Optional (Future)
- [ ] Simple password protection (stored in NVS)
- [ ] Basic HTTP auth for /update endpoint
- [ ] HTTPS with self-signed cert (ESP32 support limited)

### 5.3 Input Validation Rules

| Parameter | Type | Range | Validation |
|-----------|------|-------|------------|
| effectId | uint8_t | 0-46 | < NUM_EFFECTS |
| brightness | uint8_t | 0-255 | Always valid |
| speed | uint8_t | 1-50 | Clamp to range |
| paletteId | uint8_t | 0-32 | < gGradientPaletteCount |
| zoneId | uint8_t | 0-3 | < MAX_ZONES |
| zoneCount | uint8_t | 1-4 | 1-4 inclusive |

---

## 6. Error Handling and Recovery Mechanisms

### 6.1 Connection States

```
CONNECTING → CONNECTED → DISCONNECTED
     ↓            ↓            ↓
  timeout     message      reconnect
     ↓         error          ↓
   retry         ↓         CONNECTING
              fallback
              to REST
```

### 6.2 Error Handling Strategy

#### Client-Side (Dashboard)
```javascript
const ErrorHandler = {
  // Connection errors
  onWsClose: () => {
    showToast('Connection lost. Reconnecting...', 'warning');
    scheduleReconnect(3000);
  },

  // API errors
  onApiError: (response) => {
    if (response.status === 400) {
      showToast(`Invalid request: ${response.message}`, 'error');
    } else if (response.status === 500) {
      showToast('Server error. Please restart device.', 'error');
    }
  },

  // Timeout handling
  onTimeout: (operation) => {
    showToast(`${operation} timed out. Retrying...`, 'warning');
    retryOperation(operation, 3);
  }
};
```

#### Server-Side (Firmware)
```cpp
// Validate before processing
if (effectId >= NUM_EFFECTS) {
    sendWsError(client, "INVALID_EFFECT_ID",
                "Effect ID out of range");
    return;
}

// Graceful degradation
if (!zoneComposer.isEnabled()) {
    // Still accept commands, queue for when enabled
    pendingZoneCommands.push(cmd);
}
```

### 6.3 Recovery Mechanisms

| Failure Mode | Detection | Recovery |
|--------------|-----------|----------|
| WS disconnect | onclose event | Auto-reconnect 3s |
| API timeout | 5s timeout | Retry 3x then fallback |
| Invalid data | JSON parse error | Ignore + log |
| Device reboot | WS reconnect | Re-fetch full state |
| OTA failure | Upload error | Keep previous firmware |

---

## 7. Performance Optimization Considerations

### 7.1 Dashboard Optimizations

#### Bundle Size
```
Current (CDN): ~500KB initial load
Target (Bundled): ~150KB gzipped

Actions:
- Bundle Tailwind (purge unused)
- Self-host fonts (subset)
- Replace Iconify with inline SVGs
- Minify JS/CSS
```

#### Rendering Performance
```javascript
// Throttle expensive operations
const throttledVisualize = throttle(updateVisualization, 16); // 60fps max

// Debounce user inputs
const debouncedSlider = debounce(sendSliderValue, 150);

// Use requestAnimationFrame for canvas
function drawLoop() {
  drawVisualization();
  requestAnimationFrame(drawLoop);
}
```

#### Memory Management
```javascript
// Limit WebSocket message queue
const MAX_QUEUE_SIZE = 100;
if (messageQueue.length > MAX_QUEUE_SIZE) {
  messageQueue.shift(); // Drop oldest
}

// Clean up event listeners
function cleanup() {
  ws.close();
  canvas.removeEventListener('resize', onResize);
}
```

### 7.2 Firmware Optimizations

#### WebSocket Efficiency
```cpp
// Batch broadcasts
void broadcastStatus() {
  if (ws->count() == 0) return; // Skip if no clients

  // Use StaticJsonDocument to avoid heap allocation
  StaticJsonDocument<512> doc;
  // ... build message
  ws->textAll(output);
}

// Throttle performance broadcasts
static uint32_t lastPerfBroadcast = 0;
if (millis() - lastPerfBroadcast > 1000) { // 1Hz max
  broadcastPerformance();
  lastPerfBroadcast = millis();
}
```

#### Memory Constraints
```
Available: 327KB RAM
Current usage: 62KB (19%)
Budget for WS: 10KB max per client
Max clients: 4 recommended
```

---

## 8. Integration Implementation Phases

### Phase 1: Core Integration (Week 1)

**Goal:** Basic functional connection

Tasks:
1. [ ] Replace hardcoded EFFECTS array with `/api/effects` fetch
2. [ ] Replace hardcoded PALETTES array with `/api/palettes` fetch
3. [ ] Implement WebSocket connection with reconnect
4. [ ] Wire brightness/speed sliders to API
5. [ ] Wire effect selection to API
6. [ ] Wire palette selection to API

Deliverable: Dashboard controls hardware in real-time

### Phase 2: Zone System (Week 2)

**Goal:** Full zone composer integration

Tasks:
1. [ ] Redesign Zone Mixer for 4-zone support
2. [ ] Add per-zone controls (effect, brightness, speed, palette)
3. [ ] Implement zone.state sync
4. [ ] Add zone presets UI
5. [ ] Add zone count selector (1-4)

Deliverable: Zone system fully controllable

### Phase 3: Advanced Features (Week 3)

**Goal:** Complete feature parity

Tasks:
1. [ ] Implement Visual Pipeline controls
2. [ ] Add transition selection and preview
3. [ ] Wire Performance Monitor with live data
4. [ ] Implement Network settings (scan, connect)
5. [ ] Wire OTA update with progress

Deliverable: All tabs functional

### Phase 4: Polish (Week 4)

**Goal:** Production-ready

Tasks:
1. [ ] Bundle assets (remove CDN dependencies)
2. [ ] Add loading states and error toasts
3. [ ] Implement LED visualization sync with firmware
4. [ ] Add preset save/load functionality
5. [ ] Comprehensive testing

Deliverable: Ready for deployment

---

## 9. Test Cases

### 9.1 Connection Tests

| ID | Test Case | Expected Result |
|----|-----------|-----------------|
| C1 | Initial WS connection | Receives status + zone.state |
| C2 | WS disconnect recovery | Reconnects within 5s |
| C3 | Multiple clients | All receive broadcasts |
| C4 | REST fallback | Works when WS unavailable |

### 9.2 Control Tests

| ID | Test Case | Expected Result |
|----|-----------|-----------------|
| T1 | Set brightness to 255 | LEDs at full brightness |
| T2 | Set brightness to 0 | LEDs off |
| T3 | Change effect | Smooth transition occurs |
| T4 | Change palette | Colors update immediately |
| T5 | Set speed to 1 | Animation very slow |
| T6 | Set speed to 50 | Animation very fast |

### 9.3 Zone Tests

| ID | Test Case | Expected Result |
|----|-----------|-----------------|
| Z1 | Enable zone system | Zones render independently |
| Z2 | Set zone count to 4 | 4 zones visible |
| Z3 | Set zone effect | Only that zone changes |
| Z4 | Set zone palette | Only that zone's colors change |
| Z5 | Load preset | All zone settings apply |
| Z6 | Save config | Persists across reboot |

### 9.4 Performance Tests

| ID | Test Case | Expected Result |
|----|-----------|-----------------|
| P1 | 120 FPS target | Measured FPS >= 118 |
| P2 | Heap usage | < 80% after 1 hour |
| P3 | WS latency | < 50ms round-trip |
| P4 | Page load time | < 2s on WiFi |

### 9.5 Error Tests

| ID | Test Case | Expected Result |
|----|-----------|-----------------|
| E1 | Invalid effect ID | Error toast shown |
| E2 | Device power cycle | Dashboard reconnects |
| E3 | OTA upload wrong file | Error message, no flash |
| E4 | Network disconnect | "Reconnecting" shown |

---

## 10. File Mapping

### Dashboard Files to Create/Modify

| File | Action | Purpose |
|------|--------|---------|
| `data/index.html` | Replace | New V3 dashboard |
| `data/app.js` | Replace | New JS with WS/REST |
| `data/styles.css` | Replace | Extracted & bundled CSS |
| `data/icons.svg` | Create | SVG sprite for icons |

### Firmware Files to Modify

| File | Changes |
|------|---------|
| `src/network/WebServer.cpp` | Add missing endpoints, enhance responses |
| `src/network/WebServer.h` | Add new function declarations |

---

## 11. Summary

The V3 Dashboard is a significant upgrade with professional design and comprehensive features. Integration requires:

1. **Protocol:** Hybrid WebSocket + REST
2. **Format:** JSON throughout
3. **Security:** Input validation, rate limiting
4. **Errors:** Client reconnect, server validation
5. **Performance:** Throttling, debouncing, bundling

**Estimated effort:** 4 weeks for full integration

**Risk areas:**
- Zone mixer redesign (3→4 zones)
- CDN dependency removal (bundling)
- LED visualization sync (canvas performance)

**Recommendation:** Proceed with phased implementation starting with core control integration.
