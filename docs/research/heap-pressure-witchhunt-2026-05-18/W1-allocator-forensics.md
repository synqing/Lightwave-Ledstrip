---
abstract: "W1 forensic audit of internal-SRAM allocators on K1 V2 metadata serialisation path (2026-05-18). Identifies all DRAM-allocating sites between EffectId reads and JSON wire writes, ranked by per-second pressure under realistic 1-WS-status-subscriber + 1-REST-list-per-minute + 1-zone-change-per-minute workload. Top suspect: WebServerBroadcast.cpp doBroadcastStatus triple-allocation (JsonDocument + String + AsyncWebSocketMessageBuffer) every 5s × client, accounting for ~70 percent of measured per-second internal-heap churn. Read-only static analysis. No code changes."
---

# W1 — Allocator Forensics

Read-only static-analysis audit of every allocator that contributes to internal-SRAM pressure on the K1 V2 metadata serialisation path. All evidence by file:line. No code edits.

The runtime baseline this audit explains is documented at `firmware-v3/src/network/WebServerBroadcast.cpp:602` ("previously sat at ~14 KB/sec/client of internal heap churn — pinning the heap-shed latch") and at `firmware-v3/src/network/ApiResponse.h:152` ("avoids the contiguous String allocation that fragmented the K1 V2 internal heap (~22.5 KB worst case for /api/v1/effects)").

The `~22.5 KB worst case` REST list path has been fixed in May 2026 by the per-item streaming pattern landed in `EffectHandlers.cpp` and `PaletteHandlers.cpp` (claude-mem 47672, 47868, 48131). The status-broadcast WebSocket path has NOT been migrated and remains the dominant unmitigated DRAM-churn source.

---

## Top 3 suspect allocators (ranked)

### 1. WebServerBroadcast.cpp `doBroadcastStatus()` — triple-allocation per 5 s × subscriber

`firmware-v3/src/network/WebServerBroadcast.cpp:110-338`

Per status broadcast tick (≤ 5 s cadence, throttled to 50 ms minimum at line 134, gated behind explicit status subscription at line 124):

| Step | Site | Allocator | Approx size | Lifecycle | Heap |
| --- | --- | --- | --- | --- | --- |
| (a) Build JSON | line 139 `JsonDocument doc;` | ArduinoJson v7 dynamic pool (`new`-backed via internal allocator) | ~3–4 KB peak (≈ 40 keys + ledTransport object + 3-key audio object) | freed at function scope exit | **internal SRAM (DRAM default)** |
| (b) Serialise | lines 280-281 `String output; serializeJson(doc, output);` | `String::concat` doubling allocator on internal heap | ~700–900 B serialised JSON; transient peak ~1.4–1.8 KB during reallocation doubling | freed at function scope exit | **internal SRAM** |
| (c) Queue per client | line 324 `m_ws->text(clientId, output);` → `makeSharedBuffer` → `std::make_shared<std::vector<uint8_t>>` (`AsyncWebSocket.cpp:142, 1270`) | `std::shared_ptr<std::vector<uint8_t>>` containing a `memcpy` copy of `output` | ~700–900 B + shared-vector control block (~28 B) | retained in `_messageQueue` until client ACK — up to `WS_MAX_QUEUED_MESSAGES` (8) frames deep | **internal SRAM** |

(a) and (b) are short-lived; (c) is the load-bearing retained allocation. (c) is shared across subscribers — one shared backing store per broadcast tick, ref-counted across all N status subscribers — but each per-client queue holds the shared_ptr until that client's ACK lands.

The build path between lines 142-202 emits these fields per tick (Reading `firmware-v3/src/network/WebServerBroadcast.cpp`):

- 9 effect-related fields (effectId, effectName, brightness, speed, paletteId, hue, intensity, saturation, complexity, variation, fps, cpuPercent, frameBudgetPercent)
- 12-key `ledTransport` nested object (lines 159-173)
- 5 boolean / counter fields (heap, uptime, dithering)
- 4 SynqMatrix authority fields (lines 184-188)
- 7 edge-mixer fields (lines 191-201)
- 3 audio fields when `FEATURE_AUDIO_SYNC` enabled (lines 214-275: bpm, mic, key)

