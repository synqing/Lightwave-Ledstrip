# Current Tab5.encoder Dashboard - Visual Description

**Purpose**: This document describes exactly what the current dashboard looks like visually, based on the actual implementation code. This is for reference when evaluating mockups - if a mockup looks like this, it has FAILED the redesign task.

---

## Screen Dimensions & Layout

- **Resolution**: 1280×720px (M5Stack Tab5 5" display)
- **Background Color**: `#0A0A0B` (very dark gray, almost black)
- **Layout Strategy**: Fixed-position containers with grid/flexbox layouts
- **Spacing Constants**:
  - Header/Footer Height: 66px
  - Grid Gap: 14px (between cards)
  - Grid Margin: 24px (side margins)

---

## Main Dashboard (Global View)

### 1. Header Bar (Top, Fixed)

**Position**: (10, 7) from top-left, size 1260×66px
**Background**: `#1A1A1C` (elevated dark gray)
**Border**: 2px white, 14px radius
**Padding**: 24px left/right, 16px top/bottom

**Content Layout** (Flexbox row, left to right):

1. **Branding** (Left):
   - "LIGHTWAVE" - White, Bebas Neue 40px
   - "OS" - Yellow `#FFC700`, Bebas Neue 40px
   - 54px gap after branding

2. **Effect Name** (Middle-left):
   - Container: 300px wide × 24px tall
   - Text: Current effect name (e.g., "Neon Pulse V2")
   - Gray `#9CA3AF`, Bebas Neue 24px
   - Circular scroll if text too long
   - 48px gap after effect

3. **Palette Name** (Middle-left):
   - Container: 280px wide × 24px tall
   - Text: Current palette name (e.g., "Cyber Night")
   - Gray `#9CA3AF`, Bebas Neue 24px
   - Circular scroll if text too long
   - Flexible spacer pushes next items right

4. **Network Info** (Right):
   - SSID: Gray `#9CA3AF`, Bebas Neue 24px, 18px left padding
   - RSSI: Color-coded (green >-50, yellow -50 to -70, red <-70), Bebas Neue 24px, format "(-42 dBm)", 4px left padding
   - IP Address: White `#FFFFFF`, Bebas Neue 24px, format "192.168.1.105", 8px left padding

5. **RETRY Button** (Hidden by default):
   - "RETRY" - White, Rajdhani Medium 24px
   - Clickable label (no button widget)
   - Shows conditionally

**Visual Style**: Elevated dark gray bar with white border, rounded corners. Text is tightly packed horizontally with minimal spacing.

---

### 2. Parameter Gauge Row (First Content Row)

**Position**: Below header, centered, 24px side margins
**Container Size**: 1232px wide (1280 - 48px) × 125px tall
**Layout**: 8-column grid, equal width columns, 14px gaps

**Each Gauge Card** (8 identical cards):

**Card Properties:**
- Background: `#121214` (base dark gray)
- Border: 2px white
- Radius: 14px
- Padding: 10px all sides
- Size: ~147px wide × 125px tall (after gaps)

**Content (Top to Bottom):**

1. **Label** (Top):
   - Parameter name: "EFFECT", "PALETTE", "SPEED", "MOOD", "FADE", "COMPLEXITY", "VARIATION", "BRIGHTNESS"
   - Gray `#9CA3AF`, Bebas Bold 24px
   - Position: Top center, 0px from top

2. **Value** (Center):
   - Numeric value: "42", "128", "25", etc.
   - White `#FFFFFF`, JetBrains Mono Regular 32px
   - Position: Top center, 30px from top

3. **Progress Bar** (Bottom):
   - Background: Dark gray `#2A2A2E`, 90% width, 10px height, 8px radius
   - Fill: Yellow `#FFC700` (brand color), represents value 0-255
   - Position: Bottom center, 10px from bottom

**Visual State:**
- **Default**: White border (`#FFFFFF`)
- **Highlighted** (when encoder adjusted): Yellow border (`#FFC700`) for 300ms

**Parameter Order** (Left to Right):
1. EFFECT
2. PALETTE
3. SPEED
4. MOOD
5. FADE
6. COMPLEXITY
7. VARIATION
8. BRIGHTNESS

**Visual Style**: Grid of identical rectangular cards. All same size, same style, uniform appearance. No visual hierarchy between parameters. Horizontal progress bars at bottom.

---

### 3. Preset Bank Row (Second Content Row)

**Position**: Below parameter row, 14px gap
**Container Size**: 1232px wide × 85px tall
**Layout**: 8-column grid, equal width columns, 14px gaps

**Each Preset Slot Card** (8 identical cards):

**Card Properties:**
- Background: `#1A1A1C` (elevated dark gray) - slightly lighter than parameter cards
- Border: 2px colored border (state-dependent)
- Radius: 14px
- Padding: 10px all sides
- Size: ~147px wide × 85px tall

**Border Colors by State:**
- **Empty**: White `#FFFFFF`
- **Saved (Occupied)**: Yellow `#FFC700`
- **Active (Currently Recalled)**: Green `#00FF99`
- **Saving Feedback**: Light yellow `#FFE066` (600ms flash)
- **Recall Feedback**: Green `#00FF99` (600ms flash)
- **Delete Feedback**: Red `#FF3355` (600ms flash)

**Content (Top to Bottom):**

1. **Label** (Top):
   - "P1", "P2", ... "P8"
   - Gray `#9CA3AF`, Bebas Neue 24px
   - Position: Top center, 0px from top

2. **Value** (Below label):
   - Format: "E## P## ###" (Effect ID, Palette ID, Brightness)
   - Example: "E42 P15 180"
   - White `#FFFFFF`, Bebas Neue 24px
   - Position: Top center, 28px from top
   - Shows "--" if empty

**Visual Style**: Smaller cards than parameter row, elevated background. Border color indicates state. No icons or visual indicators beyond border color.

---

### 4. Action Row (Third Content Row)

**Position**: Below preset row, 14px gap
**Container Size**: 1232px wide × 100px tall
**Layout**: 5-column grid, equal width columns, 14px gaps

**Each Action Button** (5 cards):

**Card Properties:**
- Background: `#121214` (base dark gray)
- Border: 2px white (default) or yellow `#FFC700` (when active/enabled)
- Radius: 14px
- Padding: 10px all sides
- Clickable: Yes (clickable card, not button widget)
- Size: ~235px wide × 100px tall

**Content (Top to Bottom):**

1. **Label** (Top):
   - Action name: "GAMMA", "COLOUR", "EXPOSURE", "BROWN", "ZONES"
   - Gray `#9CA3AF`, Rajdhani Medium 24px
   - Position: Top center, 0px from top

2. **Value** (Below label):
   - GAMMA: "2.2", "2.5", "2.8", or "OFF" - White, JetBrains Mono Bold 32px
   - COLOUR: "OFF", "HSV", "RGB", or "BOTH" - White, Rajdhani Bold 32px
   - EXPOSURE: "ON" or "OFF" - White, Rajdhani Bold 32px
   - BROWN: "ON" or "OFF" - White, Rajdhani Bold 32px
   - ZONES: (Opens Zone Composer when clicked)
   - Position: Top center, 28px from top

**Border Color States:**
- **Inactive/Disabled**: White `#FFFFFF`
- **Active/Enabled**: Yellow `#FFC700`

**Visual Style**: Same card style as parameter gauges. Slightly taller than preset slots. Border color indicates on/off state. ZONES button switches to Zone Composer screen.

**Action Order** (Left to Right):
1. GAMMA
2. COLOUR
3. EXPOSURE
4. BROWN
5. ZONES

---

### 5. Footer Bar (Bottom, Fixed)

**Position**: (3, 651) from top-left, size 1274×66px (1280 - 6px for border/padding)
**Background**: `#1A1A1C` (elevated dark gray)
**Border**: 2px white, 14px radius
**Padding**: 24px left/right, 16px top/bottom
**Layout**: Flexbox row, space-between alignment

**Left Group** (Flexbox row, 40px gaps between items):

1. **BPM** (Container: 95px × 24px):
   - "BPM:" - Gray `#9CA3AF`, Rajdhani Medium 24px, 2px right padding
   - Value: "120" or "--" - Gray `#9CA3AF`, Rajdhani Medium 24px

2. **KEY** (Container: 112px × 24px):
   - "KEY:" - Gray `#9CA3AF`, Rajdhani Medium 24px, 2px right padding
   - Value: "C#min" or "--" - Gray `#9CA3AF`, Rajdhani Medium 24px

3. **MIC** (Container: 125px × 24px):
   - "MIC:" - Gray `#9CA3AF`, Rajdhani Medium 24px, 2px right padding
   - Value: "-46.4 DB" or "--" - Gray `#9CA3AF`, Rajdhani Medium 24px

4. **UPTIME** (Container: 145px × 24px):
   - "UPTIME:" - Gray `#9CA3AF`, Rajdhani Medium 24px, 2px right padding
   - Value: "1h 24m 32s" or "0s" - Gray `#9CA3AF`, Rajdhani Medium 24px

**Right Group** (Fixed width: 345px × 24px, 24px gap between items):

5. **WS Status**:
   - "WS: OK" (green `#00FF00`), "WS: OFF" (gray), "WS: ..." (yellow), "WS: ERR" (red)
   - Rajdhani Medium 24px

6. **Battery** (Container: flex row, 8px gap):
   - Label: "BATTERY: 85%" - Color-coded (green >50%, yellow 20-50%, red <20%), Rajdhani Medium 24px
   - Bar: 60px wide × 8px tall, 4px radius
     - Background: Dark gray `#2A2A2E`
     - Fill: Color-coded (same as text), 0-100%

**Visual Style**: Same as header - elevated dark gray bar with white border. Information-dense with many metrics in small space. All text is same size (24px). No visual hierarchy between metrics.

---

## Zone Composer Screen (Secondary View)

### Access
Accessed via "ZONES" button on main dashboard. Completely separate screen with own rendering.

### Layout Structure

**Screen Background**: `#0A0A0B` (same as main dashboard)
**Note**: Header is shared/reused from main dashboard (rendered separately by DisplayUI)

### 1. LED Strip Visualization (Top Section)

**Position**: (40, 140) from top, size 1200×80px (below header)
**Title**: "LED STRIP VISUALIZATION" - White, Font4 (32px), centered above

**Strip Labels**:
- "Left (0-79)" - Gray, Font2 (18px), left-aligned
- "Right (80-159)" - Gray, Font2 (18px), right-aligned

**Visualization**:
- **Strip Height**: 60px
- **LED Width**: ~7px per LED (calculated from 1200px width for 160 LEDs)
- **Center Gap**: 8px wide (cyan accent color `#6EE7F3`)
- **Center Position**: x = 640px (screen center)

**Left Strip** (LEDs 0-79, reversed):
- LEDs drawn right-to-left (LED 79 at center, LED 0 at left edge)
- Each LED colored by zone:
  - Zone 0: Cyan `#6EE7F3`
  - Zone 1: Green `#22DD88`
  - Zone 2: Orange `#FFB84D`
  - Zone 3: Purple `#9D4EDD`
- LED 79 (center) has white border highlight

**Right Strip** (LEDs 80-159, normal order):
- LEDs drawn left-to-right from center (LED 80 at center, LED 159 at right edge)
- Same color coding as left strip
- LED 80 (center) has white border highlight

**Center Divider**:
- 8px wide cyan bar at screen center
- Label below: "Centre pair: LEDs 79 (left) / 80 (right)" - Gray, Font2 (18px)

**Visual Style**: Simple colored bars showing which LEDs belong to which zone. Functional but not visually interesting. No gradients, no labels on zones, just colored rectangles.

---

### 2. Zone List (Middle Section)

**Position**: (40, 260) from top, size 1200×320px
**Label**: "Zone Controls" - Gray, Font2 (18px), left-aligned above

**Layout**: Vertical list of zone rows, evenly divided height (320px / zone count)

**Each Zone Row**:

**Card Properties:**
- Background: `#0841` (dark gray panel)
- Border: Zone color (Cyan/Green/Orange/Purple)
- Height: 320px / zone count (minimum 40px)

**Content** (Left to right, all Font2 18px):

1. **Zone Title** (x + 10px):
   - "Zone 0", "Zone 1", "Zone 2", "Zone 3"
   - White, Font2 (18px), middle-left aligned

2. **LED Range** (x + 100px):
   - Format: "LED 0-40 / 120-159" (left start-end / right start-end)
   - Gray, Font2 (18px), middle-left aligned

3. **Effect Name** (x + 300px):
   - Effect name from server or "Effect #42"
   - Gray, Font2 (18px), middle-left aligned

4. **Palette Name** (x + 500px):
   - Palette name from server or "Palette #15"
   - Gray, Font2 (18px), middle-left aligned

5. **Blend Mode** (x + 700px):
   - Blend mode name or "Blend #0"
   - Gray, Font2 (18px), middle-left aligned

**Visual Style**: Horizontal rows with colored borders. All text same size (18px). Text-heavy, information-dense. No clear visual distinction for selected/active zone. No parameter values (Effect/Palette/Speed/Brightness) visible in read-only M5GFX rendering.

**Note**: There's also an LVGL interactive UI (`createInteractiveUI`) that shows clickable parameter controls (Effect, Palette, Speed, Brightness) per zone, but the M5GFX rendering (legacy) only shows the read-only zone list above.

---

### 3. Zone Info Display (Bottom Section)

**Position**: (40, 600) from top, size 1200×180px
**Content**:
- "Zones: 3" - Gray label, White value, Font2 (18px)
- "Layout: Centre-out" - Gray, Font2 (18px)

**Visual Style**: Simple text display, minimal information. Mostly empty space.

---

## Color Palette (Current Implementation)

### Backgrounds
- **Page Background**: `#0A0A0B` (very dark gray, almost black)
- **Surface Base**: `#121214` (dark gray)
- **Surface Elevated**: `#1A1A1C` (lighter dark gray)
- **Border Base**: `#2A2A2E` (medium gray)

### Text Colors
- **Primary**: `#FFFFFF` (white)
- **Secondary**: `#9CA3AF` (gray)

### Brand/Accent
- **Brand Primary**: `#FFC700` (yellow/gold) - Used for:
  - "OS" branding
  - Active/highlighted states
  - Progress bar fills
  - Preset saved state borders
  - Action button active borders

### Status Colors
- **Green**: `#00FF00` (WebSocket OK, battery >50%, RSSI >-50, preset active)
- **Red**: `#FF0000` (WebSocket error, battery <20%, RSSI <-70, delete feedback)
- **Yellow**: `#FFC700` (same as brand - connecting, battery 20-50%, RSSI -50 to -70)

### Zone Colors (Zone Composer)
- **Zone 0**: `#6EE7F3` (Cyan)
- **Zone 1**: `#22DD88` (Green)
- **Zone 2**: `#FFB84D` (Orange)
- **Zone 3**: `#9D4EDD` (Purple)

### Preset State Colors
- **Empty**: `#FFFFFF` (white border)
- **Saved**: `#FFC700` (yellow border)
- **Active**: `#00FF99` (green border)
- **Saving**: `#FFE066` (light yellow, 600ms flash)
- **Deleting**: `#FF3355` (red, 600ms flash)

---

## Typography (Current Implementation)

### Fonts Used
1. **Bebas Neue** (Brand font):
   - 24px: Parameter labels, preset labels, effect/palette names in header
   - 40px: "LIGHTWAVE" and "OS" branding
   - Bold variant: Parameter names

2. **Rajdhani** (Secondary font):
   - Medium 24px: Footer labels, action button labels, header retry button
   - Bold 32px: Action button values (text)

3. **JetBrains Mono** (Monospace):
   - Regular 32px: Parameter numeric values
   - Bold 32px: GAMMA numeric value

4. **M5GFX Fonts** (Zone Composer M5GFX rendering):
   - Font2 (18px): Zone list, LED visualization labels
   - Font4 (32px): "LED STRIP VISUALIZATION" title

### Typography Issues
- **Limited Size Variation**: Only 24px, 32px, 40px sizes used
- **Small Size Differences**: 8px difference between label (24px) and value (32px) is subtle
- **No Clear Hierarchy**: All labels same size, all values same size (except action buttons)
- **Monospace Overuse**: Parameter values use monospace (good) but could benefit from larger sizes for importance

---

## Visual Hierarchy (Current State)

### Problems Identified:

1. **Flat Hierarchy**: All parameter cards identical size and style - no indication of importance
2. **Uniform Spacing**: 14px gaps everywhere - lacks rhythm or emphasis
3. **Same Card Style**: Parameter cards, preset cards, action cards all use same visual treatment (only background color differs slightly)
4. **Weak State Feedback**: Border color changes are subtle - not obvious when active
5. **Footer Overload**: Too many metrics in footer - information density is high
6. **Generic Appearance**: Looks like every other embedded system dashboard - no personality
7. **Limited Visual Interest**: White borders on gray backgrounds - boring
8. **No Visual Breathing Room**: Everything is packed tightly with minimal whitespace
9. **Weak Brand Presence**: Branding is small in header, not prominent
10. **Parameter Importance**: All 8 parameters treated equally - no visual distinction

### Layout Structure:
```
┌─────────────────────────────────────────────────────────┐
│ HEADER (66px)                                           │
│ [LIGHTWAVE OS] [Effect Name] [Palette] ... [Network]   │
├─────────────────────────────────────────────────────────┤
│                                                         │
│ PARAMETER ROW (125px)                                   │
│ [EFFECT] [PALETTE] [SPEED] [MOOD] [FADE] [COMPLEX] ... │
│   Card     Card     Card    Card    Card     Card       │
│                                                         │
│ PRESET ROW (85px)                                       │
│ [P1] [P2] [P3] [P4] [P5] [P6] [P7] [P8]                │
│                                                         │
│ ACTION ROW (100px)                                      │
│ [GAMMA] [COLOUR] [EXPOSURE] [BROWN] [ZONES]            │
│                                                         │
├─────────────────────────────────────────────────────────┤
│ FOOTER (66px)                                           │
│ [BPM] [KEY] [MIC] [UPTIME] ... [WS] [BATTERY]          │
└─────────────────────────────────────────────────────────┘
```

**Layout Issues**:
- Rigid grid structure (everything in rows)
- All cards same width (equal columns)
- No visual grouping or sectioning
- No breathing room between sections
- Footer crammed with metrics

---

## Zone Composer Layout Structure

```
┌─────────────────────────────────────────────────────────┐
│ HEADER (66px) - Shared with main dashboard             │
├─────────────────────────────────────────────────────────┤
│                                                         │
│ LED STRIP VISUALIZATION (80px height)                  │
│ [Left 0-79]  [Centre]  [Right 80-159]                  │
│ ████████████ ████ ████████████                         │
│                                                         │
│ ZONE LIST (320px height)                               │
│ Zone 0: LED 0-40 / 120-159  Effect Name  Palette ...  │
│ Zone 1: LED ...          Effect Name  Palette ...      │
│ Zone 2: ...                                              │
│ Zone 3: ...                                              │
│                                                         │
│ ZONE INFO (180px height)                               │
│ Zones: 3  Layout: Centre-out                            │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

**Zone Composer Issues**:
- LED visualization is just colored bars - not informative
- Zone rows are text-heavy with all info crammed horizontally
- No clear visual distinction for selected zone
- No parameter controls visible in read-only view (Effect/Palette/Speed/Brightness hidden in text)
- Zone info section is mostly empty space
- No clear indication of which parameter mode is active (Effect/Palette/Speed/Brightness)

**Note**: There's an LVGL interactive UI (`createInteractiveUI`) that adds clickable parameter controls, but it's not fully visible in the M5GFX legacy rendering path.

---

## Interaction & Feedback (Current State)

### Encoder Feedback
- **Parameter Adjustment**: Border changes to yellow `#FFC700` for 300ms when encoder turned
- **No Visual Scale**: All parameters flash the same - no indication of adjustment magnitude

### Preset Feedback
- **Save**: Yellow flash `#FFE066` for 600ms
- **Recall**: Green flash `#00FF99` for 600ms
- **Delete**: Red flash `#FF3355` for 600ms
- **Feedback Duration**: 600ms (longer than parameter highlight)

### Action Button Feedback
- **Active State**: Border changes to yellow `#FFC700`
- **Inactive State**: White border `#FFFFFF`
- **No Press Feedback**: No visual feedback when tapped/pressed

### Touch Targets
- **Action Buttons**: ~235px wide × 100px tall (good size)
- **Preset Slots**: ~147px wide × 85px tall (adequate)
- **Parameter Gauges**: ~147px wide × 125px tall (visual only, not touchable)

---

## Overall Visual Assessment

### Strengths (What Works):
- ✅ Clean grid layout (functional, not cluttered)
- ✅ Consistent spacing (14px gaps)
- ✅ Readable typography (Bebas Neue, JetBrains Mono are clear)
- ✅ Color coding for states (green/yellow/red semantics)
- ✅ Functional feedback (border color changes work)

### Weaknesses (What Needs Improvement):
- ❌ **No Visual Hierarchy**: Everything same size, no emphasis
- ❌ **Generic Appearance**: Looks like 2000s embedded UI, not 2024 design
- ❌ **Boring Visuals**: White borders on gray - no depth or interest
- ❌ **Weak Brand Presence**: Small branding, doesn't stand out
- ❌ **Information Density**: Footer overloaded, no grouping
- ❌ **Uniform Spacing**: No rhythm or breathing room
- ❌ **Subtle Feedback**: State changes are too subtle
- ❌ **No Personality**: Generic "dashboard" feel
- ❌ **Limited Typography Scale**: Only 3 sizes used (24px, 32px, 40px)
- ❌ **Same Card Style**: All cards look identical (just background color differs)

### Critical Issues for Redesign:
1. **8 Equal Cards in Row**: No visual importance distinction
2. **White Borders Everywhere**: Boring, generic
3. **Same Font Sizes**: Limited hierarchy
4. **Footer Overload**: Too much crammed in
5. **Zone Composer Text-Heavy**: No visual parameter controls
6. **No Visual Interest**: Flat, no depth, no gradients, no shadows
7. **Weak State Visualization**: Border color changes are subtle

---

## What a Successful Redesign Should Address

A successful redesign should:

1. ✅ **Break the Grid**: Not just 8 equal cards in a row
2. ✅ **Strong Typography Hierarchy**: Dramatic size differences (e.g., 16px labels, 56px values)
3. ✅ **Purposeful Color**: Each parameter could have its own color identity
4. ✅ **Visual Breathing Room**: Larger gaps, whitespace as design element
5. ✅ **Professional Depth**: Subtle gradients, shadows (if LVGL supports), layering
6. ✅ **Clear State Feedback**: Obvious active/highlighted states (not subtle)
7. ✅ **Grouped Information**: Related metrics grouped visually
8. ✅ **Brand Personality**: Distinctive look, not generic
9. ✅ **Innovative Layout**: Asymmetric, variable sizes, modern patterns
10. ✅ **Zone Composer Clarity**: Clear parameter controls, obvious selection

**If a mockup looks like the current dashboard, it has FAILED. Show something RADICALLY DIFFERENT and PROFESSIONAL.**

---

## ASCII Layout Diagrams

### Main Dashboard (Global View) - 1280×720px

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ SCREEN: 1280×720px                                                                                          │
│ Background: #0A0A0B (very dark gray)                                                                        │
├─────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                                               │
│ ┌─────────────────────────────────────────────────────────────────────────────────────────────────────────┐ │
│ │ HEADER BAR (66px height, 1260px wide, pos: 10,7)                                                         │ │
│ │ ┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐ │ │
│ │ │ [LIGHTWAVE] [OS] │ [Effect Name...] │ [Palette Name...] │ ... │ [SSID] (RSSI) [IP] │ [RETRY]      │ │ │
│ │ │   White  Yellow  │    Gray 24px     │    Gray 24px      │     │  Gray  Color  White │  White      │ │ │
│ │ │  40px    40px    │    Scroll        │    Scroll         │     │  24px   24px  24px  │  24px       │ │ │
│ │ └─────────────────────────────────────────────────────────────────────────────────────────────────────┘ │ │
│ │ Background: #1A1A1C | Border: 2px white | Radius: 14px | Padding: 24px L/R, 16px T/B                    │ │
│ └─────────────────────────────────────────────────────────────────────────────────────────────────────────┘ │
│                                                                                                               │
│ ┌─────────────────────────────────────────────────────────────────────────────────────────────────────────┐ │
│ │ PARAMETER GAUGE ROW (125px height, 1232px wide, 14px gaps)                                              │ │
│ │ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐                            │ │
│ │ │EFFECT│ │PALETT│ │SPEED │ │ MOOD │ │ FADE │ │COMPLX│ │VARIA │ │BRIGHT│                            │ │
│ │ │      │ │E     │ │      │ │      │ │      │ │ITY   │ │TION  │ │NESS  │                            │ │
│ │ │  42  │ │ 128  │ │  25  │ │ 180  │ │  10  │ │ 250  │ │  80  │ │ 255  │                            │ │
│ │ │      │ │      │ │      │ │      │ │      │ │      │ │      │ │      │                            │ │
│ │ │██████│ │██████│ │███   │ │██████│ │█     │ │██████│ │███   │ │██████│                            │ │
│ │ └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ └──────┘                            │ │
│ │  Card 1   Card 2   Card 3   Card 4   Card 5   Card 6   Card 7   Card 8                              │ │
│ │ ~147px × 125px each | Background: #121214 | Border: 2px white | 14px radius                         │ │
│ │ Label: 24px gray | Value: 32px white mono | Bar: 10px yellow fill                                    │ │
│ └─────────────────────────────────────────────────────────────────────────────────────────────────────────┘ │
│                                                                                                               │
│ ┌─────────────────────────────────────────────────────────────────────────────────────────────────────────┐ │
│ │ PRESET BANK ROW (85px height, 1232px wide, 14px gaps)                                                  │ │
│ │ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐                            │ │
│ │ │  P1  │ │  P2  │ │  P3  │ │  P4  │ │  P5  │ │  P6  │ │  P7  │ │  P8  │                            │ │
│ │ │      │ │      │ │      │ │      │ │      │ │      │ │      │ │      │                            │ │
│ │ │E42   │ │E03   │ │  --  │ │  --  │ │  --  │ │E09   │ │  --  │ │  --  │                            │ │
│ │ │P15   │ │P12   │ │      │ │      │ │      │ │P99   │ │      │ │      │                            │ │
│ │ │ 180  │ │ 128  │ │      │ │      │ │      │ │ 200  │ │      │ │      │                            │ │
│ │ └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ └──────┘                            │ │
│ │  Card 1   Card 2   Card 3   Card 4   Card 5   Card 6   Card 7   Card 8                              │ │
│ │ ~147px × 85px each | Background: #1A1A1C | Border: 2px (white/yellow/green) | 14px radius          │ │
│ │ Label: 24px gray | Value: 24px white | Border color = state (empty/saved/active)                   │ │
│ └─────────────────────────────────────────────────────────────────────────────────────────────────────────┘ │
│                                                                                                               │
│ ┌─────────────────────────────────────────────────────────────────────────────────────────────────────────┐ │
│ │ ACTION ROW (100px height, 1232px wide, 14px gaps)                                                       │ │
│ │ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐                                      │ │
│ │ │  GAMMA   │ │  COLOUR  │ │ EXPOSURE │ │  BROWN   │ │  ZONES   │                                      │ │
│ │ │          │ │          │ │          │ │          │ │          │                                      │ │
│ │ │   2.2    │ │   HSV    │ │   OFF    │ │   ON     │ │    -->   │                                      │ │
│ │ └──────────┘ └──────────┘ └──────────┘ └──────────┘ └──────────┘                                      │ │
│ │   Card 1       Card 2       Card 3       Card 4       Card 5                                          │ │
│ │ ~235px × 100px each | Background: #121214 | Border: 2px (white/yellow) | 14px radius                │ │
│ │ Label: 24px gray | Value: 32px white (mono for GAMMA, bold for others)                               │ │
│ └─────────────────────────────────────────────────────────────────────────────────────────────────────────┘ │
│                                                                                                               │
│ ┌─────────────────────────────────────────────────────────────────────────────────────────────────────────┐ │
│ │ FOOTER BAR (66px height, 1274px wide, pos: 3,651)                                                       │ │
│ │ ┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐ │ │
│ │ │ [BPM: 120] [KEY: C#min] [MIC: -46.4 DB] [UPTIME: 1h 24m] ... [WS: OK] [BATTERY: 85% ████████]    │ │ │
│ │ │   Gray      Gray          Gray            Gray              Green    Green text + bar            │ │ │
│ │ │  24px      24px          24px            24px              24px     24px label + 60px bar        │ │ │
│ │ └─────────────────────────────────────────────────────────────────────────────────────────────────────┘ │ │
│ │ Background: #1A1A1C | Border: 2px white | Radius: 14px | Padding: 24px L/R, 16px T/B                  │ │
│ └─────────────────────────────────────────────────────────────────────────────────────────────────────────┘ │
│                                                                                                               │
└─────────────────────────────────────────────────────────────────────────────────────────────────────────────┘

Layout Summary:
- Header: 66px
- Parameter Row: 125px
- Gap: 14px
- Preset Row: 85px
- Gap: 14px
- Action Row: 100px
- Gap: (remaining space)
- Footer: 66px
Total: 66 + 125 + 14 + 85 + 14 + 100 + 66 = 470px (leaves 250px gap between action row and footer)
```

### Zone Composer Screen - 1280×720px

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ SCREEN: 1280×720px                                                                                          │
│ Background: #0A0A0B (very dark gray)                                                                        │
├─────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                                               │
│ ┌─────────────────────────────────────────────────────────────────────────────────────────────────────────┐ │
│ │ HEADER BAR (66px height) - Shared with main dashboard                                                    │ │
│ │ [LIGHTWAVE] [OS] │ [Effect Name] │ [Palette Name] │ ... │ [Network Info] │ [< BACK]                   │ │
│ └─────────────────────────────────────────────────────────────────────────────────────────────────────────┘ │
│                                                                                                               │
│ ┌─────────────────────────────────────────────────────────────────────────────────────────────────────────┐ │
│ │ LED STRIP VISUALIZATION (80px height, pos: 40,140)                                                       │ │
│ │                                                                                                           │ │
│ │ "LED STRIP VISUALIZATION" (32px white, centered above)                                                   │ │
│ │ "Left (0-79)" (18px gray, left)                    "Right (80-159)" (18px gray, right)                │ │
│ │                                                                                                           │ │
│ │ ████████████████████████████████████████████ ████ ████████████████████████████████████████████        │ │
│ │ ←─────────────────── Left Strip ───────────────────→  │  ←─────────────────── Right Strip ───────────→ │ │
│ │ (LED 79 at center)  (LED 0 at left)          │  (LED 80 at center)  (LED 159 at right)                │ │
│ │                                                                                                           │ │
│ │ Zone colors: Cyan | Green | Orange | Purple                                                              │ │
│ │ Center gap: 8px cyan divider                                                                             │ │
│ │ "Centre pair: LEDs 79 (left) / 80 (right)" (18px gray, below)                                           │ │
│ └─────────────────────────────────────────────────────────────────────────────────────────────────────────┘ │
│                                                                                                               │
│ ┌─────────────────────────────────────────────────────────────────────────────────────────────────────────┐ │
│ │ ZONE LIST (320px height, pos: 40,260)                                                                    │ │
│ │                                                                                                           │ │
│ │ "Zone Controls" (18px gray, left-aligned above)                                                          │ │
│ │                                                                                                           │ │
│ │ ┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐ │ │
│ │ │ Zone 0 │ LED 0-40 / 120-159 │ Effect Name │ Palette Name │ Blend Mode                              │ │ │
│ │ │ Cyan   │                    │             │              │                                          │ │ │
│ │ │ Border │                    │             │              │                                          │ │ │
│ │ └─────────────────────────────────────────────────────────────────────────────────────────────────────┘ │ │
│ │ ┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐ │ │
│ │ │ Zone 1 │ LED 41-60 / 100-119 │ Effect Name │ Palette Name │ Blend Mode                              │ │ │
│ │ │ Green  │                     │             │              │                                          │ │ │
│ │ │ Border │                     │             │              │                                          │ │ │
│ │ └─────────────────────────────────────────────────────────────────────────────────────────────────────┘ │ │
│ │ ┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐ │ │
│ │ │ Zone 2 │ LED 61-79 / 80-99 │ Effect Name │ Palette Name │ Blend Mode                                │ │ │
│ │ │ Orange │                   │             │              │                                            │ │ │
│ │ │ Border │                   │             │              │                                            │ │ │
│ │ └─────────────────────────────────────────────────────────────────────────────────────────────────────┘ │ │
│ │ ┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐ │ │
│ │ │ Zone 3 │ (if 4 zones active) │ Effect Name │ Palette Name │ Blend Mode                              │ │ │
│ │ │ Purple │                     │             │              │                                          │ │ │
│ │ │ Border │                     │             │              │                                          │ │ │
│ │ └─────────────────────────────────────────────────────────────────────────────────────────────────────┘ │ │
│ │                                                                                                           │ │
│ │ Each row: Background #0841 | Colored border (zone color) | Height: 320px / zone count                  │ │
│ │ Text: 18px Font2 | All gray except zone title (white)                                                    │ │
│ └─────────────────────────────────────────────────────────────────────────────────────────────────────────┘ │
│                                                                                                               │
│ ┌─────────────────────────────────────────────────────────────────────────────────────────────────────────┐ │
│ │ ZONE INFO (180px height, pos: 40,600)                                                                    │ │
│ │                                                                                                           │ │
│ │ "Zones: 3" (18px gray label, white value)                                                                │ │
│ │ "Layout: Centre-out" (18px gray)                                                                         │ │
│ │                                                                                                           │ │
│ │ (Mostly empty space)                                                                                     │ │
│ └─────────────────────────────────────────────────────────────────────────────────────────────────────────┘ │
│                                                                                                               │
│ Note: There's also an LVGL interactive UI with clickable parameter controls (Effect/Palette/Speed/         │
│ Brightness) per zone, but the M5GFX legacy rendering only shows the read-only zone list above.            │
│                                                                                                               │
└─────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
```

### Visual Hierarchy (Current - Flat)

```
IMPORTANCE LEVEL (All Same):
┌─────────────────────────────────────────────────────────┐
│ Level 1: Header/Footer (66px each)                       │
│ Level 2: Parameter Row (125px)                           │
│ Level 2: Action Row (100px)                              │
│ Level 3: Preset Row (85px)                               │
│                                                          │
│ All cards: Same size, same style, same importance       │
│ All text: Similar sizes (24px labels, 32px values)    │
│ All spacing: Uniform 14px gaps                           │
│                                                          │
│ Result: NO VISUAL HIERARCHY                             │
└─────────────────────────────────────────────────────────┘
```

### What Redesign Should Achieve (Hierarchical)

```
IMPORTANCE LEVEL (Redesigned - Hierarchical):
┌─────────────────────────────────────────────────────────┐
│ Level 1: PRIMARY (Largest, most prominent)             │
│   - Most important parameter (e.g., Brightness)         │
│   - Current effect/palette (large, prominent)          │
│                                                          │
│ Level 2: SECONDARY (Medium size)                       │
│   - Other important parameters                         │
│   - Active preset                                       │
│                                                          │
│ Level 3: TERTIARY (Smaller, less prominent)             │
│   - Less frequently used parameters                   │
│   - Saved presets                                       │
│   - Footer metrics                                      │
│                                                          │
│ Result: CLEAR VISUAL HIERARCHY                         │
│   - Size differences: 2x, 3x variations               │
│   - Typography: 16px → 32px → 56px → 72px             │
│   - Spacing: 8px → 16px → 32px → 48px                 │
│   - Color: Semantic meaning (not just decoration)      │
└─────────────────────────────────────────────────────────┘
```

### Card Style Comparison

```
CURRENT (Generic):
┌─────────────┐
│   LABEL     │  ← 24px gray
│             │
│    VALUE    │  ← 32px white
│             │
│  ████████   │  ← 10px yellow bar
└─────────────┘
2px white border
14px radius
#121214 background

REDESIGNED (Should Be Different):
┌─────────────┐
│   label     │  ← 16px gray (smaller)
│             │
│             │
│    VALUE    │  ← 56px white (larger, bold)
│             │
│  ████████   │  ← 12px colored bar (parameter color)
└─────────────┘
No border OR colored border OR gradient background
Larger radius (20px+) OR different shape
Background: colored tint OR gradient OR transparent
```

### Spacing System Comparison

```
CURRENT (Uniform):
┌──┐ ┌──┐ ┌──┐ ┌──┐ ┌──┐ ┌──┐ ┌──┐ ┌──┐
│  │ │  │ │  │ │  │ │  │ │  │ │  │ │  │
└──┘ └──┘ └──┘ └──┘ └──┘ └──┘ └──┘ └──┘
 14px  14px  14px  14px  14px  14px  14px
(All same - boring)

REDESIGNED (Variable):
┌──────┐    ┌──┐ ┌──┐    ┌──┐ ┌──┐    ┌──┐
│      │    │  │ │  │    │  │ │  │    │  │
│      │    └──┘ └──┘    └──┘ └──┘    └──┘
└──────┘
  48px    16px  16px    16px  16px    16px
(Large primary, smaller secondary - creates rhythm)
```
