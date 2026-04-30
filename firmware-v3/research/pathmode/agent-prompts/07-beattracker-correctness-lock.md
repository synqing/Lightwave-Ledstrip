You are a Senior Engineer. Implement the following Intent Spec using a sequential workflow.

CRITICAL: The BeatTracker rebuild has ALREADY landed (commit fab1802d, 2026-03-20). This intent is about LOCKING that implementation behind versioning + regression gates + a Captain re-approval rule. You are NOT redesigning the algorithm.

## Phase 1: Orientation
Read: firmware-v3/HANDOVER_BeatTracker.md (algorithm rationale, ACF rejection narrative). Read: firmware-v3/src/audio/pipeline/BeatTracker.cpp (current implementation, post-fab1802d). Read: firmware-v3/test/test_spine16k/test_spine16k_acceptance.cpp (existing 12-clip suite).

## Phase 2: Plan
1. Add `kBeatTrackerAlgoVersion` constant + header comment block to BeatTracker.cpp explaining current algorithm + why ACF rejected (cross-reference HANDOVER §2).
2. Wire `test_spine16k_acceptance` into release-tag CI gate (≥ 9/12 coherent).
3. Add pre-commit hook blocking commits to BeatTracker.cpp without an attached test-result artefact.
4. Expose BPM lock confidence as a telemetry health metric.
5. Document Goertzel `TempoTracker` (firmware/v2/.../TempoTracker.h, 48–143 BPM) as fallback only.

Use TodoWrite per outcome.

## Phase 3: Implement
Dependency order. Don't change the algorithm. The version-constant change is purely additive.

## Phase 4: Validate
**Unit**: `pio test -e native_test_esv11_music -f test_spine16k_acceptance` — ≥ 9/12 coherent.
**Manual**: K1 hardware locks 120 BPM kick within 2–3 kicks. Algorithm version constant present and matches HANDOVER documented value.

# Intent Spec: REC-PERF-2: Lock landed comb-tooth BeatTracker behind regression gate, version constant, and Captain re-approval

**ID**: `4df0963f-fe02-42dc-9d3d-b5ee565d502b` | **Status**: validated

## Objective

Prevent another silent overwrite of BeatTracker.cpp like the Feb 2026 ACF regression that dropped real-audio coherence from 6/12 to 2/12. The comb-tooth rebuild has already landed (Captain-approved 2026-03-01, commit fab1802d, 2026-03-20). This intent locks that implementation behind versioning, regression testing, and a Captain-re-approval-required-on-change rule.

**Algorithm (verbatim, do not change)**: OSS ring → CBSS peak detection → BPM density histogram (σ=2 BPM Gaussian) → argmax winner with bidirectional subharmonic enhancement (0.5/0.33) and log-Gaussian prior at `tempoPriorBpm`. ACF is rejected per HANDOVER §2.

## Success Outcomes

- [ ] BeatTracker.cpp carries a `kBeatTrackerAlgoVersion` constant and a header comment block stating the comb-tooth approach is in use and why ACF was rejected (cross-references HANDOVER §2).
- [ ] `test_spine16k_acceptance` must achieve ≥ 9/12 coherent on the 12-clip real-drum-loop suite before any release tag.
- [ ] Pre-commit hook blocks commits that touch BeatTracker.cpp without a corresponding test-result artefact.
- [ ] BPM lock confidence reported via telemetry as a health metric.
- [ ] Goertzel TempoTracker (firmware/v2/.../TempoTracker.h, 48–143 BPM) retained as documented fallback only — not the chosen path.

## Constraints & Constitution

- [!] DSP Spine v0.1 frozen parameters (Fs=16 kHz, hop=128, FFT=512).
- [!] 60–240 BPM target range.
- [!] < 50 ms tempo-update latency.
- [!] **Algorithm change requires Captain re-approval. Any future swap away from comb-tooth (e.g. to Goertzel or hybrid) must update HANDOVER_BeatTracker.md and bump `kBeatTrackerAlgoVersion`.**
- [!] Precedence: Constraint > Standard > Pattern.

## Edge Cases
- **Weak/ambiguous beat (jazz, classical)** → Confidence reports low, no guess.
- **Octave error (60 BPM detected as 120)** → CBSS phase tracker + log-Gaussian prior at tempoPriorBpm disambiguates.
- **Tempo change at drop** → Re-acquires within 4 bars without oscillation.

## Cross-references
- HANDOVER_BeatTracker.md §2 (ACF rejection + comb-tooth narrative)
- claude-mem #35423, #35484, #35333, #37915
- commit fab1802d (2026-03-20)
