# Consolidated Research Assessment — Quality Evaluation Pipeline

**Date:** 2026-03-10
**Source:** 33 sweep files, ~70 unique findings (after deduplication), 10+ HIGH-VALUE flags
**Status:** Research phase CLOSED. Implementation phase begins.

---

## 1. Current System Architecture (for context)

```
Audio In → 8-band Goertzel + BPM + Chord + Style → 168 Effects → 320 WS2812 LEDs @ 120 FPS
                                                        ↓
                                              Binary Serial v2 (1008 bytes/frame)
                                                        ↓
                                              Quality Evaluation Pipeline (L0–L4)
                                                        ↓
                                              PROPOSE → APPLY → RE-EVALUATE loop
```

**Metric Layers:**
- **L0 (Frame):** Per-frame quality — pixel validity, colour range, saturation
- **L1 (Temporal):** Frame-to-frame consistency — flicker, jitter, smoothness
- **L2 (Audio-Visual):** Audio-visual alignment — beat sync, energy correlation, onset response
- **L3 (Perceptual):** Perceptual similarity — distribution matching, structural similarity
- **L4 (Aesthetic):** Overall aesthetic quality — MLLM-as-judge, rubric-scored evaluation

---

## 2. Deduplicated Finding Catalogue

### TIER 1 — Implement Now (Low complexity, high leverage, no dependencies)

| # | Finding | Paper | Layer | Complexity | Why Now |
|---|---------|-------|-------|------------|---------|
| T1.1 | **TS3IM — Structural Similarity for 1D Time Series** | arXiv:2405.06234 | L3 | Low | Our signal IS a 1D time series (320px × 120FPS). TS3IM decomposes into trend/variability/structure — replaces forced 2D SSIM hacks. Directly computable on LED captures. |
| T1.2 | **elaTCSF — Temporal Contrast Sensitivity for Flicker** | arXiv:2503.16759 | L1 | Low | Psychophysically-calibrated flicker detection. Models human sensitivity to temporal luminance changes. Replaces ad-hoc flicker thresholds with perceptually grounded ones. |
| T1.3 | **Hi-Light Light Stability Score** | (Hi-Light paper) | L1 | Very Low | 10–15 lines of array math. Lighting-specific temporal stability metric. No dependencies. |
| T1.4 | **VMCR Frequency-Domain Temporal Consistency Loss** | arXiv:2501.05484 | L1 | Low | FFT-based temporal consistency. ~15 lines of NumPy. Detects periodic artefacts invisible to frame-differencing. |
| T1.5 | **Omni-Judge 9-Metric Evaluation Taxonomy** | arXiv:2602.01623 | L4/L2 | Low | Structured per-dimension MLLM prompting. 4 categories, 9 dimensions, task-specific instructions per dimension. Pure prompt engineering. |
| T1.6 | **Mix-GRM B-CoT / D-CoT Reasoning Styles** | arXiv:2603.01571 | L4 | Low | Use Breadth-CoT for aesthetic (L3/L4) evaluation, Depth-CoT for objective (L0/L1) evaluation. Mismatching degrades accuracy 8%+. Prompt structure change only. |
| T1.7 | **RRD — Rubric Refinement through Decomposition** | (RRD paper) | L4 | Low | Decompose monolithic quality rubrics into atomic sub-rubrics scored independently. Reduces evaluation variance. |
| T1.8 | **CyclicJudge — Variance Decomposition for MLLM Judges** | arXiv:2603.01865 | L4 | Low | Round-robin prompt templates + variance decomposition diagnostic. Quantifies how much evaluation noise comes from judge vs prompt vs content. |
| T1.9 | **ToolRLA Multiplicative Reward Decomposition** | arXiv:2603.01620 | Arch | Low | Switch L0–L4 composition from additive to multiplicative (geometric mean). Ensures no single bad metric can be hidden by good scores elsewhere. One-line code change. |
| T1.10 | **CAE-AV Per-Frame Audio-Visual Agreement Gating** | arXiv:2602.08309 | L2 | Low | Weight L2 metrics by audio salience (which we already compute). Musically important moments count more. |
| T1.11 | **FlowPortrait Composite Reward Architecture** | arXiv:2603.00159 | L4/Opt | Low | Weighted MLLM signals + hard metric regularisers. GRPO ranking for reward stability. Calibrate λ weights. |
| T1.12 | **PSNR_DIV — Motion-Divergence Weighted Quality** | arXiv:2510.01361 | L1 | Low | Weight frame-to-frame metrics by motion field divergence. Penalises inconsistency in high-motion regions. 1D optical flow on 320 pixels is trivial. |

