# SpectraSynq / K1‑Lightwave — Founding Architecture Spec

Date: 2026-02-09
Author: Founding Principal Architect (SpectraSynq)
Scope: Ground‑up design proposal that honours Constitution invariants and current decisions.

Companion outputs:
- Interface contracts: `CANON_PACK_BRIEFING_BUNDLE/06_INTERFACE_CONTRACTS.md`
- Migration plan: `CANON_PACK_BRIEFING_BUNDLE/07_MIGRATION_PLAN.md`

Reference map for citations in this spec:
- Invariants: I1..I14 from `00_CONSTITUTION.md` in listed order.
- Decisions: D1..D13 from `01_DECISION_REGISTER.md` in listed order.

---

## 4. AS‑IS Recap (one page max)

Mission: audio‑reactive LED controller with web/app control, dual‑strip 320‑LED WS2812 topology. (I14, I9, D1)

Core engine: `RendererActor` owns frame buffers and runs high‑frequency scheduling targeting 120 FPS with strict frame budgets. (I7, I8, D6, D9)

Audio: capture + feature extraction (tempo/chroma/flux) feeds render‑time controls, with hop cadence coupled to render cadence. (D8, D9)

Concurrency: actor boundaries with explicit state ownership across render, audio, and control layers. (I12, D13)

Control plane: REST for commands and WebSocket for streaming/state. (I9, D1)

Network: AP/STA fallback with deterministic IP paths; mDNS as hint/fallback. (I10, I11, D2, D7)

Storage: LittleFS with recovery paths. (D4)

Rendering laws: centre‑origin propagation from LEDs 79/80, no linear sweeps, no rainbow or hue‑wheel sweeps, and no heap allocation in render paths (static buffers only). (I1, I2, I3, I4, I5, D5, D11, D12)

Fixed segments: zone/segment boundaries are fixed with explicit composer/state contracts. (D3)

Open unknowns remain unknown (validation only, no guessing): control‑plane recovery choices, hardware topology boundaries, palette/preset enforcement via API validation. (Open Questions)

---

## 5. TO‑BE Architecture (single coherent design)

### 5.1 Architecture intent
Build a deterministic, actor‑owned pipeline where audio features drive centre‑origin effects under strict timing and memory guarantees, with resilient control and network planes that never interfere with render cadence. (I7, I8, I12, I9, D1, D9)

### 5.2 Runtime actors and ownership (single source of truth)
Actors are the concurrency model; state ownership is exclusive and explicit.

- `ControlPlaneActor` — validates external commands, owns current control snapshot. (I9, I12, D1)
- `NetworkActor` — AP/STA fallback, deterministic IP discovery, mDNS hinting, rate limits. (I10, I11, D2, D7)
- `StorageActor` — LittleFS persistence + recovery for presets/palettes/state. (D4)
- `AudioActor` — capture + DSP, emits fixed‑size feature frames. (D8)
- `FeatureBus` — lock‑free ring buffer for feature frames. (I4, I5, I6)
- `RenderActor` — exclusive frame ownership, 120 FPS cadence, centre‑origin semantics. (I1, I2, I7, D5, D6, D9)
- `OutputDriver` — LED push at stable cadence, bounded latency. (I7, I8)
- `WatchdogSupervisor` — monitors frame deadlines and system health. (I8, D9)

### 5.3 Memory and timing discipline
- All render buffers, feature buffers, and frame queues are pre‑allocated in PSRAM; no heap allocations in render paths. (I4, I5, I6, D12)
- Render cadence fixed at 120 FPS target; per‑frame effect budget under 2 ms. (I7, I8, D9)

### 5.4 Centre‑origin render pipeline
- A single, shared LED index mapping places the centre at LEDs 79/80; all effects originate at centre and propagate outward or inward. (I1, I2, D5)
- No linear sweeps, no rainbow or full hue‑wheel sweeps, enforced at effect API boundaries. (I2, I3, D11)

### 5.5 Control‑plane decoupling
- External control changes are applied to immutable snapshots consumed by `RenderActor` only at frame boundaries. (I12, I9, D1)
- REST for commands and WebSocket for streaming/state remain the sole external interfaces. (I9, D1)

### 5.6 Network resilience
- AP/STA fallback behaviour is preserved; deterministic IP paths are prioritised with mDNS as hint/fallback. (I10, I11, D2, D7)

---

## 7. Failure Modes + Containment

1. Frame budget overrun. Containment: reduce effect complexity, drop non‑essential interpolation, preserve 120 FPS cadence. (I7, I8, D9)
2. PSRAM init failure or exhaustion. Containment: fail fast into safe mode; refuse effect activation; reboot if required. (I6, I4, I5)
3. Audio capture dropouts. Containment: hold last feature frame, decay energy/flux to zero, never fabricate features. (D8)
4. Control‑plane overload (REST/WS storms). Containment: rate limiting, back‑pressure, prioritise WS stream integrity. (I9, I10, D1)
5. Network instability. Containment: deterministic IP path first, mDNS as hint, AP/STA fallback with recoverable state. (I10, I11, D2, D7)
6. Storage corruption. Containment: LittleFS recovery path; verify preset integrity before apply. (D4)
7. LED bus timing faults. Containment: safe brightness caps; conservative timing profile; log and retry. (D11)

---

## 9. Acceptance Checks (measurable)

Timing
- 120 FPS sustained; frame period 8.33 ms average; 99th percentile within budget. (I7, D9)
- Effect kernel time under 2 ms per frame. (I7, I8)

Memory
- Zero heap allocations in render path; static buffers only. (I4, I5, D12)
- PSRAM present and in use; failure to initialise PSRAM triggers safe abort. (I6)

Determinism / Drift
- Feature frames consumed in order; no duplicate `seq` use; audio hop vs render cadence drift under tolerance. (I7, I8, D8)

Visual invariants
- Centre‑origin propagation verified by test pattern. (I1, I2, D5)
- No rainbow/hue‑wheel sweeps in any effect. (I3, D11)

Control plane
- REST commands and WS updates remain responsive under load; rate limiting engages correctly. (I9, D1)

Network resilience
- AP/STA fallback works; deterministic IP path works; mDNS used only as hint. (I10, I11, D2, D7)

Storage
- Preset save/load round‑trips without corruption; recovery path validated. (D4)

---

## 10. Unknowns + Validation Steps (no guessing)

1. Control‑plane recovery choices (contested). Validation: load‑test AP/STA fallback, IP‑first discovery, portable‑mode recovery; measure reconnect latency and error rates. (Open Questions)
2. Hardware topology boundaries (contested). Validation: dependency matrix for device, hub, tooling; measure latency, fault isolation, and deployment reliability. (Open Questions)
3. Palette/preset enforcement via API validation (contested). Validation: prototype validation hooks; measure rejection rates, client friction, and stability gains. (Open Questions)

---

## Rules Compliance Notes
- No generic IoT boilerplate was introduced; all choices are tied to Constitution invariants or current decisions.
- British English spelling is used throughout.
