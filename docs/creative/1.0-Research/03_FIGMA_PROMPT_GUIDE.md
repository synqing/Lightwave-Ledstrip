# Figma Make Agent Prompt Engineering Guide

> Sequential prompts for generating LightwaveOS dashboard wireframes

## 1. Master Prompt Template

Use this template structure for all Figma Make Agent prompts:

```
# LightwaveOS LED Control Dashboard - [Component Name]

## Project Context
Web-based control interface for dual-strip LED system (320 LEDs total).
- Dual 160-LED WS2812 strips with CENTER ORIGIN design (effects radiate from LED 79/80)
- 45+ visual effects optimized for Light Guide Plate (LGP) interference patterns
- Multi-zone composer (up to 4 zones)
- Real-time performance monitoring (target 120 FPS)
- WebSocket + REST API integration

## Design System: PRISM.node Tokens
**Colors** (Dark Theme):
- Background: #0A0E14 (canvas), #1A1F2E (surface), #252B3D (elevated)
- Accent: #FFD700 (gold - primary), #00CED1 (cyan - secondary)
- Text: #E8E8E8 (primary), #A0A0A0 (secondary), #6B7280 (muted)
- Status: #10B981 (success), #F59E0B (warning), #EF4444 (error)
- Zone colors: #FFD700 (Zone 1), #00CED1 (Zone 2), #10B981 (Zone 3), #F59E0B (Zone 4)

**Typography**:
- Headings: Inter, 600-700 weight
- Body: Inter, 400-500 weight
- Code/Numbers: JetBrains Mono, 400 weight

**Spacing**: 4px base (4, 8, 12, 16, 24, 32, 48, 64)
**Border Radius**: 4px (small), 8px (medium), 12px (large)

## Component Requirements
[SPECIFIC COMPONENT DETAILS]

## Output Format
- Component variants (default, hover, active, disabled, error)
- Responsive breakpoints (1920px, 1280px, 768px)
- Interactive prototype connections
- Design token documentation
```

---

## 2. Phase 1: Foundation (Prompts 1-2)

### Prompt 1: Design System Tokens

```
Create a comprehensive PRISM.node design token library for LightwaveOS:

COLORS (organized as Figma styles):
- Backgrounds: #0A0E14 (canvas), #1A1F2E (surface), #252B3D (elevated)
- Primary accent: #FFD700 (gold) with hover/active variants
- Secondary accent: #00CED1 (cyan) with hover/active variants
- Text: #E8E8E8 (primary), #A0A0A0 (secondary), #6B7280 (muted)
- Status: #10B981 (success), #F59E0B (warning), #EF4444 (error)
- LED-specific: #1A1F2E (off), #00CED1 (center marker)
- Zone colors: Gold, Cyan, Emerald, Amber

TYPOGRAPHY (as text styles):
- h1: Inter 32px/700 (page titles)
- h2: Inter 24px/600 (section headers)
- h3: Inter 20px/600 (card titles)
- body: Inter 14px/400 (UI text)
- label: Inter 12px/500 (form labels)
- code: JetBrains Mono 14px/400 (values)

SPACING SCALE: 4, 8, 12, 16, 24, 32, 48, 64px
BORDER RADIUS: 4px (small), 8px (medium), 12px (large)
SHADOWS: Subtle (2px), Moderate (4px), Elevated (8px)

Output: Multi-page Figma document with all tokens as reusable styles.
```

### Prompt 2: Navigation Shell

```
Create the main navigation structure for LightwaveOS dashboard:

HEADER (64px height):
- Logo "LightwaveOS" (gold accent, left)
- Connection indicator: 🟢 Connected / 🔴 Disconnected (center-right)
- Settings gear icon (40x40px, right)

TAB BAR (48px height, below header):
- 4 tabs: [Control + Zones] [Shows] [Effects] [System]
- Active tab: 4px gold bottom border, gold text
- Inactive: Transparent, muted text
- Hover: Gold text, no border yet

LED STRIP (120px height, always visible):
- 320 LED representation (4px each, 1px gap)
- CENTER marker at LED 79/80 (cyan line + "CENTER" label)
- Zone boundary markers (dashed gold lines, conditional)
- Dual strip layout (Strip 1 top, Strip 2 bottom, 16px gap)

Generate all 4 tab states + connection states (connected, disconnected, connecting).
```

---

## 3. Phase 2: Components (Prompts 3-7)

### Prompt 3: Control + Zones Tab

