---
abstract: "Index and navigation guide for audio-reactive parameter exposure research (2026-03-25). Links to four detailed documents covering industry research, parameter design, infrastructure analysis, and implementation code examples."
---

# Audio Parameters Research — Document Index

**Date:** 2026-03-25
**Status:** Complete research phase; ready for implementation planning

---

## Overview

This research package documents how embedded ESP32 LED projects expose audio-reactive parameters to users, with specific recommendations for K1's onset/silence detection system.

### Key Finding

**WLED Sound Reactive** is the industry-standard reference. Its success comes from:
- **Two core user-facing parameters:** Squelch (silence threshold) and Gain (input amplification)
- **Normalized ranges (0–100)** hiding DSP complexity
- **Immediate effect** on next processing frame
- **NVS persistence** across power cycles
- **Intuitive naming** ("How quiet is silent?" vs. "agcTargetRms")

**K1 already has the infrastructure** to expose parameters via REST API, WebSocket, serial CLI, and NVS. No new subsystems needed.

---

## Document Guide

### 1. **AUDIO_PARAMETERS_EXPOSURE_RESEARCH.md** — Full Research Report

**Purpose:** Complete comparative analysis of three projects and best practices.

**Contents:**
- WLED Sound Reactive (Squelch + Gain model, user workflow, ranges, defaults)
- Emotiscope (simplicity-first philosophy, preset modes vs. parameter tuning)
- LightwaveOS K1 (existing infrastructure, REST endpoints, WebSocket, serial CLI)
- Industry best practices (parameter naming, ranges, effect timing, storage tiers)
- Recommended parameter set for K1 (3 core parameters: silenceThreshold, onsetSensitivity, silenceHysteresis)
- API design proposal (REST, WebSocket, serial endpoints with examples)
- Implementation checklist

**Audience:** Architects, technical leads, implementers
**Read time:** 15–20 minutes
**Length:** ~400 lines

**When to read:** Start here to understand the full context and rationale.

---

### 2. **ONSET_PARAMETERS_QUICK_REFERENCE.md** — User & Developer Quick Guide

**Purpose:** Practical quick-reference for the three core parameters.

**Contents:**
- Parameter definitions (range, default, meaning, mapping formula)
- REST API examples (GET, POST with save flag, PATCH)
- WebSocket command syntax
- Serial CLI syntax (proposed)
- User calibration workflow (how to tune each parameter in practice)
- Comparison to WLED (parameter naming differences)
- Notes for implementers

**Audience:** Developers, UI implementers, end users
**Read time:** 5–10 minutes
**Length:** ~200 lines

**When to read:** Use this during implementation as a reference. Print it, tab it, refer back often.

---

### 3. **K1_PARAMETER_INFRASTRUCTURE_ANALYSIS.md** — Infrastructure Gap Analysis

**Purpose:** Technical assessment of what K1 already has vs. what must be built.

**Contents:**
- Existing infrastructure assessment (✅ REST API, ✅ WebSocket, ✅ Serial CLI, ✅ NVS persistence)
- Reusable code patterns (EdgeMixer as reference, copy-paste templates)
- Onset/silence detector current state (what's been done, what's incomplete)
- Implementation effort estimate (12–16 hours, layer by layer)
- File locations & patterns for each component
- Gap analysis (what's ready, what's missing, what needs investigation)
- Recommendations (create dedicated `/api/v1/audio/onset-parameters` endpoint vs. extending general parameters)

**Audience:** Technical leads, implementation planners
**Read time:** 10–15 minutes
**Length:** ~350 lines

**When to read:** Use before starting implementation. Creates realistic project plan.

---

### 4. **ONSET_PARAMETERS_CODE_EXAMPLES.md** — Copy-Paste Implementation Templates

**Purpose:** Concrete C++ code ready for integration.

**Contents:**
- `OnsetParameters` struct definition (header + implementation, ~150 lines)
- REST handler (`GET`, `POST`, `PATCH` endpoints with JSON parsing, bounds checking, NVS save, ~100 lines)
- WebSocket handler (message dispatch, three command types, ~80 lines)
- Serial CLI parser (`audio onset {get,set,save}` commands, ~60 lines)
- OnsetDetector integration example (how to read parameters in real-time, ~20 lines)
- Boot sequence integration (loading from NVS on startup, ~5 lines)
- Unit test examples (mocking NVS, testing bounds, ~30 lines)

**Audience:** C++ implementers, code reviewers
**Read time:** 10–15 minutes
**Length:** ~400 lines of code + explanations

**When to read:** During implementation, copy and adapt patterns as needed.

---

## Quick Navigation

| Task | Document | Section |
|------|----------|---------|
| **Understand the big picture** | #1 Research | Executive Summary + Comparative Analysis |
| **Learn the three parameters** | #2 Quick Ref | Three Core Parameters table |
| **Check what's already done** | #3 Infrastructure | Existing Infrastructure Assessment |
| **Estimate project effort** | #3 Infrastructure | Implementation Effort Estimate |
| **Start coding** | #4 Code Examples | OnsetParameters Struct Definition |
| **Add REST endpoints** | #4 Code Examples | REST Handler section |
| **Add WebSocket support** | #4 Code Examples | WebSocket Handler section |
| **Add serial CLI** | #4 Code Examples | Serial CLI Handler section |
| **Integrate with detector** | #4 Code Examples | OnsetDetector Integration section |
| **Write tests** | #4 Code Examples | Testing Examples section |

---

## Implementation Roadmap

