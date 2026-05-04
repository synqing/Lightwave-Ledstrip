# Native Harness Hardening Execution Plan

**State:** GROUNDED

## Summary

Fix the root cause by retiring the ambiguous `native_test` catch-all path and replacing it with explicit, passing native harness routes. Current evidence: `[env:native_test]` mixed every `test/test_native/*` file with only `ManifestCodec.cpp`, so codec/effect/router tests linked without their provider objects. CI/docs also contained stale `native_test` commands, and the router/native codec focused envs had separate drift to repair.

No firmware runtime behaviour, LED rendering, WiFi mode, protocol wire shape, or hardware path changes.

## Implementation Changes

- Save this plan so the final commit can cite it per repo governance.
- Replace the stale `[env:native_test]` route with explicit scoped native routes so it cannot silently be treated as a valid full-suite signal again.
- Add `firmware-v3/scripts/native_harness_matrix.py` as the canonical host validation entrypoint.
- Add `firmware-v3/scripts/check_native_harness_routes.py` and wire it into CI so new stale broad-native commands fail review.
- Fix scoped harness drift in the native webserver mock and WebSocket codec tests.
- Update stale routing in CI, docs, test comments, and PlatformIO comments to point at scoped envs or the matrix script.

## Test Plan

- Run `cd firmware-v3 && python3 scripts/check_native_harness_routes.py`.
- Run `cd firmware-v3 && python3 scripts/native_harness_matrix.py`.
- Run `cd firmware-v3 && pio run -e esp32dev_audio_esv11_k1v2_32khz`.
- Run `git diff --check`.
- Confirm `git status --short` contains only intentional harness/config/docs/changelog files before commit.

## Changelog And Commit

- Add a `CHANGELOG.md` entry under `## [Unreleased]` / `### Fixed`.
- Add `instructions/changelog/2026-05-05--firmware-v3--native-harness-hardening.md`.
- Commit with DCO using subject `fix(firmware-v3): harden native test harness routing`.
- Commit body includes `Refs: plan docs/superpowers/plans/2026-05-05-native-harness-hardening.md`.

## Assumptions

- The correct fix is to remove/deprecate ambiguous `native_test`, not expand it into a giant aggregate.
- Hardware validation is not required because this changes only host-side harness configuration, mocks, CI routing, docs, and tests.
