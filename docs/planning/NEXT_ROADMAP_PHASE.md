# Next Roadmap Phase

## Phase Name
Runtime Performance Hardening: Internal SRAM Shedding Baseline

## Trigger
Milestone snapshot complete for all-effects tunables exposure rollout.

## Entry Criteria
- Manifest gate green across all active families
- Build green (`esp32dev_audio_esv11_32khz`)
- Native codec suite green
- Flash and runtime sanity verified

## Primary Objective
Reduce low-heap shedding pressure and establish a stable performance envelope while preserving current API and visual behaviour.

## First Execution Slice
1. Run profiling matrix (idle, REST load, WS load)
2. Produce internal heap trend charts and shedding event histogram
3. Ship minimal-risk mitigation patch set
4. Re-verify with full gate run

## Exit Criteria
- Shedding behaviour remains non-blocking under sustained load
- Measurable heap/shedding improvement documented
- No regressions in build/test/runtime gates
