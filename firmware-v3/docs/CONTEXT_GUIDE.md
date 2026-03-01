# Context Guide — LightwaveOS

Detailed delegation guidance for AI agents working on this codebase. Referenced from CLAUDE.md; read on-demand, not every session.

## Subsystem Context Costs

| Subsystem | Files | Size | Risk | Guidance |
|-----------|-------|------|------|----------|
| Effects | 349 | 1.77 MB / 49K LOC | **EXTREME** | Never read more than 1-2 effects in main context. Delegate audits, pattern searches, and cross-effect comparisons. Effects follow identical structure — read one to understand the pattern. |
| Network/API | 130 | 1.4 MB | HIGH | REST handlers, WebSocket, MQTT, OTA. Delegate investigation; grep for specific endpoints. |
| Audio pipeline | 65 | 632 KB | HIGH | AudioActor + PipelineCore + PipelineAdapter + ControlBus. Self-contained subsystem — delegate deep investigation. |
| Codec | 60 | 472 KB | MODERATE | Effect payload marshalling, command dispatch. Delegate unless editing a specific file. |
| Actors | 15 | 395 KB | MODERATE | All follow the same actor pattern. Read the one being modified; delegate cross-actor analysis. |
| Docs | 40 | 775 KB | **TRAP** | api-v1.md alone is 2,124 lines. Never read docs eagerly — search for specific sections. |
| LED driver | 14 | 86 KB | Low | OK in main context. Small files, hardware-specific. |
| Config headers | 13 | 101 KB | Low | OK in main context. Pin configs, feature flags, effect IDs. |

**Totals**: 802 source files, 5.84 MB, ~145K LOC (excluding .pio/ dependencies).

## Delegation Scenarios

| Scenario | Why delegate | What to pass the subagent |
|----------|-------------|--------------------------|
| Effect audits ("find all effects using X") | 349 files — classic subagent task | The pattern to search for; Hard Constraints if touching render paths |
| Audio pipeline investigation | Requires 5+ files: AudioActor, PipelineCore, PipelineAdapter, ControlBus, audio_config | Audio constraints: `bands[0..7]`, `chroma[0..11]`, dt-decay pattern, silence gate |
| Crash/bug investigation | Backtrace analysis + multiple source files + FreeRTOS internals | Hard Constraints; the crash log or symptom description |
| Cross-backend comparison | ESV11 vs PipelineCore are parallel subsystems with conditional compilation | Which backend matters for the current task |
| API endpoint investigation | api-v1.md is 2,124 lines + handler code + CQRS architecture (652 lines) | The specific endpoint or feature being investigated |
| Network layer exploration | 130 files across REST, WebSocket, MQTT, OTA | The specific protocol or feature |

## Context Traps

Things that waste tokens without proportional value:

- **Reading multiple effect files** to understand the pattern — they're structurally identical. Read one (e.g. a recent LGP effect), understand the pattern, delegate the rest.
- **Reading api-v1.md in full** (2,124 lines) — search/grep for the specific endpoint instead.
- **Reading ESV11 backend code** when working on PipelineCore (and vice versa) — only one compiles at a time.
- **Reading vendor/ directories** (ESV11 vendor shim, .pio/ deps) — framework/proprietary code.
- **Reading CQRS_STATE_ARCHITECTURE.md** (652 lines) unless the task specifically involves state management or command dispatch.
- **Reading audio-visual-semantic-mapping.md** (467 lines) unless writing or debugging an audio-reactive effect.
- **Eagerly loading any Further Doc** — all are 170-2,124 lines. Check the trigger condition in the CLAUDE.md table first.

## Task-Scoped Constraint Routing

When delegating to subagents, pass only constraints relevant to that task:

**Always pass:**
- Hard Constraints (centre origin, no rainbows, no heap alloc in `render()`, 120 FPS, British English)

**Conditionally pass:**

| Task domain | Additional constraints |
|-------------|----------------------|
| Effects / rendering | Centre origin, no rainbows, no heap alloc in `render()`, 120 FPS budget, zone bounds-checking (`ctx.zoneId < kMaxZones`) |
| Audio pipeline | `bands[0..7]` API (backend-agnostic), `chroma[0..11]`, dt-decay pattern, silence gate hysteresis |
| API / network | REST endpoint conventions, CQRS command/query separation |
| LED driver | RMT channel config (`FASTLED_RMT_MAX_CHANNELS=4`), Core 1 assertion guard, dual-strip parallel TX |

## Subagent Return Contract

Every subagent must return:
- Files inspected (with paths)
- Concrete findings and recommendations
- Confidence level and open questions

## Reference Doc Costs

| Doc | Lines | Trigger |
|-----|-------|---------|
| CONSTRAINTS.md | 170 | Performance work, memory optimisation |
| audio-visual-semantic-mapping.md | 467 | Writing/debugging audio-reactive effects |
| api-v1.md | 2,124 | API endpoint work — **search, don't read in full** |
| CQRS_STATE_ARCHITECTURE.md | 652 | State management, command dispatch |
| HARNESS_RULES.md | 364 | Harness/test infrastructure |
