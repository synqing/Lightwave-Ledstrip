# Implementation Audit Report - Part 4: Phased Action Plan

**Date:** January 2025  
**Focus:** Structured timeline with clear milestones, prioritized by impact and feasibility

---

## 4. Phased Action Plan

This section provides a detailed, prioritized roadmap for addressing identified gaps. Actions are organized into phases based on dependencies, risk, and strategic value.

---

## Phase 1: Foundation & API Surface (Week 1)

**Goal:** Expose Pattern Registry metadata via APIs and create FastLED utility layer

**Priority:** P0 (Critical gaps 3.1.1, 3.2.2)

### Task 1.1: Expose Pattern Registry via REST/WebSocket APIs

**Owner:** Backend Developer  
**Effort:** 2-3 days  
**Dependencies:** None

**Actions:**
1. Modify `WebServer::handleEffectsMetadata()` to query `PatternRegistry::getPatternMetadata(effectId)`
   - Replace ID-based category assignment with registry family
   - Include tags, story, optical intent in response
   - File: `v2/src/network/WebServer.cpp` (around line 1008)

2. Add REST endpoint `/api/v1/effects/families`
   - Return list of all 10 families with counts
   - Use `PatternRegistry::getFamilyCount(PatternFamily)`
   - File: `v2/src/network/WebServer.cpp`

3. Update WebSocket `effects.getCategories` handler
   - Replace hardcoded categories with registry families
   - Return family names, IDs, pattern counts
   - File: `v2/src/network/WebServer.cpp` (around line 2318)

4. Add WebSocket `effects.getByFamily` command
   - Accept `familyId` parameter (0-9)
   - Return array of effect IDs in that family
   - Use `PatternRegistry::getPatternsByFamily()`

**Success Criteria:**
- ✅ REST `/api/v1/effects/metadata?id=N` returns registry family/tags/story
- ✅ REST `/api/v1/effects/families` returns all 10 families with counts
- ✅ WebSocket `effects.getCategories` returns registry families
- ✅ WebSocket `effects.getByFamily` returns filtered effect lists
- ✅ All responses validated against PatternRegistry data

**Verification:**
- Unit tests for API handlers
- Integration test: query metadata for all 65 effects, verify family assignments match registry
- Manual API testing via curl/Postman

---

### Task 1.2: Create FastLED Optimization Utility Header

**Owner:** Core Developer  
**Effort:** 1-2 days  
**Dependencies:** None

**Actions:**
1. Create `v2/src/effects/utils/FastLEDOptim.h`
   - Extract sin16/cos16 wrapper functions with inline documentation
   - Extract scale8/qadd8/qsub8 helper macros/functions
   - Add beatsin8/beatsin16 timing utility wrappers
   - Include usage examples in comments

2. Refactor 3 representative effects to use utilities
   - `effectPlasma()` - uses sin16
   - `effectSinelon()` - uses beatsin16
   - `effectConfetti()` - uses scale8 patterns
   - File: `v2/src/effects/CoreEffects.cpp`

3. Measure code reduction
   - Count lines before/after refactoring
   - Target: ≥20% reduction in refactored effects

4. Performance validation
   - Benchmark refactored effects (ensure ≥120 FPS maintained)
   - Compare before/after FPS measurements

**Success Criteria:**
- ✅ `FastLEDOptim.h` exists with all recommended utilities
- ✅ 3+ effects refactored to use utilities
- ✅ Code reduction ≥20% in refactored effects
- ✅ No performance regression (FPS ≥120 for all refactored effects)
- ✅ Build passes with no warnings

**Verification:**
- Code review: verify utility functions are correct
- Performance profiling: measure FPS before/after
- Build validation: ensure no compilation errors

---

### Task 1.3: Update UI to Display Pattern Metadata

**Owner:** Frontend Developer  
**Effort:** 2-3 days  
**Dependencies:** Task 1.1 (API endpoints must exist)

**Actions:**
1. Add family filter dropdown to effect browser
   - Query `/api/v1/effects/families` for family list
   - Filter effects by selected family
   - File: `lightwave-dashboard/src/components/EffectBrowser.tsx` (or equivalent)

