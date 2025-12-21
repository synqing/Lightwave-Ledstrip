# LightwaveOS UI Requirements Specification

> Dashboard design requirements based on K1.node1 analysis and Figma sample templates

## 1. Design Token System (PRISM.node)

### 1.1 Color Palette

```css
/* Structural Colors */
--prism-bg-canvas: #0A0E14;      /* Main background (deep charcoal) */
--prism-bg-surface: #1A1F2E;     /* Panel backgrounds */
--prism-bg-elevated: #252B3D;    /* Cards, modals, raised elements */
--prism-bg-overlay: rgba(10, 14, 20, 0.95);  /* Modal overlays */

/* Text Colors */
--prism-text-primary: #E8E8E8;   /* Main content */
--prism-text-secondary: #A0A0A0; /* Labels, metadata */
--prism-text-muted: #6B7280;     /* Placeholders, disabled */
--prism-text-inverse: #0A0E14;   /* For light backgrounds */

/* Accent Colors */
--prism-gold: #FFD700;           /* Primary accent (CTAs, active states) */
--prism-gold-hover: #FFC700;
--prism-gold-active: #FFB600;
--prism-info: #00CED1;           /* Secondary accent (info, status) */
--prism-info-hover: #00B8BB;
--prism-info-active: #00A3A6;

/* Semantic Colors */
--prism-success: #10B981;        /* Emerald */
--prism-success-bg: rgba(16, 185, 129, 0.1);
--prism-warning: #F59E0B;        /* Amber */
--prism-warning-bg: rgba(245, 158, 11, 0.1);
--prism-error: #EF4444;          /* Red */
--prism-error-bg: rgba(239, 68, 68, 0.1);

/* Border Colors */
--prism-border-default: #2D3748;
--prism-border-accent: rgba(255, 215, 0, 0.2);
--prism-border-focus: #FFD700;
--prism-border-error: #EF4444;

/* Zone Colors */
--prism-zone-1: #FFD700;         /* Gold */
--prism-zone-2: #00CED1;         /* Cyan */
--prism-zone-3: #10B981;         /* Emerald */
--prism-zone-4: #F59E0B;         /* Amber */

/* LED-Specific Colors */
--prism-led-off: #1A1F2E;
--prism-led-center: #00CED1;     /* CENTER origin marker */
```

### 1.2 Typography Scale

| Style | Font | Size | Weight | Line Height | Usage |
|-------|------|------|--------|-------------|-------|
| `h1` | Inter | 32px | 700 | 1.2 | Page titles |
| `h2` | Inter | 24px | 600 | 1.3 | Section headers |
| `h3` | Inter | 20px | 600 | 1.4 | Card titles |
| `h4` | Inter | 16px | 600 | 1.4 | Subsection headers |
| `body.large` | Inter | 16px | 400 | 1.5 | Primary content |
| `body.default` | Inter | 14px | 400 | 1.5 | UI text |
| `body.small` | Inter | 12px | 400 | 1.4 | Captions, labels |
| `code.default` | JetBrains Mono | 14px | 400 | 1.5 | Parameter values |
| `code.small` | JetBrains Mono | 12px | 400 | 1.4 | Inline code |
| `label.default` | Inter | 14px | 500 | 1.4 | Form labels |
| `label.small` | Inter | 12px | 500 | 1.4 | Small labels |

### 1.3 Spacing System

**Base Unit**: 4px

| Token | Value | Usage |
|-------|-------|-------|
| `space.1` | 4px | Tight spacing |
| `space.2` | 8px | Default gap |
| `space.3` | 12px | Component internal |
| `space.4` | 16px | Standard padding |
| `space.6` | 24px | Section spacing |
| `space.8` | 32px | Large gaps |
| `space.12` | 48px | Major sections |
| `space.16` | 64px | Page margins |

### 1.4 Border Radius

| Token | Value | Usage |
|-------|-------|-------|
| `radius.none` | 0px | Sharp edges |
| `radius.sm` | 4px | Buttons, inputs |
| `radius.md` | 8px | Cards, panels |
| `radius.lg` | 12px | Modals, major containers |
| `radius.xl` | 16px | Feature highlights |
| `radius.full` | 9999px | Pills, circular |

### 1.5 Elevation (Shadows)

