---
abstract: "ADR for Zone Composer Instrument Program Phase 1 gating decisions (D-1 through D-5). Records recommended defaults, alternatives rejected, reversibility, and downstream impact for: effect instance isolation, expression override semantics, zone-aware effect parameter setter (gated behind D-1), audio routing semantics, demo audio fixture sourcing. Captain may approve all defaults with one statement or override per-decision."
---

# ADR-001: Zone Composer Architecture Decisions (D-1 through D-5)

**Status:** APPROVED — Captain sign-off 2026-05-01 ("ADR defaults approved for D-1 through D-5")
**Date:** 2026-05-01 (drafted), 2026-05-01 (approved)
**Branch:** firmware/heap-shed-fragmentation-fix (HEAD ~9d7bc261)
**Companion plan:** `~/.claude/plans/zone-composer-instrument-program.md`
**Verification pass:** COMPLETE — all six claims (V1-V6) confirmed against HEAD with verbatim code; findings recorded in Appendix A below
**Sequencing (preserved):** D-1 → D-3 → D-4 → D-2; D-5 parallel as fixture gate
**D-5 fixture constraint (preserved):** approved benchmark corpus only (`/Users/spectrasynq/Workspace_Management/Software/hybrid-beat-tracker/tests/benchmark`); synthetic fixtures require separate Captain approval per file with explicit generation method

---

## Context

Zone Composer Instrument Program requires five architectural decisions locked before Phase 1 implementation can safely proceed. Without them, implementation risks:

- Silent state corruption when same effectId is used in multiple zones (D-1)
- Breaking existing effect contracts that read global expression fields (D-2)
- Shipping zone-aware parameter API that targets a corrupted shared substrate (D-3 without D-1)
- Audio routing that's "wired" but doesn't change visual output (D-4)
- Demo fixtures that violate CLAUDE.md audio playback safety constraint (D-5)

Each decision below is marked with: recommended default, alternatives rejected with reasoning, reversibility, gating dependency, implementation impact, and test impact.

**Gating sequence (D-3 corrected to depend on D-1 per Captain):**

```
D-1 (effect instance isolation)
  └─→ D-3 (zone-aware parameter setter — depends on D-1's instance pool)
        └─→ D-4 (audio routing — independent, but Phase 1 scope)
              └─→ D-2 (expression overrides — secondary generic layer)

D-5 (demo fixtures) is parallel; gates D2 demo work, not D-1/D-3/D-4.
```

---

## D-1 — Effect instance isolation

### Decision (recommended default)
**Per-zone effect instance pool.** ZoneComposer maintains an internal instance pool where each zone slot owns its own instance of any effect, even if multiple zones reference the same `effectId`. Pool slots are keyed by `(effectId, zoneSlot)` and rotated when a zone changes effect.

### Why
`RendererActor::getEffectInstance(effectId)` returns a stable singleton per effectId (verified by 5-SSA context report; targeted verification pass in flight to re-confirm). When zones 1, 2, 3 all use `EID_LGP_HOLOGRAPHIC`, they share ONE instance — its mutable internal state (`m_phase`, accumulators, smoothing, decay state, trail buffers, `m_lastTrigger`, etc.) is rewritten 3 times per frame with 3 different per-zone contexts. This silently corrupts any stateful effect used in multiple zones.

The effects in firmware-v3 are largely NOT stateless — they hold phase counters, smoothing buffers, transient detectors, and accumulators by design.

### Alternatives rejected
- **(b) Externalised state.** Refactor every effect to read/write through `EffectContext.zoneState[MAX_ZONES]`. Touches 100+ effect files; high refactor cost; one missed effect re-introduces the bug.
- **(c) Enforce stateless effects.** Audit + force any internal state to be re-derivable per call. Brittle (no enforcement infrastructure); one careless future effect breaks the invariant silently.

