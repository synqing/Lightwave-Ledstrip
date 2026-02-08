# VFX Research Document Index

## Research Completed: Game & Shader Techniques for LED Strip Effects

**Date:** 2026-02-08  
**Scope:** 320-LED addressable strip (WS2812), 120 FPS target, ESP32-S3  
**Total Documentation:** 3,574 lines across 5 files, 124 KB  
**Sources:** 40+ references (game development, VFX, procedural animation, embedded systems)

---

## Quick Navigation

### I Need to... → Read This File

| Need | Document | Section | Purpose |
|------|----------|---------|---------|
| **Understand VFX principles** | GAME_VFX_TO_LED_PATTERNS.md | All 10 sections | Comprehensive theory & patterns |
| **Copy working code** | ALGORITHM_IMPLEMENTATIONS.md | All sections | Production-ready C++ |
| **Find an effect quickly** | QUICK_REFERENCE.md | "Visual Goal → Technique" table | Fast lookup |
| **Make a decision** | TECHNIQUES_MATRIX.md | "Decision Tree" | Choose technique per goal |
| **Navigate all docs** | README.md | "Document Guide" | Overview & integration |
| **See complete table** | TECHNIQUES_MATRIX.md | "30 Common Effects" | Summary of all effects |

---

## File Descriptions

### 1. GAME_VFX_TO_LED_PATTERNS.md (39 KB, 1,255 lines)

**The Master Reference** - Deep dive into visual effects design for LED strips.

#### Contents:
- **§1 Particle Systems** - Ring buffers, emission patterns, shock waves, performance tuning
- **§2 Shader Techniques** - Bloom, glow, chromatic aberration approximations for discrete LEDs
- **§3 Procedural Animation** - Perlin/Simplex noise, wave interference, cellular automata algorithms
- **§4 Hit/Impact Effects** - Layered burst design, shockwave physics, afterglow mechanics
- **§5 Energy/Power Effects** - Charging visualization, release mechanics, visual feedback
- **§6 Trail Effects** - Motion trails, history buffers, velocity-based stretching
- **§7 State Communication** - Health indicators, action feedback, warning signals
- **§8 Integration Patterns** - Effect composition, choreography, multi-effect synchronization
- **§9 Performance & Memory** - Frame budgets, optimization tips, code patterns to avoid
- **§10 Practical Examples** - Electric storm, resonance with beat sync, complete effects

#### Best For:
- Foundational understanding of VFX principles
- Design decisions and architectural patterns
- Theory behind each technique
- Complete code examples with explanations

#### Time to Read:
- Full: 1-2 hours
- Sections: 10-15 minutes each
- Quick skim: 30 minutes

---

### 2. ALGORITHM_IMPLEMENTATIONS.md (28 KB, 930 lines)

**Production Code Library** - Ready-to-integrate C++ implementations.

#### Contents:
- **§1 Perlin Noise 1D** - Complete Arduino-optimized implementation with lookup tables
- **§2 Bloom Effect** - Multi-pass Gaussian bloom and fast local bloom
- **§3 Wave Interference** - Multiple source superposition with fast sine approximations
- **§4 Cellular Automata** - 1D rule-based pattern generation (Rule 30, 110, 184, etc.)
- **§5 Particle System** - Ring buffer pool with burst emission patterns
- **§6 Performance Profiling** - Timing instrumentation macros for optimization
- **§7 Fast Math Utilities** - Lookup tables, sine approximations, integer optimizations

#### Code Quality:
- No external dependencies
- Minimal SRAM footprint
- Compatible with ESP32 firmware structure
- Includes usage examples
- Performance characteristics documented

#### Best For:
- Direct integration into firmware
- Copy-paste implementation
- Reference implementations
- Performance optimization patterns

#### Time to Read:
- Full: 45 minutes to 1 hour
- Individual algorithms: 10-15 minutes
- Copy & adapt: 5-10 minutes

---

### 3. QUICK_REFERENCE.md (12 KB, 377 lines)

**Cheatsheet & Quick Lookup** - Fast reference for common tasks.

