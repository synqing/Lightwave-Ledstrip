# F-6 ControlBus Zone Count Audit

**Date:** 2026-05-06
**RBDO label:** GROUNDED
**Scope:** Phase 1 audit only for `BACKLOG.md` F-6. No refactor, no firmware behaviour change.
**Verdict:** `CONTROLBUS_NUM_ZONES = 4` is a real source/documentation hazard, but it is not currently a live production K1v2 ESV11 audio-shaping path. Implementation still requires Captain selection of the band-restructuring approach plus hardware validation.

## Source Facts

| Source | Finding |
|---|---|
| `firmware-v3/src/audio/contracts/ControlBus.h:20-22` | Defines the disputed constant as `4` and labels it "4 zones across frequency spectrum". |
| `firmware-v3/src/audio/contracts/ControlBus.h:303-307` | Documents the 8-band split as four 2-band buckets: `0-1`, `2-3`, `4-5`, `6-7`. |
| `firmware-v3/src/audio/contracts/ControlBus.h:520-530` | Allocates `m_zones[CONTROLBUS_NUM_ZONES]` and `m_chroma_zones[CONTROLBUS_NUM_ZONES]`; comments also describe 4 chroma zones. |
| `firmware-v3/src/audio/contracts/ControlBus.cpp:61-64` | Reset loops over all zones for band and chroma followers. |
| `firmware-v3/src/audio/contracts/ControlBus.cpp:95-115` | Runtime setter loops for band/chroma AGC rates and min floor use the constant directly. |
| `firmware-v3/src/audio/contracts/ControlBus.cpp:372-408` | Stage 3 band Zone AGC loops `z < CONTROLBUS_NUM_ZONES`, using `start_band = z * 2`, `end_band = start_band + 2`. |
| `firmware-v3/src/audio/contracts/ControlBus.cpp:439-475` | Stage 3b chroma Zone AGC loops `z < CONTROLBUS_NUM_ZONES`, using `start_bin = z * 3`, `end_bin = start_bin + 3`. |
| `firmware-v3/src/audio/contracts/ControlBus.h:458-469` | `applyDerivedFeatures()` computes chord/liveliness/saliency/silence only; it is not the Zone AGC path. |
| `firmware-v3/src/audio/AudioActor.cpp:1041` | Production ESV11 calls `applyDerivedFeatures(frame, ...)`, not `UpdateFromHop()` Stage 3 Zone AGC. |
| `firmware-v3/src/audio/AudioActor.cpp:1928` and `:3679` | Legacy/non-ES paths still call `UpdateFromHop()`, so the 4-zone AGC remains live in those paths. |
| `firmware-v3/src/network/webserver/ws/WsAudioCommands.cpp:507-510` | WS `audio.zone-agc.get/set` returns feature-disabled in ESV11 backend builds. |
| `firmware-v3/src/network/webserver/handlers/AudioHandlers.cpp:1106-1111` | REST `/api/v1/audio/zone-agc` returns feature-disabled in ESV11 backend builds. |
| `docs/protocol/k1-ws-contract.yaml:791-811` | WS contract marks audio Zone AGC as PipelineCore only. |
| `docs/protocol/k1-rest-contract.yaml:331-337` | REST contract exposes Zone AGC routes without the same visible PipelineCore-only caveat. |
| `firmware-v3/docs/audio-visual/AUDIO_OUTPUT_SPECIFICATIONS.md:103` and `:960` | Existing docs already claim `CONTROLBUS_NUM_ZONES = 3`, contradicting current source. |
| `firmware-v3/src/audio/GoertzelAnalyzer.cpp:129-130` | Separate 64-bin Goertzel metadata also assigns four 16-bin zones. This is not `CONTROLBUS_NUM_ZONES`, but it is another 4-way audio-zone concept to review before public claims. |

## Current 4-Zone Partition