### TIER 2 — Implement Next (Medium complexity, high leverage)

| # | Finding | Paper | Layer | Complexity | Why Next |
|---|---------|-------|-------|------------|---------|
| T2.1 | **CP Loss — Channel-wise Perceptual Loss for 1D** | arXiv:2601.18829 | L3 | Medium | Learnable 1D perceptual metric per LED channel. ICASSP 2026. Requires training data (quality-labelled LED captures). Could subsume L1 too. |
| T2.2 | **TALBO — Time-Aware Latent BO** | arXiv:2603.00935 | Opt | Medium-High | Our parameter landscape drifts as music changes. TALBO embeds temporal context into BO. Addresses the non-stationarity problem directly. |
| T2.3 | **RESCUE — Causal Multi-Fidelity BO** | arXiv:2602.00788 | Opt/Arch | Medium | Budget-optimal metric layer selection. Run cheap L0/L1 first, only invoke expensive L3/L4 when L0/L1 pass. Causal graph over our known L0→L1→L2→L3→L4 structure. |
| T2.4 | **CAMEL — Confidence-Gated Selective Evaluation** | arXiv:2602.20670 | L4/Opt | Low-Medium | Only trigger full MLLM re-evaluation when confidence is low. Requires token log-probabilities (available via API). Reduces L4 cost for closed-loop viability. |
| T2.5 | **VisionDirector — Goal-Level Closed-Loop with Rollback** | arXiv:2512.19243 | Opt | Medium | Per-metric state tracking + rollback if a parameter change improves one metric but degrades another. Critical for stable closed-loop operation. |
| T2.6 | **UrbanAlign — Post-hoc VLM Judge Calibration** | arXiv:2602.19442 | L4 | Medium | Concept mining + Observer-Debater-Judge chain + geometric calibration. One-time setup with ~50–100 labelled pairs. |
| T2.7 | **AdaEvolve — Bandit-Based Optimisation Budget Allocation** | arXiv:2602.20133 | Opt | Low-Medium | Three-level adaptive hierarchy for optimisation. Bandit scheduling detects stalls and reallocates budget. The bandit part is immediately usable without the LLM parts. |
| T2.8 | **Latent-State Bandits — Non-Stationary Parameter Selection** | arXiv:2602.05139 | Opt | Low-Medium | LinUCB with lagged audio features as context. Online learning from parameter→quality observations. No model training. |
| T2.9 | **DecDTW — Differentiable DTW** | (DecDTW paper) | L2/L3 | Medium | Exact differentiable DTW via implicit differentiation. Enables gradient-based alignment of audio and visual time series. |
| T2.10 | **12-Factor Decomposed MLLM-as-Judge Framework** | arXiv:2602.13028 | L4 | Low-Medium | Structured multi-factor evaluation rubric. Requires iteration to calibrate for LED domain. |
| T2.11 | **BoGA — GA as Proposal Generator in Proxy-Verified Loop** | arXiv:2603.02753 | Opt | Medium | Maps cleanly onto our propose→evaluate loop. GA generates parameter candidates; verifier evaluates. |
| T2.12 | **LAGO — Local-Global Competition** | arXiv:2603.02970 | Opt | Medium | Dual proposal engines (local trust-region + global BO) with competition. Balances exploitation vs exploration. |
| T2.13 | **EST — Evaluator Stress Tests for Proxy Gaming** | (EST paper) | Opt | Low-Medium | Perturbation framework detecting when the optimiser is gaming the metrics rather than improving quality. Essential safety mechanism for closed-loop. |

### TIER 3 — Implement Later (High complexity or deferred value)