### Reversibility
**Medium-low.** The pool API is internal to ZoneComposer + RendererActor. Refactoring to a different model later is contained but the per-zone instance allocation pattern propagates into snapshot schemas (D-2/A6) and effect lifecycle expectations. Reverting after Phase 1 ships is significant work.

### Gating
**Blocks:** D-3, A4, all per-zone effect work
**Blocked by:** Verification pass V1 confirming singleton behaviour (in flight)

### Implementation impact
- New: `ZoneComposer::m_zoneEffects[MAX_ZONES]` (instance pointers, not just effectIds)
- New: `RendererActor::getOrCreateEffectInstance(effectId, slotKey)` — pool API; ZoneComposer owns slot keys
- New: instance lifecycle — `init()` called once on first slot creation, `deinit()` on slot reuse (when zone changes effect)
- PSRAM cost bounded: ~3× the in-use effect set, NOT 3× all 100+ catalogue effects (only currently-active effects are instantiated)
- New invariant: each pool slot has stable `(effectId, zoneSlot)` association until rotated; render reads from the slot's instance, not the global singleton
- Single-effect (zones-disabled) render path remains unchanged — uses singleton via `getEffectInstance()` as today

### Test impact
- **Spike 1** (gating Phase 1 build): same effectId in zones 1, 2, 3 with controlled audio → state divergence verified per zone
- **E6 regression suite** (Phase 1 exit): unit test asserting per-zone state isolation
- **Performance test**: 3 simultaneous instances of same effect type → frame budget within 2.0 ms
- **Memory test**: pool slot rotation cycles → no leaks (heap stable across N effect changes per zone)

---

## D-2 — Expression override semantics

### Decision (recommended default)
**Augmentation via `EffectContext.zoneOverrides`.** Add an optional override struct alongside global expression fields. Effects opt in by checking `if (ctx.zoneOverrides.intensity.has_value()) ...` else fall back to global `ctx.intensity`. Default behaviour preserved for all existing effects.

### Why
Current effects read global `EffectContext.intensity` / `saturation` / `complexity` / `variation` / `mood` / `fadeAmount` directly. Replacing those values per-zone (without effect cooperation) silently changes behaviour in ways tied to specific effect implementations — some effects rely on global mood for cohesion across zones; replacement breaks them invisibly.

Augmentation requires effect cooperation but is backward-compatible: effects that don't opt in keep their current global behaviour, and the override fields are ignored without consequence.

### Alternatives rejected
- **Replacement.** ZoneComposer overwrites `ctx.intensity` etc. per-zone before each `effect->render()`. Breaks cross-zone cohesion. No way for an effect to say "I want global intensity here."
- **Skip D-2 entirely; rely on D-3 alone.** Effect-specific parameters (D-3) cover the high-leverage cases. Abstract expression knobs are secondary. But abstract knobs are useful for effects that don't expose specific parameters (legacy or simple effects), and for global tone-shaping. Defer is acceptable but not preferred.

### Reversibility
**High.** Adding optional fields is non-breaking. If we deprecate later, the field set can be empty without breaking effects.

### Gating
**Blocked by:** D-1 (effect instance must be isolated before per-zone state matters), D-3 (Phase 1 keystone — D-2 builds on the architectural foundation D-3 establishes)
**Blocks:** Phase 2 abstract expression UI in iOS / dashboard

### Implementation impact
- Add `ZoneOverrides` struct to `EffectContext.h` — fields use `std::optional<uint8_t>` or sentinel `0xFF` for "unset"
- Helper accessors: `ctx.getZoneIntensity()` returns override if set else global
- ZoneComposer populates only when zone has overrides set (otherwise leaves struct cleared)
- Effects that opt in: small refactor to read through accessors

### Test impact
- Effects opting in: regression test for opt-in behaviour
- Effects not opting in: regression test for unchanged global behaviour (continuous gate)

---

## D-3 — Zone-aware effect parameter setter