#### Contents:
- **Visual Goal → Technique Table** - 30 effect goals mapped to implementations
- **Code Snippets** - 15+ ready-to-use effect implementations (5-15 lines each)
- **Shader Approximations** - Compare GPU techniques to LED adaptations
- **Performance Budget Checklist** - Component-by-component timing breakdown
- **Memory Budget Checklist** - SRAM allocation per technique
- **Colour Theory** - Emotional impact, HSV space, colour shifts
- **Beat Sync Integration** - Tempo locking, frequency mapping, phase calculation
- **Debugging Tips** - Common pitfalls, fixes, profiling procedures
- **Quick Wins** - 5 easy effects with big visual impact

#### Best For:
- Fast lookups during implementation
- Quick inspiration for new effects
- Debugging checklist
- Performance verification
- Memory planning

#### Time to Read:
- Full: 15-20 minutes
- Lookup tables: 5 minutes
- Code snippets: 1-2 minutes each
- Checklists: 5 minutes

---

### 4. TECHNIQUES_MATRIX.md (21 KB, 604 lines)

**Decision Matrix & Selection Guide** - Choose the right technique for your goal.

#### Contents:
- **Decision Tree by Category** - 8 visual categories with flowchart decision paths
- **Performance Comparison** - Techniques ranked by rendering time
- **Memory Comparison** - Techniques ranked by SRAM/PSRAM allocation
- **Selection Flowchart** - Visual decision tree from idea to implementation
- **30 Common Effects Table** - Complete summary with time, memory, complexity, impact
- **Integration Path** - 5-phase workflow from concept to optimized effect
- **Quick Selection by Constraint** - Choose based on available time or memory

#### Best For:
- Choosing between similar techniques
- Understanding performance tradeoffs
- Planning complex multi-layer effects
- Resource allocation decisions
- Visual decision trees

#### Time to Read:
- Full: 30-40 minutes
- Decision tree: 10 minutes
- Performance table: 5 minutes
- Flowchart: 5 minutes

---

### 5. README.md (15 KB, 408 lines)

**Navigation & Integration Guide** - Overview of all research.

#### Contents:
- **Research Overview** - Scope, sources, constraints
- **Document Guide** - What each file contains
- **Key Insights by Domain** - Summarized findings per category
- **Integration Checklist** - Adding effects to firmware
- **Technique Selection Decision Tree** - Visual flowchart
- **Performance Summary Table** - All techniques at a glance
- **Constraint Reminders** - Hard constraints, visual constraints
- **Research Methodology** - Source categories, key references
- **Quick Start Guides** - For different user roles
- **FAQ** - Common questions answered

#### Best For:
- First-time orientation
- Navigation between documents
- Integration planning
- Understanding constraints
- Finding sources

#### Time to Read:
- Full: 20-30 minutes
- Sections: 5-10 minutes each
- Quick start: 5 minutes

---

## Reading Paths by Role

### Visual Effect Designer
**Goal:** Create cool effects  
**Reading Order:**
1. README.md - Quick start guide
2. QUICK_REFERENCE.md - "Visual Goal → Technique Table"
3. TECHNIQUES_MATRIX.md - Decision tree
4. GAME_VFX_TO_LED_PATTERNS.md - Relevant sections
5. ALGORITHM_IMPLEMENTATIONS.md - Copy code

**Time Commitment:** 1-2 hours initial, 5-10 minutes per new effect

### Performance Engineer
**Goal:** Optimize and profile  
**Reading Order:**
1. QUICK_REFERENCE.md - "Performance Budget Checklist"
2. GAME_VFX_TO_LED_PATTERNS.md - § 9 (Performance & Memory)
3. ALGORITHM_IMPLEMENTATIONS.md - § 6 (Profiling)
4. Other docs as needed for specific effects

**Time Commitment:** 30 minutes initial, 10-15 minutes per optimization

### Audio Engineer (Beat Sync)
**Goal:** Implement music-reactive effects  
**Reading Order:**
1. QUICK_REFERENCE.md - "Beat Sync Integration"
2. GAME_VFX_TO_LED_PATTERNS.md - § 3.3, § 7
3. TECHNIQUES_MATRIX.md - "Music-Reactive Effects"
4. ALGORITHM_IMPLEMENTATIONS.md - § 3 (Wave Interference)