2. Display pattern metadata in effect detail view
   - Show family, tags, story, optical intent
   - Display related patterns (if Task 2.4 completed)
   - File: Effect detail component

3. Add tag-based filtering
   - Checkbox filters for tags (STANDING, TRAVELING, MOIRE, etc.)
   - Combine with family filter

**Success Criteria:**
- ✅ UI displays all 10 pattern families in filter dropdown
- ✅ Filtering by family shows correct effects
- ✅ Effect detail view shows metadata (family, tags, story)
- ✅ Tag filters work correctly

**Verification:**
- Manual UI testing: filter by each family, verify correct effects displayed
- Verify metadata displays correctly for all 65 effects

---

**Phase 1 Deliverables:**
- Pattern Registry fully exposed via REST/WebSocket
- FastLED utility layer created and validated
- UI updated with family/tag filters
- Documentation updated with API changes

**Phase 1 Risk Mitigation:**
- Create feature branch: `feature/pattern-registry-api`
- Test API endpoints thoroughly before UI integration
- Keep FastLED utility refactoring separate (different branch if needed)

---

## Phase 2: Taxonomy Alignment & Chromatic Effects (Weeks 2-3)

**Goal:** Align effect count with registry, implement physics-accurate chromatic dispersion

**Priority:** P0 (Critical gap 3.1.2), P1 (Major gap 3.2.1)

### Task 2.1: Enforce Effect Count Parity

**Owner:** Core Developer  
**Effort:** 1 day  
**Dependencies:** None

**Actions:**
1. Add compile-time assertion for effect count
   - Define `EXPECTED_EFFECT_COUNT = 65` constant
   - Add `static_assert(PATTERN_METADATA_COUNT >= EXPECTED_EFFECT_COUNT)`
   - File: `v2/src/effects/PatternRegistry.cpp`

2. Add runtime validation in `registerAllEffects()`
   - Verify registered count matches EXPECTED_EFFECT_COUNT
   - Log warning if mismatch detected
   - File: `v2/src/effects/CoreEffects.cpp`

3. Document registry metadata strategy
   - Decision: Keep 118 entries (for future effects) or reduce to 65?
   - If keeping 118: Mark inactive entries, filter from API responses
   - If reducing to 65: Update PATTERN_METADATA array

**Success Criteria:**
- ✅ Compile-time assertion prevents accidental count mismatches
- ✅ Runtime validation logs warnings if counts don't match
- ✅ Documentation explains registry metadata strategy
- ✅ API responses only return metadata for implemented effects

**Verification:**
- Build test: verify static_assert works correctly
- Runtime test: verify validation logging works
- API test: verify metadata only returned for IDs 0-64

---

### Task 2.2: Implement Physics-Accurate Chromatic Dispersion Effects

**Owner:** Effect Developer  
**Effort:** 3-4 days  
**Dependencies:** Task 1.2 (FastLED utilities should exist)

**Actions:**
1. Create `v2/src/effects/LGPChromaticEffects.cpp` and `.h`
   - Implement Cauchy equation-based dispersion function
   - Reference: `b1. LGP_OPTICAL_PHYSICS_REFERENCE.md` lines 380-450

