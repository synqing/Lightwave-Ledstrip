# SpectraSynq — System-of-Systems Founding Architecture

**Date:** 2026-02-10
**Author:** Founding Principal Architect (SpectraSynq)
**Status:** PHASE B — Architecture Proposal (awaiting review)
**Scope:** Full system design across device, hub, apps, cloud, ML, and protocol layers.

**Companion documents:**
- Constitution invariants (I1–I14): `00_CONSTITUTION.md`
- Decision register (D1–D13): `01_DECISION_REGISTER.md`
- AS-IS architecture: `02_AS_IS_ARCHITECTURE_ATLAS.md`
- Edge interface contracts: `06_INTERFACE_CONTRACTS.md`
- Edge migration plan: `07_MIGRATION_PLAN.md`

**Governing principle:**
> SpectraSynq exists to make music visible so people can feel, shape, and share it — not just listen to it.

---

## 0. Notation and Conventions

- **[CANON]** — Edge Canon invariant or decision; non-negotiable.
- **[DECISION]** — New decision made in this spec; traceable to user answers.
- **[ASSUMPTION]** — Unconfirmed belief; validation step provided.
- **[DEFERRED]** — Explicitly out of scope for MVP; revisit trigger documented.
- British English throughout.

---

## 1. Scope Map — Subsystems, Boundaries, Responsibilities

### 1.1 Subsystem Registry

```
┌─────────────────────────────────────────────────────────────────┐
│                        SPECTRASYNQ                              │
│                                                                 │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────────────────┐ │
│  │  K1_RUNTIME   │  │  TAB5_HUB    │  │   LIGHTWAVE_APP       │ │
│  │  (ESP32-S3)   │  │  (ESP32-P4)  │  │   (iOS / SwiftUI)    │ │
│  │  LED render   │  │  Controller  │  │   Browse, perform,   │ │
│  │  authority    │  │  + local hub │  │   light edit          │ │
│  └──────┬───────┘  └──────┬───────┘  └───────────┬───────────┘ │
│         │                  │                      │             │
│         └──────────┬───────┴──────────────────────┘             │
│                    │  TRINITY_PROTOCOL (wire)                   │
│         ┌──────────┴──────────────────────┐                     │
│         │                                 │                     │
│  ┌──────┴───────┐  ┌─────────────┐  ┌────┴────────────────┐   │
│  │ TRINITY_      │  │ PRISM_       │  │ PRISM_API            │  │
│  │ PIPELINE      │  │ STUDIO       │  │ (Cloud backend)      │  │
│  │ (Desktop ML)  │  │ (Web app)    │  │ Content + community  │  │
│  └──────────────┘  └─────────────┘  └───────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

### 1.2 Subsystem Definitions

| Subsystem | Runtime | Authority | Owner of |
|-----------|---------|-----------|----------|
| **K1_RUNTIME** | ESP32-S3 firmware | Render authority | LED frame buffers, effect execution, centre-origin semantics, real-time DSP |
| **TAB5_HUB** | ESP32-P4 firmware | Local orchestration authority (when no laptop/cloud) | Cached show content, local session state, encoder input |
| **TRINITY_PIPELINE** | Desktop Python (conda) | Analysis authority | Beat/segment/stem data for a given audio file |
| **PRISM_STUDIO** | Web application (browser) | Authoring authority | Show definitions, timeline compositions, visual mappings |
| **PRISM_API** | Cloud backend | Content + identity authority | User accounts, ShowBundles, versioning, distribution, community |
| **LIGHTWAVE_APP** | iOS (SwiftUI) | Performance + browse authority | Local device control, show playback, content browsing |
| **TRINITY_PROTOCOL** | Wire format (not a runtime) | Contract authority | Message schemas, versioning rules, sequencing semantics |

### 1.3 Boundary Rules

1. **K1_RUNTIME is the only subsystem that writes LED frames.** [CANON: I1, I7, I12]
   No other subsystem bypasses RendererActor to push pixels.

2. **TRINITY_PIPELINE is the only subsystem that runs heavy ML inference.** [DECISION]
   K1_RUNTIME runs Goertzel DSP only. TAB5_HUB does not run ML. LIGHTWAVE_APP does not run ML.

3. **PRISM_API is the only subsystem with persistent user identity.** [DECISION]
   Device/hub/app authenticate against PRISM_API for content operations. Local-only mode works without accounts.

4. **Every subsystem communicates through TRINITY_PROTOCOL messages.** [DECISION]
   No proprietary side-channels. REST endpoints are typed by TRINITY_PROTOCOL schemas.

5. **Every subsystem works offline.** [DECISION]
   Cloud enhances but never gates core functionality. A device + phone + Tab5 must be fully functional with no internet.

---

## 2. End-to-End Dataflow

### 2.1 The Two Paths

There are two fundamentally different paths through the system. Both terminate at the same RendererActor, and both produce centre-origin LED frames.

#### Path A: Live Audio (real-time, no pre-computation)

```
Microphone (I2S SPH0645)
    ↓ 12.8 kHz mono
AudioActor (Core 0)
    ├→ Fast lane (125 Hz): RMS, spectral bands, flux
    └→ Beat lane (62.5 Hz): Goertzel tempo + PLL
    ↓ FeatureBus (lock-free ring buffer, PSRAM)
RendererActor (Core 1)
    ├→ MusicalGrid → beat phase interpolation
    ├→ Effect render (centre-origin, static buffers)
    └→ OutputDriver → WS2812 dual-strip
```

This path exists today and is fully functional. [CANON: D8, D9, I4, I5, I7]

#### Path B: Trinity-Mapped Show (pre-computed, choreographed)

```
Audio File (WAV, user-supplied)
    ↓
TRINITY_PIPELINE (desktop)
    ├→ Beat This! → beats, downbeats, BPM
    ├→ allin1-trinity → segments (intro/verse/chorus/bridge/outro)
    ├→ Demucs/external → stems (vocals/drums/bass/other)
    └→ Stem features → per-stem energy/presence timelines
    ↓
TrinityContract (JSON) + optional StemPack
    ↓
PRISM_STUDIO (web)
    ├→ Import TrinityContract + audio waveform
    ├→ Visualise: waveform + beat markers + segment boundaries + stem layers
    ├→ Author: assign effects, palettes, automations to segments/stems/beats
    ├→ Preview: simulated LED strip rendering
    └→ Export: ShowBundle
    ↓
