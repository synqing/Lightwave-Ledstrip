# Staging and Commit Strategy — feature/tab5-ios-effectid-alignment

This document provides a comprehensive staging and commit strategy for all uncommitted changes in the working directory, with change analysis, atomic commit grouping, conventional commit messages, context documentation, lessons learned, and future contributor guidance.

---

## 1. Change Analysis

### 1.1 Summary

| Category | File count | Description |
|----------|------------|-------------|
| **New features** | ~50+ | Control lease, external render stream, effect tunables, Showpiece Pack 3 effects, K1 composer dashboard |
| **Refactors** | ~45 | IEffect parameter API, all LGP/reference effects extended with descriptors and setParameter; codec/WS payloads |
| **Bug fixes / alignment** | ~15 | Tab5 encoder effect range 146→166; WS single-session guard relaxed for observe-mode; OLD_TO_NEW extended |
| **Documentation** | ~15 | API v1/v2, control-lease-v1, render-stream-v1, agent/architecture docs |
| **Configuration** | ~8 | features.h, platformio.ini, pre-commit, CI workflows |
| **Tests** | 6 | Codec tests updated for new effect IDs and parameter/tunable payloads |
| **Chore (deletions)** | 36 | .claude/skills removed (Changelog, Content Research, brand-guidelines, d3js, finishing-branch, internal-comms, supabase, theme-factory, toon-formatter) |

**Totals (from `git diff --stat`)**: 131 modified/deleted files, ~4,569 insertions, ~8,995 deletions (net reduction dominated by removed skills and binary).

### 1.2 Categorisation by Type

#### New features
- **Control lease**: `ControlLeaseManager`, `ControlHandlers`, `WsControlCommands`; gates in main (serial JSON/text, encoder), WebServer (mutating HTTP), WsGateway (mutating WS); lease release on disconnect; CORS headers extended.
- **External render stream**: `RendererActor` external mailbox, `startExternalRender`/`stopExternalRender`/`ingestExternalFrame`/`getExternalRenderStats`; `WsStreamCommands` (`render.stream.*`); WsGateway binary frame handling and render-stream client disconnect; `FEATURE_EXTERNAL_RENDER_STREAM`.
- **Effect tunables**: `EffectTunableStore` (NVS/volatile, debounce); `RendererActor` calls `onParameterApplied`/`onEffectActivated`; HTTP/WS codecs encode persistence and extended parameter fields; `generate_manifest.py` + `tunable_manifest.json`; pre-commit and CI manifest validation.
- **Showpiece Pack 3 effects**: New effect IDs 0x1B00–0x1B08; new implementations: TimeReversalMirror (base + AR + Mod1/2/3), KdVSolitonPair, GoldCodeSpeckle, QuasicrystalLattice, FresnelCausticSweep; registered in CoreEffects; display_order and OLD_TO_NEW extended.
- **K1 composer dashboard**: `tools/k1-composer-dashboard/` (HTML/JS, diagnostic, effect catalog, scripts); `docs/dashboard/` (wireframe, telemetry schema, implementation path).

#### Refactors
- **IEffect API**: `EffectParameter` extended with `type`, `step`, `group`, `unit`, `advanced`; `EffectParameterType` enum; `kEffectParameterNameMaxLen`; `setParameter` returns `bool`.
- **All IEffect implementations**: Each effect now has `getParameterCount`/`getParameter`/`setParameter`/`getParameter(name)` and richer descriptor arrays (e.g. LGPFresnelZonesEffect pattern repeated across LGP*, ModalResonance, Es*Ref, SbWaveform310Ref).
- **Codecs**: `HttpEffectsCodec`/`WsEffectsCodec` encode persistence + type/step/group/unit/advanced; `WsEffectsCodec::encodeGetCurrent` and `encodeParametersGet` signatures extended; `EffectHandlers`/`DeviceHandlers`/WS command sites updated to pass new data.

#### Bug fixes / alignment
- **Tab5 encoder**: `Config.h` EFFECT_MAX 146→166, ZONE_EFFECT_MAX 146→166; `NameLookup.h` `s_pendingEffectHexId` for deferred sync; `ParameterHandler.cpp`/`ZoneComposerUI.cpp`/`main.cpp` adjustments for effect range and name lookup.
- **Firmware v2**: `effect_ids.h` FAMILY_RESERVED_START 0x1B→0x1C; new EIDs and OLD_TO_NEW entries; `display_order.h` and `CoreEffects.cpp` registration for Showpiece Pack 3.
- **WsGateway**: Single-session-per-IP rejection removed; control lease enforces single-writer; multiple observe-mode connections allowed; binary frame path and render-stream/lease cleanup on disconnect.

