# LightwaveOS Web Dashboard PRD

**Product Requirements Document for External Dashboard Development**

**Version:** 1.0
**Date:** December 13, 2025
**Target Tool:** aura.build or similar UI generation platform

---

## Executive Summary

LightwaveOS requires an external web dashboard to control a dual-strip LED Light Guide Plate (LGP) device. The current embedded web interface works but is constrained by ESP32 SPIFFS storage (~60KB). An external dashboard can provide richer UX without these constraints.

**Key Differentiator:** The LED strips fire into an acrylic waveguide creating interference patterns - effects MUST originate from center (LED 79/80) and propagate outward/inward. This is not aesthetic preference but hardware physics.

---

## 1. Product Overview

### 1.1 What is LightwaveOS?

An ESP32-S3 based LED controller for:
- **Hardware:** Dual 160-LED WS2812 strips (320 total) mounted on Light Guide Plate edges
- **Effects:** 45+ visual effects with zone-based composition
- **Control:** Web interface + Serial commands (no physical buttons/encoders)
- **Connectivity:** WiFi with mDNS (`lightwaveos.local`)

### 1.2 Target Users

1. **Primary:** Home automation enthusiasts controlling ambient lighting
2. **Secondary:** Makers integrating LGP into projects
3. **Tertiary:** Developers extending the effect library

### 1.3 Key Constraints

| Constraint | Value | Reason |
|------------|-------|--------|
| Center Origin | LED 79/80 | LGP physics - edge effects look wrong |
| No Rainbows | Forbidden | Design decision |
| Embedded Storage | ~60KB | ESP32 SPIFFS limit |
| Target FPS | 120 | Smooth animations |

---

## 2. Current Functionality (What Exists)

### 2.1 Embedded Web Interface

**Location:** `data/` folder (index.html, app.js, styles.css)

**Current Features:**

| Feature | Description | Status |
|---------|-------------|--------|
| Status Bar | FPS, Memory, Effect name, Connection status | ✅ Working |
| Zone Composer | 1-4 zones, per-zone effect/brightness/speed | ✅ Working |
| Zone Mixer | Vertical faders for all zones | ✅ Working |
| LGP Visualization | Zone position reference (NOT real-time LED colors) | ✅ Working |
| Global Settings | Palette selection, transitions toggle | ✅ Working |
| Visual Pipeline | Intensity/Saturation/Complexity/Variation | ✅ Working |
| Enhancement Engines | ColorEngine, MotionEngine controls | ✅ Working |
| Firmware Update | OTA upload with progress | ✅ Working |
| Presets | P0-P4 load/save | ✅ Working |

### 2.2 API Surface

**REST Endpoints:** 25+ endpoints (see `docs/api/API.md`)
**WebSocket:** Real-time bidirectional communication

**Key Endpoints:**
```
GET  /api/status          - System state
GET  /api/effects         - Effect list (paginated)
GET  /api/palettes        - Palette list
POST /api/effect          - Set effect
POST /api/brightness      - Set brightness
POST /api/zone/count      - Set zone count
POST /api/zone/config     - Configure zone
POST /api/zone/brightness - Per-zone brightness
POST /api/zone/speed      - Per-zone speed
POST /update              - OTA firmware upload
```

### 2.3 Current Design System

**Theme:** PRISM.node Dark

```css
/* Backgrounds */
--canvas-bg: #1c2130;
--surface-bg: #252d3f;
--elevated-bg: #2f3849;

/* Text */
--text-primary: #e6e9ef;
--text-secondary: #b5bdca;
--text-tertiary: #8b92a0;

/* Zone Colors */
--zone-1-color: #6ee7f3;  /* Cyan - center */
--zone-2-color: #22dd88;  /* Green - middle */
--zone-3-color: #ffb84d;  /* Gold - outer */

/* Accent */
--accent-primary: #00d4ff;
```

**Typography:** Geist/Inter, weight 600 (semi-bold, NOT 700)

---

## 3. Requirements for External Dashboard

### 3.1 Must-Have Features (P0)

#### 3.1.1 Core Control Panel

| Feature | Description |
|---------|-------------|
| Effect Browser | Grid/list view of 45+ effects with search/filter |
| Effect Preview | Thumbnail or animation preview per effect |
| Zone Composer | Visual zone editor with drag boundaries |
| Global Controls | Brightness, Speed, Palette selection |
| Real-time Sync | WebSocket connection with reconnection logic |

#### 3.1.2 Zone System

