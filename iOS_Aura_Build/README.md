# iOS Aura Build -- Design Exploration Project

**Project:** LightwaveOS iOS v2 UI/UX Redesign
**Created:** 2026-02-16
**Platform:** aura.build (AI-powered design generation)

---

## Folder Structure

```
iOS_Aura_Build/
├── README.md                          <- This file
├── prompts/                           <- Ready-to-paste aura.build prompts
│   ├── 01_play_tab_redesign.md        <- Main performance screen
│   ├── 02_audio_tab_restructure.md    <- Audio diagnostics dashboard
│   ├── 03_zones_tab_redesign.md       <- Zone composition interface
│   └── 04_connection_onboarding.md    <- Device discovery & connection
├── reports/                           <- Agent analysis reports
│   ├── 01_codebase_inventory.md       <- Full project structure (100+ files)
│   ├── 02_ux_architecture_review.md   <- Architecture grade + 15 UX issues
│   ├── 03_design_system_analysis.md   <- Design tokens + improvement proposals
│   ├── 04_aura_build_research.md      <- Platform capabilities & workflow
│   └── 05_visual_assets_inventory.md  <- Uploadable assets catalog
└── reference/                         <- Key source files for upload
    ├── DesignTokens.swift             <- Colour/font/spacing/glass tokens (451 lines)
    └── DESIGN_SPEC.md                 <- Full UI specification (391 lines)
```

---

## How to Use

### Step 1: Capture Screenshots
Build and run the app in iOS Simulator:
```bash
cd lightwave-ios-v2
open LightwaveOS.xcodeproj  # or generate with xcodegen
# Run on iPhone 15 Pro simulator
# Screenshot each tab: Play, Zones, Audio, Device
# Screenshot: ConnectionSheet, EffectSelectorView, PaletteSelectorView
```

### Step 2: Upload to aura.build
For each prompt in `prompts/`:
1. Open aura.build
2. Copy the **Prompt** section text
3. Attach 2 files as specified in **Attachments** section
4. Generate design
5. Create 2-3 variations with slight prompt tweaks
6. Export winners to Figma

### Step 3: Review & Iterate
- Use aura.build **Design Mode** for micro-edits without full regeneration
- Export Figma files for team annotation
- Document which design directions resonate

### Step 4: Implement in SwiftUI
- Use Figma exports as visual spec
- Use React code exports as layout reference
- Apply changes to existing `DesignTokens.swift` system
- Start with quick wins (touch targets, contrast, LazyVStack)

---

## Key aura.build Limitations

- Does NOT generate SwiftUI code (web-only: HTML/React)
- 2 file attachments per prompt maximum
- @context supports up to 100K chars (~2,000 lines)
- Design Mode allows tweaks without full regeneration
- Export to Figma for best iOS design handoff

---

## Reference Files for Upload

| File | Source Location | Best Paired With |
|------|----------------|------------------|
| `reference/DesignTokens.swift` | `lightwave-ios-v2/LightwaveOS/Theme/DesignTokens.swift` | Any app screenshot |
| `reference/DESIGN_SPEC.md` | `lightwave-ios-v2/docs/DESIGN_SPEC.md` | UI overhaul mockup |
| App icon PNG | `lightwave-ios-v2/LightwaveOS/Assets.xcassets/AppIcon.appiconset/` | Connection screen |
| UI mockup HTML | `lightwave-ios-v2/mockups/ui-overhaul-mockup.html` | Any screenshot |
| Audio viz HTML | `lightwave-ios-v2/mockups/audio-viz-explorer.html` | Audio tab screenshot |

---

## Analysis Summary (from 5 specialist agents)

| Metric | Value |
|--------|-------|
| Total Swift code | ~21,805 lines across 100+ files |
| Architecture grade | B+ (Modern @Observable MVVM) |
| UX/Accessibility grade | C+ (touch targets, contrast, loading states) |
| Design system grade | B- (strong tokens, component duplication) |
| Critical UX issues found | 15 |
| Quick wins identified | 10 (most under 1 hour) |
| aura.build suitability | Excellent for visual exploration, NOT for SwiftUI code |
