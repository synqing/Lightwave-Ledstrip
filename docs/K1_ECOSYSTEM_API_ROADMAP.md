---
abstract: "Strategic API roadmap for K1 LightwaveOS ecosystem expansion. Covers current API inventory (150 WS + 110 REST), consumer gap analysis (zone-mixer/Tab5/iOS), 10 integration opportunities (DMX, MIDI, OSC, multi-K1, etc.), and phased implementation plan. Read before planning any new API work."
---

# K1 Ecosystem API Roadmap

## 1. Current API Landscape

K1 LightwaveOS exposes a substantial API surface built up over 3 firmware major versions:

| Transport | Endpoints | Domains | Notes |
|-----------|-----------|---------|-------|
| WebSocket | ~150 commands | 15 | Real-time control, streaming, state sync |
| REST (v1) | ~110 endpoints | 20 | Configuration, discovery, bulk operations |
| UDP | 3 streams | 2 | LED frame broadcast, stimulus injection |
| Automatic broadcasts | 6 | 4 | Status, LED frames, audio metrics, heap |

**Total: ~260 addressable API surfaces across 3 transports.**

### Usage Reality

Only **23% of API surfaces are actively consumed** by any client (approximately 60 of 260). The remaining 77% are either:

- **Implemented but never wired to a consumer** (show, plugins, motion, narrative, stimulus, trinity, benchmark, filesystem)
- **Partially consumed** (only GET used, SET/LIST/DELETE unused)
- **Disabled at compile time** (filesystem commands behind `#ifdef`)

### Domains by Activity

| Status | Domains |
|--------|---------|
| **Actively used** | effects, parameters, palettes, zones, presets, audio, edgeMixer, colour correction, transitions, OTA, system/status |
| **Partially used** | motion (set only), show (play only), WiFi (status only) |
| **Completely unused** | plugins, narrative, stimulus, trinity, benchmark, filesystem (58+ commands) |

---

## 2. Consumer Capability Matrix

Three consumers exist today: zone-mixer (AtomS3 physical controller), Tab5 (M5Stack touchscreen), and iOS (companion app).

### What Each Consumer Can Do

| Capability | Zone-Mixer | Tab5 | iOS |
|------------|:----------:|:----:|:---:|
| Effect selection (next/prev/direct) | Partial | Yes | Yes |
| Effect parameter adjustment | No | Yes | Yes |
| Palette selection | Yes | Yes | Yes |
| Zone brightness/speed | Yes | Yes | Yes |
| Zone layout editing | No | No | Yes |
| Colour correction | No | Yes | Yes |
| EdgeMixer control | Yes | Yes | Yes |
| Transition control | No | No | Yes |
| Preset save/load | No | No | Yes |
| Audio metrics display | No | No | Yes |
| LED frame preview | No | No | Yes (UDP) |
| Show playback | No | No | Yes |
| OTA firmware update | No | No | Yes |
| System diagnostics | No | No | Yes |

### Key Asymmetries

**Zone-mixer** is the most limited consumer. It controls brightness, speed, palette, and EdgeMixer via physical encoders but cannot:
- Adjust per-effect parameters (no parameter discovery or slider mapping)
- Edit zone layouts
- Preview LED output
- Access audio metrics or spectral data
- Trigger transitions or shows

**Tab5** sits in the middle with effect parameters and colour correction but lacks:
- UDP streaming (no LED preview)
- Zone layout editing
- Show management
- Audio metrics beyond basic beat indicator

**iOS** is the most capable consumer with full WS + REST + UDP access, but even it does not use:
- Motion engine commands
- Narrative engine commands
- Plugin management
- Stimulus injection
- Trinity commands
- Benchmark commands

### Commands K1 Supports but Nobody Uses

| Domain | Command Count | Notes |
|--------|:------------:|-------|
| Show system | ~8 | Play exists, no create/edit/delete/progress |
| Plugin system | ~6 | LittleFS manifests, no remote management |
| Motion engine | ~5 | Phase rotation, particle physics — no consumer |
| Narrative engine | ~4 | Song structure mapping — no consumer |
| Stimulus | ~4 | External stimulus injection — no consumer |
| Trinity | ~3 | Cross-strip synchronisation — no consumer |
| Benchmark | ~3 | Performance profiling — no consumer |
| Filesystem | ~5 | Disabled behind compile flag |

