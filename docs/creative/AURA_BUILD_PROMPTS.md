# Aura.build Prompts for LightwaveOS Dashboard

**Context**: These 3 prompts are designed to polish the existing `LightwaveOS_Dashboard.html` template before React integration. Each prompt is crafted to maximize aura.build's ~40k character output capacity.

**Usage**: Submit each prompt to aura.build with the current HTML as the starting file.

---

## PROMPT 1: LED Strip Visualization + Zone Indicators

```
TASK: Upgrade the LED strip visualization to be hardware-accurate and add zone color indicators.

EXISTING FILE: LightwaveOS_Dashboard.html

CRITICAL REQUIREMENTS:

1. LED STRIP VISUALIZATION (Control + Zones tab)
   Replace the current #ledBars1 visualization with EXACTLY 160 bars representing the physical LED strip.

   ZONE COLORING (this is critical - the hardware has 4 zones):
   - Zone 1 (LEDs 0-26 and 133-159): Amber/Gold #ffb84d
   - Zone 2 (LEDs 27-52 and 107-132): Green #22dd88
   - Zone 3 (LEDs 53-79): Cyan #6ee7f3 (LEFT of center)
   - Zone 3 (LEDs 80-106): Cyan #6ee7f3 (RIGHT of center)

   The center point is between LED 79 and LED 80 - this is where effects originate.

   Implementation:
   - Create exactly 160 div bars
   - Color each bar according to its zone
   - Heights should vary smoothly: tallest at center (LEDs 79-80), shortest at edges (LEDs 0 and 159)
   - Add subtle glow effect to center bars (box-shadow with zone color)
   - Bars should have 1px gap between them
   - The existing center marker and zone boundary lines should remain

2. ZONE TAB COLOR INDICATORS
   In the Zone Composer section, the 4 zone tabs currently have no color indication.

   Add a small colored dot (6px diameter) to each zone tab:
   - Zone 1 tab: Amber dot #ffb84d
   - Zone 2 tab: Green dot #22dd88
   - Zone 3 tab: Cyan dot #6ee7f3
   - Zone 4 tab: Gray dot #717182 (disabled zone)

   Position the dot to the left of the "Zone N" text within each button.

3. CENTER PULSE ANIMATION
   Add a subtle CSS animation to the center marker (#ledBars1 center line):
   - Pulse the box-shadow intensity from 35% to 55% opacity
   - 2-second duration, ease-in-out
   - Infinite loop

   Also add a subtle "breathing" effect to the CENTER badge:
   - Scale from 1.0 to 1.02 and back
   - Same timing as the center line pulse

4. PREVIEW STRIP (#ledBarsPreview in Effects tab)
   Update the preview strip to also use 160 bars with the same zone coloring.
   This should match the main visualization exactly.

5. JAVASCRIPT UPDATES
   Update the renderBars() function to:
   - Accept a 'count' parameter (default 160)
   - Apply zone-based coloring instead of random palette
   - Calculate bar heights using a bell curve centered at position 79.5
   - Add the CSS animation class to center bars

DESIGN TOKENS (preserve these exactly):
- Canvas: #1c2130
- Surface: #252d3f
- Gold: #ffb84d
- Cyan: #6ee7f3
- Green: #22dd88
- Text: #e6e9ef
- Muted: #b5bdca

OUTPUT: Complete updated HTML file with all changes applied.
```

---

## PROMPT 2: Missing Controls + Palette Enhancements