| Feature | Description |
|---------|-------------|
| Zone Count | 1-4 zone selector |
| Zone Tabs | Switch between zones |
| Per-Zone Controls | Effect, Brightness, Speed per zone |
| Zone Visualization | LGP diagram showing zone boundaries |
| Zone Mixer | Fader-style multi-zone control |

#### 3.1.3 Status Display

| Feature | Description |
|---------|-------------|
| Connection Status | Online/Offline indicator |
| Performance | FPS, Free Heap memory |
| Current Effect | Active effect name |
| WiFi Signal | RSSI strength indicator |

### 3.2 Should-Have Features (P1)

#### 3.2.1 Effect Management

| Feature | Description |
|---------|-------------|
| Favorites | Star/save favorite effects |
| Recently Used | Track last N effects used |
| Effect Categories | Group by type (fire, water, physics, etc.) |
| Effect Parameters | Per-effect custom parameters (future) |

#### 3.2.2 Advanced Controls

| Feature | Description |
|---------|-------------|
| Visual Pipeline | Intensity, Saturation, Complexity, Variation |
| Enhancement Engines | ColorEngine, MotionEngine controls |
| Transition Settings | Type, duration, auto-cycle |

#### 3.2.3 Presets

| Feature | Description |
|---------|-------------|
| Preset Slots | 5+ saveable configurations |
| Preset Names | User-defined names |
| Quick Load | One-tap preset activation |
| Export/Import | JSON preset sharing |

### 3.3 Nice-to-Have Features (P2)

| Feature | Description |
|---------|-------------|
| Scenes | Time-based automation (morning, evening) |
| Schedules | Timer-based effect changes |
| Multi-Device | Control multiple LightwaveOS devices |
| Effect Creator | Simple effect parameter builder |
| Palette Editor | Custom palette creation |
| History | Undo/redo for parameter changes |
| Dark/Light Mode | Theme toggle (default: dark) |

---

## 4. Technical Requirements

### 4.1 API Integration

**Base URL:** `http://lightwaveos.local` or `http://<device-ip>`

**WebSocket:** `ws://lightwaveos.local/ws`

**Authentication:** None (local network only)

**CORS:** Enabled (`Access-Control-Allow-Origin: *`)

**Rate Limits:**
- REST: 10 req/sec max
- WebSocket: No limit

### 4.2 Data Flow

```
┌─────────────────┐       ┌─────────────────┐
│  External       │       │  ESP32          │
│  Dashboard      │◄─────►│  LightwaveOS    │
│  (Browser)      │  WS   │  WebServer      │
└─────────────────┘       └─────────────────┘
        │                         │
        │  REST API               │  LED Output
        │  /api/*                 │
        ▼                         ▼
   ┌─────────┐              ┌──────────┐
   │ Initial │              │ WS2812   │
   │  State  │              │ Strips   │
   │  Fetch  │              │ (320 LED)│
   └─────────┘              └──────────┘
```

### 4.3 State Management

**Initial Load:**
1. Fetch `/api/status` for current state
2. Fetch `/api/effects` for effect list
3. Fetch `/api/palettes` for palette list
4. Connect WebSocket for real-time updates

**Real-time Updates:**
- WebSocket broadcasts state changes
- Client updates UI without polling

**Debouncing:**
- Slider changes: 150ms debounce
- Pipeline changes: 250ms debounce

### 4.4 Error Handling

| Scenario | Behavior |
|----------|----------|
| WebSocket disconnect | Auto-reconnect every 3s, show offline status |
| REST API error | Show inline error, retry button |
| Invalid parameters | Clamp to valid range, warn user |
| Device unavailable | mDNS fallback to IP, discovery UI |

---

## 5. UX Requirements

### 5.1 Layout

**Desktop (1200px+):**
```
┌────────────────────────────────────────────────────┐
│  Status Bar (FPS, Memory, Effect, Connection)      │
├────────────────────────────────────────────────────┤
│                                                    │
│         LGP Visualization (Zone Reference)         │
│                                                    │
├─────────────────┬──────────────────────────────────┤
│  Zone Composer  │  Effect Browser                  │
│  - Zone Tabs    │  - Search/Filter                 │
│  - Effect       │  - Grid View                     │
│  - Brightness   │  - Categories                    │
│  - Speed        │                                  │
├─────────────────┴──────────────────────────────────┤
│  Zone Mixer (Faders for all zones)                 │
├────────────────────────────────────────────────────┤
│  Collapsible: Pipeline, Enhancements, Firmware     │
├────────────────────────────────────────────────────┤
│  Preset Bar (P0-P4, Save)                          │
└────────────────────────────────────────────────────┘
```