### Commands Consumers Need but K1 Lacks

| Missing Command | Consumer | Status |
|-----------------|----------|--------|
| `zone.nextEffect` / `zone.prevEffect` | Zone-mixer, Tab5 | Being implemented separately |
| Effect parameter discovery (min/max/step) | Zone-mixer, Tab5 | Not exposed |
| Transition completion event | All | Not broadcast |
| Show playback progress | iOS | Not broadcast |
| Per-client broadcast subscription | All | No filtering mechanism |

---

## 3. Internal Capabilities Not Exposed

Ten firmware subsystems exist with no or minimal API exposure:

### 3.1 MusicalSaliency
Computes harmonic, rhythmic, and timbral saliency scores from audio. Used internally by effects but not queryable or streamable. External visualisers and AI integrations would benefit from these derived metrics.

### 3.2 NarrativeEngine
Maps song structure (intro/build/drop/breakdown) to effect intensity curves. Runs internally but provides no state queries, no override commands, and no progress events. Show sequencing depends on this but cannot coordinate with it.

### 3.3 MotionEngine
Phase rotation, particle physics, and spatial movement computations. Commands exist but no consumer uses them. No state broadcasts for external synchronisation.

### 3.4 Effect Parameter Ranges
Effects define parameters internally (min, max, step, default, display name) but this metadata is not discoverable via API. Consumers must hardcode slider ranges or use generic 0-255 ranges.

### 3.5 Full Spectral Data
K1 computes 256-bin FFT, 64-bin Goertzel, and 12-band chroma internally. Only octave bands (8 values) and basic metrics (RMS, beat, onset) are exposed via broadcast. External visualisers need configurable spectral resolution.

### 3.6 STM (Spectral-Temporal Modulation)
New analysis stage (in uncommitted changes) that extracts spectral-temporal modulation features. Not yet exposed via any API. Potential high-value stream for advanced audio-visual mapping.

### 3.7 Show System
10 shows stored in PROGMEM. Play command exists but no runtime creation, editing, deletion, or import. Shows cannot be uploaded from clients. No playback progress events.

### 3.8 Plugin System
LittleFS-based plugin manifests exist in firmware. No remote plugin management (install, remove, list, enable/disable). Plugins must be compiled into firmware.

### 3.9 Zone Composition
Advanced blend modes exist internally for compositing zone renders. Only basic zone parameters (brightness, speed, effect) are exposed. Blend mode, opacity, and composition order are not API-controllable.

### 3.10 Colour Correction Presets
Individual colour correction parameters are adjustable, but preset management (save/load/name named correction profiles) is not exposed.

---

## 4. Missing Broadcasts (Ecosystem Blockers)

K1 broadcasts status and LED frames automatically but lacks five events that block ecosystem features:

| Missing Broadcast | Impact | Blocked Features |
|-------------------|--------|------------------|
| **Transition completion** | Consumers cannot sequence actions after transitions finish | Automated show flows, iOS transition chaining |
| **Show playback progress** | No timeline indicator, no "show ended" event | iOS show UI, synchronised multi-K1 shows |
| **Motion engine state** | External visualisers cannot track motion phase | VJ software sync, multi-K1 motion coordination |
| **OTA progress** | Only start/complete known, no percentage | iOS progress bar, Tab5 update indicator |
| **Stimulus mode changes** | No notification when stimulus mode activates/deactivates | External sensor integration, automation triggers |

These are not feature requests — they are infrastructure gaps that block multiple downstream capabilities.

---

## 5. CRUD Completeness Gaps

### Complete Domains (13)
Full GET/SET/LIST/DELETE + event coverage:

Effects, Parameters, Palettes, Zones, Presets, Audio (basic), EdgeMixer, Colour Correction, System Status, WiFi Config, Brightness, Speed, Global Controls.

### Partial Domains (10)

| Domain | Has | Missing |
|--------|-----|---------|
| Transitions | SET, GET | Completion event |
| Shows | PLAY, LIST | CREATE, EDIT, DELETE, progress events |
| Motion | SET | GET state, broadcasts |
| Audio (advanced) | Basic metrics | Spectral streaming, saliency |
| Narrative | Internal only | All CRUD |
| OTA | START, STATUS | Progress events |
| Plugins | Internal manifest | LIST, INSTALL, REMOVE, ENABLE |
| Zone Composition | Basic params | Blend modes, opacity, ordering |
| Stimulus | INJECT | Mode change events |
| Colour Presets | Individual params | Named preset CRUD |