### Decision (recommended default)
**Ship `zone.effects.parameters.set` — but GATED BEHIND D-1.** Without D-1's instance pool, this endpoint silently corrupts cross-zone state: setting Zone 1's `BassQuake.shockwaveVelocity` mutates the singleton, affecting Zones 2 and 3 that share the same effectId. With D-1 in place, each zone's instance has independent parameter state and the endpoint behaves correctly.

This is the **Phase 1 keystone API** — marries the just-shipped runtime parameter system (`effects.parameters.set` global, commit `e9f37eed`) with zones.

### API shape
```
REST:       POST /api/v1/zones/{zoneId}/effects/parameters
              body: {"parameters": {"name": value, ...}}
WS:         zone.effects.parameters.set
              {"zoneId": <1-3>, "parameters": {...}, "requestId": ...}
SerialJSON: zone.effects.parameters.set
              {"type": "zone.effects.parameters.set", "zoneId": <1-3>,
               "parameters": {...}, "requestId": ...}
Response:   {"zoneId": ..., "effectId": <implicit from zone>,
             "queued": [...], "failed": [...]}
```

`effectId` is **implicit** from the zone's current state (no caller needs to specify which effect; whatever effect Zone N has loaded is the target). This avoids the verification overhead of cross-checking caller's effectId against zone state.

### Why
The runtime parameter system (`effects.parameters.set`) shipped 2026-05-01 enables direct knob-tuning for every effect's declared parameters. It's currently GLOBAL — only the active single-effect can be tuned. The single most leveraged extension is making it per-zone: every effect's specific parameters (BassQuake.shockwaveVelocity, Bloom.scrollRate, Holographic.layerCount, etc.) become per-zone-tunable for free, without adding any new abstraction layer.

iOS Phase 2 already shipped `EffectParameterSheet` (REST + 12 tests) — retargeting it with `zoneId` makes the entire effect catalogue per-zone-tunable on iOS with minimal client work.

### Alternatives rejected
- **Ship D-3 first, defer D-1.** Silently corrupts cross-zone state. Agents would file mysterious bugs. NO.
- **Endpoint requires caller-supplied effectId.** Adds verification overhead — caller must ensure their effectId matches what's in the zone, otherwise endpoint must reject or apply to wrong target. Implicit-from-zone is cleaner.
- **Use abstract expression overrides (D-2) instead.** Abstract knobs are rarely consumed in interesting ways by effects. Concrete effect-specific parameters are what users want.

### Reversibility
**Medium.** Endpoint becomes part of public contract. Changing payload shape later requires versioning. The underlying behaviour (per-zone parameter targeting) is sticky once shipped.

### Gating
**Blocked by:** D-1 (effect instance pool must exist; otherwise endpoint silently corrupts state)
**Blocks:** iOS Phase 1 EffectParameterSheet zone retargeting; bass/body/air demo path; Phase 2 dashboard per-zone parameter editor

### Implementation impact
- ZoneComposer exposes `setZoneParameter(zoneSlot, name, value)` — forwards to per-zone effect instance's `IEffect::setParameter()` (existing API)
- REST handler reads zoneId from path, payload from body, calls ZoneComposer
- WS codec adds `decodeZoneEffectsParametersSet` mirroring existing `decodeEffectsParametersSet`
- SerialJSON: new handler block in `SerialJsonGateway.cpp`
- Response shape mirrors existing `effects.parameters.set` (queued/failed arrays)
- Validation: zoneId range (1 ≤ zoneId ≤ MAX_ZONES — note: 1-INDEXED per `feedback_zone_numbering.md`); parameter name validation per existing IEffect parameter contract

### Test impact
- **Spike 2** (gating Phase 1 build): same effectId in 2 zones, change Zone 1 parameter, verify Zone 2 unchanged
- **Cross-transport equivalence (Spike 4 / E5)**: same payload via REST/WS/SerialJSON → identical final state
- **Regression**: global `effects.parameters.set` still works (single-effect mode)
- **Continuous gate (D-8)**: zones-disabled → single-effect parameters work