2. Implement core dispersion function:
```cpp
CRGB chromaticDispersion(float position, float aberration, float phase) {
    // Normalize position from centre (0.0 = centre, 1.0 = edge)
    float normalizedDist = abs(position - 79.5f) / 79.5f;
    
    // Cauchy equation coefficients for acrylic (n=1.49)
    const float A = 1.49f;
    const float B = 0.0042f;  // Approximate for visible spectrum
    const float C = 0.0001f;
    
    // Wavelength-dependent refractive index
    float lambda_red = 650e-9f;   // 650nm
    float lambda_green = 550e-9f; // 550nm
    float lambda_blue = 450e-9f;  // 450nm
    
    float n_red = A + B / (lambda_red * lambda_red) + C / (lambda_red * lambda_red * lambda_red * lambda_red);
    float n_green = A + B / (lambda_green * lambda_green) + C / (lambda_green * lambda_green * lambda_green * lambda_green);
    float n_blue = A + B / (lambda_blue * lambda_blue) + C / (lambda_blue * lambda_blue * lambda_blue * lambda_blue);
    
    // Focus positions based on refractive index differences
    float redFocus = sin((normalizedDist - 0.1f * aberration * (n_red - n_green)) * PI + phase);
    float greenFocus = sin(normalizedDist * PI + phase);
    float blueFocus = sin((normalizedDist + 0.1f * aberration * (n_blue - n_green)) * PI + phase);
    
    return CRGB(
        constrain((int)(128 + 127 * redFocus), 0, 255),
        constrain((int)(128 + 127 * greenFocus), 0, 255),
        constrain((int)(128 + 127 * blueFocus), 0, 255)
    );
}
```

3. Create three effects:
   - **Chromatic Lens**: Static aberration, lens position controlled by speed encoder
   - **Chromatic Pulse**: Aberration sweeps from centre outward, intensity pulse
   - **Chromatic Interference**: Dual-edge injection with dispersion, interference patterns

4. Register effects in `registerLGPColorMixingEffects()` or new registration function
   - Assign effect IDs (likely 65-67, depending on current count)
   - Update PatternRegistry metadata entries

5. Hardware validation
   - Test on actual LGP hardware
   - Verify RGB separation at edges (visual validation)
   - Ensure no rainbow-like cycling (hue span < 60°)

**Success Criteria:**
- ✅ `LGPChromaticEffects.cpp` exists with Cauchy-based dispersion
- ✅ Three effects implemented and registered
- ✅ Effects pass hardware validation (visual RGB separation confirmed)
- ✅ No rainbow cycling (hue span validated < 60°)
- ✅ Performance ≥120 FPS for all three effects

**Verification:**
- Code review: verify Cauchy equation implementation matches research
- Hardware test: visual validation on LGP, verify physics-accurate separation
- Performance test: FPS measurement for each effect
- Compliance test: hue span calculation < 60°

---

**Phase 2 Deliverables:**
- Effect count parity enforced (compile-time + runtime validation)
- Physics-accurate chromatic dispersion effects implemented
- Hardware validation completed
- Registry metadata updated for new effects

**Phase 2 Risk Mitigation:**
- Cauchy equation implementation may require tuning coefficients for visual accuracy
- Hardware validation essential before merging
- Constrain aberration parameter to prevent rainbow-like appearance

---

## Phase 3: Narrative Integration & Validation Tooling (Weeks 4-5)

**Goal:** Expose narrative system via API, integrate tension into effects, add validation command

**Priority:** P0 (Critical gap 3.1.3), P1 (Major gap 3.2.3)

### Task 3.1: Expose Narrative Status via REST/WebSocket

**Owner:** Backend Developer  
**Effort:** 2 days  
**Dependencies:** None (NarrativeEngine already exists)

**Actions:**
1. Add REST endpoint `/api/v1/narrative/status`
   - Return: tension (0-100), phase (BUILD/HOLD/RELEASE/REST), phaseT (0-1), cycleT (0-1)
   - Include enabled flag and phase durations
   - File: `v2/src/network/WebServer.cpp`

2. Add WebSocket command `narrative.getStatus`
   - Same data as REST endpoint
   - Real-time updates (could add subscription for live updates)

3. Add REST endpoint `/api/v1/narrative/config` (GET/POST)
   - GET: Return current phase durations, curves, breathe amount
   - POST: Update phase durations and configuration
   - Persist to NVS if enabled

**Success Criteria:**
- ✅ REST `/api/v1/narrative/status` returns current narrative state
- ✅ WebSocket `narrative.getStatus` returns same data
- ✅ Configuration endpoint allows phase duration adjustment
- ✅ Settings persist to NVS (optional)

**Verification:**
- API test: query status endpoint, verify values match NarrativeEngine state
- Configuration test: update phase durations, verify they persist

---