### Write-Only Domains (3)

| Domain | State |
|--------|-------|
| Trinity | SET commands only, no GET/LIST, no events |
| Filesystem | Disabled behind compile flag |
| Benchmark | Trigger only, no result retrieval |

---

## 6. Integration Opportunities

### 6.1 Show Sequencing — MUST HAVE
**Priority: P0 | Estimate: ~80h**

| Aspect | Detail |
|--------|--------|
| What K1 has | 10 PROGMEM shows, play/stop commands |
| What is missing | Runtime show CRUD, upload from clients, playback timeline, progress events, loop/shuffle modes |
| Minimum viable API | `show.create`, `show.update`, `show.delete`, `show.upload`, `show.progress` broadcast |
| Why it matters | Shows are the primary "set and forget" user experience. Without runtime creation, every show requires a firmware rebuild |

### 6.2 Audio Data Streaming — MUST HAVE
**Priority: P0 | Estimate: ~40h**

| Aspect | Detail |
|--------|--------|
| What K1 has | Full spectral pipeline (256-bin FFT, 64-bin Goertzel, 12-band chroma, saliency) |
| What is missing | Configurable spectral stream (resolution, rate, feature selection) |
| Minimum viable API | `audio.stream.configure` (features, resolution, rate), `audio.stream.start/stop`, UDP spectral packets |
| Why it matters | External visualisers, VJ software, and AI effect generators need raw audio features |

### 6.3 DMX/ArtNet Bridge — SHOULD HAVE
**Priority: P1 | Estimate: ~120h**

| Aspect | Detail |
|--------|--------|
| What K1 has | 320 individually addressable LEDs, zone system |
| What is missing | ArtNet UDP listener (port 6454), DMX universe mapping, fixture profile |
| Minimum viable API | ArtNet receiver on UDP 6454, DMX→LED mapping config, fixture personality file |
| Why it matters | Professional stage integration. K1 becomes a controllable fixture in existing lighting rigs |

### 6.4 OSC Integration — SHOULD HAVE
**Priority: P1 | Estimate: ~60h**

| Aspect | Detail |
|--------|--------|
| What K1 has | WebSocket command router with type-based dispatch |
| What is missing | OSC UDP listener, address-to-command mapping, OSC output for state feedback |
| Minimum viable API | OSC listener on configurable port, `/k1/effect/select`, `/k1/zone/brightness`, `/k1/audio/beat` output |
| Why it matters | VJ industry standard (Resolume, TouchDesigner, Pure Data, Max/MSP). Opens K1 to the creative coding community |

### 6.5 MIDI Control — SHOULD HAVE
**Priority: P1 | Estimate: ~80h**

| Aspect | Detail |
|--------|--------|
| What K1 has | Parameter system with normalised 0.0-1.0 ranges |
| What is missing | USB MIDI device class, CC-to-parameter mapping, note-to-effect trigger, MIDI learn mode |
| Minimum viable API | USB MIDI device (ESP32-S3 native USB), CC mapping table, `midi.learn` command, note trigger config |
| Why it matters | DAW integration (Ableton, Reaper, Logic). Musicians control lights directly from their session |

### 6.6 Multi-K1 Synchronisation — SHOULD HAVE
**Priority: P1 | Estimate: ~100h**

| Aspect | Detail |
|--------|--------|
| What K1 has | Accurate beat tracking, internal tempo, AP WiFi |
| What is missing | Leader/follower protocol, beat sync packets, effect coordination, shared show playback |
| Minimum viable API | `sync.discover` (find other K1s), `sync.setRole` (leader/follower), beat sync UDP broadcast, shared effect state |
| Why it matters | Multi-fixture installations. Bars, stages, and installations with 2-10 K1 units need coordinated behaviour |

### 6.7 Cross-Platform Discovery — SHOULD HAVE
**Priority: P1 | Estimate: ~40h**

