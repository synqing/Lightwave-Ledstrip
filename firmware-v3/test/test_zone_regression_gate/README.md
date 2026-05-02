---
abstract: "D-8 single-effect regression gate skeleton. Documents the four invariants every Zone Composer-touching PR must satisfy. Phase 0 deliverable — scaffolding ready, full implementation lands as Phase 1 commands and effects integrate. Runs against K1 V2 hardware via Python smoke test."
---

# Zone Composer Single-Effect Regression Gate (D-8)

**Status:** Phase 0 skeleton. Test invariants documented; Python smoke test scaffolded at `firmware-v3/scripts/zone_regression_smoke.py`. Native unit-test variant lands in Phase 1 when ZoneComposer mocks are mature enough.

**Purpose.** ZoneComposer mutually excludes the single-effect render path: when zones are enabled, `RendererActor::renderFrame()` returns early at line 1693 and bypasses single-effect rendering entirely. Every zone-related change risks breaking the non-zone case if state transitions are mishandled. Captain's Hard Rule: **no PR ships if zones-disabled mode no longer works.**

## The four invariants

Every PR that touches `ZoneComposer.cpp/.h`, `RendererActor.cpp/.h`, `BlendMode.h`, any zone handler, or any effect that participates in zone rendering MUST verify these four invariants on K1 V2 hardware before merge:

### Invariant I1 — ZoneComposer disabled → single-effect mode works

```
1. Send: zone.enable {enable: false}
2. Send: parameters.set {effectId: 0x1301, brightness: 128}  (or any single effect)
3. Verify: effect renders correctly at 120 FPS
4. Verify: stats.currentFPS >= 110 sustained for 10 seconds
5. Verify: no panic, no watchdog reset
```

### Invariant I2 — ZoneComposer enabled → zone render path works

```
1. Send: zone.enable {enable: true}
2. Apply factory preset 1 (Dual Split)
3. Verify: zones 0 and 1 render with their respective effects
4. Verify: stats.currentFPS >= 110 sustained for 10 seconds
5. Verify: no panic, no buffer corruption (LED output sane)
```

### Invariant I3 — Toggle enabled/disabled repeatedly → no stale state

```
1. Send 50 toggles: enable, disable, enable, disable, ...
2. After each toggle, verify the appropriate render path is active:
   - enabled → zone path (Zone 1 effect renders)
   - disabled → single-effect path (last single-effect renders)
3. Verify: no crash, no stale buffer (e.g. zone state shouldn't bleed into single-effect render)
4. Verify: heap stable — final freeHeap >= initial freeHeap minus 4 KB tolerance
5. Verify: no RMT errors, no spinlock asserts
```

### Invariant I4 — Global effects.parameters.set still works in single-effect mode

```
1. Send: zone.enable {enable: false}
2. Send: parameters.set {effectId: 0x1301, brightness: 200}
3. Send: effects.parameters.set {effectId: 0x1301, parameters: {...}} (any declared param)
4. Verify: parameter applied (read back via parameters.get or check visual)
5. Verify: no panic, FPS stable
```

## How to run

### Hardware smoke test (canonical, runs every PR)

```bash
# 1. Flash the canonical K1 V2 env
cd firmware-v3
pio run -e esp32dev_audio_esv11_k1v2_32khz -t upload --upload-port /dev/cu.usbmodem2101

# 2. Run the regression smoke test
python3 scripts/zone_regression_smoke.py --port /dev/cu.usbmodem2101 --invariants all
```

**Exit code 0** means all four invariants pass. **Exit code 1** means one or more failed — DO NOT MERGE.

### Native unit test (Phase 1 — when mocks are ready)

```bash
pio test -e native_test_zone_regression_gate
```

**Currently scaffolded but not implemented.** The `test_main.cpp` exists as a placeholder. Full implementation requires:

- Mock RendererActor (intercepts `renderFrame()` calls and tracks which path was taken)
- Mock FastLED (`leds[]` buffer accessible for buffer-corruption checks)
- Mock ControlBus (canned audio frames for repeatable behaviour)

These mocks are partial today (`firmware-v3/test/test_native/mocks/fastled_mock.h` exists). Phase 1 work expands them to cover the regression-gate cases.

## When the gate fails

If the smoke test fails, the PR is BLOCKED until the failure is understood and fixed.

Common failure modes:
- **I1 fails (single-effect broken after zone work):** likely a state pollution bug in RendererActor's mutual-exclusion check. Check whether ZoneComposer left buffers dirty when disabled.
- **I2 fails (zone path broken):** likely a regression in `renderZone()` or buffer arithmetic. Check `m_zoneBuffers` initialisation in `init()`.
- **I3 fails (toggle instability):** likely a synchronisation bug between Core 0 (zone state writes) and Core 1 (render). Check `m_enabled` atomic ordering.
- **I4 fails (effects.parameters.set broken in single-effect mode):** verify the global path (RendererActor's `applyPendingEffectParameterUpdates()`) is unaffected by Zone Composer changes.

## Scope expansion (Phase 1+)

When zone-aware `effects.parameters.set` ships (Phase 1 keystone D-3), add:

### Invariant I5 — Zone-aware parameters do not corrupt global parameters

```
1. zone.enable true
2. zone.effects.parameters.set zoneId=1 {param: A}
3. zone.enable false
4. effects.parameters.set {param: B}  (global)
5. Verify: global effect uses param B (zone 1's param A did not leak through singleton)
```

This invariant explicitly tests that the D-1 instance pool isolation is preserved across the zone↔global boundary.

## Cross-references

- ADR D-1, D-3, D-8: [`../../docs/adr/zone-composer-architecture-decisions.md`](../../../docs/adr/zone-composer-architecture-decisions.md)
- Command matrix: [`../../docs/protocol/zones-command-matrix.md`](../../../docs/protocol/zones-command-matrix.md)
- Smoke test script: [`../scripts/zone_regression_smoke.py`](../../scripts/zone_regression_smoke.py)
- Hard Rule: no UI/hardware/marketing surface exposes capability until engine state, transport command, readback state, and regression test all agree.

---
**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-05-01 | Claude (Phase 0 D-8) | Created. Four invariants documented; Python smoke test scaffolded; native test deferred to Phase 1 when mocks mature. |