| Token | Value | Usage |
|-------|-------|-------|
| `elevation.none` | none | Flat elements |
| `elevation.subtle` | `0 2px 4px rgba(0,0,0,0.3)` | Subtle depth |
| `elevation.moderate` | `0 4px 12px rgba(0,0,0,0.4)` | Cards |
| `elevation.elevated` | `0 8px 24px rgba(0,0,0,0.5)` | Modals, dropdowns |
| `elevation.overlay` | `0 16px 48px rgba(0,0,0,0.6)` | Full overlays |

---

## 2. Information Architecture (4-Tab Structure)

### 2.1 Tab Overview

```
LightwaveOS Dashboard (4 Tabs)
│
├── TAB 1: CONTROL + ZONES (Primary Interface)
│   └── 3-column layout: Global | Zone Composer | Quick Tools
│
├── TAB 2: SHOWS (ShowDirector - Dedicated)
│   └── 2-panel layout: Library (40%) | Timeline (60%)
│
├── TAB 3: EFFECTS LIBRARY (Effects + Palettes Combined)
│   └── 2-panel layout: Effects (50%) | Palettes (50%)
│
└── TAB 4: SYSTEM (Profiling + Network + Settings)
    └── 2x2 dashboard grid + collapsible sections
```

### 2.2 Tab 1: Control + Zones

**Layout**: 3-column (30% | 40% | 30%)

| Column | Components |
|--------|------------|
| **Left (30%)** | Brightness slider, Speed slider, Palette dropdown, Quick actions |
| **Center (40%)** | Zone tabs, Zone mixer, Visual pipeline sliders |
| **Right (30%)** | Transitions panel, Recent effects, Favorites, Mini performance |

**Key Design Decisions**:
- Transitions are **contextual actions**, not a standalone tab
- Zone tabs are color-coded (Gold, Cyan, Emerald, Amber)
- LED strip visualization is always visible above content

### 2.3 Tab 2: Shows (ShowDirector)

**Layout**: 2-panel (40% | 60%) + sticky Now Playing bar

| Panel | Components |
|-------|------------|
| **Left (40%)** | Show list cards, Search/filter, [+ New Show] button |
| **Right (60%)** | Timeline scrubber, Scene blocks, Narrative tension curve, Keyframes |
| **Bottom Bar** | Progress bar, Transport controls, Current scene |

### 2.4 Tab 3: Effects Library

**Layout**: 2-panel (50% | 50%) + shared preview

| Panel | Components |
|-------|------------|
| **Left (50%)** | Search bar, Category pills, Effect cards grid, Favorites |
| **Right (50%)** | Palette grid, Custom palette creator, HSV sliders |
| **Bottom** | Mini LED preview, Effect metadata, [Apply to Zone] |

### 2.5 Tab 4: System

**Layout**: 2x2 dashboard grid + collapsible sections

| Card | Content |
|------|---------|
| **Performance** | FPS chart, CPU gauge, Memory bar |
| **Network** | Connection status, IP, latency, WebSocket status |
| **Device** | LED count, Pin config, Power limit, Hardware |
| **Firmware** | Version, Build date, Flash/RAM usage, Reset/Reboot |

**Collapsible Sections**: WiFi config, Debug options, Serial monitor

---

## 3. Component Library (shadcn/ui)

### 3.1 Button Variants

| Variant | Background | Text | Border | Usage |
|---------|------------|------|--------|-------|
| **Primary** | Gold fill | Dark | None | Main CTAs |
| **Secondary** | Transparent | Gold | Gold | Secondary actions |
| **Ghost** | Transparent | Light | None | Tertiary actions |
| **Destructive** | Red fill | White | None | Dangerous actions |
| **Icon** | Transparent | Light | None | Icon-only buttons |

**States**: Default, Hover (+10% brightness), Active, Disabled (40% opacity)

### 3.2 Slider Component

| Element | Specification |
|---------|---------------|
| **Track** | 4px height, dark surface fill |
| **Progress** | Gold fill |
| **Thumb** | 16px diameter circle, gold, elevated shadow |
| **Hover Thumb** | 20px diameter, smooth scale transition |
| **Active Thumb** | 18px diameter, glow effect |
| **Labels** | Name (left), Value (right, monospace) |
| **Min/Max** | Below track, muted text |

### 3.3 Card Variants

| Variant | Background | Border | Shadow | Usage |
|---------|------------|--------|--------|-------|
| **Surface** | #1A1F2E | None | Subtle | Default cards |
| **Elevated** | #252B3D | None | Moderate | Modal content |
| **Bordered** | Transparent | 1px default | None | List items |

### 3.4 Input Components