**Mobile (< 768px):**
- Single column layout
- Collapsible sections
- Bottom sheet for effect browser
- Sticky preset bar

### 5.2 Interactions

| Element | Interaction |
|---------|-------------|
| Effect Card | Click to apply, long-press for details |
| Zone Tab | Click to select, shows zone controls |
| Slider | Drag for live preview, release to apply |
| Toggle | Instant on/off |
| Preset Button | Click to load |
| Save Button | Click to save current config |

### 5.3 Animations

| Animation | Duration | Easing |
|-----------|----------|--------|
| Zone select | 200ms | ease-out |
| Collapsible expand | 300ms | ease-in-out |
| Status change | 150ms | ease |
| Effect apply | instant | - |

**Accessibility:** Respect `prefers-reduced-motion`

---

## 6. Visual Design Requirements

### 6.1 Theme

**Base:** Dark theme (PRISM.node)

**Rationale:**
- LED control is typically in low-light environments
- Reduces eye strain during extended use
- Matches K1.node1 reference implementation

### 6.2 Color System

```css
:root {
  /* Backgrounds */
  --bg-canvas: #1c2130;
  --bg-surface: #252d3f;
  --bg-elevated: #2f3849;

  /* Text */
  --text-primary: #e6e9ef;
  --text-secondary: #b5bdca;
  --text-muted: #8b92a0;

  /* Zones */
  --zone-1: #6ee7f3;  /* Cyan */
  --zone-2: #22dd88;  /* Green */
  --zone-3: #ffb84d;  /* Gold */
  --zone-4: #ff7eb3;  /* Pink */

  /* Accent */
  --accent: #00d4ff;
  --success: #22dd88;
  --error: #ff5555;
  --warning: #ffb84d;

  /* Borders */
  --border-subtle: #3a4254;
  --border-interactive: #4a5568;
}
```

### 6.3 Typography

| Element | Size | Weight | Tracking |
|---------|------|--------|----------|
| H1 (Page Title) | 32px | 600 | -0.025em |
| H2 (Section) | 24px | 600 | -0.015em |
| H3 (Card Title) | 18px | 600 | -0.01em |
| Body | 14px | 400 | normal |
| Label | 12px | 500 | 0.025em |
| Mono (Values) | 12px | 500 | normal |

**Font Stack:** `Geist, Inter, system-ui, sans-serif`
**Mono Stack:** `Space Mono, monospace`

### 6.4 Component Patterns

**Cards:**
- Background: `var(--bg-surface)`
- Border: 1px solid `var(--border-subtle)`
- Border-radius: 12px
- Padding: 16px (mobile), 24px (desktop)

**Buttons:**
- Primary: `var(--accent)` background, black text
- Secondary: `var(--bg-elevated)` background, secondary text
- Border-radius: 8px
- Height: 36px (mobile), 40px (desktop)

**Sliders:**
- Track: `var(--bg-elevated)`
- Fill: `var(--accent)`
- Thumb: white, 16px diameter
- Height: 6px

**Toggles:**
- Off: `var(--bg-elevated)`
- On: `var(--accent)`
- Thumb: white, 20px diameter

---

## 7. LGP Visualization Specification

### 7.1 Purpose

**The LGP visualization is for ZONE POSITION REFERENCE ONLY**

- NOT for real-time LED color rendering
- Shows zone boundaries with colored overlays
- Selected zone highlighted at 0.5 opacity
- Unselected zones at 0.1 opacity or hidden

### 7.2 Aspect Ratio

**Container:** `aspect-[32/6]` (5.33:1 ultra-wide)
**Max Width:** 960px (`max-w-[60rem]`)

### 7.3 Zone Layout (Default 3-Zone)

```
┌──────┬─────────────────┬────────┬─────────────────┬──────┐
│  Z3  │       Z2        │   Z1   │       Z2        │  Z3  │
│ LEFT │      LEFT       │ CENTER │      RIGHT      │ RIGHT│
│12.5% │     28.125%     │ 18.75% │     28.125%     │12.5% │
│ 0-19 │     20-64       │ 65-94  │     95-139      │140-59│
└──────┴─────────────────┴────────┴─────────────────┴──────┘
        ◄────────────────── LED 79/80 (CENTER) ──────────────────►
```

### 7.4 Zone Colors