#### Documentation updates
- **API**: `docs/api/api-v1.md`, `docs/api/api-v2.md` (effect list, parameters, control lease, render stream); new `control-lease-v1.md`, `render-stream-v1.md`.
- **Agent/architecture**: `docs/agent/NOGIC_REVIEW.md`, `NOGIC_WORKFLOW.md`; `docs/architecture/ARCHITECTURE_MAP.md`, `ARCHITECTURE_CANVAS.html`, `nogic_canvas_spec.json`.
- **Dashboard**: `docs/dashboard/*` (wireframe, telemetry schema, implementation path); `firmware/v2/docs/effects/tunable_manifest.json`.

#### Configuration changes
- **features.h**: `FEATURE_CONTROL_LEASE`, `FEATURE_EXTERNAL_RENDER_STREAM`, `FEATURE_WASM_COMPOSER` (all 1).
- **platformio.ini**: Trailing newline only (no functional change).
- **.pre-commit-config.yaml**: New hook `effect-tunable-manifest-check` (manifest validation).
- **.github/workflows/codec_boundary_check.yml**: Python 3.11 setup; “Validate Effect Tunable Manifest” step.
- **New workflow**: `.github/workflows/route_shape_smoke.yml` (untracked).

#### Tests
- `test_http_effects_codec`, `test_ws_effects_codec`, `test_ws_color_codec`, `test_ws_device_codec`, `test_ws_palette_codec`, `test_ws_transition_codec`: Updated for new effect IDs, parameter/tunable/persistence payloads, and encode signatures.

#### Chore (deletions)
- **.claude/skills**: Removed skill trees (Changelog-generator, Content Research Writer, brand-guidelines, claude-d3js-skill-main, finishing-a-development-branch, internal-comms, supabase-expert, theme-factory, toon-formatter) and their assets/licenses/themes.

### 1.3 File Dependencies and Affected Components

```
effect_ids.h, display_order.h, features.h
    → CoreEffects.cpp, RendererActor, HttpEffectsCodec, WsEffectsCodec
    → EffectHandlers, WsEffectsCommands, DeviceHandlers
    → All IEffect .cpp/.h (registration + parameter descriptors)
    → Tab5 Config.h, NameLookup, ParameterHandler, ZoneComposerUI, main

IEffect.h (EffectParameter, kEffectParameterNameMaxLen)
    → All IEffect implementations, EffectTunableStore, RendererActor (EffectParamUpdate)
    → HttpEffectsCodec, WsEffectsCodec, ApiResponse.h

ControlLeaseManager
    → main.cpp (serial/encoder gates), WebServer (HTTP gate, callback, CORS)
    → WsGateway (WS gate, release on disconnect), WsControlCommands, ControlHandlers

RendererActor (external stream, EffectTunableStore)
    → WsStreamCommands, WsGateway (binary frame, disconnect)
    → main.cpp (no direct dependency; WebServer calls into renderer)

EffectTunableStore
    → RendererActor (init, onParameterApplied, onEffectActivated)
    → generate_manifest.py, tunable_manifest.json
    → pre-commit + codec_boundary_check.yml
```

---

## 2. Staging Strategy

Commits are ordered so that **configuration and shared API** come first, then **core features**, then **effects and codecs**, then **network and handlers**, then **encoder and tests**, and finally **docs, CI, and chore**. No commit mixes unrelated concerns.

### Commit 1 — chore(config): feature flags and effect tunable tooling
**Stage**: `firmware/v2/src/config/features.h`, `platformio.ini`, `.pre-commit-config.yaml`, `firmware/v2/tools/effect_tunables/generate_manifest.py`, `firmware/v2/docs/effects/tunable_manifest.json` (if present as new), and any other effect_tunables/ files that are purely tooling.

**Rationale**: Defines feature toggles and manifest tooling used by later commits; no dependency on code that uses them yet in this commit.

### Commit 2 — refactor(api): extend IEffect parameter descriptor and name length
**Stage**: `firmware/v2/src/plugins/api/IEffect.h`.

**Rationale**: Single API contract change; all effect and codec changes depend on it.

### Commit 3 — feat(core): effect tunable persistence store
**Stage**: `firmware/v2/src/core/persistence/EffectTunableStore.cpp`, `EffectTunableStore.h`.

**Rationale**: New component; RendererActor and codecs will depend on it.

### Commit 4 — feat(core): control lease manager
**Stage**: `firmware/v2/src/core/system/ControlLeaseManager.cpp`, `ControlLeaseManager.h`.

**Rationale**: New component; main, WebServer, WsGateway, ControlHandlers, WsControlCommands depend on it.

