You are a Senior Engineer. Implement the following Intent Spec using a sequential workflow.

CRITICAL RULES:
- The 'constraints' section contains constitutional rules you MUST NOT VIOLATE.
- The **Space Context** (product vision, audience, principles) explains WHY we are building this. Read it.
- The **Supporting Evidence** section contains real user feedback — friction, quotes, and metrics that drove this intent. Ground your decisions in it.
- Your goal is to satisfy ALL 'outcomes'. Each one is a required deliverable.
- Follow the phases below in order. Do not skip phases.

## Phase 1: Orientation (Do NOT write code yet)
Read the full spec below. Then:
1. Identify the files and modules most likely affected by this change.
2. Read those files to understand existing patterns, types, and conventions.
3. Note any conflicts between the spec and the current codebase.
4. Read the Supporting Evidence section — understand the real user pain behind this intent.
5. Use your tools (Glob, Grep, Read) to explore before changing anything.

### Gate 1: Orientation Complete
Before proceeding, confirm:
- [ ] You have read the relevant source files
- [ ] You understand the existing patterns
- [ ] No constraint conflicts identified (or flagged if found)

## Phase 2: Plan
Create a step-by-step implementation plan:
1. List every file you will create or modify.
2. For each file, describe the specific changes.
3. Order the changes so each step builds on the previous (dependencies first).
4. Map each change back to the outcome it satisfies (by outcome ID). Every outcome must be covered.
5. Note which edge cases each change addresses.

Use TodoWrite to create a task for each outcome. Mark them in_progress as you work.

## Phase 3: Implement
Execute your plan step-by-step:
- Make changes in dependency order (shared utilities and types first, then consumers).
- After each file change, verify it doesn't break existing functionality.
- If you encounter something unexpected, pause and reassess the plan — do not push through.
- Respect ALL constraints. Constitutional rules are non-negotiable.

## Phase 4: Validate
Run through every verification step in the spec:

**E2E Tests** — verify these pass:
- [ ] Capture LED data on logic analyser; confirm parallel transmission begins simultaneously on both strips.
- [ ] 5-minute capture on K1 v2 hardware with parallel RMT enabled; p99 audio-to-photon ≤ 8 ms.

**Unit Tests** — verify these pass:
- [ ] Mock actors with known timestamps; verify cumulative latency calculation.

### Gate 2: Validation Complete
If any verification step fails:
1. Identify the root cause.
2. Fix the specific issue (do not rewrite unrelated code).
3. Re-run the failed verification.
4. Repeat until all checks pass.

Only mark implementation complete when ALL outcomes are satisfied and ALL verification steps pass.

# Intent Spec: Audio-to-photon latency programme: parallel RMT + per-stage instrumentation

**ID**: `e3102ffc-d733-4ab2-a160-019fd32f51c0` | **Status**: validated

## Objective (The "Why")

Today the <8 ms North Star is aspirational. The 6.3 ms WS2812 wire transfer (single-RMT-channel driving 320 LEDs serially) dominates the chain, leaving <1.7 ms for the entire audio path; per-stage instrumentation does not exist, so a regression at any stage is invisible until total latency is wrong. This programme combines two inseparable workstreams: (1) parallel RMT channels per 160-LED strip to halve the wire-time floor to ~3.2 ms (Captain decision 2026-04-26), making the budget defensible; (2) end-to-end timestamping to make the budget continuously measurable in CI and in the field.

