---
abstract: "Phase 3 scoping doc for iOS↔firmware parity. Captures findings from 4 parallel SSAs covering: show upload/delete UX (small effort, firmware ready), preset broadcast wiring (BLOCKER discovered — PresetsViewModel not instantiated on AppViewModel), runtime parameter ENUM case-name display (recommend firmware-side fix; small effort), contract YAML regenerator (medium effort, larger drift than audit suggested — 170 REST + 152 WS in firmware vs ~50/40 documented). Presents Captain a sized decision menu, NOT a multi-choice form — upstream facts surfaced first."
---

# iOS ↔ Firmware Parity — Phase 3 Scoping

**Branch context:** PR #11 (Phase 1 → main) + PR #12 (Phase 2 → Phase 1) open and awaiting review.
**This doc:** scoping ONLY — produces the next decision menu. No implementation here.

## Findings (from 4 parallel SSAs)

### Finding 1 — Show upload + delete (SSA-α)

| | |
|---|---|
| **Firmware mechanism** | Upload: `POST /api/v1/shows` (32 KB JSON max) OR WS `show.upload` text frame → both call `ShowBundleParser::parse()`. Delete: `DELETE /api/v1/shows/{id}` OR WS `show.delete` — built-in shows return 403. |
| **Payload format** | JSON ShowBundle DSL: chapters (max 32) + cues (max 512) + timing + zone targets + narrative phases. Max 4 uploaded shows in PSRAM. |
| **iOS UX** | SwiftUI `.fileImporter` for upload (user provides `.json` directly — iOS does NOT author shows). Confirmation dialog for delete. Disable Delete button for `builtin: true` rows. |
| **iOS effort** | **SMALL — 6-8 hours.** Reuses existing `ShowViewModel`, `RESTClient`, `WebSocketService`. |
| **Confidence** | HIGH — firmware complete, protocol unambiguous. |
| **Open Q** | (1) Is there a documented ShowBundle JSON schema? Currently inferred from parser code. (2) `show.cue.inject` — Phase 3 or out of scope? |

### Finding 2 — Preset broadcast wiring (SSA-β) — **BLOCKER FOUND**

