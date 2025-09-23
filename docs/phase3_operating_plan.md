# Phase 3 Operating Plan

This document records the staged rollout for the visual pipeline overhaul. The plan is versioned so progress, scope changes, and CI gates remain auditable.

## Branching Model

| Branch | Purpose |
| --- | --- |
| `phase3/vp-overhaul` _(protected)_ | Long-lived parent for all Phase 3 work. Only fast-forward merges from reviewed PRs. |
| `refactor/visual-boundaries` | Phase 3 A1 – mechanical split of rendering boundaries and static allocations. |
| `refactor/visual-registry` | Phase 3 A1 – effect registry with metadata, one runner per channel. |
| `feat/async-led-double-buffer` | Phase 3 A2 – asynchronous LED pipeline + double-buffer flips. |
| `feat/audio-ring-buffer` | Phase 3 A3 – bounded I2S capture ring buffer. |
| `feat/dual-channel-director` | Phase 3 A4 – dual-channel director, catalog pairing, throttled logs. |

Each branch starts from `phase3/vp-overhaul`. Merge back by PR; delete feature branches after tags are cut.

## CI Gates (apply to every PR targeting `phase3/vp-overhaul`)

1. **Build matrix:** Debug + Release with `SECONDARY_FIRST_CLASS=1`.
2. **Header cop:** Enforce include guards/order and public header hygiene.
3. **Static allocation audit:** No dynamic memory in `lib/*`; Phase A1 extends scan to new sources touched.
4. **Watchdog configuration:** Integration test guaranteeing WDT is configured and tasks yield.
5. **Size budget:** Alert if flash, IRAM, or DRAM exceeds phase-specific limits (baseline: current image +10 KB flash, +2 KB RAM).

## Tags

| Tag | Contents |
| --- | --- |
| `phase3-a1` | Boundaries, static allocations, registry scaffolding. No logic change. |
| `phase3-a2` | Asynchronous LED double buffer, deterministic frame flips. |
| `phase3-a3` | Bounded I2S ring buffer with overflow counters. |
| `phase3-a4` | Dual-channel director (2–3 pairs), throttled logging. |

Tag after merging into `phase3/vp-overhaul`; include release notes (scope, size deltas, perf metrics).

## Phase Breakdown

### Phase A1 – Boundaries & Registry

- **Branches:** `refactor/visual-boundaries`, `refactor/visual-registry`.
- **Objectives:**
  - Split rendering boundaries (effects vs engine) without behavioral change.
  - Convert runtime allocations to static buffers.
  - Introduce effect registry (metadata: IDs, categories, required capabilities).
- **Acceptance:**
  - CI matrix green.
  - Static allocation audit passes for all touched files.
  - WDT test passes; max frame time unchanged (±5%).
  - Binary size increase <10 KB flash, <2 KB RAM.
- **Instrumentation:** Extend `PerformanceMonitor` to log frame budget variance and registry load time.

### Phase A2 – Async LED Double Buffer

- **Branch:** `feat/async-led-double-buffer`.
- **Objectives:**
  - Implement asynchronous render pipeline with double-buffered flips.
  - Transition engine queues updates; effect switches routed through a central director API.
- **Acceptance:**
  - Stress test at target FPS with <1% dropped frames.
  - CI watchdog and static allocation gates green.
  - IRAM usage remains <99% (fast-path code pinned).
- **Instrumentation:** Counters for flip latency, missed frames exposed via serial + optional JSON.

### Phase A3 – Audio Ring Buffer

- **Branch:** `feat/audio-ring-buffer`.
- **Objectives:**
  - Build power-of-two ring buffer for I2S input with overflow/underflow counters.
  - Integrate with I2S path (still behind `FEATURE_AUDIO_SYNC`).
  - Provide CLI/web introspection of audio stats.
- **Acceptance:**
  - Unit/system tests simulating ISR pressure with zero data corruption.
  - Static allocation audit and watchdog test green.
  - Size increase <8 KB flash, <2 KB RAM.
- **Instrumentation:** Expose buffer counters via `SerialMenu` and telemetry.

### Phase A4 – Dual Channel Director

- **Branch:** `feat/dual-channel-director`.
- **Objectives:**
  - Replace global effect state with a director service that pairs catalog entries with variations.
  - Implement minimal dual-channel set (2–3 effect pairs) with variation injectors.
  - Introduce throttled logging macros and configurable verbosity.
- **Acceptance:**
  - Automated tests verifying pairing fallback (e.g., audio-required skips when audio disabled).
  - CI gates green; size delta <10 KB.
  - Director counters wired into perf telemetry.
- **Instrumentation:** Track transitions per pair, variation hits, log throttle metrics.

## Acceptance & Instrumentation Checklist (pre-work)

- [ ] Update CI configuration for build matrix, watchdog, size, static allocation checks.
- [ ] Document instrumentation endpoints (serial menu, telemetry payloads).
- [ ] Define size budgets per phase and publish in CI config.
- [ ] Publish logging policy (rate limiting, verbosity levels) before Phase A4.
- [ ] Link this plan in README/ROADMAP.

## Tracking Progress

- Reference this document in every PR description and release note.
- Update checkboxes/metrics as phases complete.
- Use GitHub issues or a project board aligned with the branch names to map granular tasks.