```
TASK: Add missing control sliders and enhance the palette dropdown with color swatches.

EXISTING FILE: LightwaveOS_Dashboard.html (with Prompt 1 changes applied)

CRITICAL REQUIREMENTS:

1. TRANSITION DURATION SLIDER
   In the "Quick Tools" panel > "Transitions" section, add a Duration slider ABOVE the Type dropdown.

   Slider specifications:
   - Label: "Duration"
   - Range: 100ms to 3000ms
   - Default: 1000ms
   - Display format: "1.0s" (divide by 1000, 1 decimal place)
   - Same visual style as Brightness/Speed sliders
   - Fill color: #ffb84d

   Position: Between the "Transitions" header and the "Type" dropdown.

2. SOFTNESS SLIDER IN VISUAL PIPELINE
   In the Zone Composer > Visual Pipeline section, add a Softness slider BETWEEN Blur and Gamma.

   Slider specifications:
   - Label: "Softness"
   - Range: 0 to 100
   - Default: 45
   - Display format: "0.45" (divide by 100, 2 decimal places)
   - Same visual style as Blur/Gamma sliders
   - Fill color: #ffb84d

   Add tooltip hint below the Visual Pipeline header:
   "pre → blur → soft → gamma → post"

3. PALETTE DROPDOWN WITH COLOR SWATCHES
   Enhance the existing palette dropdown to show color previews.

   Each palette option should display:
   - 4 small color circles (12px diameter each) showing the palette colors
   - Palette name to the right of the circles
   - 4px gap between circles, 12px gap between circles and name

   Palette definitions:
   - Aurora Glass: [#6ee7f3, #22dd88, #ffb84d, #b5bdca]
   - Cyan Ember: [#6ee7f3, #00a3cc, #ff6b35, #2d3a4f]
   - Edge Amber: [#ffb84d, #ff8c00, #ffd700, #fff8dc]
   - Neon Ice: [#00ffff, #00e5ff, #ffffff, #87ceeb]
   - Photon Smoke: [#4a4a4a, #6b6b6b, #8b8b8b, #b5bdca]

   When a palette is selected:
   - Show the same 4 color circles in the button (replacing current text-only display)
   - Keep the palette name visible next to the circles

4. TRANSITION TYPE DROPDOWN ENHANCEMENT
   Currently the transition type is a button. Convert it to a proper dropdown like the palette.

   Available transition types:
   - Radial Crossfade (default)
   - Center Wipe
   - Edge Fade
   - Spiral Reveal
   - Shatter
   - Dissolve
   - Zoom Blur
   - Wave Morph

   Each option should show a small icon (use Lucide icons) representing the transition:
   - Radial Crossfade: lucide:circle
   - Center Wipe: lucide:move-horizontal
   - Edge Fade: lucide:arrow-right-left
   - Spiral Reveal: lucide:loader (or similar spiral)
   - Shatter: lucide:grid-3x3
   - Dissolve: lucide:cloud
   - Zoom Blur: lucide:zoom-out
   - Wave Morph: lucide:waves

5. JAVASCRIPT UPDATES
   - Add bindSlider() calls for Duration and Softness
   - Create a bindDropdown() helper for the transition type dropdown
   - Update palette selection to render color circles
   - Ensure all new controls sync with the same interactive patterns

DESIGN TOKENS (preserve these exactly):
- Canvas: #1c2130
- Surface: #252d3f
- Gold: #ffb84d
- Cyan: #6ee7f3
- Green: #22dd88
- Text: #e6e9ef
- Muted: #b5bdca
- Input background: #2f3849
- Border: rgba(255,255,255,.12)

OUTPUT: Complete updated HTML file with all changes applied.
```

---

## PROMPT 3: Polish, Animations + Mobile Layout