```
Create the Control + Zones combined view:

LEFT COLUMN (30% - Global Controls):
- Brightness master slider (0-255)
- Speed global slider (1-50)
- Active palette dropdown (12 presets)
- Power quick actions: [All Off] [Save Preset]

CENTER COLUMN (40% - Zone Composer):
- Zone tabs: [Zone 1] [Zone 2] [Zone 3] [Zone 4] (color-coded)
- Zone mixer panel per tab:
  - Effect dropdown (45+ effects)
  - Blend mode selector (Add, Multiply, Overlay)
  - Zone enable toggle
- Visual pipeline sliders:
  - Blur (0-100%)
  - Softness (0-100%)
  - Gamma (0.5-2.0)

RIGHT COLUMN (30% - Quick Tools):
- TRANSITIONS section:
  - Type selector (12 types: Fade, Wipe, Dissolve, etc.)
  - Duration slider (100ms - 5000ms)
  - [Trigger] button (gold, prominent)
- Recent effects (last 5)
- Favorites (starred effects)
- Mini performance: FPS badge, memory indicator

Zone tabs should be color-coded: Zone 1=Gold, Zone 2=Cyan, Zone 3=Emerald, Zone 4=Amber
Include all hover/active states for interactive elements.
```

### Prompt 4: Shows Tab

```
Create the Shows/ShowDirector tab:

LEFT PANEL (40% - Show Library):
- Show list cards:
  - Show name (h3)
  - Duration badge (e.g., "3:45")
  - Scene count (e.g., "12 scenes")
  - Thumbnail preview
- [+ New Show] button (gold, bottom)
- Search/filter bar (top)

RIGHT PANEL (60% - Chapter Timeline):
- Timeline scrubber (horizontal, full width)
- Scene blocks (draggable rectangles):
  - Effect name
  - Duration indicator
  - Color-coded by effect category
- Narrative tension curve (overlay line graph):
  - Low → Medium → High → Climax → Resolution
- Keyframe markers (small diamonds on timeline)
- Zoom controls (+/- buttons)

STICKY NOW PLAYING BAR (bottom, 64px):
- Progress bar (gold fill)
- Current time / Total time
- [⏮] [▶/⏸] [⏹] [⏭] transport controls
- Current scene name
- Volume/brightness quick adjust

Show both "no show selected" empty state and "show playing" active state.
```

### Prompt 5: Effects Library Tab

```
Create the Effects + Palettes browser tab:

LEFT PANEL (50% - Effect Browser):
- Search bar (top, full width, magnifying glass icon)
- Category filter pills: [All] [Basic] [LGP] [Quantum] [Geometric] [Organic]
- Effect cards grid (3 columns):
  - Effect icon/thumbnail
  - Effect name
  - Param count badge
  - Favorite star (toggleable)
- Favorites section (collapsible, pinned top)

RIGHT PANEL (50% - Palette Browser):
- Preset palette grid (4x3 = 12 palettes):
  - Gradient preview bar
  - Palette name
  - Active: gold border
- Custom palette section:
  - [+ Create Custom] button
  - Color stops editor (horizontal gradient bar)
- HSV fine-tune sliders:
  - Hue (0-360°, rainbow gradient track)
  - Saturation (0-100%)
  - Value (0-100%)
- Color preview: Large circle showing current color + hex value

SHARED PREVIEW (bottom, 80px):
- Mini LED strip preview (160px width, centered)
- Effect metadata: "Ocean • Basic Effects • 4 parameters"
- [Apply to Zone] dropdown + button

Include search results state, empty search state, and palette editing state.
```

### Prompt 6: System Tab

```
Create the System (Profiling + Network + Settings) tab:

2x2 DASHBOARD GRID:

TOP-LEFT: Performance Card
- FPS chart (sparkline, 60 points, gold line)
- Current FPS: Large number (green/amber/red based on threshold)
- CPU gauge: Circular, percentage
- Memory bar: Horizontal, percentage
- Thresholds: Green ≥100 FPS, Amber 60-99, Red <60

TOP-RIGHT: Network Card
- Connection status: Large dot + label (Connected/Disconnected)
- Device IP: "192.168.1.100" (monospace)
- mDNS: "lightwaveos.local"
- Latency: "12ms" with mini sparkline
- WebSocket: ● Active / ○ Polling fallback

BOTTOM-LEFT: Device Card
- LED count: "320 LEDs (2×160)"
- Pin config: "Strip 1: GPIO4, Strip 2: GPIO5"
- Power limit: "10A @ 5V"
- Hardware: "ESP32-S3 @ 240MHz"

BOTTOM-RIGHT: Firmware Card
- Version: "v1.2.3"
- Build date: "Dec 21, 2025"
- Flash: Progress bar + "36.4%"
- RAM: Progress bar + "36.6%"
- [Factory Reset] button (red, confirmation required)
- [Reboot] button (amber)

COLLAPSIBLE SECTIONS (below grid):
- WiFi Configuration (SSID, password, reconnect button)
- Debug Options (verbose logging toggle, perf overlay toggle)
- Serial Monitor (terminal-style output area, 10 lines visible)

Show healthy state (all green) and degraded state (amber warnings).
```