| Type | Height | Background | Border | Usage |
|------|--------|------------|--------|-------|
| **Text** | 40px | Dark surface | 1px default | Text entry |
| **Select** | 40px | Dark surface | 1px default | Dropdowns |
| **Toggle** | 24px | Muted/Gold | None | On/off settings |
| **Checkbox** | 20px | Dark surface | 1px default | Multi-select |

### 3.5 Status Indicators

| Type | Size | Colors | Usage |
|------|------|--------|-------|
| **Dot** | 8px | Green/Amber/Red | Connection status |
| **Badge** | Pill | Status background | Counts, labels |
| **Progress** | 4px height | Fill percentage | Loading, usage |

---

## 4. Responsive Breakpoints

### 4.1 Desktop (≥1920px)

- Full 3-column layout for Control tab
- All panels visible simultaneously
- LED strip at full 1600px width

### 4.2 Laptop (1280-1919px)

- 3-column → 2-column (Quick Tools in bottom drawer)
- LED strip scales to viewport
- Sidebar collapsible to icons

### 4.3 Tablet (768-1279px)

- Single column, stacked sections
- Hamburger menu for tabs
- LED strip horizontal scroll if needed
- Collapsible panels

### 4.4 Mobile (<768px) - Optional

- Bottom tab bar (icons only)
- LED strip simplified (every 4th LED)
- Full-width stacked layout
- Modal sheets for parameters

---

## 5. Accessibility Requirements (WCAG AA)

### 5.1 Color Contrast

| Element | Minimum Ratio |
|---------|---------------|
| Body text | 4.5:1 |
| Large text (≥18px) | 3:1 |
| UI components | 3:1 |
| Focus indicators | 3:1 |

### 5.2 Interactive Elements

| Requirement | Specification |
|-------------|---------------|
| **Touch targets** | ≥40×40px minimum |
| **Focus rings** | 2px gold outline |
| **Keyboard navigation** | Full tab/arrow support |
| **Reduced motion** | Respect `prefers-reduced-motion` |

### 5.3 Screen Reader Support

- Semantic HTML (`<main>`, `<nav>`, `<button>`)
- ARIA roles and labels for all icons
- Live regions for dynamic content
- Skip links for navigation

---

## 6. Animation Tokens

### 6.1 Duration Scale

| Token | Value | Usage |
|-------|-------|-------|
| `duration.instant` | 100ms | Micro-feedback |
| `duration.fast` | 200ms | Button hover |
| `duration.normal` | 300ms | Panel transitions |
| `duration.slow` | 500ms | Complex animations |

### 6.2 Easing Functions

| Token | Value | Usage |
|-------|-------|-------|
| `easing.default` | `cubic-bezier(0.4, 0.0, 0.2, 1)` | Standard |
| `easing.in` | `cubic-bezier(0.4, 0.0, 1, 1)` | Enter |
| `easing.out` | `cubic-bezier(0.0, 0.0, 0.2, 1)` | Exit |

---

## 7. Progressive Disclosure

### 7.1 Layer Visibility

| Layer | Always Visible | Default Expanded | Collapsed |
|-------|----------------|------------------|-----------|
| **Control** | LED viz, zone tabs | Zone mixer, transitions | Pipeline sliders |
| **Shows** | Show list, now playing | Timeline, scenes | Tension curve |
| **Effects** | Search, categories | Effect cards, HSV | Metadata, custom |
| **System** | 4 dashboard cards | None | WiFi, debug, serial |

---

## 8. Domain-Specific Requirements

### 8.1 LED Visualization

| Requirement | Specification |
|-------------|---------------|
| **LED count** | 320 (2×160 strips) |
| **LED size** | 4px width, 1px gap |
| **CENTER marker** | LED 79/80, cyan line + label |
| **Zone boundaries** | Dashed gold lines |
| **Update rate** | 30 FPS (WebSocket sync) |

### 8.2 Forbidden Patterns

| Pattern | Reason |
|---------|--------|
| **Rainbow color cycling** | Explicitly forbidden by project mandate |
| **Linear L→R effects** | Violates CENTER ORIGIN constraint |
| **Edge-originating effects** | Look wrong on LGP hardware |

### 8.3 Performance Thresholds

| Metric | Green | Amber | Red |
|--------|-------|-------|-----|
| FPS | ≥100 | 60-99 | <60 |
| CPU | 0-50% | 51-80% | >80% |
| Memory | <70% | 70-85% | >85% |
| Render Time | <10ms | 10-16ms | >16ms |
