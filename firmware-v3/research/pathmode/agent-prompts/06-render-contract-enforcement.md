You are a Senior Engineer. Implement the following Intent Spec using a sequential workflow.

CRITICAL: This intent BUNDLES six related render-contract enforcement workstreams. They share infrastructure (lint/CI gates, PatternMetadata extensions, RendererActor instrumentation). Implement in dependency order — do NOT treat them as six independent PRs.

## Phase 1: Orientation
Likely-affected: src/effects/IEffect.h, src/effects/PatternRegistry.{h,cpp}, src/config/effect_ids.h, src/audio/contracts/FrequencyMap.h, src/core/actors/RendererActor.{h,cpp}, scripts/ (new lint/audit scripts), .github/workflows/ (or pre-commit-config.yaml).

Read EXISTING patterns: PatternMetadata struct, shouldSkipColorCorrection() switch, RenderStats. Then sample 15-20 effects strategically (mix of REACTIVE, percussion, spectrum, ambient, physics) before designing lint rules.

## Phase 2: Plan
Suggested order:
1. Extend PatternMetadata (skip_color_correction bool + reason; add silence behaviour cross-ref to Slot 4).
2. Create canonical FrequencyMap key-decision doc + lint rule for direct bands[N] indexing.
3. Add Inversion Bypass paired-CI gate + reference inverted test effect.
4. Add no-heap-in-render static checker (clang AST query / scripted clangd).
5. Add per-effect render-budget regression harness (build/CI step + TAP_A/TAP_B histograms).
6. Add hue-sweep audit script.

Use TodoWrite per outcome.

## Phase 3: Implement
Dependency order. Each lint rule needs a positive + negative test case. After each: verify no breakage on the catalogue.

## Phase 4: Validate
**E2E**: Reference inverted test effect renders correctly on hardware. 5 interference + 5 organic + 5 quantum effects parity test (correction on/off).
**Unit**: Profile 20 effects on ESP32-S3, confirm 2.0 ms ceiling. CI rejects mismatched-Inversion-Bypass effects. No-heap checker on 10 PSRAM + 10 simple effects: zero false positives.
**Manual**: Spot-check 10 hue-risky effects; spot-check audit report against known-PSRAM effects (RippleEffect, FireEffect).

# Intent Spec: Render contract enforcement: per-effect budget + colour skip-list + Inversion Bypass + no-heap + rainbow audit + FrequencyMap canonical

**ID**: `5a3abcab-3101-43b9-8c63-3a954b9b1629` | **Status**: validated

## Objective

The render contract today (centre-origin, no-heap, no-rainbows, 2.0 ms ceiling, REACTIVE silence, Inversion Bypass, frequency-spatial mapping) is mostly convention with sparse enforcement. The 349-effect catalogue is too large for code review alone to be a reliable gate. This intent bundles six render-contract enforcement workstreams into a coordinated programme so the contract is actually kept rather than nominally promised.

## Success Outcomes

- [ ] **Per-effect render-budget regression harness**: build/CI profiles each effect at canonical ctx.speed=25 (baseline + variance); CI gate fails on any effect exceeding 2.0 ms p99; RendererActor TAP_A/TAP_B debug captures record per-effect timing histograms; per-release ranking report; regression DB tracks timing across firmware versions.
- [ ] **Colour-correction skip-list audit**: PatternMetadata.skip_color_correction: bool added with mandatory free-text reason field; audit report lists every physics-family effect with skip status + justification; CI check rejects Interference / Quantum / Advanced-Optical effects without the flag; AU library backward-compatible.
- [ ] **Inversion Bypass paired-CI gate** (Captain decision 2026-04-26): an effect with [INVERTED] registry prefix MUST contain @spatial-mapping: inverted comment, and vice-versa; pre-commit hook + CI rejects mismatched pairs; reference inverted test effect added; runtime activation logged for audit; full catalogue audited for unintended inversions.
- [ ] **No-heap-in-render static checker**: clang AST query (or scripted clangd lookup) flags new / malloc / String / std::vector push-back in any function transitively reachable from render(); IEffect.h template includes the canonical PSRAM pattern; audit report lists every effect with PSRAM buffer + size.
- [ ] **Hue-sweep ('rainbow') audit**: scripted grep for unbounded hue increments and full-range CHSV with hue wrapping; lint flags getColor() with unclamped hue cycling 0–255; per-release compliance report; documented safe patterns (palette-driven, narrow-range, chroma-anchored).
- [ ] **FrequencyMap canonical reference**: Pathmode key decision added — 'Frequency-to-space mapping uses canonical named bands (KICK → centre, SNARE → inner, HIHAT → outer); effects obtain energy via FrequencyMap named-band queries, not hard-coded bin indices.' Lint catches bands[N]-style direct indexing; FrequencyMap.h boundaries published in firmware-v3/docs/audio-visual/.

## Constraints & Constitution

- [!] Static checks and per-effect timing measurement must operate without modifying the render hot path (>10 µs overhead is unacceptable).
- [!] Centre-origin remains default; Inversion Bypass requires both markers; no other escape hatch.
- [!] Static buffers, init() / cleanup() allocations, and pre-allocated infrastructure are exempt from the no-heap checker.
- [!] Palette-driven 0–255 lookup, chroma-driven hue, spatial-position-based hue are explicitly safe; lint must not flag them.
- [!] British English.
- [!] Precedence: Constraint > Standard > Pattern.

## Edge Cases
- **Frame-dependent timing variance (1.2–1.8 ms flicker)** → Report variance + hard ceiling 2.0 ms; treat as pass if p99 ≤ 2.0 ms.
- **Effect blends physics + audio in different layers** → Tag layers independently for skip-list.
- **Author wants to invert one segment of a dual-strip effect** → Must split into two effects.
- **Effect uses chroma-driven hue spanning near-full sweep (12 notes × 21 hue units)** → Accepted with documented rationale; lint passes.
- **Effect hard-codes bands[0]** → Lint fails; refactor to FrequencyMap named-band query.
- **Effect creates temporary SmallBuffer in render** → No-heap checker flags; require static or PSRAM allocation.
