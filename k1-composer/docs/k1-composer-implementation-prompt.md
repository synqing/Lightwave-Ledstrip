# K1 Lightwave Composer — Implementation Prompt

> **Purpose**: This document is a one-shot instruction set for an implementing agent to merge two existing versions of the K1 Composer Dashboard, fix identified flaws, harden the codebase, and lay the architectural foundation for AI-native integration. Read this document in its entirety before writing any code.

---

## TABLE OF CONTENTS

1. [Project Context & Hardware Reality](#1-project-context--hardware-reality)
2. [What You Are Building](#2-what-you-are-building)
3. [Source Material — Two Versions](#3-source-material--two-versions)
4. [Merge Strategy — What Comes From Where](#4-merge-strategy--what-comes-from-where)
5. [Identified Flaws — Must Fix](#5-identified-flaws--must-fix)
6. [Hardening Requirements](#6-hardening-requirements)
7. [Design System & Visual Language](#7-design-system--visual-language)
8. [JavaScript Architecture Contract](#8-javascript-architecture-contract)
9. [AI Integration Foundation](#9-ai-integration-foundation)
10. [Acceptance Criteria](#10-acceptance-criteria)

---

## 1. PROJECT CONTEXT & HARDWARE REALITY

### What is K1-Lightwave?

K1-Lightwave is a music visualization hardware device. It is NOT a screen-based visualizer and it is NOT a standard LED strip product. It uses a **dual edge-lit light guide plate (LGP) stack** — two sheets of laser-engraved acrylic, each illuminated from one edge by a strip of 160 WS2812B LEDs (320 LEDs total across two strips). Light enters the acrylic edge and is scattered by the laser-etched extraction pattern across the plate surface, creating diffused, volumetric glow effects impossible with direct-view LED strips.

This distinction matters for everything you build. The visual preview, the parameter model, the AI context — all must reflect this optical reality.

### Hardware Specifications

```
MCU:              ESP32-S3, dual-core Xtensa LX7 @ 240MHz
Memory:           512KB SRAM (internal), 8MB PSRAM (external, cached via MMU)
LEDs:             320 × WS2812B (2 strips × 160), driven in parallel via RMT/I2S DMA
LED data output:  ~5.1ms (parallel), ~9.6ms (serial fallback)
Animation budget: ≤3ms per frame on Core 1
Audio budget:     ≤4ms per frame on Core 0
Target FPS:       120+ (firmware achieves 108+ currently)
Audio input:      SPH0645 I2S MEMS microphone
Audio pipeline:   96 musically-spaced Goertzel bins → chromagram → beat/tempo detect
Latency:          Sub-8ms audio-to-visual (measured end-to-end)
Connectivity:     WiFi (STA mode), WebSocket + REST API
```

### LGP Optical Model (Critical for AI + Preview)

Each LED does NOT illuminate a single pixel. Through the acrylic, each LED influences a zone approximately **20–40mm wide** on the plate surface. Light intensity follows approximate inverse-square falloff from the LED injection point. Adjacent LED colors blend within ~15mm overlap zones. The first ~5mm from the LED edge is a bright "hot zone." Beyond ~50mm, contribution is negligible. With 160 LEDs on one edge, the effective visual resolution is roughly **20–30 perceptual zones**, not 160 discrete pixels.

**Implications for the dashboard preview and AI code generation:**
- Sharp per-pixel patterns that look great on direct-view strips appear as **muddy smears** through an LGP
- Optimal LGP patterns are: smooth gradients, color washes, broad pulses, slow zone transitions
- The preview must visualize the LGP blending/diffusion, not show raw per-pixel colors
- AI-generated effects must prefer `fill_gradient()` over per-pixel assignments
- The code and UI should think in terms of "visual zones" not "pixel indices"

### The Visual Pipeline

LightwaveOS processes frames through a six-stage directed acyclic graph:

```
Input → Mapping → Modulation → Render → PostFX → Output
```

| Stage      | What it does                                             | Key parameters              |
|------------|----------------------------------------------------------|-----------------------------|
| Input      | Captures audio via I2S MEMS mic, reads user parameters   | bufferSize, sampleRate      |
| Mapping    | 96 Goertzel bins → frequency bands, chromagram, key      | mapper config, band weights |
| Modulation | Time-based transforms, LFOs, beat-sync                   | speed, intensity, phase     |
| Render     | Maps modulated values to LED buffer (CRGB[320])          | hue, saturation, brightness |
| PostFX     | Gamma correction, temporal smoothing, dithering           | gamma, decay, blur          |
| Output     | Pushes buffer to LEDs via RMT/I2S DMA                   | (hardware-locked)           |

This pipeline is the mental model the entire Composer UI is built around. The code visualiser shows a TypeScript representation of it. The timeline scrubs through its phases. The inspector exposes parameters from each stage. The AI assistant will reason about it. **Everything connects to this pipeline.**

### Communication Protocol

The ESP32 exposes:
- **WebSocket** at `ws://{host}/ws` — bidirectional JSON messages with request/response pattern using `requestId` correlation
- **REST API** at `http://{host}/api/v1/...` — for parameter mutations, status queries
- **Control Lease System** — exclusive-access mutex preventing concurrent parameter writes from multiple clients. Uses acquire → heartbeat → release lifecycle with automatic expiry on missed heartbeats.

Message types the dashboard handles:
- `control.acquire` / `control.release` / `control.heartbeat` / `control.status` — lease management
- `control.stateChanged` — unsolicited lease state change (e.g., another client took control)
- `status` — periodic device status frames with audio/visual state
- `error` — error responses, including `CONTROL_LOCKED`

---

## 2. WHAT YOU ARE BUILDING

A single self-contained HTML file (`k1-composer.html`) that is the **K1 Lightwave Composer Dashboard**. It must:

1. Use **v3's visual shell** (CSS, layout, component styling) as the presentation layer
2. Restore **v1's complete JavaScript** (WebSocket, lease management, transport, tuner, telemetry, health monitoring) as the interaction layer
3. Fix all identified flaws from both versions
4. Add the **AI assistant panel** as a new grid section with the structural HTML and CSS ready for future JS wiring
5. Produce a single HTML file with inlined CSS and JS (no external dependencies beyond CDN fonts, Tailwind, and Iconify)

---

## 3. SOURCE MATERIAL — TWO VERSIONS

### v1 (`k1-composer.html` — the original)

**What it has that v3 lacks:**
- ~700 lines of working JavaScript in a `<script type="module">` block
- Complete `state` object with: connection state, lease state, transport state, health tracking, event buffer, alert tracking
- `ui` object mapping all DOM IDs to element references
- Full WebSocket lifecycle: `connect()`, `cleanupWs()`, `sendWsCommand()`, `handleUnsolicitedMessage()`
- Lease management: `acquireLease()`, `releaseLease()`, `startHeartbeat()`, `stopHeartbeat()`, `refreshLeaseStatus()`, `updateLeaseFromPayload()`
- Transport controls: `playTimeline()`, `pauseTimeline()`, `resetTimeline()`, `transportTick()` (requestAnimationFrame loop)
- Parameter tuner: `createTuner()` builds inspector sections from `paramConfig` array with grouped sliders, collapse/expand, live value display
- Health monitoring: `computeAndRenderHealth()` runs on 500ms interval, tracks message cadence, jitter (stddev), queue depth, drop ratios at 10s/60s windows. `recordMessageTiming()` on every WS message. `updateKPI()` updates tile bar widths and colors based on thresholds.
- Alert system: `pushAlert()`, `processAlertFromEvent()`, `renderAlerts()` — auto-generates alerts from event patterns and health threshold breaches
- Telemetry logger: `logEvent()` renders chronological feed with severity pills, timestamps, sanitized payloads
- Code visualiser: `renderCode()` generates syntax-highlighted pipeline code, `highlightCodePhase()` highlights active lines based on pipeline phase
- LED preview: `paintStrip()` renders 64-LED strips with HSL color calculation, `renderCentreOriginVis()` renders centre-origin visualization
- Security: `sanitiseTelemetry()` redacts sensitive keys (leaseToken, authorization, apiKey) from log output
- Utility: `toast()`, `stddev()`, `makeReqId()`, `canMutate()`, `updateControls()`

**What's wrong with v1's visual design:**
- No LGP viewport mock (strips show as flat bars only)
- No window chrome row (red/yellow/green dots)
- Simpler topbar layout without the "tool shell" aesthetic
- Code panel uses a plain header, not the tabbed console strip
- Telemetry log has no static seed rows (starts empty)
- Timeline has no colored phase segments behind the scrubber
- Font sizes slightly larger throughout (0.75rem vs 0.6875rem headers)
- No `code-line.active-line` CSS class (v1 uses inline style application)
- Larger spacing/padding throughout (12px vs 10px, 24px vs 16px gaps)
- Inspector sections use flex layout, not CSS Grid for rows
- v1 uses `.slider-grid` class that v3 drops
- Topbar glass has a `::after` gradient pseudo-element that v3 removes
- No viewport shell component at all
- Missing v3's refined timeline track with colored phase segments

### v3 (`k1-composer-session3.html` — the visual polish)

**What it has that v1 lacks:**
- Window chrome row with traffic light dots + file path + firmware version badge
- LGP viewport mock with perspective-transformed 3D visualization:
  - `.viewport-shell`, `.viewport-top`, `.viewport-canvas`, `.viewport-grid`
  - `.lgp-mock` with perspective transform, `.lgp-plate` with gradient + pseudo-element glow
  - `.lgp-edge-left` / `.lgp-edge-right` with colored box-shadows (purple left, blue right)
  - `.lgp-glow` radial gradient background
  - HUD badges ("LGP", "Preview"), axis indicator, bottom bar with hints
  - Move/Rotate/Scale toolbar buttons (decorative)
- Console-style header strip for the code panel with tab UI (`.console-header-strip`, `.console-tab`)
- Tabular-nums on timestamps, tighter feed row grid layout
- Timeline with colored phase segments behind the range input (`.timeline-track-bg`, `.timeline-segment.seg-*`)
- Telemetry section with pre-seeded static log rows including inline SVG sparkline
- Inspector panel header with collapse-all and options buttons
- Inspector rows using CSS Grid (`grid-template-columns: minmax(72px,auto) 1fr minmax(30px,auto)`) instead of flex
- Section labels as a reusable component class (`.section-label`)
- Refined badge styling with explicit border colors on severity variants
- Tighter spacing rhythm: `--radius: 10px` (was 12px), panels use 8px/13px padding (was 10px/14px)
- Font sizes reduced ~0.5–1px across most elements
- Topbar glass uses simpler background without `::after` pseudo-element
- Code panel background changed from `var(--bg)` to hardcoded `#090a0d`
- Telemetry feed pill borders with explicit rgba border colors
- `prefers-reduced-motion` media query (also in v1)
- `btn:active:not(:disabled) { transform: scale(0.98); }` micro-interaction

**What's wrong with v3:**
- **JavaScript is completely gutted** — line 1015 says `/* JS unchanged */` inside an empty script module. NOTHING works.
- **Inspector panel body is empty** — `<div id="tunerGrid">` has no content and no JS to populate it
- **Telemetry log has only static seed rows** — no dynamic event logging, no `logEvent()`, no sanitization
- **KPI tiles show hardcoded values** — "60" FPS, "12ms" latency, "42MB" heap, "-54dBm" WiFi RSSI. These are fake display-only values with no connection to real health data
- **KPI semantics changed incorrectly** — v1 tracks Queue p95, Cadence (Hz), Jitter (ms stddev), Drop Ratio (%). v3 renamed these to FPS, Latency, Heap, WiFi RSSI — completely different metrics. The v1 metrics are correct for a WebSocket stream health monitor. v3's metrics are aspirational hardware telemetry that doesn't exist yet.
- **Timeline phase labels changed** — v3 shows "Input / Mapping / Modulate / Render / Output" (5 labels for 5 segments). v1 shows "Input / Mapping / Render / Output" (4 labels for a continuous range). v3's colored segments are visually better but the Modulate label is redundant given the pipeline node grid already shows it.
- **Pipeline node grid says "PostFX"** in v3 but the timeline says "Render" then "Output" — the PostFX stage exists in the pipeline model but has no timeline segment
- **LED strip count is wrong** — both versions show "64 LEDs" per strip. The actual K1 has **160 LEDs per strip** (320 total). This needs correction.
- **Inline styles are pervasive** — dozens of layout-critical styles specified as HTML attributes rather than classes, creating a dual-source-of-truth problem with the CSS custom property system
- **No `aria-hidden` on decorative elements** — window chrome dots, viewport mock controls lack accessibility attributes
- **Viewport mock buttons have no interaction path** — Move/Rotate/Scale are purely decorative with no planned JS, no disabled state, no tooltip explaining they're previews of future functionality

---

## 4. MERGE STRATEGY — WHAT COMES FROM WHERE

### From v3 (visual layer — CSS + HTML structure):

Take ALL of v3's CSS, including:
- All custom properties (`:root` block)
- Range slider styling
- `.code-glow` pseudo-element
- All viewport classes (`.viewport-shell`, `.viewport-top`, `.viewport-canvas`, `.viewport-grid`, `.lgp-mock`, `.lgp-plate`, `.lgp-edge`, `.lgp-glow`, `.viewport-bottom`, `.viewport-hud`, `.viewport-axis`)
- `.strip` styles
- `.algo-flow` grid layout
- `.speed-group` button group
- `.telemetry-log` and `.feed-row/.feed-ts/.feed-pill/.feed-msg` (with the grid template, tabular-nums, explicit border colors)
- `.kv-grid`, `.toast`, `.code-line` + syntax token classes + `.active-line`
- `.btn`, `.btn-accent`, `.btn-danger`, `.btn-ghost`, `.btn-icon` (including the `scale(0.98)` active state)
- `.panel`, `.panel-header`, `.panel-badge`, `.panel-body`
- `.badge-status` + semantic variants
- `@keyframes execPulse` + `.exec-dot-active`
- `.inspector-section`, `.inspector-section-header`, `.inspector-section-title`, `.inspector-section-chevron`, `.inspector-row` (with Grid layout), `.inspector-row-label`, `.inspector-row-value`, `.inspector-row-slider`
- `.topbar-glass` (v3's simpler version without `::after`)
- `.kpi-grid`, `.kpi-tile`, `.kpi-tile-label`, `.kpi-tile-value`, `.kpi-tile-sub`, `.kpi-tile-bar`
- Alert classes
- `.timeline-track`, `.timeline-track-bg`, `.timeline-segment` + phase variants
- `.console-header-strip`, `.console-tab`
- `.section-label`
- All responsive breakpoints (3-tier)

Take v3's HTML structure for:
- Window chrome row (but add `aria-hidden="true"` to the dots)
- Topbar layout with K1 badge, title, host input, connect button (as icon), control buttons, lease metadata
- Code panel with console header strip + tabs
- LED Preview panel WITH the viewport shell mock
- Stream Health / Diagnostics panel
- Execution Timeline panel WITH colored phase segments
- Inspector panel header (with collapse-all and options buttons)
- Telemetry panel with collapsible header

### From v1 (interaction layer — JavaScript):

Take ALL of v1's JavaScript, specifically:
- The `ui` object (will need updates to reference v3's new DOM elements)
- The `state` object (complete)
- `paramConfig` array
- `SENSITIVE_TELEMETRY_KEYS` set
- `sampleCode` array + `phaseLines` map
- All functions: `renderCode`, `highlightCodePhase`, `renderCentreOriginVis`, `makeReqId`, `toast`, `sanitiseTelemetry`, `eventSeverity`, `eventLabel`, `logEvent`, `pushAlert`, `processAlertFromEvent`, `renderAlerts`, `updateKPI`, `computeAndRenderHealth`, `updateDiagBadge`, `stddev`, `recordMessageTiming`, `setConnectionBadge`, `setLeaseModeBadge`, `canMutate`, `updateControls`, `updateLeaseMeta`, `updateLeaseFromPayload`, `startLeaseCountdown`, `stopLeaseCountdown`, `stopHeartbeat`, `startHeartbeat`, `cleanupWs`, `wsUrl`, `apiUrl`, `connect`, `handleUnsolicitedMessage`, `sendWsCommand`, `fetchJson`, `fetchMutating`, `refreshLeaseStatus`, `acquireLease`, `releaseLease`, `emergencyStop`, `applyStatusFrame`, `derivePhase`, `highlightAlgorithmPhase`, `updateStripPreview`, `paintStrip`, `createTuner`, `transportTick`, `playTimeline`, `pauseTimeline`, `resetTimeline`, `startStatusPolling`, `bindEvents`, `init`
- The 500ms `setInterval` for health refresh
- The lease toggle event listener

### Conflict Resolution Rules

When v1 and v3 disagree, apply these rules:

| Conflict | Resolution |
|----------|------------|
| KPI labels/metrics | Use **v1's metrics** (Queue p95, Cadence, Jitter, Drop Ratio) — these are real stream health metrics the WebSocket code actually computes. v3's FPS/Latency/Heap/RSSI are aspirational but have no data source. |
| KPI sub-labels | Use v1's format: `buffered msgs`, `msg/sec`, `stddev`, percentage with 10s/60s breakdown |
| LED count per strip | Use **160** (neither version is correct — both say 64) |
| Inspector row layout | Use **v3's CSS Grid** (`grid-template-columns: minmax(72px,auto) 1fr minmax(30px,auto)`) but populate via **v1's `createTuner()` function** |
| Telemetry log | Use **v3's CSS styling** (grid template, feed-pill borders, tabular-nums) with **v1's `logEvent()` dynamic rendering**. Keep v3's static seed rows as initial content that gets replaced on first real event. |
| Code panel header | Use **v3's console strip** with tabs, but the tabs should be decorative for now (pipeline.ts active, config inactive) |
| Code panel background | Use **v3's** `#090a0d` |
| Timeline labels | Use 5 labels matching v3's 5 segments: Input, Mapping, Modulate, Render, Output |
| `derivePhase()` | Update to match 5 segments: `t < 0.2` → input, `< 0.4` → mapping, `< 0.6` → modulation, `< 0.8` → render, `≥ 0.8` → output |
| `phaseLines` map | Verify it includes entries for all 6 pipeline stages (input, mapping, modulation, render, post, output) |
| Viewport mock buttons | Keep from v3 but add `disabled` attribute and `title="Coming soon"` |
| Connect button | v1 uses a labeled button (`Connect`), v3 uses an icon-only button. Use **v3's icon button** but add the text "Connect" back as a visible label beside the icon. When connected, change to "Disconnect" icon+label. |
| Panel body padding | Use **v3's** values (9px/10px/11px/13px) |
| Font sizing | Use **v3's** values throughout |

---

## 5. IDENTIFIED FLAWS — MUST FIX

These are concrete bugs, errors, and design problems that exist in one or both versions. Each must be resolved in the merged output.

### FLAW-01: LED Count is Wrong (Both versions)
**Problem:** Both versions display "64 LEDs" per strip and render 64 elements in `paintStrip()`. The K1 has 160 LEDs per strip (320 total).
**Fix:**
- Update strip labels to "160 LEDs"
- Update `paintStrip()` to create 160 span elements per strip (though consider performance — 320 DOM elements animating at 60fps via requestAnimationFrame is expensive)
- Consider rendering at reduced resolution (e.g., 80 elements per strip with each representing 2 LEDs) for performance, while displaying the true LED count in the label
- Update `sampleCode` config block: `ledsPerStrip: 160` (currently shows 64)
- Update `renderCentreOriginVis()` to use 64 elements (currently 32) or proportional representation

### FLAW-02: KPI Metrics Mismatch (v3)
**Problem:** v3 changed KPI tile labels to FPS/Latency/Heap/WiFi RSSI with hardcoded fake values. The JavaScript health monitoring system computes completely different metrics (queue depth, message cadence, jitter, drop ratio).
**Fix:** Restore v1's KPI tile labels and IDs. The `computeAndRenderHealth()` function references `kpiQueueP95`, `kpiCadence`, `kpiJitter`, `kpiDropRatio`, `kpiDrop10s`, `kpiDrop60s` — these DOM IDs must exist and match. The tile HTML must use v1's metric structure:
- Tile 1: "Queue p95" → `id="kpiQueueP95"` → sub: "buffered msgs"
- Tile 2: "Cadence" → `id="kpiCadence"` → sub: "msg/sec"
- Tile 3: "Jitter" → `id="kpiJitter"` → sub: "stddev"
- Tile 4: "Drop Ratio" → `id="kpiDropRatio"` → sub with two spans: `id="kpiDrop10s"` and `id="kpiDrop60s"`

### FLAW-03: Pervasive Inline Styles (v3, partially v1)
**Problem:** Dozens of elements specify layout-critical styles inline (`style="padding:5px 13px;font-size:0.625rem;gap:4px;"` on buttons, `style="display:flex;flex-direction:column;gap:9px;"` on panel bodies, etc.). This creates a dual-source-of-truth problem and makes the codebase hostile to AI-driven parameter modification.
**Fix:** Extract the most commonly repeated inline style patterns into named CSS classes. Priority targets:
- Topbar row layouts → `.topbar-chrome-row`, `.topbar-main-row`, `.topbar-controls`
- Panel body flex columns → `.panel-body-stack` (flex column with gap)
- Button size variants → `.btn-sm` (the smaller padding variant used throughout v3)
- Divider lines → `.divider-v` (vertical), `.divider-h` (horizontal)
- Mono metadata spans → `.meta-label` (the font-family:mono; font-size:0.5rem; color:tertiary pattern)
- Strip header rows → `.strip-header` (flex, space-between, margin-bottom)

Do NOT attempt to extract ALL inline styles. Some are genuinely one-off (e.g., grid-column assignments on sections). Focus on patterns that repeat 3+ times.

### FLAW-04: Telemetry Log innerHTML Rebuild (v1)
**Problem:** `logEvent()` calls `container.innerHTML = ''` and then rebuilds the entire event buffer (up to 120 entries) on every single event. This is a performance anti-pattern — full DOM teardown and reconstruction for every WebSocket message.
**Fix:** Use incremental DOM insertion:
- On new event: prepend a new `.feed-row` element to the top of the container
- Trim excess rows from the bottom when count exceeds the cap (120)
- Avoid `innerHTML = ''` entirely
- Keep the `state.eventBuffer` array for state management but decouple it from full re-renders

### FLAW-05: Alert Badge Styling via Inline Style Overrides (v1)
**Problem:** `renderAlerts()` sets `alertCount.style.cssText` with full inline style strings to change badge appearance. `updateDiagBadge()` does the same. These fight against the CSS class system.
**Fix:** Use class toggling instead:
- Define `.badge-count-critical`, `.badge-count-warning`, `.badge-count-muted` CSS classes
- Replace `style.cssText` assignments with `className` changes
- Same for `updateDiagBadge()` — use classes `.diag-critical`, `.diag-degraded`, `.diag-idle`, `.diag-healthy`

### FLAW-06: highlightCodePhase Uses Inline Styles (v1)
**Problem:** `highlightCodePhase()` directly sets `el.style.background`, `el.style.borderLeftColor`, `el.style.color` on each code line. v3 defines `.code-line.active-line` CSS class for this exact purpose.
**Fix:** Use v3's `.active-line` class:
```javascript
allLines.forEach(el => {
  const ln = parseInt(el.dataset.line);
  el.classList.toggle('active-line', lines.includes(ln));
});
```

### FLAW-07: No Debouncing on Parameter Slider Changes (v1)
**Problem:** `createTuner()` sends an HTTP POST (`fetchMutating`) on every `change` event from sliders. If a user rapidly adjusts a slider, this fires multiple requests. More importantly, the slider also updates the display value on `input` (which fires continuously during drag) but only sends the API call on `change`. This is acceptable behavior but should be explicit.
**Fix:** Add a note-comment in the code clarifying the intentional `input` vs `change` split. Optionally add a debounce (150ms) on the `change` handler to coalesce rapid discrete changes. Do NOT debounce the visual `input` handler — that must remain instant.

### FLAW-08: WebSocket Reconnection is Missing (v1)
**Problem:** When the WebSocket closes (`ws.onclose`), the dashboard cleans up but makes no attempt to reconnect. User must manually click Connect again.
**Fix:** Implement exponential backoff reconnection:
- On unintended disconnect (not user-initiated), start reconnect attempts
- Backoff schedule: 1s → 2s → 4s → 8s → 16s → 30s (cap)
- Show reconnection status in the connection badge: "Reconnecting (attempt 3)..."
- Cancel reconnection if user clicks Disconnect or navigates away
- On successful reconnect, re-acquire lease if the user previously had one
- Cap at 10 attempts, then show "Connection failed — click to retry"

### FLAW-09: Toast Timer Race Condition (v1)
**Problem:** `toast()` uses `window.clearTimeout(toast.timer)` but `toast.timer` is set as a property on the function object. This works but is fragile — if toast is reassigned, the timer reference is lost. Additionally, rapid successive toasts replace each other immediately with no queue.
**Fix:** Use a module-scoped variable instead of a function property. Consider a simple toast queue: if a toast is currently showing, queue the next one and display it after the current one fades (or immediately replace with a counter badge like "2 notifications").

### FLAW-10: LGP Preview Doesn't Reflect Optical Reality
**Problem:** The LED strips in the preview show raw per-pixel colors in flat bars. This accurately represents the LED data buffer but NOT what the user actually sees through the light guide plates. Users designing effects need to see the LGP-diffused output, not the raw data.
**Fix (structural preparation — full implementation can be deferred):**
- Add a `<canvas>` element below or alongside the strip bars labeled "LGP Preview (simulated)"
- Add a `renderLGPPreview(stripAColors, stripBColors)` function stub that:
  - Takes the computed strip colors
  - Applies a Gaussian blur approximation (averaging neighboring LEDs with falloff weights)
  - Renders the blended result on the canvas
  - Shows the two LGP plates as overlapping layers with alpha blending
- Mark this function with a `// TODO: LGP optical simulation` comment for future enhancement
- The existing raw strip bars should remain as "Strip A (raw)" and "Strip B (raw)" for debugging

### FLAW-11: Missing Error Boundary on WebSocket JSON Parse (v1)
**Problem:** `ws.onmessage` does `try { msg = JSON.parse(event.data); } catch { return; }` — silently swallows parse errors with no logging.
**Fix:** Log parse failures to telemetry: `logEvent("ws.parse_error", { snippet: event.data.slice(0, 100) })`.

### FLAW-12: Health Computation Runs When Disconnected (v1)
**Problem:** The 500ms `setInterval(computeAndRenderHealth, 500)` runs unconditionally even when disconnected. The function handles this with an early return, but the interval itself is wasteful.
**Fix:** Start the interval on connect, clear it on disconnect. Restart on reconnect.

### FLAW-13: `leaseCountdownTimer` Accumulates Drift (v1)
**Problem:** `startLeaseCountdown()` decrements `remainingMs` by 100 every 100ms using `setInterval`. `setInterval` is not guaranteed to fire at exact intervals — over a 5000ms TTL, cumulative drift can reach 50–200ms, making the displayed remaining time inaccurate.
**Fix:** Store the countdown start time and compute remaining time from `performance.now() - startTime` on each tick. This eliminates drift entirely.

### FLAW-14: Font Sizes Below Readability Threshold (v3)
**Problem:** v3 uses `0.5rem` (8px) extensively for section labels, viewport titles, axis indicators, metadata, and badge text. At 8px, text is borderline illegible on non-retina displays and for users with less-than-perfect vision.
**Fix:** Establish a minimum readable size of `0.5625rem` (9px) for all interactive/informational text. Purely decorative labels (axis indicator "XYZ", viewport hint text) can remain at `0.5rem`. Add a CSS custom property `--text-min: 0.5625rem` and reference it in section labels, badge text, and tile labels. Audit all `font-size: 0.5rem` occurrences and upgrade informational ones.

### FLAW-15: Viewport Mock Suggests Interactivity That Doesn't Exist (v3)
**Problem:** The viewport has Move/Rotate/Scale buttons, "drag to orbit — scroll to zoom" hint text, and an XYZ axis indicator. These suggest interactive 3D manipulation that is entirely absent. This creates false expectations.
**Fix:** 
- Add `disabled` attribute to all viewport tool buttons
- Change hint text to "3D viewport — coming soon" or remove entirely
- Add a subtle "preview" watermark or badge to the viewport canvas
- Keep the LGP mock visualization (it's valuable as a static reference)

---

## 6. HARDENING REQUIREMENTS

These are not bug fixes but engineering standards the merged codebase must meet.

### H-01: DOM ID Contract
Every element referenced by JavaScript MUST have a stable `id` attribute. Create a constant map at the top of the script documenting all required IDs:

```javascript
// DOM Contract — these IDs must exist in the HTML
const DOM_IDS = {
  // Topbar
  hostInput: 'hostInput', connectBtn: 'connectBtn', connectBadge: 'connectBadge',
  acquireBtn: 'acquireBtn', releaseBtn: 'releaseBtn', reacquireBtn: 'reacquireBtn',
  statusBtn: 'statusBtn', emergencyBtn: 'emergencyBtn',
  leaseBadge: 'leaseBadge', ownerBadge: 'ownerBadge',
  leaseId: 'leaseId', leaseTtl: 'leaseTtl', hbState: 'hbState',
  // Code panel
  codeContent: 'codeContent', lineNumbers: 'lineNumbers', lineCount: 'lineCount',
  execIndicator: 'execIndicator', execStatus: 'execStatus', execPhase: 'execPhase',
  // Preview
  stripA: 'stripA', stripB: 'stripB', centreOriginVis: 'centreOriginVis',
  streamBadge: 'streamBadge',
  // Health
  kpiQueueP95: 'kpiQueueP95', kpiCadence: 'kpiCadence',
  kpiJitter: 'kpiJitter', kpiDropRatio: 'kpiDropRatio',
  kpiDrop10s: 'kpiDrop10s', kpiDrop60s: 'kpiDrop60s',
  kpiGrid: 'kpiGrid', diagBadge: 'diagBadge',
  // Pipeline
  algoFlow: 'algoFlow',
  // Alerts
  alertList: 'alertList', alertCount: 'alertCount',
  // Lease state
  leaseToggle: 'leaseToggle', leaseChevron: 'leaseChevron',
  leaseStateWrapper: 'leaseStateWrapper', leaseStateBlock: 'leaseStateBlock',
  // Timeline
  timeline: 'timeline', playBtn: 'playBtn', pauseBtn: 'pauseBtn', resetBtn: 'resetBtn',
  // Inspector
  tunerGrid: 'tunerGrid',
  // Telemetry
  telemetryLog: 'telemetryLog', teleChevron: 'teleChevron', teleContent: 'teleContent',
  // Toast
  toast: 'toast',
};
```

### H-02: Event Delegation
Replace all inline `onclick` handlers with JavaScript event listeners. The telemetry collapse toggle currently uses an inline `onclick` — move this to `bindEvents()`.

### H-03: Defensive DOM Queries
The `ui` object initialization should use a helper that logs missing elements rather than silently returning null:

```javascript
function getEl(id) {
  const el = document.getElementById(id);
  if (!el) console.warn(`[K1 Composer] Missing DOM element: #${id}`);
  return el;
}
```

### H-04: State Immutability Discipline
The `state` object is currently mutated directly throughout the codebase. While a full state management system is overkill for a single-file dashboard, add a `setState(path, value)` helper that logs state transitions in development mode. This becomes critical when the AI layer needs to observe state changes.

### H-05: CSS Custom Property Completeness
Ensure ALL colors used in the dashboard are defined as CSS custom properties. Audit for hardcoded hex values in the CSS — several exist in v3:
- `#090a0d` (code panel background) — define as `--bg-deep`
- `#0a0b0e` (telemetry log background) — use `--bg-deep`
- `#9ca3af`, `#b6c0cf` (feed message colors) — define as `--text-feed` and `--text-feed-hover`
- `#93c5fd`, `#86efac`, `#fcd34d`, `#fca5a5` (pill text colors) — define as `--pill-info-text`, `--pill-success-text`, `--pill-warning-text`, `--pill-error-text`
- `#1e2028` (strip default LED color, v3) vs `#2a2d35` (v1) — define as `--led-off` and use the v3 value

---

## 7. DESIGN SYSTEM & VISUAL LANGUAGE

### Color Tokens (from v3's `:root`, extended)

```css
:root {
  /* Backgrounds */
  --bg: #0c0d10;
  --bg-deep: #090a0d;           /* NEW: code panel, telemetry bg */
  --surface: #16181d;
  --surface-raised: #1c1f26;
  
  /* Borders */
  --border: rgba(255,255,255,0.08);
  --border-subtle: rgba(255,255,255,0.05);
  
  /* Text hierarchy */
  --text: #ececef;
  --text-secondary: #8b8d98;
  --text-tertiary: #5c5e6a;
  --text-feed: #9ca3af;          /* NEW: telemetry message text */
  --text-feed-hover: #b6c0cf;    /* NEW: telemetry message hover */
  --text-min: 0.5625rem;         /* NEW: minimum readable font size */
  
  /* Semantic colors */
  --accent: #6e56cf;
  --accent-hover: #7c66d4;
  --accent-subtle: rgba(110,86,207,0.15);
  --danger: #e5484d;
  --danger-hover: #ec5d5e;
  --danger-subtle: rgba(229,72,77,0.12);
  --success: #30a46c;
  --success-subtle: rgba(48,164,108,0.12);
  --warning: #f5a623;
  --warning-subtle: rgba(245,166,35,0.12);
  --info: #3b82f6;
  --info-subtle: rgba(59,130,246,0.12);
  
  /* Pill text (for telemetry severity pills) */
  --pill-info-text: #93c5fd;
  --pill-success-text: #86efac;
  --pill-warning-text: #fcd34d;
  --pill-error-text: #fca5a5;
  
  /* LED preview */
  --led-off: #1e2028;
  
  /* Layout */
  --radius: 10px;
  --radius-lg: 14px;
  --shadow: 0 1px 3px rgba(0,0,0,0.4), 0 8px 24px rgba(0,0,0,0.25);
  --shadow-sm: 0 1px 2px rgba(0,0,0,0.3);
  
  /* Typography */
  --font: 'Inter', -apple-system, BlinkMacSystemFont, sans-serif;
  --mono: 'IBM Plex Mono', 'Menlo', monospace;
}
```

### Typography Scale

| Use case | Size | Weight | Font |
|----------|------|--------|------|
| Panel titles | 0.6875rem (11px) | 560 | --font |
| Code text | 0.6875rem | 400 | --mono |
| Button labels | 0.625rem (10px) | 520-560 | --font |
| Badge text | 0.5625rem (9px) | 560 | --mono |
| Section labels | 0.5rem (8px) | 600 | --mono |
| KPI values | 1.0625rem (17px) | 600 | --mono |
| KPI labels | 0.5rem | 520 | --mono |
| Metadata / timestamps | 0.5625rem | 400-500 | --mono |

### Iconography
All icons use Iconify's Solar Linear icon set with `stroke-width: 1.5`. Do not mix icon sets. Key icons:
- Code: `solar:code-linear`
- LED/Light: `solar:lightbulb-bolt-linear`
- Health: `solar:graph-new-linear`
- Play/Pause/Reset: `solar:play-linear`, `solar:pause-linear`, `solar:restart-linear`
- Inspector: `solar:tuning-2-linear`
- Telemetry: `solar:document-text-linear`
- Connect: `solar:plug-circle-linear`
- Lock/Unlock: `solar:lock-keyhole-linear`, `solar:lock-unlocked-linear`
- Emergency: `solar:danger-triangle-linear`
- Alerts: `solar:bell-linear`
- Pipeline: `solar:cpu-linear`
- Refresh: `solar:refresh-linear`
- Chevrons: `solar:alt-arrow-down-linear`, `solar:alt-arrow-right-linear`
- Settings/Options: `solar:menu-dots-linear`
- AI Assistant: `solar:magic-stick-3-linear` (for the new AI panel)

---

## 8. JAVASCRIPT ARCHITECTURE CONTRACT

### Module Structure

The JS should be organized into logical sections within the single `<script type="module">` block:

```
1. DOM Contract (DOM_IDS constant)
2. UI Element References (ui object with getEl helper)
3. State Object (state)
4. Configuration (paramConfig, SENSITIVE_TELEMETRY_KEYS, sampleCode, phaseLines)
5. Utility Functions (toast, stddev, makeReqId, sanitiseTelemetry, getEl)
6. Rendering Functions (renderCode, highlightCodePhase, paintStrip, renderCentreOriginVis, renderLGPPreview stub)
7. Health & Diagnostics (computeAndRenderHealth, updateKPI, updateDiagBadge, recordMessageTiming)
8. Alert System (pushAlert, processAlertFromEvent, renderAlerts)
9. Telemetry (logEvent, eventSeverity, eventLabel)
10. Lease Management (acquireLease, releaseLease, startHeartbeat, stopHeartbeat, updateLeaseFromPayload, refreshLeaseStatus, etc.)
11. WebSocket Layer (connect, cleanupWs, wsUrl, apiUrl, sendWsCommand, handleUnsolicitedMessage, reconnection logic)
12. Transport Controls (playTimeline, pauseTimeline, resetTimeline, transportTick)
13. Inspector/Tuner (createTuner)
14. UI State Updates (updateControls, updateLeaseMeta, setConnectionBadge, setLeaseModeBadge)
15. Event Binding (bindEvents)
16. Initialization (init)
```

### State Shape

```javascript
const state = {
  host: '',
  ws: null,
  connected: false,
  reconnect: { attempts: 0, timer: null, maxAttempts: 10, userDisconnected: false },
  clientName: 'K1 Composer Dashboard',
  clientInstanceId: crypto.randomUUID(),
  lease: {
    active: false,
    leaseId: '',
    leaseToken: '',
    ownerClientName: '',
    ownerInstanceId: '',
    ownerWsClientId: null,
    remainingMs: 0,
    ttlMs: 5000,
    heartbeatIntervalMs: 1000,
    scope: 'global',
    countdownStartTime: 0,   // NEW: for drift-free countdown
    countdownStartRemaining: 0, // NEW
  },
  heartbeatTimer: null,
  statusPollTimer: null,
  leaseCountdownTimer: null,
  healthPollTimer: null,      // NEW: moved from global setInterval
  pending: new Map(),
  reqSeq: 0,
  transport: { playing: false, speed: 1, timeline: 0, raf: null, lastTs: 0 },
  eventBuffer: [],
  codeActiveLine: 0,
  health: {
    msgTimestamps: [],
    jitterSamples: [],
    lastMsgTime: 0,
    dropped10s: 0, total10s: 0,
    dropped60s: 0, total60s: 0,
    queueP95: 0,
  },
  alerts: [],
};
```

### `paintStrip()` Performance Consideration

With 160 LEDs per strip (320 total DOM elements) updating at 60fps via `requestAnimationFrame`, direct style manipulation of 320 `<span>` elements is borderline. Options:

**Option A (recommended): Render at half-resolution.** Create 80 `<span>` elements per strip. Each represents 2 physical LEDs. Average or pick the first LED's color. Display "160 LEDs" in the label. Add a title tooltip: "Rendering at 2:1 for performance."

**Option B: Canvas rendering.** Replace strip `<div class="strip">` with `<canvas>` elements. Draw colored rectangles. Eliminates DOM overhead entirely. This is the better long-term approach and aligns with the LGP preview canvas.

For this implementation, use **Option A** for strips and add the LGP preview canvas as a structural stub.

---

## 9. AI INTEGRATION FOUNDATION

This section defines the structural HTML, CSS, and architectural hooks that prepare the Composer for AI integration. You are NOT implementing the AI backend — you are building the UI panel, the event bus hooks, and the state management extension points.

### AI Assistant Panel

Add a new grid section in the main grid. Position: below the Inspector panel, spanning columns 9-13 (same width as Inspector).

```html
<!-- ── AI ASSISTANT ── -->
<section class="panel ai-panel" style="grid-column:9/13;">
  <div class="panel-header">
    <span class="panel-icon">
      <iconify-icon icon="solar:magic-stick-3-linear" width="13" style="color:var(--accent);stroke-width:1.5"></iconify-icon>
    </span>
    <span>AI Assistant</span>
    <span class="panel-badge badge-muted">preview</span>
    <div style="margin-left:auto;display:flex;align-items:center;gap:6px;">
      <span class="ai-status-dot"></span>
      <button class="btn-icon" type="button" aria-label="AI settings" title="AI settings">
        <iconify-icon icon="solar:settings-linear" width="14" style="stroke-width:1.5"></iconify-icon>
      </button>
    </div>
  </div>
  <div class="panel-body ai-panel-body">
    <div class="ai-welcome">
      <iconify-icon icon="solar:magic-stick-3-linear" width="20" style="color:var(--accent);opacity:0.5;"></iconify-icon>
      <p>AI-assisted effect design</p>
      <span>Connect to your K1 to enable AI features</span>
    </div>
    <div class="ai-chat-log" id="aiChatLog"></div>
    <div class="ai-input-row">
      <input type="text" id="aiInput" placeholder="Describe an effect or ask for help..."
             class="ai-text-input" disabled>
      <button id="aiSendBtn" class="btn btn-accent btn-sm" disabled>
        <iconify-icon icon="solar:arrow-up-linear" width="14" style="stroke-width:1.5"></iconify-icon>
      </button>
    </div>
  </div>
</section>
```

### AI Panel CSS

```css
/* ── AI Assistant ── */
.ai-panel-body {
  display: flex;
  flex-direction: column;
  gap: 8px;
  min-height: 200px;
  max-height: 400px;
}
.ai-welcome {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 6px;
  text-align: center;
  padding: 20px 10px;
}
.ai-welcome p {
  font-size: 0.6875rem;
  font-weight: 560;
  color: var(--text-secondary);
}
.ai-welcome span {
  font-size: var(--text-min);
  color: var(--text-tertiary);
  font-family: var(--mono);
}
.ai-chat-log {
  flex: 1;
  overflow-y: auto;
  display: none; /* shown when AI is active */
  flex-direction: column;
  gap: 6px;
  padding: 4px 0;
}
.ai-input-row {
  display: flex;
  gap: 6px;
  align-items: center;
  padding-top: 6px;
  border-top: 1px solid var(--border-subtle);
}
.ai-text-input {
  flex: 1;
  min-width: 0;
  border: 1px solid var(--border);
  border-radius: 6px;
  padding: 6px 10px;
  font-family: var(--font);
  font-size: 0.6875rem;
  background: var(--bg);
  color: var(--text);
  outline: none;
  transition: border-color 0.13s;
}
.ai-text-input:focus { border-color: var(--accent); }
.ai-text-input:disabled { opacity: 0.4; cursor: not-allowed; }
.ai-status-dot {
  width: 5px;
  height: 5px;
  border-radius: 50%;
  background: var(--text-tertiary);
  transition: background 0.3s;
}
.ai-status-dot.active { background: var(--success); }
```

### AI State Extension

Add to the `state` object:

```javascript
ai: {
  enabled: false,
  connected: false,
  messages: [],
  pendingRequest: false,
  model: 'claude-sonnet-4-20250514',
  systemContext: null, // Will hold pipeline context for API calls
},
```

### Event Bus Hook (Future AI Integration Point)

Add a simple pub/sub mechanism that the AI layer can subscribe to:

```javascript
// Simple event bus for cross-system communication
const eventBus = {
  _listeners: {},
  on(event, fn) {
    (this._listeners[event] ||= []).push(fn);
    return () => { this._listeners[event] = this._listeners[event].filter(f => f !== fn); };
  },
  emit(event, data) {
    (this._listeners[event] || []).forEach(fn => fn(data));
  },
};
```

Wire this into key state changes:
- `eventBus.emit('pipeline:phase', { phase })` in `highlightAlgorithmPhase()`
- `eventBus.emit('connection:change', { connected })` in `setConnectionBadge()`
- `eventBus.emit('lease:change', { ...state.lease })` in `updateLeaseMeta()`
- `eventBus.emit('parameter:change', { key, value })` in the tuner slider change handler
- `eventBus.emit('health:update', { ...healthMetrics })` in `computeAndRenderHealth()`
- `eventBus.emit('transport:state', { playing, speed, position })` in transport functions
- `eventBus.emit('alert:new', { tier, msg })` in `pushAlert()`

This event bus is the future MCP bridge point. When the AI backend is connected, it subscribes to these events to maintain pipeline state awareness.

### Responsive Grid Update

Add the AI panel to the responsive breakpoint rules:

```css
@media (max-width: 1079px) and (min-width: 768px) {
  .ai-panel { grid-column: 5/7 !important; }
}
@media (max-width: 767px) {
  .ai-panel { grid-column: 1/-1 !important; }
}
```

---

## 10. ACCEPTANCE CRITERIA

The merged file must pass ALL of these checks:

### Functional
- [ ] Opening the HTML file in a browser shows the complete dashboard with all panels rendered
- [ ] All 6 pipeline nodes display in the grid (Input, Mapping, Modulate, Render, PostFX, Output)
- [ ] Clicking "Run" starts the transport — timeline scrubs, pipeline phases highlight in the code, LED strips animate, execution dot pulses green
- [ ] Clicking "Pause" stops animation, "Reset" returns to position 0
- [ ] Speed buttons (0.25×, 0.5×, 1×, 2×) change transport speed, active button is highlighted
- [ ] Inspector panel shows 6 parameter sliders in 3 groups (Motion: Speed/Intensity, Color: Saturation/Complexity, Output: Variation/Brightness)
- [ ] Sliders are disabled when not connected (correct — no lease)
- [ ] Entering a host IP and clicking Connect attempts WebSocket connection
- [ ] Connection badge changes between "Disconnected" (muted) and "Connected" (green)
- [ ] Take Control / Release / Reacquire buttons manage lease state
- [ ] E-Stop button sends brightness=0
- [ ] Telemetry log shows timestamped events with severity pills
- [ ] KPI tiles show real health metrics (Queue p95, Cadence, Jitter, Drop Ratio) with animated bars
- [ ] Alerts appear in the alert list based on events and health thresholds
- [ ] Lease state section toggles open/closed
- [ ] Telemetry section toggles open/closed
- [ ] Toast notifications appear and auto-dismiss
- [ ] LED strips display "160 LEDs" label
- [ ] LGP viewport mock renders with perspective transform
- [ ] AI Assistant panel renders with welcome state and disabled input
- [ ] Viewport tool buttons are disabled with "Coming soon" tooltip

### Visual
- [ ] Dark theme renders correctly with no white flashes
- [ ] Code panel has the purple glow border effect
- [ ] Timeline shows colored phase segments behind the range input
- [ ] Window chrome row shows traffic light dots
- [ ] Console header strip shows "pipeline.ts" tab as active
- [ ] All fonts load (Inter + IBM Plex Mono)
- [ ] No horizontal scrolling at 1440px width
- [ ] Responsive layout works at 768px and 375px widths

### Code Quality
- [ ] No inline `onclick` handlers in HTML
- [ ] No hardcoded hex colors in CSS (all use custom properties)
- [ ] All JS DOM references go through the `ui` object
- [ ] `logEvent()` uses incremental DOM insertion, not innerHTML rebuild
- [ ] Health poll interval starts on connect, stops on disconnect
- [ ] Lease countdown is drift-free (uses `performance.now()` delta)
- [ ] WebSocket reconnection with exponential backoff is implemented
- [ ] `eventBus` is wired into all key state transition points
- [ ] Parse errors on WebSocket messages are logged, not silently swallowed
- [ ] File validates as valid HTML5

### Architecture
- [ ] All CSS custom properties are defined in `:root`
- [ ] Extracted utility classes exist for repeated inline style patterns
- [ ] `state` object includes `ai` and `reconnect` sub-objects
- [ ] `eventBus` exists and emits events from pipeline, connection, lease, parameter, health, transport, and alert systems
- [ ] AI panel HTML structure exists with all required IDs (`aiChatLog`, `aiInput`, `aiSendBtn`)
- [ ] DOM_IDS contract constant is defined and used

---

## FINAL NOTES FOR THE IMPLEMENTING AGENT

**Priority order:** Get the merge working and functional first (v3 shell + v1 JS), then fix flaws, then add hardening, then add AI panel structure. Do not attempt to wire up AI API calls — that is a separate task.

**Testing approach:** After each major integration step, verify the file opens in a browser and the transport controls work (Run/Pause/Reset with LED animation). This is the fastest smoke test.

**When in doubt:** Refer back to the hardware specifications and pipeline model. Every UI decision should be traceable to the physical reality of how K1-Lightwave actually works — two edge-lit acrylic plates, 320 LEDs, sub-8ms latency, Goertzel frequency analysis, centre-origin rendering.

**Do not invent features.** The viewport mock buttons, the config tab, and the AI chat are all structural placeholders. Add them as disabled UI elements with clear "coming soon" affordances. Do not implement 3D rendering, configuration editing, or AI API calls.

**The single most important integration test:** With the dashboard open in a browser, click "Run" — the timeline should scrub, the code visualiser should highlight different pipeline phases, the LED strips should animate with cycling hues, and the execution status should read "Running · [phase name]". If this works, the v1 JS has been successfully merged into the v3 shell.