| Aspect | Detail |
|--------|--------|
| What K1 has | mDNS (`k1.local`), AP SSID broadcast |
| What is missing | SSDP/UPnP for Windows/Android (mDNS unreliable on those platforms), device description XML |
| Minimum viable API | SSDP responder, UPnP device description at `/description.xml`, discovery response with capabilities |
| Why it matters | Windows and Android clients cannot reliably discover K1 via mDNS. UPnP is the fallback standard |

### 6.8 Generative/AI Effects — COULD HAVE
**Priority: P2 | Estimate: ~60h**

| Aspect | Detail |
|--------|--------|
| What K1 has | UDP frame injection (stimulus), 320-LED frame buffer |
| What is missing | Documented frame injection protocol, ML model output format, external effect registration |
| Minimum viable API | `stimulus.register` (declare external source), documented UDP frame format, frame rate negotiation |
| Why it matters | External ML models (running on phone/laptop) can generate LED patterns and inject them into K1 |

### 6.9 Remote Visualisation — COULD HAVE
**Priority: P2 | Estimate: ~50h**

| Aspect | Detail |
|--------|--------|
| What K1 has | LED frame broadcast via UDP, status broadcast via WS |
| What is missing | HTML5 Canvas LED preview endpoint, diagnostics dashboard, performance graphs |
| Minimum viable API | WebSocket LED frame relay (lower bandwidth than UDP), `/dashboard` static page, real-time heap/FPS/show-time charts |
| Why it matters | Debug and demo without physical hardware. Remote monitoring for installed units |

### 6.10 Sensor/Trigger Integration — COULD HAVE
**Priority: P2 | Estimate: ~70h**

| Aspect | Detail |
|--------|--------|
| What K1 has | GPIO, I2C (PaHub), stimulus injection |
| What is missing | Sensor polling framework, trigger-to-action mapping, external sensor registration via API |
| Minimum viable API | `trigger.register` (sensor type, threshold), `trigger.map` (sensor→action), I2C sensor auto-detect |
| Why it matters | Proximity, motion, temperature, and ambient light sensors driving effect changes. Interactive installations |

---

## 7. API Design Principles

### Transport Selection

| Transport | Use For | Latency | Bandwidth |
|-----------|---------|---------|-----------|
| **WebSocket** | Real-time commands, state sync, event subscriptions | <10ms | Medium |
| **REST** | Configuration, discovery, bulk operations, firmware info | <100ms | Low |
| **UDP** | High-bandwidth streaming (LED frames, audio spectral, stimulus) | <5ms | High |

**No BLE.** K1 hardware has no BLE radio. Do not design APIs that assume BLE availability.

### Command Format (WebSocket)

All WebSocket commands follow the existing pattern:

```json
{
  "type": "domain.action",
  "payload": { ... }
}
```

Response:

```json
{
  "type": "domain.action.response",
  "success": true,
  "payload": { ... }
}
```

### Versioning

- **v1** is the current and only version. All endpoints live under `/api/v1/`.
- **v2** will only be introduced for breaking changes. Additive changes (new commands, new fields) do not require a version bump.
- Both versions will coexist for at least 2 firmware releases after v2 introduction.

### Broadcast Subscription Model

Currently, all connected clients receive all broadcasts. Future broadcasts should support per-client subscription via bitmask:

```json
{
  "type": "subscribe",
  "events": ["status", "audio.metrics", "led.frame", "transition.complete"]
}
```

Unsubscribed events are not sent, reducing WiFi bandwidth for limited clients (zone-mixer, Tab5).

### Authentication

- **Current:** Optional API key in header (`X-API-Key`) — disabled by default.
- **Future:** Role-based access control (admin/operator/viewer) for multi-user installations.
  - **Admin:** Full access (OTA, WiFi config, zone layout, shows)
  - **Operator:** Effect control, parameters, presets, playback
  - **Viewer:** Read-only status, LED preview, audio metrics

---

## 8. Implementation Roadmap

### Phase 1: v3.3.0 — Foundation
**Focus:** Show sequencing + audio streaming + infrastructure

| Work Item | Estimate | Dependencies |
|-----------|----------|--------------|
| Show CRUD API (create, edit, delete, upload) | 40h | LittleFS storage |
| Show playback events (progress, complete) | 10h | Show CRUD |
| Audio spectral streaming (configurable UDP) | 30h | None |
| Broadcast subscription model | 15h | None |
| Effect parameter discovery API | 15h | None |
| Transition completion broadcast | 5h | None |
| Contract YAML updates + consumer SDK notes | 10h | All above |
| **Phase total** | **~125h** | |