### Commit 5 — feat(renderer): external render stream mailbox and ingest
**Stage**: `firmware/v2/src/core/actors/RendererActor.cpp`, `RendererActor.h` (only the external-stream and EffectTunableStore integration; if mixed with other edits, stage the full file and ensure this commit message describes both external stream and tunable store integration).

**Note**: If RendererActor has both external-stream and EffectTunableStore in one diff, one commit is acceptable: “feat(renderer): external render stream and effect tunable store integration”.

### Commit 6 — feat(effects): Showpiece Pack 3 effect IDs and display order
**Stage**: `firmware/v2/src/config/effect_ids.h`, `firmware/v2/src/config/display_order.h`, `firmware/v2/src/effects/CoreEffects.cpp` (registration only for new effects).

**Rationale**: Central ID and display config; CoreEffects registration for new effect instances.

### Commit 7 — feat(effects): add Showpiece Pack 3 effect implementations
**Stage**: All new effect files under `firmware/v2/src/effects/ieffect/`:  
`LGPTimeReversalMirrorEffect{.cpp,.h}`, `LGPTimeReversalMirrorEffect_AR{.cpp,.h}`, `LGPTimeReversalMirrorEffect_Mod1{.cpp,.h}`, `LGPTimeReversalMirrorEffect_Mod2{.cpp,.h}`, `LGPTimeReversalMirrorEffect_Mod3{.cpp,.h}`, `LGPKdVSolitonPairEffect{.cpp,.h}`, `LGPGoldCodeSpeckleEffect{.cpp,.h}`, `LGPQuasicrystalLatticeEffect{.cpp,.h}`, `LGPFresnelCausticSweepEffect{.cpp,.h}`.

**Rationale**: New effects only; keeps ID/registration (commit 6) separate from implementation.

### Commit 8 — refactor(effects): IEffect parameter descriptors and setParameter for all effects
**Stage**: All modified effect files under `firmware/v2/src/effects/ieffect/` (LGP*, ModalResonance, Es*Ref, SbWaveform310Ref): `.cpp` and `.h` for each.

**Rationale**: One refactor for “parameter API adoption” across existing effects; avoids mixing with new Showpiece Pack 3 logic.

### Commit 9 — refactor(codec): effects codec persistence and parameter fields
**Stage**: `firmware/v2/src/codec/HttpEffectsCodec.cpp`, `HttpEffectsCodec.h`, `firmware/v2/src/codec/WsEffectsCodec.cpp`, `WsEffectsCodec.h`, `firmware/v2/src/network/ApiResponse.h` (if it only touches effect/parameter response types).

**Rationale**: HTTP/WS encoding of new parameter and persistence fields in one place.

### Commit 10 — feat(network): control lease HTTP gate and REST handlers
**Stage**: `firmware/v2/src/network/WebServer.cpp` (control lease callback, CORS, HTTP gate, ControlHandlers + WsControlCommands registration, `serviceRenderStreamState`), `firmware/v2/src/network/webserver/handlers/ControlHandlers.cpp`, `ControlHandlers.h`.

**Rationale**: REST and WebServer wiring for control lease; does not include WS command implementation.

### Commit 11 — feat(network): WebSocket control commands and lease gate
**Stage**: `firmware/v2/src/network/webserver/ws/WsControlCommands.cpp`, `WsControlCommands.h`.

**Rationale**: WS command layer for control lease.

### Commit 12 — feat(network): render stream WebSocket commands and binary handling
**Stage**: `firmware/v2/src/network/webserver/ws/WsStreamCommands.cpp`, `WsStreamCommands.h`, and WsGateway changes that affect binary frames and render-stream disconnect (i.e. `firmware/v2/src/network/webserver/WsGateway.cpp`, `WsGateway.h`).

**Rationale**: Render stream WS protocol and gateway binary/disconnect behaviour in one feature.

### Commit 13 — feat(network): control lease WebSocket gate and release on disconnect
**Stage**: Remaining WsGateway changes (lease-exempt command check, `releaseByDisconnect`, `handleMessage` signature with `arg` for binary). If already staged in commit 12, ensure commit 12 message mentions “binary frame handling and render-stream disconnect” and add a short note in commit 13 that “WsGateway lease gate and release on disconnect” are included there or in 12. Alternatively, stage the full WsGateway diff once and use a single commit: “feat(network): WsGateway binary frames, render stream disconnect, and control lease gate”.

**Simplified option**: Combine commits 12 and 13 into one: **feat(network): WsGateway binary frames, render stream, and control lease** — stage `WsGateway.cpp`, `WsGateway.h`, `WsStreamCommands.cpp`, `WsStreamCommands.h`.

