---
abstract: "Master build spec for the K1 landing page. Links to 8 section-level specs produced by specialist agents. Covers hero, audio demo, DSP/timing, transition/ambient, LGP, comparison/CTA/FAQ, visual asset pipeline, and scroll narrative. All specs are execution-ready — developer, 3D artist, videographer, photographer, and illustrator execute independently."
---

# K1 Landing Page — Master Build Specification

Generated 2026-04-16 from 8 parallel specialist agents. Each spec is self-contained and execution-ready.

## Page Narrative Arc

```
1. HERO — "Music. Made visible." + 16s looping K1 render
2. AUDIO DEMO — "Play your favourite song. Then watch." + 55s instrument build-up video with audio
3. DSP SPECS — "It hears what you hear." + stat pills (125 Hz / 64 bins / 12 notes / 8 bands)
4. TIMING — "Timing is the whole game." + 120 FPS hero stat + frame budget bar
5. TRANSITION — "Sound you can see. Light you can feel." + "And then the music ends. K1 doesn't."
6. AMBIENT — "Light that lives in the room." + 3 callout cards + ambient video loop
7. LGP — "330 mm of liquid light." + cross-section diagram + macro photography + colour mixing demo
8. COMPARISON — "Why it looks different." + 6-row table (K1 vs Govee vs Nanoleaf)
9. CTA — "Reserve Your K1" + $369 + Founders' Edition + "Everything else is a mood lamp."
10. FAQ — "Before you decide." + 7 accordion items (revised for dual-state)
11. FOOTER — Minimal
```

## Section Specs (8 Documents)

Each spec was produced by a specialist agent with full access to the landing page codebase (`SpectraSynq.LandingPage/`), design system (`DESIGN.md`, `tokens.ts`), and locked copy (`LAUNCH_TRUTH.md`). All specs include exact CSS values, typography tables, animation timings, responsive breakpoints, accessibility requirements, and file-level implementation notes.

| # | Spec | Covers | Key Deliverables |
|---|---|---|---|
| 1 | **Hero** | Section 1 | 16s video loop (4 effect segments, seamless), codec chain (HEVC+H.264), poster frame, shimmer loading state, connection-aware degradation, descriptor bar |
| 2 | **Audio Demo** | Section 2 | 55s instrument build-up video (kick→hats→bass→melody→full mix), play button interaction, Option B recommended (real footage + direct audio), genre brief (downtempo electronic), analytics events, quality gate |
| 3 | **DSP + Timing** | Sections 3-4 | Stat pill design (standard + hero variants), 4-column grid, asymmetric layout for 120 FPS, frame budget bar (proportional flex segments), count-up animation, stagger choreography, complete HTML structure |
| 4 | **Transition + Ambient** | Sections 5-6 | Two-line stacked transition moment (white + gold), ambient section with video loop + 3 callout cards, colour temperature shift (cool→warm via `#0a0804` background tint), CSS glow beneath video, photography direction |
| 5 | **LGP** | Section 7 | Layer-by-layer SVG cross-section diagram (8 layers, exact hex per element), macro photography spec, colour mixing video loop, centre-origin animation (CSS), spec strip (4 data points), benefits list |
| 6 | **Comparison + CTA + FAQ** | Sections 8-11 | 6-row table (desktop) + stacked cards (mobile), "Everything else is a mood lamp." line, CTA card with CTAGlow, three-tier pricing consideration, FAQ accordion (CSS Grid height animation), footer |
| 7 | **Asset Pipeline** | All sections | 26-asset manifest with IDs (H-01 through X-04), Blender render specs (4 cameras, lighting rig, 7 renders), 6 real capture shots, 5 photography shots, 2 diagrams, ffmpeg encoding pipeline, 45hr production estimate, dependency graph, 13-point quality checklist |
| 8 | **Scroll Narrative** | Full page | 12-14vh total page length, 3 pacing zones (active/pivot/resolution), colour temperature journey, caesura between technical and emotional halves, single shared IntersectionObserver for ~50 reveal elements, hero parallax, sticky nav CTA, mobile optimisations, 60fps frame budget |

## Decisions Requiring Captain Approval

These surfaced across the 8 specs. Each needs a YES/NO before implementation begins.

