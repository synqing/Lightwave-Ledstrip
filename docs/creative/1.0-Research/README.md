# LightwaveOS Creative Documentation

> Dashboard Architecture Analysis, UI Requirements, and Figma Implementation Guide

## Overview

This documentation provides comprehensive resources for designing and implementing the LightwaveOS dashboard based on analysis of the K1.node1 PRISM.node2 system and Figma reference templates.

## Documents

| Document | Purpose |
|----------|---------|
| **[01_K1_NODE1_ARCHITECTURE.md](./01_K1_NODE1_ARCHITECTURE.md)** | Complete architecture analysis of K1.node1 (30,000 LOC React+TypeScript system) |
| **[02_UI_REQUIREMENTS.md](./02_UI_REQUIREMENTS.md)** | UI requirements specification with 4-tab information architecture |
| **[03_FIGMA_PROMPT_GUIDE.md](./03_FIGMA_PROMPT_GUIDE.md)** | 11 sequential Figma Make Agent prompts for wireframe generation |
| **[04_DESIGN_TOKENS.md](./04_DESIGN_TOKENS.md)** | Complete PRISM.node design token specification (CSS custom properties) |

## Quick Reference

### 4-Tab Dashboard Structure

```
Tab 1: CONTROL + ZONES      ─ Primary interface (3-column layout)
Tab 2: SHOWS                ─ ShowDirector timeline (40/60 split)
Tab 3: EFFECTS LIBRARY      ─ Effects + Palettes browser (50/50 split)
Tab 4: SYSTEM               ─ Profiling + Network + Settings (2x2 grid)
```

### Key Design Decisions

1. **Transitions are contextual** - Placed in Control tab's "Quick Tools" column, not standalone
2. **Effects + Palettes combined** - Linear workflow enables side-by-side comparison
3. **Shows tab dedicated** - Different mental model requires full-width timeline
4. **System tab unified** - All technical tasks share context in dashboard format

### PRISM.node Color Palette

| Token | Hex | Usage |
|-------|-----|-------|
| `bg-canvas` | `#0A0E14` | Main background |
| `bg-surface` | `#1A1F2E` | Panels, cards |
| `bg-elevated` | `#252B3D` | Modals, dropdowns |
| `accent-primary` | `#FFD700` | Gold - CTAs, active |
| `accent-secondary` | `#00CED1` | Cyan - Info, status |
| `text-primary` | `#E8E8E8` | Main content |
| `zone-1` | `#FFD700` | Zone 1 (Gold) |
| `zone-2` | `#00CED1` | Zone 2 (Cyan) |
| `zone-3` | `#10B981` | Zone 3 (Emerald) |
| `zone-4` | `#F59E0B` | Zone 4 (Amber) |

### Domain Constraints

| Constraint | Requirement |
|------------|-------------|
| **CENTER ORIGIN** | Effects must radiate from LED 79/80 |
| **NO RAINBOWS** | Rainbow color cycling is forbidden |
| **320 LEDs** | Dual 160-LED WS2812 strips |
| **120 FPS** | Target frame rate |

## Figma Prompt Execution Order

1. **Phase 1 (Foundation)**: Prompts 1-2 - Design tokens, navigation shell
2. **Phase 2 (Components)**: Prompts 3-7 - All 4 tabs, component library
3. **Phase 3 (Composition)**: Prompts 8-9 - Full layout, responsive variants
4. **Phase 4 (Refinement)**: Prompts 10-11 - Edge cases, prototype, handoff

## Source References

### K1.node1 Patterns to Adopt

| Pattern | File | Benefit |
|---------|------|---------|
| CORS Fallback | `lib/api.ts` | ESP32 compatibility |
| Parameter Coalescing | `hooks/useCoalescedParams.ts` | 70-90% fewer API calls |
| WebSocket + Polling | `services/websocket.ts` | Graceful degradation |
| Optimistic UI | `hooks/useOptimisticPatternSelection.ts` | Instant feedback |

### Figma Sample Templates

| Sample | Location | Key Patterns |
|--------|----------|--------------|
| **Sample 2** | `/docs/templates/Figma_Sample2/` | PRISM.node tokens, control panels, profiling charts |

## Integration Points

| LightwaveOS File | Action |
|------------------|--------|
| `/data/index.html` | Replace with generated dashboard |
| `/data/app.js` | Preserve API integration logic |
| `/data/styles.css` | Replace with PRISM.node tokens |

---

*Generated: December 2025*
*Source: K1.node1 Dashboard Architecture Analysis*
