# Tab5.encoder — Validation Deltas + Minimum Artefacts (Grounded in Repo Code)

**Scope:** `firmware/Tab5.encoder/` (ESP32‑P4 controller).  
**Goal:** (1) delta report vs the spec doc, (2) the minimum artefacts: parameter→command table + inbound/outbound message catalogue + routing notes.

---

## 0) High-signal deltas (doc/spec vs code)

### D0.1 I2C topology
- **Spec doc intent:** Unit A on SDA/SCL 53/54; Unit B on secondary bus SDA/SCL 49/50.
- **Code reality:** `src/main.cpp` uses **one external bus** (`Wire.begin(M5.Ex_I2C.getSDA(), getSCL())`) and talks to **two ROTATE8 devices on that same bus** via two addresses:
  - Unit A: `0x42`
  - Unit B: `0x41`
- `src/config/Config.h` still defines EXT2 pins (49/50) but they are not used in the observed boot wiring.

### D0.2 Effect ID range inconsistency inside Tab5
There are competing “truths”:
- `src/config/Config.h`: effect max = **103** (0–103)
- `src/parameters/ParameterMap.cpp`: effect max = **99** (0–99)
- `src/main.cpp`: comments mention enforcing <=99, but name lookup doesn’t actually clamp.

**Consequence:** encoder clamping / wrap behaviour can disagree depending on which code path is active.

### D0.3 Zones WS request type mismatch (naming)
- `WebSocketClient::requestZonesState()` sends **`{"type":"zones.get"}`**.
- `WsMessageRouter` expects to handle **`"zones.list"`** messages (and also handles `zone.status`, `zones.changed`).

**Implication:** v2 must respond to `zones.get` with a payload whose `type` is `zones.list` (or Tab5 router needs to handle `zones.get` responses directly).

### D0.4 WiFi / host fallback default
- `src/config/network_config.h` defines `LIGHTWAVE_IP "192.168.4.1"` *enabled by default*.
- That bypasses mDNS (unless overridden elsewhere).

---

## 1) Minimum artefact A — Parameter→Command Table (0..15)

> **Source of truth for mapping:** `firmware/Tab5.encoder/src/parameters/ParameterMap.cpp` (PARAMETER_TABLE) + `ParameterHandler.cpp` send logic + `main.cpp` fallback send logic.

### Encoders 0–7 (Unit A: Global)

| Index | Name | Status field (inbound `status`) | Outbound WS type | Payload key | Range (current in ParameterMap) | Default |
|---:|---|---|---|---|---|---:|
| 0 | Effect | `effectId` | `effects.setCurrent` | `effectId` | 0–99 | 0 |
| 1 | Palette | `paletteId` | `parameters.set` | `paletteId` | 0–74 | 0 |
| 2 | Speed | `speed` | `parameters.set` | `speed` | 1–100 | 25 |
| 3 | Mood | `mood` | `parameters.set` | `mood` | 0–255 | 0 |
| 4 | Fade Amount | `fadeAmount` | `parameters.set` | `fadeAmount` | 0–255 | 0 |
| 5 | Complexity | `complexity` | `parameters.set` | `complexity` | 0–255 | 128 |
| 6 | Variation | `variation` | `parameters.set` | `variation` | 0–255 | 0 |
| 7 | Brightness | `brightness` | `parameters.set` | `brightness` | 0–255 | 128 |

### Encoders 8–15 (Unit B: Zones)