## Space Context
**Space**: K1
**Product Vision**: K1 is a dedicated hi-fi instrument for music visualization. We bet that by stripping away 'smart' distractions—apps, cloud, and latency—we can create a physical medium where light is as immediate and high-fidelity as the audio itself. It is a permanent fixture for critical listening, not a disposable party accessory.
**Target Audience**: High-fidelity audiophiles and dedicated music room owners who prioritize tactile hardware and zero-latency visual feedback over app-controlled smart home gimmicks.
**Constraints**:
- Microphone input only: Do not design for or reference physical Line-In/Aux features.
- Bilateral Centre-Origin: Rendering logic must assume a dual-strip (2x160 LED) layout mirrored around a central origin (LED 79/80).
- Canonical Spectrum Geography: K1 uses bass-at-centre, treble-at-edges. Origin represents low-frequency/pressure; outer edges represent treble/air. (Band 0 = centre, Band 7 = edge).
- Inversion Bypass Protocol: To bypass the Bass-at-Center constraint, the source code MUST include the comment tag '@spatial-mapping: inverted' AND the registry metadata MUST begin with '[INVERTED]'.
- Fixed 8/8 Encoder Split: Physical Unit A (Encoders 0-7) is HARD-CODED for Global Performance. Physical Unit B (8-15) is CONTEXTUAL based on touchscreen tab.
- No Hidden Shift Layers: Primary performance controls must never require a shift-key or 'hold' gesture.
**Principles**:
- Spatial Consistency: Users must be able to operate Unit A by feel. Never break the 1-to-1 mapping of global parameters.
- Aesthetic: Geometric/Abstract priority. Use premium organic smoothness. Avoid 'party-light' chaos.
- REACTIVE Pattern Contract: Patterns must respect the silence contract (fade to black via silentScale).
- Frequency-Spatial Logic: Bass/Kick = Centre (dist 0-20); Midrange = Inner-middle; Treble/Hats = Outer edge (dist 60-79).
**Key Decisions**:
- Hardware: Dual 160-LED linear strips (320 total).
- Controller: 2x M5ROTATE8 units (Unit A = Global, Unit B = Contextual).
- Unit A Map: 0:Effect, 1:Palette, 2:Brightness, 3:Sensitivity, 4:Speed, 5:Intensity, 6:Complexity, 7:Variation.
- Unit B Modes: FX Params, Edge/Color, Zones, Presets (Switched via touchscreen tabs).
- Silence Gate Logic: Single RMS test (clamp01(rmsUngated) < threshold) + sustain timer + EMA fade-to-black.

## Success Outcomes

- [ ] FastLED RMT driver configured with one DMA-capable channel per 160-LED strip; 320-LED transfer drops from ~6.3 ms to ~3.2 ms p99.
- [ ] End-to-end timestamping: I2S capture-to-first-sample, ESV11 hop, AudioActor → RendererActor publish, per-effect render time, FastLED transfer all timestamped continuously and emitted in a single telemetry frame {capture_ts, dsp_done_ts, publish_ts, render_done_ts, photon_ts} via /api/system/perf-snapshot.
- [ ] Pathmode North Star wording updated to a measurable budget: 'End-to-end latency (microphone sample → first photon emitted) targets ≤ 8 ms at the 99th percentile under steady-state operation. Decomposition: I2S capture ≤ 1 ms, ESV11 DSP hop ≤ 1 ms (50 Hz cadence), cross-core publish ≤ 100 µs, effect render ≤ 2 ms, FastLED/RMT WS2812 transmit ≤ 3.2 ms.'
- [ ] CI gate: build fails if any p99 stage exceeds its budget on the reference workload.
- [ ] Hardware design doc records GPIO assignments for both RMT channels; firmware build flags select dual-channel mode for K1 v2; K1 v1 single-channel fallback documented with North-Star caveat.

## Constraints & Constitution

- [!] Each strip's RMT channel must be DMA-capable on ESP32-S3.
- [!] GPIO assignment must not conflict with mic I2S pins or encoder I2C bus.
- [!] esp_timer_get_time() clock alignment between Core 0 and Core 1 must be verified (drift documented).
- [!] Total telemetry overhead < 100 µs per frame.
- [!] ESV11-only path (PipelineCore deprecated per Slot 5).
- [!] British English.
- [!] Precedence: Constraint > Standard > Pattern. Constraints are hard gates.
- [!] Standards: every implementation must identify source of truth, distinguish product-surface from implementation-detail, name the affected client/device/surface, and include verification appropriate to the risk.
- [!] Patterns: look for existing named patterns and helper APIs before creating new behaviour. New variants declare relation to the existing pattern.

## Edge Cases
- **K1 v1 boots on a build expecting dual-channel RMT**: Falls back to single-channel; spec North Star applies to v2 only.
- **RMT channel conflict with another peripheral**: Build fails at compile time via static_assert.
- **Audio dropout mid-frame**: Stage marked stale, not last-known-value.
- **Trinity sync override active**: Photon timestamp anchors to transport; reliable=false.
- **Render exceeds budget occasionally**: Histogram captures p99 + max; ceiling violation surfaces in alert.

## Verification
**E2E**: Logic analyser confirms parallel transmission. 5-min K1 v2 capture: p99 ≤ 8 ms.
**Unit**: Mock actors + known timestamps verify cumulative latency calc.
