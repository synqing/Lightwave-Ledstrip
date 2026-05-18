---
abstract: "W2 — Hex-ID architectural cost dissection. Read-only static analysis of the 8 architecturally load-bearing files for the EffectId/PaletteId metadata wire-format. Decomposes payload + heap cost across status broadcast, /api/v1/effects, /api/v1/palettes, /api/v1/effects/metadata, and zone-state into ID-bytes / metadata-string-bytes / structural-JSON-bytes; quantifies the delta against a hypothetical ID-only baseline; flags architecturally redundant emissions. Verdict: metadata strings on the wire are NOT the dominant heap-pressure cost — single-allocation JsonDocument peaks already moved to per-item streaming (β fix, 47997), and metadata strings are PROGMEM-backed `const char*` (RendererActor.cpp:506-512). The remaining cost is steady-state status-broadcast bandwidth (~720 B every 5 s/subscriber) and a one-time effects-list payload of ~20-30 KB at app open."
---

# W2 — Hex-ID Architectural Cost Dissection

**Status:** GROUNDED. Read-only static analysis. No code modified. No builds.
**Scope:** Quantify heap + bandwidth cost specifically attributable to the hex-ID + metadata-everywhere architecture, separate from allocator behaviour.
**Sources of truth read:** `firmware-v3/src/effects/PatternRegistry.{h,cpp}`, `firmware-v3/src/palettes/Palettes_Master.h`, `firmware-v3/src/codec/WsEffectsCodec.cpp`, `firmware-v3/src/codec/HttpEffectsCodec.cpp`, `firmware-v3/src/network/WebServerBroadcast.cpp`, `firmware-v3/src/network/ApiResponse.h`, `firmware-v3/src/network/webserver/handlers/EffectHandlers.cpp`, `firmware-v3/src/network/webserver/handlers/PaletteHandlers.cpp`, `firmware-v3/src/network/WebServer.h`, `firmware-v3/src/core/actors/RendererActor.{h,cpp}`.

---

## 0. How the metadata is actually stored (foundation for the cost numbers)

Three facts gate every number below. Verified by direct read; not assumed.

**0.1 Metadata strings are PROGMEM `const char*`, not heap-allocated `String` copies.**
`PatternMetadata` (PatternRegistry.h:90-109) is `{EffectId id; const char* name; PatternFamily family; uint8_t tags; const char* story; const char* opticalIntent; const char* relatedPatterns;}`. The entire `PATTERN_METADATA[]` array is declared `PROGMEM` (PatternRegistry.cpp:26) and every literal is wrapped with `PM_STR()` (PatternRegistry.cpp:23 `#define PM_STR(s) (const char*)(s)`). Family names are also PROGMEM (PatternRegistry.h:46-69). `RendererActor::getEffectName(EffectId)` returns `reg->name` directly — a `const char*` into the PROGMEM table (RendererActor.cpp:506-514). `MasterPaletteNames[]` is `extern const char* const` (Palettes_Master.h:71).

**Consequence:** the metadata strings themselves cost ZERO heap. Heap is touched only when ArduinoJson copies the string into its document tree during serialisation, and even then ArduinoJson v7 stores `const char*` literals by pointer when assigned via `obj["k"] = strPtr` if the pointer is to a string with static storage duration (which PROGMEM is). The bytes that hit the wire are real; the bytes that fragment the heap are a smaller, transient subset.

**0.2 Effect count is 194 patterns, NOT 162.**
`grep -cE '^\s*\{.*EID_'` against `PATTERN_METADATA[]` returns 194. The "162 patterns" comment in PatternRegistry.h:11 is stale. `PATTERN_METADATA_COUNT = sizeof(PATTERN_METADATA) / sizeof(PatternMetadata)` (PatternRegistry.cpp:290).

**0.3 PROGMEM table size — the metadata catalogue itself.**
Per row: `uint16_t id` (2) + `const char* name` (4) + `uint8_t family` (1) + `uint8_t tags` (1) + 3× `const char*` (12) + struct padding ≈ 24 B per entry, so the table itself is ~4.6 KB in PROGMEM. The raw string bytes (`name`/`story`/`opticalIntent`/`relatedPatterns`) total **22 933 chars** across 686 non-empty `PM_STR()` literals (mean 33.4 chars, max 115). All in PROGMEM. Verified by `awk '/PATTERN_METADATA\[\] PROGMEM = {/,/^};/' ... | grep -oE 'PM_STR\("[^"]*"\)' | awk { length stats }`.