### Task 3.2: Add Narrative Tension Visualization to UI

**Owner:** Frontend Developer  
**Effort:** 2-3 days  
**Dependencies:** Task 3.1 (API endpoints must exist)

**Actions:**
1. Add narrative status component to dashboard
   - Display current phase (BUILD/HOLD/RELEASE/REST) with visual indicator
   - Show tension level (0-100) as progress bar
   - Display phase progress (phaseT) as sub-progress bar

2. Add narrative configuration panel
   - Sliders for phase durations
   - Toggle for enable/disable
   - Save/load configuration

3. Real-time updates via WebSocket
   - Subscribe to narrative status updates
   - Update UI every 100ms (or on phase change events)

**Success Criteria:**
- ✅ UI displays current narrative phase with visual indicator
- ✅ Tension progress bar updates in real-time
- ✅ Phase duration configuration works
- ✅ Real-time updates via WebSocket function correctly

**Verification:**
- Manual UI testing: verify tension bar updates during narrative cycle
- Configuration test: adjust phase durations, verify they persist

---

### Task 3.3: Integrate Narrative Tension into Effect Parameters

**Owner:** Core Developer  
**Effort:** 3-4 days  
**Dependencies:** Task 3.1 (NarrativeEngine API should be stable)

**Actions:**
1. Modify 2-3 representative effects to use narrative tension
   - Example: `effectPlasma()` - modulate speed based on `NARRATIVE.getTempoMultiplier()`
   - Example: `effectOcean()` - modulate wave intensity based on `NARRATIVE.getIntensity()`
   - Example: `effectFire()` - modulate spark frequency based on tension

2. Add optional narrative modulation flag
   - Effects can opt-in to narrative modulation
   - Default: narrative modulation disabled (backward compatibility)

3. Document narrative integration pattern
   - Add code examples to effect development guide
   - Show how to query `NARRATIVE.getTension()`, `getTempoMultiplier()`, `getComplexityScaling()`

**Success Criteria:**
- ✅ 2-3 effects demonstrate narrative tension integration
- ✅ Effects respond to narrative phase changes (visual validation)
- ✅ Optional modulation flag preserves backward compatibility
- ✅ Documentation includes integration examples

**Verification:**
- Visual test: run narrative cycle, verify effects respond to tension changes
- Performance test: ensure narrative queries don't impact FPS

---

### Task 3.4: Implement Firmware Validation Command

**Owner:** Core Developer  
**Effort:** 3-4 days  
**Dependencies:** None

**Actions:**
1. Add serial command `validate <effect_id>`
   - Parse effect ID from serial input
   - Switch to effect temporarily
   - Run validation checks

2. Implement validation checks:
   - **Centre-origin check**: Measure brightness at LEDs 79-80 vs edges
     - Capture LED brightness values for 1 second
     - Calculate average brightness at centre (LEDs 79-80)
     - Calculate average brightness at edges (LEDs 0-10, 150-159)
     - Pass if centre brightness > edge brightness * 1.2
   
   - **Hue span check**: Calculate maximum hue difference
     - Capture color values for all LEDs over 1 second
     - Convert RGB to HSV, extract hue values
     - Calculate min/max hue
     - Pass if (max_hue - min_hue) < 60° (accounting for hue wrap-around)
   
   - **Frame rate check**: Measure FPS during effect execution
     - Use existing `RenderStats.currentFPS`
     - Run effect for 2 seconds
     - Pass if FPS ≥ 120

3. Output validation report:
```
> validate 0
Validating effect: Fire (ID 0)
[✓] Centre-origin: PASS (centre brightness 245, edge avg 89, ratio 2.75x)
[✓] Hue span: PASS (hue range 45°, within 60° limit)
[✓] Frame rate: PASS (142 FPS, above 120 FPS target)
[✓] Memory: PASS (static allocation, no heap usage)
Overall: PASS (4/4 checks passed)
```

4. Integrate with existing serial command handler
   - File: `v2/src/main.cpp` (serial command parsing)