### Commit 14 — refactor(network): effect and device handlers for parameters and tunables
**Stage**: `firmware/v2/src/network/webserver/handlers/EffectHandlers.cpp`, `firmware/v2/src/network/webserver/handlers/DeviceHandlers.cpp`, `firmware/v2/src/network/webserver/V1ApiRoutes.cpp` (if only effect/device route changes), `firmware/v2/src/network/webserver/ws/WsEffectsCommands.cpp`, `WsDeviceCommands.cpp`, `WsSysCommands.cpp` (only if changes are parameter/tunable/effect-ID related).

**Rationale**: Handlers and WS effect/device commands updated for new API and IDs.

### Commit 15 — fix(main): gate serial and encoder mutations by control lease
**Stage**: `firmware/v2/src/main.cpp` (control lease checks for serial JSON, serial text, encoder events).

**Rationale**: Application-level enforcement of control lease for local input.

### Commit 16 — fix(tab5): encoder effect range and name lookup for v2 alignment
**Stage**: `firmware/Tab5.encoder/src/config/Config.h`, `firmware/Tab5.encoder/src/utils/NameLookup.h`, `firmware/Tab5.encoder/src/main.cpp`, `firmware/Tab5.encoder/src/parameters/ParameterHandler.cpp`, `firmware/Tab5.encoder/src/ui/ZoneComposerUI.cpp`.

**Rationale**: Tab5 encoder alignment with v2 effect count and ID handling.

### Commit 17 — test(codec): update codec tests for effect IDs and parameter payloads
**Stage**: All files under `firmware/v2/test/` that were modified (`test_http_effects_codec`, `test_ws_effects_codec`, `test_ws_color_codec`, `test_ws_device_codec`, `test_ws_palette_codec`, `test_ws_transition_codec`).

**Rationale**: Tests stay in sync with codec and effect ID changes.

### Commit 18 — chore(ci): effect tunable manifest validation and Python setup
**Stage**: `.github/workflows/codec_boundary_check.yml`, and optionally `.github/workflows/route_shape_smoke.yml` if you want it in this branch.

**Rationale**: CI and optional smoke workflow only.

### Commit 19 — docs(api): control lease and render stream API
**Stage**: `docs/api/api-v1.md`, `docs/api/api-v2.md`, `docs/api/control-lease-v1.md`, `docs/api/render-stream-v1.md`.

**Rationale**: API documentation for new surface area.

### Commit 20 — docs(agent): Nogic review and architecture
**Stage**: `docs/agent/NOGIC_REVIEW.md`, `docs/agent/NOGIC_WORKFLOW.md`, `docs/architecture/ARCHITECTURE_MAP.md`, `docs/architecture/ARCHITECTURE_CANVAS.html`, `docs/architecture/nogic_canvas_spec.json`.

**Rationale**: Agent and architecture docs.

### Commit 21 — docs(dashboard): K1 composer wireframe and telemetry
**Stage**: `docs/dashboard/` (all new files).

**Rationale**: Dashboard design and schema docs.

### Commit 22 — feat(tools): K1 composer dashboard and effect catalog
**Stage**: `tools/k1-composer-dashboard/` (entire directory), `firmware/v2/tools/effect_tunables/` (if not already in commit 1), and `firmware/v2/tools/test_broadcast_fix.sh`, `tools/web/web_smoke_test.py` if they are part of dashboard/smoke tooling.

**Rationale**: New tooling and scripts for dashboard and smoke.

### Commit 23 — chore(skills): remove unused Claude skills
**Stage**: All deleted files under `.claude/skills/` (Changelog-generator, Content Research Writer, brand-guidelines, claude-d3js-skill-main, finishing-a-development-branch, internal-comms, supabase-expert, theme-factory, toon-formatter).

**Rationale**: Single chore commit for skill cleanup; per project rules, do not exclude files—commit only what you intend to change. If the intent was to remove these skills, stage and commit the deletions.

---

## 3. Commit Message Format

Each commit MUST follow conventional commits and include a body and footer where applicable.

### Template

```
<type>(<scope>): <subject (≤50 chars)

[optional body]
- Implementation approach
- Technical considerations
- Architectural impacts
- Dependencies introduced/removed

[optional footer]
Refs: #issue
BREAKING CHANGE: ...
Migration: ...
```

### Full commit message examples (copy into repo)

**Commit 1 — chore(config)**
```
chore(config): add control lease, render stream, WASM flags and effect tunable manifest

Implementation:
- features.h: FEATURE_CONTROL_LEASE, FEATURE_EXTERNAL_RENDER_STREAM, FEATURE_WASM_COMPOSER (all 1).
- Pre-commit hook and codec_boundary_check.yml: effect tunable manifest validation (generate_manifest.py --validate --enforce-all-families).
- platformio.ini: trailing newline only.

Technical: No code depends on these flags in this commit; later commits gate on FEATURE_*.
Dependencies: None introduced.
Footer: Ref: feature/tab5-ios-effectid-alignment
```