### Phase 2: v3.4.0 — Professional Integration
**Focus:** DMX + OSC + MIDI + discovery

| Work Item | Estimate | Dependencies |
|-----------|----------|--------------|
| ArtNet/DMX receiver + fixture profile | 80h | None |
| OSC listener + address mapping | 40h | None |
| USB MIDI device + CC mapping + learn mode | 60h | None |
| SSDP/UPnP discovery responder | 25h | None |
| Cross-platform discovery documentation | 10h | SSDP |
| **Phase total** | **~215h** | |

### Phase 3: v3.5.0 — Multi-Device + Intelligence
**Focus:** Multi-K1 sync + generative effects + remote viz

| Work Item | Estimate | Dependencies |
|-----------|----------|--------------|
| Multi-K1 discovery + leader/follower protocol | 60h | SSDP (Phase 2) |
| Beat sync UDP broadcast + drift correction | 40h | Multi-K1 discovery |
| External frame injection protocol + docs | 30h | None |
| HTML5 LED preview + diagnostics dashboard | 40h | LED broadcast |
| Role-based authentication | 30h | None |
| **Phase total** | **~200h** | |

### Phase 4: v3.6.0+ — Ecosystem Maturity
**Focus:** Sensors + plugins + show editor

| Work Item | Estimate | Dependencies |
|-----------|----------|--------------|
| Sensor polling framework + trigger mapping | 50h | None |
| Remote plugin management (install/remove/enable) | 40h | LittleFS |
| Show editor UI (web-based) | 60h | Show CRUD (Phase 1) |
| Custom effect upload (WASM or scripted) | 80h | Plugin system |
| **Phase total** | **~230h** | |

### Total Estimated Effort

| Phase | Version | Hours | Priority |
|-------|---------|------:|----------|
| Phase 1 | v3.3.0 | 125h | MUST HAVE |
| Phase 2 | v3.4.0 | 215h | SHOULD HAVE |
| Phase 3 | v3.5.0 | 200h | SHOULD HAVE |
| Phase 4 | v3.6.0+ | 230h | COULD HAVE |
| **Total** | | **770h** | |

---

## 9. Open Questions

1. **Cloud sync for shows?** User-created shows could be backed up to a cloud service, but K1 has no internet access (AP-only). Sync would require the companion app as a relay. Privacy implications for audio-derived show data need consideration.

2. **WiFi bandwidth limits for multi-K1 sync?** K1's AP supports ~4 concurrent clients reliably. A 10-unit installation would need mesh WiFi or a dedicated router with all K1s as clients (requires STA mode — currently architecturally prohibited). Leader-as-AP with followers-as-STA is one option but conflicts with the AP-only constraint.

3. **Beat sync latency budget?** Recommend 50ms maximum for perceptible synchronisation. WiFi round-trip on a local AP is typically 2-5ms, so the budget is achievable for 2-4 units. Larger installations need investigation.

4. **Content licensing for user-uploaded shows?** If shows contain effect sequences that reference proprietary effect IDs, sharing shows between users implicitly shares the effect catalogue. Need to define what a "show file" contains and whether it is portable.

5. **Backward compatibility for v1 clients on v3.4+ K1?** Existing iOS/Tab5/zone-mixer consumers must continue working when K1 adds new API surfaces. Strategy: additive-only changes in v1, no field removals, new domains are opt-in via subscription model.

6. **STM streaming priority?** The Spectral-Temporal Modulation extractor is new and uncommitted. Should it be exposed in Phase 1 alongside basic audio streaming, or deferred until the feature stabilises?

7. **ArtNet universe count?** K1 has 320 LEDs (960 channels) — fits in 2 DMX universes. Should the ArtNet bridge support arbitrary universe counts for future hardware with more LEDs?

8. **MIDI over WiFi?** USB MIDI requires physical connection. rtpMIDI (AppleMIDI) over WiFi is an alternative but adds latency. Worth supporting both?

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-04-01 | agent:research-synthesis | Created from 7-SSA parallel research (API surface, consumer matrix, ecosystem vision) |