**Success Criteria:**
- ✅ Serial command `validate <id>` works for all 65 effects
- ✅ Centre-origin check correctly identifies violations
- ✅ Hue span check correctly calculates hue range
- ✅ Frame rate check uses RenderStats
- ✅ Validation report is clear and actionable

**Verification:**
- Test with known-good effect (should pass all checks)
- Test with edge case (e.g., effect that might fail centre-origin)
- Verify validation doesn't impact system stability

---

**Phase 3 Deliverables:**
- Narrative status exposed via REST/WebSocket
- UI visualization of narrative tension/phase
- 2-3 effects demonstrate narrative integration
- Firmware validation command implemented

**Phase 3 Risk Mitigation:**
- Narrative integration should be optional (opt-in) to preserve backward compatibility
- Validation command should be non-destructive (temporary effect switch)
- Performance impact of narrative queries should be minimal (cached values)

---

## Phase 4: Polish & Documentation (Week 6)

**Goal:** Complete related patterns, update documentation, add integration tests

**Priority:** P2 (Major gap 3.2.4), P3 (Documentation)

### Task 4.1: Implement Related Patterns Parsing

**Owner:** Backend Developer  
**Effort:** 2 days  
**Dependencies:** None

**Actions:**
1. Implement comma-separated string parsing in `PatternRegistry::getRelatedPatterns()`
   - Parse `relatedPatterns` field from metadata
   - Handle edge cases (empty string, single pattern, multiple patterns)
   - Return array of pattern names

2. Add REST endpoint `/api/v1/effects/{id}/related`
   - Query `PatternRegistry::getRelatedPatterns()`
   - Return array of related pattern names with metadata

3. Add WebSocket command `effects.getRelated`
   - Accept effect ID or name
   - Return related patterns array

**Success Criteria:**
- ✅ `getRelatedPatterns()` correctly parses comma-separated strings
- ✅ REST endpoint returns related patterns for any effect
- ✅ WebSocket command works correctly
- ✅ Handles edge cases (no related patterns, invalid effect ID)

**Verification:**
- Unit test: test parsing with various comma-separated strings
- Integration test: verify related patterns returned for effects with metadata

---

### Task 4.2: Update Documentation

**Owner:** Documentation Lead  
**Effort:** 2-3 days  
**Dependencies:** All previous phases (document final state)

**Actions:**
1. Update API documentation
   - Document new endpoints: `/api/v1/effects/families`, `/api/v1/narrative/status`
   - Update effect metadata endpoint documentation
   - Include WebSocket command documentation

2. Update effect development guide
   - Document FastLED utility usage
   - Add narrative integration examples
   - Include validation command usage

3. Update PatternRegistry documentation
   - Explain metadata structure
   - Document family/tag system
   - Show how to add new patterns

4. Create integration test suite
   - Test all new API endpoints
   - Test narrative status endpoints
   - Test validation command (automated where possible)

**Success Criteria:**
- ✅ API documentation reflects all new endpoints
- ✅ Effect development guide includes utility usage
- ✅ Integration tests cover new functionality
- ✅ All documentation is accurate and up-to-date

---

**Phase 4 Deliverables:**
- Related patterns fully implemented and exposed
- Complete documentation updates
- Integration test suite
- Final validation of all improvements

---

## Overall Timeline Summary

| Phase | Duration | Key Deliverables | Dependencies |
|-------|----------|------------------|--------------|
| Phase 1 | Week 1 | Registry API exposure, FastLED utilities, UI filters | None |
| Phase 2 | Weeks 2-3 | Effect count parity, chromatic effects | Phase 1 Task 1.2 |
| Phase 3 | Weeks 4-5 | Narrative API, UI visualization, validation command | None |
| Phase 4 | Week 6 | Related patterns, documentation, tests | All phases |

**Total Estimated Duration:** 6 weeks (with 1 developer full-time)

**Critical Path:**
Phase 1 (API exposure) → Phase 2 (Chromatic effects can proceed in parallel) → Phase 3 (Narrative integration) → Phase 4 (Documentation)

---

**Next Section:**
- Part 5: Success Metrics and References

