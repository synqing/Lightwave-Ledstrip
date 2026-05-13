---
abstract: "Naming-convention review index for the SynqMatrix migration. Catalogues every function, method, parameter, member field, enum, and constant across the firmware-v3 non-effect codebase (~6,680 lines across 11 subsystem files). Surfaces cross-cutting anomalies, the incomplete songAware rename gap, British/American spelling drift, and duplicate types. Read this index first, then drill into the subsystem file matching the surface Captain wants to rename."
---

# SynqMatrix Migration — Naming-Convention Review

**Generated:** 2026-05-13
**Branch:** `feature/synqmatrix-rename-2026-05-13`
**Production target:** `esp32dev_audio_esv11_k1v2_32khz`
**Scope:** every C++ function, method, parameter, member field, enum value, and constant across non-effect firmware-v3 source. Effects (~349 files under `src/effects/`) deliberately excluded.

---

## Table of contents

| # | Subsystem | File | Lines | Coverage |
|---|---|---|---|---|
| 01 | Audio Contracts | [`01-audio-contracts.md`](01-audio-contracts.md) | 745 | ControlBus, MusicalGrid, OnsetSemantics, AudioTime, MotionSemantics, MotionShaper, MusicalSaliency, StyleDetector, AudioEffectMapping, SnapshotBuffer (13 files) |
| 02 | Audio Backends + AudioActor | [`02-audio-backends-and-actor.md`](02-audio-backends-and-actor.md) | 571 | AudioActor, AudioCapture, ChromaAnalyzer, GoertzelAnalyzer, AudioBehaviorSelector, esv11/* backend (24 files) |
| 03 | Audio Pipeline + Onset + Tempo | [`03-audio-pipeline-onset-tempo.md`](03-audio-pipeline-onset-tempo.md) | 857 | pipeline/, onset/, tempo/, TranslationEngine, BeatTracker (18 files) |
| 04 | RendererActor | [`04-renderer-actor.md`](04-renderer-actor.md) | 421 | RendererActor.{h,cpp} — the monolith (2 files, ~4200 LOC source) |
| 05 | Actors + Base | [`05-actors-and-base.md`](05-actors-and-base.md) | 531 | Actor, ActorSystem, ShowDirectorActor, NodeOrchestrator, RendererNode (8 files) |
| 06 | SynqMatrix + Plugin API | [`06-synqmatrix-and-plugin-api.md`](06-synqmatrix-and-plugin-api.md) | 466 | synqmatrix/, plugins/api/, BuiltinEffectRegistry, PluginManagerActor, LegacyEffectAdapter (13 files) |
| 07 | Network — Server + Routes | [`07-network-server-routes.md`](07-network-server-routes.md) | 419 | WebServer, WebServerBroadcast, V1ApiRoutes, StaticAssetRoutes, WsGateway, WsCommandRouter (10 files) |
| 08 | Network — Handlers + WS Commands | [`08-network-handlers-and-ws-commands.md`](08-network-handlers-and-ws-commands.md) | 917 | REST handlers (25 modules) + WS commands (28 modules) (106 files, ~20 KLOC) |
| 09 | Serial CLI + JSON + Capture | [`09-serial-and-capture.md`](09-serial-and-capture.md) | 472 | SerialCLI, SerialJsonGateway, CaptureStreamer, ValidationMode (8 files) |
| 10A | HAL + Config | [`10A-hal-config.md`](10A-hal-config.md) | 586 | hal/{display,esp32p4,esp32s3,interface,led}, config/* (37 files) |
| 10B | Core utilities | [`10B-core-utilities.md`](10B-core-utilities.md) | 695 | bus, diagnostics, narrative, persistence, shows, state, system, SystemInit, EffectTypes (23 files inspected) |

**Total: 11 files, 6,680 lines, ~260 firmware source files inventoried.**

---

## Cross-cutting findings (Captain — start here)

The 14 SSAs surfaced five themes that span multiple subsystems. These are the highest-leverage rename targets — addressing them once cleans up many files.

### 1. SongAware rename gap — confirmed across the whole codebase

The d1d7b807 commit message overstated scope. My sed substitution missed the `songAware` camelCase form entirely. The rename is INCOMPLETE in these places (cross-confirmed by SSA-N04, N06, N07, N08, N09):

| Surface | Count | Source file |
|---|---|---|
| RendererActor private fields `m_songAwareDirector*` | 8 fields + 1 helper | 04-renderer-actor.md |
| SynqMatrix helper functions `synqmatrix::songAware*Name(...)` | 11 functions + 2 callers | 06-synqmatrix-and-plugin-api.md |
| Struct fields `rawSongState`, `previousSongState`, `currentSongState`, `candidateSongState` | 4 fields | 06-synqmatrix-and-plugin-api.md |
| REST routes `/api/v1/songAware/*` | 5–7 routes | 07-network-server-routes.md, 08-network-handlers-and-ws-commands.md |
| WS command strings `songAware.*` | 13 commands (incl. casing alias `songAware.countersReset`) | 08-network-handlers-and-ws-commands.md |
| WS response `type:` values | 9 distinct types | 08-network-handlers-and-ws-commands.md |
| JSON wire field keys (`currentSongState` etc.) | 4 keys | 08-network-handlers-and-ws-commands.md |
| User-facing error strings ("Invalid songAware config…") | 4 strings | 08-network-handlers-and-ws-commands.md |
| Serial CLI prose output (`songAware: …`) | 60 lines | 09-serial-and-capture.md |
| Serial JSON gateway types + status prefixes | 45 lines | 09-serial-and-capture.md |
| `g_songAwareRestorePoint*` file-scope state | 4 globals across 4 TUs | 09-serial-and-capture.md |
| Comment / doc-prose references | ~50+ | All files |

**Total: ~200+ rename touchpoints across 11 files plus YAML contracts.** Phase 0.1 is larger than originally framed but the camelCase pattern (`songAware → synqMatrix`) is mechanical.

### 2. British/American spelling drift

CLAUDE.md mandates British English in comments, docs, logs, and UI strings — but identifiers across the codebase are American (`color`, `center`, `behavior`). This creates a split:

| Module | American identifier | British comment / prose | Source |
|---|---|---|---|
| Audio actor | `getColor`, `colorOrder` | comments use "colour" | 02-audio-backends-and-actor.md |
| RendererActor | `CENTER_LED_INDEX` | otherwise British prose | 04-renderer-actor.md |
| HAL public API | `colorOrder`, `centerPoint`, `colorCorrection` | — | 10A-hal-config.md |
| VrmsHandlers (WS) | `colourVariance` (the only British wire field) | rest of wire surface is American | 08-network-handlers-and-ws-commands.md |
| Effect API | `getBand`, `getHeavyBand`, `getCenterIndex` | — | 06-synqmatrix-and-plugin-api.md |

**Decision needed:** keep American in code (low-cost), or sweep to British (high-cost, ABI-affecting for HAL public API). Recommend keeping American in identifiers, British in everything else — close to status quo. Or commit fully to British across identifiers as a one-time sweep.

### 3. Casing-convention drift within the same subsystem

Constants, enums, and methods use inconsistent cases even within single files:

- `kCamelCase` (`kMaxLedsPerStrip`, `kT0H`) vs `SCREAMING_SNAKE_CASE` (`COLOR_BLACK`, `GLYPH_WIDTH`) — SSA-N10A
- `PascalCase` vs `camelCase` methods in the same class — SSA-N01, N02, N03
- `SHOUTY_SNAKE_CASE` enum values (`CaptureTap`) vs `PascalCase` enums elsewhere — SSA-N04
- Whole modules in `snake_case` (`WsTrinity`, `WsStimulus`, parts of `WsSys`) amongst PascalCase peers — SSA-N08
- CLI verbs in `lowercase` (`status`, `dbg`) vs `VAL:*` uppercase — SSA-N09
- `kPascalCase` Google-style constants in `.cpp` anonymous namespaces vs `UPPER_SNAKE` in headers — SSA-N02

### 4. Duplicate / parallel types — unification candidates

| Duplication | Files | Risk |
|---|---|---|
| Two `ILedDriver` interfaces in same namespace | `hal/interface/ILedDriver.h` + `hal/led/ILedDriver.h` | Compile-time confusion |
| Two `BeatTracker` types | `audio/BeatTracker` + `audio/pipeline/BeatTracker` | Symbol-search ambiguity |
| Two `ZonePreset` types in `lightwaveos::persistence` | built-in vs user-saveable | Already namespace-collision territory |
| Two narrative phase enums | `NarrativePhase` vs `ShowNarrativePhase` | Cross-module mismatch |
| NVS namespace `"zones"` + NVS key `"zones"` (in `zone_config` namespace) | — | Persistence-key collision risk |
| `LedStripConfig` (per-strip) vs `LedDriverConfig` (whole-system) | — | Easy to swap-wrong |
| `prism::` namespace + `lightwaveos::shows` namespace for same subsystem | — | Domain duplication |

### 5. Real bugs caught incidentally

- **`CaptureStats::hopsCapured`** — typo (missing `t`). Real bug. `02-audio-backends-and-actor.md`.
- **`ActorSystem::setPalette` writes JSON to hard-coded host path `.cursor/debug.log`** — leaked dev path runs on production firmware. `05-actors-and-base.md`.
- **Tautological condition in `Actor::run` line 383** — dead branch. `05-actors-and-base.md`.
- **Three WS modules register ZERO commands** (`WsFilesystemCommands`, `WsModifierCommands`, `WsPresetCommands`) — dead stubs. `08-network-handlers-and-ws-commands.md`.
- **`DisplayActor` is a real `actors::Actor` placed under `hal/display/`** not `core/actors/` — directory placement bug. `10A-hal-config.md`.
- **`effect_ids.h` is AUTO-GENERATED** via `gen_effect_ids.py` — any future rename of effect IDs must update the generator, not the output. `10A-hal-config.md`.
- **NarrativeEngine v1 aliases** (`getTension`, `getPhaseProgress`) — predecessor of SynqMatrix per project memory, may be removable. `10B-core-utilities.md`.

---

## Suggested rename categories (Captain decides scope)

| Category | Impact | Risk | Files affected |
|---|---|---|---|
| **A.** Complete `songAware → synqMatrix` camelCase wire-rename (Phase 0.1 of three-layer migration) | HIGH (external wire) | LOW (no clients consume) | 11 files + YAML contracts |
| **B.** Wire-name vocabulary alignment — REST kebab-case vs camelCase segments (`colorCorrection`, `edgeMixer`, `factoryPresets`, `songAware`) | MEDIUM | LOW (mechanical) | V1ApiRoutes.cpp |
| **C.** WS command vocabulary — singular/plural duplication, snake_case islands | MEDIUM | MEDIUM (some legacy aliases may break clients) | 08-* files |
| **D.** British/American spelling — public API surface (centerPoint → centrePoint etc.) | HIGH (ABI-affecting) | HIGH (touches every consumer) | HAL + Effect API |
| **E.** Casing-convention consolidation — pick one style per scope (constants kCamelCase OR SCREAMING_SNAKE, never mixed) | LOW | LOW | Many files |
| **F.** Duplicate-type unification (two `ILedDriver`, two `BeatTracker`, two `ZonePreset`, two narrative enums) | HIGH (structural) | MEDIUM | hal/, audio/, core/persistence/ |
| **G.** Dead-code removal (`WsFilesystem/Modifier/Preset` stubs, NarrativeEngine v1 aliases) | LOW | LOW | 08-*, 10B-* |
| **H.** Bug fixes (`hopsCapured` typo, `.cursor/debug.log` leaked path) | LOW (size) but HIGH (correctness) | LOW | 02-*, 05-* |
| **I.** Helper function naming (`synqmatrix::songAware*Name` → `synqMatrix*Name`) — internal cleanup | LOW (internal) | LOW | 06-* |
| **J.** RendererActor field naming (`m_songAwareDirector*` → `m_synqMatrix*`) — internal cleanup | LOW (internal) | LOW | 04-* |

---

## How to use this catalogue

1. **Pick a category from the table above.** Or scan the subsystem files for specific names that stand out.
2. **Open the relevant subsystem file** (e.g. `08-network-handlers-and-ws-commands.md` for wire-name renames).
3. **Mark the names you want changed.** Annotate by editing the file inline, or by sending me a list.
4. **I'll dispatch rename SSAs** category-by-category with hardware A/B gates between, per the phased-migration discipline already approved.

Files at `docs/temporary/projects/synqmatrix-naming-review/` — under `temporary/` deliberately so this catalogue can be archived or deleted after the migration completes, per the workspace hygiene rules.

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-13 | agent:claude-opus-4-7 | Created index from 14-SSA parallel extraction. Catalogues 11 subsystem files totalling ~6,680 lines covering ~260 firmware source files. Surfaces cross-cutting anomalies including the incomplete songAware rename gap and 5 incidental bugs. |
