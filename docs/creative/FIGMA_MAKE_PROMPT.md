# LightwaveOS Dashboard - Figma Make Prompt

> Copy everything below the line into Figma Make.

---

```
Please design a complete LED control dashboard called "LightwaveOS".

## Context
Control interface for a dual-strip WS2812 LED system (320 LEDs total, 2 strips of 160) embedded in a Light Guide Plate. All visual effects must radiate from the CENTER point (LED 79/80), never edge-to-edge. Target users are makers and lighting designers who need real-time parameter control with live performance monitoring.

## Platform
Web dashboard, 1920x1080px desktop frame. Create one frame per tab (4 frames total).

## Visual Style

COLORS (use exactly these hex values):
- Background/Canvas: #1c2130
- Surface (cards, panels): #252d3f
- Elevated (dropdowns, modals): #2f3849
- Primary Accent (gold): #ffb84d
- Secondary Accent (cyan): #6ee7f3
- Text Primary: #e6e9ef
- Text Secondary: #b5bdca
- Text Muted: #717182
- Success: #22dd88
- Warning: #f59e0b
- Error: #ef4444
- Zone 1 (center): #6ee7f3
- Zone 2 (mid): #22dd88
- Zone 3 (edges): #ffb84d

TYPOGRAPHY:
- Page titles: Bebas Neue, 24px, uppercase, letter-spacing 2%
- Section headers: Bebas Neue, 16px, uppercase
- Card titles: Bebas Neue, 14px, uppercase
- Body text: Inter, 14px, regular
- Labels: Inter, 12px, medium
- Values/numbers: JetBrains Mono, 13px
- Small labels: Inter, 10px

CARD STYLE (apply to all cards and panels):
- Fill: #252d3f with 95% opacity
- Corner radius: 16px
- Border: 1px solid white at 12% opacity
- Drop shadow: Y=12, blur=32, black at 25% opacity
- Inner glow: white at 8% opacity along top edge
- Subtle gradient overlay: radial from top-left corner, white 8% fading to transparent

SPACING:
- Page margins: 32px
- Card padding: 24px
- Gap between cards: 16px
- Gap between form elements: 12px
- Section gap: 24px

## Component Specifications

HEADER (height 64px, full width):
- Background: #252d3f
- Left: Logo icon (40x40px gold rounded square with lightning bolt) + "LightwaveOS" in Bebas Neue 20px
- Right: Connection indicator (12px circle, green #22dd88 when connected) + "Connected" label + gear icon 24x24px

LED STRIP VISUALIZATION (height 120px, max-width 960px, centered):
- Container: aspect ratio 32:7, #1c2130 fill, 12px corner radius
- Show 160 vertical bars representing LEDs, 4px wide each, 1px gap
- CENTER marker: vertical cyan line at 50% position with "CENTER" label above in cyan
- Zone boundaries: dashed vertical lines at 17%, 33%, 67%, 83% positions
- Zone coloring: LEDs near center are cyan, mid-range are green, edges are amber
- Subtle animated glow effect emanating from center
- Labels: "LED 0" bottom-left, "LED 159" bottom-right in JetBrains Mono 10px

TAB BAR (height 48px):
- 4 tabs evenly spaced: "Control + Zones", "Shows", "Effects", "System"
- Active tab: gold #ffb84d text with 3px gold underline
- Inactive tabs: #b5bdca text
- Tab padding: 24px horizontal

SLIDERS:
- Track: 4px height, #2f3849 fill, full corner radius
- Filled portion: gold #ffb84d
- Thumb: 20px circle, gold fill, white 20% border, drop shadow
- Value label: JetBrains Mono 13px, right-aligned

BUTTONS:
- Primary: gold #ffb84d fill, #1c2130 text, 8px radius, 44px height, 16px horizontal padding
- Secondary: transparent fill, 1px gold border, gold text, 8px radius, 44px height
- Ghost: transparent, #b5bdca text, no border
- All buttons: hover state 10% brighter

DROPDOWNS:
- Height 44px, #2f3849 fill, 8px radius, 1px white 10% border
- Chevron icon right-aligned
- Selected value in #e6e9ef

## Tab Layouts

FRAME 1 - CONTROL + ZONES:
Three-column layout (30% | 40% | 30%)

Left column "Global Controls":
- Brightness: slider 0-255, current value displayed
- Speed: slider 1-50
- Palette: dropdown with 12 preset names
- Two buttons: [All Off] secondary, [Save Preset] primary

Center column "Zone Composer":
- 4 zone tabs at top (numbered 1-4, color-coded with zone colors)
- Selected zone panel showing:
  - Effect dropdown
  - Blend mode dropdown (Normal, Add, Multiply, Screen)
  - Enable toggle switch
  - Visual Pipeline section with sliders: Blur, Softness, Gamma

Right column "Quick Tools":
- Transitions card: Type dropdown (12 options), Duration slider, [Trigger] button
- Recent Effects: list of 5 effect names with star icons
- Performance: mini display showing "120 FPS" in green

FRAME 2 - SHOWS:
Two-column layout (40% | 60%)

Left "Show Library":
- List of show cards, each with: thumbnail placeholder, show name, duration badge, scene count
- [+ New Show] button at bottom

Right "Timeline Editor":
- Horizontal timeline with time markers
- Scene blocks as draggable rectangles
- Playhead/scrubber line
- Keyframe markers as small diamonds

Bottom "Now Playing" bar (64px):
- Show progress bar
- Transport controls: [Previous] [Play/Pause] [Stop] [Next]
- Current scene name

FRAME 3 - EFFECTS:
Two-column layout (50% | 50%)

Left "Effects Browser":
- Search input field with magnifying glass icon
- Category filter pills: All, Basic, LGP, Quantum, Organic, Geometric
- 3-column grid of effect cards, each showing: icon, effect name, category badge
- Favorite star on each card

Right "Palettes & Colors":
- Grid of 12 palette swatches (rectangular, showing gradient preview)
- HSV sliders: Hue (0-360), Saturation (0-100), Value (0-100)
- Color preview circle 64px

Bottom bar:
- Mini LED strip preview (simplified version)
- Effect metadata: name, parameter count, category
- [Apply to Zone] dropdown button

FRAME 4 - SYSTEM:
2x2 grid layout

Top-left "Performance":
- FPS display: large number with unit, color-coded (green >=100, amber 60-99, red <60)
- CPU usage: horizontal bar with percentage
- Memory usage: horizontal bar with percentage
- Frame time: value in milliseconds

Top-right "Network":
- Connection status: large indicator dot + status text
- Device IP address
- Latency: value in ms
- WebSocket status indicator

Bottom-left "Device":
- LED count: 320
- Pin configuration: GPIO 4, GPIO 5
- Power limit setting with slider
- Hardware revision

Bottom-right "Firmware":
- Version number
- Flash usage bar
- RAM usage bar
- Two buttons: [Reboot] secondary, [Factory Reset] ghost with warning color

Below grid (collapsible sections):
- WiFi Configuration (SSID, password fields, [Connect] button)
- Debug Options (checkboxes)
- Serial Monitor (dark terminal-style text area)

## States to Include

Show these states as variants or annotations:
- Button hover (10% brighter)
- Button disabled (40% opacity)
- Slider active/dragging (thumb enlarged to 24px)
- Dropdown open (showing options list)
- Tab switching (gold underline animation)
- Connection disconnected (red indicator, warning banner)
- Effect card selected (gold border)
- Zone disabled (50% opacity)

## Critical Constraints

- All interactive elements: minimum 44x44px touch target
- Text contrast: minimum 4.5:1 ratio (WCAG AA)
- NO rainbow color cycling in any preview or visualization
- LED strip must always show CENTER marker prominently
- Use gold #ffb84d as the primary action color consistently
- Keep performance metrics visible - users need to monitor FPS

Please generate all 4 tab frames with consistent styling.
```