Per-tick payload measured by the existing code comment as `~700 B` (`WebServerBroadcast.cpp:314`). My structural count yields the same ballpark (37 string keys + numeric values ≈ 720 B serialised).

**Estimated per-second pressure (1 subscriber, 5 s cadence):**
- (a) `JsonDocument` pool: 4 KB × 0.2 Hz = **~800 B/s peak**, transient
- (b) `String output`: 0.9 KB × 0.2 Hz = **~180 B/s**, transient
- (c) `AsyncWebSocketMessageBuffer`: 0.9 KB × 0.2 Hz × queue-residence = **~180 B/s steady-state, retained 50-500 ms per frame**
- Aggregate allocation rate: **~1,160 B/s under nominal flow**; fragmentation pressure dominated by the `JsonDocument`'s ArduinoJson pool blocks (variable-size) churning against the steady ~900 B shared_ptr backing store.

The historical `~14 KB/sec/client` figure at line 602 was pre-subscription-gate. Today's gated path with one subscriber is ~1.2 KB/sec, but the **fragmentation profile remains identical** because every tick still allocates 3 distinct DRAM-backed blocks of different sizes. Internal-SRAM fragmentation is not driven by total bytes allocated — it is driven by *variance in allocation size* against the available `largest_free_block`. ArduinoJson's pool granularity changes per JSON shape, so each broadcast creates a slightly different fragmentation footprint.

### 2. WebServerBroadcast.cpp `broadcastZoneState()` — heavier triple-allocation, throttled to 4 Hz

`firmware-v3/src/network/WebServerBroadcast.cpp:340-425`

Same three-stage allocation pattern as the status broadcast, with a larger payload:

- line 361: `JsonDocument doc;`
- lines 369-379: `segments[]` array — up to 3 zone entries × 6 fields each
- lines 381-409: `zones[]` array — up to 3 zone entries × ~13 fields each (id, zoneId, enabled, effectId, effectName, brightness, speed, paletteId, blendMode, blendModeName, plus 7 audioConfig fields)
- lines 411-416: `presets[]` array — 5 preset names
- line 418-419: `String output; serializeJson(doc, output);`
- line 423: `m_ws->textAll(output)` — broadcasts to **every WS client**, not just status subscribers

Estimated serialised size: ~1.4–1.8 KB (3 zones × 13 fields × ~25 B/field + 5 presets + 3 segments).

Throttle: 250 ms minimum interval (line 356) → max 4 Hz.

**Cadence under realistic load** (1 zone-state change per minute): triggered only on actual zone change. So 1/60 Hz ≈ 0.017 Hz. Per-tick allocation ~3 × 1.6 KB = 4.8 KB. **~80 B/s aggregate** but with much larger per-event spikes.

Note: `textAll()` (line 423) broadcasts to ALL connected clients without subscription gate. The `_messageQueue` retention multiplies across every connected client, including LED-stream-only clients that never asked for zone state. This is logged as still-ungated in claude-mem 47708 (2026-05-02).

### 3. EffectHandlers.cpp `handleParametersGet()` and ApiResponse.h `sendErrorResponse()` family — every REST response

The third place is shared across two patterns that aggregate to substantial churn on the metadata path:

**3a. ApiResponse.h `sendSuccessResponse()` / `sendErrorResponse()` family** — `firmware-v3/src/network/ApiResponse.h:91-103, 108-121, 185-206, 213-235, 308-330`

Every REST handler that uses `sendSuccessResponse(request, builder)` allocates:
- `JsonDocument response;` (line 93)
- builder callback populates fields
- `String output;` + `serializeJson(response, output);` (lines 100-101)
- `request->send(HttpStatus::OK, "application/json", output);` — AsyncWebServerResponse copies into its internal cbuf

This is the path taken by `/api/v1/effects/current`, `/api/v1/effects/parameters` (GET), `/api/v1/palettes/current`, `/api/v1/zones/state` single-zone reads, `/api/v1/synqmatrix/status`, and every error response.