**Commit 2 — IEffect API**
```
refactor(api): extend IEffect parameter descriptor and name length

Implementation:
- EffectParameter: type (EffectParameterType), step, group, unit, advanced; constructor extended with defaults.
- EffectParameterType enum (FLOAT, INT, BOOL, ENUM); effectParameterTypeToString() helper.
- kEffectParameterNameMaxLen = 40 for transport and EffectTunableStore key length.
- setParameter(name, value) return type changed to bool for persistence feedback.

Technical: All IEffect implementations must implement getParameterCount/getParameter/setParameter/getParameter(name); codecs and handlers updated in later commits.
Architectural: EffectTunableStore and HTTP/WS parameter encoding depend on this contract.
Dependencies: None new; existing effects gain new optional descriptor fields.

Refs: feature/tab5-ios-effectid-alignment
BREAKING CHANGE: setParameter now returns bool; callers should check return value.
```

**Commit 4 — Control lease**
```
feat(core): control lease manager for exclusive dashboard control

- ControlLeaseManager: acquire/heartbeat/release by WebSocket client; TTL and heartbeat interval; release on disconnect.
- Mutation sources: LocalSerial, LocalEncoder, REST, WebSocket; read-only commands exempt.
- main.cpp gates serial JSON/text and encoder; WebServer gates mutating HTTP; WsGateway gates mutating WS and releases lease on disconnect.

BREAKING CHANGE: Mutating commands blocked when another client holds the lease.
Migration: Clients that need to control the device must call control.acquire and send heartbeats; otherwise commands may be rejected.
```

**Commit 5 — RendererActor**
```
feat(renderer): external render stream and effect tunable store integration

- External stream: mailbox ingest, start/stop, stale timeout revert to internal render; ExternalLock (taskENTER_CRITICAL on ESP32).
- EffectTunableStore: init on actor start; onParameterApplied after setParameter; onEffectActivated after effect init.
- Render loop: if external mode and frame valid, copy mailbox to LEDs and increment framesRendered; else run internal renderFrame().
```

**Commit 6 — Effect IDs**
```
feat(effects): Showpiece Pack 3 effect IDs and display order

- FAMILY_SHOWPIECE_PACK3 0x1B; FAMILY_RESERVED_START 0x1C.
- EIDs 0x1B00–0x1B08: TimeReversalMirror, KdVSolitonPair, GoldCodeSpeckle, QuasicrystalLattice, FresnelCausticSweep, TRM AR/Mod1/2/3.
- OLD_TO_NEW extended to 168 entries; display_order extended; CoreEffects registration for new effect instances.
```

**Commit 16 — Tab5**
```
fix(tab5): encoder effect range and name lookup for v2 alignment

- EFFECT_MAX and ZONE_EFFECT_MAX 146→166 (167 effects 0–166).
- NameLookup: s_pendingEffectHexId for deferred sync before effects.list load.
- ParameterHandler, ZoneComposerUI, main updated for range and lookup.
```

**Commit 23 — Skills**
```
chore(skills): remove unused Claude skills

Implementation:
- Deleted .claude/skills trees: Changelog-generator, Content Research Writer, brand-guidelines, claude-d3js-skill-main, finishing-a-development-branch, internal-comms, supabase-expert, theme-factory, toon-formatter (including assets, licenses, themes, references).

Technical: Large net line reduction; no code references these paths.
Architectural: No impact on firmware or tooling.
Dependencies: None removed from build or runtime.

Refs: feature/tab5-ios-effectid-alignment
Migration: Re-add from backup or upstream if a skill is needed again.
```

---

## 4. Context Documentation

### 4.1 Control lease

- **Problem**: Multiple clients (dashboard, Tab5, serial) could mutate state concurrently; no single-writer guarantee.
- **Solution**: Explicit acquire/heartbeat/release over WebSocket; mutation gates in main (serial/encoder), WebServer (HTTP), WsGateway (WS); release on disconnect.
- **Alternatives**: Optimistic locking (rejected: harder to reason about); per-resource locks (rejected: scope creep).
- **Challenges**: Defining read-only vs mutating commands; avoiding blocking status/query; lease expiry and takeover semantics.
- **Performance**: One static check per mutating request; no measurable render impact.
- **Security**: Lease token in responses; future auth can bind lease to identity.
- **Testing**: Manual WS acquire/release; serial and encoder blocked when lease held.
- **Limitations**: Single global lease; no per-zone or per-effect lease.

### 4.2 External render stream