```
TASK: Add micro-interactions, smooth animations, and responsive mobile layout.

EXISTING FILE: LightwaveOS_Dashboard.html (with Prompt 1 and 2 changes applied)

CRITICAL REQUIREMENTS:

1. TAB SWITCHING ANIMATION
   Add smooth transitions when switching between tabs.

   Implementation:
   - Tab indicator (gold underline) should slide horizontally to the active tab
   - Use CSS transition: transform 0.3s cubic-bezier(0.4, 0, 0.2, 1)
   - Tab content should fade in: opacity 0→1 over 200ms
   - Add a subtle scale effect: scale 0.98→1.0 on tab content appear

   CSS classes to add:
   - .tab-indicator-animated: for the sliding underline
   - .tab-content-enter: opacity 0, transform scale(0.98)
   - .tab-content-enter-active: opacity 1, transform scale(1)

2. BUTTON + INTERACTIVE HOVER STATES
   Add consistent hover/active states to all interactive elements.

   Primary buttons (gold background):
   - Hover: brightness(1.1), translateY(-1px), box-shadow increase
   - Active: brightness(0.95), translateY(0)
   - Transition: 150ms ease

   Secondary buttons (transparent/border):
   - Hover: background rgba(255,255,255,0.05)
   - Active: background rgba(255,255,255,0.08)

   Cards/panels on hover:
   - Subtle border-color brighten (opacity 0.12 → 0.18)
   - Very subtle box-shadow expansion

   Slider thumbs:
   - Hover: scale(1.1)
   - Active: scale(0.95)

3. ZONE TAB SELECTION ANIMATION
   When selecting a zone tab in Zone Composer:
   - Animate the border color change (150ms)
   - Add a subtle pop effect (scale 1→1.02→1)
   - The zone's color should pulse briefly in the LED visualization

4. LED BAR ANIMATIONS
   Add subtle life to the LED visualization:
   - Each bar should have a slight random height variation (+/- 3%) every 500ms
   - Bars near the center should have slower, more gentle variation
   - Edge bars should have faster, more pronounced variation
   - Use requestAnimationFrame for smooth 60fps updates
   - Add CSS transition on bar heights for smooth morphing

5. CONNECTION STATUS ANIMATION
   The status dot in the header:
   - When connected: subtle pulse animation (opacity 0.8→1)
   - When disconnected: faster pulse (warning effect)
   - Reconnecting state: rotating ring animation

6. MOBILE RESPONSIVE LAYOUT

   Breakpoints:
   - Desktop: ≥1280px (current layout)
   - Tablet: 768px-1279px
   - Mobile: <768px

   Tablet adjustments:
   - Grid changes from 10-column to 6-column
   - Quick Tools moves below Zone Composer (full width)
   - LED visualization maintains aspect ratio but fills width

   Mobile adjustments:
   - Single column layout throughout
   - Tab bar becomes horizontally scrollable with snap
   - LED visualization uses 80 bars instead of 160 (performance)
   - Panels stack vertically
   - Sliders become full-width
   - Zone tabs become 2x2 grid
   - Timeline editor becomes simplified (no drag, just list view)
   - Button text sizes reduce, icons remain same size

   Add these mobile utilities:
   - .mobile-only: hidden on desktop
   - .desktop-only: hidden on mobile
   - Touch-friendly tap targets (minimum 44px)

7. LOADING SKELETON
   Add a loading state for when data is being fetched:
   - Show shimmer animation placeholders for:
     - Effect list items
     - Show library cards
     - Diagnostic values
   - Shimmer color: linear-gradient from #2f3849 to #3a4559 sliding left-to-right
   - 1.5s duration, infinite

8. MICRO-INTERACTIONS
   - Dropdown menus: scale in from 0.95 with fade
   - Form inputs focus: add gold ring animation
   - Toggle switches: spring physics (overshoot on toggle)
   - Cards: slight parallax on mouse move (translateX/Y based on cursor position)
   - FPS counter: subtle color fade based on value (green >90, yellow 60-90, red <60)

CSS ORGANIZATION:
Add a <style> block at the end of <head> with:
- Animation keyframes
- Responsive breakpoints (@media queries)
- Utility classes
- Component-specific transitions

JAVASCRIPT OPTIMIZATION:
- Debounce resize handlers
- Use CSS transforms instead of layout properties where possible
- Add IntersectionObserver for animations (only animate visible elements)
- Cache DOM references

DESIGN TOKENS (preserve these exactly):
- Canvas: #1c2130
- Surface: #252d3f
- Gold: #ffb84d
- Cyan: #6ee7f3
- Green: #22dd88
- Text: #e6e9ef
- Muted: #b5bdca

OUTPUT: Complete updated HTML file with all changes applied. This is the FINAL polish pass - ensure everything feels premium and responsive.
```

---

## Submission Order

1. **Submit Prompt 1** → Get updated HTML with LED visualization
2. **Submit Prompt 2** (using output from Prompt 1) → Get updated HTML with controls
3. **Submit Prompt 3** (using output from Prompt 2) → Get final polished HTML

After all 3 prompts are complete, the HTML template will be the visual reference for React component development in Phase 3.

---

## Post-Aura.build Verification Checklist

After receiving the final HTML:

- [ ] LED strip shows exactly 160 bars with zone coloring
- [ ] Zone tabs have colored indicator dots
- [ ] Center pulse animation is visible
- [ ] Transition duration slider works (100ms-3000ms)
- [ ] Softness slider works in Visual Pipeline
- [ ] Palette dropdown shows color swatches
- [ ] Tab switching has smooth animation
- [ ] Mobile layout works at 375px width
- [ ] All hover states feel responsive
- [ ] Loading skeletons appear correctly

Save the final HTML to: `docs/templates/LightwaveOS_Dashboard_Final.html`