| Index | Name | Status field (inbound `status`) | Outbound WS type (default) | Payload keys | Range (current in ParameterMap) | Default |
|---:|---|---|---|---|---|---:|
| 8 | Zone0 Effect | `zone0Effect` | `zone.setEffect` | `zoneId=0, effectId` | 0–99 | 0 |
| 9 | Zone0 Speed (or Palette mode) | `zone0Speed` | `zone.setSpeed` (or `zone.setPalette`) | `zoneId=0, speed` OR `paletteId` | 1–100 | 25 |
| 10 | Zone1 Effect | `zone1Effect` | `zone.setEffect` | `zoneId=1, effectId` | 0–99 | 0 |
| 11 | Zone1 Speed (or Palette mode) | `zone1Speed` | `zone.setSpeed` (or `zone.setPalette`) | `zoneId=1, speed` OR `paletteId` | 1–100 | 25 |
| 12 | Zone2 Effect | `zone2Effect` | `zone.setEffect` | `zoneId=2, effectId` | 0–99 | 0 |
| 13 | Zone2 Speed (or Palette mode) | `zone2Speed` | `zone.setSpeed` (or `zone.setPalette`) | `zoneId=2, speed` OR `paletteId` | 1–100 | 25 |
| 14 | Zone3 Effect | `zone3Effect` | `zone.setEffect` | `zoneId=3, effectId` | 0–99 | 0 |
| 15 | Zone3 Speed (or Palette mode) | `zone3Speed` | `zone.setSpeed` (or `zone.setPalette`) | `zoneId=3, speed` OR `paletteId` | 1–100 | 25 |

**Mode routing:** `ParameterHandler::sendParameterChange()` switches zone speed encoder to `zone.setPalette` if `ButtonHandler` says that zone is in PALETTE mode.

---

## 2) Minimum artefact B — WebSocket message catalogue (Tab5 side)

### Outbound (Tab5 → v2)

**Handshake / sync**
- `getStatus` (no payload)
- `zones.get` (no payload)
- `effects.list` `{page, limit, details:false, requestId?}`
- `palettes.list` `{page, limit, requestId?}`
- `colorCorrection.getConfig` (no payload)

**Global params**
- `effects.setCurrent` `{effectId}`
- `parameters.set` subset payloads:
  - `{brightness}` `{speed}` `{paletteId}` `{mood}` `{fadeAmount}` `{complexity}` `{variation}`

**Zones**
- `zone.setEffect` `{zoneId, effectId}`
- `zone.setSpeed` `{zoneId, speed}`
- `zone.setPalette` `{zoneId, paletteId}`
- `zone.setBlend` `{zoneId, blendMode}`
- `zone.setBrightness` `{zoneId, brightness}` (present in client, may not be used)
- `zones.setLayout` `{zones:[{zoneId,s1LeftStart,s1LeftEnd,s1RightStart,s1RightEnd}, ...]}`

**Colour correction**
- `colorCorrection.setConfig` `{gammaEnabled,gammaValue,autoExposureEnabled,autoExposureTarget,brownGuardrailEnabled,mode}`
- `colorCorrection.setMode` `{mode}`

### Inbound (v2 → Tab5) — what Tab5 actually routes today

Handled by `firmware/Tab5.encoder/src/network/WsMessageRouter.h`:
- `status` (bulk sync)
- `device.status` (uptime, informational)
- `parameters.changed` (notification)
- `zone.status` (zone sync variant)
- `zones.changed` (triggers `zones.get` request)
- `zones.list` (expected response shape for zone state)
- `effects.changed` (notification)
- `colorCorrection.getConfig` (response; may contain `data` object)

**Note:** `WsMessageRouter` currently expects `zones.list` as the payload `type` for a zone listing response.

---

## 3) Minimum artefact C — UI routing (encoders vs screens)

From `src/main.cpp:onEncoderChange()`:
- If UI screen is `UIScreen::ZONE_COMPOSER`, encoder deltas are routed to `ZoneComposerUI::handleEncoderChange(index, delta)`.
- Otherwise:
  - encoders update ParameterHandler state
  - UI updated via `DisplayUI::updateValue(index, value)`
  - WebSocketClient sends global/zone commands (with button-mode logic for zone speed/palette).

---

## 4) Follow-up fixes (recommended)

1) **Unify effect count handling:** Tab5 should not hardcode 99 if v2 is now 101 effects (and max slot is 104). Best: ingest `effectCount` from status and call `updateParameterMetadata(EffectIndex, 0, effectCount-1)`.
2) **Unify zones response naming:** either v2 returns `type:"zones.list"` for `zones.get`, or Tab5 accepts the actual response type.
3) **Confirm I2C design:** either update docs to “single bus + dual addresses” or reintroduce the second external bus.