- **Problem**: K1F1 dashboard needs to push pre-rendered framebuffers (e.g. WASM composer) to the device.
- **Solution**: Binary WebSocket frames ingested into a small mailbox in RendererActor; render loop copies latest frame to LEDs; stale timeout reverts to internal render.
- **Alternatives**: REST POST of base64 frame (rejected: overhead); raw UDP (rejected: reliability).
- **Challenges**: Lock-free or low-contention mailbox; avoiding render loop allocation; 320×3 byte frame size contract.
- **Performance**: One memcpy per frame from mailbox to m_leds; lock held briefly.
- **Testing**: Manual WS binary send; validate revert on timeout.
- **Limitations**: Single producer; no back-pressure; frame size fixed.

### 4.3 Effect tunables

- **Problem**: Per-effect parameters (e.g. zone_count, edge_threshold) were not persisted; dashboard could not show “dirty” or “last error”.
- **Solution**: EffectTunableStore (NVS namespace + volatile fallback), debounced write; RendererActor calls onParameterApplied/onEffectActivated; HTTP/WS parameters get encode persistence + type/step/group/unit/advanced.
- **Alternatives**: Per-effect NVS keys (rejected: fragmentation); global blob only (current: one blob per effect, capped entries).
- **Challenges**: Name length limit (40) and checksum for blob integrity; migration if descriptor changes.
- **Performance**: Tick and write debounced; no alloc in render path.
- **Testing**: Manifest validation in pre-commit and CI; manual parameter change and reload.
- **Limitations**: Max effects and entries per effect fixed; no schema versioning in NVS yet.

### 4.4 Showpiece Pack 3 and effect ID alignment

- **Problem**: Tab5 and iOS need stable effect IDs and a shared display order; new “Showpiece Pack 3” effects need IDs and registration.
- **Solution**: New family 0x1B; EIDs 0x1B00–0x1B08; OLD_TO_NEW and display_order extended; Tab5 EFFECT_MAX 146→166; NameLookup s_pendingEffectHexId for status-before-list sync.
- **Alternatives**: Keep sequential 0–161 only (rejected: no room); separate ID space per platform (rejected: cross-platform UI).
- **Challenges**: Keeping OLD_TO_NEW and display_order in sync; Tab5 build and UI tested with 167 effects.
- **Testing**: Codec tests updated; manual Tab5 scroll and effect switch.
- **Limitations**: OLD_TO_NEW size is fixed at compile time; adding more effects requires a bump.

### 4.5 IEffect parameter API

- **Problem**: UI needed type, step, group, unit, and “advanced” for each parameter; persistence and validation need a stable name length.
- **Solution**: EffectParameter extended; setParameter returns bool; kEffectParameterNameMaxLen; all effects implement getParameterCount/getParameter/setParameter/getParameter(name) with descriptors.
- **Alternatives**: Keep min/max/default only (rejected: poor UX); JSON schema per effect (rejected: codegen and sync cost).
- **Challenges**: Many effect files; avoiding heap in descriptor arrays (static const).
- **Performance**: No runtime alloc; descriptor read at parameter list time.
- **Testing**: Codec tests; effect.list and effect.parameters.get in WS.
- **Limitations**: Name length 40; no i18n for displayName yet.

---

## 5. Lessons Learned

- **Effect ID strategy**: Reserve family blocks (0x1B, 0x1C…) and extend OLD_TO_NEW and display_order in one place to avoid drift between firmware and encoders.
- **Control lease**: Exempt read-only commands by naming convention (e.g. `.get`, `.list`, `status`, `subscribe`) so status dashboards and observers do not need a lease.
- **WsGateway**: Relaxing “one session per IP” allowed multiple observe-only connections; single-writer is enforced by lease, not by connection count.
- **Binary WS**: AsyncWebSocket’s `AwsFrameInfo* arg` is required to detect binary opcode; handleMessage signature change was necessary.
- **EffectTunableStore**: Debounce (e.g. 1500 ms) avoids NVS thrash when sliders are moved quickly; dirty flag and lastError improve dashboard UX.
- **Tab5 EFFECT_MAX**: Must match v2’s effective count (e.g. display order length or MAX_EFFECTS); s_pendingEffectHexId avoids showing wrong name before effects.list arrives.
- **Pre-commit**: Manifest validation (`generate_manifest.py --validate --enforce-all-families`) catches missing or inconsistent tunable metadata before push.
- **Skills cleanup**: Large file deletions (e.g. supabase-expert, theme-factory) significantly reduce diff size; do in a dedicated chore commit.

---

## 6. Future Contributor Guidance

### Maintenance