| | |
|---|---|
| **What was thought** | Phase 2 P2-3 said "AppViewModel→PresetsViewModel broadcast wiring is the single follow-up needed for Phase 3 — two-line edit". |
| **What is actually true** | `PresetsViewModel` is **NOT instantiated** in `AppViewModel.init()` (verify: lines 94-106 — `effects, palettes, parameters, zones, audio, transition, colourCorrection, edgeMixer` — no `presets`). PresetsViewModel currently lives… nowhere on AppViewModel. The Phase 2 PresetsView likely creates its own VM via `@StateObject` or similar. |
| **Real wiring scope** | (1) Add `var presets: PresetsViewModel` property to AppViewModel. (2) Init it. (3) Inject REST client into it (PresetsViewModel doesn't expose a `.restClient` property today — needs adding). (4) Wire 2 stub branches in switch. So **NOT a 2-line edit — closer to ~12 lines + a property pattern decision**. |
| **iOS effort** | **SMALL — 1-2 hours**, but needs deciding *where* presets live. Either (a) child VM on AppViewModel like the others, or (b) PresetsView owns its own VM and the Phase 1 broadcast hooks become NotificationCenter-style fan-out. Recommend (a) for symmetry. |
| **Confidence** | MEDIUM (architectural choice needed, not just typing). |
| **Open Q** | Other Phase 1 broadcast stubs (`cameraMode.changed`, `factoryPresets.changed`, `colourCorrection.set*`) have NO Phase 2 ViewModel hooks yet. Are they intentional gaps or did P2 miss them? |

### Finding 3 — Runtime parameter ENUM case-name display (SSA-γ)

| | |
|---|---|
| **Firmware emits enum case names today** | NO. `EffectParameter` struct (`firmware-v3/src/plugins/api/IEffect.h:144-172`) has no field for it. `EffectHandlers.cpp:257-300` emits only `name/displayName/min/max/default/value/type`. Effects like `LGPGradientFieldEffect` define enum params as floats with integer range and hardcode case meanings in render code. |
| **Recommended fix** | **Firmware-side, non-breaking.** Add Optional `enumCaseNames: const char**` + `enumCaseCount: uint8_t` to `EffectParameter`. Default `nullptr`/`0` — backward-compatible. Emit `enumCases: ["Label0", "Label1", ...]` in JSON when populated. iOS decodes via Optional Codable field; fallback to integer labels remains. |
| **Memory cost (ESP32)** | ~8 bytes per parameter (2 pointers) + flash for case strings. Acceptable — effects already store displayName/description as `const char*`. |
| **Effort** | Firmware: **SMALL — ~2 hours** (struct fields + 3 effects + tests). iOS: **SMALL — ~1 hour** (Optional Codable field + ParameterRow update). |
| **iOS-only workaround** | Hardcoded `[effectId: [paramName: [caseLabel]]]` table. **NOT recommended** — brittle (breaks when firmware adds cases), maintenance burden scales with effect count. |
| **Confidence** | HIGH. |
| **Open Q** | Backfill existing enum-parameter effects (e.g. `LGPGradientFieldEffect`'s `basis`, `repeatMode`, `interpolation`), or only newly-added effects? |

### Finding 4 — Contract YAML regenerator (SSA-δ)

| | |
|---|---|
| **Drift extent** | LARGER than initial audit. **170 REST routes registered** in `V1ApiRoutes.cpp` (vs ~132 audit), **152 WS commands** registered across 24 `Ws*Commands.cpp` files (vs ~126 audit). Contract YAML covers ~117 REST + ~141 WS. **Real drift gap: ~50 REST + ~10+ WS routes undocumented.** |
| **Recommended tool** | Python 3 (precedent: `firmware-v3/tools/check_effect_contracts.py`). Hybrid regex + AST-lite pattern matching — full clang AST is overkill. **Incremental merge model**: read existing YAML anchors, generate skeletons from code, merge preserving descriptions/consumers/notes (the human-input fields). New routes get placeholder `description: "FIXME"`, `consumers: []`. |
| **Tool location** | `firmware-v3/tools/contract-regen/` (`regen_rest.py`, `regen_ws.py`, `merge.py`). |
| **Effort** | **MEDIUM — 5-8 days.** Drivers: multi-line lambda capture regex, glob across 24 WS files, field-level merge logic. |
| **Confidence** | HIGH. Risk LOW (analysis tool only; no firmware/CI modification). |
| **Open Q** | CI integration (e.g. `make contract-check` failing on drift) or on-demand only? Inferring response schemas from handler bodies (next-phase complexity) or placeholder + human fill? |

## Phase 3 candidates — sized

| ID | Item | Source | Effort | Risk | Recommendation |
|----|------|--------|--------|------|----------------|
| **P3-1** | Show upload + delete UI | Phase 2 deferral | S (1d) | low | **PRIORITY** — closes the ShowsView surface, low risk, high user value |
| **P3-2** | Preset broadcast wiring + PresetsViewModel as child VM | Phase 2 deferral | S-M (2-4h) | low | **PRIORITY** — small, but unblocks live multi-client preset sync |
| **P3-3** | ENUM case-name display (firmware + iOS) | Phase 2 deferral | S+S (~3h total) | low | **PRIORITY** — coordinated firmware+iOS PR, ships with effect parameters that have ENUM type (e.g. 0x130E successors) |
| **P3-4** | Contract YAML regenerator | F-1 follow-up | M (5-8d) | low | **DEFERRABLE** — F-1 explicitly said this is on-demand. Worth doing but not blocking Phase 3 ship. |
| **P3-5** | Other Phase 1 broadcast stubs (`cameraMode.changed`, `factoryPresets.changed`, `colourCorrection.set*`) — wire to Phase 2 VMs | SSA-β open Q | unknown | unknown | **NEEDS-CAPTAIN** — were these intentional gaps? Each needs its own decision. |
| **P3-6** | Show.cue.inject + show upload progress UX polish | SSA-α open Q | M (2-3d) | medium | **DEFERRABLE** — advanced feature, requires designing the cue authoring UX |
| **P3-7** | ShowBundle JSON schema doc | SSA-α open Q | S (4h) | none | **OPPORTUNISTIC** — write a `docs/protocol/show-bundle-schema.md` next time someone touches `ShowBundleParser` |

## Recommended Phase 3 scope (NEEDS-CAPTAIN approval)

**Ship one focused PR**: P3-1 + P3-2 + P3-3 in parallel via 3 sandboxed SSAs. Total iOS effort ~1.5 days; coordinated firmware change for P3-3 is ~2 hours.

- Total surface: ~17 hours of work, all SMALL items, low risk
- Bumps iOS test count from 115 → estimate 130-135
- Coordinated firmware/iOS lockstep on P3-3 is the only complexity

Defer: P3-4 (regenerator) to its own work track; P3-5 (other broadcast wiring) pending Captain decision on intent; P3-6 (cue.inject) pending UX design; P3-7 opportunistically.

## Decisions needed from Captain (RBDO upstream facts before tactical commit)

These are the upstream facts that gate the Phase 3 plan. Each is one decision Captain can make in 5 minutes; without them, my plan is DEGRADED-MODE.

1. **D-1 — Phase 3 scope confirmation:** Does the recommended P3-1+P3-2+P3-3 bundle match Captain's intent, or do you want a different mix?
2. **D-2 — P3-2 ownership pattern:** Should `PresetsViewModel` become a child VM on `AppViewModel` (symmetric with effects/zones/audio/etc.), or stay PresetsView-local with broadcast routing via NotificationCenter?
3. **D-3 — P3-3 firmware backfill:** When iOS gets enum case-name display, should firmware backfill existing enum-parameter effects (e.g. `LGPGradientFieldEffect`'s 3 enum params), or only emit names for *new* effects going forward?
4. **D-4 — P3-5 broadcast triage:** Are the silently-stubbed `cameraMode.changed`, `factoryPresets.changed`, `colourCorrection.set*` Phase 1 broadcasts an intentional gap (e.g. those are device-state events that AppViewModel doesn't need to react to, only log), OR is each one a future wiring task waiting for its target ViewModel?
5. **D-5 — P3-4 schedule:** Does the contract YAML regenerator block any current work? F-1 says no; if confirmed, it slots into a future quiet week — no urgency.

Once D-1..D-5 land, Phase 3 becomes a determinate plan and I can dispatch implementation SSAs.

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-01 | claude-opus-4-7 | Created from 4 parallel scoping SSAs (show upload/delete, preset wiring, enum case names, contract regenerator). Captures findings + sized Phase 3 candidate list + Captain decisions D-1..D-5 needed before implementation plan. |
