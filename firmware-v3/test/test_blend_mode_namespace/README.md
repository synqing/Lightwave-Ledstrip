---
abstract: "Compile-time regression test for B7 BlendMode namespace disambiguation. Asserts zones::BlendMode and effects::gradient::GradientBlendMode remain distinct types after the 2026-05-01 rename. Test scaffolded but not yet wired into platformio.ini — one-line addition needed to activate."
---

# test_blend_mode_namespace

Compile-time regression test for the B7 disambiguation work performed during
Phase 0 of the Zone Composer Instrument Program (2026-05-01).

## What this guards

Before 2026-05-01, two enum types named `BlendMode` existed in the firmware:

- `lightwaveos::zones::BlendMode` — 8 modes (OVERWRITE/ADDITIVE/.../DARKEN), used
  by ZoneComposer, REST handlers, WS commands, Serial JSON gateway.
- `lightwaveos::effects::gradient::BlendMode` — 4 modes (REPLACE/ADD/SCREEN/MULTIPLY),
  used internally by the gradient rendering kernel.

The gradient enum was renamed to `GradientBlendMode` to eliminate name-namespace
ambiguity. This test asserts the rename is preserved.

## What fails the test

- Re-introducing `BlendMode` as the gradient enum's name (compile-time fail via static_assert)
- Aliasing the two types via `using` declaration (compile-time fail)
- Renaming the canonical `zones::BlendMode` (compile-time fail; the test references the canonical enumerators)

## Wiring into platformio.ini (TODO)

This test is scaffolded but not yet active. Add to `firmware-v3/platformio.ini`:

```ini
[env:native_test_blend_mode_namespace]
platform = native
test_framework = unity
build_flags =
    -std=c++17
    -DNATIVE_BUILD=1
test_filter = test_blend_mode_namespace
```

Then run:

```bash
cd firmware-v3
pio test -e native_test_blend_mode_namespace
```

Expected output: 2 unit tests pass + zero compile warnings (compile-time
asserts pass silently).

## Why a separate test env

This test specifically requires both `zones/BlendMode.h` AND
`gradient/GradientTypes.h` to be in the same translation unit. Adding the
include to an existing test env risks breaking that env's build flags or
mocks. A standalone env keeps the regression check isolated.

## Related work

- Companion ADR: `docs/adr/zone-composer-architecture-decisions.md`
- Verification pass V4 confirmed only two declarations existed (zones + gradient,
  different domains) — rename, not unify.

---
**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-05-01 | Claude (Phase 0 B7) | Created. Test scaffolded; PIO wiring deferred to follow-up Phase 0 close-out. |