- **Adding an effect**: Assign EID from next slot in family (see effect_ids.h); add to display_order and OLD_TO_NEW; register in CoreEffects; implement getParameterCount/getParameter/setParameter/getParameter; add to tunable manifest and run `generate_manifest.py --validate`.
- **Adding a parameter**: Extend EffectParameter array; implement setParameter/getParameter; keep name length ≤ kEffectParameterNameMaxLen.
- **Control lease**: New mutating WS commands must not be in the lease-exempt list; new mutation sources (e.g. MQTT) need a gate in the same style as main.cpp/WebServer/WsGateway.
- **External stream**: Frame size must remain EXTERNAL_STREAM_FRAME_BYTES (320×3); changing LED count requires a contract bump and dashboard update.

### Improvement areas

- **Lease**: Per-zone or per-resource lease; takeover with confirmation; lease history for debugging.
- **Tunables**: Schema version in NVS for migration; optional i18n for displayName/group/unit.
- **Render stream**: Back-pressure or ACK; configurable mailbox depth; frame rate cap.
- **Effect IDs**: Run-time or generated OLD_TO_NEW from a single manifest to avoid manual sync.

### Gotchas

- **Do not** add heap allocation in render() or in the external-frame copy path; use static/member buffers only.
- **Do not** extend EffectParameter with variable-length strings without a new name length constant and transport contract.
- **Do not** remove commands from the lease-exempt list without checking dashboard and Tab5 for read-only usage.
- **Do not** change EXTERNAL_STREAM_FRAME_BYTES without updating render.stream.* docs and any K1 composer contract.

### References

- Centre origin and constraints: `.cursor/rules/000-core-constraints.mdc`, `CLAUDE.md`, `AGENTS.md`
- Architecture: `docs/agent/ARCHITECTURE.md`, `docs/architecture/ARCHITECTURE_MAP.md`
- Effects: `docs/agent/EFFECTS.md`; IEffect: `firmware/v2/src/plugins/api/IEffect.h`
- Control lease API: `docs/api/control-lease-v1.md`
- Render stream API: `docs/api/render-stream-v1.md`
- Build: `docs/agent/BUILD.md`; Tab5: `.cursor/rules/030-build-commands.mdc`

---

---

## 7. Execution Checklist

Use the following as a reference to stage and commit. Adjust paths if files were moved or renamed.