PRISM_API (cloud, optional)
    ├→ Store ShowBundle with metadata
    ├→ Version, attribute, distribute
    └→ Community feed
    ↓
LIGHTWAVE_APP (iOS) or TAB5_HUB
    ├→ Download ShowBundle
    ├→ Transfer show definition to K1_RUNTIME (REST)
    ├→ User plays their own audio file
    ├→ App/hub streams trinity.beat/macro/segment in sync with playback
    ↓
K1_RUNTIME (ESP32-S3)
    ├→ ShowDirectorActor loads show definition
    ├→ TrinityControlBusProxy maps incoming macros → ControlBusFrame
    ├→ MusicalGrid → external sync mode (PLL bypassed)
    ├→ RendererActor renders per show cues + Trinity features
    └→ OutputDriver → WS2812 dual-strip
```

### 2.2 The Bridge Problem (the user's stated blocker)

The critical gap is between TRINITY_PIPELINE output and PRISM_STUDIO authoring. Specifically:

**From Trinity, the system knows:**
- When every beat and downbeat occurs
- Song structure (verse at 13s–37s, chorus at 37s–58s, etc.)
- Per-stem presence (vocals loud here, drums drop out there)
- Energy/brightness/percussiveness timelines

**What the creator needs to do:**
- See this data as a visual timeline (like a DAW arrangement view)
- Assign effects/palettes/transitions to segments ("chorus = BeatPulseBloom + palette Sunset")
- Draw automation curves over stem layers ("vocal energy → brightness ramp")
- Set zone assignments per segment ("zone 1 = this effect, zone 2 = that effect")
- Preview the result in a simulated LED strip

**What ShowDirectorActor needs to receive:**
- A sequence of ShowCues with timestamps, effect IDs, palette IDs, parameter sweeps
- Chapter definitions with narrative phases (BUILD/HOLD/RELEASE/REST)
- Zone configurations per chapter/cue

**The bridge is a mapping function:**
```
TrinityContract × CreatorIntent → ShowDefinition
```

This mapping function IS PRISM_STUDIO. Everything else is plumbing.

---

## 3. TRINITY_PROTOCOL — The Wire Contract

### 3.1 Protocol Envelope

Every TRINITY_PROTOCOL message, regardless of transport, is wrapped:

```json
{
  "v": 1,
  "type": "trinity.beat",
  "seq": 42,
  "ts_ms": 1707580800000,
  "payload": { ... }
}
```

| Field | Type | Semantics |
|-------|------|-----------|
| `v` | uint8 | Protocol version. Current: `1`. Receivers MUST accept `v <= current`. |
| `type` | string | Message type (dot-namespaced). |
| `seq` | uint32 | Monotonic per-type, per-sender. Wraps at 2^32. |
| `ts_ms` | uint64 | Sender timestamp (millis since epoch, or millis since session start for local). |
| `payload` | object | Type-specific data. |

### 3.2 Message Type Catalogue

#### Category 1: Analysis Artifacts (store-and-forward)

These are produced by TRINITY_PIPELINE, stored as files, and consumed by PRISM_STUDIO and K1_RUNTIME.

**`trinity.contract`** — Full analysis output for one audio file.
```yaml
type: trinity.contract
payload:
  contract_version: "1.0.0"
  audio_hash: string           # SHA-256 of source WAV
  audio_duration_sec: float64
  bpm: float32
  beats: float64[]             # seconds
  downbeats: float64[]         # seconds
  beat_positions: uint8[]      # 1-indexed position within bar
  time_signature: uint8        # beats per bar (usually 4)
  segments:
    - start: float64
      end: float64
      label: string            # canonical: intro|verse|chorus|bridge|outro|instrumental|start|end
  stems:                       # optional
    vocals: string             # relative path to WAV
    drums: string
    bass: string
    other: string
  stem_features:               # optional, per-stem timelines
    sample_rate_hz: float32    # typically 30 (30 FPS macro updates)
    vocals: float32[]
    drums: float32[]
    bass: float32[]
    other: float32[]
  analysis_model_versions:
    beat_tracker: string       # "beat_this_v1.0"
    segmenter: string          # "allin1_trinity_v0.2"
    separator: string          # "demucs_htdemucs_v4" or "external"
```

[DECISION: contract_version field enables schema evolution without breaking consumers.]
[DECISION: audio_hash enables caching — same file = same analysis unless model_versions change.]

**`trinity.show_bundle`** — A complete show definition with Trinity reference.
```yaml
type: trinity.show_bundle
payload:
  bundle_version: "1.0.0"
  id: uuid
  name: string
  author: string               # display name
  author_id: uuid?             # PRISM_API user ID (null for local-only)
  parent_id: uuid?             # fork source (null if original)
  created_at: iso8601
  updated_at: iso8601

  # What audio this show was authored against
  audio_reference:
    title: string
    artist: string
    duration_sec: float64
    audio_hash: string?        # SHA-256 of WAV (for matching)
    contract_hash: string?     # SHA-256 of TrinityContract used during authoring

  # The show itself
  song_specific: boolean       # true = choreographed to this song; false = generic/ambient

  chapters:
    - name: string
      start_ms: uint32
      duration_ms: uint32
      narrative_phase: enum(BUILD|HOLD|RELEASE|REST)
      tension_level: uint8     # 0–255
      cue_start_index: uint16
      cue_count: uint16

  cues:
    - time_ms: uint32
      type: enum(EFFECT|PARAMETER_SWEEP|ZONE_CONFIG|PALETTE|NARRATIVE|TRANSITION|MARKER)
      target_zone: uint8       # 0 = global, 1–4 = zone
      data: object             # type-specific payload

  # Zone layout (how many zones, LED segment ranges)
  zone_config:
    zone_count: uint8          # 1–4
    zones:
      - effect_id: uint8
        palette_id: uint8
        brightness: uint8
        speed: uint8
        blend_mode: uint8
        segment_start: uint16
        segment_end: uint16

  # Metadata for community
  tags: string[]
  description: string
  preview_url: string?         # URL to preview video/GIF
  license: enum(CC_BY|CC_BY_SA|CC_BY_NC|ALL_RIGHTS_RESERVED)
