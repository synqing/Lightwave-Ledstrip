# Configuration for mgx.dev Agentic Assistant Prompt

## Answers to Research Agent Questions

### 1. Focus: Visual Mockups Only (NOT Implementation)

**Answer: VISUAL MOCKUPS ONLY**

- **Primary Goal**: Generate high-quality visual mockups (1280×720px) that look radically different from current design
- **What we DON'T need**: LVGL implementation code, encoder handling, data refresh logic, or technical implementation
- **What we DO need**: 
  - Visual design mockups (PNG/JPG/Figma-like)
  - Design system specifications (colors, typography, spacing)
  - Layout concepts and visual hierarchy
  - Component design ideas
  - Theme exploration (5-7 different directions)

**Reason**: We already have the technical implementation (`DASHBOARD_SOURCE_REFERENCES.md`). We need **visual design inspiration** that shows how the UI could look with professional polish, not how to code it.

---

### 2. Multiple Themes vs Single Theme Refinement

**Answer: MULTIPLE THEMED DESIGN DIRECTIONS**

- Generate **5-7 completely different themed mockups**
- Each theme should explore a distinct design direction:
  1. Modern Audio Interface (Ableton Live, Logic Pro style)
  2. Gaming RGB Controller (Razer Synapse, Corsair iCUE style)
  3. Apple/iOS Professional (macOS/iOS modern style)
  4. Material Design 3 (Google Material 3)
  5. Minimalist Control Center (Nordic/Scandinavian)
  6. Neon Cyberpunk (Futuristic/Retro)
  7. Warm Analog (Vintage Hardware, 1970s synths)

- Each theme should include **BOTH screens**:
  - Main Dashboard (Global Parameters view)
  - Zone Composer (Zone editing view)

- After mockups are generated, we'll **select 1-2 themes** for implementation
- **No refinement needed** at mockup stage - we want to see bold, different directions first

**Reason**: We need to see multiple creative directions to choose what feels right for LightwaveOS brand. Refinement happens after we pick a direction.

---

### 3. Real Data vs Simulated/Mock Data

**Answer: SIMULATED/MOCK DATA (Realistic but Fake)**

- Use **realistic mock/simulated data** for all mockups
- No need for real encoder logs or system metrics
- Mock data should be **realistic and representative**:
  - Parameter values: "42", "128", "25", "180", etc. (0-255 range)
  - Effect names: "Neon Pulse V2", "Quantum Bloom", "Cosmic Swirl", etc.
  - Palette names: "Cyber Night", "Ocean Depth", "Sunset Gradient", etc.
  - Preset states: Mix of empty, saved (E42 P15 180), and active states
  - Network: "HOME_5G (-42 dBm) 192.168.1.105"
  - Footer metrics: "BPM: 120", "KEY: C#min", "MIC: -46.4 DB", "UPTIME: 1h 24m 32s", "WS: OK", "BATTERY: 85%"
  - Zone Composer: 3-4 zones active with realistic effect/palette names and LED ranges

- **Purpose**: Show how the UI looks with typical data, not test with real inputs
- **States to Show**: 
  - Default state (all parameters at normal values)
  - Active/highlighted state (one parameter selected)
  - Preset recall state (green border flash)
  - Zone Composer with zones selected

**Reason**: Mockups are visual design exercises. Real data integration happens during implementation. We need to see the design with representative data to evaluate visual appeal and information hierarchy.

---

### 4. Intended Update Frequency

**Answer: VARIABLE (Show Different Update Patterns in Mockups)**

- **For Mockup Purposes**: Show UI in various states (static is fine, but indicate update patterns)
- **Actual Update Frequencies** (for reference, not implementation):
  - **Parameter Values**: Real-time (on encoder turn)
  - **Effect/Palette Names**: On change (from WebSocket)
  - **Footer Metrics**: 
    - BPM/Key/Mic: Real-time (audio analysis updates)
    - Uptime: 1Hz (updates every second)
    - Battery: 1Hz (updates every second)
    - WS Status: On connection state change
  - **Preset Feedback**: Brief flash (600ms for save/recall, 600ms for delete)
  - **Parameter Highlight**: Brief flash (300ms when encoder adjusted)
  - **Zone Composer**: Updates on parameter change or zone selection

- **For Mockups**: 
  - Show **static states** (default, selected, active)
  - **Indicate update patterns** in design notes if relevant (e.g., "BPM updates real-time", "Battery updates every second")
  - Consider showing **transition states** if useful for design evaluation (e.g., before/after preset recall)

**Reason**: Update frequency affects visual design (how much can change, animation needs, etc.), but for mockup stage, static representations with notes on update patterns is sufficient.

---

## Summary for mgx.dev Prompt

**Focus**: Visual mockups only (no implementation code)

**Quantity**: 5-7 themed design directions (each with Main Dashboard + Zone Composer)

**Data**: Realistic mock/simulated data (not real system data)

**Updates**: Static mockups showing different states, with notes on update patterns if relevant

**Primary Deliverable**: High-quality visual mockups that look professional, modern, and RADICALLY DIFFERENT from current implementation

---

## What We're Looking For

The mgx.dev assistant should:

1. ✅ Generate professional visual mockups (like Figma designs, but for embedded UI)
2. ✅ Explore multiple creative directions (5-7 themes)
3. ✅ Show both screens (Main Dashboard + Zone Composer) for each theme
4. ✅ Provide design system specs (colors, typography, spacing)
5. ✅ Make mockups look NOTHING like current implementation
6. ✅ Create designs that feel modern (2030, not 2000s)
7. ✅ Show how Zone Composer manages 16 parameters without clutter

The assistant should NOT:

❌ Write LVGL code or implementation details
❌ Handle encoder input or data refresh logic
❌ Refine a single theme (explore multiple first)
❌ Use real system data or logs
❌ Create designs that look like current dashboard

---

## Recommended mgx.dev Prompt Structure

1. **Start with `GEMINI_MOCKUP_BRIEF.md`** - This is the main creative brief
2. **Reference `DASHBOARD_SOURCE_REFERENCES.md`** - For technical constraints only (don't let constraints kill creativity, but ensure designs are feasible)
3. **Emphasize**: Visual mockups, multiple themes, both screens, radical differences
4. **Output**: PNG/JPG mockups (or format mgx.dev prefers) with design system documentation

---

## Success Criteria for mgx.dev Output

✅ Mockups look professional and polished
✅ Each theme is visually distinct from others
✅ Main Dashboard and Zone Composer match theme
✅ Designs look NOTHING like current implementation
✅ Zone Composer effectively shows 16 parameters
✅ Typography hierarchy is clear and dramatic
✅ Color usage is purposeful and semantic
✅ Layout is innovative (not just rearranged grid)
✅ Would a professional designer say "wow, that's modern"?