```bash
# Commit 1 — config and manifest tooling
git add firmware/v2/src/config/features.h platformio.ini .pre-commit-config.yaml
git add firmware/v2/tools/effect_tunables/ firmware/v2/docs/effects/tunable_manifest.json
git commit -m "chore(config): add control lease, render stream, WASM flags and effect tunable manifest"

# Commit 2 — IEffect API
git add firmware/v2/src/plugins/api/IEffect.h
git commit -m "refactor(api): extend IEffect parameter descriptor and name length"

# Commit 3 — EffectTunableStore
git add firmware/v2/src/core/persistence/EffectTunableStore.cpp firmware/v2/src/core/persistence/EffectTunableStore.h
git commit -m "feat(core): effect tunable persistence store"

# Commit 4 — ControlLeaseManager
git add firmware/v2/src/core/system/ControlLeaseManager.cpp firmware/v2/src/core/system/ControlLeaseManager.h
git commit -m "feat(core): control lease manager"

# Commit 5 — RendererActor (external stream + tunable integration)
git add firmware/v2/src/core/actors/RendererActor.cpp firmware/v2/src/core/actors/RendererActor.h
git commit -m "feat(renderer): external render stream and effect tunable store integration"

# Commit 6 — effect IDs and display order
git add firmware/v2/src/config/effect_ids.h firmware/v2/src/config/display_order.h
git add firmware/v2/src/effects/CoreEffects.cpp
git commit -m "feat(effects): Showpiece Pack 3 effect IDs and display order"

# Commit 7 — new effect implementations
git add firmware/v2/src/effects/ieffect/LGPTimeReversalMirrorEffect*.cpp firmware/v2/src/effects/ieffect/LGPTimeReversalMirrorEffect*.h
git add firmware/v2/src/effects/ieffect/LGPKdVSolitonPairEffect.* firmware/v2/src/effects/ieffect/LGPGoldCodeSpeckleEffect.*
git add firmware/v2/src/effects/ieffect/LGPQuasicrystalLatticeEffect.* firmware/v2/src/effects/ieffect/LGPFresnelCausticSweepEffect.*
git commit -m "feat(effects): add Showpiece Pack 3 effect implementations"

# Commit 8 — refactor all existing effects (parameter API)
git add firmware/v2/src/effects/ieffect/LGP*.cpp firmware/v2/src/effects/ieffect/LGP*.h
git add firmware/v2/src/effects/ieffect/ModalResonanceEffect.* firmware/v2/src/effects/ieffect/esv11_reference/*.cpp firmware/v2/src/effects/ieffect/esv11_reference/*.h
git add firmware/v2/src/effects/ieffect/sensorybridge_reference/*.cpp firmware/v2/src/effects/ieffect/sensorybridge_reference/*.h
# (exclude new Showpiece Pack 3 files if already committed in 7)
git commit -m "refactor(effects): IEffect parameter descriptors and setParameter for all effects"

# Commit 9 — codecs
git add firmware/v2/src/codec/HttpEffectsCodec.cpp firmware/v2/src/codec/HttpEffectsCodec.h
git add firmware/v2/src/codec/WsEffectsCodec.cpp firmware/v2/src/codec/WsEffectsCodec.h
git add firmware/v2/src/network/ApiResponse.h
git commit -m "refactor(codec): effects codec persistence and parameter fields"

# Commit 10 — WebServer + ControlHandlers
git add firmware/v2/src/network/WebServer.cpp
git add firmware/v2/src/network/webserver/handlers/ControlHandlers.cpp firmware/v2/src/network/webserver/handlers/ControlHandlers.h
git commit -m "feat(network): control lease HTTP gate and REST handlers"

# Commit 11 — WsControlCommands
git add firmware/v2/src/network/webserver/ws/WsControlCommands.cpp firmware/v2/src/network/webserver/ws/WsControlCommands.h
git commit -m "feat(network): WebSocket control commands and lease gate"

# Commit 12 — WsStreamCommands + WsGateway
git add firmware/v2/src/network/webserver/ws/WsStreamCommands.cpp firmware/v2/src/network/webserver/ws/WsStreamCommands.h
git add firmware/v2/src/network/webserver/WsGateway.cpp firmware/v2/src/network/webserver/WsGateway.h
git commit -m "feat(network): render stream WebSocket and WsGateway binary/lease"

# Commit 14 — handlers and WS effect/device
git add firmware/v2/src/network/webserver/handlers/EffectHandlers.cpp firmware/v2/src/network/webserver/handlers/DeviceHandlers.cpp
git add firmware/v2/src/network/webserver/V1ApiRoutes.cpp
git add firmware/v2/src/network/webserver/ws/WsEffectsCommands.cpp firmware/v2/src/network/webserver/ws/WsDeviceCommands.cpp firmware/v2/src/network/webserver/ws/WsSysCommands.cpp
git commit -m "refactor(network): effect and device handlers for parameters and tunables"

# Commit 15 — main.cpp lease gate
git add firmware/v2/src/main.cpp
git commit -m "fix(main): gate serial and encoder mutations by control lease"

# Commit 16 — Tab5 encoder
git add firmware/Tab5.encoder/src/config/Config.h firmware/Tab5.encoder/src/utils/NameLookup.h
git add firmware/Tab5.encoder/src/main.cpp firmware/Tab5.encoder/src/parameters/ParameterHandler.cpp firmware/Tab5.encoder/src/ui/ZoneComposerUI.cpp
git commit -m "fix(tab5): encoder effect range and name lookup for v2 alignment"

# Commit 17 — tests
git add firmware/v2/test/
git commit -m "test(codec): update codec tests for effect IDs and parameter payloads"

# Commit 18 — CI
git add .github/workflows/codec_boundary_check.yml .github/workflows/route_shape_smoke.yml
git commit -m "chore(ci): effect tunable manifest validation and Python setup"

# Commit 19 — API docs
git add docs/api/api-v1.md docs/api/api-v2.md docs/api/control-lease-v1.md docs/api/render-stream-v1.md
git commit -m "docs(api): control lease and render stream API"

# Commit 20 — agent/architecture docs
git add docs/agent/NOGIC_REVIEW.md docs/agent/NOGIC_WORKFLOW.md
git add docs/architecture/ARCHITECTURE_MAP.md docs/architecture/ARCHITECTURE_CANVAS.html docs/architecture/nogic_canvas_spec.json
git commit -m "docs(agent): Nogic review and architecture"

# Commit 21 — dashboard docs
git add docs/dashboard/
git commit -m "docs(dashboard): K1 composer wireframe and telemetry"

# Commit 22 — K1 dashboard tooling
git add tools/k1-composer-dashboard/ firmware/v2/tools/test_broadcast_fix.sh tools/web/web_smoke_test.py
git commit -m "feat(tools): K1 composer dashboard and effect catalog"

# Commit 23 — skills removal
git add -u .claude/skills/
git commit -m "chore(skills): remove unused Claude skills"
```

**Note**: Commit 8 may require care to exclude the new Showpiece Pack 3 effect files if they were added in commit 7 (e.g. `git add` specific filenames or use `git reset` then re-add). Verify with `git status` after each commit.

---

*This strategy was generated from a full `git diff` and `git status` review. Execute staging in the order above and adjust commit boundaries if your working tree has diverged.*