---

## D-4 — Audio routing semantics

### Decision (recommended default)
**Hint + envelope combo.** First cut supports four modes: `FULL_MIX`, `BASS`, `MID`, `HIGH`. Custom weighting deferred. Each zone's `ZoneAudioConfig.routingMode` drives:

1. **Primary-band hint** — `EffectContext.audio.primaryBand` set per zone for effects that opt in
2. **Derived energy envelope** — `EffectContext.audio.zoneEnergy` (float) computed by ZoneComposer per-frame from the selected bands of ControlBus, populated for all effects
3. **Full ControlBus available** — effects that want spectral data still see `ctx.audio.controlBus` unchanged

Effects choose: simple effects use `zoneEnergy` as drive; effects with their own band logic check `primaryBand` and self-select; effects that don't care ignore both and read full ControlBus.

### Why
Per the 5-SSA context report and verification pass V3 (in flight): `m_zoneAudioConfigs[]` is set/stored/callback-fired in ZoneComposer but the render loop (`renderZone()`) never reads it. Audio routing is dead-coded.

Captain's "DJ mixer" mental model needs per-zone audio routing — Zone 1 reacts to bass, Zone 2 to mids, Zone 3 to treble. Wiring the existing struct is medium effort; the keystone unlocks the bass/body/air demo path.

### Alternatives rejected
- **Filtered ControlBus** (zero out non-selected bands). Strongest separation but breaks effects that derive features from cross-band relationships (e.g., chord detection, spectral flux, bass-vs-treble ratios). Loses information.
- **Effect-side pure** (effects implement their own band selection). Defeats the purpose; user can't say "make this zone bass" without effect cooperation in every effect.
- **Custom band weighting from the outset** (`bandWeights` array per zone). Adds API surface and UI complexity before core behaviour is proven. Defer to Phase 4+ if validated.

### Reversibility
**Medium-high.** Adding new modes is additive. Removing existing modes after they're documented breaks contract.

### Gating
**Blocked by:** D-1 (instance pool — different audio routing per zone implies different per-zone state, which requires isolation)
**Blocks:** Bass/body/air demo path (D3); Phase 1 hardware validation

### Implementation impact
- Wire `m_zoneAudioConfigs[i]` reads into `renderZone()` (currently dead-coded)
- Compute `zoneEnergy` per zone per frame: weighted sum of selected `ControlBus.bands[]` per `routingMode`
  - `FULL_MIX`: `rms` (existing)
  - `BASS`: weighted sum of bands[0..1]
  - `MID`: weighted sum of bands[2..4]
  - `HIGH`: weighted sum of bands[5..7]
- Set `ctx.audio.primaryBand` to the routing mode's representative band
- Cost: ~1 µs per zone per frame (negligible against 2.0 ms budget)

### Test impact
- **Spike 3** (gating Phase 1 build): same effect in 3 zones with bass/mid/high routing, controlled audio fixture → 3 visibly different outputs
- **Backward compat**: zones with `FULL_MIX` mode (default) → unchanged behaviour
- **Continuous gate (D-8)**: zones-disabled → single-effect audio path unchanged

---

## D-5 — Demo audio fixture sourcing

### Decision (recommended default)
**Default to existing approved benchmark corpus** at `/Users/spectrasynq/Workspace_Management/Software/hybrid-beat-tracker/tests/benchmark` (CLAUDE.md-approved standing source).

If that corpus does not cover bass-heavy / mid-heavy / transient-heavy / sparse / dense scenarios sufficiently, **request Captain pre-approval for synthetic fixtures** (sine sub, mid-band noise, transient clicks, full-mix synthetic loop). Synthetic fixtures generated locally are safer (legally clean, reproducible) but still count as new audio per CLAUDE.md hard rule and require explicit Captain authorisation per source.