| Zone | Color | Hex | Use |
|------|-------|-----|-----|
| Zone 1 | Cyan | #6ee7f3 | Center (continuous) |
| Zone 2 | Green | #22dd88 | Middle (split L+R) |
| Zone 3 | Gold | #ffb84d | Outer (split L+R) |
| Zone 4 | Pink | #ff7eb3 | Extended (if 4 zones) |

---

## 8. API Reference Summary

### 8.1 Core Endpoints

```javascript
// Status
GET  /api/status

// Effects
GET  /api/effects?start=0&count=100
POST /api/effect { effect: number }

// Palettes
GET  /api/palettes
POST /api/palette { palette: number }

// Parameters
POST /api/brightness { brightness: 0-255 }
POST /api/speed { speed: 1-50 }
POST /api/pipeline { intensity, saturation, complexity, variation }

// Transitions
POST /api/transitions { enabled: boolean }
```

### 8.2 Zone Endpoints

```javascript
// Zone System
GET  /api/zone/status
POST /api/zone/count { count: 1-4 }
POST /api/zone/config { zoneId, effectId, enabled }
POST /api/zone/brightness { zoneId, brightness: 0-255 }
POST /api/zone/speed { zoneId, speed: 1-50 }

// Presets
GET  /api/zone/presets
POST /api/zone/preset/load { presetId: 0-4 }
POST /api/zone/config/save
```

### 8.3 WebSocket Messages

```javascript
// Commands (send)
{ type: "setEffect", effectId: number }
{ type: "setBrightness", value: number }
{ type: "setPalette", paletteId: number }
{ type: "zone.setCount", count: number }
{ type: "zone.setEffect", zoneId: number, effectId: number }
{ type: "zone.setBrightness", zoneId: number, brightness: number }

// Status (receive)
{ type: "status", effect, brightness, palette, zoneEnabled, zoneCount }
{ type: "performance", fps, freeHeap }
{ type: "effectChange", effectId, name }
```

---

## 9. Effect Catalog

### 9.1 Effect Categories

| Category | Count | Examples |
|----------|-------|----------|
| Fire/Heat | 5 | Fire, Lava, Ember |
| Water/Ocean | 4 | Ocean, Waves, Ripple |
| Nature | 3 | Forest, Aurora |
| Physics | 8 | LGP Interference, Chromatic Aberration |
| Patterns | 10 | Pulse, Breathe, Chase |
| Color | 6 | Solid, Gradient, Blend |
| Special | 9 | Sparkle, Twinkle, Meteor |

### 9.2 Effect Types

- `standard` - Basic effects
- `zone` - Zone-aware effects
- `physics` - LGP physics-based
- `reactive` - Future audio-reactive

---

## 10. Future Considerations

### 10.1 Audio Sync (Not Implemented)

The firmware has audio effect infrastructure but no microphone hardware. Future dashboard may include:
- Audio level meters
- Beat detection visualization
- Audio-reactive effect controls

### 10.2 Scene Automation

Future features:
- Time-based scene triggers
- Sensor integration (motion, ambient light)
- Home automation integration (MQTT, Home Assistant)

### 10.3 Multi-Device

Future support for:
- Device discovery
- Grouped control
- Synchronized effects

---

## 11. Handoff Checklist

Before generating UI with aura.build or similar:

- [ ] Review this PRD completely
- [ ] Reference `docs/api/API.md` for complete API details
- [ ] Study `data/index.html` for current implementation patterns
- [ ] Understand zone system (center-origin, symmetric)
- [ ] Use PRISM.node dark theme colors
- [ ] Use Geist/Inter font (weight 600, NOT 700)
- [ ] Implement WebSocket reconnection logic
- [ ] Add debouncing for sliders (150ms)
- [ ] Test with real device at `http://lightwaveos.local`

---

## 12. Appendix

### A. Reference Files

| File | Purpose |
|------|---------|
| `docs/api/API.md` | Complete API reference |
| `data/index.html` | Current embedded UI |
| `data/app.js` | Current JS implementation |
| `data/styles.css` | Current CSS styles |
| `src/network/WebServer.cpp` | Server implementation |
| `src/effects/zones/ZoneComposer.h` | Zone system |

### B. Test Device

- **mDNS:** `lightwaveos.local`
- **Fallback IP:** Check serial output or router DHCP
- **Serial:** 115200 baud for debug output

### C. Related Documentation

- `CLAUDE.md` - Project instructions
- `ARCHITECTURE.md` - System architecture
- `Master_Archive/design-artifacts/AURA_BUILD_PROMPT.md` - Previous Aura prompt (archived)