| # | Decision | Options | Spec Source |
|---|---|---|---|
| 1 | **Hero subtitle** — which is locked? | A: "A real-time light instrument that turns sound into fluid motion..." (current DESIGN.md) / B: "K1 turns music into living colour in real time — then remains as liquid light..." (dual-state) | Hero |
| 2 | **LiquidMetal WebGL** — keep, relocate, or remove? | A: Keep in hero / B: Relocate to another section / C: Remove entirely | Hero |
| 3 | **Descriptor** — "Music Visualiser. Liquid Light." confirmed? | YES/NO (replaces current "Optical Music Visualiser") | Hero |
| 4 | **Audio demo video source** — which production option? | A: Real footage + room mic / B: Real footage + direct audio (recommended) / C: Full Blender / D: Hybrid | Audio Demo |
| 5 | **Audio composition** — commission original or license? | A: Original (recommended, no Content ID risk) / B: Licensed | Audio Demo |
| 6 | **Section renumbering** — new order confirmed? | Current 01-08 → new 01-11 per the narrative arc above | All specs |

## Visual Asset Summary

| Type | Count | Source | Critical Path? |
|---|---|---|---|
| Blender renders (video) | 2 (hero loop, product shot) | Blender pipeline | YES — blocks hero |
| Real K1 video | 4 (instrument demo, ambient loop, LGP macro, colour mixing) | Hardware capture | YES — blocks audio demo |
| Photography | 3-5 (ambient room, LGP macro, alternate contexts) | Product shoot | No — can use renders as placeholders |
| SVG diagrams | 2 (LGP cross-section, Founders badge) | Illustration | No |
| CSS-only sections | 4 (DSP specs, timing, comparison table, FAQ) | Developer | No |

**Critical path: Hero video loop + Audio demo video.** Everything else can launch with placeholders.

## Implementation Phasing

### Phase 0: Foundation (before any section work)
- [ ] Implement shared `scroll-reveal.ts` with single IntersectionObserver
- [ ] Add `.reveal` / `.visible` CSS classes and variants
- [ ] Verify design tokens match spec (`tokens.ts` alignment)
- [ ] Set up video lazy-loading infrastructure

### Phase 1: Hero + Audio Demo (blocks on video production)
- [ ] Commission audio track (composition brief in Audio Demo spec)
- [ ] Begin Blender hero loop render (16s, 4 segments)
- [ ] Shoot audio demo video (Option B: real footage + direct audio)
- [ ] Build hero section layout
- [ ] Build audio demo section with custom video player

### Phase 2: Technical Proof (no asset dependencies)
- [ ] Build DSP specs section (stat pills, grid, supporting stats)
- [ ] Build timing section (hero stat, secondary grid, frame budget bar)
- [ ] Implement count-up animations
- [ ] Implement stagger choreography

### Phase 3: Emotional Pivot (needs ambient video)
- [ ] Build transition moment (two-line stacked)
- [ ] Capture ambient video loop (6-8s, warm programme)
- [ ] Build ambient section (headline, subhead, video, 3 callout cards)
- [ ] Implement colour temperature shift

### Phase 4: LGP + Comparison + CTA
- [ ] Commission LGP cross-section SVG diagram
- [ ] Shoot LGP macro photography + colour mixing video
- [ ] Build LGP section
- [ ] Build comparison table (desktop + mobile cards)
- [ ] Build CTA section with CTAGlow
- [ ] Update FAQ with dual-state content

### Phase 5: Scroll Polish
- [ ] Implement hero parallax (desktop only)
- [ ] Add caesura spacers
- [ ] Add sticky nav CTA (appears after hero)
- [ ] Mobile optimisations (disable parallax, reduce stagger, simplify CTAGlow)
- [ ] Performance validation (Lighthouse > 90, CLS < 0.1, no scroll jank)

## File References

| Document | Location |
|---|---|
| Design system | `SpectraSynq.LandingPage/DESIGN.md` |
| Locked copy | `SpectraSynq.LandingPage/LAUNCH_TRUTH.md` |
| Design tokens | `SpectraSynq.LandingPage/apps/web-main/theme/tokens.ts` |
| Content data | `SpectraSynq.LandingPage/apps/web-main/lib/content.ts` |
| Current page | `SpectraSynq.LandingPage/apps/web-main/app/page.tsx` |
| Asset pipeline | `SpectraSynq.LandingPage/docs/VISUAL-ASSET-PRODUCTION-PIPELINE.md` |
| Marketing taglines | `Lightwave-Ledstrip/docs/marketing/K1-TAGLINES.md` |
| Marketing strategy | `Lightwave-Ledstrip/docs/marketing/K1-STRATEGY.md` |
| Dual-state positioning | `Lightwave-Ledstrip/docs/marketing/K1-DUAL-STATE-POSITIONING.md` |
| HTML preview variants | `Lightwave-Ledstrip/docs/marketing/previews/variant-{a-e}.html` |

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-04-16 | agent:opus-4.6 (Claude Code) | Created. Master build spec compiled from 8 parallel specialist agents. Links all section specs, surfaces 6 Captain decisions, defines implementation phasing and critical path. |
