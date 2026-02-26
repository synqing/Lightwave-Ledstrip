# Perf Task: Baseline Low-Heap Shedding (Non-Blocking)

## Context
Post-rollout verification confirmed stable runtime with WebServer low-heap shedding warnings under internal SRAM pressure. This is a pre-existing baseline condition and not a blocker for tunables exposure.

Observed baseline (2026-02-26):
- Internal heap steady-state: ~27.9 KB
- Lowest observed: ~26.7 KB
- Duration: 10 minutes
- Crashes/reboots/WDT: 0
- WS protocol errors: 0

## Goal
Characterise and reduce internal SRAM shedding pressure without changing API contracts or regressing effect/render stability.

## Scope
In scope:
- Internal heap telemetry under idle, REST-heavy, and WS-heavy scenarios
- Attribution of internal SRAM usage to subsystems (WebServer, WS buffers, routing, effect control)
- Practical mitigation proposals with measured before/after impact

Out of scope:
- AP/STA provisioning redesign
- Feature removals or large architecture rewrites
- Behavioural changes to effect visuals at default tunables

## Work Plan
1. Baseline Profiling
- Add/enable periodic internal/PSRAM heap snapshots at fixed cadence
- Record shedding state transitions and counts
- Capture 15-min traces for:
  - idle (no clients)
  - REST smoke loop
  - WS subscribe + parameter churn

2. Allocation Attribution
- Identify top internal SRAM consumers during shedding windows
- Verify static allocations vs runtime bursts
- Confirm whether WS queues and HTTP request buffers dominate low-heap periods

3. Mitigation Candidates
- Tune non-critical queue/buffer depths for internal SRAM pressure
- Move eligible data from internal SRAM to PSRAM where safe
- Tighten transient allocations in HTTP/WS hot paths
- Confirm no render-path heap allocations are introduced

4. Validation
- Repeat 10-minute and 30-minute stability runs
- Compare shedding frequency and minimum internal heap floor
- Re-run firmware build + native codec tests

## Acceptance Criteria
- No crashes, reboots, or WDT under 30-minute stress profile
- Shedding remains controlled with no functional regressions
- Internal heap minimum improves measurably OR shedding event rate decreases measurably
- Build and native codec suite remain green

## Deliverables
- Perf evidence matrix with before/after metrics
- Recommendation note with selected mitigations and rollback notes
- Follow-up implementation PR(s) linked to this task