### Phase 0: Planning (1–2 hours)
1. Read #1 (Research) — Executive Summary
2. Read #3 (Infrastructure) — Gap Analysis & Recommendations
3. Clarify: Will parameters be persistent by default? Can we restart audio actor on parameter change?
4. Review EdgeMixer code as reference implementation

### Phase 1: Core Data Structure (2–3 hours)
1. Create `OnsetParameters` struct (use code from #4)
2. Implement NVS serialization (follow EdgeMixerState pattern)
3. Boot-time load integration
4. Unit tests for struct

### Phase 2: REST API (2–3 hours)
1. Implement `/api/v1/audio/onset-parameters` endpoints (use code from #4)
2. JSON parsing, bounds checking
3. Broadcast support for WebSocket clients
4. Integration tests

### Phase 3: WebSocket & Serial (1–2 hours)
1. WebSocket command handlers (use code from #4)
2. Serial CLI commands (use code from #4)
3. Manual testing on hardware

### Phase 4: Detector Integration (2–4 hours)
1. Refactor OnsetDetector to read parameters (may depend on current state of detector)
2. Hot-parameter update callback
3. Soak test on hardware (ensure parameters take effect immediately)

### Phase 5: Documentation & Testing (2–3 hours)
1. Update `docs/api/api-v1.md` with endpoint documentation
2. User guide for parameter tuning (based on #2 Calibration Guide)
3. Full test suite (bounds checking, persistence, effect latency)

**Total estimate:** 12–17 hours for complete implementation

---

## Cross-References

### Related K1 Documentation

- `firmware-v3/docs/api/api-v1.md` — Full REST API v1 spec (see § Audio Endpoints)
- `firmware-v3/docs/EFFECT_DEVELOPMENT_STANDARD.md` — Effect coding patterns
- `firmware-v3/docs/CQRS_STATE_ARCHITECTURE.md` — State management philosophy
- `research/K1_PERSISTENCE_IMPLEMENTATION_GUIDE.md` — NVS patterns (from earlier research)

### EdgeMixer Implementation Reference

Follow the EdgeMixer implementation as a direct template:
- `src/effects/enhancement/EdgeMixer.h/cpp` — parameter struct
- `src/network/handlers/EdgeMixerHandler.cpp` — REST handler
- `src/network/WebSocketManager.cpp` — WebSocket handlers (search for "edge_mixer")
- `src/serial/SerialCLI.cpp` — serial command dispatch (search for edge mixer commands)

### Build & Test

- **Build:** `pio run -e esp32dev_audio_esv11_k1v2_32khz`
- **Upload:** `pio run -e esp32dev_audio_esv11_k1v2_32khz -t upload`
- **Monitor:** `pio device monitor -b 115200`
- **API endpoint test:** `curl http://192.168.4.1/api/v1/audio/onset-parameters`

---

## Research Sources

### WLED Sound Reactive
- [GitHub Repository](https://github.com/athom-tech/Sound-Reactive-WLED)
- [Squelch and Gain Reference](https://mm.kno.wled.ge/WLEDSR/Squelch-and-Gain/)
- [Sound Settings Documentation](https://mm.kno.wled.ge/soundreactive/Sound-Settings/)

### Emotiscope
- [Tindie Product Page](https://www.tindie.com/products/lixielabs/emotiscope-a-music-visualizer-from-the-future/)
- [Hackster.io Article](https://www.hackster.io/news/lixie-labs-emotiscope-is-a-powerful-bridge-between-sight-and-sound-b97e93c4e92d)

### K1 Infrastructure
- `firmware-v3/src/network/WebServer.cpp` — REST handler patterns
- `firmware-v3/src/network/WebSocketManager.cpp` — WebSocket dispatch
- `firmware-v3/src/serial/SerialCLI.cpp` — serial command handling
- `firmware-v3/src/core/persistence/` — NVS patterns

### ESP32 Audio & Persistence
- [ESP32 Preferences Library](https://zbotic.in/esp32-preferences-library-non-volatile-storage-nvs-guide/)
- [Espressif Microphone Design Guidelines](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/audio_front_end/Espressif_Microphone_Design_Guidelines.html)

---

## Next Steps

### For Implementation Team

1. **Read document #1 (Research)** — 15 minutes for context
2. **Review EdgeMixer code** — understand the pattern K1 already uses
3. **Use document #4 (Code Examples)** — start coding
4. **Follow roadmap above** — 5 phases, 12–17 hours total
5. **Test on hardware** — ensure parameter changes take immediate effect

### For Product/Design

1. **Decide on naming** — will we expose "Squelch" (WLED style) or "Silence Threshold" (more explicit)?
2. **Decide on defaults** — may need field tuning based on K1 v2 microphone gain and strip characteristics
3. **Plan UI** — three sliders (0–100) in a web/app UI, or just serial/REST API for now?
4. **Plan calibration workflow** — will you ship with a calibration guide, or rely on user intuition?

### For QA/Testing

1. **Bounds checking** — ensure out-of-range values are rejected
2. **Persistence** — power cycle the device, verify parameters survive
3. **Effect latency** — ensure parameter changes take effect within 16 ms (next frame)
4. **Hardware variation** — test across K1 v1 (if applicable) and K1 v2
5. **Environment robustness** — test in quiet room, noisy room, different music genres

---

## Document Changelog

| Date | Author | Change |
|------|--------|--------|
| 2026-03-25 | agent:search | Created index document. Links to 4 detailed research files. Quick navigation table, implementation roadmap (5 phases, 12–17 hours), next steps for implementation/product/QA teams. |

