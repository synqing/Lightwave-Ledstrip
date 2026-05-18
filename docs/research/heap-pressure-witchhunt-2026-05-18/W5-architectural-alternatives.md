---
abstract: "Three architectural alternatives (A: PSRAM-relocate, B: PROGMEM-intern + delta wire, C: hex-ID surgery) for resolving K1 V2 internal-SRAM fragmentation under sustained HTTP+WS load. Decision matrix, hybrid recommendation (A+B), and fall-through sequencing for Captain + reviewer ranking. Read-only design output, no code changes."
---

# W5 - Architectural Alternatives

**Mission:** Design three named architectural responses to K1 V2's structural internal-SRAM fragmentation, score each, and produce a Captain-facing decision matrix.

**RBDO label:** **DEGRADED-MODE** for the *fragmentation-reduction* and *headroom-gained* columns of the scoring rubric.

- *Unresolved assumption:* the W2/W3/W4 working memory is "fragmentation, not absolute exhaustion." All percentage estimates in this document treat that hypothesis as the working frame; soak telemetry (W4) is the validator. If the root cause is *throughput* (queue depth) rather than *coalescence failure*, Option A degrades to "buys time, does not fix" and Option B becomes load-bearing.
- *Risk if wrong:* picking Option A first when the real problem is wire payload size means flashing PSRAM relocation, observing no improvement, and burning a cycle.
- *Fallback:* the W4 soak protocol is the gate. Re-run after each option lands; do not stack options without an interim soak.
- *Revisit trigger:* W4 telemetry confirms or refutes the "5-10x payload shrink" estimate in §Option B, OR PSRAM-relocate ships in §Option A and W4 still shows shed hysteresis.
- *Debt count / affected outputs:* the working hypothesis underpins W4, W5, and the eventual W6 implementation plan (3 dependents - within the gate's "≤3 without a fourth" limit).

**Source-truth scope:** all file:line citations verified against HEAD at 2026-05-18. Notes on commit hash drift are flagged inline. British English throughout.

---

## Ground-truth facts (load-bearing)

| Fact | Evidence |
|---|---|
| Effect count = 162 (auto-generated from `inventory.json` via `gen_effect_ids.py`) | `firmware-v3/src/config/effect_ids.h:1-23` header + `PatternRegistry.h:11` comment |
| Palette count = 75 (NOT ~256 as the mission brief states) | `firmware-v3/src/palettes/Palettes_Master.h:7-10, 65` |
| `EffectId = uint16_t` (family-byte | sequence-byte) | `effect_ids.h:35` |
| Heap-shed hysteresis exists | `WebServer.cpp:562-613, 911-1572` |
| PSRAM allocation pattern with DRAM fallback already in production | `WebServer.cpp:338,371,381` (audio scratch frames), `StaticAssetRoutes.cpp:54-67` (`getProvisioningBuffer`), `WsCommandRouter.cpp:30-39` (handler table) |
| ArduinoJson 7 has a custom-allocator hook | `.pio/libdeps/<env>/ArduinoJson/src/ArduinoJson/Document/JsonDocument.hpp:24` - `explicit JsonDocument(Allocator* alloc = detail::DefaultAllocator::instance())` |
| `AsyncResponseStream` already used to skip the intermediate String | `ApiResponse.h:165-180` (`sendSuccessResponseStreamed`), with explicit "fragmented the K1 V2 internal heap (~22.5 KB worst case for /api/v1/effects)" commentary |
| Periodic status broadcast already gated on explicit subscription | `WebServerBroadcast.cpp:120-124, 596-640` - was ~14 KB/sec/client churn |
| Status broadcast payload is full-state (not delta) | `WebServerBroadcast.cpp:139-281` builds ~25 top-level fields per tick |
| Family names are *already* in `PROGMEM` | `PatternRegistry.h:46-69` - the model exists, just not extended to effect/palette names |
| Effect names live in struct members as `const char*` | `PatternRegistry.h:90-97` - `name`, `story`, `opticalIntent`, `relatedPatterns` all `const char*`. Whether the underlying literals end up in flash or DRAM depends on the constants table built by `PatternRegistry.cpp`. |
| Codecs use `JsonDocument` (heap) for every response | `WsEffectsCodec.cpp` 19 encoder paths; `HttpEffectsCodec.cpp` 7 paths; `WsPaletteCodec.cpp` 3 paths; `WebServerBroadcast.cpp` 5 broadcast paths |
| `ArduinoJson::JsonString(ptr, ZeroCopy)` reference-storage exists | ArduinoJson 7 standard - confirmed library version in libdeps |

**Mission-brief correction:** the brief states "~256 palettes with similar metadata"; the actual count is **75**. Palette JSON churn is real but smaller than expected; this *reduces* Option C's blast-radius justification proportionally.

---

## Option A: PSRAM-relocate (allocator layer only)

### Architecture sketch (~10 lines)

1. Introduce `lightwaveos::network::PsramJsonAllocator : public ArduinoJson::Allocator` that calls `heap_caps_malloc(.., MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)` first and falls back to `MALLOC_CAP_8BIT` (mirrors `StaticAssetRoutes.cpp:62-67`).
2. Wrap every `JsonDocument` site at construction with `JsonDocument doc(&g_psramAllocator)`. This is a mechanical sweep of ~30 call sites in `network/` + `codec/`.
3. Replace the `String output; serializeJson(doc, output);` egress pattern (~12 sites) with `AsyncResponseStream*` (HTTP) or a PSRAM-backed `std::string`-like buffer (WS). The streamed-response helper at `ApiResponse.h:165-180` is the template.
4. AsyncWebSocket egress chunks: enable the per-client text-buffer custom allocator if ESPAsyncWebServer 3.9.3 exposes it (verify via `AsyncWebSocketMessageBuffer` ctor in `.pio/libdeps/.../ESPAsyncWebServer/src/`).
5. Hex-ID metadata system **unchanged**; wire contract **unchanged**; client migration cost **zero**.

### Files touched

- New: `firmware-v3/src/network/PsramJsonAllocator.{h,cpp}` (~80 LOC)
- Mod: `WebServerBroadcast.cpp` (~5 `JsonDocument` sites + 3 `String output` egresses)
- Mod: `ApiResponse.h` (extend streamed helper to WS error/response builders; replace `String` egress)
- Mod: `WsEffectsCodec.cpp`, `WsPaletteCodec.cpp`, `WsZonesCodec.cpp`, `WsTransitionCodec.cpp`, `WsAudioCodec.cpp`, `WsColorCodec.cpp`, `WsCommonCodec.cpp`, `WsBatchCodec.cpp` (~16 `JsonDocument doc;` -> `JsonDocument doc(&g_psramAllocator);`)
- Mod: `HttpEffectsCodec.cpp`, `HttpZoneCodec.cpp`, `HttpAudioCodec.cpp`, `HttpPaletteCodec.cpp`, `HttpDeviceCodec.cpp`, `HttpSystemCodec.cpp` (~10 sites)
- Mod: `V1ApiRoutes.cpp` (103 KB file - audit for `JsonDocument` literal construction, ~25-40 sites)
- Mod: `WsGateway.cpp` (39 KB; per-client text egress)
- Mod: `WebServer.cpp` (existing PSRAM audio-scratch pattern stays; status broadcast `JsonDocument` switches allocator)

Total surface: **~12 files modified, 1 new pair.** Roughly 60-80 mechanical line changes.

### Wire contract impact

**None.** `docs/protocol/k1-ws-contract.yaml` and `k1-rest-contract.yaml` unchanged. Same JSON shape, same field names, same payload sizes. The bytes the client receives are byte-identical.

### Migration plan

**None.** Tab5, iOS, dashboard, k1-composer all continue to consume the exact wire shape. Zero client-side changes.

### Verification plan

1. Flash to K1 V2 via `pio run -e esp32dev_audio_esv11_k1v2_32khz -t upload`.
2. Run W4 soak protocol (HTTP + WS sustained load).
3. Sample `heap_caps_get_free_size(MALLOC_CAP_INTERNAL)` and `heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)` every 5 s.
4. **Pass criterion:** `m_lowHeapShed` never latches over a 30-minute soak; largest free internal block stays above `INTERNAL_HEAP_SHED_BELOW_BYTES + INTERNAL_HEAP_LARGEST_BLOCK_MARGIN_BYTES` (constants at `WebServer.cpp:562`).
5. **UDP ENOMEM check:** capture UDP packet allocation telemetry; failures should drop to zero or near-zero.

### Failure mode

If fragmentation root cause is **payload size, not allocator location**:
- PSRAM-relocate **does not help**. The byte streams the WS still pushes through `AsyncWebSocketClient::text()` allocate into the AsyncWebSocket per-client TX buffer, which lives in *internal* DRAM (AsyncTCP design constraint). Large status broadcasts (~700 B) still pin per-client queues.
- Symptom: `m_lowHeapShed` continues to latch; UDP ENOMEM persists; the only relief is that ArduinoJson scratch no longer competes with TCP buffers.

**Secondary risk:** PSRAM access is ~5x slower than DRAM. `serializeJson` over a PSRAM-backed doc adds 100-300 µs per call. For broadcasts at 5 Hz this is negligible. For per-frame JSON (audio.frame at 125 Hz) this **matters** - relocate that path last and measure.

**Tertiary risk:** PSRAM exhaustion. K1 V2 has 8 MB PSRAM with `LedStreamBroadcaster`, `LogStreamBroadcaster`, audio scratch, and effect buffers already resident. Available headroom needs measuring; the existing `getProvisioningBuffer()` fallback pattern protects against catastrophic failure.

### One-line recommendation

**Pick A first** when the working hypothesis is "internal-DRAM allocator can't coalesce because too many heterogeneous short-lived allocations" - this is the lowest-risk, highest-reversibility change, and the production codebase already proves the PSRAM allocation pattern at 5 sites.

---

## Option B: PROGMEM-intern + delta wire (metadata layer)

### Architecture sketch (~10 lines)

1. Audit `PatternRegistry.cpp`: convert all 162 effect `name`, `story`, `opticalIntent`, `relatedPatterns` literals to `static const char NAME_FIRE[] PROGMEM = "Fire";` form, pointed at by the existing struct (family-name pattern at `PatternRegistry.h:46-69` is the template).
2. Same for `MasterPaletteNames[]` in `Palettes_MasterData.cpp` (75 entries).
3. Change all JSON construction to use `obj["name"] = JsonString(ptr, ZeroCopy);` so the string reference (4 B pointer) is stored in the JsonDocument rather than copied into ArduinoJson's variant pool.
4. Introduce `status.delta` WS message type: only fields that **changed since last broadcast** are emitted. Keep `status` full-state for backwards compatibility; opt-in via `status.subscribe { "delta": true }`.
5. Delta state requires a `CachedRendererState` shadow held by `WebServer.cpp`; diff at broadcast time; emit only the diff. Effect-name lookup table stays on K1.
6. Hex-ID system **unchanged**; new wire field is **additive** (backwards-compatible).

### Files touched

- New: `firmware-v3/src/effects/PatternNamesProgmem.{h,cpp}` (162 PROGMEM string literals, 1 lookup function)
- New: `firmware-v3/src/palettes/PaletteNamesProgmem.{h,cpp}` (75 PROGMEM literals)
- Mod: `PatternRegistry.cpp` (replace inline string literals with PROGMEM refs)
- Mod: `Palettes_MasterData.cpp` (same)
- Mod: `WebServerBroadcast.cpp` - introduce `m_lastBroadcastSnapshot`, diff logic, optional delta path
- Mod: `WsEffectsCodec.cpp` `encodeMetadata`, `encodeList`, `encodeParametersGet`, `encodeByFamily` - switch to `JsonString(ptr, ZeroCopy)`
- Mod: `HttpEffectsCodec.cpp` `encodeList`, `encodeCurrent`, `encodeMetadata` - same
- Mod: `WsStatusCommands.cpp` (extend subscribe payload schema with `delta: true`)
- Mod: `docs/protocol/k1-ws-contract.yaml` - add `status.delta` opt-in message type
- Mod: iOS / Tab5 / dashboard - opt-in to `delta` subscription, apply patches on top of cached state

Total surface: **~9 files modified, 2 new pairs, 1 YAML schema change, 3 client repos.**

### Wire contract impact

**Additive.** New WS message type `status.delta` with `{type: "status.delta", changedFields: {...}}`. Full-state `status` remains available; clients opt in via subscription option. Per the W5 contract-first gate: **update `docs/protocol/k1-ws-contract.yaml` FIRST**, then implement.

### Migration plan

- iOS: extend `WebSocketService` to handle `status.delta` and apply patches to a local `LightwaveState` cache. Backwards-compatible: app works without opt-in, just at the old payload size.
- Tab5: same opt-in pattern. The `WsStateApply.cpp` consumer needs a partial-update path.
- Dashboard: optional - leave at full-state until iOS/Tab5 are validated.
- All clients can defer indefinitely; the **firmware benefit is unlocked only when a client opts in**, which is the design choice.

### Verification plan

1. Phase 1 (PROGMEM-only, no delta): flash, soak, measure internal heap free + largest block. Expect modest improvement from removed `String` copies of effect names during `notifyEffectChange` and metadata responses.
2. Phase 2 (delta wire): iOS in `delta` mode under continuous status load. Expect ~5-10x reduction in WS TX payload size (status drops from ~700 B to ~80 B typical-tick), proportional reduction in AsyncWebSocket per-client queue pressure.
3. **Pass criterion:** same as Option A. Heap-shed latch never triggers under W4 soak.

### Failure mode

If fragmentation root cause is **scratch-buffer churn, not wire egress**:
- PROGMEM-intern still helps a little (no String copies for names).
- Delta wire still helps a lot (smaller payloads ease TX-buffer pressure).
- But the **JsonDocument scratch itself** still allocates from internal DRAM. The 5-10x egress shrink might not be enough; the ArduinoJson pool allocations during construction are the actual culprit.

**Secondary risk:** delta state cache divergence between K1 and client. If a delta is dropped (network glitch, client backpressure), the client's view of K1 state diverges silently. Mitigation: periodic full-state resync every N seconds, OR client-side request for resync if it detects a sequence-number gap. The `cleanupClients()` path already cycles per broadcast - lean on that.

**Tertiary risk:** PROGMEM access on ESP32-S3 is mapped flash; reads cost ~10 cycles vs DRAM ~1 cycle, but JSON construction calls are not in a hot render path. Negligible.

### One-line recommendation

**Pick B when A alone hasn't closed the gap**, or when telemetry shows the AsyncWebSocket per-client TX buffer (not the ArduinoJson scratch) is the binding constraint.

---

## Option C: Hex-ID architectural surgery

### Architecture sketch (~10 lines)

1. **Sub-option C.1 (uint8_t flatten):** redefine `EffectId = uint8_t` (0-255). Drop family-byte encoding. Build a separate `EffectFamilyMap[256]` array on K1 for backwards `family-of-effect` queries.
2. **Sub-option C.2 (keep uint16_t, drop metadata from broadcast):** keep `EffectId = uint16_t` (no data-pattern change, no flash-layout disruption), but **strip `name`, `family`, `story`, `opticalIntent` from periodic status and from `effectChanged` broadcasts**. Status carries only `effectId: 0x1313`. Clients fetch full catalogue once via `GET /api/v1/effects/metadata` and cache by ID.
3. K1 stops needing per-tick name lookups (`cached.findEffectName(cached.currentEffect)` at `WebServerBroadcast.cpp:144`); the lookup moves to the client.
4. Effect family taxonomy ships as a downloadable manifest (versioned, ETag'd), refreshed only on K1 firmware version change.
5. Hex-ID system either changes (C.1) or stays (C.2); wire contract **breaks** in both cases - status payload schema changes.

### Files touched

- Mod: `effect_ids.h` (auto-generated; rerun `gen_effect_ids.py`) - C.1 only
- Mod: `inventory.json` schema - C.1 only
- Mod: every effect class header declaring `kId` - C.1 only (~162 files, mechanical)
- Mod: `PatternRegistry.{h,cpp}` - C.1 only (lookup tables)
- Mod: `WebServerBroadcast.cpp` - both: strip name/family fields from `doBroadcastStatus`, `notifyEffectChange`, `broadcastZoneState`
- Mod: every status consumer in iOS, Tab5, dashboard, k1-composer
- Mod: `docs/protocol/k1-ws-contract.yaml` (breaking) + version bump
- Mod: `docs/protocol/k1-rest-contract.yaml` for the new `/api/v1/effects/metadata` shape (if not already there)
- Mod: Tab5 firmware - inflate its existing per-ID lookup cache and stop reading `effectName`/`family` from status
- Mod: iOS `LightwaveState` model and all views that bind to `effectName`
- Migration tooling: a one-shot script that consumes the cached metadata response and stores it in client app bundle for offline display

Total surface: **C.1 = ~165+ firmware files + 3 client repos + breaking contract. C.2 = ~3 firmware files + 3 client repos + breaking contract.**

### Wire contract impact

**Breaking.** Status payload schema changes (fields removed). Requires:
- WS contract version bump (e.g. `v3`)
- Coordinated client release across iOS, Tab5, dashboard, k1-composer
- A grace period where K1 still emits the old `status` and adds a new `status.compact` opt-in, **OR** a flag day where everything ships at once.

### Migration plan

- All clients must add the metadata-cache pattern: hit `GET /api/v1/effects/metadata` on app cold start, store in `UserDefaults` / NVS / IndexedDB, refresh on firmware-version-change.
- Tab5 already has small EffectId-keyed structs; this expands those.
- iOS `EffectsListView` must render from cache, not from status broadcast. Currently the status's `effectName` is consumed at three places (HUD, parameter view, transition overlay).
- Dashboard the same.
- The metadata endpoint must be **paginated and cache-friendly** because it's now a ~5-15 KB payload (162 effects × ~80 B each). ETag + 24h Cache-Control.

### Verification plan

1. Stand up the metadata endpoint, deploy and cache from each client.
2. Phase the breaking change behind a feature flag for one release.
3. Validate every client renders correctly using cached metadata.
4. Cut over the wire format on a flag day.
5. Run W4 soak.

### Failure mode

If the root cause is **not the metadata fields but the JsonDocument pool churn**:
- C.2 still shrinks the per-broadcast payload by 60-70 % (name + family + family-name removed; that's ~60 B per status tick) - marginal benefit.
- C.1 doesn't help directly with fragmentation at all. It's a cleanup of the ID space, not a heap optimisation. The justification collapses if heap fragmentation is the only problem.
- Client breakage during phased migration leaves K1 broadcasting fields no one consumes, or worse, breaks clients that haven't migrated.

**Secondary risk:** metadata-cache version drift. If iOS caches v1 metadata and K1 ships v2 firmware with new effects, the new effect ID broadcasts produce "Unknown effect" in the iOS UI until a metadata refresh. Mitigation: include `metadataVersion` in every status payload; client checks-and-refreshes.

**Tertiary risk:** all the new effects discovered post-launch via OTA upload (`FAMILY_OTA_USER = 0xF0`) must trigger client-side metadata refresh.

### One-line recommendation

**Pick C only after A and B have shipped and W4 still shows pressure** - this is the highest-cost, highest-blast-radius option, and the metadata-on-the-wire is at most ~60 B per status broadcast (small compared to the ~640 B telemetry payload). The 162-effect surgery in C.1 is **unjustified by the fragmentation evidence alone**.

---

## Hybrid options

### Hybrid A+B (recommended hybrid)

**Strongest combination.** Option A removes scratch-buffer churn from internal DRAM; Option B reduces the per-broadcast egress bytes by 5-10x. Together they attack the two distinct pressure sources:

- A relieves **producer-side** allocator pressure (JsonDocument scratch).
- B relieves **consumer-side** TX-buffer pressure (AsyncWebSocket per-client queue depth, which lives in internal DRAM and cannot be relocated).

Sequence: ship A first (no contract impact, single-PR, zero client coordination), measure, then layer B's PROGMEM-intern (still no contract change), then B's delta-wire (additive contract). Each phase is independently revertible.

### Hybrid A+C.2

A + the small-surgery flavour of C (strip metadata from broadcasts, keep uint16_t). Avoids the 162-file rename of C.1 but takes the contract break of C.2. Worth considering only if B's delta-wire design is judged too complex for the iOS/Tab5/dashboard release window.

### Anti-pattern: B+C combined

Adopting both B (delta wire) and C (strip metadata) at once creates a doubly-breaking contract change. Do not do this. Pick one wire-side intervention or the other.

---

## Decision matrix

| Dimension | A: PSRAM | B: PROGMEM + Delta | C.2: Strip metadata | C.1: uint8_t flatten |
|---|---|---|---|---|
| Expected fragmentation reduction | **40-60 %** (medium-high) - allocator churn moves to PSRAM, internal stays clean | **20-40 %** alone (PROGMEM phase), **+30-50 %** with delta (egress shrink) | **15-30 %** - smaller payloads only | **5-15 %** - flash layout cleanup, marginal heap effect |
| Internal-SRAM working set headroom (bytes) | **+8-25 KB** estimated (JsonDocument pools off DRAM) | **+4-8 KB** PROGMEM, **+12-18 KB** with delta (per-client TX buffer) | **+2-4 KB** | **+1-2 KB** |
| Implementation effort (SSA-days) | **2-3 days** | **5-8 days** (PROGMEM cheap, delta-wire is the cost) | **3-5 days** (mostly client coordination) | **8-15 days** (162-file mechanical sweep + extensive testing) |
| Blast radius (files touched) | **~12 firmware files, 1 new pair** | **~9 firmware, 2 new pairs, 1 YAML, 3 client repos** | **~3 firmware, 3 client repos, 1 YAML** | **~165 firmware, 3 client repos, 2 YAMLs** |
| Wire contract breakage | **None** | **Additive** (new opt-in message type) | **Breaking** (fields removed from status) | **Breaking** (ID space redefined) |
| Tab5 + iOS client migration cost | **Zero** | **Medium** - delta application logic, version-cache | **High** - metadata caching, all UI bindings rewired | **High** - all hardcoded IDs reviewed, all stored prefs migrated |
| Failure mode if hypothesis is wrong | "Bought time, didn't fix" - PSRAM scratch still beats DRAM scratch on coalescence | PROGMEM phase always net-positive; delta-wire gives nothing without client adoption | Marginal payload reduction; doesn't help if scratch-pool is the binding constraint | Significant work for negligible heap benefit |
| Reversibility | **High** - allocator class can be a global toggle; revert by switching default | **High** for PROGMEM phase; **Medium** for delta (need to maintain dual code path in WebServer for one release) | **Low** - clients already migrated; rolling back firmware reintroduces fields they no longer parse | **Very low** - 162 effect IDs are stored in NVS, presets, user histories; rollback breaks those |
| Hardware / library dependence | **Confirmed safe:** ArduinoJson 7 `JsonDocument(Allocator*)` ctor at `JsonDocument.hpp:24`; `MALLOC_CAP_SPIRAM` pattern in production at 5 sites; AsyncWebSocket custom-allocator hooks for chunk payloads need verification before §A4 lands | **PROGMEM:** standard ESP-IDF flash addressing. **`JsonString(ptr, ZeroCopy)`:** ArduinoJson 7 standard, safe | **Standard ESP-IDF + ArduinoJson** | **Touches `gen_effect_ids.py`, all effect_ids.h consumers, all client-side stored IDs** |

---

## Recommended sequencing

### Phase 1: Option A (PSRAM-relocate) - week 1

- New `PsramJsonAllocator` class + sweep all `JsonDocument` sites under `network/`, `codec/`, `webserver/`.
- Replace remaining `String output; serializeJson(...)` egress with `AsyncResponseStream` (HTTP) and a PSRAM-backed buffer (WS).
- Audit `AsyncWebSocket` 3.9.3 for custom-allocator hooks on per-client TX buffers; if present, route those through PSRAM too.
- W4 soak.
- **Decision point:** if W4 passes (no shed-latch in 30 min), STOP HERE. Sequencing complete.

### Phase 2: Option B PROGMEM-intern (only if Phase 1 fails or partially succeeds) - week 2

- Convert `PatternRegistry.cpp` effect name/story literals to `PROGMEM`.
- Convert `Palettes_MasterData.cpp` palette names to `PROGMEM`.
- Switch all metadata JSON encoders to `JsonString(ptr, ZeroCopy)`.
- W4 soak.
- **Decision point:** if W4 passes, STOP HERE.

### Phase 3: Option B delta-wire (only if Phase 2 fails) - week 3-4

- Update `docs/protocol/k1-ws-contract.yaml` FIRST.
- Implement K1-side `status.delta` opt-in path with `m_lastBroadcastSnapshot` diff.
- iOS opts in; iterate on patch-application correctness.
- Tab5 opts in.
- W4 soak under multi-client load.
- **Decision point:** if W4 still fails, fragmentation is not the root cause - re-open the hypothesis (consult W3 evidence).

### Phase 4: Option C.2 (only if Phase 3 fails AND the W4 evidence specifically implicates metadata-bytes) - timeline TBD

- Bump contract to v3.
- Strip name/family from status.
- Coordinate client release. Flag day.

### Phase 5: Option C.1 (DO NOT ENTER unless C.2 still insufficient and the uint16_t hex space is judged a long-term liability for other reasons)

- 162-file rename. Multi-week. Treat as a separate program, not a heap-pressure fix.

### Stop-loss gates

- After Phase 1, **expected outcome is success.** Captain should pre-approve A as the unblocker and treat subsequent phases as escalation paths.
- After Phase 2, if soak still fails by the same shed-hysteresis signature, the W3 hypothesis ("metadata churn drives fragmentation") needs revisiting before authorising the contract break in Phase 3.
- Never skip directly to Phase 4 or 5 without W4 evidence pointing specifically at wire-payload size or hex-ID schema.

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-05-18 | agent:SSA-W5 (Software Architect, Opus 4.7) | Created. Three architectural alternatives + hybrid + decision matrix + phased sequencing for K1 V2 internal-SRAM fragmentation resolution. Read-only design, no code changes. Mission-brief palette-count assumption (256) corrected to 75 per `Palettes_Master.h:65`. |