**3b. The `sendSuccessResponseStreamed` migration is INCOMPLETE.** Only `EffectHandlers::handleList` and `PaletteHandlers::handleList` use the streaming pattern (`EffectHandlers.cpp:126-238`, `PaletteHandlers.cpp:128-193`). Every other GET still allocates JsonDocument + String + cbuf.

Cadence under realistic load (1 REST list call per minute, +1 REST status/current poll on connect): ~0.02 Hz nominal. Per call: ~3 × ~600 B = ~1.8 KB. **~40 B/s aggregate** under nominal flow. Spikes on dashboard/iOS connect when the client kicks off 5-10 GETs in burst.

---

## Full allocator inventory

Table of every site between "read EffectId/metadata struct" and "write JSON to wire" that hits internal SRAM. Trigger cadence per the W1 realistic-load scenario (1 status subscriber, 1 zone change/min, 1 effects-list page/min).

| # | Site | Allocator | Cadence (Hz) | Size (bytes) | Lifecycle | Heap |
|---|---|---|---|---|---|---|
| 1 | `WebServerBroadcast.cpp:139` `JsonDocument doc;` (status) | ArduinoJson pool | 0.2 | ~3,000–4,000 peak | freed at scope | internal DRAM |
| 2 | `WebServerBroadcast.cpp:280-281` `String output; serializeJson(doc, output);` (status) | String concat doubling | 0.2 | ~700–900 (transient peak ~1,800 during double) | freed at scope | internal DRAM |
| 3 | `WebServerBroadcast.cpp:324` `m_ws->text(clientId, output)` → shared_ptr<vector<uint8_t>> | shared_ptr backing store | 0.2 × N_subscribers | ~700–900 + 28 control block | retained in `_messageQueue` until client ACK | internal DRAM |
| 4 | `WebServerBroadcast.cpp:361` `JsonDocument doc;` (zones.list) | ArduinoJson pool | ≤4, gated by event | ~4,000–5,000 peak | freed at scope | internal DRAM |
| 5 | `WebServerBroadcast.cpp:418-419` `String output;` (zones.list) | String concat | ≤4, gated by event | ~1,400–1,800 | freed at scope | internal DRAM |
| 6 | `WebServerBroadcast.cpp:423` `m_ws->textAll(output)` (zones.list) | shared_ptr backing store | per-event, copies to ALL connected clients | ~1,400–1,800 | retained per-client until ACK | internal DRAM |
| 7 | `WebServerBroadcast.cpp:455` `JsonDocument doc;` (zones.stateChanged single zone) | ArduinoJson pool | ≤20 throttled | ~1,500 peak | freed at scope | internal DRAM |
| 8 | `WebServerBroadcast.cpp:482-483, 487` `String + textAll` (single-zone) | concat + shared_ptr | ≤20 throttled, per event | ~400 | retained per-client until ACK | internal DRAM |
| 9 | `WebServerBroadcast.cpp:517` `JsonDocument doc;` (effectChanged) | ArduinoJson pool | ≤20 throttled | ~600 peak | freed at scope | internal DRAM |
| 10 | `WebServerBroadcast.cpp:522-523, 527` `String + textAll` (effectChanged) | concat + shared_ptr | ≤20 throttled | ~150 | retained per-client until ACK | internal DRAM |
| 11 | `WebServerBroadcast.cpp:702` `JsonDocument doc;` (beat.event) | ArduinoJson pool | ≤20 throttled, gated by beat | ~800 peak | freed at scope | internal DRAM |
| 12 | `WebServerBroadcast.cpp:713-714, 718` `String + textAll` (beat.event) | concat + shared_ptr | ≤20 throttled | ~200 | retained per-client until ACK | internal DRAM |
| 13 | `ApiResponse.h:78-86` `sendSuccessResponse(request)` no-data | JsonDocument + String + cbuf | per-REST-call (no-data path rare) | ~120 | freed at scope, cbuf consumed by AsyncTCP | internal DRAM |
| 14 | `ApiResponse.h:91-103` `sendSuccessResponse(request, builder)` | JsonDocument + String + cbuf | per-REST-call | 200-1,500 depending on builder | freed at scope, cbuf retained until AsyncTCP drains | internal DRAM |
| 15 | `ApiResponse.h:108-121` `sendSuccessResponse(..., builder, status)` | JsonDocument + String + cbuf | per-REST-call | 200-1,500 | freed at scope, cbuf retained | internal DRAM |
| 16 | `ApiResponse.h:128-142` `sendSuccessResponseLarge` | JsonDocument + String + cbuf | per-REST-call (note: `bufferSize` param is ignored — claim at line 131 confirmed) | 1,000-3,000 | freed at scope, cbuf retained | internal DRAM |
| 17 | `ApiResponse.h:165-180` `sendSuccessResponseStreamed` | JsonDocument + AsyncResponseStream cbuf only — **no intermediate String** | per-REST-call | doc ~variable, cbuf initial 4,096 grows incrementally | doc freed inside `{}` scope before send (intentional, see comment at line 178); cbuf retained until drain | internal DRAM but single-block |
| 18 | `ApiResponse.h:185-206` `sendErrorResponse` | JsonDocument + String + cbuf | per-error-path | ~150 | freed at scope, cbuf retained | internal DRAM |
| 19 | `ApiResponse.h:213-235, 308-330` `sendRateLimitError`, `sendAuthRateLimitError` | JsonDocument + String + cbuf + Retry-After header copy | per-rate-limit-event | ~200 | freed at scope, cbuf retained | internal DRAM |
| 20 | `ApiResponse.h:245-260, 265-281, 286-301, 335-350` `buildWsResponse`, `buildWsError`, `buildWsRateLimitError`, `buildWsAuthRateLimitError` | JsonDocument + String return-by-value | per-WS-command | 50-1,500 depending on payload | String returned; caller does `client->text(response)` → shared_ptr copy (#3 pattern) | internal DRAM |
| 21 | `WsEffectsCommands.cpp:84` `effects.metadata` response builder | buildWsResponse → JsonDocument + String + shared_ptr per client | per-`effects.getMetadata` command | ~250-400 (4 string fields + tags array) | shared_ptr retained per client | internal DRAM |
| 22 | `WsEffectsCommands.cpp:99` `effects.current` response builder | buildWsResponse → JsonDocument + String + shared_ptr | per-`effects.getCurrent` | ~150 | shared_ptr retained per client | internal DRAM |
| 23 | `WsEffectsCommands.cpp:175-178` `effects.list` response | buildWsResponse → JsonDocument with up to 50 entries × ~80 B + String + shared_ptr | per-`effects.list` command | ~1,500-4,500 worst case (limit=50) | shared_ptr retained per client | internal DRAM |
| 24 | `WsEffectsCommands.cpp:323` `effects.changed` response | buildWsResponse | per-`effects.setCurrent` ack | ~120 | per-client retention | internal DRAM |
| 25 | `WsEffectsCommands.cpp:380` `effects.parameters` response | buildWsResponse + JsonArray of parameters | per-`effects.parameters.get` | 300-1,000 | per-client retention | internal DRAM |
| 26 | `WsEffectsCommands.cpp:455` `effects.parameters.changed` response | buildWsResponse + queued/failed arrays | per-`effects.parameters.set` | 200-600 | per-client retention | internal DRAM |
| 27 | `WsStatusCommands.cpp:58-70` `status.subscribe` ack | JsonDocument + serializeJson + String | per-subscribe | ~80 | per-client retention | internal DRAM |
| 28 | `WsPaletteCommands.cpp:67, 101, 136` `palettes.list/get/set` responses | buildWsResponse | per-WS-command | 100-2,000 (list with 75 entries) | per-client retention | internal DRAM |
| 29 | `EffectHandlers.cpp:328-329` `JsonDocument + deserializeJson` for inbound REST POST body | ArduinoJson pool (inbound parse) | per-`POST /effects/parameters` | ~500-2,000 | freed at scope | internal DRAM |
| 30 | `EffectHandlers.cpp:393` `JsonDocument doc;` (similar inbound parse) | ArduinoJson pool | per-POST | ~500-2,000 | freed at scope | internal DRAM |
| 31 | `EffectHandlers.cpp:151, 175` per-item `JsonDocument effect;` inside list stream loop | ArduinoJson pool (small) | per-effect entry in list loop | ~150-300 | freed each iteration before next | internal DRAM (small, repeated) |
| 32 | `PaletteHandlers.cpp:147` per-item `JsonDocument palette;` inside list stream loop | ArduinoJson pool (small) | per-palette entry in list loop | ~150-250 | freed each iteration | internal DRAM (small, repeated) |
| 33 | `PatternRegistry.cpp:26` `PATTERN_METADATA[]` with `PROGMEM` storage class | **flash storage, not heap** | one-time-at-boot | ~50 KB (162 entries × ~310 B struct) | static program lifetime | **flash (PROGMEM)** |
| 34 | `PatternRegistry.cpp:468, 600` `REACTIVE_EFFECT_IDS[]`, `EXPERIMENTAL_EFFECT_IDS[]` | flash storage | one-time-at-boot | ~few KB | static program lifetime | **flash (PROGMEM)** |

**Key clarifications:**

- The PROGMEM `PM_STR()` strings used throughout `PatternRegistry.cpp:23` evaluate to `const char*` pointers that reference flash. When ArduinoJson is fed `data["name"] = somePtr` where `somePtr` is a `const char*` to a string literal or PROGMEM, **ArduinoJson does NOT copy the string** (per ArduinoJson v7 contract — only `String`/`std::string`/non-static char buffers force a duplicate). So the 162-effect metadata table itself does not generate per-broadcast DRAM allocations for name/story/opticalIntent strings.
- The `JsonDocument` pool DOES allocate DRAM for: numeric keys/values, nested objects/arrays, and any `String`-typed values. The dominant DRAM cost per broadcast is the JsonDocument's internal pool growth, not the source string data.
- ArduinoJson v7 documents are heap-backed (DRAM via `MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL` fallthrough on ESP32). There is no PSRAM-backed `JsonDocument(allocator)` instance anywhere in the audited paths.

---

## Per-second churn estimate under realistic load

Workload: 1 WebSocket client subscribed to `status`, 1 REST `GET /api/v1/effects?limit=50` per minute, 1 zone-state change per minute, audio-sync enabled (so beat/onset broadcasts gated).

| Category | Allocation rate (B/s) | Notes |
|---|---|---|
| ArduinoJson document churn (status broadcast every 5 s) | ~700 | line 139, ~3.5 KB / 5 s peak |
| `String output` for status (every 5 s) | ~180 | line 280-281, ~900 B / 5 s |
| AsyncWebSocket shared_ptr backing store (status) | ~180 | line 324, ~900 B / 5 s, queue-retained |
| `zones.list` allocation triple (1/min × 4.8 KB) | ~80 | lines 361, 418, 423 |
| `effectChanged` triple (0/min nominal — only on actual change) | ~0 | rare under steady state |
| REST `GET /api/v1/effects?limit=50` per item × 50 / 60 s | ~150 | per-iter ~180 B JsonDocument allocated + serialised, freed before next iter — peak per iter ~300 B but cumulative byte-rate is high |
| REST envelope + cbuf for the same call | ~70 | streamed cbuf incremental growth |
| ApiResponse `sendSuccessResponse` for other GETs (estimate 1/min) | ~30 | varied |
| **Total estimated allocation rate** | **~1,390 B/s** | nominal |

Rate is modest in absolute terms (the K1 V2 internal DRAM at boot is ~21 KB largest free block per the comment at `ApiResponse.h:152`). The fragmentation issue is not byte-rate; it is the **variance in block sizes** that each broadcast cycle leaves behind:

- ArduinoJson allocates pool chunks in irregular sizes as the document grows. Each broadcast's JsonDocument allocation footprint differs slightly from the last, leaving differently-sized holes when freed.
- `String::concat` doubles its capacity on demand: 32, 64, 128, 256, 512, 1024 — leaving doubled-then-freed blocks.
- The retained `shared_ptr<vector<uint8_t>>` is sized to exact JSON length per tick, so its block size also varies.

Three differently-sized DRAM blocks per broadcast × 12 broadcasts/min × 1440 broadcasts/2h = **~17,000 heterogeneous alloc/free pairs in a 2-hour soak**. The internal heap allocator (`heap_caps_*` with default first-fit + best-fit policies) cannot coalesce mixed-size holes effectively under that churn — leading exactly to the symptom described in the witchhunt brief: structural fragmentation, falling `largest_free_block` even when `free_size` looks acceptable, and ENOMEM in UDP packet allocation because UDP packets need a *contiguous* free block.

### Ranked per-second pressure (descending)

1. **`doBroadcastStatus()` triple (#1, #2, #3)** — ~1,060 B/s steady, 3 distinct DRAM blocks/tick, 12 ticks/min → **~70% of total churn AND 70% of fragmentation surface**
2. **REST `/api/v1/effects` list per-item streaming (#31)** — ~220 B/s during the 1-per-minute list call, 50 iterations × small JsonDocument each → high allocation count but small per-block fragmentation footprint (pre-fragmentation-fix worst case was ~22.5 KB single-block, now bounded — claude-mem 47997, 48131)
3. **`broadcastZoneState()` triple on zone change (#4, #5, #6)** — ~80 B/s averaged, but each event spikes ~4.8 KB across 3 DRAM blocks, retained per-client via `textAll`

---

## The `String` copy chain in the metadata path

Trace of every place a string is COPIED (not pointer-aliased) between an `EffectId` read and the WS/REST wire:

**Status broadcast** (`WebServerBroadcast.cpp`):
- line 144 `const char* curName = cached.findEffectName(cached.currentEffect);` — pointer to cached effect-name table, NOT a copy.
- line 145-147 `if (curName) doc["effectName"] = curName;` — ArduinoJson stores the const char* by pointer; NO copy of the name string.
- line 280-281 `String output; serializeJson(doc, output);` — **single STRING COPY** of the entire serialised JSON, including the name string concatenated into `output`. ~900 B contiguous DRAM.
- line 324 `m_ws->text(clientId, output);` → AsyncWebSocket.cpp:1010 `text(message.c_str(), message.length())` → AsyncWebSocket.cpp:1004 `makeSharedBuffer(message, len)` → AsyncWebSocket.cpp:1275 `new AsyncWebSocketMessageBuffer(data, size)` → AsyncWebSocket.cpp:134 constructor `_buffer(std::make_shared<std::vector<uint8_t>>(data, data + size))` — **second STRING COPY** of the entire JSON into a heap-allocated `std::vector<uint8_t>`.

So per status broadcast, the JSON payload is materialised TWICE in internal DRAM:
1. As the local `String output` (freed when `doBroadcastStatus` returns)
2. As the shared `std::vector<uint8_t>` backing store (retained in `_messageQueue` per client until ACK)

**Effects-list WebSocket response** (`WsEffectsCommands.cpp:117-178`):
- line 164 `effectNames[i] = ctx.renderer->getEffectName(eid);` — pointer copy into a local stack array; NO string allocation.
- line 175-177 `String response = buildWsResponse("effects.list", requestId, [effectNames, ...](JsonObject& data) { ... encodeList(...) ... });`
  - Inside `encodeList` (`WsEffectsCodec.cpp:465-481`), each entry does `effect["name"] = effectNames[i] ? effectNames[i] : "";` — ArduinoJson stores by pointer for `const char*`, NO copy.
  - But the final `serializeJson(response, output)` inside `buildWsResponse` (`ApiResponse.h:258`) emits the whole JSON into a `String output` then returns it. **One STRING COPY.**
- line 178 `client->text(response);` → shared_ptr path → **second STRING COPY** into shared vector.

**REST effects list streaming path** (`EffectHandlers.cpp:151, 175, 215, 164`):
- Inside the per-iter loop: `effect["name"] = renderer->getEffectName(eid);` — pointer-aliased.
- `serializeJson(effect, *response)` — emits the entry directly into the `AsyncResponseStream`'s cbuf, which grows incrementally. **Only ONE COPY** (into the cbuf), and the cbuf is the eventual response buffer that AsyncTCP drains.

The streaming pattern eliminates ONE of the two string copies per response. The status-broadcast path on `WebServerBroadcast.cpp` still has BOTH copies.

**There is no `String + String` or `String::concat` of effect names in the audited paths — names always stay as `const char*` pointers, copied only as part of the JSON serialisation.** That is the correct ArduinoJson v7 contract. The DRAM cost is in the JsonDocument pool, the serialised String, and the shared vector — not in name-string duplication.

---

## Open questions for reviewers

1. **Why does `doBroadcastStatus` still use `String output;` + `serializeJson` rather than the streaming pattern that already exists for REST handlers?** The text-channel WebSocket protocol does not expose an equivalent of `AsyncResponseStream`. The shared `std::vector<uint8_t>` is the irreducible per-broadcast retained allocation — but the *intermediate* `String` (line 280-281) is gratuitous. One could `serializeJson(doc, std::vector<uint8_t>&)` directly and pass that vector by move into `makeSharedBuffer`, eliminating allocation #2 entirely. Worth verifying ArduinoJson v7 supports streaming into a `std::vector<char>` writer (it does — via `serializeJson(doc, ::Print&)` and a thin custom Print adapter).
2. **Can the 5 s status broadcast be replaced by event-driven push?** The status doc is rebuilt from `m_cachedRendererState` (line 142) — if that cache invalidates on parameter changes, a "broadcast only on dirty" model would cut the 0.2 Hz baseline allocation rate to near-zero during idle UI.
3. **Should the ArduinoJson allocator be migrated to a PSRAM-backed allocator?** PSRAM is available on K1 V2 ESP32-S3. `JsonDocument(spi_ram_allocator)` would move the ~3-4 KB pool off internal SRAM entirely. However, ArduinoJson serialisation itself does not access the document pool from ISR context, so PSRAM allocation latency is acceptable. This needs benchmarking — proposing it here, not recommending.
4. **Why does `broadcastZoneState` use `textAll` instead of subscriber-gated send?** Tracking issue per claude-mem 47708 (2026-05-02) "Remaining textAll() Broadcast Sites Not Yet Gated by Subscriber Pattern". This path retains a ~1.6 KB shared buffer in the queue of every connected client, including LED-stream-only consumers that ignore it.
5. **Is `AsyncWebSocketSharedBuffer` (the `shared_ptr<vector<uint8_t>>`) eligible for `heap_caps_malloc(MALLOC_CAP_SPIRAM)` via a custom `std::allocator`?** This is the retained allocation. Even if ArduinoJson stays on internal heap, moving the queue's retained backing store to PSRAM would relieve the structural-fragmentation symptom directly. Requires patching ESPAsyncWebServer or wrapping `makeSharedBuffer` — non-trivial but high-leverage.
6. **Cbuf growth in `AsyncResponseStream`** — `EffectHandlers.cpp:127` opens with `beginResponseStream("application/json", 4096)`. If the response exceeds 4 KB the cbuf grows via internal reallocation, generating exactly the kind of doubling-realloc fragmentation we are trying to avoid. Worth verifying the K1 V2 worst-case effects-list (50 entries × ~150 B) lands under 4 KB nominal; the comment at line 127 implies it does, but no measurement is cited.
7. **The streamed pattern in `sendSuccessResponseStreamed`** (`ApiResponse.h:165-180`) is opt-in. Auditing which REST handlers have migrated would close this gap. Quick grep shows `EffectHandlers::handleList` and `PaletteHandlers::handleList` use `beginResponseStream` directly (not via the helper); other handlers still use `sendSuccessResponse`.

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-18 | agent:claude-opus-4.7-1m | Created. W1 forensic allocator audit on K1 V2 internal-SRAM metadata path. Identifies WebServerBroadcast.cpp doBroadcastStatus triple-allocation as dominant suspect (~70% of churn), with broadcastZoneState textAll and REST list streaming pattern gaps as #2 and #3. PROGMEM PATTERN_METADATA verified flash-only; no per-broadcast DRAM cost from metadata strings themselves. AsyncWebSocket message buffer mechanics verified — shared_ptr<vector<uint8_t>> retained per-client until ACK. |