**Time Commitment:** 30-45 minutes, focused on audio sections

### Researcher / Student
**Goal:** Understand full system  
**Reading Order:**
1. README.md - Full document
2. GAME_VFX_TO_LED_PATTERNS.md - All sections
3. TECHNIQUES_MATRIX.md - Decision trees and tables
4. ALGORITHM_IMPLEMENTATIONS.md - Code implementations
5. QUICK_REFERENCE.md - Checklists and summaries

**Time Commitment:** 3-4 hours deep dive

### Busy Developer (Quick Integration)
**Goal:** Add one effect ASAP  
**Reading Order:**
1. TECHNIQUES_MATRIX.md - Decision tree (find your effect)
2. QUICK_REFERENCE.md - Copy code snippet
3. ALGORITHM_IMPLEMENTATIONS.md - Get dependencies
4. Integrate and test

**Time Commitment:** 15-30 minutes

---

## Content Cross-Reference

### Perlin Noise
- **Theory:** GAME_VFX_TO_LED_PATTERNS.md § 3.1
- **Code:** ALGORITHM_IMPLEMENTATIONS.md § 1
- **Snippet:** QUICK_REFERENCE.md "Perlin Noise Effect"
- **Lookup:** TECHNIQUES_MATRIX.md (Organic effects)

### Particle Systems
- **Theory:** GAME_VFX_TO_LED_PATTERNS.md § 1
- **Code:** ALGORITHM_IMPLEMENTATIONS.md § 5
- **Snippet:** QUICK_REFERENCE.md "Particle Burst"
- **Lookup:** TECHNIQUES_MATRIX.md (Impact effects)

### Bloom / Glow
- **Theory:** GAME_VFX_TO_LED_PATTERNS.md § 2.1
- **Code:** ALGORITHM_IMPLEMENTATIONS.md § 2
- **Snippet:** QUICK_REFERENCE.md "Bloom Effect"
- **Lookup:** TECHNIQUES_MATRIX.md (Light effects)

### Wave Interference
- **Theory:** GAME_VFX_TO_LED_PATTERNS.md § 3.3
- **Code:** ALGORITHM_IMPLEMENTATIONS.md § 3
- **Snippet:** QUICK_REFERENCE.md "Wave Interference"
- **Lookup:** TECHNIQUES_MATRIX.md (Music-reactive)

### Cellular Automata
- **Theory:** GAME_VFX_TO_LED_PATTERNS.md § 3.4
- **Code:** ALGORITHM_IMPLEMENTATIONS.md § 4
- **Lookup:** TECHNIQUES_MATRIX.md (Evolving patterns)

### Impact Effects
- **Theory:** GAME_VFX_TO_LED_PATTERNS.md § 4
- **Code:** GAME_VFX_TO_LED_PATTERNS.md (example in section)
- **Lookup:** TECHNIQUES_MATRIX.md (Impact category)

### State Communication
- **Theory:** GAME_VFX_TO_LED_PATTERNS.md § 7
- **Code:** QUICK_REFERENCE.md (effect classes)
- **Lookup:** TECHNIQUES_MATRIX.md (State effects)

### Beat Synchronization
- **Theory:** GAME_VFX_TO_LED_PATTERNS.md § 3.3, § 7.1
- **Snippets:** QUICK_REFERENCE.md "Beat Sync Integration"
- **Lookup:** TECHNIQUES_MATRIX.md (Music-reactive category)

---

## Performance Reference

### By Technique

| Technique | Time | Memory | Reference |
|-----------|------|--------|-----------|
| Perlin (1 octave) | 4.8ms | 1KB | ALGORITHMS § 1, QUICK_REF snippets |
| Perlin (3 octave) | 14.4ms | 1KB | GAME_VFX § 3.1, TECHNIQUES table |
| Bloom (3-pass) | 1.5ms | 2KB | ALGORITHMS § 2, QUICK_REF |
| Bloom (local) | 0.5ms | 0B | GAME_VFX § 2.1, QUICK_REF |
| Wave interference | 2-5ms | 500B | ALGORITHMS § 3, TECHNIQUES |
| Cellular automata | 1-2ms | 320B | ALGORITHMS § 4, TECHNIQUES |
| Particle (64) | 1-3ms | 1.5KB | ALGORITHMS § 5, GAME_VFX § 1 |
| Trail effect | 0.5-1ms | 240B | GAME_VFX § 6, QUICK_REF |
| Chromatic aberration | 0.5-1ms | 0B | GAME_VFX § 2.2 |