### Prompt 7: Component Library

```
Create reusable UI components for shadcn/ui mapping:

BUTTONS:
- Primary: Gold fill, dark text, 40px height
- Secondary: Transparent, gold border, gold text
- Ghost: Transparent, light text
- Destructive: Red fill, white text
- Icon-only: 40x40px, centered icon
- States: default, hover (+10% brightness), active, disabled (40% opacity)

SLIDERS:
- Track: 4px height, dark surface fill
- Progress: Gold fill
- Thumb: 16px circle, gold, elevated shadow
- Labels: Name (left), Value (right, monospace)
- Min/Max: Below track, muted text

INPUTS:
- Text: Dark surface, 1px border, 40px height, 8px radius
- Select: Dropdown arrow, dark overlay, scrollable options
- Toggle: 40x24px, gold when on, muted when off
- Checkbox: 20x20px, gold checkmark when checked

CARDS:
- Surface: #1A1F2E, 8px radius, subtle shadow
- Elevated: #252B3D, 12px radius, moderate shadow
- Bordered: 1px border, no shadow

STATUS INDICATORS:
- Dot: 8px circle (green/amber/red)
- Badge: Pill shape, status color background, dark text
- Progress: Horizontal bar with percentage fill

Generate all components with all states and accessibility annotations.
```

---

## 4. Phase 3: Composition (Prompts 8-9)

### Prompt 8: Full Dashboard Layout

```
Compose all components into the complete LightwaveOS dashboard:

LAYOUT GRID:
┌─────────────────────────────────────────────────────────┐
│  HEADER (64px) - Logo, Tabs, Connection Status          │
├─────────────────────────────────────────────────────────┤
│  LED STRIP VISUALIZATION (120px) - Always visible       │
├─────────────────────────────────────────────────────────┤
│  TAB CONTENT (fills remaining height)                   │
│  ┌───────────┬────────────────────────┬────────────────┐│
│  │ Left Col  │ Center Column          │ Right Column   ││
│  │ (30%)     │ (40%)                  │ (30%)          ││
│  └───────────┴────────────────────────┴────────────────┘│
└─────────────────────────────────────────────────────────┘

Show all 4 tabs with correct content:
- Tab 1: Control + Zones (3-column layout)
- Tab 2: Shows (40/60 split with timeline)
- Tab 3: Effects (50/50 split with preview)
- Tab 4: System (2x2 dashboard grid)

Create interactive prototype links:
- Tab clicks switch content
- Effect selection populates parameters
- Zone tab clicks switch zone config
- Slider interactions (hover states)

Ensure consistent 16px grid alignment throughout.
```

### Prompt 9: Responsive Variants

```
Create responsive layouts for LightwaveOS dashboard:

LAPTOP (1600x900):
- Header: Same
- LED strip: Same (scales to viewport)
- Tab content:
  - 3-column → 2-column (right column moves to bottom sheet)
  - Global controls: Left
  - Zone composer: Right (expanded)
  - Quick tools: Bottom drawer (pull-up)

TABLET (1280x720):
- Header: Collapse to hamburger menu for tabs
- LED strip: Horizontal scroll if needed
- Tab content: Single column, stacked sections
- Collapsible panels for each section

MOBILE (768x1024) - Optional:
- Bottom tab bar (icons only, 4 tabs)
- LED strip: Simplified (every 4th LED, 80 pixels shown)
- Full-width stacked layout
- Modal sheets for parameters

Show at least Laptop and Tablet breakpoints with annotations.
```

---

## 5. Phase 4: Refinement (Prompts 10-11)

### Prompt 10: Edge Cases & Empty States

