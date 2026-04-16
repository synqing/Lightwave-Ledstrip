# Inference Task Decision Brief (K1 / LightwaveOS)

**Audience:** Architecture and planning — especially anything downstream of on-device DSP (e.g. companion accelerators, Jetson-class hardware).  
**Date:** 2026-04-12  
**Scope:** `firmware-v3` as built in this repository. External launch-planning repos are out of scope unless cited.

**Related artefacts:** [INFERENCE_TASK_PLACEMENT_MATRIX.md](INFERENCE_TASK_PLACEMENT_MATRIX.md) — task–placement matrix **rev 0.3+** (parallel SSA research: ESV11 `EsBeatClock` vs `MusicalGrid`, merge hold semantics, translation silence vs defaults, effect-count vs `features.h` comments, wire drift nuance); **Yes / No / Research only** mandatory column; use for programme sign-off and Orin-downstream planning.

---

## Evidence tags (mandatory discipline)

| Tag | Meaning |
|-----|---------|
| **Verified** | Directly supported by source, config, or measured values in this tree. |
| **Inference** | Reasonable conclusion from Verified facts; not explicitly named as a product requirement. |
| **Hypothesis** | Plausible but not established; needs experiment or stakeholder input. |
| **Unknown / requires spec** | Cannot be answered from the repo; needs an authored spec, owner, or artefact. |

### Vocabulary: three classes of “inference”

The word **inference** is overloaded. In programme and matrix documents, classify work into exactly one of:

1. **Deterministic DSP** — fixed transforms (FFT, Goertzel, flux, STM, RMS).
2. **Heuristic / symbolic** — rules, thresholds, translation to `SceneParameters`, chord/style bucketing, merge arbitration.
3. **Statistical / ML** — learned models; **as-built K1 firmware** has no shipped on-S3 ML inference unless a future row cites one; external producers (e.g. Trinity macros) are **Statistical_ML (external)** until product names the model.

This brief’s body may still say “inference tasks” loosely; the **matrix** enforces the split.

---

## 1. The single blocking question (still open)

> **Which exact inference tasks must exist, at what latency and memory budget, on which execution target?**

**Unknown / requires spec** — The repository defines a large *as-built* analytical pipeline and render contracts, but it does **not** authoritatively answer:

- Which tasks are **mandatory** vs optional for product intent.
- Per-task **SLO** (deadline, jitter, acceptable drop rate) beyond local comments and `CONSTRAINTS.md`.
- Whether future heavy models run **on S3**, **on Tab5**, **on a tethered SoM (e.g. Orin)**, or split across them — that is a **programme decision**, not derivable from firmware alone.

**Inference:** Until that spec exists, **hardware discussions (Orin, PCIe, etc.) remain downstream**: you cannot size an accelerator without a task graph, budgets, and placement.

---

## 2. Execution topology (where work already runs)

| Fact | Tag |
|------|-----|
| `AudioActor` is documented and structured as **Core 0** capture + DSP; `RendererActor` as **Core 1** at **120 FPS** with `FRAME_TIME_US = 1_000_000 / TARGET_FPS` (~8333 µs). | **Verified** (`AudioActor.h`, `RendererActor.cpp`, `RendererActor.h`, `Actor.h`) |
| Renderer self-clocks to the frame budget using `esp_timer` one-shot pacing after each frame. | **Verified** (`RendererActor.cpp`) |
| `ControlBusFrame` is the **by-value** contract published toward the render side; `static_assert` caps raw + frame structs at **5120 bytes** each. | **Verified** (`ControlBus.h`) |
| `AudioMappingRegistry` may use **PSRAM** for the mapping table at startup (`begin()`), not in the per-pixel render hot path. | **Verified** (`AudioEffectMapping.h` comments) |

---

## 3. Render-side ceiling (anything that feeds effects this frame)

| Constraint | Value / rule | Tag |
|------------|----------------|-----|
| Frame budget (target) | ~**8.33 ms** at 120 FPS | **Verified** (`RendererActor.h`, `CONSTRAINTS.md`) |
| Effect CPU (project rule) | **&lt; ~2 ms** typical budget called out for effect work | **Verified** (`CONSTRAINTS.md`; aligns with `CLAUDE.md` 2.0 ms ceiling language) |
| `FastLED.show()` | Wire-limited, **~4.8–6.3 ms** range documented from measurement | **Verified** (`CONSTRAINTS.md`) |
| Heap in `render()` | **Forbidden** (project invariant) | **Verified** (`CONSTRAINTS.md`, `CLAUDE.md`) |

