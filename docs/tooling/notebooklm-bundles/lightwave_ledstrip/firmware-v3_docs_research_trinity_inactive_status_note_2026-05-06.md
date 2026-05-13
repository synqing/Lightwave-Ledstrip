# Trinity Inactive Status Note

**Date:** 2026-05-06
**RBDO label:** GROUNDED
**Scope:** Source-truth note only. No code changes.

## Captain Clarification

Captain clarified on 2026-05-06:

> Trinity is not and had never actually been actively deployed or utilised at all.

This should be treated as product reality. Future work must not infer active deployment from the existence of firmware hooks, command handlers, or diagnostic artefacts.

## Source Cross-Check

| Source | Finding |
|---|---|
| `docs/K1_ECOSYSTEM_API_ROADMAP.md:22-34` | Trinity is listed under implemented-but-never-wired and "completely unused" API domains. |
| `docs/K1_ECOSYSTEM_API_ROADMAP.md:88-94` | Trinity is described as cross-strip synchronisation with "no consumer". |
| `docs/K1_ECOSYSTEM_API_ROADMAP.md:183-188` | Trinity is a write-only domain: SET commands only, no GET/LIST, no events. |
| `firmware-v3/src/effects/CoreEffects.cpp:870-874` | `TrinityTestEffect` registration is commented out and marked removed/dead/broken. |
| `firmware-v3/src/effects/PatternRegistry.cpp:156-157` | Stale metadata still maps old effect slot 99 to `EID_TRINITY_TEST`, so catalogue surfaces can mention it even though `CoreEffects` does not register it. |
| `firmware-v3/src/config/features.h:60-63` | `FEATURE_AUDIO_SYNC` still defaults on and is labelled "Audio Reactive Effects + Trinity Protocol". This is a compile-time hook, not deployment evidence. |
| `firmware-v3/src/network/WebServer.cpp:1342-1344` and `firmware-v3/src/network/webserver/ws/WsTrinityCommands.cpp:243-248` | WS Trinity command handlers are registered when `FEATURE_AUDIO_SYNC` is enabled. Registration means the hook exists; it does not prove a client or production workflow uses it. |

## Operating Rule

Use "legacy inactive Trinity compatibility" or "dormant Trinity hooks" when describing current code.

Avoid:

- "active Trinity pipeline"
- "deployed Trinity sync"
- "Trinity consumer"
- "production Trinity mode"

When a refactor touches `TrinityControlBusProxy`, `trinity.*` WS commands, `AudioInputMode::Trinity`, or stimulus mode `trinity`, treat it as compatibility cleanup for dormant hooks unless Captain explicitly revives Trinity as a product workflow.