```

[DECISION: ShowBundle is the shareable unit. It references audio (by hash) but does NOT contain audio.]
[DECISION: `song_specific: boolean` distinguishes generic shows from choreographed ones.]
[DECISION: `parent_id` enables fork/remix tracking with attribution chain.]

#### Category 2: Playback Sync (real-time, local network)

These are streamed over WebSocket from app/hub to K1_RUNTIME during playback. They already exist in the firmware; this formalises them.

**`trinity.sync`** — Playback transport control.
```yaml
payload:
  action: enum(start|stop|pause|resume|seek)
  position_sec: float64
  bpm: float32
```

**`trinity.beat`** — Beat event (streamed in sync with audio playback).
```yaml
payload:
  bpm: float32
  beat_phase: float32          # 0.0–1.0
  tick: boolean
  downbeat: boolean
  beat_index: uint32
  beat_in_bar: uint8           # 1-indexed
```

**`trinity.macro`** — Audio macro features (streamed at 30 FPS).
```yaml
payload:
  t: float64                   # playback position in seconds
  energy: float32              # 0.0–1.0
  vocal_presence: float32
  bass_weight: float32
  percussiveness: float32
  brightness: float32
```

**`trinity.segment`** — Structural segment change.
```yaml
payload:
  index: uint16
  label: string                # canonical label
  start: float64
  end: float64
```

**`trinity.show_cue`** — Show cue event (streamed by app/hub during show playback). [NEW]
```yaml
payload:
  cue_index: uint16
  type: enum(EFFECT|PARAMETER_SWEEP|ZONE_CONFIG|PALETTE|NARRATIVE|TRANSITION|MARKER)
  target_zone: uint8
  data: object
```

[DECISION: Show cues can be streamed in real-time (app as conductor) OR pre-loaded onto device. Both paths valid.]

#### Category 3: Device Telemetry (device → app/hub, lightweight)

**`device.state`** — Current device state snapshot. [EXISTING: via WebSocket]
```yaml
payload:
  effect_id: uint8
  palette_id: uint8
  brightness: uint8
  speed: uint8
  zone_count: uint8
  zones: object[]
  audio_mode: enum(live|trinity|off)
  show_playing: boolean
  show_id: string?
```

**`device.audio_features`** — Live DSP features from microphone. [NEW, opt-in]
```yaml
payload:
  rms: float32
  bands: float32[8]
  tempo_bpm: float32
  beat_phase: float32
  beat_confidence: float32