```
Add edge case handling to LightwaveOS dashboard:

EMPTY STATES:
- No effect selected: "Select an effect to begin" + effect icon
- No zones configured: "Configure your first zone" + setup wizard link
- No shows created: "Create your first show" + [+ New Show] button
- Disconnected: "Connection lost" banner with [Reconnect] button

LOADING STATES:
- Effect loading: Skeleton pulse on effect cards
- Parameters loading: Shimmer effect on sliders
- Chart loading: Animated placeholder bars
- Show loading: Timeline skeleton

ERROR STATES:
- Effect failed: Red border on effect card, error message
- Connection timeout: Amber warning banner with retry countdown
- Parameter out of range: Red slider thumb, error text below
- WebSocket disconnect: Fallback to polling indicator

EDGE CASES:
- Long effect names (truncate with ellipsis after 24 chars)
- 0 parameters effect (show "No adjustable parameters" message)
- All zones disabled (show warning in LED strip area)
- Maximum effects applied (45+ effects in grid, with scroll)

Add all states as separate Figma frames with clear labels.
```

### Prompt 11: Prototype & Handoff

```
Finalize LightwaveOS dashboard for developer handoff:

INTERACTIVE PROTOTYPE:
1. Tab navigation (all 4 tabs linked)
2. Effect selection → parameter panel population
3. Zone tab switching → zone config display
4. Slider hover → thumb enlargement
5. Button clicks → state changes
6. Dropdown open/close → overlay display
7. Transitions trigger → animation preview

MICRO-INTERACTIONS:
- 200ms fade transition between tabs
- 150ms scale on button hover (1.02×)
- 300ms slide for dropdown open
- 100ms ripple on click
- Smooth 300ms slider thumb drag

ANNOTATIONS:
- All spacing values (8px grid)
- Color token references (e.g., "bg.surface = #1A1F2E")
- Font specifications (Inter, JetBrains Mono)
- Component names matching shadcn/ui (Card, Slider, Select)

EXPORT SPECIFICATIONS:
- SVG icons at 24px, 20px, 16px sizes
- Color palette as exportable JSON
- Typography scale as CSS variables
- Component spec sheet (dimensions, padding, states)

Create developer handoff checklist in final Figma page.
```

---

## 6. Quality Evaluation Checklist

### Design System Adherence

- [ ] All colors use PRISM.node tokens (no arbitrary hex values)
- [ ] Typography uses Inter (UI) and JetBrains Mono (code)
- [ ] Spacing follows 8px grid (4, 8, 12, 16, 24, 32, 48, 64)
- [ ] Border radius consistent (4px, 8px, 12px)
- [ ] Shadows use defined elevation tokens

### Component Quality

- [ ] Interactive states defined (default, hover, active, disabled, error)
- [ ] Accessibility annotations present (contrast ratios, ARIA roles)
- [ ] Responsive behavior documented (breakpoints, layout shifts)
- [ ] Edge cases considered (empty states, loading, errors)
- [ ] Component maps to shadcn/ui primitives

### Domain-Specific

- [ ] CENTER ORIGIN marker visible (LED 79/80, cyan)
- [ ] Zone boundaries color-coded (Gold, Cyan, Emerald, Amber)
- [ ] No rainbow color cycling in any preview
- [ ] Performance thresholds correct (FPS, CPU, Memory)

### Prototype Quality

- [ ] All 4 tabs linked and functional
- [ ] 5+ key interaction flows demonstrated
- [ ] Micro-interactions defined (hover, click, transitions)
- [ ] Empty/error states covered

---

## 7. Iteration Strategy

### Phase Gates

| Phase | Gate Criteria | If Failed |
|-------|---------------|-----------|
| **Phase 1** | Design tokens match spec | Re-prompt with corrected values |
| **Phase 2** | Components have ≥3 states | Request specific additions |
| **Phase 3** | Layout works at 3 breakpoints | Add responsive prompts |
| **Phase 4** | Prototype shows 5+ flows | Add interaction links |

### Common Fix Prompts

**Color Mismatch**:
```
The colors don't match PRISM.node specification. Please update:
- bg.canvas should be #0A0E14 (not #1c2130)
- accent.primary should be #FFD700 (not #ffb84d)
Keep all other aspects the same.
```

**Missing States**:
```
The [Component] is missing hover and disabled states. Please add:
- Hover: 10% brightness increase, 200ms transition
- Disabled: 40% opacity, cursor not-allowed
Keep existing default and active states.
```

**Spacing Issues**:
```
The layout doesn't follow 8px grid. Please adjust:
- Card padding: 16px (not 20px)
- Section gap: 24px (not 30px)
- Margins: 16px (consistent)
```
