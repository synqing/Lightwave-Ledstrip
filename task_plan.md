# Task Plan: PRISM/Trinity → Lightwave ShowDirector Bridge

## Goal
Create a real-time “semantic/parameter/music” data pipeline so Lightwave-Ledstrip (visual consumer) can ingest PRISMv2/Trinity v0.2 (audio producer) outputs and drive ShowDirector/Narrative/ParameterSweeper coherently.

## Current Phase
Phase 3

## Phases

### Phase 1: Requirements & Discovery
- [x] Identify PRISMv2/Trinity v0.2 output surfaces (schema + transport)
- [x] Identify Lightwave-Ledstrip input surfaces for semantics/parameters (APIs + internal entrypoints)
- [x] Capture key findings + mapping constraints in `findings.md`
- **Status:** complete

### Phase 2: Protocol & Mapping Design
- [x] Define a stable “feature frame / semantic event” schema (Trinity WS messages)
- [x] Define transport (Lightwave WS `/ws`, message envelope `type`)
- [ ] Define mapping from producer features → Lightwave parameters/cues/narrative (default mapping + config format)
- **Status:** in_progress

### Phase 3: Implementation (Bridge + Firmware/API)
- [x] Implement bridge service (stdin JSONL → Lightwave WS + optional segment mapping)
- [x] Implement Lightwave ingestion endpoint for `trinity.segment`
- [x] Add example mapping config
- **Status:** complete

### Phase 4: Testing & Verification
- [ ] Add a deterministic fixture (recorded PRISM output) and replay into Lightwave
- [ ] Verify latency, rate limiting, and correctness under load
- [ ] Document test results in `progress.md`
- **Status:** pending

### Phase 5: Delivery
- [ ] Write integration documentation + quickstart
- [ ] Provide example “show mappings” for common music semantics
- **Status:** pending

## Key Questions
1. What *exact* output schema does PRISMv2/Trinity v0.2 emit (beats, sections, energy, stems, etc.) and over what transport?
2. What is the best Lightwave ingestion surface for real-time control: existing ShowDirector commands, new WS command, or a new internal “AudioSemanticBus” module?
3. How are clocks synchronised (producer timestamps vs device millis) and how do we handle jitter/dropouts?

## Decisions Made
| Decision | Rationale |
|----------|-----------|
| Create a versioned “feature frame” schema | Keeps producer/consumer decoupled; enables recording/replay |
| Prefer host-side bridge first | Minimises firmware changes; iterate quickly; add firmware endpoint only if required |

## Errors Encountered
| Error | Attempt | Resolution |
|-------|---------|------------|
|       | 1       |            |

## Notes
- Hard constraints from `AGENTS.md` apply to firmware render paths (no heap alloc in render, 120 FPS target, British English, etc.).

---

# Task Plan: Claude-Mem Audio Effect Reference Audit

## Goal
Trace historical reference details from Claude-mem for 10 effect families and produce an audit-ready mapping of design intent, parity targets, and known mismatch notes.

## Current Phase
Complete

## Phases

### Phase 1: Memory Discovery
- [x] Search Claude-mem for each target effect family
- [x] Collect candidate observation IDs and prioritise anchors
- **Status:** complete

### Phase 2: Context Expansion
- [x] Run timeline context around selected anchors
- [x] Identify authoritative references (specs, parity docs, algorithm notes)
- **Status:** complete

### Phase 3: Detail Extraction
- [x] Fetch full observations for filtered IDs only
- [x] Summarise per-effect reference chain and potential risk flags
- **Status:** complete

### Phase 4: Delivery
- [x] Provide concise audit matrix for all 10 effects
- [x] Call out missing data and confidence level per effect
- **Status:** complete

## Scope
1. Kuramoto
2. SB Waveform (Parity)
3. SB Waveform (REF)
4. ES Analog
5. ES Spectrum
6. ES Octave
7. ES Bloom
8. ES Waveform
9. Snapwave
10. Beat Pulse

---

# Task Plan: Experimental Audio-Reactive Effects Pack (10 Effects)

## Goal
Create and register 10 new experimental audio-reactive effects for `firmware/v2` that are compliant with the current audio pipeline, centre-origin rendering rules, and memory/timing constraints.

## Current Phase
Phase 4

## Phases

### Phase 1: Constraints + Pipeline Grounding
- [x] Read mandatory memory policy and render-path constraints
- [x] Read audio-visual pipeline and motion-direction conventions
- [x] Confirm current effect capacity and ID/register parity points (`MAX_EFFECTS`, metadata, registry)
- **Status:** complete

### Phase 2: Effect Pack Design
- [x] Define 10 effect identities and audio mappings (non-rainbow, centre-origin)
- [x] Define effect IDs, metadata names/descriptions, and category/family tags
- [x] Define shared implementation strategy (single pack file vs separate files)
- **Status:** complete

### Phase 3: Implementation
- [x] Implement new effect classes
- [x] Register all 10 effects in `CoreEffects.cpp`
- [x] Add metadata entries in `PatternRegistry.cpp`
- [x] Extend reactive register list for new audio-reactive IDs
- [x] Bump `limits::MAX_EFFECTS` to match
- **Status:** complete

### Phase 4: Build + Verification
- [x] Build `esp32dev_audio_esv11`
- [x] Validate count parity (`registerAllEffects` total vs `MAX_EFFECTS`)
- [x] Sanity-check no heap allocations in render paths
- **Status:** complete

---

# Task Plan: SPH0645 Audio Capture Failure Forensics (PipelineCore Regression)

## Goal
Identify exactly why SPH0645 capture is now all-zero after `PipelineCore` implementation, prove the interruption point with trace evidence, and produce a corrective action + validation plan.

## Current Phase
Phase 1

## Phases

### Phase 1: Baseline + Instrumentation Review
- [x] Collect current failure signatures from provided serial logs and source trace points
- [x] Map end-to-end capture path (I2S read -> staging buffers -> pipeline ingress -> output metrics)
- **Status:** complete

### Phase 2: Historical Delta (pre/post AP changes)
- [x] Pull commit history around `PipelineCore` and audio capture files
- [x] Extract prior known-good audio config (sample rate, channels, DMA, clocks)
- [x] Build configuration delta matrix
- **Status:** complete

### Phase 3: Runtime Verification + Controlled Regression Test
- [x] Run targeted firmware build verification for default env (`esp32dev_audio_esv11_32khz`)
- [x] Define/execute temporary rollback test window for capture restore check
- [x] Record measurable outcomes (non-zero sample counts/RMS/DC diagnostics)
- **Status:** complete

### Phase 4: Root Cause + Remediation
- [ ] Isolate root cause component (capture driver init, channel format, scaling, pipeline handoff, or scheduling)
- [ ] Produce specific implementation fix steps
- [ ] Produce validation checklist for on-device confirmation
- **Status:** in_progress