| Surface | Current partition | Purpose |
|---|---|---|
| 8-band AGC | Zone 0: bands `0-1`; Zone 1: `2-3`; Zone 2: `4-5`; Zone 3: `6-7` | Per-frequency max follower so bass does not dominate mids/highs. |
| 12-chroma AGC | Zone 0: chroma `0-2`; Zone 1: `3-5`; Zone 2: `6-8`; Zone 3: `9-11` | Per-pitch-class max follower with 3 chroma bins per follower. |
| Goertzel metadata | `bin >> 4`, four 16-bin buckets across 64 bins | Bin metadata only; no direct evidence found that it bridges to user-facing zone IDs. |

## Consumer Map

| Consumer class | Files | Impact if constant changes to 3 |
|---|---|---|
| ControlBus band/chroma AGC internals | `ControlBus.h`, `ControlBus.cpp` | Requires real repartition logic; changing only the constant would leave `z * 2` covering only bands `0-5` and `z * 3` covering only chroma `0-8`, silently dropping the top bands/chroma. |
| Legacy/PipelineCore API snapshot | `AudioActor.h`, `AudioActor.cpp` | Arrays auto-resize, but API payload count and indexed debug output change. Production ESV11 currently disables this surface. |
| REST/WS handlers | `AudioHandlers.cpp`, `WsAudioCommands.cpp` | Non-ES builds would expose fewer `zones[]` entries. ESV11 remains feature-disabled. |
| Bench toggles | `BenchRegistry.*`, `test_control_bus_bench_toggles.cpp` | Toggles are count-agnostic; tests prove enabled/disabled behaviour, not zone count. |
| Contracts/docs | `k1-rest-contract.yaml`, `k1-ws-contract.yaml`, `AUDIO_OUTPUT_SPECIFICATIONS.md`, Trace docs, NotebookLM source bundle | Several surfaces are stale or intentionally struck. Contract/doc cleanup must follow the chosen implementation. |
| Unrelated user-zone docs | `src/core/state/README.md`, `src/core/state/QUICK_REFERENCE.md`, `src/effects/README.md`, reference-effect comments | These are not `CONTROLBUS_NUM_ZONES` consumers, but they still leak 4-zone/user-zone language and should be cleaned separately if Captain wants the hard rule enforced repo-wide. |

## Implementation Hazards

1. A mechanical `4 -> 3` change is unsafe. The current band/chroma loops assume even `2` and `3` items per zone respectively.
2. Dropping zone 3 would suppress or under-normalise high bands `6-7` and chroma bins `9-11`, exactly the material used for hats, cymbals, brilliance, and upper pitch classes.
3. A 3-zone band split must be explicitly chosen, for example `0-1 / 2-4 / 5-7`, `0-2 / 3-5 / 6-7`, or another musically justified grouping.
4. A 3-zone chroma split cannot preserve the current 3-bins-per-zone layout; it needs a 4/4/4 grouping, a disabled chroma-zone AGC path, or a new pitch-class normalisation strategy.
5. The production ESV11 path currently bypasses this AGC path, so hardware validation must use a build/path that actually exercises the changed code or a deliberate ESV11 semantic change.

## Recommendation

Keep F-6 open, but mark Phase 1 audit complete.

Do not implement until Captain chooses the restructuring direction. The safest future engineering plan is:

1. Add explicit zone-boundary tables/helpers instead of deriving ranges from `z * 2` and `z * 3`.
2. Unit-test that every band `0-7` and every chroma bin `0-11` is covered exactly once.
3. Pick a product-safe 3-zone partition after Captain chooses the strategy.
4. Build and flash the relevant path, then hardware-test AGC behaviour against the approved reference audio corpus before committing.

## Deferred Captain Decision

Captain still needs to choose one direction before implementation:

| Option | Meaning | Risk |
|---|---|---|
| A | Drop the fourth/high zone | Lowest code churn, highest visual/audio risk for hats/cymbals/brilliance. |
| B | Merge two adjacent zones | Moderate churn; choice of which merge changes spectral balance. |
| C | Repartition to three explicit musical buckets | Best long-term model; requires tests and hardware A/B because it changes normalisation semantics. |
