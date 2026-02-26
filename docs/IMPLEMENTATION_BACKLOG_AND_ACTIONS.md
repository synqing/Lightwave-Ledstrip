# Implementation Backlog & Actionable Tasks

**Purpose:** Single reference for fixes, TODOs, and outstanding work that need to be actioned.  
**Last reviewed:** After `feature/tab5-ios-effectid-alignment` branch goal verification.

---

## 1. Quick wins (small, high-impact)

| Task | Location | Action |
|------|----------|--------|
| **ApiResponse.h deprecation** | `firmware/v2/src/network/ApiResponse.h` ~line 254 | Replace `root.createNestedObject("error")` with ArduinoJson v7 pattern (e.g. `root["error"].to<JsonObject>()`) in `buildWsRateLimitError()`. One-line fix. |
| **API auth device verification** | N/A | After deploy: run `python3 firmware/v2/tools/test_api_auth.py [host]` (default `lightwaveos.local`) to confirm 401 (no key), 403 (wrong key), 200 (correct `X-API-Key`). Documented in `docs/API_AUTH_DEPRECATION_DEBRIEF.md` § Blocker 3. |

---

## 2. Outstanding blockers (documented)

| Blocker | Source | Resolution |
|---------|--------|------------|
| **RendererActor Trinity sync** | `docs/API_AUTH_DEPRECATION_DEBRIEF.md` § Blocker 2 | **Resolved.** Message-based TRINITY_SYNC; no code change required. WsTrinityCommands → ActorSystem.trinitySync() → RendererActor message loop. |
| **API auth not verified on device** | Same debrief § Blocker 3 | Run `firmware/v2/tools/test_api_auth.py` after upload; add to CI or release checklist if desired. |

---

## 3. In-code TODOs (firmware v2)

| Area | File | TODO |
|------|------|------|
| WebServer | `WebServer.cpp` ~250, ~325 | Implement logging callback system for WebSocket log streaming |
| WebServer | `main.cpp` ~333 | Add `setPluginManager()` to WebServer when plugin UI wiring is needed |
| Stream | `WsStreamCommands.h` ~83 | Implement full FFT streaming if/when required |
| Audio | `AudioActor.cpp` ~1658, ~1663 | Send HEALTH_STATUS / PONG once MessageBus integration lands |
| Narrative | `NarrativeHandlers.cpp` ~156 | Persist to NVS if narrative state must survive reboot |
| LED driver | `LedDriver_P4_RMT.cpp` ~599 | Implement power limiting (calculate total power, scale brightness) |
| Tests | `test_ws_router.cpp`, `test_ws_gateway.cpp`, `test_webserver_routes.cpp`, etc. | Multiple stub tests: WsCommandRouter register/route, gateway rate limiter/auth, StaticAssetRoutes/LegacyApiRoutes/V1ApiRoutes registration, integration contract |

---

## 4. iOS

| Item | Location | Action |
|------|----------|--------|
| **ConnectionManager** | `ConnectionManager.swift` ~308 | Add UUID to firmware endpoint then enable verification (TODO comment) |
| **Stubs / TODOs** | `lightwave-ios-v2/README.md` | Views marked `// STUB:` or `// TODO:` — search codebase for these markers for work-in-progress items |

---

## 5. Documented backlogs (strategic)

| Doc | Contents |
|-----|----------|
| **docs/planning/TODO.md** | Performance (SIMD, LUTs, frame skip, dual-core), audio-reactive LGP, encoder controls (gestures, presets, haptics), network (multi-device, DMX, MQTT, OSC), new physics, known issues (heap, WiFi, centre-origin), effect sequencer, modifiers, LGP enhancements, dev infra, wild ideas. Many items aspirational. |
| **docs/IMPLEMENTATION_CHECKLIST_AND_NEXT_STEPS.md** | Phased roadmap: Phase 1–2 (FFT, RMS, centroid, LED driver, smoothing), Phase 2 (onset, sub-bass, motion speed, saturation, performance, testing). Research-led implementation checklist. |
| **docs/architecture/PROJECT_STATUS.md** | Next steps: test build, enable web server, complete pipeline effects, add tests, documentation. Known issues: pipeline stubs, audio not implemented, web server to be created. |
| **docs/planning/DEVELOPMENT_PIPELINE_AUDIT.md** | Uncommitted changes analysis (may be stale); API v2 response type fixes, cJSON migration, etc. |
| **docs/security/SECURITY_FINDINGS_SUMMARY.md** | Immediate/short/medium-term recommendations (many marked ✅). Remaining: CORS whitelist, security event logging, NVS resilience, etc. |
| **docs/architecture/EFFECT_COUNT_SYNCHRONIZATION.md** | Pattern: runtime check that registered effect count matches `EFFECT_COUNT`; `[ACTION REQUIRED]` message to update `EffectRegistry.h` if mismatch. Use as pattern for any registry that has a compile-time count. |

---

## 6. Recommended order of action

1. **Immediate:** Fix `ApiResponse.h` line 254 (deprecation) and push.
2. **Immediate:** Run API auth verification on device; document result or add to checklist.
3. **Short-term:** ~~Decide fate of RendererActor Trinity sync~~ Resolved (message-based TRINITY_SYNC; no code change).
4. **Short-term:** Add or expand tests for WS router, gateway, and webserver routes where TODOs exist.
5. **Backlog:** Tackle firmware TODOs (WebServer logging callback, FFT streaming, MessageBus HEALTH_STATUS/PONG, NarrativeHandlers NVS, LedDriver power limiting) when the relevant features are in scope.
6. **Backlog:** iOS ConnectionManager UUID + verification when firmware exposes UUID.
7. **Strategic:** Use `docs/planning/TODO.md` and `docs/IMPLEMENTATION_CHECKLIST_AND_NEXT_STEPS.md` for sprint/roadmap planning; no single “must do next” from these.

---

*Generated from codebase grep, docs/API_AUTH_DEPRECATION_DEBRIEF.md, docs/planning/TODO.md, IMPLEMENTATION_CHECKLIST, PROJECT_STATUS, SECURITY_FINDINGS_SUMMARY, and EFFECT_COUNT_SYNCHRONIZATION.*
