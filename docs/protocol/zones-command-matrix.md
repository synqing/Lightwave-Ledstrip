---
abstract: "Canonical command matrix for Zone Composer. Single source of truth across REST, WebSocket, SerialJSON, SerialCLI transports plus client (iOS/Tab5/Dashboard/HW Mixer) coverage. Every command labelled implemented / partial / stub→501 / unsupported / planned with readback path and regression-test status. Drives Phase 0 contract truth and Phase 1 keystone work."
---

# Zone Composer Command Matrix

**Purpose.** Single source of truth for every zone-related command in LightwaveOS firmware-v3. Drafted 2026-05-01 as part of Phase 0 (B2) of the Zone Composer Instrument Program.

**Hard rule (Captain's invariant).** No new UI, hardware, or marketing surface exposes a capability until the engine state, transport command, readback state, and regression test all agree.

**Wire-format note (2026-05-02 migration LANDED).** All `zoneId` values on the wire (REST request bodies, REST path parameters, WebSocket payloads, SerialJSON payloads) are 1-indexed: valid values are `1`, `2`, `3`. Wire value `0` is RESERVED and rejected with INVALID_VALUE / 400. The 1-indexed wire format matches the user-facing Zone 1/2/3 labels. Internal C++ array indexing is unchanged (0..2); translation is performed once at the network boundary. All examples below use the post-migration 1-indexed format.

**Status legend.**
- ✅ — implemented + verified
- ⚠️ — partial (works but missing validation, error path, or readback consistency)
- 🚫 — stub returning HTTP 501 NOT_IMPLEMENTED (formerly fake-success placeholder; cleaned up Phase 0 B1)
- ❌ — not exposed on this transport
- 📋 — planned for a specific Phase
- N/A — not applicable to this transport

---

## 1. System enable / disable

| Command | REST | WS | SerialJSON | SerialCLI | iOS | Tab5 | Dashboard | HW Mixer | Persistence | Validation | Readback | Regression |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Toggle zone composer | ✅ POST `/api/v1/zones/enabled` | ✅ `zone.enable` | ✅ `zone.enable` / `zones.enabled` | ✅ `z` hotkey | ✅ ZoneHeaderCard switch | ✅ encoder 7 click | ✅ implicit via layout apply | 📋 Phase 2 | volatile (NVS via separate save endpoint, currently 🚫) | none | `isEnabled()` getter; `GET /api/v1/zones` returns `enabled` field | D-8 single-effect regression gate (Phase 0) |

## 2. Layout

| Command | REST | WS | SerialJSON | SerialCLI | iOS | Tab5 | Dashboard | HW Mixer | Persistence | Validation | Readback | Regression |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Set zone layout (segments) | ✅ POST `/api/v1/zones/layout` | ✅ `zones.setLayout` | ❌ **GAP — Phase 0 B5 #1** | ❌ | ⚠️ implicit via zone count selector | ⚠️ implicit via encoder 6 | ✅ ZoneEditor full editor | 📋 Phase 2 (after B5) | volatile | `validateLayout()` exhaustive (segment ranges, symmetry, centre inclusion, ordering, no overlaps) | `getZoneConfig()` via REST `GET /api/v1/zones`; missing on WS/SerialJSON | E5 cross-transport equivalence (Phase 0 skeleton, fills in Phase 1) |
| Get zone layout | ✅ `GET /api/v1/zones` (bundled with state) | ⚠️ `zones.get` includes layout | ⚠️ `zones.list` includes layout | ❌ | ✅ via REST | ✅ via WS | ✅ via REST | 📋 Phase 2 | N/A (read) | none | full state | E5 |

## 3. Per-zone state setters (effect / brightness / speed / palette / blend mode / enabled)

| Command | REST | WS | SerialJSON | SerialCLI | iOS | Tab5 | Dashboard | HW Mixer | Persistence | Validation | Readback | Regression |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Set zone effect | ✅ `POST /api/v1/zones/{id}/effect` | ✅ `zone.setEffect`, `zones.setEffect`, `zones.update` | ✅ `zone.setEffect`, `zones.update` | ❌ | ✅ EffectNavigationRow + selector | ✅ encoder 0 | ✅ planned per-zone editor (Phase 1 D-3) | 📋 Phase 2 | volatile | `isEffectRegistered()` check | `getZoneEffect()` getter | E5 cross-transport |
| Set zone brightness | ✅ `POST /api/v1/zones/{id}/brightness` | ✅ `zone.setBrightness`, `zones.update` | ✅ `zone.setBrightness`, `zones.update` | ❌ | ✅ LWSlider 0..255 | ✅ encoder 3 | 📋 Phase 1 | 📋 Phase 2 | volatile | uint8 range | `getZoneBrightness()` | E5 |
| Set zone speed | ✅ `POST /api/v1/zones/{id}/speed` | ✅ `zone.setSpeed`, `zones.update` | ✅ `zone.setSpeed`, `zones.update` | ⚠️ `zs <id> <speed>` (no batch) | ✅ LWSlider 1..100 | ✅ encoder 2 | 📋 Phase 1 | 📋 Phase 2 | volatile | clamped to [1, 100] in setter | `getZoneSpeed()` | E5 |
| Set zone palette | ✅ `POST /api/v1/zones/{id}/palette` | ✅ `zone.setPalette`, `zones.update` | ✅ `zone.setPalette`, `zones.update` | ❌ | ✅ PaletteNavigationRow + selector | ✅ encoder 1 | 📋 Phase 1 | 📋 Phase 2 | volatile | `validatePaletteId()` (0-74) | `getZonePalette()` | E5 |
| Set zone blend mode | ✅ `POST /api/v1/zones/{id}/blend` | ✅ `zone.setBlend`, `zones.update` | ✅ `zone.setBlend`, `zones.update` | ❌ | ❌ **GAP — UI not wired (model has setZoneBlend)** | ✅ encoder 4 | 📋 Phase 1 | 📋 Phase 2 | volatile | range-checked against `MODE_COUNT=8` (commit `9d7bc261`, F1) | `getZoneBlendMode()` | E5; F1 hardware-validated 2026-05-01 |
| Enable / disable single zone | ✅ `POST /api/v1/zones/{id}/enabled` | ✅ `zone.enableZone` | ❌ **GAP — Phase 0 B5 #2** | ❌ | ⚠️ implicit | ⚠️ via zone count | ⚠️ implicit | 📋 Phase 2 | volatile | none | `isZoneEnabled()` | E5 |

## 4. Per-zone effect parameter setter (Phase 1 keystone, D-3 gated behind D-1)

| Command | REST | WS | SerialJSON | SerialCLI | iOS | Tab5 | Dashboard | HW Mixer | Persistence | Validation | Readback | Regression |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Global effect parameters (existing, single-effect mode) | ✅ `POST /api/v1/effects/parameters` | ✅ `effects.parameters.set` | ✅ `effects.parameters.set` (commit `e9f37eed`) | ❌ | ✅ EffectParameterSheet (Phase 2) | ❌ | 📋 | ❌ | volatile | per-effect parameter contract (`IEffect::setParameter()`) | per-effect getParameter | F1 regression |
| Zone-aware effect parameters (D-3 keystone) | 📋 Phase 1 `POST /api/v1/zones/{zoneId}/effects/parameters` | 📋 Phase 1 `zone.effects.parameters.set` | 📋 Phase 1 `zone.effects.parameters.set` | ❌ | 📋 Phase 1 (retarget EffectParameterSheet with zoneId) | 📋 Phase 1 expression page | 📋 Phase 1 | 📋 Phase 2 (after B5) | volatile | zoneId range + per-parameter contract | per-zone instance getParameter (after D-1 instance pool) | Spike 2 + E5 |

**Gating:** Every cell in row 2 above is BLOCKED on Spike 1 (D-1 effect instance pool) succeeding on hardware. Without instance isolation, the zone-aware setter silently corrupts cross-zone state — verified V2 in the targeted verification pass.

## 5. Audio configuration per zone

| Command | REST | WS | SerialJSON | SerialCLI | iOS | Tab5 | Dashboard | HW Mixer | Persistence | Validation | Readback | Regression |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Zone audio routing (D-4) | 🚫 `GET/POST /api/v1/zones/{id}/audio` 501 (cleaned Phase 0 B1) | ❌ | ❌ | ❌ | 📋 Phase 1 audio routing selector | 📋 Phase 1 audio page | 📋 Phase 1 | 📋 Phase 2 | volatile | `routingMode` enum (FULL_MIX/BASS/MID/HIGH; CUSTOM deferred) | `getZoneAudioConfig()` (currently dead-coded — V3 confirmed) | Spike 3 + E5 |

**Implementation status:** `m_zoneAudioConfigs[]` storage exists in ZoneComposer but is NOT read in `renderZone()` (V3 dead-coded confirmation). Phase 1 A3 wires it into render via D-4 hint+envelope contract.

## 6. Presets (factory)

| Command | REST | WS | SerialJSON | SerialCLI | iOS | Tab5 | Dashboard | HW Mixer | Persistence | Validation | Readback | Regression |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| List zone presets | ✅ `GET /api/v1/zone-presets`, `GET /api/v1/presets/zones` | ❌ | ❌ | ❌ | ✅ via zone count selector | ⚠️ implicit | ✅ ZoneEditor preset listbox | 📋 Phase 2 | N/A | none | array of preset metadata | basic |
| Get specific preset | ✅ `GET /api/v1/zone-presets/get?id=N`, `GET /api/v1/presets/zones/{id}` | ❌ | ❌ | ❌ | N/A | N/A | ✅ implicit | 📋 Phase 2 | N/A | preset ID range | preset config | basic |
| Apply preset | ✅ `POST /api/v1/zone-presets/apply?id=N`, `POST /api/v1/presets/zones/{id}/load` | ✅ `zone.loadPreset` | ✅ `zone.loadPreset`, `zones.setPreset` | ❌ | ✅ via zone count selector | ⚠️ implicit | ✅ ZoneEditor | 📋 Phase 2 | volatile | preset ID range | full state via subsequent get | E5 |

## 7. Custom presets (user-saved)

| Command | REST | WS | SerialJSON | SerialCLI | iOS | Tab5 | Dashboard | HW Mixer | Persistence | Validation | Readback | Regression |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Save current as preset | ✅ `POST /api/v1/zone-presets`, `POST /api/v1/presets/zones/save-current` | ❌ | ❌ | ❌ | ❌ — UI gap | ❌ | ❌ | 📋 Phase 4 | NVS | name 1-31, slot availability | via list | manual |
| Delete preset | ✅ `DELETE /api/v1/zone-presets/delete?id=N`, `DELETE /api/v1/presets/zones/{id}` | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | 📋 Phase 4 | NVS | preset ID + built-in protection | via list | manual |

## 8. Snapshots (Phase 3 — A6)

| Command | REST | WS | SerialJSON | SerialCLI | iOS | Tab5 | Dashboard | HW Mixer | Persistence | Validation | Readback | Regression |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Save zone state snapshot | 🚫 `POST /api/v1/zones/config/save` 501 (Phase 0 B1) — replaces with A6 endpoint Phase 3 | ❌ | ❌ | ❌ | 📋 Phase 3 scene memory UI | 📋 Phase 3 snapshot recall mode | 📋 Phase 3 snapshot manager | 📋 Phase 4 | NVS (versioned schema) | full state validation | dedicated snapshot endpoint Phase 3 | E6 |
| Load zone state snapshot | 🚫 `POST /api/v1/zones/config/load` 501 (Phase 0 B1) — Phase 3 | ❌ | ❌ | ❌ | 📋 | 📋 | 📋 | 📋 | NVS | snapshot ID + version compat | full state via subsequent get | E6 |
| Get current state for export | 🚫 `GET /api/v1/zones/config` 501 (Phase 0 B1) — Phase 3 | ❌ | ❌ | ❌ | 📋 | 📋 | 📋 | 📋 | N/A | none | full state | E6 |

## 9. Performance / live (Phase 4)

| Command | REST | WS | SerialJSON | SerialCLI | iOS | Tab5 | Dashboard | HW Mixer | Persistence | Validation | Readback | Regression |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Solo zone (mute others) | 📋 A5 Phase 4 | 📋 | 📋 | 📋 | 📋 Phase 4 button | 📋 Phase 4 gesture | 📋 | 📋 (HW button) | volatile | zoneId range | enabled-state per zone | future |
| A/B preview banks | 📋 Phase 4 | 📋 | 📋 | ❌ | 📋 | 📋 A/B mode | 📋 | 📋 (HW crossfader) | NVS for banks, volatile for preview | bank ID + crossfade ms | bank state | future |
| Beat-trigger config | 🚫 `GET/POST /api/v1/zones/{id}/beat-trigger` 501 (Phase 0 B1) — Phase 4 | ❌ | ❌ | ❌ | 📋 | 📋 | 📋 | 📋 | volatile | tempo source + interval | beat-trigger snapshot | future |

## 10. Diagnostics / metrics (Phase 8)

| Command | REST | WS | SerialJSON | SerialCLI | iOS | Tab5 | Dashboard | HW Mixer | Persistence | Validation | Readback | Regression |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Per-zone render timing | 🚫 `GET /api/v1/zones/timing` 501 (Phase 0 B1) — Phase 8 | ❌ | ❌ | ❌ | 📋 | 📋 | 📋 dev panel | ❌ | volatile | none | metrics snapshot | future |
| Reset timing stats | 🚫 `POST /api/v1/zones/timing/reset` 501 (Phase 0 B1) — Phase 8 | ❌ | ❌ | ❌ | ❌ | ❌ | 📋 dev panel | ❌ | N/A | none | confirmation | future |

## 11. Advanced layering (Phase 6)

| Command | REST | WS | SerialJSON | SerialCLI | iOS | Tab5 | Dashboard | HW Mixer | Persistence | Validation | Readback | Regression |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Z-order / reorder | 🚫 `POST /api/v1/zones/reorder` 501 (Phase 0 B1) — Phase 6 (A9) | ❌ | ❌ | ❌ | ❌ | ❌ | 📋 Phase 6 layer editor | ❌ | volatile | array completeness check | order array | future |

---

## Summary by status

| Status | Count |
|---|---|
| ✅ Implemented + verified across REST/WS/Serial | 13 commands |
| ⚠️ Partial (mostly implicit / readback-only / single-transport) | 4 commands |
| 🚫 Stub→501 (Phase 0 B1 cleaned) | 10 endpoints (5 conceptual groups × GET+POST) |
| 📋 Planned Phase 1 (D-3, D-4, audio routing, blend UI) | 5 command groups |
| 📋 Planned Phase 2-3 | snapshot system + dashboard parity + serialJSON parity |
| 📋 Planned Phase 4-8 | solo/mute, A/B banks, transitions, Z-order, modulation, chaining |

## Cross-references

- Phase 0 B1 stub disposition: this matrix's 🚫 cells. Captain decision recorded in this matrix.
- Phase 0 B5 SerialJSON parity: see [`zones-serial-json-parity.md`](zones-serial-json-parity.md). Gaps: setLayout, per-zone setEnabled, audio config, getZoneConfig, future zone-aware parameters.
- Phase 0 D-8 single-effect regression gate: every PR proves zones-disabled → single-effect renders correctly.
- Phase 0 E5 cross-transport equivalence: any state created via REST/WS/SerialJSON must be readable identically via every other transport.
- Phase 1 keystone D-3: `zone.effects.parameters.set` (row in section 4 above), gated behind D-1 instance pool.
- ADR for D-1..D-5: [`../../docs/adr/zone-composer-architecture-decisions.md`](../adr/zone-composer-architecture-decisions.md).

---
**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-05-01 | Claude (Phase 0 B2) | Created. Single source of truth for Zone Composer command coverage. Drafted from 5-SSA context report + verification pass V1-V6 + Phase 0 B1 disposition decisions. |
| 2026-05-02 | Claude (B2 wire-format migration) | Added wire-format note: zoneId migrated to 1-indexed across REST/WS/SerialJSON. Internal C++ indexing unchanged. |
