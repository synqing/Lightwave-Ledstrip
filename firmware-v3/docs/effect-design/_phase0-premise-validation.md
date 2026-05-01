# K1 Effect Repair Workbench — Phase 0 Premise Validation

**Label: DEGRADED-MODE**

- **Unresolved assumption:** Iterations 1-3 could not be fully recovered as verbatim transcript in this Codex shell because `$RECALL_CLI` is unset; this report uses local source, `claude-mem` observations, the persisted 0x1B04 implementation plan, and Captain-provided iteration-4 postmortem text.
- **Risk if wrong:** The retroactive rules check may over-credit or under-credit which failures the workbench rules would have caught.
- **Fallback:** Treat the rules fixtures as provisional until the verbatim iteration messages are recovered; Track B remains valid because it does not claim prediction or hardware truth.
- **Revisit trigger:** `$RECALL_CLI` becomes available or Captain provides the verbatim iteration 1-3 messages.
- **Debt count / affected outputs:** 3 — this report, the Track B v0 rules fixtures, and any future `0x1B04` directive.

## 1. Upstream Facts

| Fact | Evidence |
|---|---|
| `0x1B04` is `EID_LGP_FRESNEL_CAUSTIC_SWEEP` / Fresnel Caustic Sweep. | `firmware-v3/src/config/effect_ids.h:326`, `firmware-v3/src/effects/PatternRegistry.cpp:250` |
| Current source contains a sinusoidal focus sweep. | `firmware-v3/src/effects/ieffect/LGPFresnelCausticSweepEffect.cpp:180-198` |
| Current source still uses RMS as a continuous sweep-rate bridge. | `firmware-v3/src/effects/ieffect/LGPFresnelCausticSweepEffect.cpp:152-156` |
| Current source still uses `ctx.gHue` as an audio-independent hue base. | `firmware-v3/src/effects/ieffect/LGPFresnelCausticSweepEffect.cpp:247-250` |
| Current source still uses beat flash separate from the selected kick-only story. | `firmware-v3/src/effects/ieffect/LGPFresnelCausticSweepEffect.cpp:158-164`, `240-245` |
| First-class onset events exist for beat, downbeat, transient, kick, snare, and hihat. | `firmware-v3/src/plugins/api/OnsetContext.h:16-39` |
| Effect-facing chroma, beat, bands, silence, and onset accessors exist in `EffectContext`. | `firmware-v3/src/plugins/api/EffectContext.h:134-178`, `276-367` |
| Runtime protocol authority is currently firmware, not YAML. | `BACKLOG.md:43-49` |
| Forward TODO handoffs in `.claude/handoff*.md` are forbidden. | `AGENTS.md:38-46` |

## 2. Rules Checklist

| Rule ID | Rule | Status |
|---|---|---|
| `R-CENTRE-001` | Dominant geometry must originate at centre pair 79/80 or move inward to it. No linear sweep primitive. | blocked on violation |
| `R-MOTION-001` | Dominant visible position must not reverse in pixel space via sin, cos, triangle, ping-pong, or pendulum functions. | blocked on violation |
| `R-AUDIO-001` | Dominant visible change must be a direct consequence of one selected audio event. | blocked on missing event |
| `R-AUDIO-002` | Broadband magnitudes (`rms`, `fastRms`, `flux`, `fastFlux`) must not continuously drive visible motion, position, or brightness for 0x1B04 repair. | blocked on violation |
| `R-AUDIO-003` | Band magnitudes (`bass`, `mid`, `treble`, `heavy_*`) must not continuously drive 0x1B04 repair output. | blocked on violation |
| `R-AUDIO-004` | Event strength may be sampled at event spawn and frozen for the event consequence; it must not be re-read as a per-frame continuous bridge. | allowed only as spawn seed |
| `R-COLOUR-001` | Hue must not rotate from `ctx.gHue` or a wall-clock hue-wheel. Chroma is allowed only as sampled/frozen or explicitly smoothed harmonic colour. | blocked on violation |
| `R-SILENCE-001` | If the design story says silence is dark, no ambient carrier, fallback glow, or idle scan may remain visible. | blocked on violation |
| `R-PERF-001` | Effect render max must be under 2.0 ms, not just average. | blocked on violation |
| `R-EVIDENCE-001` | Browser preview is not hardware sign-off; predicted frames must be source-labelled. | blocked on false truth claim |

