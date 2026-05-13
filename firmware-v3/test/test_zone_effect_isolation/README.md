---
abstract: "Native PlatformIO test pinning the per-zone IEffect instance isolation invariant for D-1 Spike 1. Asserts ZoneEffectPool returns distinct instances for the same effectId across zones, falls back safely to the registry singleton when no factory is registered, and frees only pool-owned instances on clear()."
---

# test_zone_effect_isolation — D-1 Spike 1 regression suite

Native unit tests for `lightwaveos::zones::ZoneEffectPool`, the per-zone effect instance pool that gates the D-1 keystone work (and unblocks D-3, `zone.effects.parameters.set`).

## Run

```
cd firmware-v3
pio test -e native_test_zone_effect_isolation
```

## What this guards

The pool MUST satisfy these invariants — without them, the K1 Bloom + K1 Bloom (same effect, two zones) configuration silently corrupts internal effect state by sharing the registry singleton across zones.

1. **Per-zone isolation** — same `effectId` in two zones returns two distinct `IEffect*` pointers when a factory is registered.
2. **Idempotent re-acquire** — repeated calls for the same `(effectId, zoneSlot)` return the cached instance and do not re-query the factory.
3. **Init-once-per-pair** — driven by `ZoneComposer::acquireZoneEffect()`, `IEffect::init()` runs exactly once per `(effectId, zoneSlot)` on first acquire.
4. **Singleton fallback (degraded mode)** — when no factory is registered, the pool falls back to the registry singleton. For multi-zone use of the same effectId this preserves the pre-D-1 known-bad behaviour. Documented in ADR D-1; phase-2 work registers factories for the affected effects to fix.
5. **Clear semantics** — `clear()` calls `cleanup()` then `delete` on pool-owned instances; singleton fallbacks are NOT freed (they're owned by the registry).
6. **Defensive rejection** — invalid zone slots and `INVALID_EFFECT_ID` produce `nullptr` without mutating the pool.
7. **Peek non-mutating** — `peek()` returns `nullptr` for unknown pairs and does not create slots.

## Implementation notes

- The test uses a minimal mock `IEffect` and `IZoneEffectSource` — it does NOT pull in `EffectContext`, FastLED, or FreeRTOS, so it runs cleanly on the host under `platform = native`.
- The pool's `acquire()` is the test target. `init()` is driven manually in the test to mirror what `ZoneComposer::acquireZoneEffect()` does in firmware (the pool itself stays generic and does not call `init()`).
- The `MockEffect` carries a unique sequential `uniqueId()` proxy for "internal state" — a per-zone counter would be corrupted under the pre-D-1 singleton-share, so distinct ids across zones is a faithful witness for the isolation invariant.

## Spike 1 status

The pool implementation is feature-gated in firmware behind `K1_ZONE_INSTANCE_POOL_ENABLED` (default `0`). The live render path stays on the partial-fix code path until hardware validation of the prototype passes. These tests run unconditionally — they pin the pool's host-side contract whether the firmware gate is on or off.

## Refs

- `firmware-v3/src/effects/zones/ZoneEffectPool.{h,cpp}` — implementation
- `firmware-v3/src/effects/zones/ZoneComposer.cpp` — consumer (gated)
- `docs/adr/zone-composer-architecture-decisions.md` § D-1 — design

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-02 | agent:visual-fx-architect | Created — D-1 Spike 1 isolation tests, 7 cases, all passing under `native_test_zone_effect_isolation`. |
