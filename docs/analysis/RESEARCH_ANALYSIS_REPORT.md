# LGP Pattern Development Research Analysis Report

**Date:** December 21, 2025
**Analyst:** Claude Code
**Scope:** 11 research documents spanning creative design, optical physics, implementation, and narrative frameworks
**Project:** LightwaveOS LED Control System

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Detailed Analysis](#2-detailed-analysis)
3. [Visual Comparison Matrix](#3-visual-comparison-matrix)
4. [Implementation Checklist](#4-implementation-checklist)
5. [Appendix: Document Inventory](#appendix-document-inventory)

---

# 1. Executive Summary

## Strategic Value Proposition

The 11 research documents represent a comprehensive body of work establishing LightwaveOS as a physics-grounded, narratively-driven LED control platform. The documentation defines:

- **83 catalogued visual patterns** across 10 taxonomic families
- **Optical physics foundation** based on real waveguide behavior (TIR, interference, chromatic dispersion)
- **Narrative storytelling system** (BUILD/HOLD/RELEASE/REST) for choreographed shows
- **Implementation framework** with 5-phase development workflow and safety protocols

### Current State Assessment

| Dimension | Documented | Implemented | Gap |
|-----------|------------|-------------|-----|
| Pattern Taxonomy | 83 patterns | ~46 effects | 37 patterns undefined in code |
| Narrative Engine | Full specification | Partial (ShowDirector) | Tension curves, phase sync |
| Physics Reference | Complete equations | Some effects | Chromatic dispersion effects |
| Development Workflow | 5-phase process | Ad-hoc | No automation/tooling |
| Testing Procedures | Defined in c3 | Manual | No automated validation |

---

## Top 5 Recommendations

### 1. Implement Pattern Taxonomy Registry
**Priority:** HIGH | **Impact:** Strategic | **Effort:** Medium

The b2 taxonomy document catalogs 83 patterns across 10 families, but this knowledge exists only in documentation. Creating a programmatic registry enables:
- Runtime pattern discovery and filtering
- Composition rules enforcement
- Show choreography automation
- API-driven pattern selection by family/tags

**Evidence:** b2:45-767 defines complete taxonomy with relationships; current `effects[]` array (main.cpp:202) lacks categorization.

**Risk:** Medium - requires refactoring effect registration
**Success Criteria:** API returns patterns by family; 80%+ patterns tagged

---

### 2. Formalize BUILD/HOLD/RELEASE/REST Engine
**Priority:** HIGH | **Impact:** Strategic | **Effort:** High

The b3 storytelling framework defines a sophisticated 4-phase narrative system with tension curves, but ShowDirector only partially implements this. Full implementation enables:
- Automatic tempo modulation based on narrative phase
- Tension-driven parameter sweeps
- Natural show pacing without manual cue scripting

**Evidence:** b3:1-200 defines phase behaviors; ShowDirector.cpp has `NarrativePhase` enum but limited integration.

**Risk:** High - touches multiple subsystems
**Success Criteria:** Shows auto-modulate tempo; phases visible in API status

---

### 3. Create FastLED Optimization Utility Layer
**Priority:** HIGH | **Impact:** Operational | **Effort:** Low

Documents b4 and c3 repeatedly reference FastLED optimization patterns (sin16, scale8, beatsin8) that should be centralized. Currently effects duplicate optimized math.

**Evidence:** b4:420-450 lists optimization techniques; effects contain redundant implementations.

**Risk:** Low - additive change
**Success Criteria:** Single header with documented optimizations; 20%+ code reduction in effects

---

### 4. Establish Pattern Specification Template Enforcement
**Priority:** MEDIUM | **Impact:** Operational | **Effort:** Low

Document c3 defines a clear PR template and Definition of Done checklist, but these aren't enforced. Automation prevents:
- Centre-origin violations
- Rainbow pattern submissions
- Undocumented effects

**Evidence:** c3:16-45 defines spec template; c3:113-122 defines DoD.

**Risk:** Low - process change
**Success Criteria:** GitHub PR template created; 100% new effects include spec

---

### 5. Build Chromatic Dispersion Effect Library
**Priority:** MEDIUM | **Impact:** Tactical | **Effort:** Medium

Document b1 provides complete physics equations for chromatic dispersion (wavelength-dependent refraction), but no effects leverage this. The LGP hardware naturally exhibits this behavior.

**Evidence:** b1:380-420 provides Cauchy equation implementation; b1:400-420 shows code example.

**Risk:** Medium - requires physics validation
**Success Criteria:** 3+ effects using chromatic dispersion; visual validation on hardware

---

## Critical Path

```
Phase 1 (Foundation)          Phase 2 (Integration)         Phase 3 (Polish)
─────────────────────────────────────────────────────────────────────────────
[FastLED Utilities]     ──────▶ [Pattern Registry]    ──────▶ [PR Templates]
         │                              │                           │
         ▼                              ▼                           ▼
[Chromatic Effects]     ──────▶ [Narrative Engine]   ──────▶ [Testing Suite]
```

---

# 2. Detailed Analysis

## 2.1 Evaluation Methodology

### Criteria Weights

| Criterion | Weight | Rationale |
|-----------|--------|-----------|
| **User Impact** | 30% | Visible improvement in pattern quality and variety |
| **Technical Debt Reduction** | 25% | Code maintainability, documentation alignment |
| **Performance Gain** | 20% | FPS stability, CPU/memory efficiency |
| **Creative Enablement** | 15% | New pattern possibilities unlocked |
| **Safety/Compliance** | 10% | Constraint adherence, photosensitivity |

### Impact Classification

- **Strategic:** Architecture-level changes enabling new capabilities
- **Operational:** Workflow improvements, developer tooling
- **Tactical:** Bug fixes, documentation updates, incremental improvements

### Evidence Standards

All recommendations cite source documents with line numbers. Implementation gaps verified against current codebase.

---

## 2.2 Document-by-Document Analysis

### A-Series: Initial Frameworks

#### a1. LGP_CREATIVE_PATTERN_GUIDE.md
**Focus:** Artist-oriented LGP conceptualization
**Key Contribution:** Establishes "optical levers" metaphor—treating LED patterns as inputs to a physical optical system rather than direct pixel art.

**Findings:**
- Introduces dual-edge injection model (light enters from both 160-LED edges)
- Defines centre as narrative anchor (LEDs 79/80)
- Maps encoder controls to optical properties (Speed → temporal, Intensity → contrast)

**Gaps:** Lacks specific physics equations; addressed in b1.

---

#### a2. LIGHTSHOW_STORYTELLING_FRAMEWORK.md
**Focus:** Narrative structure for light shows
**Key Contribution:** Establishes cause-propagation-interaction-afterimage story model.

**Findings:**
- Defines 5 core motifs: Centre ignition, Edge approach, Collision, Lattice, Dissolution
- Three-act structure with timing guidelines (15-30s / 30-90s / 15-30s)
- Maps emotions to control parameters

**Gaps:** Doesn't specify programmatic implementation; addressed in b3.

---

#### a3. PATTERN_IMPLEMENTATION_PLAYBOOK.md
**Focus:** Basic implementation guidance
**Key Contribution:** Early version of PR templates and testing procedures.

**Gaps:** Superseded by more comprehensive b4 and c3 documents.

---

### B-Series: Technical Deep Dives

#### b1. LGP_OPTICAL_PHYSICS_REFERENCE.md (640 lines)
**Focus:** Complete waveguide physics reference
**Key Contribution:** First-principles physics for LGP behavior.

**Critical Findings:**

1. **Total Internal Reflection (TIR)** (lines 45-120)
   - Critical angle: 42.2° for acrylic (n=1.49)
   - Explains why edge-injected light stays in plate
   - Defines extraction mechanisms

2. **Interference Mathematics** (lines 180-280)
   - Standing wave equation: `A = 2 * cos(k*x) * cos(ωt)`
   - Phase relationship formulas for dual-edge injection
   - Box count prediction: `N = floor(L / (λ/2))`

3. **Chromatic Dispersion** (lines 380-450)
   - Cauchy equation: `n(λ) = A + B/λ² + C/λ⁴`
   - Implementation code for wavelength-dependent focus
   - RGB separation factors

**Implementation Gap:** Chromatic dispersion equations exist but no effects use them.

---

#### b2. LGP_PATTERN_TAXONOMY.md (767 lines)
**Focus:** Complete pattern classification system
**Key Contribution:** Organizes 83 patterns into 10 families with relationships.

**Pattern Families:**

| Family | Count | Example Patterns |
|--------|-------|------------------|
| Interference | 13 | Standing Wave, Moiré Envelope, Phase Collision |
| Geometric | 8 | Tiled Boxes, Radial Rings, Fractal Zoom |
| Advanced Optical | 6 | Chromatic Lens, Diffraction Grating, Caustics |
| Organic | 12 | Bioluminescence, Mycelium, Plankton Waves |
| Quantum | 11 | Wave Function, Soliton Explorer, Entanglement |
| Color Mixing | 12 | Warm Crystal, Cold Crystal, Ember Fade |
| Physics-Based | 6 | Plasma Flow, Magnetic Field, Electrostatic |
| Novel Physics | 5 | Rogue Wave, Turing Pattern, Kelvin-Helmholtz |
| Fluid & Plasma | 5 | Shockwave, Convection, Vortex |
| Mathematical | 5 | Mandelbrot Zoom, Strange Attractor, Kuramoto |

**Relationship Types:**
- Parent-child (base pattern → variations)
- Complementary (patterns that combine well)
- Transition-friendly (smooth morphing pairs)

**Implementation Gap:** Current `effects[]` array has no family metadata.

---

#### b3. LGP_STORYTELLING_FRAMEWORK.md (663 lines)
**Focus:** Programmatic narrative system
**Key Contribution:** Defines BUILD/HOLD/RELEASE/REST engine specification.

**Phase Definitions:**

| Phase | Tension | Tempo | Typical Duration |
|-------|---------|-------|------------------|
| BUILD | 0→100 | Increasing | 15-45 seconds |
| HOLD | 80-100 | Stable high | 10-30 seconds |
| RELEASE | 100→20 | Decreasing | 10-30 seconds |
| REST | 0-20 | Minimal | 15-60 seconds |

**Integration Points:**
- Tension value modulates parameter ranges
- Phase transitions trigger TransitionEngine
- Tempo affects animation speed multiplier

**Implementation Status:** ShowDirector has `NarrativePhase` enum but tension curves not integrated.

---

#### b4. LGP_PATTERN_DEVELOPMENT_PLAYBOOK.md (925 lines)
**Focus:** Complete development workflow
**Key Contribution:** 5-phase development process with templates.

**Development Phases:**

1. **Concept** (lines 50-150)
   - Story brief
   - Optical intent
   - Constraint compliance check

2. **Architecture** (lines 160-300)
   - Pattern family selection
   - Physics model choice
   - Memory budget allocation

3. **Implementation** (lines 310-550)
   - Code templates (Simple, Physics-Based, Audio-Reactive)
   - FastLED optimization patterns
   - Centre-origin enforcement code

4. **Integration** (lines 560-700)
   - Effect registration
   - Encoder mapping
   - Zone compatibility

5. **Optimization** (lines 710-900)
   - Performance profiling
   - Memory analysis
   - Safety validation

**Safety Budgets:**
- Frame rate: ≥120 FPS (8.33ms budget)
- CPU usage: <25% per effect
- Memory: <10KB per pattern (static allocation)

---

#### b5. LGP_CREATIVE_PATTERN_DEVELOPMENT_GUIDE.md (294 lines)
**Focus:** Unified framework cross-reference
**Key Contribution:** Bridges all documents; provides navigation guide.

**Cross-Reference Matrix:**

| Need | Primary Doc | Supporting Docs |
|------|-------------|-----------------|
| Physics equations | b1 | - |
| Pattern ideas | b2 | c1 |
| Narrative structure | b3 | a2, c2 |
| Implementation | b4 | c3 |
| Quick reference | c1, c2, c3 | - |

---

### C-Series: Operational Guides

#### c1. CREATIVE_PATTERN_DEVELOPMENT_GUIDE.md (164 lines)
**Focus:** Concise creative reference
**Key Contribution:** Optical levers → code levers mapping.

**Optical Lever Table:**

| Optical Lever | Visual Effect | Code Lever | Location |
|---------------|---------------|------------|----------|
| Spatial frequency | Box count | Distance multiplier | Per-effect |
| Edge phase | Standing vs traveling | Phase offset | MotionEngine |
| Edge balance | Depth bias | Strip weighting | Per-effect |
| Temporal modulation | Breathing | Speed encoder | Global |
| Extraction softness | Crisp vs mist | Diffusion amount | ColourEngine |

**Palette Grammar (No-Rainbows Compliant):**
- Warm crystal: burnt amber + ivory + near-black
- Cold crystal: deep cyan + pale teal + near-black
- Abyssal: navy + cyan-white highlights
- Ember: deep red-brown + amber highlights
- Violet glass: deep violet + magenta + white

---

#### c2. STORYTELLING_FRAMEWORK.md (149 lines)
**Focus:** Concise narrative reference
**Key Contribution:** Scene cards with parameter envelopes.

**Scene Card Example - "Approach and Impact":**
```
Start: Edge approach (inward contraction) at low Saturation
Event: Collision bloom at centre (safe pulse width)
After: Fragments reorganise into stable lattice

Parameter Envelope:
  Speed:     ramp up → dip at impact → settle mid
  Intensity: ramp up → peak at impact → settle mid-low
  Complexity: low → medium after impact
```

**Transition Choreography:**
- Crossfade: "time passes"
- Morph: "same thing becomes something else"
- Centre wipe: "reveal from centre"
- Centre collapse: "convergence"

---

#### c3. IMPLEMENTATION_PLAYBOOK_LIGHT_PATTERNS.md (124 lines)
**Focus:** Concise implementation reference
**Key Contribution:** PR template and Definition of Done.

**Pattern Specification Template (lines 16-45):**
```markdown
### 1) Overview
- Name:
- Category: standing / travelling / moiré / depth / spectral split
- One-sentence story:

### 2) Optical Intent
- Which optical levers used
- Film-stack assumptions
- Expected signature

### 3) Control Mapping
- Speed →
- Intensity →
- Saturation →
- Complexity →
- Variation →

### 4) Performance Budget
- Target: 120 FPS minimum
- Worst-case compute path
- Memory strategy

### 5) Compliance
- Centre origin: yes/no
- No rainbows: yes/no
- No unsafe strobe: yes/no
```

**Definition of Done (lines 113-122):**
- [ ] Pattern has clear one-sentence story
- [ ] Fits taxonomy category
- [ ] Encoder mappings documented
- [ ] Centre origin rule satisfied
- [ ] No-rainbows rule satisfied
- [ ] Visual verification steps completed
- [ ] Performance validation passed
- [ ] No new stability risks

---

## 2.3 Cross-Document Synthesis

### Theme 1: Physics-First Design

All documents emphasize that LGP patterns should derive from real optical physics:

| Document | Physics Emphasis |
|----------|------------------|
| b1 | Complete equations for TIR, interference, dispersion |
| c1 | "Optical levers" as primary design vocabulary |
| b2 | Pattern families organized by physics principle |
| b4 | Physics-based code templates |

**Synthesis:** Effects that ignore physics (pure pixel art) will appear incorrect on LGP hardware due to interference between dual-edge injection.

---

### Theme 2: Centre-Origin as Narrative Anchor

The centre-origin constraint (LEDs 79/80) appears in every document:

| Document | Lines | Emphasis |
|----------|-------|----------|
| c1 | 7-8 | "Non-negotiable" |
| c3 | 10-11 | "Hard constraint" |
| b4 | 85-100 | Implementation pattern |
| c2 | Throughout | Motifs reference centre |

**Synthesis:** This isn't arbitrary—it's physics. Dual-edge injection creates constructive interference at centre; edge-originating patterns fight the hardware.

---

### Theme 3: Constrained Palettes Enable Sophistication

The "no-rainbows" rule appears restrictive but enables:

| Document | Palette Strategy |
|----------|------------------|
| c1 | Two colour families + neutral |
| b2 | Musical note-to-colour mapping |
| c2 | Emotional palette relationships |

**Synthesis:** Limiting hue sweep forces creative use of intensity, phase, diffusion, and spatial frequency—the optical levers that actually look good on LGP.

---

## 2.4 Gap Analysis

### Documentation vs Implementation Gaps

| Gap | Documents | Current State | Priority |
|-----|-----------|---------------|----------|
| Pattern taxonomy registry | b2 | Flat effects[] array | HIGH |
| Narrative engine integration | b3 | Partial ShowDirector | HIGH |
| FastLED optimization utilities | b4, c3 | Duplicated in effects | HIGH |
| PR template automation | c3 | No enforcement | MEDIUM |
| Chromatic dispersion effects | b1 | No implementations | MEDIUM |
| Tension curve system | b3 | Not implemented | MEDIUM |
| Visual verification automation | c3 | Manual only | LOW |
| Pattern relationship graph | b2 | Not implemented | LOW |

### Document Overlap/Redundancy

| Topic | Primary | Redundant | Recommendation |
|-------|---------|-----------|----------------|
| Storytelling | b3 (663 lines) | a2, c2 | Keep all; different detail levels |
| Implementation | b4 (925 lines) | a3, c3 | Deprecate a3; keep b4 (detail) and c3 (quick ref) |
| Creative guide | c1 (164 lines) | a1 | Deprecate a1; c1 is more complete |

---

# 3. Visual Comparison Matrix

## 3.1 Priority vs Impact Grid

```
                    ┌─────────────────────────────────────────────────────────────┐
                    │                        IMPACT                               │
                    │     TACTICAL           OPERATIONAL          STRATEGIC       │
         ┌──────────┼─────────────────────────────────────────────────────────────┤
         │          │                        │                    │               │
         │   HIGH   │ • Chromatic           │ • FastLED          │ • Pattern     │
         │          │   Dispersion          │   Utilities [3]    │   Registry [1]│
         │          │   Effects [5]         │                    │               │
         │          │                        │ • PR Template      │ • Narrative   │
PRIORITY │          │                        │   Enforcement [4]  │   Engine [2]  │
         ├──────────┼─────────────────────────────────────────────────────────────┤
         │          │                        │                    │               │
         │  MEDIUM  │ • Visual              │ • Tension Curve    │ • Zone-Aware  │
         │          │   Verification        │   System           │   Parameters  │
         │          │   Automation          │                    │               │
         ├──────────┼─────────────────────────────────────────────────────────────┤
         │          │                        │                    │               │
         │   LOW    │ • Document            │ • Pattern          │               │
         │          │   Consolidation       │   Relationship     │               │
         │          │                        │   Graph            │               │
         └──────────┴─────────────────────────────────────────────────────────────┘

[n] = Top 5 Recommendation number
```

## 3.2 Implementation Complexity Matrix

```
┌─────────────────────────────────┬──────────┬────────────┬───────────┬────────────┐
│ Recommendation                  │ Effort   │ Risk       │ Dependencies │ Payoff   │
├─────────────────────────────────┼──────────┼────────────┼───────────┼────────────┤
│ 1. Pattern Taxonomy Registry    │ Medium   │ Medium     │ None         │ High     │
│    - New header with metadata   │ (2-3d)   │            │              │          │
│    - Refactor effect array      │          │            │              │          │
├─────────────────────────────────┼──────────┼────────────┼───────────┼────────────┤
│ 2. Narrative Engine             │ High     │ High       │ #1           │ Very High│
│    - Tension curve system       │ (5-7d)   │            │              │          │
│    - Phase sync integration     │          │            │              │          │
│    - ShowDirector refactor      │          │            │              │          │
├─────────────────────────────────┼──────────┼────────────┼───────────┼────────────┤
│ 3. FastLED Utilities            │ Low      │ Low        │ None         │ Medium   │
│    - Extract common patterns    │ (1d)     │            │              │          │
│    - Document usage             │          │            │              │          │
├─────────────────────────────────┼──────────┼────────────┼───────────┼────────────┤
│ 4. PR Template Enforcement      │ Low      │ Low        │ None         │ Medium   │
│    - GitHub template file       │ (0.5d)   │            │              │          │
│    - Checklist automation       │          │            │              │          │
├─────────────────────────────────┼──────────┼────────────┼───────────┼────────────┤
│ 5. Chromatic Dispersion Library │ Medium   │ Medium     │ #3           │ High     │
│    - Implement b1 equations     │ (2-3d)   │            │              │          │
│    - Create 3+ effects          │          │            │              │          │
│    - Hardware validation        │          │            │              │          │
└─────────────────────────────────┴──────────┴────────────┴───────────┴────────────┘

Legend: Effort (days), Risk (implementation risk), Payoff (value delivered)
```

## 3.3 Document Coverage Heat Map

```
                        ┌───────────────────────────────────────────────────────┐
                        │              TOPIC COVERAGE BY DOCUMENT               │
                        ├────────┬────────┬────────┬────────┬────────┬──────────┤
                        │Physics │Taxonomy│Narrative│Implement│Testing │Workflow │
          ┌─────────────┼────────┼────────┼────────┼────────┼────────┼──────────┤
          │ a1          │   ●    │   ○    │   ●    │   ○    │   ○    │    ○     │
          │ a2          │   ○    │   ○    │   ●●●  │   ○    │   ○    │    ○     │
A-Series  │ a3          │   ○    │   ○    │   ○    │   ●●   │   ●    │    ●     │
          ├─────────────┼────────┼────────┼────────┼────────┼────────┼──────────┤
          │ b1          │  ●●●●  │   ○    │   ○    │   ●    │   ○    │    ○     │
          │ b2          │   ●    │  ●●●●  │   ●    │   ○    │   ○    │    ○     │
B-Series  │ b3          │   ○    │   ●    │  ●●●●  │   ●    │   ○    │    ○     │
          │ b4          │   ●    │   ●    │   ●    │  ●●●●  │  ●●●   │   ●●●●   │
          │ b5          │   ●    │   ●    │   ●    │   ●    │   ●    │    ●     │
          ├─────────────┼────────┼────────┼────────┼────────┼────────┼──────────┤
          │ c1          │   ●●   │   ●    │   ●    │   ●●   │   ○    │    ○     │
C-Series  │ c2          │   ○    │   ○    │  ●●●   │   ●    │   ○    │    ○     │
          │ c3          │   ○    │   ○    │   ○    │  ●●●   │  ●●●   │   ●●●    │
          └─────────────┴────────┴────────┴────────┴────────┴────────┴──────────┘

Legend: ○ = None, ● = Light, ●● = Moderate, ●●● = Heavy, ●●●● = Definitive
```

---

# 4. Implementation Checklist

## Phase 1: Foundation (Week 1)

### 4.1.1 FastLED Optimization Utilities
**Owner:** Core Developer
**Dependencies:** None
**Source:** b4:420-450

- [ ] Create `src/effects/utils/FastLEDOptim.h`
- [ ] Extract sin16/cos16 wrapper functions
- [ ] Extract scale8/qadd8/qsub8 helpers
- [ ] Add beatsin8/beatsin16 timing utilities
- [ ] Document each function with usage examples
- [ ] Update 3 existing effects to use utilities
- [ ] Verify no performance regression

**Success Criteria:**
- Single header provides all optimized math
- At least 20% code reduction in refactored effects
- Build passes with no warnings

**Risk Mitigation:**
- Create branch for refactoring
- Run performance benchmarks before/after
- Keep original implementations until validated

---

### 4.1.2 PR Template Creation
**Owner:** Project Lead
**Dependencies:** None
**Source:** c3:16-45

- [ ] Create `.github/PULL_REQUEST_TEMPLATE.md`
- [ ] Include pattern specification sections from c3
- [ ] Add compliance checkboxes (centre-origin, no-rainbows)
- [ ] Add performance budget section
- [ ] Document in CONTRIBUTING.md

**Success Criteria:**
- All new effect PRs use template
- Reviewers can quickly verify compliance
- Template matches c3 specification exactly

**Risk Mitigation:**
- Start with voluntary adoption
- Gather feedback for two weeks before enforcement

---

## Phase 2: Infrastructure (Weeks 2-3)

### 4.2.1 Pattern Taxonomy Registry
**Owner:** Core Developer
**Dependencies:** 4.1.1 (for utility patterns)
**Source:** b2:45-767

- [ ] Create `src/effects/PatternRegistry.h`
- [ ] Define `PatternFamily` enum (10 families from b2)
- [ ] Define `PatternMetadata` struct:
  ```cpp
  struct PatternMetadata {
    const char* name;
    PatternFamily family;
    uint8_t tags;  // bitfield: STANDING|TRAVELING|MOIRE|DEPTH|SPECTRAL
    const char* story;  // one-sentence
  };
  ```
- [ ] Extend `Effect` struct in effects.h with metadata pointer
- [ ] Create family lookup functions
- [ ] Add API endpoint `/api/v1/effects/families`
- [ ] Update web UI effect browser with family filter
- [ ] Tag all 46 existing effects

**Success Criteria:**
- API returns effects grouped by family
- Web UI shows family filter dropdown
- 100% of effects have metadata

**Risk Mitigation:**
- Make metadata optional initially (nullptr)
- Gradual migration of effects over time
- Preserve backward compatibility with existing API

---

### 4.2.2 Chromatic Dispersion Effects
**Owner:** Effect Developer
**Dependencies:** 4.1.1 (for FastLED utilities)
**Source:** b1:380-450

- [ ] Create `src/effects/strip/LGPChromaticEffects.cpp`
- [ ] Implement core dispersion function:
  ```cpp
  // From b1:400-420
  CRGB chromaticDispersion(float position, float aberration, float phase) {
    float normalizedDist = abs(position - 79.5f) / 79.5f;
    float redFocus = sin((normalizedDist - 0.1f * aberration) * PI + phase);
    float greenFocus = sin(normalizedDist * PI + phase);
    float blueFocus = sin((normalizedDist + 0.1f * aberration) * PI + phase);
    return CRGB(
      constrain(redFocus * 255, 0, 255),
      constrain(greenFocus * 255, 0, 255),
      constrain(blueFocus * 255, 0, 255)
    );
  }
  ```
- [ ] Effect 1: Chromatic Lens (static aberration)
- [ ] Effect 2: Chromatic Pulse (aberration sweeps from centre)
- [ ] Effect 3: Chromatic Interference (dual-edge with dispersion)
- [ ] Register in effects[] array
- [ ] Hardware validation on LGP

**Success Criteria:**
- 3 new effects in effects[] array
- Visual validation shows RGB separation at edges
- No rainbow cycling (dispersion is controlled separation)

**Risk Mitigation:**
- Constrain aberration parameter to prevent rainbow-like appearance
- Hardware test before merge
- Document physics basis for code reviewers

---

## Phase 3: Integration (Weeks 4-5)

### 4.3.1 BUILD/HOLD/RELEASE/REST Engine
**Owner:** Core Developer
**Dependencies:** 4.2.1 (for pattern-aware transitions)
**Source:** b3:1-200

- [ ] Create `src/core/NarrativeTension.h`:
  ```cpp
  class NarrativeTension {
    float m_tension;  // 0.0 - 1.0
    NarrativePhase m_phase;
    uint32_t m_phaseStartMs;

  public:
    void setPhase(NarrativePhase phase, uint32_t durationMs);
    float getTension();  // current tension value
    float getTempoMultiplier();  // for speed modulation
    void update();  // call each frame
  };
  ```
- [ ] Define tension curves per phase (b3:50-80):
  - BUILD: exponential rise
  - HOLD: plateau with micro-variations
  - RELEASE: exponential decay
  - REST: near-zero with subtle drift
- [ ] Integrate with ShowDirector chapter transitions
- [ ] Add tension-modulated parameter scaling
- [ ] Expose tension in API status response
- [ ] Add tension visualization to web UI (progress bar)

**Success Criteria:**
- Shows automatically modulate tempo based on phase
- API reports current tension value (0-100)
- Web UI shows phase indicator and tension level

**Risk Mitigation:**
- Make tension modulation optional (config flag)
- Provide manual override via API
- Log phase transitions for debugging

---

### 4.3.2 Visual Verification Tooling
**Owner:** QA Developer
**Dependencies:** 4.2.1 (for pattern metadata)
**Source:** c3:78-106

- [ ] Create verification checklist in code comments
- [ ] Add `--validate` flag to serial menu:
  ```
  > validate <effect_id>
  Checking: lgpStandingWave
  [✓] Centre-origin: brightest at LED 79-80
  [✓] No rainbow: hue range < 60°
  [✓] Frame rate: 125 FPS (target: 120)
  [?] Box count: manual verification needed
  ```
- [ ] Implement automatic checks:
  - Centre brightness check (LED 79-80 should be peak)
  - Hue span check (should be < 60° for no-rainbows)
  - Frame rate measurement
- [ ] Document manual verification steps

**Success Criteria:**
- Serial command validates any effect
- Catches 80%+ of constraint violations automatically
- Clear guidance for manual verification

**Risk Mitigation:**
- Start with warnings, not failures
- Allow override for intentional exceptions (with documentation)

---

## Phase 4: Polish (Week 6+)

### 4.4.1 Document Consolidation
**Owner:** Documentation Lead
**Dependencies:** All previous phases

- [ ] Archive a1 (superseded by c1)
- [ ] Archive a3 (superseded by b4/c3)
- [ ] Add deprecation notices to archived docs
- [ ] Update b5 cross-reference to reflect changes
- [ ] Create single-page quick reference card

**Success Criteria:**
- No duplicate guidance across documents
- Clear "start here" entry point
- Quick reference fits on one printed page

---

### 4.4.2 Pattern Relationship Graph
**Owner:** Effect Developer
**Dependencies:** 4.2.1 (Pattern Registry)
**Source:** b2:600-767

- [ ] Define relationship types in PatternRegistry:
  ```cpp
  enum RelationType { PARENT, CHILD, COMPLEMENT, TRANSITION_PAIR };
  ```
- [ ] Add relationship data to pattern metadata
- [ ] Create API endpoint `/api/v1/effects/{id}/related`
- [ ] Update web UI to show "Related Patterns" section
- [ ] Document relationship rules for new patterns

**Success Criteria:**
- API returns related patterns for any effect
- Web UI shows clickable related patterns
- Relationships documented in pattern metadata

---

## Risk Assessment Summary

| Recommendation | Risk Level | Primary Risk | Mitigation |
|----------------|------------|--------------|------------|
| 1. Pattern Registry | Medium | Effect array refactor breaks existing code | Feature flag, gradual migration |
| 2. Narrative Engine | High | Complex integration with multiple subsystems | Optional tension modulation, thorough testing |
| 3. FastLED Utilities | Low | Minor performance regression | Benchmarking before/after |
| 4. PR Templates | Low | Developer adoption resistance | Start voluntary, then enforce |
| 5. Chromatic Effects | Medium | Rainbow-like appearance | Physics-based constraints, hardware validation |

---

## Success Metrics Dashboard

| Metric | Current | Target | Measurement |
|--------|---------|--------|-------------|
| Effects with metadata | 0/46 | 46/46 | Pattern Registry coverage |
| Narrative-aware shows | 0/10 | 10/10 | Shows using tension system |
| Code duplication | ~15% | <5% | Utility function adoption |
| PR template usage | 0% | 100% | GitHub template compliance |
| Chromatic effects | 0 | 3+ | New effects count |
| Automated validation | 0% | 80% | Constraint checks passing |

---

# Appendix: Document Inventory

| ID | Document | Lines | Primary Focus | Recommended Action |
|----|----------|-------|---------------|-------------------|
| a1 | LGP_CREATIVE_PATTERN_GUIDE.md | ~200 | Artist lens | Archive (superseded by c1) |
| a2 | LIGHTSHOW_STORYTELLING_FRAMEWORK.md | ~250 | Narrative intro | Keep (different audience than b3) |
| a3 | PATTERN_IMPLEMENTATION_PLAYBOOK.md | ~300 | Basic implementation | Archive (superseded by b4/c3) |
| b1 | LGP_OPTICAL_PHYSICS_REFERENCE.md | 640 | Physics equations | **Primary reference** |
| b2 | LGP_PATTERN_TAXONOMY.md | 767 | 83 patterns | **Primary reference** |
| b3 | LGP_STORYTELLING_FRAMEWORK.md | 663 | BUILD/HOLD/RELEASE/REST | **Primary reference** |
| b4 | LGP_PATTERN_DEVELOPMENT_PLAYBOOK.md | 925 | Full workflow | **Primary reference** |
| b5 | LGP_CREATIVE_PATTERN_DEVELOPMENT_GUIDE.md | 294 | Unified overview | Keep (navigation guide) |
| c1 | CREATIVE_PATTERN_DEVELOPMENT_GUIDE.md | 164 | Quick creative ref | **Keep as quick reference** |
| c2 | STORYTELLING_FRAMEWORK.md | 149 | Quick narrative ref | **Keep as quick reference** |
| c3 | IMPLEMENTATION_PLAYBOOK_LIGHT_PATTERNS.md | 124 | Quick impl ref | **Keep as quick reference** |

---

*Report generated by Claude Code based on analysis of 11 LGP research documents totaling ~4,500 lines.*
