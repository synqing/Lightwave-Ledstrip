RBDO label: GROUNDED

Date: 2026-05-12 AWST
Repo HEAD: 627b0b14

Action item map executed:
- A Source discovery: read songaware, renderer, show owner, serial/API, REST/WS, protocols, registry/effect IDs, tests. Blocker: unreadable source or missing registry. Acceptance: implementation touched real source, not docs-only.
- B Existing SongAware V1 audit: found runtime-only parameter support and partial Director hooks. Blocker: parameter-only lane. Acceptance: Director switches effect IDs.
- C Mode contract completion: off, parameter, director. Blocker: parameter mode switching. Acceptance: serial mode controls verified.
- D Classifier completion: silence, ambient, steady, build, drop, breakdown, dense, transition, unknown. Blocker: no coarse state output. Acceptance: 19 native tests.
- E Effect allowlist validation: real registered IDs only. Blocker: invented target. Acceptance: serial allowlist count 9 includes 0x1302, 0x0202, 0x0201, 0x0204, 0x0407, 0x0900, 0x0901, 0x1B01.
- F Selection scoring: state/confidence/target scoring. Blocker: roulette. Acceptance: tests cover same-effect and blocked gates.
- G Switching gates: enable, mode, constrained switching, confidence, dwell, cooldown, rate limit, anti-thrash, ownership, health. Acceptance: tests plus serial suppression.
- H Transition handling: Director-specific transition queue using existing TransitionEngine. Blocker: refactor existing path. Acceptance: no existing transition path refactor.
- I Parameter envelopes: speed/intensity/complexity support layer. Acceptance: parameter mode tests, runtime parameterUpdates non-zero.
- J Ownership suppression: manual/show marks. Acceptance: tests cover both; serial captured same-effect/low-confidence suppression.
- K Health suppression: show_skips, failures, rmt_errors, underruns. Acceptance: tests and runtime zero counters.
- L Restore/rollback: serial restore to 0x1302 fixed controls off. Acceptance: final serial status and vp stack.
- M Telemetry: status/debug/policy/allowlist/health. Acceptance: serial transcript captured.
- N Serial/API surface: serial canonical, JSON/REST/WS parity where cleanly supported. Acceptance: YAML parse and build.
- O REST/WS/protocol parity: updated existing SongAware routes only. Acceptance: YAML parse, build, native matrix.
- P Tests: native SongAware and native harness matrix. Acceptance: pass.
- Q Build/upload: K1 env build and upload. Acceptance: success on /dev/cu.usbmodem1101.
- R Runtime smoke: at least two allowlisted Director decisions plus suppression and restore. Acceptance: four logged decisions, hard counters zero.
- S Evidence: required folder filled. Acceptance: this folder.
- T Final decision: PASS if all above hold.
