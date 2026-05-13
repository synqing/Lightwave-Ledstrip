# K1 Medium Programme Phase 0/1 Decision Record - 2026-05-12

RBDO label: DEGRADED-MODE.

## Decision

The existing repo already contains useful optical/math source material. The immediate problem is not missing visual concepts; it is classification, metadata, and proof.

Do not implement a global optical/medium pass yet. Do not implement Adaptive Medium State, a new soliton/wave engine, attractor idle, 240 Hz timing work, colour-correction placement changes, or silence-policy metadata changes in this phase.

Proceed only to a bounded hardware evidence pass when power-safe controls are agreed, using the required candidate set and the evidence file naming rule.

## Viable Source Material

Strongest source material:

- `0x0201` LGP Holographic: proven flagship direction, controlled palette path, useful depth/interference source.
- `0x0204` LGP Wave Collision: current source has audio-reactive wave/collision logic and centre subpixel collision core.
- `0x0900` LGP Chromatic Lens, `0x0901` LGP Chromatic Pulse, `0x0902` Chromatic Interference: direct chromatic/dispersion source material for optical-transfer thinking.
- `0x1B01` KdV Soliton Pair: already implements the fake-soliton direction as staged `sech^2` profiles, not a full PDE solver.

Useful but higher-risk source material:

- `0x0202` LGP Modal Resonance: standing-mode control case.
- `0x0407` LGP Photonic Crystal: audio-reactive optical texture but nondeterministic defects must be controlled.
- `0x0505` LGP Fluid Dynamics: persistent pressure/velocity source, but memory/timing evidence required.
- `0x1B00`, `0x1B05`-`0x1B08` Time-Reversal Mirror family: visually ambitious, but PSRAM/history/tone-map/audio-metadata risks make them diagnostic until proven.

Not current active source material:

- `0x1903` Lorenz Ribbon: quarantined and unregistered in current source. Defer until the task-WDT issue is explicitly resolved.

## Pipeline-Risky Effects

Pipeline-risky because of effect-owned chromatic/optical logic:

- `0x0900`, `0x0901`, `0x0902`.
- These should not receive a creative global chromatic pass by default.

Pipeline-risky because of previous-frame/history/persistence:

- `0x0204`, `0x0603`, `0x1B00`, `0x1B05`-`0x1B08`.
- These need explicit medium policy before any persistence, blur, or temporal layer is applied.

Pipeline-risky because of tone map:

- `0x1B00`, `0x1B05`, `0x1B06`, `0x1B07`, `0x1B08`.
- These are already tuned with tone mapping and should not be judged through a new medium pass without a before/after diagnostic.

Pipeline-risky because of silence/audio metadata mismatch:

- `0x1B00`, `0x1B01`, `0x1B05`, `0x1B06`, `0x1B07`, `0x1B08`.
- Source consumes `ctx.audio`, but the current registry audio-reactive set does not list these effects. `vp stack` hard-gate reporting can therefore understate their silence-policy needs.

Pipeline-risky because unavailable/quarantined:

- `0x1903`.

## MediumPolicy Recommendation Only

Do not implement this enum in Phase 0/1. The minimum viable shape remains:

```cpp
enum class MediumPolicy {
    None,
    CorrectiveOnly,
    CreativeAllowed,
    EffectOwned,
    DiagnosticOnly
};
```

Initial policy mapping recommendation:

| Effect class | Recommended policy | Reason |
|---|---|---|
| Required chromatic effects: `0x0900`, `0x0901`, `0x0902` | EffectOwned | They already implement dispersion/fringe behaviour. |
| Required interference/advanced optical effects: `0x0201`, `0x0202`, `0x0204`, `0x0406`, `0x0407` | EffectOwned by default; some may become CreativeAllowed only after hardware review | Current colour-correction skip implies fragile optical intent. |
| Required physics/organic/quantum effects: `0x0505`, `0x0603`, `0x1B00`, `0x1B05`-`0x1B08` | EffectOwned or DiagnosticOnly | Persistence/history/PSRAM/tone-map risks are too high for inferred eligibility. |
| `0x1B01` KdV Soliton Pair | EffectOwned, source-material | Existing fake-soliton source should be evaluated before new engine work. |
| `0x1903` Lorenz Ribbon | DiagnosticOnly | Current source quarantine blocks active runtime use. |
| Basic palette-driven non-candidate effects | CorrectiveOnly or CreativeAllowed | Not decided here; outside candidate set. |
| Future fake-soliton engine | CreativeAllowed, effect-local first | Only after audit and hardware review prove the gap. |

## Smallest Safe Opt-In Insertion Point

The smallest safe next step is not a global insertion point.

The smallest safe opt-in path is:

1. Run the required candidates as single-effect hardware baseline samples.
2. Force EdgeMixer MIRROR for substrate isolation.
3. Capture `vp stack`, `s`, `dbg memory`, control state, EdgeMixer state, silence state, timing/load, show skips, failures, RMT errors, underruns.
4. Store each hardware-tested sample as `k1_medium_phase0_<effectid>_<effectname>_<date>_serial.md`.
5. Batch Captain visual verdicts in one table.
6. Only after that, choose an effect-local diagnostic A/B pass for one candidate family.

If a medium transform is later prototyped, it should be effect-local or diagnostic-only first, operating on the actual authored surface confirmed by `vp stack`. It must not be promoted to global/default behaviour from this audit.

## Hardware Baseline Controls

The requested fixed controls remain the audit target, except brightness must obey current power safety:

- brightness: power-safe baseline `160` (no external power case was confirmed for a higher value).
- speed: `27` where applicable.
- intensity: `128`.
- saturation: current product default unless visual test requires `253`.
- complexity: `128`.
- variation: `0` or current default, exact value recorded.
- EdgeMixer: MIRROR for substrate isolation.
- palette: fixed known K1 palette, exact ID/name recorded.
- audio: same repeatable source where available.

Any second pass using current product EdgeMixer/tetradic state must be marked comparison, not baseline.

## Explicitly Deferred Work

Deferred non-goals:

- production visual default changes;
- global optical/medium pass;
- `MediumPolicy` code;
- Adaptive Medium State;
- new wave/soliton engine;
- attractor idle;
- 240 Hz or timing/cadence work;
- colour-correction placement fix;
- silence-policy metadata implementation;
- VP substrate patch;
- refactoring or cleanup;
- extra candidates.

Deferred candidate IDs unless directly required as duplicate, parent, variant, or control case:

- `0x1000`
- `0x1202`
- `0x0E08`
- `0x060B`

Do not expand the candidate set merely because time remains.

## Stop Condition

Phase 0/1 stops here for source/VP documentation: required candidate set is complete, VP sufficiency decision is written, and implementation work remains explicitly deferred.

The next authorised work should be a hardware evidence pass only, with power-safe brightness agreed first.