| # | Finding | Paper | Layer | Complexity | Why Later |
|---|---------|-------|-------|------------|---------|
| T3.1 | **GT-SVJ — Self-Supervised Video Judge** | arXiv:2602.05202 | L4 | High | Train a small generative model as a quality judge. Our 320×1 resolution makes it tractable but still needs domain training data. |
| T3.2 | **CMI-RM — Lightweight Audio-Conditioned Reward Model** | (CMI-RM paper) | L2/L4 | Medium | Two-tower reward model conditioned on audio. Requires A-vs-B preference data collection. |
| T3.3 | **ACE-Step PMI Reward** | arXiv:2602.00744 | Opt/L2 | Medium | PMI as intrinsic reward (penalises generic outputs). Requires designing the inverse mapping (visual→audio feature reconstruction). |
| T3.4 | **DIFNO — Derivative-Informed FNO** | (DIFNO paper) | Opt | Medium-High | Surrogate with learned Jacobians for PDE-constrained optimisation. Depends on testbed being built first. |
| T3.5 | **VideoJudge — Bootstrapped Small MLLM** | arXiv:2509.21451 | L4 | High | Fine-tune a small MLLM for domain-specific evaluation. Long-term cost reduction play. |
| T3.6 | **MORSE — Automatic Reward Shaping** | (MORSE paper) | Opt | High | Bi-level optimisation for reward composition. Computationally expensive. Outer loop can be offline. |
| T3.7 | **K-Sort Eval — Bayesian Posterior Judge Calibration** | arXiv:2602.09411 | L4 | Medium | Dynamic calibration with posterior updates. Requires human-rated seed set (~50–100 samples). |
| T3.8 | **Reward Distillation — Fast Proxy from MLLM Traces** | (Distillation paper) | L4/Opt | Medium | Collect MLLM evaluation traces → train small fast model. Recalibration pipeline needed. |
| T3.9 | **Deep Time Warping** | (DTW paper) | L2/L3 | Medium | CNN-based multi-series alignment. Our 120 FPS × minutes is very long; piecewise decomposition needed. |
| T3.10 | **LLM-AMARL — Multi-Agent RL for Lighting** | (LLM-AMARL paper) | Opt | High | Full MARL training. Overkill for now but the preference clustering pattern is independently usable. |

### DROPPED — Noise or Superseded

| Finding | Reason |
|---------|--------|
| DVAR / FMG-DFS | Overlaps with REACT; requires fine-tuning Qwen2.5-VL on 80k videos — impractical |
| CycleSync (round-trip V2A) | Requires V2A model for 320×1 — massive domain gap, high risk |
| FVMD (Fréchet Video Motion Distance) | 2D video metric, poor transfer to 1D strip |
| VMBench PMM metrics | Interesting but MSS uses Q-Align (2D image model), TCS uses GroundedSAM2 — 2D dependencies |
| Group Averaging (bilateral symmetry) | Already known; trivial implementation noted in testbed doc |
| PIDT (physics-informed digital twin) | Testbed doc already covers this under Phase 2 |
| Adjoint-Optimised Neural PDEs | Theoretical convergence; no implementation path yet |
| GRPO for T2A | Overlaps with FlowPortrait (also uses GRPO). Audio reward composition (CLAP+KL+FAD) is domain-specific |
| Null-Space Judge Debiasing | Requires open-weight model access; less relevant for API-based judges |

---

## 3. Implementation Roadmap

### Phase A — Metric Foundation (Weeks 1–2)
**Goal:** Wire up L0–L2 with hard metrics. No MLLM, no optimisation loop. Just capture frames and compute numbers.

**Build order:**
1. **T1.3** Hi-Light Stability Score → L1 baseline (10 lines, <1 hour)
2. **T1.4** VMCR Frequency-Domain Temporal Consistency → L1 complement (15 lines, <1 hour)
3. **T1.12** PSNR_DIV motion-divergence weighting → L1 enhancement (extends existing frame diff)
4. **T1.2** elaTCSF flicker detection → L1 psychophysical calibration (replaces thresholds)
5. **T1.1** TS3IM time series similarity → L3 foundation (1D native, no 2D hacks)
6. **T1.10** CAE-AV audio-salience gating → L2 weighting (uses existing firmware salience data)

**Validation:** Run all L0–L2 metrics on 10 captured LED sequences (varied genres). Check:
- Do L1 metrics agree on "good" vs "bad" sequences?
- Does TS3IM correlate with human preference more than raw SSIM?
- Does audio-salience gating increase L2 discriminability?

**Pass/fail:** Metrics must rank 5 known-good sequences above 5 known-bad sequences with >80% agreement.

### Phase B — MLLM Evaluation Protocol (Weeks 2–3)
**Goal:** Build L4 evaluation with structured prompting. No closed-loop yet.