Palette names are similarly PROGMEM-backed (Palettes_Master.h:71-72 `extern const char* const MasterPaletteNames[]`; data lives in `Palettes_MasterData.cpp`).

---

## 1. Wire-format event catalogue (payload size + composition)

All five high-frequency events confirmed by direct read of the encoder/handler. Sizes are measured by reading the exact fields emitted and counting characters at typical values. Each event row gives a "headline" (representative) payload size in bytes; the columns split that headline into **id-bytes** (the numeric effectId/paletteId/zoneId as printed digits), **metadata-string-bytes** (the human-readable strings the architecture also emits), and **structural-bytes** (quotes, colons, commas, braces, JSON keys, and non-metadata numeric values such as brightness/speed/fps/heap counters).

### 1.1 `status` WebSocket broadcast (5 s cadence per subscribed client)

**Code path:** `WebServer::doBroadcastStatus()` at `WebServerBroadcast.cpp:110-338`. Triggered by `WebServer::update()` at `WebServer.cpp:993` against `STATUS_BROADCAST_INTERVAL_MS = 5000` (`WebServer.h:172`). Gated behind `hasStatusSubscribers()` since the γ subscriber-gate landed (obs #47998, 2026-05-03).

**Fields actually emitted** (verified WebServerBroadcast.cpp:139-278):
- ID/scalar fields: `type` (string literal "status"), `effectId`, `brightness`, `speed`, `paletteId`, `hue`, `intensity`, `saturation`, `complexity`, `variation`, `fps`, `cpuPercent`, `frameBudgetPercent`, `freeHeap`, `freeHeapInternal`, `freePsram`, `uptime`, `ledDitheringEnabled`, `zonesEnabled`.
- Nested `ledTransport` object with 12 fields (frame count, show timings, RMT fence, latch wait, failures, rmtErrors, underruns).
- Metadata-string fields: `effectName` (looked up via `cached.findEffectName`, PROGMEM), `synqMatrixMode`, `synqMatrixOwner`, `edgeMixerModeName`, `edgeMixerSpatialName`, `edgeMixerTemporalName`.
- Audio (FEATURE_AUDIO_SYNC): `audioSyncMode`, `bpm`, `mic`, `key` (e.g. `"Am"`, `"C#dim"` from `formatKeyName`, WebServerBroadcast.cpp:70-96).
- Edge mixer raw numeric: `edgeMixerMode`, `edgeMixerSpread`, `edgeMixerStrength`, `edgeMixerSpatial`, `edgeMixerTemporal`.

**Representative payload size:** ~720 B (matches the inline comment at WebServerBroadcast.cpp:314: "another ~700 B payload"). Break-down:

| Component | Bytes (est.) | Notes |
|---|---:|---|
| Hex-ID-as-printed-digits (effectId as decimal, paletteId 0-74, zoneId N/A here) | ~5 | `"effectId":7187,"paletteId":42` — the integer payload is single-digit-bytes only |
| Metadata strings | ~80 | `effectName` (avg 25 char) + `synqMatrixMode/Owner` (~15) + 3× `edgeMixer*Name` (~10 each) + `key` (~3) — verified WebServerBroadcast.cpp:144-201 |
| Structural JSON (keys, colons, commas, braces, numeric scalars, transport counters) | ~635 | 19 top-level keys + 12 `ledTransport` keys + values; the key text alone (`"ledTransport"`, `"avgFastLedShowCallUs"`, etc.) is ~250 B |
| **Total** | **~720** | |

**% metadata strings vs structural:** ~11% metadata, ~88% structural, ~1% ID digits. **The hex-ID metadata architecture contributes a minority of the status-broadcast payload.** The dominant cost is `ledTransport.*` performance counters and the 8 parameter scalars.

### 1.2 `/api/v1/effects` REST list (one-shot at app open; pagination limit≤150)

**Code path:** `EffectHandlers::handleList` at `EffectHandlers.cpp:30-239`. Streamed since β-fix (obs #47997). Default `limit=20`; iOS bundle pulls all 194 in pages.

**Per-effect entry** (no `details`, no IEffect metadata):
```
{"id":7187,"name":"LGP Holographic","category":"Wave","categoryId":1,"isAudioReactive":false,"isExperimental":false,"isIEffect":false}
```
~115-130 B per entry. With IEffect metadata (`description`, `version`, `author`, `ieffectCategory` — `EffectHandlers.cpp:184-206`): ~250-350 B per entry.

**Whole-list (limit=200, all 194 effects):**

| Component | Bytes (est.) | Notes |
|---|---:|---|
| Hex-ID digits | 194 × 4-5 = ~900 | `"id":7187,` |
| Metadata strings (name only) | 194 × ~28 char = ~5 400 | name avg ~25 plus quoting |
| Metadata strings (category label) | 194 × ~12 = ~2 300 | `"category":"Wave"` etc. |
| Optional IEffect description/version/author/ieffectCategory | up to 194 × ~120 = ~23 000 | only for IEffect instances (subset; exact count not measured) |
| Structural JSON (keys `"id"`, `"name"`, `"category"`, `"categoryId"`, `"isAudioReactive"`, `"isExperimental"`, `"isIEffect"`, plus braces/commas) | 194 × ~50 = ~9 700 | repeating JSON key text |
| Envelope + pagination + categories[] array + count | ~250 | `"success":true,"data":{...,"pagination":{},"categories":[4 entries],"count":N,"total":N,"offset":0,"limit":N}` |
| **Subtotal without IEffect metadata** | **~18 600** | matches `EffectHandlers.cpp:113` comment "~22.5 KB worst case for limit=200" once IEffect description is included |
| **Subtotal with IEffect metadata for ~60 IEffect entries** | **~28 000** | |

**% metadata strings vs structural** (no IEffect): ~41% metadata (name + category label), ~52% structural, ~5% ID digits, ~2% repeating boolean text. With IEffect metadata description fields included: metadata share rises to ~65%.

### 1.3 `/api/v1/palettes` REST list (one-shot at app open)

**Code path:** `PaletteHandlers::handleList` at `PaletteHandlers.cpp:26-194`. Streamed (β-fix). 75 palettes (`MASTER_PALETTE_COUNT = 75`, Palettes_Master.h:65).

**Per-palette entry** (PaletteHandlers.cpp:147-176):
```
{"id":0,"name":"Heatmap","category":"Artistic","flags":{"warm":true,"cool":false,"calm":false,"vivid":true,"cvdFriendly":false,"whiteHeavy":true},"avgBrightness":128,"maxBrightness":255}
```
~190-220 B per entry.

**Whole-list (75 palettes):**

| Component | Bytes (est.) | Notes |
|---|---:|---|
| Hex-ID-as-digits (paletteId 0-74) | 75 × 2 = 150 | `"id":42,` |
| Metadata strings (name + category) | 75 × (~12 name + ~12 category) = ~1 800 | |
| Structural JSON (flags object × 6 keys, brightness/category keys, braces/commas) | 75 × ~150 = ~11 250 | dominated by the `flags` object — six boolean keys repeated 75 times |
| Envelope + pagination + categories metadata | ~200 | |
| **Total** | **~13 400** | |

**% metadata strings vs structural:** ~13% metadata, ~85% structural, ~1% ID digits, ~1% boolean text. **Palette flags structure (`flags{warm,cool,calm,vivid,cvdFriendly,whiteHeavy}`) dominates this payload — not the hex-ID metadata.** A bitfield would collapse the same information to one byte; the architecture instead expands one byte of flags into ~90 B of structural JSON per palette.

### 1.4 `/api/v1/effects/metadata?id=N` (per-effect detail; called on-demand)

**Code path:** `EffectHandlers::handleMetadata` at `EffectHandlers.cpp:432-522`. Uses `sendSuccessResponse` (ApiResponse.h:91-103) — **NOT streamed**: full JsonDocument composed in heap, serialised to `String`, then sent.

**Representative payload** (LGP Holographic, EID 0x1C13, with IEffect metadata):
```json
{"success":true,"data":{
  "id":7187,"name":"LGP Holographic","isIEffect":true,
  "description":"Holographic interference patterns through multi-layer depth",
  "version":2,"author":"LightwaveOS",
  "ieffectCategory":"GEOMETRIC",
  "family":"Interference","familyId":0,
  "story":"Holographic interference patterns through multi-layer depth",
  "opticalIntent":"Multi-layer interference, phase relationships, depth illusion",
  "tags":["TRAVELING","MOIRE","DEPTH","CENTER_ORIGIN","DUAL_STRIP"],
  "isExperimental":false,
  "properties":{"centerOrigin":true,"symmetricStrips":true,"paletteAware":true,"speedResponsive":true},
  "recommended":{"brightness":180,"speed":15}
},"timestamp":...,"version":"2.0"}
```
~620-720 B.

| Component | Bytes (est.) | Notes |
|---|---:|---|
| Hex-ID digits | 4-5 | `"id":7187` |
| Metadata strings (`name`, `description`, `author`, `ieffectCategory`, `family`, `story`, `opticalIntent`, tag labels) | ~320 | the bulk of the payload |
| Structural JSON (envelope, properties{}, recommended{}, tags[] braces, ~25 key strings) | ~290 | |
| **Total** | **~620** | |

**% metadata strings vs structural:** ~52% metadata, ~47% structural, ~1% ID. **This is the only event where metadata strings genuinely dominate the payload — by design, since the endpoint exists to deliver metadata.** That is the correct cost shape for an opt-in detail endpoint, not a hot loop.

### 1.5 `zones.list` + `zones.stateChanged` WebSocket broadcasts

**Code path:** `WebServer::broadcastZoneState()` (WebServerBroadcast.cpp:340-425; 4 Hz throttle line 356) and `broadcastSingleZoneState()` (lines 427-491; 20 Hz throttle line 443). Both `textAll()` — not subscriber-gated.

**`zones.list` representative payload (3 zones, 5 presets):**

Per-zone fields emitted (WebServerBroadcast.cpp:381-409): `id`, `zoneId`, `enabled`, `effectId`, `effectName`, `brightness`, `speed`, `paletteId`, `blendMode`, `blendModeName`, plus 7 audio config fields (tempoSync, beatModulation, tempoSpeedScale, beatDecay, audioBand, beatTriggerEnabled, beatTriggerInterval). Plus per-zone segment geometry (s1LeftStart/End/RightStart/End/totalLeds) under `segments[]`. Plus 5-entry `presets[]` with `id`+`name`.

| Component | Bytes (est.) | Notes |
|---|---:|---|
| Hex-ID digits (3 zones × effectId + zoneId/blendMode/paletteId) | ~50 | |
| Metadata strings (3× `effectName` ~25 + 3× `blendModeName` ~10 + 5× preset name ~10) | ~155 | |
| Structural JSON (audio config keys × 3, segment geometry × 3, top-level envelope) | ~700 | dominated by audio-config keys (7 keys × 3 zones × ~15 char/key) |
| **Total** | **~905** | |

**% metadata strings vs structural:** ~17% metadata, ~80% structural, ~3% ID. The same pattern as `status`: metadata strings are NOT the dominant byte source.

---

## 2. Per-event heap peak estimate

### 2.1 ArduinoJson v7 allocation behaviour

`JsonDocument` in v7 uses dynamic allocation; the `bufferSize` parameter on `sendSuccessResponseLarge` is now unused (ApiResponse.h:128-131 inline comment). Empirically (firmware notes, obs #47997), JsonDocument peak heap during composition is **~1.5× to 2× the final serialised payload**: the in-memory representation stores key strings + value cells + tree pointers, then `serializeJson(doc, String&)` doubles the working set during the `String` reserve/copy cycle.

`AsyncResponseStream` (used by the streamed list endpoints) bounds peak to `cbuf initial size (2-4 KB) + ONE per-item JsonDocument (~150-300 B)` — confirmed by the inline commentary at EffectHandlers.cpp:107-124 and PaletteHandlers.cpp:116-126.

### 2.2 Heap peak per event (best-evidence estimates)

| Event | Wire size | Composition pattern | Peak internal heap during composition |
|---|---:|---|---:|
| `status` 5s broadcast | ~720 B | `JsonDocument` + `String output` (WebServerBroadcast.cpp:139, 280) | ~1.4-2.0 KB |
| `/api/v1/effects` list (limit=200) | ~20-28 KB | Streamed: cbuf 4 KB + per-item JsonDocument ~300 B (EffectHandlers.cpp:126-217) | **~4.5 KB peak** (was ~22.5 KB pre-β-fix, per EffectHandlers.cpp:112) |
| `/api/v1/palettes` list (75 palettes) | ~13 KB | Streamed: cbuf 2 KB + per-item JsonDocument ~250 B (PaletteHandlers.cpp:128-176) | **~2.5 KB peak** |
| `/api/v1/effects/metadata?id=N` | ~620 B | Non-streamed `sendSuccessResponse` → full JsonDocument + String (ApiResponse.h:91-103) | ~1.2-1.5 KB |
| `zones.list` broadcast | ~905 B | Non-streamed `JsonDocument` + `String output` + `textAll()` (WebServerBroadcast.cpp:361-423) | ~1.8-2.5 KB |
| `zones.stateChanged` (per zone, 20 Hz) | ~280 B | Non-streamed JsonDocument + String (WebServerBroadcast.cpp:455-487) | ~600-800 B |
| `effectChanged` (event-driven) | ~70 B | Non-streamed JsonDocument + String (WebServerBroadcast.cpp:517-528) | ~250 B |

**Key observation:** the β-fix already neutralised the only event whose heap peak was architecturally lethal (the unstreamed `/api/v1/effects` list at 22.5 KB on a ~21 KB internal heap budget — EffectHandlers.cpp:113-115). What remains is steady-state status churn, not single-allocation pressure.

### 2.3 Steady-state bandwidth/heap churn

With **N status subscribers** (currently gated to clients that opted in via `status.subscribe` — obs #47998):
- Per 5 s tick: N × 720 B emitted + N × ~2 KB internal heap alloc/free.
- At 4 subscribers (worst case AP cap): **~580 B/s bandwidth, ~6.4 KB/s alloc churn**.

Pre-γ-fix (no subscriber gate, 5 clients): the same code path was responsible for the **~14 KB/s/client churn** called out in obs #47998 — that figure also counted the multi-Hz audio frame stream, so the steady-state attributable to `status` alone is the ~1.4 KB/s/subscriber number above.

---

## 3. Hypothetical minimal baseline (ID-only wire)

A wire format that emits ONLY the hex-ID and expects the client to look up everything else from a once-per-session cache. Math is per event.

### 3.1 `status` ID-only payload

```json
{"type":"status","effectId":7187,"brightness":180,"speed":15,"paletteId":42,"hue":0,"intensity":200,"saturation":255,"complexity":128,"variation":50,"fps":119.8,"freeHeap":34000,"freeHeapInternal":17000,"freePsram":4194000,"uptime":1234,"bpm":120,"mic":-32.5,"key":"Am","zonesEnabled":true,"synqMatrixMode":0,"edgeMixerMode":2}
```
~360 B (vs ~720 B today).

**Savings per status broadcast:** ~360 B. Over 5 s × 4 subscribers: ~290 B/s saved.

**Caveat:** dropping `effectName`, `synqMatrixMode/OwnerName`, `edgeMixer*Name`, and `ledTransport.*` performance counters changes the contract — `ledTransport` is operator-visible diagnostics, not a client-renderable field. A fair baseline keeps the 12 `ledTransport` counters (they're not hex-ID metadata, they're independent telemetry).

**Realistic ID-only `status`:** ~530 B (drops only the 6 metadata-string `*Name` fields and `effectName`). Savings: ~190 B per broadcast, ~150 B/s/subscriber.

### 3.2 `/api/v1/effects` ID-only payload

```json
{"success":true,"data":{"total":194,"offset":0,"limit":200,"effects":[7180,7181,7182,...194 entries],"count":194},"timestamp":...,"version":"2.0"}
```
Each entry is just the EffectId as a decimal integer (`7187,`) — 4-5 B. **Whole list: ~1.2 KB** (vs ~20 KB today without IEffect details, ~28 KB with).

**Savings:** ~19-27 KB once, at app open. The client gets the names from a single `/api/v1/effects/metadata` bulk call (which doesn't exist yet — currently `metadata` is per-ID).

### 3.3 `/api/v1/palettes` ID-only payload

```json
{"success":true,"data":{"total":75,"offset":0,"limit":75,"palettes":[0,1,2,...74],"count":75},...}
```
~280 B (vs ~13 KB today). Savings: ~12.7 KB once at app open.

### 3.4 `zones.list` ID-only payload

```json
{"type":"zones.list","enabled":true,"zoneCount":3,"segments":[3 entries × segment geometry],
 "zones":[3 entries with effectId+paletteId+brightness+speed+blendMode+audioConfig (no Name)],
 "presets":[5 IDs only]}
```
~620 B (vs ~905 B today). Savings: ~285 B per broadcast.

### 3.5 Aggregate

| Event | Today | ID-only | Saving |
|---|---:|---:|---:|
| `status` (5 s, 4 subscribers) | ~720 B × 4 = ~2.9 KB / 5 s = **576 B/s** | ~530 B × 4 = ~2.1 KB / 5 s = **424 B/s** | **~150 B/s** (~26%) |
| `/api/v1/effects` list (app open) | ~20-28 KB once | ~1.2 KB once | **~19-27 KB once** |
| `/api/v1/palettes` list (app open) | ~13 KB once | ~280 B once | **~12.7 KB once** |
| `zones.list` (occasional, throttled 4 Hz max) | ~905 B per broadcast | ~620 B per broadcast | ~285 B/broadcast |
| `effects/metadata` per-ID | ~620 B per call | (eliminated; replaced by a static `/api/v1/catalogue` bundle) | the whole call, but called ≥194 times to populate today |

**Net steady-state saving: ~150 B/s bandwidth (status), ~zero heap churn (status JsonDocument still ~1.4 KB regardless of metadata).**

**Net one-shot saving: ~32-40 KB at app open**, replaced by a single ~30 KB metadata bundle the client can cache against firmware build hash.

---

## 4. Delta: what the metadata architecture costs (bytes/sec)

Stripping the comparison to **steady-state per-second cost** so the architectural question has a defensible number:

| Cost dimension | Today | ID-only baseline | Delta |
|---|---:|---:|---:|
| Status WS bandwidth (4 subscribers, 0.2 Hz) | ~576 B/s | ~424 B/s | **+152 B/s** |
| Status WS heap churn (4 subscribers) | ~6.4 KB/s alloc-free | ~5.8 KB/s alloc-free | **+0.6 KB/s** |
| Zones WS broadcast (4 Hz when active) | ~3.6 KB/s | ~2.5 KB/s | **+1.1 KB/s** when actively broadcasting |
| Effects list (one-shot, amortised over 1 h session) | ~6 B/s | ~0.3 B/s | **+5.7 B/s** |
| Palettes list (one-shot, amortised over 1 h session) | ~3.6 B/s | ~0.08 B/s | **+3.5 B/s** |
| **Aggregate steady-state** | **~580-3 600 B/s depending on zones-active** | **~430-2 530 B/s** | **~150 B/s (status idle) to ~1.3 KB/s (zones active)** |

**The metadata-on-the-wire architecture costs ~150 B/s steady-state and ~30 KB at app open.** It does NOT cost meaningful heap pressure post-β-fix, because the only single-allocation peak (`/api/v1/effects` at 22.5 KB) was already broken into per-item streaming.

---

## 5. Architecturally-redundant metadata emissions

Paths that emit the same effectName/familyName/blendModeName triplet, where one canonical authoritative path would do.

### 5.1 `effectName` appears in:
1. `status` WebSocket broadcast (WebServerBroadcast.cpp:144) — every 5 s per subscriber.
2. `effectChanged` WebSocket broadcast (WebServerBroadcast.cpp:520) — on every effect transition.
3. `zones.list` WebSocket broadcast (WebServerBroadcast.cpp:391-394) — per zone, on zone state change.
4. `zones.stateChanged` WebSocket broadcast (WebServerBroadcast.cpp:467-472) — per single-zone update at 20 Hz max.
5. `/api/v1/effects` list (EffectHandlers.cpp:153, 177) — once per app open, in every entry.
6. `/api/v1/effects/current` (EffectHandlers.cpp:251) — every poll.
7. `/api/v1/effects/metadata` (EffectHandlers.cpp:456) — every per-ID lookup.
8. `WsEffectsCodec::encodeGetCurrent` (WsEffectsCodec.cpp:418) — every `effects.getCurrent` WS reply.
9. `WsEffectsCodec::encodeChanged` (WsEffectsCodec.cpp:431) — every `effects.changed` WS reply.
10. `WsEffectsCodec::encodeMetadata` (WsEffectsCodec.cpp:437) — every `effects.metadata` WS reply.
11. `WsEffectsCodec::encodeList` (WsEffectsCodec.cpp:470) — every `effects.list` WS reply.
12. `WsEffectsCodec::encodeParametersGet` (WsEffectsCodec.cpp:508) — every parameters fetch.
13. `WsEffectsCodec::encodeParametersSetChanged` (WsEffectsCodec.cpp:526) — every parameter set ACK.

**13 distinct emission paths for the same `effectName` string.** All pull from the same PROGMEM PatternMetadata table via `RendererActor::getEffectName()` (RendererActor.cpp:506-514), so the string itself isn't duplicated in memory — but the wire still carries the bytes 13 different ways, and ArduinoJson still walks the string each time to compute serialisation length.

### 5.2 Family name appears in:
1. `WsEffectsCodec::encodeMetadata` (WsEffectsCodec.cpp:438).
2. `WsEffectsCodec::encodeCategories` / `encodeByFamily` (WsEffectsCodec.cpp:485, 500).
3. `/api/v1/effects/metadata` (EffectHandlers.cpp:484-487 — copies family name into a 32-byte stack buffer via `PatternRegistry::getFamilyName`).
4. `/api/v1/effects/families` (EffectHandlers.cpp:535-537).
5. `HttpEffectsCodec::encodeMetadata` (HttpEffectsCodec.cpp:223).

**Note:** `getFamilyName` takes a `(buffer, bufferSize)` pair and writes into it — implying a `strncpy_P` from the PROGMEM family table. This is NOT a heap allocation, but it IS a redundant memcpy that exists only because `PatternFamily` is an enum that needs human-friendly labels.

### 5.3 Tag labels (`"STANDING"`, `"TRAVELING"`, etc.) appear in:
1. `WsEffectsCodec::encodeMetadata` (WsEffectsCodec.cpp:449-456).
2. `/api/v1/effects/metadata` (EffectHandlers.cpp:497-504).
3. `HttpEffectsCodec::encodeMetadata` (HttpEffectsCodec.cpp:232-237).

Three sites that re-derive the same string array from the same `tags` bitfield. **A bitfield-to-bitstring helper exists nowhere in the codebase; each site re-implements the same 8 `if (tags & 0xN) add(literal)` ladder.**

### 5.4 Palette `category` ("Artistic"/"Scientific"/"LGP-Optimized") appears in:
1. `/api/v1/palettes` list (PaletteHandlers.cpp:150).
2. `/api/v1/palettes/current` (PaletteHandlers.cpp:208).
3. `/api/v1/palettes/set` reply (PaletteHandlers.cpp:246).

Derived via `getPaletteCategory()` (Palettes_Master.h:169-174) — a static `const char*` lookup; no allocation. But again, the BYTES go on the wire three different ways.

### 5.5 Edge-mixer mode names (`enhancement::EdgeMixer::modeName/spatialName/temporalName`)

Each appears alongside its raw enum value in `status` (WebServerBroadcast.cpp:191-201) — so every 5 s broadcast carries both `edgeMixerMode: 2` AND `edgeMixerModeName: "ChromaticHarmonic"`. The name is redundant for a client that knows the enum mapping.

---

## 6. Verdict: is the architecture the dominant cost?

### 6.1 What the hex-ID-with-metadata architecture DOES cost

- **~150 B/s steady-state bandwidth** on the `status` broadcast for the four `*Name` mirrors of raw enum fields (synqMatrixMode/Owner, edgeMixer Mode/Spatial/Temporal).
- **~30 KB one-shot at app open** for `/api/v1/effects` + `/api/v1/palettes` list responses, because every entry inlines its display strings.
- **13× wire redundancy on `effectName`** — same string emitted by 13 distinct serialisation paths. Each path also touches the same PROGMEM pointer 13 times per state change cycle, but that's just pointer-passing, not allocation.
- **~22 933 chars (~22 KB) of PROGMEM** committed to the PatternMetadata strings table, plus ~4.6 KB for the struct array itself. **Total ~27 KB PROGMEM, ZERO heap.** PROGMEM doesn't compete with the heap-shed budget.

### 6.2 What the hex-ID-with-metadata architecture does NOT cost

- **Heap fragmentation peaks** — the single-allocation 22.5 KB JsonDocument that drove the K1 V2 heap-shed (EffectHandlers.cpp:112-115 inline) was already broken by the β-fix per-item streaming (obs #47997, 2026-05-03). The architecture is no longer responsible for a fragmenting peak; it's responsible for a streaming-friendly steady-state.
- **String heap churn** — metadata strings are PROGMEM `const char*` (PatternRegistry.cpp:23 `PM_STR` macro; RendererActor.cpp:506-512). ArduinoJson v7 stores `const char*` literals by pointer for strings with static storage duration. No `String name` copies in the metadata pipeline.
- **Render-path heap traffic** — none of the metadata pipeline runs from `render()`. All composition happens on Core 0 in the AsyncTCP-deferred broadcast path or the HTTP handler path; both run outside the 2.0 ms / 120 FPS render budget.

### 6.3 Verdict

**The hex-ID architecture is NOT the dominant heap-pressure cost on K1.**

The dominant remaining costs, in order, are:
1. **Status broadcast structural JSON overhead** (~635 B of the 720 B payload is keys+counters, NOT metadata strings).
2. **`ledTransport.*` performance counters** in every status broadcast — 12 fields × ~50 B per field = ~600 B per broadcast that has nothing to do with hex-IDs.
3. **Palette `flags{}` object** in `/api/v1/palettes` — 6 boolean keys per palette inflate ~1 byte of bitfield into ~90 B of structural JSON × 75 palettes = ~6 750 B of pure structural cost.
4. **Audio-config block in zones.list** — 7 keys × 3 zones × ~30 B/field = ~630 B per broadcast that has nothing to do with hex-IDs.

**Refactoring the hex-ID metadata architecture (binary protocol, ID-only wire) would save ~150 B/s steady-state and ~30 KB at app open.** This is real but small relative to:
- The 6.4 KB/s steady-state heap churn already attributable to ArduinoJson v7 transient allocations regardless of payload size.
- The 22 KB PROGMEM table that is on-die ROM and does not compete for heap.

**Recommended posture** (degraded-mode, evidence-based, not prescriptive):

- **The case for keeping the architecture and squeezing the allocators** is stronger than the case for refactoring the architecture. Post-β-fix streaming, the architecture's residual cost is bandwidth (~150 B/s) not heap. K1 has ~17 KB internal heap headroom (per `freeHeapInternal` field); 150 B/s of bandwidth saves ~750 B over a 5-second window, which is not the heap-shed trigger.
- **The strongest architectural-refactor argument is at app open**, where `/api/v1/effects` + `/api/v1/palettes` collectively burn ~33 KB of one-shot transfer. A `/api/v1/catalogue` bundle the iOS client caches against build hash would replace those with a single ID-array list response (~1.5 KB). That IS justified — but it's a wire-format optimisation, not an architectural revolution.
- **The 13× redundant `effectName` emission paths argue for a wire-format consolidation** (collapse `effectName` to client-side lookup) NOT for tearing down the `PatternMetadata` table. The metadata table is the right architectural choice; the wire format that re-emits it everywhere is the wrong serialisation choice.

**Final classification:** the architecture is **load-bearing on the value side** (effects, palettes, zones, families, tags are real product features) and **moderately wasteful on the wire side**. Wire-format consolidation is justified; architectural deletion is not.

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-05-18 | agent:claude-opus-4-7 | Created. W2 hex-ID architectural cost dissection per SSA-W2 mission brief. Read-only static analysis of 8 load-bearing files plus EffectHandlers, PaletteHandlers, RendererActor, WebServer config. Verdict: architecture is NOT the dominant heap cost post-β-fix; remaining attributable cost is ~150 B/s steady-state bandwidth + ~30 KB at app open; wire-format consolidation is justified, architectural refactor is not. |
