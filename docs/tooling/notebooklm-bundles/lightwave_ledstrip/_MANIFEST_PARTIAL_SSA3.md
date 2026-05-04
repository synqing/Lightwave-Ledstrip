# SSA3 Partial Manifest — firmware-v3 SOURCE bundles

Generated: 2026-05-04 (lightwave-ledstrip notebooklm curation)
Working dir: `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip`
Output dir:  `docs/tooling/notebooklm-bundles/lightwave_ledstrip/`

## Bundle 1 — `_BUNDLE_firmware_contracts.txt`

Path on disk: `docs/tooling/notebooklm-bundles/lightwave_ledstrip/_BUNDLE_firmware_contracts.txt`
Size: 153,107 bytes
Files packed: 14

| # | Relative path | Source size |
|---|---------------|-------------|
| 1 | `firmware-v3/src/audio/contracts/ControlBus.h` | ~30.0 KB |
| 2 | `firmware-v3/src/plugins/api/EffectContext.h` | ~48.5 KB |
| 3 | `firmware-v3/src/plugins/api/IEffect.h` | ~10.5 KB |
| 4 | `firmware-v3/src/plugins/api/IEffectRegistry.h` | ~1.5 KB |
| 5 | `firmware-v3/src/plugins/api/OnsetContext.h` | ~0.8 KB |
| 6 | `firmware-v3/src/audio/contracts/MotionSemantics.h` | ~11.5 KB |
| 7 | `firmware-v3/src/audio/contracts/OnsetSemantics.h` | ~1.1 KB |
| 8 | `firmware-v3/src/audio/contracts/MusicalSaliency.h` | ~7.1 KB |
| 9 | `firmware-v3/src/audio/contracts/MusicalGrid.h` | ~5.6 KB |
| 10 | `firmware-v3/src/audio/contracts/AudioTime.h` | ~1.1 KB |
| 11 | `firmware-v3/src/audio/contracts/SnapshotBuffer.h` | ~3.9 KB |
| 12 | `firmware-v3/src/audio/contracts/StyleDetector.h` | ~1.8 KB |
| 13 | `firmware-v3/src/audio/contracts/MotionShaper.h` | ~4.2 KB |
| 14 | `firmware-v3/src/audio/contracts/AudioEffectMapping.h` | ~15.9 KB |

### Files MISSING / flagged for Bundle 1
- `firmware-v3/src/plugins/api/RenderContext.h` — **DOES NOT EXIST** as a separate file in this codebase. The render context surface is in fact `EffectContext` and lives entirely inside `firmware-v3/src/plugins/api/EffectContext.h` (which is included in this bundle). The curation prompt acknowledged this and instructed me to flag it here. There is a `struct RenderContext` declared inside `firmware-v3/src/core/actors/RendererActor.h` (legacy effect-render-fn signature; used only by the old `EffectRenderFn` typedef) — this is a separate, internal renderer-side struct, not the plugin contract; that file is in Bundle 2.
- The curation prompt also incorrectly named the effect interface `EffectBase.h`. The real file is `IEffect.h` (now included). No separate `EffectBase.h` exists.

## Bundle 2 — `_BUNDLE_firmware_actors.txt`

Path on disk: `docs/tooling/notebooklm-bundles/lightwave_ledstrip/_BUNDLE_firmware_actors.txt`
Size: 99,994 bytes
Files packed: 5

| # | Relative path | Source size |
|---|---------------|-------------|
| 1 | `firmware-v3/src/audio/AudioActor.h` | ~41.9 KB |
| 2 | `firmware-v3/src/core/actors/RendererActor.h` | ~37.5 KB |
| 3 | `firmware-v3/src/core/actors/ShowDirectorActor.h` | ~5.8 KB |
| 4 | `firmware-v3/src/plugins/PluginManagerActor.h` | ~7.3 KB |
| 5 | `firmware-v3/src/network/webserver/WsCommandRouter.h` | ~2.6 KB |