## 3. Retroactive Rules Check

| Reconstructed iteration | Basis | Rules fired | Would catch? |
|---|---|---|---|
| Iteration 1 — original / pre-repair sweep | Current source and design postmortem: sinusoidal focus, RMS speed bridge, `ctx.gHue`, beat flash. | `R-MOTION-001`, `R-AUDIO-001`, `R-AUDIO-002`, `R-COLOUR-001`, conditional `R-SILENCE-001` | yes |
| Iteration 2 — deletion-only RMS patch | Persisted plan says deletion patch removed RMS coupling but left the file without a designed replacement bridge. | `R-AUDIO-001`; likely `R-MOTION-001` and `R-COLOUR-001` if carrier unchanged | yes |
| Iteration 3 — kick-ring overlay on scanning carrier | Persisted plan locks a constant visible scanning lens plus kick rings; Captain postmortem rejects carrier/pendulum disease and later notes measured `render_us_max=2331`. | `R-MOTION-001`, `R-SILENCE-001`, `R-COLOUR-001`, `R-PERF-001`, and for retained beat flash `R-AUDIO-001` | yes |

Result: **3/3 provisional failures caught**. This is not a grounded Phase 0 pass: the iterations are reconstructed, so the rules check may be construction-circular. Confidence remains degraded until verbatim iteration messages are recovered and the rules check is rerun against those messages.

## 4. Byte-Strip Perception Test

No usable LED byte capture was found in the checkout. Existing capture infrastructure is present:

- MabuTrace JSON capture: `firmware-v3/tools/capture_trace.py`
- Serial binary frame capture: `firmware-v3/testbed/evaluation/capture_cli.py`
- Serial parser documents 1009-byte v2 and 529-byte v4 capture frames in `firmware-v3/testbed/evaluation/frame_parser.py`.
- Web LED stream docs mention 961-byte and 966-byte LED frame variants in `firmware-v3/docs/api/api-v2.md` and `firmware-v3/docs/api/api-v1.md`.

Captain perception answer: **not collected in this turn**. Therefore Phase 1 must not claim preview sufficiency. Track B is the honest v0, limited to directive authoring and actual-capture replay until a capture is imported and Captain answers the perception question.

## 5. Catalogue Sketches

### AudioSignalCatalogue

| Key | Class | Evidence | v0 status |
|---|---|---|---|
| `onset.kick.fired` + `.reliable` | dominant event | `OnsetContext.h:16-39` | supported |
| `onset.snare.fired` + `.reliable` | event | `OnsetContext.h:16-39` | supported |
| `onset.hihat.fired` + `.reliable` | event | `OnsetContext.h:16-39` | supported |
| `onset.transient.fired` | event | `OnsetContext.h:26-34`, `EffectContext.h:300-307` | supported |
| `onset.beat.fired` / `isOnBeat()` | event | `EffectContext.h:137-138` | supported, not for 0x1B04 iter-4 story |
| `chroma()` / `heavyChroma()` | harmonic colour | `EffectContext.h:276-287` | supported as colour source |
| `rms`, `fastRms`, `flux`, `fastFlux` | broadband continuous | `EffectContext.h:96-101` | blocked for 0x1B04 dominant motion |
| `bass`, `mid`, `treble`, `heavy*` | band continuous | `EffectContext.h:114-131` | blocked for 0x1B04 dominant motion |
| `bins64` / `musicalRange` | spectrum vector | `EffectContext.h:373-385` | research for this workbench |
| `bins256`, `binHz` | internal/debug | `ControlBus.h` PipelineCore fields | blocked by default |
| `hfEnergy`, `hfFlux`, `hatEvent` | HF semantic tier | `EffectContext.h:334-352` | blocked until HF gates pass |
| direct `ctx.audio.controlBus.*` reads | reach-through | audio-visual contract surface | blocked; use accessors/semantic onset |

