---
abstract: "SerialJSON parity inventory for Zone Composer commands. Lists every capability missing from SerialJSON that exists on REST or WS, ordered by leverage (impact on agent tooling and hardware mixer). Phase 0 B5 deliverable. Drives sequencing for Phase 2 SerialJSON completeness work."
---

# Zone Composer — SerialJSON Parity Inventory

**Purpose.** Document every zone capability missing from SerialJSON today, ordered by leverage. SerialJSON is the canonical machine-control transport for agent tooling and the future hardware mixer (per Captain's doctrine: SerialCLI is human/diagnostic, SerialJSON is machine API). Gaps here block agent and hardware control of zone capability.

**Source of truth:** Verification pass V6 (2026-05-01) confirmed against `firmware-v3/src/serial/SerialJsonGateway.cpp` HEAD `9d7bc261`. Cross-checked against the unified command matrix (`zones-command-matrix.md`).

**Scope.** This is an inventory + leverage ranking. It is NOT an implementation plan — Phase 2 work will pick from this list per Captain's stated ordering: "Do not implement all parity gaps blindly. Order them by leverage."

---

## What SerialJSON HAS today (8 commands)

Verified at the cited line numbers in `firmware-v3/src/serial/SerialJsonGateway.cpp`:

| Command | Line | Calls into ZoneComposer |
|---|---|---|
| `zone.enable` / `zones.enabled` | 605 | `setEnabled()` |
| `zone.setEffect` | 615 | `setZoneEffect()` |
| `zone.setBrightness` | 632 | `setZoneBrightness()` |
| `zone.setSpeed` | 648 | `setZoneSpeed()` |
| `zone.setPalette` | 664 | `setZonePalette()` |
| `zone.setBlend` | 680 | `setZoneBlendMode()` (range-check fix F1, commit `9d7bc261`) |
| `zones.update` (batch) | 701 | conditional setters |
| `zone.loadPreset` / `zones.setPreset` | 738 | `loadPreset()` |

**Listing also exists:** `zones.list` at line 320 (read-only).

## What SerialJSON DOES NOT have (gap list, ordered by leverage)

Captain's directive: order by leverage, do not implement blindly.

### Gap 1 — `setLayout` (HIGH leverage)

**Why this is #1.** Layout determines what zones EXIST. Without `setLayout` over SerialJSON, agents and the future hardware mixer cannot reconfigure zone topology — they're locked to whatever layout was applied via REST/WS or factory preset. The hardware mixer's "always-on three-zone faders" implicitly requires fixed-or-preset layout, but live agent reconfiguration (e.g. switch from Triple Rings to Dual Split) must work over Serial.

**Existing transports:** REST `POST /api/v1/zones/layout` (`ZoneHandlers.cpp:75`); WS `zones.setLayout` (`WsZonesCommands.cpp:524`).

**ZoneComposer method:** `setLayout(const ZoneSegment* segments, uint8_t count)` with `validateLayout()` (~50 lines of geometric/symmetry checks).

**Implementation shape (proposed for Phase 2; uses the 1-indexed wire format mandated by Captain's 2026-05-02 directive):**
```json
{
  "type": "zones.setLayout",
  "requestId": "...",
  "zoneCount": 3,
  "zones": [
    {"zoneId": 1, "s1LeftStart": 65, "s1LeftEnd": 79, "s1RightStart": 80, "s1RightEnd": 94},
    {"zoneId": 2, "s1LeftStart": 20, "s1LeftEnd": 64, "s1RightStart": 95, "s1RightEnd": 139},
    {"zoneId": 3, "s1LeftStart": 0, "s1LeftEnd": 19, "s1RightStart": 140, "s1RightEnd": 159}
  ]
}
```

> **Wire-format migration LANDED 2026-05-02.** Firmware now accepts `zoneId` as 1-indexed (1, 2, 3) on every transport (REST, WebSocket, SerialJSON). Internal C++ array indices remain 0..2; translation happens once at the network boundary. Out-of-range wire values (0, 4+) are rejected with INVALID_VALUE. **All examples in this document use the canonical 1-indexed format.**

Mirror response shape from `zones.setLayout` WS handler.

### Gap 2 — Per-zone `setEnabled` (HIGH leverage)

**Why this is #2.** Performance-time mute/solo workflows need rapid toggle of individual zones. Today over SerialJSON the only way is `zones.update` with `{enabled: false}` per zone, but `zones.update` doesn't include an `enabled` field in the schema (verified — only effectId/brightness/speed/paletteId/blendMode). The global `zone.enable` toggles the whole composer, not individual zones.

**Existing transports:** REST `POST /api/v1/zones/{id}/enabled`; WS `zone.enableZone` (`WsZonesCommands.cpp:66`).

**ZoneComposer method:** `setZoneEnabled(uint8_t zone, bool enabled)`.

**Implementation shape:**
```json
{"type": "zone.setEnabled", "requestId": "...", "zoneId": 1, "enabled": false}
```

### Gap 3 — Blend mode (NOT a gap, present at line 680 — verify only)

**Status:** Captain's source-list mentioned blend "if missing." Verified present at `SerialJsonGateway.cpp:680` (`zone.setBlend`) and within `zones.update` at line 718 (with F1 range-check fix). **NOT A GAP.** Listed here per Captain's defensive instruction.

### Gap 4 — Audio config getter / setter (MEDIUM leverage, BLOCKED by D-4)

**Why this is #4.** Audio routing per zone is dead-coded today (V3 confirmed). Adding SerialJSON commands now would create a fake-working surface — a violation of the Hard Rule. Phase 0 B1 cleaned the REST stubs to 501; SerialJSON should remain absent until D-4 wires the routing config into `renderZone()`.

**Existing transports:** Phase 0 B1 made these explicit 501 stubs at REST. WS has no command. SerialJSON has no command.

**ZoneComposer method:** `getZoneAudioConfig(uint8_t zone)` / `setZoneAudioConfig(uint8_t zone, const ZoneAudioConfig&)`.

**Phase 1 implementation (after D-4 wiring):**
```json
{
  "type": "zone.audio.set",
  "zoneId": 1,
  "routingMode": "BASS",   // FULL_MIX | BASS | MID | HIGH
  "tempoSync": false,
  "beatModulation": false
}
```

### Gap 5 — `getZoneConfig` / readback (MEDIUM leverage)

**Why this is #5.** SerialJSON has `zones.list` for current per-zone state. After `setLayout` lands (Gap 1), agents need to verify the layout took effect.

**Existing transports:** REST `GET /api/v1/zones` includes the segment data. WS `zones.get` includes layout. SerialJSON `zones.list` returns per-zone effect/brightness/speed/palette/blend/enabled but NOT yet the segment geometry (s1LeftStart/End, s1RightStart/End per zone).

**Status update 2026-05-02 (B2 migration):**
- Field-level row parity gap CLOSED — SerialJSON `zones.list` now emits `zoneId` (1-indexed) and `effectName` (string) per row, matching REST `GET /api/v1/zones`.
- Segment-geometry parity (the `segments[]` array) remains a Phase 2 task.

**Implementation shape (Phase 2, after Gap 1):**
- Either extend `zones.list` to include segment geometry in the response (preferred — single command)
- Or add `zones.getLayout` for layout-only readback (cleaner separation)

### Gap 6 — `zone.effects.parameters.set` (HIGHEST LEVERAGE — Phase 1 keystone, BLOCKED by D-1)

**Why this is the keystone.** This is the single highest-leverage SerialJSON addition for the entire program (per ADR D-3). Marries the runtime parameter system (`effects.parameters.set` global, shipped commit `e9f37eed`) with zones. Every effect's specific declared parameters become per-zone-tunable.

**BLOCKED by:** D-1 effect instance pool. Without isolation, this command silently corrupts cross-zone state (V2 confirmed: `RendererActor::applyPendingEffectParameterUpdates` operates on the singleton via `reg->effect->setParameter()`).

**Implementation shape (Phase 1 after Spike 1+2 succeed):**
```json
{
  "type": "zone.effects.parameters.set",
  "requestId": "...",
  "zoneId": 1,
  "parameters": {"shockwaveVelocity": 0.72, "decay": 0.45}
}
```

`effectId` is **implicit** from the zone's current state — caller does not specify; ZoneComposer routes the call to whichever effect is loaded in zoneId.

Response mirrors existing `effects.parameters.set` shape: `{queued: [...], failed: [...]}`.

### Gap 7 — Snapshot save/recall (LOW leverage — Phase 3)

Per A6, deferred. SerialJSON support follows REST + WS once the snapshot system lands.

### Gap 8 — Solo / mute (LOW leverage — Phase 4)

Per A5, deferred. SerialJSON support follows once solo/mute primitives exist in ZoneComposer.

---

## Build order (Captain's directive)

> "Order them by leverage:
> 1. setLayout
> 2. per-zone setEnabled
> 3. blend
> 4. audio config
> 5. getZoneConfig/readback
> 6. zone.effects.parameters.set after D-1 succeeds"

**This document's ordering matches Captain's directive.** Notes:

- Gap 3 (blend) is already present, not a gap — kept in inventory per Captain's defensive listing.
- Gap 6 (zone.effects.parameters.set) is the highest-leverage by Captain's own ADR D-3 framing, but it is GATED on D-1 (instance pool). It cannot ship before Spike 1 succeeds. Captain's order intentionally puts it LAST in the SerialJSON parity list because it depends on architectural foundation, not because it's low-leverage.

## Phase 1 keystone path (zone.effects.parameters.set)

This is the highest-leverage path to "first-class instrument". Sequencing:

1. **Phase 0 (now):** Phase 0 B1-B7 + B2 + B5 (this doc) + D-8 + E5
2. **Spike 1 (D-1):** Effect instance isolation proven on hardware
3. **A4 implementation:** Effect instance pool in ZoneComposer + `RendererActor::getOrCreateEffectInstance()`
4. **A2′ keystone:** `setZoneParameter(zoneSlot, name, value)` in ZoneComposer; REST + WS + SerialJSON adapters
5. **Spike 2:** Per-zone parameter targeting verified
6. **iOS Phase 1.a:** EffectParameterSheet retargets with zoneId

After step 6, every effect's already-declared parameters become per-zone-tunable across REST + WS + SerialJSON. iOS, dashboard, hardware mixer all gain "live channel-strip" capability essentially for free.

---

## Cross-references

- Command matrix: [`zones-command-matrix.md`](zones-command-matrix.md)
- ADR: [`../adr/zone-composer-architecture-decisions.md`](../adr/zone-composer-architecture-decisions.md)
- Companion plan: `~/.claude/plans/zone-composer-instrument-program.md`
- F1 commit (BlendMode range check, zone.setBlend at line 680): `9d7bc261`
- Verification pass V6 source: 5-SSA context report, in conversation history

---
**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-05-01 | Claude (Phase 0 B5) | Created. SerialJSON parity inventory ordered per Captain's directive. Gap 6 (zone.effects.parameters.set) marked as Phase 1 keystone gated on D-1 — not low-leverage, deferred for architectural reason. |
| 2026-05-02 | Claude (B2 wire-format migration) | zoneId migration LANDED — wire format now 1-indexed across REST/WS/SerialJSON; per-row `zoneId` + `effectName` added to SerialJSON `zones.list` (Gap 5 row-parity portion closed; segment-geometry portion still Phase 2). |