**Build order:**
1. **T1.5** Omni-Judge taxonomy → Define our 6–9 evaluation dimensions
2. **T1.6** Mix-GRM B-CoT / D-CoT → Structure prompts per layer type
3. **T1.7** RRD rubric decomposition → Atomic sub-rubric per dimension
4. **T1.8** CyclicJudge variance decomposition → Measure judge noise baseline
5. **T1.9** ToolRLA multiplicative composition → Combine L0–L4 scores

**Validation:** Evaluate 20 LED sequences (4 genres × 5 quality levels) with the full L0–L4 stack.
- Inter-dimension agreement: Do dimensions correlate as expected?
- Judge variance: Is L4 noise < 15% of score range?
- Composition: Does multiplicative catch cases that additive misses?

**Pass/fail:** Full-stack ranking must agree with human pairwise preference >70% of the time on 30 A-vs-B pairs.

### Phase C — Optimisation Loop (Weeks 3–5)
**Goal:** Close the loop. PROPOSE parameters → EVALUATE → ACCEPT/REJECT.

**Build order:**
1. **T2.7** AdaEvolve bandit scheduling → Budget allocation across metric layers
2. **T2.4** CAMEL confidence gating → Skip expensive L4 when cheap layers suffice
3. **T2.13** EST evaluator stress tests → Safety mechanism against metric gaming
4. **T2.5** VisionDirector rollback → Per-metric state tracking, revert bad changes
5. **T2.8** Latent-State Bandits → Online parameter selection using audio context
6. **T1.11** FlowPortrait composite reward → Combine MLLM + hard metrics for proposal ranking

**Validation:** Run closed loop on 3 test songs. Measure:
- Does quality score improve over 50 iterations?
- Does EST detect any gaming behaviour?
- Does rollback prevent quality regression?

**Pass/fail:** Quality improves monotonically (with rollback) on ≥2 of 3 test songs. Zero metric gaming detected by EST.

### Phase D — Advanced Metrics & Optimisation (Weeks 5+)
**Goal:** Swap in learned metrics and sophisticated optimisation.

**Build order (priority-ordered, execute as needed):**
1. **T2.1** CP Loss learnable 1D perceptual metric
2. **T2.2** TALBO time-aware BO (if non-stationarity is a problem in Phase C)
3. **T2.3** RESCUE causal multi-fidelity BO (if evaluation budget is a bottleneck)
4. **T2.9** DecDTW differentiable alignment (if L2 needs gradient-based optimisation)
5. **T2.11** BoGA or **T2.12** LAGO (if the proposal engine from Phase C is insufficient)

---

## 4. Dependencies and Critical Path

```
Phase A (L0-L2 hard metrics)
    ├── does NOT depend on testbed (runs on captured frames)
    └── BLOCKS Phase C (optimisation needs metrics to optimise against)

Phase B (L4 MLLM evaluation)
    ├── does NOT depend on Phase A (can run in parallel)
    └── BLOCKS Phase C (optimisation needs the full scoring stack)

Phase C (closed loop)
    ├── DEPENDS on Phase A + Phase B
    └── BLOCKS Phase D (advanced methods only needed if Phase C reveals gaps)

Testbed (PyTorch port of PDE core)
    ├── INDEPENDENT of Phases A–C (metric work uses real captures)
    └── ENABLES Phase D and DIFNO training (T3.4)
```

**[INFERENCE] Phases A and B can run in parallel. Phase C is the integration point. The testbed is an independent workstream that unblocks Phase D but is not on the critical path for the initial closed-loop demonstration.**

---

## 5. What We Still Don't Have (Known Gaps)

1. **No 1D-native perceptual embedding.** TS3IM and CP Loss address this but neither is trained on LED data. We'll need to validate transfer or collect domain-specific training pairs.

2. **No audio-visual alignment metric beyond correlation.** BeatAlign and corr(|Δ|) are coarse. DecDTW could fill this but is Phase D. For Phase A, the CAE-AV gating is a pragmatic bridge.

3. **No human preference baseline.** Every MLLM judge calibration method (K-Sort, UrbanAlign, CyclicJudge variance decomposition) requires a seed set of human-rated LED sequences. We need to collect 50–100 rated A-vs-B pairs before Phase B validation.

4. **No real-time constraint accounting.** All metrics are designed for offline evaluation. The closed loop will need latency budgets per layer — CAMEL (T2.4) and RESCUE (T2.3) address this but are Phase C/D.