### Why
CLAUDE.md hard rule: "Approval for one audio file does not authorise other files, synthetic fixtures, white/pink noise, hats, cymbals, speech, generated tones, or any agent-chosen sound. For AFS/runtime audio capture, the approved reference corpus is `/Users/spectrasynq/.../tests/benchmark` unless Captain explicitly names a different source."

Demo validation cannot proceed on agent-sourced audio. Captain must approve sources upfront.

### Alternatives rejected
- **Free-form audio sourcing.** Violates CLAUDE.md hard constraint. NO.
- **Skip controlled fixtures, validate by vibes.** Captain's stated concern: "agents will claim success based on vibes." Subjective validation is unreliable for audio-routing correctness. NO.

### Reversibility
**Trivial.** Audio fixtures are test data; can be replaced.

### Gating
**Blocked by:** Captain approval of fixture sources
**Blocks:** D2 demo validation harness; Spike 3 audio routing visibility

### Implementation impact
- Inventory benchmark corpus first — what bass / mid / treble / sparse / dense coverage already exists
- If gaps exist: prepare a synthetic fixture proposal with file list, generation method, intended use → Captain approval before generation
- Document approved fixtures in `firmware-v3/docs/research/zone-composer/demo-fixtures.md` with provenance per file

### Test impact
- All Phase 1 hardware validation (Spike 3, D3 demo path) consumes only fixtures from this approved set
- Continuous: any new test that requires audio cites its fixture by file path

---

## Sign-off

Captain may approve all defaults with one statement (e.g. "ADR defaults approved") or override per-decision with rationale.

**Phase 0 no-regret cleanup proceeds in parallel without sign-off** (BlendMode unification, stub disposition, command matrix, SerialJSON inventory, regression gate, equivalence test skeleton — none depend on the architectural choices in this ADR).

**Phase 1 implementation is gated on:**
1. ADR sign-off (or per-decision overrides)
2. Targeted verification pass results (in flight; ground-truths claims V1-V6)
3. Spikes 1, 2, 3 succeed on hardware

**Hard rule (per Captain):** No new UI, hardware, or marketing surface exposes a capability until the engine state, transport command, readback state, and regression test all agree.

---

## Appendix A — Verification Pass Findings (V1-V6)

Targeted verification pass completed 2026-05-01 against branch `firmware/heap-shed-fragmentation-fix` (HEAD ~9d7bc261). Each claim ground-truthed with verbatim code citations. Findings:

### V1 — Effect instance ownership: SINGLETON CONFIRMED

**Source:** `firmware-v3/src/core/actors/RendererActor.cpp:440-447`

```cpp
plugins::IEffect* RendererActor::getEffectInstance(EffectId id) const
{
    const auto* reg = findById(id);
    if (reg) {
        return reg->effect;
    }
    return nullptr;
}
```

ZoneComposer usage at `firmware-v3/src/effects/zones/ZoneComposer.cpp:297` calls the same function per zone. Same `effectId` → same pointer. **D-1 instance pool decision validated.**

### V2 — Parameter mutation path: SINGLETON-AFFECTING CONFIRMED

**Source:** `firmware-v3/src/core/actors/RendererActor.cpp:1198-1233`

The lock-free queue `enqueueEffectParameterUpdate(effectId, name, value)` and its drain function `applyPendingEffectParameterUpdates()` operate on `reg->effect->setParameter(...)` — the SINGLETON instance.

**Implication:** changing parameter via the global path mutates the singleton, immediately affecting ALL zones rendering that effectId. **D-3 gating behind D-1 validated.**

### V3 — Audio context routing: DEAD-CODED CONFIRMED

**Source:** `firmware-v3/src/effects/zones/ZoneComposer.cpp:272-406` (renderZone body)

`m_zoneAudioConfigs[]` is:
- Declared at `ZoneComposer.h:231`
- Written via setter at `ZoneComposer.cpp:663-674`
- Read ONLY via getter at `ZoneComposer.cpp:656-661`
- Zero reads inside `renderZone()`