### TransportCatalogue

YAML describes client/K1 commands such as `audio.parameters.get`, `audio.parameters.set`, `stimulus.patch`, and `beat.subscribe` (`docs/protocol/k1-ws-contract.yaml:700-728`, `1454-1469`, `1598-1616`). It is not the audio-signal catalogue for effect authoring. Because `BACKLOG.md:43-49` says firmware is runtime source of truth, workbench entries need labels such as `source-confirmed`, `yaml-documented`, `firmware-only/docs-drift`, `compile-gated:<FEATURE>`, `regex-route`, `broadcast-only`, `tab5-only`, `deprecated-no-handler`, `unregistered`, or `invariant-blocked`.

Current explorer reconciliation found concrete docs drift: `vrms.subscribe`, `vrms.unsubscribe`, and `merge.submit` are active firmware WS registrations missing from YAML; `GET /api/v1/vrms` and legacy `POST /update` are firmware REST routes missing from YAML. This confirms Track B should treat YAML as enrichment, not authority.

### VisualPrimitiveCatalogue

| Primitive | v0 status | Evidence |
|---|---|---|
| Centre-origin pulse / shockwave / outward event consequence | supported | Effect standard maps onset/percussion to centre flash/burst (`EFFECT_DEVELOPMENT_STANDARD.md:353-355`) |
| `drawDot`, `drawSpriteScrolled`, `fillFromBins` | supported shared primitives | `RenderPrimitives.h` shared Layer 4 primitive API |
| Chroma-derived palette colour | supported | Effect framework requires palette/hue from chromagram (`EFFECT_FRAMEWORK_STANDARD.md:70-78`) |
| Byte-strip actual capture replay | supported | Serial parser and API docs provide frame sizes |
| Linear sweep | blocked | Centre-origin MUST (`EFFECT_FRAMEWORK_STANDARD.md:62-66`) |
| Rainbow / hue-wheel | blocked | No-rainbow enforcement (`EFFECT_FRAMEWORK_STANDARD.md:70-78`) |
| Pendulum / ping-pong position | blocked for repair directives | Captain doctrine from 0x1B04 postmortem; encoded as `R-MOTION-001` |
| Bespoke full-strip render loop where shared primitives exist | blocked by workbench directive policy | Pipeline reform doctrine prefers primitive calls over bespoke per-pixel loops |
| Browser LGP bloom model | deferred | Requires calibrated capture rig; not proven in v0 |

## 6. Directive Schema Lock

Every generated directive must include:

- Sign-off status: rule-check level, hardware truth, Captain visual approval, LGP calibration status.
- Design intent: visual identity, dominant event, silence behaviour, forbidden behaviours.
- Two-sentence visual story.
- Accepted AP/VP pairing with source evidence.
- Blocked behaviours with rule IDs.
- Evidence used: catalogue source hashes/versions, capture paths, rule catalogue version.
- Prediction summary: explicitly `NOT PROVEN` for Track B.
- Implementation hints: source areas, state shape, render path constraints.
- Required tests.
- Not proven: LGP readability, final firmware implementation correctness, subjective approval, perceptual bloom/scatter.

## 7. Phase 0 Gate

Rules catch: **3/3 provisional, not grounded**.

Truth-strip sufficiency: **not established**.

Gate decision: **Provisional pass only; Phase 2 is blocked until verbatim iteration 1-3 audit is complete.** Track B may be used for directive authoring and capture replay, but not as evidence that the rules premise is validated.