### Files MISSING / flagged for Bundle 2
- `firmware-v3/src/commands/CommandActor.h` (path supplied in prompt) — **DOES NOT EXIST**. Verified via `find firmware-v3/src -name "*Actor*.h"`: the only actor headers in the tree are `audio/AudioActor.h`, `core/actors/{Actor,ActorSystem,RendererActor,ShowDirectorActor}.h`, `hal/display/DisplayActor.h`, `plugins/PluginManagerActor.h`, `sync/SyncManagerActor.h`, and `codec/RendererActorStub.h`. There is no class named `CommandActor` anywhere in the source tree.
- **Closest equivalent included:** `firmware-v3/src/network/webserver/WsCommandRouter.h` — the WebSocket-side command dispatcher (table-driven routing of all WebSocket commands to per-domain handlers in `network/webserver/ws/*Commands.h`). Sibling files `firmware-v3/src/sync/CommandType.h` and `firmware-v3/src/sync/CommandSerializer.h` exist but are CQRS message-typing helpers used inside SyncManagerActor, not the routing entry point.
- Prompt's other "stale paths" — confirmed corrected and resolved:
  - `firmware-v3/src/rendering/RendererActor.h` → real path `firmware-v3/src/core/actors/RendererActor.h` (included).
  - `firmware-v3/src/show/ShowDirectorActor.h` → real path `firmware-v3/src/core/actors/ShowDirectorActor.h` (included).
  - `firmware-v3/src/plugins/PluginManagerActor.h` — real path matches prompt; included.

## STA-mode scan across all bundled headers

Command run:
```
/usr/bin/grep -EHn "STA|wifi_sta|WIFI_MODE_STA" <all 19 bundled header paths>
```

Hits found, but **all are false positives** (substring matches in unrelated identifiers — none are WiFi-STA references):
- `MusicalSaliency.h:70` — comment text "Temporal class: SUSTAINED (300ms-2s)" (matches "STA" inside "SUSTAINED").
- `AudioActor.h:1209` — `AUDIO_ACTOR_STACK_WORDS` (matches "STA" inside "STACK").
- `RendererActor.h:715` — `START_TRANSITION` message-name comment (matches "STA" inside "START").
- `ShowDirectorActor.h:15` — `SHOW_LOAD, SHOW_START, SHOW_STOP, SHOW_PAUSE, ...` comment.
- `ShowDirectorActor.h:18` — `SHOW_STARTED, SHOW_STOPPED, ...` event names comment.

**Zero `WIFI_MODE_STA`, `wifi_sta`, or standalone STA-mode references in any bundled header.** Headers are clean per the K1-AP-only invariant. (Network-subsystem AP-only doctrine is enforced separately in `firmware-v3/src/network/CLAUDE.md` and `WiFiManager.h` — not in scope of these bundles.)

## Confidence

**HIGH.**

Reasoning:
- All 19 advertised file paths verified by `ls`/`find` before reading; all sizes match the files actually packed.
- `RenderContext.h` and `CommandActor.h` non-existence verified by exhaustive `find firmware-v3/src -name ... -name "*.h"` — not partial; the entire `src/` tree was searched.
- Bundle headers are written verbatim, full-content, with the exact separator format mandated by the prompt.
- STA scan was run literally over each packed file path, not against a glob — every hit was inspected and classified.

## Unresolved flags / notes for orchestrator

1. The `RenderContext` struct in `RendererActor.h:173` is the LEGACY effect render function signature (`EffectRenderFn = void (*)(RenderContext& ctx)`). Modern effects do not use it — they use the namespaced `lightwaveos::plugins::EffectContext` from `EffectContext.h`. NotebookLM may surface both; the orchestrator should be aware that `RenderContext` (renderer-internal) and `EffectContext` (plugin contract) are different things despite the similar names.
2. `EffectContext.h` references several headers that are NOT in this bundle (e.g., `BehaviorSelection.h`, `TranslationEngine.h`, `effect_ids.h`, `features.h`). Those declarations will appear as unresolved symbols inside the bundle. This is intentional — the bundles cover only the contract + actor surface as scoped.
3. `MotionSemantics.h` includes `effects/enhancement/SmoothingEngine.h` and `MotionShaper.h` includes `effects/enhancement/TemporalOperator.h`; neither is bundled. NotebookLM will see calls to `AsymmetricFollower::update()` / `TemporalEnvelope::eval()` etc. without the implementation.
4. The two K1 audio backend selectors (`FEATURE_AUDIO_BACKEND_ESV11`, `FEATURE_AUDIO_BACKEND_PIPELINECORE`) gate massive blocks of `AudioActor.h`. The bundle includes ALL three branches verbatim per the prompt's "include in full" directive — NotebookLM should be told that K1 production builds only the ESV11 branch (per `CLAUDE.md`, the `_32khz` envs are canonical).

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-04 | agent:claude-opus-4-7-1m (SSA3) | Created — partial manifest for firmware-v3 contracts + actor bundles. Confirms 19 files packed, 2 missing files (RenderContext.h, CommandActor.h) flagged, STA scan clean, confidence HIGH. |