**Inference:** Any **new** inference whose outputs must influence **this** LED frame should be budgeted inside the above stack, or explicitly placed **off the critical path** with defined **staleness** (e.g. merge layer / async agent pattern).

---

## 4. As-built “inference tasks” on ESP32-S3 (audio + structure)

These are **signal processing and scene inference** already in-tree. They are **not** presented as a single numbered experiment matrix in this repo (see §7).

### 4.1 Backend selection (mutually exclusive at compile time)

| Fact | Tag |
|------|-----|
| `FEATURE_AUDIO_BACKEND_ESV11` vs `FEATURE_AUDIO_BACKEND_PIPELINECORE` are **mutually exclusive** (`#error` if both). | **Verified** (`features.h`) |
| Production guidance in project `CLAUDE.md`: **ESV11** is the supported production backend; **PipelineCore** called out as broken for beat tracking — treat PipelineCore as **engineering / alternate path**, not a product promise. | **Inference** (governance doc; still **Verified** that two backends exist in code) |

### 4.2 PipelineCore path (when enabled)

| Task (conceptual) | Implementation anchor | Latency / memory notes | Tag |
|-------------------|-------------------------|-------------------------|-----|
| 256-bin FFT magnitude, bands, chroma, log-flux, onset envelope, peak pick, beat tracker + compat `TempoTracker` | `PipelineCore.h` / `PipelineCore.cpp` | `FeatureFrame` exposes `process_us` / `max_process_us`; hop/window defaults in `PipelineConfig` (e.g. hop 256, window 512 — **rate-dependent**). Static buffers: `m_magSpectrum[256]`, FFT workspace, hop/window buffers. | **Verified** (struct layout); **Inference** (absolute µs on silicon without a cited measurement table in this brief) |

### 4.3 ESV11 + shared ControlBus producers (typical production narrative)

| Task (conceptual) | Contract / producer | Tag |
|-------------------|---------------------|-----|
| 64-bin Goertzel spectrum, chroma, tempo / beat fields, ES raw parity fields | `ControlBus.h` (`ControlBusRawInput` / `ControlBusFrame`) | **Verified** |
| 1024-point spectral flux onset detector | Fields `onsetFlux`, `onsetEnv`, `onsetEvent`, band fluxes, `kickTrigger` | **Verified** (`ControlBus.h` comments) |
| STM dual-edge decomposition | `stmTemporal[16]`, `stmSpectral[42]`, energies, `stmReady` | **Verified** |
| Chord state from chromagram | `ChordState` | **Verified** |
| Musical saliency frame | `MusicalSaliencyFrame saliency` | **Verified** |
| Style classification | `MusicStyle currentStyle`, `styleConfidence` | **Verified** |
| Motion-semantic extension scalars | `timing_jitter`, `syncopation_level`, `pitch_contour_dir` | **Verified** |
| Perceptual translation → scene | `SceneParameters scene` (`TranslationEngine.h`: includes `tension`, `motion_rate`, …) | **Verified** |

### 4.4 Optional / flagged analytical cost (compile-time)

| Component | Documented cost / behaviour | Tag |
|-----------|----------------------------|-----|
| Musical saliency | Comment: **~80 µs per hop**; few effects use | **Verified** (`features.h`) |
| Style detection | Comment: **~60 µs per hop** (amortised over 4 s windows) | **Verified** (`features.h`) |
| SB parity side-car | Decimation `AUDIO_SB_SIDECAR_DECIMATION` (default 1 = every hop) | **Verified** (`features.h`) |
| VRMS metrics / benchmark | Production VRMS default **on**; benchmark gated; VRMS compute described as **60 Hz decimation** on Core 1 in `VRMSMetrics.h` | **Verified** (`features.h`, `VRMSMetrics.h`) |

### 4.5 Input merge and external agents

| Fact | Tag |
|------|-----|
| `InputMergeLayer` arbitrates MANUAL / AUDIO / AI_AGENT / GESTURE into shared parameters; integrated in renderer path. | **Verified** (`InputMergeLayer.h`, `SESSION_HANDOVER_20260323.md`) |
| WebSocket / serial can submit merge payloads (`merge.submit`, serial `merge`). | **Verified** (handover + `WsStreamCommands.cpp` references in repo) |

**Inference:** This is the **intended seam** for slow or remote “inference” (e.g. LLM or large model on a host): publish **parameters** or low-rate controls, not per-LED tensors, unless separately specified.

