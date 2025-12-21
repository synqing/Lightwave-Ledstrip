# Figma Make Prompt Guide (Research-Backed)

> LightwaveOS Dashboard Wireframe Generation

## Sources

- [Figma Blog: 8 Essential Tips](https://www.figma.com/blog/8-ways-to-build-with-figma-make/)
- [UX Collective: 5 Ways to Improve Prompts](https://uxdesign.cc/how-to-prompt-figma-makes-ai-better-for-product-design-627daf3f4036)
- [UX Planet: Complete Guide](https://uxplanet.org/figma-make-complete-guide-a3bf0425537e)
- [Figma Developer Docs](https://developers.figma.com/docs/code/how-to-write-great-prompts/)
- [ADPList: Ultimate Prompt Guide](https://adplist.substack.com/p/ultimate-figma-make-prompt-guide)

---

## 1. Core Principles

**Key Finding**: Figma Make uses Claude Sonnet 4. Start comprehensive, iterate small.

| Principle | Application |
|-----------|-------------|
| **5-Part Structure** | Context → Description → Platform → Visual Style → UI Components |
| **Comprehensive Start** | One detailed prompt establishes design system + full layout |
| **Small Iterations** | Follow-ups for refinements, states, responsiveness |
| **Be Specific** | Exact grid specs, exact colors, exact component names |
| **Be Polite** | "Please" improves response quality |

---

## 2. Prompt Structure Template

```
# [Project Name] - [What You're Building]

## Context
[1-2 sentences: What is this? Who is it for?]

## Description
[Design priorities, constraints, things AI should consider]

## Platform
[Target platform + viewport dimensions]

## Visual Style
[Design tokens: colors, typography, spacing, shadows]

## UI Components
[Specific components with exact specifications]

## Constraints
[Accessibility requirements, forbidden patterns]
```

---

## 3. LightwaveOS: 3-Prompt Strategy

**Total: 3 prompts** (not 11!)

---

### PROMPT 1: Complete Dashboard Foundation

```
Please design a complete LED control dashboard called "LightwaveOS" for web (1920x1080).

## Context
Control interface for a dual-strip LED system (320 LEDs total) embedded in a Light Guide Plate.
Effects must radiate from the CENTER (LED 79/80), never edge-to-edge. Target users are makers
and lighting designers who need real-time control with performance monitoring.

## Description
- 4-tab navigation: Control+Zones | Shows | Effects | System
- LED strip visualization always visible (120px height, shows CENTER marker at LED 79/80 in cyan)
- Dark theme optimized for low-light environments
- Real-time performance metrics (FPS, CPU, memory)
- Multi-zone composition (up to 4 zones with independent effects)

## Platform
Web dashboard, desktop-first (1920x1080), with responsive breakpoints at 1280px and 768px.

## Visual Style (PRISM.node Design System)
COLORS:
- Canvas: #0A0E14 (main background)
- Surface: #1A1F2E (panels, cards)
- Elevated: #252B3D (modals, dropdowns)
- Primary Accent: #FFD700 (gold - CTAs, active states)
- Secondary Accent: #00CED1 (cyan - info, CENTER marker)
- Text: #E8E8E8 (primary), #A0A0A0 (secondary), #6B7280 (muted)
- Status: #10B981 (success/green), #F59E0B (warning/amber), #EF4444 (error/red)
- Zone Colors: #FFD700 (Zone 1), #00CED1 (Zone 2), #10B981 (Zone 3), #F59E0B (Zone 4)

TYPOGRAPHY:
- Headings: Inter, 600-700 weight
- Body: Inter, 400-500 weight
- Code/Values: JetBrains Mono, 400 weight

SPACING: 4px base grid (4, 8, 12, 16, 24, 32, 48, 64)
BORDER RADIUS: 4px (buttons), 8px (cards), 12px (modals)
SHADOWS: 0 2px 4px rgba(0,0,0,0.3) subtle, 0 4px 12px rgba(0,0,0,0.4) moderate

## UI Components

HEADER (64px):
- Logo "LightwaveOS" left-aligned, gold accent
- Connection status indicator (green dot = connected, red = disconnected)
- Settings gear icon (40x40px) right-aligned

TAB BAR (48px):
- 4 tabs with gold bottom border on active, muted text on inactive
- Tab labels: "Control + Zones", "Shows", "Effects", "System"

LED STRIP VISUALIZATION (120px, always visible):
- 320 LED dots (4px wide, 1px gap)
- Dual strip layout (Strip 1 top, Strip 2 bottom, 16px gap)
- CENTER marker at LED 79/80 with cyan vertical line and "CENTER" label
- Zone boundaries shown as dashed gold lines

TAB 1 - CONTROL + ZONES (3-column: 30% | 40% | 30%):
Left Column (Global Controls):
- Brightness slider (0-255)
- Speed slider (1-50)
- Palette dropdown (12 presets)
- Quick actions: [All Off] [Save Preset] buttons

Center Column (Zone Composer):
- Zone tabs (4 tabs, color-coded by zone color)
- Per-zone: Effect dropdown, Blend mode selector, Enable toggle
- Visual pipeline: Blur, Softness, Gamma sliders

Right Column (Quick Tools):
- Transitions section: Type selector (12 types), Duration slider, [Trigger] button
- Recent effects list (5 items)
- Favorites (starred effects)
- Mini FPS/memory display

TAB 2 - SHOWS (40% | 60% split):
Left: Show library cards (name, duration badge, scene count, thumbnail)
Right: Timeline with scene blocks, scrubber, keyframe markers
Bottom: Now Playing bar with transport controls

TAB 3 - EFFECTS (50% | 50% split):
Left: Search bar, category filter pills, 3-column effect cards grid with favorites
Right: Palette grid (12 presets), HSV sliders, color preview circle
Bottom: Mini LED preview, effect metadata, [Apply to Zone] button

TAB 4 - SYSTEM (2x2 grid):
- Performance: FPS chart, CPU gauge, memory bar (green/amber/red thresholds)
- Network: Connection status, IP, latency, WebSocket indicator
- Device: LED count, pin config, power limit, hardware info
- Firmware: Version, flash/RAM usage, [Reboot] [Factory Reset] buttons
- Collapsible sections below: WiFi config, Debug options, Serial monitor

## Constraints
- All interactive elements minimum 40x40px touch target
- WCAG AA contrast (4.5:1 for text)
- NO rainbow color cycling anywhere
- Slider thumbs: 16px circle with gold fill and shadow
- Buttons: Primary (gold fill), Secondary (gold border), Ghost (transparent)
```

---

### PROMPT 2: States, Interactions & Polish

```
Please add states and interactions to the LightwaveOS dashboard:

COMPONENT STATES:
- Buttons: default, hover (+10% brightness), active, disabled (40% opacity)
- Sliders: thumb enlarges to 20px on hover, glow effect on drag
- Cards: subtle border highlight on hover
- Tabs: underline animation on switch (200ms)

EMPTY STATES:
- No effect selected: "Select an effect to begin" with effect icon
- No shows: "Create your first show" with [+ New Show] button
- Disconnected: Warning banner with [Reconnect] button

LOADING STATES:
- Effect cards: Skeleton pulse animation
- Charts: Animated placeholder bars
- Parameters: Shimmer effect on sliders

ERROR STATES:
- Connection timeout: Amber warning banner with countdown
- Parameter out of range: Red slider thumb, error text below
- Effect failed: Red border on card with error message

MICRO-INTERACTIONS:
- 200ms fade between tabs
- 150ms scale on button hover (1.02x)
- 300ms slide for dropdowns
- 100ms ripple on click

Please show each state as a labeled variant.
```

---

### PROMPT 3: Responsive & Handoff

```
Please create responsive variants and developer handoff specs for LightwaveOS:

RESPONSIVE BREAKPOINTS:

Desktop (1920px) - Current design
Laptop (1280px):
- 3-column to 2-column layout
- Quick Tools moves to bottom drawer (pull-up sheet)
- LED strip scales to viewport width

Tablet (768px):
- Hamburger menu for tabs
- Single column, stacked sections
- Collapsible panels
- LED strip: horizontal scroll if needed

DEVELOPER ANNOTATIONS:
- All spacing values (reference 8px grid)
- Color token names (e.g., "bg.surface = #1A1F2E")
- Typography specs (Inter, JetBrains Mono with weights)
- Component mapping to shadcn/ui (Card, Slider, Select, Tabs, etc.)

PROTOTYPE CONNECTIONS:
- Tab navigation (all 4 tabs linked)
- Effect selection to parameter panel
- Zone tab switching to zone config display
- Dropdown open/close behavior

EXPORT CHECKLIST:
- [ ] Color palette as JSON
- [ ] Typography scale as CSS variables
- [ ] SVG icons at 24px, 20px, 16px
- [ ] Component spec sheet with dimensions
```

---

## 4. Iteration Guidelines

| Situation | Action |
|-----------|--------|
| **Colors wrong** | "Please update canvas to #0A0E14 and surface to #1A1F2E" |
| **Missing states** | "Add hover and disabled states to all buttons" |
| **Spacing off** | "Normalize spacing to 8px grid, card padding should be 16px" |
| **Layout broken** | "Keep 3-column layout with 30/40/30 split" |
| **Too cluttered** | "Make layout more minimal, increase spacing between sections" |
| **Need more detail** | "Add FPS thresholds: Green >=100, Amber 60-99, Red <60" |

---

## 5. Anti-Patterns to Avoid

| Don't | Do Instead |
|-------|------------|
| 11 separate prompts | 3 comprehensive + iterative prompts |
| Vague requests ("make a dashboard") | Specific specs with exact values |
| Fixing everything at once | Small, focused follow-ups |
| Starting over repeatedly | Iterate on existing design |
| Omitting design tokens | Include full color/typography specs |
| Skipping platform context | Specify viewport dimensions |

---

## 6. Quality Checklist

### Design System
- [ ] All colors match PRISM.node specification
- [ ] Typography uses Inter/JetBrains Mono with correct weights
- [ ] Spacing follows 8px grid
- [ ] Shadows use defined elevation tokens

### Components
- [ ] All 5 states defined (default, hover, active, disabled, error)
- [ ] WCAG AA contrast (4.5:1)
- [ ] Touch targets >= 40x40px
- [ ] Components consistent across all tabs

### Layout
- [ ] LED strip shows CENTER marker (LED 79/80, cyan)
- [ ] Zone boundaries are color-coded
- [ ] Works at 1920px, 1280px, 768px
- [ ] Grid alignment (16px margins, 8px gutters)

### Domain-Specific
- [ ] CENTER ORIGIN marker clearly visible
- [ ] NO rainbow color cycling anywhere
- [ ] Performance thresholds correct (FPS/CPU/Memory)
- [ ] Zone colors match spec (Gold, Cyan, Emerald, Amber)

---

## 7. Iteration Strategy

| Prompt | Gate Criteria | If Failed |
|--------|---------------|-----------|
| **Prompt 1** | Design tokens match, all 4 tabs visible, layout correct | Re-prompt with corrected values |
| **Prompt 2** | All states visible (hover, active, disabled, error, loading) | Request specific missing states |
| **Prompt 3** | Responsive layouts work at all 3 breakpoints | Add breakpoint-specific adjustments |

---

*Generated: December 2025*
*Based on official Figma Make documentation and community best practices*