```

[DECISION: Device is a Trinity CONSUMER for heavy analysis, but a lightweight PRODUCER of live DSP telemetry.]

### 3.3 Versioning Rules

1. Protocol version (`v`) increments only when envelope structure changes.
2. Payload fields are evolved additively: new optional fields may be added; existing fields are never removed or retyped.
3. Consumers MUST ignore unknown fields.
4. Producers MUST include all required fields for the declared `v`.
5. `contract_version` and `bundle_version` are independent of protocol version `v`.

### 3.4 Transport Bindings

| Transport | Used for | Subsystems |
|-----------|----------|------------|
| **WebSocket (JSON)** | Real-time sync, telemetry, commands | K1_RUNTIME ↔ LIGHTWAVE_APP, TAB5_HUB |
| **REST (JSON)** | CRUD operations, state queries, file transfers | K1_RUNTIME ↔ LIGHTWAVE_APP, TAB5_HUB |
| **UDP (binary)** | High-frequency LED frame streaming, audio feature streaming | K1_RUNTIME → LIGHTWAVE_APP (opt-in) |
| **HTTPS (JSON)** | Content API, user API | PRISM_API ↔ all clients |
| **File (JSON)** | Trinity contracts, ShowBundles, stem packs | TRINITY_PIPELINE → PRISM_STUDIO → PRISM_API |

[CANON: REST + WebSocket are stable control-plane invariants (I9, D1)]

### 3.5 Failure Semantics

| Failure | Detection | Response | Recovery |
|---------|-----------|----------|----------|
| WebSocket disconnect | TCP close / timeout | K1_RUNTIME: hold last state, decay macros to zero over 250ms | Reconnect with exponential backoff; resume from last `seq` |
| Sequence gap (real-time) | `seq` not contiguous | Drop gap, continue from latest | No retransmit for real-time streams |
| Sequence gap (store-and-forward) | `seq` not contiguous | Request retransmit | Sender re-sends missing range |
| Clock drift > 50ms | Measured by PLL phase error | Snap to latest trinity.beat position | Re-sync on next trinity.sync seek |
| Corrupt ShowBundle | CRC/hash mismatch | Reject, notify user | Re-download from PRISM_API |
| Unknown message type | `type` field not recognised | Log warning, ignore | No action needed |

---

## 4. PRISM_PLATFORM Architecture

### 4.1 Stack Decision

[ASSUMPTION: Web application is the correct Day 1 platform for PRISM_STUDIO.]

**Rationale:**
1. PRISM Dashboard V4 already exists as a PWA with PixiJS, WaveSurfer, WebSocket integration, and Trinity data visualisation. It is 80% of the viewing experience.
2. Web is cross-platform from Day 1 (macOS, Windows, Linux, iPad).
3. Web is AI-native: AI-assisted authoring integrates via API calls, no native SDK constraints.
4. Modern web handles timeline editors well (see: Remotion, PixiJS timeline, WaveSurfer regions).
5. PWA gives offline capability + installable experience.
6. Can be wrapped in Tauri/Electron later if native desktop features (file system, MIDI, audio routing) become essential.

**Validation step:** Build a proof-of-concept timeline editor in PRISM Dashboard V5 with drag-drop effect assignment to Trinity segments. If the web platform proves insufficient for the DAW-like experience (latency, file handling, audio routing), reassess with Tauri wrapper.

**Alternative considered:** Reaper DAW plugin (user mentioned this). This would give immediate access to Reaper's timeline, audio routing, and plugin ecosystem. However, it locks SpectraSynq to a niche tool, limits the non-technical audience, and creates a deep dependency on Cockos's plugin API. [DEFERRED: Revisit if PRISM_STUDIO needs professional audio routing that web cannot provide.]

### 4.2 PRISM_STUDIO Components

```
PRISM_STUDIO (Web Application)
├── SongImporter
│   ├── Audio file loader (WAV, drag-drop)
│   ├── Trinity contract loader (JSON)
│   └── Stem pack loader (optional)
│
├── TimelineEditor (the core authoring surface)
│   ├── WaveformTrack (WaveSurfer, audio playback + scrubbing)
│   ├── BeatMarkerTrack (vertical lines at beat positions from TrinityContract)
│   ├── SegmentTrack (coloured blocks for verse/chorus/bridge from TrinityContract)
│   ├── StemTracks (4 lanes: vocals/drums/bass/other, showing energy curves)
│   ├── EffectLane (drag-drop effect assignments to time ranges)
│   ├── PaletteLane (drag-drop palette assignments to time ranges)
│   ├── AutomationLane (drawable curves: brightness, speed, mood, trails, etc.)
│   ├── ZoneLane (per-zone overrides)
│   └── TransitionLane (transition types at boundaries)
│
├── EffectBrowser
│   ├── Grid of available effects (thumbnails, categories)
│   ├── Parameter inspector (sliders for selected effect's params)
│   └── AI assist: "suggest effects for this chorus" [DEFERRED: post-MVP]
│
├── PaletteBrowser
│   ├── Swatch grid of 75+ palettes
│   ├── Category filters (Artistic, Scientific, Custom)
│   └── Palette preview on simulated strip
│
├── LEDPreview
│   ├── Simulated 320-LED strip (Canvas/WebGL, centre-origin)
│   ├── Real-time playback preview
│   └── Optional: connect to real device for live preview
│
├── ShowExporter
│   ├── Generate ShowBundle (JSON)
│   ├── Validate against TRINITY_PROTOCOL schema
│   ├── Upload to PRISM_API (if signed in)
│   └── Export to file (for offline sharing)
│
└── DeviceConnector
    ├── WebSocket connection to K1_RUNTIME
    ├── Stream trinity.beat/macro/segment during preview
    └── Upload ShowBundle to device
```

### 4.3 PRISM_API Components

[ASSUMPTION: Supabase (or equivalent BaaS) for MVP backend to minimise custom infrastructure.]

**Validation step:** Evaluate Supabase row-level security, storage, and real-time features against ShowBundle CRUD + community requirements. If insufficient, fall back to custom API (FastAPI + PostgreSQL + S3).

```
PRISM_API (Cloud Backend)
├── AuthService
│   ├── Sign in with Apple, Google (OAuth2/OIDC)
│   ├── Email/password fallback
│   ├── JWT token issuance
│   └── User profile (display name, avatar, bio)
│
├── ContentService
│   ├── ShowBundle CRUD
│   │   ├── Create: upload JSON + metadata
│   │   ├── Read: fetch by ID, list by user, search by tags
│   │   ├── Update: new version (immutable versions, mutable metadata)
│   │   └── Delete: soft delete (tombstone, 30-day retention)
│   ├── Versioning: every save = new version, old versions accessible
│   ├── Forking: clone bundle, set parent_id, attribute original author
│   └── TrinityContract storage (cached by audio_hash + model_versions)
│
├── DistributionService
│   ├── CDN for ShowBundle downloads
│   ├── Device sync: LIGHTWAVE_APP polls for updates to pinned shows
│   └── Offline bundle packaging (zip with all dependencies)
│
├── CommunityService
│   ├── Feed: chronological + trending
│   ├── Likes / saves
│   ├── Comments (on ShowBundles)
│   ├── Attribution graph (fork chains)
│   └── User profiles + follow system
│
└── ModerationService
    ├── Automated: name/description content filtering
    ├── Community: flag + report
    └── Manual review queue
```

### 4.4 Content Lifecycle

```
1. AUTHOR
   Creator opens PRISM_STUDIO
   → Imports audio file + runs/imports Trinity analysis
   → Builds show on timeline
   → Previews on simulated strip (or live device)
   → Exports ShowBundle

2. PUBLISH (optional)
   Creator signs in to PRISM_API
   → Uploads ShowBundle
   → Adds metadata (title, description, tags)
   → Chooses licence (CC-BY, CC-BY-SA, CC-BY-NC, All Rights Reserved)
   → ShowBundle appears in community feed

3. DISCOVER
   Another user browses PRISM_API (via LIGHTWAVE_APP or PRISM_STUDIO)
   → Finds a ShowBundle they like
   → Downloads it

4. PLAY
   User has the same song (or any song, for generic shows)
   → Loads ShowBundle onto K1_RUNTIME (via app or Tab5)
   → Plays audio on phone/tablet/computer
   → App streams trinity.beat/macro/segment to device in sync
   → Device renders the show

5. FORK (optional)
   User opens downloaded ShowBundle in PRISM_STUDIO
   → Modifies timeline (different effects, palettes, automations)
   → Saves as new ShowBundle with parent_id pointing to original
   → Publishes: attribution chain is visible
```

### 4.5 Storage Model

| Content Type | Storage | Format | Size Estimate |
|-------------|---------|--------|---------------|
| ShowBundle | PRISM_API object storage | JSON | 10–100 KB |
| TrinityContract | PRISM_API object storage (cached) | JSON | 20–200 KB |
| StemPack manifest | PRISM_API object storage | JSON | 1–5 KB |
| Stem audio files | NOT stored (user-local) | WAV | N/A |
| Source audio | NOT stored (user-local) | WAV | N/A |
| ShowBundle on device | K1_RUNTIME LittleFS | JSON (compressed) | 5–50 KB |
| User profile | PRISM_API database | Relational | < 1 KB |

[DECISION: Audio files never touch SpectraSynq servers. Only metadata, analysis, and show definitions are stored.]

---

## 5. ML Architecture

### 5.1 Pipeline Topology

```
                    ┌─────────────────────────┐
                    │    TRINITY_PIPELINE      │
                    │    (Desktop Python)      │
                    │                          │
Audio WAV ────────→ │  ┌──────────────────┐   │
                    │  │ Beat This!        │   │──→ TrinityContract (JSON)
                    │  │ (PyTorch, MPS/CPU)│   │
                    │  └──────────────────┘   │
                    │  ┌──────────────────┐   │
                    │  │ allin1-trinity    │   │
                    │  │ (PyTorch, CPU)    │   │
                    │  └──────────────────┘   │
                    │  ┌──────────────────┐   │
External stems ───→ │  │ Stem features    │   │──→ StemPack (files + JSON)
                    │  │ (librosa)        │   │
                    │  └──────────────────┘   │
                    └─────────────────────────┘
```

### 5.2 Runtime Placement

| Capability | Where | Why |
|-----------|-------|-----|
| Beat/downbeat tracking | Desktop (Beat This!, PyTorch) | Requires ~500 MB model, GPU beneficial |
| Structure segmentation | Desktop (allin1-trinity, PyTorch) | Requires NATTEN attention, CPU-only on macOS |
| Stem separation | External service (LALAL.AI, Moises) or Desktop (Demucs, if not macOS) | GPU-heavy, OpenMP conflicts on macOS |
| Stem feature extraction | Desktop (librosa) | Lightweight, follows stem separation |
| Real-time DSP (live mic) | K1_RUNTIME (Goertzel, on-chip) | Must be <16ms latency, no network dependency |
| Live audio telemetry | K1_RUNTIME → app (WebSocket) | Lightweight feature frames, opt-in |

[DECISION: No ML on device. No ML on phone. Desktop pipeline for Day 1.]
[DEFERRED: Cloud-hosted analysis. Trigger: when non-technical user onboarding friction exceeds acceptable threshold.]

### 5.3 Latency Budgets

| Path | Budget | Measured At |
|------|--------|-------------|
| Live mic → DSP features | 8–16 ms | AudioActor output (FeatureBus write) |
| Live mic → LED frame | 18–26 ms | OutputDriver LED push |
| Trinity playback → LED frame | < 50 ms | App streams trinity.beat → device renders |
| Offline analysis (per song) | 2–5× real-time | TRINITY_PIPELINE completion |

### 5.4 Model Lifecycle

1. Models are versioned independently: `beat_this_v1.0`, `allin1_trinity_v0.2`, etc.
2. TrinityContract records which model versions produced it (`analysis_model_versions`).
3. When a model is updated, old contracts remain valid. New analysis can be triggered by user.
4. Model updates are distributed as TRINITY_PIPELINE updates (pip install / conda update).
5. [DEFERRED: Model A/B testing, automatic re-analysis on model update.]

### 5.5 Caching Strategy

- **Key:** `audio_hash + model_versions` tuple.
- **Cache location:** Local filesystem (`~/.spectrasynq/cache/`) + optional PRISM_API (shared across users with same audio file).
- **Invalidation:** Only when model version changes. Audio hash is immutable.
- **Size management:** LRU eviction when cache exceeds configurable limit (default 10 GB).

---

## 6. Security and Privacy Model

### 6.1 Principles

1. **Audio never leaves the user's device** unless they explicitly opt into cloud analysis. [DECISION]
2. **Shared content contains no audio** — only visual plans + metadata + analysis references. [DECISION]
3. **Device communication is local-network only** for Day 1. No cloud relay. [DECISION]
4. **User identity is optional.** All local functionality works without an account. [DECISION]
5. **Telemetry is opt-in.** Device never phones home without explicit consent. [DECISION]

### 6.2 Authentication

| Context | Method | Details |
|---------|--------|---------|
| PRISM_API | OAuth2 / OIDC | Sign in with Apple, Google. JWT access tokens (1 hr), refresh tokens (30 days). |
| K1_RUNTIME | None (local network) | Device trusts all clients on the same network. [ASSUMPTION: acceptable for home use.] |
| TAB5_HUB | None (local network) | Same as K1_RUNTIME. |
| PRISM_STUDIO → PRISM_API | Bearer token | JWT in Authorization header. |
| LIGHTWAVE_APP → PRISM_API | Bearer token + Keychain | JWT stored in iOS Keychain. |

[ASSUMPTION: Local-network device access without authentication is acceptable for MVP.]
[Validation step: Assess threat model for shared networks (co-working spaces, venues). If needed, add device pairing code.]

### 6.3 Data Classification

| Data | Classification | Storage | Encryption |
|------|---------------|---------|------------|
| Audio files | Private / user-local | User's device only | At-rest (device filesystem) |
| TrinityContract | Derived / shareable | Local + optional PRISM_API | TLS in transit, encrypted at rest |
| ShowBundle | User-created / shareable | Local + optional PRISM_API | TLS in transit, encrypted at rest |
| User credentials | Sensitive | PRISM_API (hashed) | bcrypt/argon2, TLS |
| Device telemetry | Operational / opt-in | Not persisted by default | TLS if transmitted |
| LED frame data | Ephemeral | Never stored | N/A |

### 6.4 Content Trust

- ShowBundles downloaded from PRISM_API are data-only (JSON). They contain effect IDs, palette IDs, parameter values, and timestamps.
- **No executable code** is transmitted in ShowBundles. Effect IDs reference firmware-compiled effects.
- This means a malicious ShowBundle can at worst set garish parameter values — it cannot execute arbitrary code on the device.
- [DEFERRED: When/if user-authored compiled effects are distributed, code signing and sandboxing become mandatory.]

---

## 7. Failure Modes and Containment

### 7.1 Inherited from Edge Canon

All K1_RUNTIME failure modes from `05_ARCHITECTURE_SPEC.md` Section 7 remain in force:
- Frame budget overrun → reduce effect complexity, preserve 120 FPS [CANON: I7, I8]
- PSRAM failure → safe mode [CANON: I6]
- Audio capture dropout → hold last frame, decay [CANON: D8]
- Control plane overload → rate limit [CANON: I9]
- Network instability → AP/STA fallback [CANON: I10, I11]
- Storage corruption → LittleFS recovery [CANON: D4]
- LED bus fault → brightness caps [CANON: D11]

### 7.2 New System-of-Systems Failure Modes

| # | Failure | Detection | Containment | Recovery |
|---|---------|-----------|-------------|----------|
| S1 | PRISM_API unreachable | HTTPS timeout / DNS failure | All local functionality continues. Cached content remains available. | Retry with exponential backoff. Queue uploads for later. |
| S2 | Trinity playback sync drift | PLL phase error > 50ms sustained | Snap device to latest trinity.beat position. | If drift persists, pause show and resync on next seek. |
| S3 | ShowBundle too large for device | LittleFS write failure | Reject transfer, notify user with size limit. | User simplifies show or increases device storage. |
| S4 | TRINITY_PIPELINE crash mid-analysis | Process exit code != 0 | Partial results discarded. User notified. | Re-run analysis. Checkpoint at stage boundaries (beat → segment → stems). |
| S5 | Tab5 hub loses WiFi to K1_RUNTIME | WebSocket disconnect | Tab5 enters "disconnected" UI state. Cached show data preserved. | Auto-reconnect. If K1_RUNTIME is in AP mode, Tab5 reconnects to AP. |
| S6 | User plays wrong song with song-specific show | Audio hash mismatch (if checkable) or visual mismatch | Warn user: "This show was authored for a different song." | Allow playback anyway (beats may still roughly align). |
| S7 | Fork chain corruption (missing parent) | parent_id references non-existent bundle | Show "Original no longer available" in attribution. | Orphan handling: fork becomes standalone. |
| S8 | Concurrent edits to same ShowBundle | Version conflict on save | Last-write-wins for local; conflict resolution UI for cloud saves. | User chooses which version to keep or merges manually. |

---

## 8. Staged Migration Plan from AS-IS

### Overview

The migration builds outward from the working K1_RUNTIME core, adding layers in order of dependency. Each stage has measurable exit criteria. No stage requires the next to be complete.

```
AS-IS (today)                          TO-BE (target)
──────────────                         ──────────────
K1_RUNTIME (functional)          →     K1_RUNTIME (ShowBundle consumer)
Tab5 (controller only)           →     TAB5_HUB (controller + cache)
PRISM Dashboard V4 (viewer)      →     PRISM_STUDIO (authoring tool)
Trinity_v0.2 (CLI pipeline)      →     TRINITY_PIPELINE (schema-validated)
lightwave-ios-v2 (device ctrl)   →     LIGHTWAVE_APP (browse + perform + sync)
(nothing)                        →     PRISM_API (content + community)
(ad-hoc WebSocket messages)      →     TRINITY_PROTOCOL (versioned contracts)
```

### Stage 0 — Formalise TRINITY_PROTOCOL Schemas

**Work:** Define JSON Schema for all message types in Section 3. Publish as a shared schema package consumable by TypeScript (PRISM_STUDIO, PRISM_API), Swift (LIGHTWAVE_APP), and C++ (K1_RUNTIME, TAB5_HUB).

**Exit criteria:**
- JSON Schema files exist for: `trinity.contract`, `trinity.show_bundle`, `trinity.beat`, `trinity.macro`, `trinity.segment`, `trinity.sync`, `trinity.show_cue`, `device.state`, `device.audio_features`.
- A validation tool (`validate_message.py`) accepts/rejects sample messages.
- TRINITY_PIPELINE output passes `trinity.contract` schema validation.
- ShowDirectorActor's existing ShowCue format is mappable to `trinity.show_bundle` (manual verification).

**Duration estimate:** 1–2 weeks.

### Stage 1 — TRINITY_PIPELINE Schema Compliance

**Work:** Update `analyze_track.py` to emit `trinity.contract` v1.0.0. Add `audio_hash`, `contract_version`, `analysis_model_versions` fields.

**Exit criteria:**
- `analyze_track.py --out result.json` produces schema-valid `trinity.contract`.
- `audio_hash` is SHA-256 of input WAV.
- Existing PRISM Dashboard V4 still consumes the output (backward-compatible).
- Smoke test passes on 3 reference tracks.

**Duration estimate:** 1 week.

### Stage 2 — K1_RUNTIME ShowBundle Loader

**Work:** Add REST endpoint `POST /api/v1/shows/upload` that accepts a `trinity.show_bundle` JSON, persists to LittleFS, and makes it available to ShowDirectorActor.

**Exit criteria:**
- Device accepts ShowBundle via REST, persists to LittleFS.
- ShowDirectorActor loads and plays back a show from the uploaded bundle.
- Show cues fire at correct timestamps (verified against test show with known timing).
- Maximum ShowBundle size enforced (configurable, default 50 KB).
- Corrupt/oversized bundles rejected with HTTP 400 + error message.

**Dependencies:** Stage 0 (schema).
**Duration estimate:** 2–3 weeks.

### Stage 3 — PRISM_STUDIO MVP (Timeline Editor)

**Work:** Evolve PRISM Dashboard V4 into PRISM_STUDIO with timeline authoring. This is the largest and most critical stage.

**Sub-stages:**
- **3a:** Trinity contract import + segment/beat visualisation on timeline (build on existing V4 waveform view).
- **3b:** Drag-drop effect assignment to timeline segments.
- **3c:** Palette assignment per segment.
- **3d:** Parameter automation lanes (brightness, speed at minimum).
- **3e:** LED strip preview (simulated, using PixiJS Canvas).
- **3f:** ShowBundle export (JSON, schema-validated).

**Exit criteria:**
- User can import a TrinityContract + audio file.
- User can assign effects and palettes to segments on a visual timeline.
- User can draw at least one automation curve (brightness).
- User can preview on simulated LED strip.
- User can export a valid ShowBundle.
- Exported ShowBundle loads and plays correctly on K1_RUNTIME (Stage 2).

**Dependencies:** Stage 0 (schema), Stage 1 (valid TrinityContract input).
**Duration estimate:** 6–10 weeks (the critical path).

### Stage 4 — LIGHTWAVE_APP Show Sync

**Work:** Add show playback sync to iOS app. App loads ShowBundle, plays audio, streams trinity.beat/macro/segment to K1_RUNTIME in sync.

**Exit criteria:**
- App downloads ShowBundle (from file or PRISM_API).
- App plays audio file and streams Trinity sync messages to device.
- Device renders show cues in sync with audio (< 50ms drift).
- Pause/resume/seek work correctly.
- Works over both WiFi and AP mode.

**Dependencies:** Stage 0 (schema), Stage 2 (device show loader).
**Duration estimate:** 3–4 weeks.

### Stage 5 — PRISM_API MVP

**Work:** Build cloud backend for content storage, user accounts, and basic community.

**Sub-stages:**
- **5a:** Auth (Sign in with Apple/Google, JWT).
- **5b:** ShowBundle CRUD (upload, download, list, search).
- **5c:** Versioning + forking.
- **5d:** Community feed (latest, trending by likes).
- **5e:** Connect PRISM_STUDIO + LIGHTWAVE_APP to API.

**Exit criteria:**
- User can create account and sign in.
- User can upload ShowBundle from PRISM_STUDIO.
- User can browse and download ShowBundles in LIGHTWAVE_APP.
- Fork creates a new bundle with parent attribution.
- API is accessible over HTTPS with rate limiting.

**Dependencies:** Stage 0 (schema), Stage 3 (ShowBundle creation).
**Duration estimate:** 4–6 weeks.

### Stage 6 — TAB5_HUB Mode

**Work:** Upgrade Tab5.encoder from pure controller to lightweight hub with show caching and offline playback orchestration.

**Exit criteria:**
- Tab5 can cache up to N ShowBundles (limited by flash, minimum 5).
- Tab5 can orchestrate show playback on K1_RUNTIME without laptop/phone.
- Tab5 shows cached show list on LVGL UI.
- Tab5 reconnects to K1_RUNTIME after network interruption.
- "Room" concept: Tab5 persists device grouping + default show across reboots.

**Dependencies:** Stage 2 (device show loader), Stage 0 (schema).
**Duration estimate:** 3–4 weeks.

### Stage 7 — Cloud Analysis [DEFERRED]

**Trigger:** Non-technical user onboarding friction measured (e.g., > 50% of interested users cannot install TRINITY_PIPELINE).

**Work:** Containerise TRINITY_PIPELINE, deploy behind API. User uploads WAV, receives TrinityContract + optional StemPack.

**Exit criteria:**
- API endpoint accepts WAV, returns TrinityContract within 5× real-time.
- Audio is deleted after processing (retention policy enforced).
- Caching by audio_hash (same song = instant return).

### Migration Sequence Diagram

```
         Stage 0     Stage 1     Stage 2     Stage 3     Stage 4     Stage 5     Stage 6
         Schema      Pipeline    Device      Studio      App Sync    Cloud API   Tab5 Hub
         ──────      ────────    ──────      ──────      ────────    ─────────   ────────
Week 1   ████████
Week 2   ████████    ████████
Week 3               ████████    ████████
Week 4                           ████████    ████████
Week 5                           ████████    ████████
Week 6                                       ████████
Week 7                                       ████████
Week 8                                       ████████    ████████
Week 9                                       ████████    ████████
Week 10                                      ████████    ████████
Week 11                                                  ████████    ████████
Week 12                                                              ████████
Week 13                                                              ████████    ████████
Week 14                                                              ████████    ████████
Week 15                                                              ████████    ████████
Week 16                                                                          ████████
```

**Critical path:** Stage 0 → Stage 3 → Stage 4. The PRISM_STUDIO timeline editor is the bottleneck.

---

## 9. Acceptance Checks (Measurable)

### 9.1 Edge Canon Compliance (unchanged)

All checks from `05_ARCHITECTURE_SPEC.md` Section 9 remain:
- 120 FPS sustained, 99th percentile within 8.33ms budget [I7, D9]
- Effect kernel time < 2ms [I7, I8]
- Zero heap allocs in render path [I4, I5, D12]
- PSRAM present and in use [I6]
- Centre-origin propagation verified [I1, I2, D5]
- No rainbow/hue-wheel sweeps [I3, D11]
- REST + WS responsive under load [I9, D1]
- AP/STA fallback works [I10, I11, D2, D7]
- Preset save/load round-trips [D4]

### 9.2 System-of-Systems Checks (new)

#### Protocol Correctness
- [ ] All TRINITY_PROTOCOL messages validate against JSON Schema.
- [ ] TrinityContract includes `audio_hash`, `contract_version`, `analysis_model_versions`.
- [ ] ShowBundle includes `bundle_version`, `id`, `parent_id` (if fork).
- [ ] Protocol version `v` is present in every message envelope.
- [ ] Unknown fields in payloads are ignored by all consumers.

#### Content Roundtrip
- [ ] ShowBundle created in PRISM_STUDIO → uploaded to PRISM_API → downloaded to LIGHTWAVE_APP → transferred to K1_RUNTIME → plays identically to preview.
- [ ] ShowBundle exported to file → imported in another PRISM_STUDIO instance → identical content.
- [ ] Fork: original ShowBundle → fork in PRISM_STUDIO → upload → attribution chain visible.

#### Playback Sync
- [ ] Trinity playback sync: audio on phone + trinity.beat/macro/segment to device → visual sync within 50ms.
- [ ] Pause/resume: device pauses within 100ms of app pause. Resume re-syncs within 50ms.
- [ ] Seek: device repositions within 100ms of seek command.
- [ ] Show cues fire within ±1 frame of target timestamp (8.33ms tolerance at 120 FPS).

#### Offline Resilience
- [ ] K1_RUNTIME + LIGHTWAVE_APP work with no internet (AP mode, local control).
- [ ] K1_RUNTIME + TAB5_HUB work with no internet and no phone (cached show, encoder control).
- [ ] PRISM_STUDIO works offline for authoring (export to file, no upload).
- [ ] PRISM_API outage: all downloaded content remains functional. Upload queued for later.

#### Privacy
- [ ] No audio files stored on PRISM_API servers (verified by storage audit).
- [ ] ShowBundles contain no audio data (schema enforced: no binary audio fields).
- [ ] Device sends no telemetry to cloud without explicit opt-in.
- [ ] User can delete their account and all associated content from PRISM_API (GDPR compliance).

#### Performance
- [ ] TRINITY_PIPELINE: 3-minute song analysed in < 15 minutes on M1 MacBook.
- [ ] PRISM_STUDIO: timeline editor responsive at 60 FPS with 500+ cues visible.
- [ ] PRISM_API: ShowBundle download < 500ms (CDN-cached).
- [ ] K1_RUNTIME: ShowBundle load from LittleFS < 1 second for 50 KB bundle.
- [ ] LIGHTWAVE_APP: ShowBundle transfer to device < 2 seconds over local WiFi.

---

## 10. Unknowns, Assumptions, and Validation Steps

### 10.1 Labelled Assumptions

| # | Assumption | Impact if Wrong | Validation Step |
|---|-----------|----------------|-----------------|
| A1 | Web (PWA) is sufficient for PRISM_STUDIO DAW-like experience | Must wrap in Tauri/Electron or build native | Build timeline PoC (Stage 3a–3b), measure UX friction |
| A2 | Supabase is sufficient for PRISM_API MVP | Must build custom backend | Prototype ShowBundle CRUD + auth in Supabase, load-test |
| A3 | 50 KB ShowBundle limit is sufficient for complex shows | Must compress or stream show data | Create 3 complex reference shows, measure size |
| A4 | Local-network device access without auth is acceptable | Must add device pairing | Threat model review for shared-network scenarios |
| A5 | ShowDirectorActor can be extended to support dynamic ShowBundle loading | Must refactor ShowDirector | Attempt Stage 2; measure deviation from PROGMEM path |
| A6 | 30 FPS macro update rate is sufficient for visual smoothness | Must increase rate or interpolate on device | A/B test with real shows at 30 vs 60 FPS macros |
| A7 | Users will supply their own audio (no audio distribution needed) | Must license audio or partner with streaming services | User research during beta |

### 10.2 Contested Decisions (inherited from Canon)

These remain from `04_OPEN_QUESTIONS.md` and are NOT resolved by this spec:

1. **Control-plane recovery choices** — AP/STA fallback, IP-first discovery, portable-mode recovery. Validation: load-test under this spec's multi-client scenarios (app + Tab5 + Studio all connected).

2. **Hardware topology boundaries** — Device vs hub vs tooling responsibilities. This spec proposes clear boundaries (Section 1.2) but the Tab5's hub role (Stage 6) needs real-world testing.

3. **Palette/preset enforcement via API validation** — This spec adds ShowBundle validation (JSON Schema), which partially addresses this. Full API validation hooks need prototyping.

### 10.3 New Open Questions

| # | Question | Blocks | Proposed Resolution |
|---|----------|--------|-------------------|
| Q1 | Should PRISM_STUDIO support multi-user collaborative editing? | Stage 5+ | [DEFERRED] Launch with single-user. Evaluate CRDTs if demand emerges. |
| Q2 | Should shows support conditional logic (if energy > threshold, switch effect)? | Stage 3 ShowBundle format | [DEFERRED] Launch with timeline-only (deterministic). Conditional logic is a v2 feature. |
| Q3 | How does a user match their audio file to a song-specific show? | Stage 4 app sync | Proposed: match by audio_hash (exact). Fallback: manual "I have this song" confirmation. |
| Q4 | Should PRISM_API support paid content / creator monetisation? | Stage 5 | [DEFERRED] Launch with free sharing. Evaluate Stripe Connect if community reaches critical mass. |
| Q5 | What is the maximum number of simultaneous WebSocket clients on K1_RUNTIME? | Stage 4 (app + Tab5 + Studio all connected) | Measure: stress-test 3 concurrent WS clients on ESP32-S3. |

---

## Appendix A: Subsystem → Source Code Mapping

| Subsystem | Repository / Path | Primary Language |
|-----------|------------------|-----------------|
| K1_RUNTIME | `Lightwave-Ledstrip/firmware/v2/` | C++ (PlatformIO) |
| TAB5_HUB | `Lightwave-Ledstrip/firmware/Tab5.encoder/` | C++ (PlatformIO) |
| TRINITY_PIPELINE | `SpectraSynq.trinity/Trinity_v0.2/` | Python (conda) |
| PRISM_STUDIO | `SpectraSynq.trinity/prism_dashboard/` (evolves into Studio) | JavaScript (vanilla → framework TBD) |
| PRISM_API | (new, to be created) | TBD (recommendation: TypeScript + Supabase) |
| LIGHTWAVE_APP | `Lightwave-Ledstrip/lightwave-ios-v2/` | Swift / SwiftUI |
| TRINITY_PROTOCOL | (new, to be created as shared schema package) | JSON Schema + generated types |
| K1.node1 | `K1.node1/firmware/` | C++ (research reference, not production) |

## Appendix B: Technology Recommendations (non-binding)

These are recommendations, not requirements. Each can be swapped for alternatives without affecting the architecture.

| Component | Recommendation | Rationale | Alternative |
|-----------|---------------|-----------|-------------|
| PRISM_STUDIO framework | Evolve from vanilla JS to **SvelteKit** or **React** | Need component model for complex UI; Svelte is lightweight, React has larger ecosystem | Vue, Solid, or stay vanilla + Web Components |
| Timeline rendering | **PixiJS** (already in use) + custom timeline | Proven in V4, WebGL performance | Canvas 2D (simpler but slower), Konva.js |
| Audio waveform | **WaveSurfer.js** (already in use) | Proven in V4, regions plugin for segments | Peaks.js (BBC), custom Canvas |
| PRISM_API backend | **Supabase** (PostgreSQL + Auth + Storage + Realtime) | Minimal custom code, generous free tier, Row Level Security | Firebase, Convex, custom FastAPI + PostgreSQL |
| PRISM_API hosting | **Vercel** (frontend) + **Supabase** (backend) | Free tier sufficient for MVP, good DX | Cloudflare Pages + Workers, Fly.io |
| Schema validation | **JSON Schema Draft 2020-12** + **Ajv** (JS) + **jsonschema** (Python) | Industry standard, multi-language | Zod (TypeScript only), Pydantic (Python only) |
| CDN | **Cloudflare R2** (object storage + CDN) | No egress fees, S3-compatible | AWS S3 + CloudFront, Vercel Blob |

## Appendix C: Glossary Amendments

These terms are used in this spec and should be added to `03_GLOSSARY_ALIAS_MAP.md`:

| Term | Definition | Context |
|------|-----------|---------|
| **ShowBundle** | A portable, shareable show definition containing timeline cues, zone configs, audio references, and metadata. The primary content unit of PRISM_PLATFORM. | System-wide |
| **TrinityContract** | JSON output of TRINITY_PIPELINE containing beat positions, segment boundaries, BPM, and optional stem paths/features for one audio file. | Analysis layer |
| **PRISM_STUDIO** | Web application for visual show authoring. Evolves from PRISM Dashboard. | Platform layer |
| **PRISM_API** | Cloud backend providing user accounts, content storage, distribution, and community features. | Platform layer |
| **StemPack** | Directory structure containing separated audio stems + manifest + optional features for one audio file. | Analysis layer |
| **audio_hash** | SHA-256 hash of a source WAV file. Used for content-addressing: same file = same hash, enabling cache hits and show-to-song matching. | Protocol layer |

---

*End of document. This spec is a proposal. No implementation should begin without explicit approval of each stage's scope and exit criteria.*