Render uses hardcoded fallback values (`mood = 128`, `fadeAmount = 128` at lines 315-316). Audio config storage is a complete stub. **D-4 wiring needed; default validated.**

### V4 — BlendMode enum collisions: 2 DECLARATIONS, DIFFERENT DOMAINS

**Sources:**
- `firmware-v3/src/effects/gradient/GradientTypes.h:67` — 4 modes (REPLACE, ADD, SCREEN, MULTIPLY)
- `firmware-v3/src/effects/zones/BlendMode.h:20` — 8 modes + MODE_COUNT (OVERWRITE..DARKEN)

Different namespaces, different enumerator names, no actual collision. **B7 action revised:** rename `effects::gradient::BlendMode` → `effects::gradient::GradientBlendMode` to eliminate name-namespace ambiguity. Phase 0 B7 implementation: rename + add canonical-naming policy comment to `zones/BlendMode.h` + scaffolded compile-time regression test at `firmware-v3/test/test_blend_mode_namespace/test_main.cpp`.

### V5 — Stubbed REST endpoints: 10 CONFIRMED

All 10 endpoints below verified to return hardcoded "not fully implemented" success-shaped responses (NO functional logic). All routes located in `firmware-v3/src/network/webserver/handlers/ZoneHandlers.cpp`:

| Endpoint | Line range | Status (Phase 0 B1) |
|---|---|---|
| GET `/api/v1/zones/config` | 425-437 | 🚫 Now returns 501 (Phase 3 A6) |
| POST `/api/v1/zones/config/save` | 439-451 | 🚫 Now returns 501 (Phase 3 A6) |
| POST `/api/v1/zones/config/load` | 453-466 | 🚫 Now returns 501 (Phase 3 A6) |
| GET `/api/v1/zones/timing` | 472-484 | 🚫 Now returns 501 (Phase 8 A12) |
| POST `/api/v1/zones/timing/reset` | 486-497 | 🚫 Now returns 501 (Phase 8 A12) |
| GET `/api/v1/zones/{id}/audio` | 503-520 | 🚫 Now returns 501 (Phase 1 D-4/A3) |
| POST `/api/v1/zones/{id}/audio` | 522-543 | 🚫 Now returns 501 (Phase 1 D-4/A3) |
| GET `/api/v1/zones/{id}/beat-trigger` | 549-566 | 🚫 Now returns 501 (Phase 4) |
| POST `/api/v1/zones/{id}/beat-trigger` | 568-589 | 🚫 Now returns 501 (Phase 4) |
| POST `/api/v1/zones/reorder` | 595-610 | 🚫 Now returns 501 (Phase 6 A9) |

Phase 0 B1 cleaned all 10 from fake-success placeholders to explicit `HttpStatus::NOT_IMPLEMENTED` (501) following the proven pattern at `AudioHandlers.cpp:655`.

### V6 — SerialJSON coverage gaps: CONFIRMED

**Source:** `firmware-v3/src/serial/SerialJsonGateway.cpp` zone command handlers (lines 320, 605, 615, 632, 648, 664, 680, 701, 738).

8 zone commands present. Missing: `setLayout`, audio config getter/setter, `getZoneConfig`, per-zone `setEnabled` (only global `zone.enable` exposed). Phase 0 B5 documented these as parity inventory ordered by leverage in `docs/protocol/zones-serial-json-parity.md`.

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-05-01 | Claude (claude-opus-4-7) | Created. Drafted from Captain's program plan review + 5-SSA context report. D-3 explicitly gated behind D-1 per Captain correction. Recommended defaults marked, alternatives rejected with reasoning. Verification pass dispatched in parallel to ground-truth claims V1-V6. |
| 2026-05-01 | Claude (Captain sign-off) | Status DRAFT → APPROVED per Captain directive "ADR defaults approved for D-1 through D-5". Verification pass complete; V1-V6 findings recorded in Appendix A. D-1 → D-3 → D-4 → D-2 sequencing preserved. D-5 fixture constraint preserved. |
