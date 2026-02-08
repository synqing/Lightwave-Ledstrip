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