### By Category

| Category | Time Range | Best Reference |
|----------|-----------|-----------------|
| Organic/Procedural | 1-15ms | GAME_VFX § 3, TECHNIQUES Matrix |
| Impact/Particle | 1-5ms | GAME_VFX § 1 & 4, ALGORITHMS § 5 |
| Light/Glow | 0.5-2ms | GAME_VFX § 2, ALGORITHMS § 2 |
| State/UI | <0.5ms | GAME_VFX § 7, QUICK_REF |
| Power/Energy | 1-3ms | GAME_VFX § 5, QUICK_REF |
| Music-Reactive | 0.5-5ms | GAME_VFX § 3.3, QUICK_REF |

---

## Memory Reference

| Component | Size | Notes |
|-----------|------|-------|
| Perlin lookup tables | 1KB | Stored in PROGMEM (flash) |
| Bloom work buffers | 2KB | Temporary, reused per frame |
| Particle pool (64) | 1.5KB | 24 bytes per particle |
| CA state | 320B | Single generation |
| History buffer (30) | 240B | Motion trail |
| Wave sources | 500B | 4 sources × ~125B |
| FastMath tables | ~1KB | Sine, exp lookups |

**Total Typical Effect:** 2-4 KB SRAM  
**Total Available:** ~320 KB internal + 8 MB PSRAM

---

## Known Constraints

### Hard Constraints
- **No malloc/new in render loops** (heap fragmentation)
- **PSRAM mandatory** (firmware requirement)
- **Centre origin** (design requirement)
- **No rainbows** (visual design constraint)
- **120 FPS target** (performance requirement)

### Soft Constraints
- Keep individual effects < 4 KB SRAM
- Total effect budget < 8 ms per frame
- Avoid floating-point division in inner loops
- Cache frequently used calculations

---

## Research Statistics

| Metric | Value |
|--------|-------|
| Total documentation | 3,574 lines |
| Total file size | 124 KB |
| Number of documents | 5 |
| Source references | 40+ |
| Code examples | 50+ |
| Performance benchmarks | 30+ |
| Decision tables | 8 |
| Effect categories | 8 |
| Common effects documented | 30 |
| Algorithms with full code | 7 |

---

## How to Use This Research

### Step 1: Decide What to Build
Use TECHNIQUES_MATRIX.md decision trees or README.md quick start guides.

### Step 2: Understand the Technique
Read relevant section in GAME_VFX_TO_LED_PATTERNS.md.

### Step 3: Get the Code
Find implementation in ALGORITHM_IMPLEMENTATIONS.md or QUICK_REFERENCE.md.

### Step 4: Integrate into Firmware
Follow integration checklist in README.md.

### Step 5: Optimize & Verify
Use performance profiling from ALGORITHM_IMPLEMENTATIONS.md § 6.

### Step 6: Tune Visual Quality
Adjust parameters and test with physical LEDs.

---

## Support & References

### Need help finding something?
- **By effect name:** TECHNIQUES_MATRIX.md "30 Common Effects" table
- **By technique:** README.md "Content Cross-Reference"
- **By performance:** QUICK_REFERENCE.md "Performance Budget Checklist"
- **By memory:** QUICK_REFERENCE.md "Memory Budget Checklist"

### Need source material?
All documents include inline citations. Major sources:
- LearnOpenGL (shader techniques)
- Game Developer Magazine (particle systems)
- The Book of Shaders (procedural generation)
- Boris FX (VFX design)
- FastLED library documentation

### Want to contribute?
Each document has a "Contributing" section outlining format and style.

---

**Research Completed:** 2026-02-08  
**Last Updated:** 2026-02-08  
**Status:** Complete and ready for integration

---

[Return to README.md](README.md) | [Browse all documents](.)