---

## 5. What `AudioEffectMapping` actually wires (vs full `ControlBus`)

| Fact | Tag |
|------|-----|
| `AudioSource` enum for auto-mapping covers RMS/flux/bands/bass/mid/treble/beat phase/BPM/tempo confidence — **not** chroma vectors, `bins256`, STM tensors, `scene.tension`, etc. | **Verified** (`AudioEffectMapping.h` / `.cpp`) |
| Effects can still read rich state via `EffectContext` / `ControlBus` paths. | **Inference** from architecture (effects catalog); **Verified** that `ControlBusFrame` carries the rich fields |

**Inference:** Adding a **new** model output for **mapping-only** consumers requires extending `AudioSource` + `applyMappings()` logic; using fields only inside custom effects requires **effect-level** contracts only.

---

## 6. Kuramoto transport (example of “tension” consumption)

| Fact | Tag |
|------|-----|
| `KuramotoTransportEffect` uses `scene.tension` when computing kick strength. | **Verified** (`KuramotoTransportEffect.cpp`) |
| Whether “tension” matches a future external model’s definition is a **semantic alignment** question. | **Unknown / requires spec** |

---

## 7. Missing programme artefacts (do not conflate with code)

| Artefact | Status in this repo | Tag |
|----------|---------------------|-----|
| `K1_CTO_Salvaged_Findings.md` (experiments, salvaged signals) | **Referenced** as created during CTO review in `SESSION_HANDOVER_20260323.md`; **file not present** under `firmware-v3/docs/` or `research/` in this tree. | **Verified** (reference exists); **Unknown / requires spec** (contents) |
| `docs/specs/P1_InputMergeLayer.md` | Cited in same handover; **not** present under `firmware-v3/docs/specs/` (only STM specs there). | **Verified** (absence); **Unknown / requires spec** (normative prose) |

**Hypothesis:** Those documents live in a **different clone** (`lw-perceptual-translation-v3`) or external storage; recovery is a **process** action, not a code search.

---

## 8. Jetson Orin (or any companion accelerator)

| Statement | Tag |
|-----------|-----|
| No Orin- or Jetson-specific **firmware integration** (drivers, RPC, tensor ingress) exists in `firmware-v3/src/`. Tangential mentions may appear in `docs/research/findings/`. | **Verified** (source tree scope) |
| Sizing Orin requires §1 answered **first**. | **Inference** |

---

## 9. Recommended next actions (execution order)

1. **Recover or rewrite** the CTO / P1 markdown artefacts if they remain the programme source of truth — else promote **`SESSION_HANDOVER_20260323.md`** + this brief as interim. **Unknown / requires spec** which is canonical.
2. **Seeded table delivered** — see [INFERENCE_TASK_PLACEMENT_MATRIX.md](INFERENCE_TASK_PLACEMENT_MATRIX.md). Next step: product + firmware DRIs sign off `Mandatory_for_product_ship`, failure policies, and any off-S3 targets; add RAM/µs columns in a spreadsheet if programme requires more granularity than this repo provides.
3. Cross-check **PipelineCore vs ESV11** parity only for fields that **product** promises to third parties (`docs/protocol/` if exposed on wire).
4. Only then: **Orin** — partition the task graph into what must stay on S3 (real-time, tight loop) vs what can tolerate **latency + transport** (merge / WS / REST).

---

## 10. Document provenance

| Path | Role |
|------|------|
| `firmware-v3/src/audio/pipeline/PipelineCore.h` | PipelineCore task breakdown + buffers |
| `firmware-v3/src/audio/contracts/ControlBus.h` | Published frame contract + size bounds |
| `firmware-v3/src/audio/TranslationEngine.h` | Scene parameters including `tension` |
| `firmware-v3/src/config/features.h` | Feature flags + stated µs costs for saliency/style |
| `firmware-v3/src/core/actors/RendererActor.cpp` | 120 FPS pacing + timing |
| `firmware-v3/CONSTRAINTS.md` | Frame / effect / LED timing budgets |
| `firmware-v3/docs/SESSION_HANDOVER_20260323.md` | Historical CTO artefact claims + merge/VRMS narrative |
| `firmware-v3/docs/design/INFERENCE_TASK_PLACEMENT_MATRIX.md` | Seeded placement + wire + mapping-bucket rules |

This brief **does not** replace a product-level inference spec; it **grounds** discussion in repository facts and tags uncertainty explicitly.
