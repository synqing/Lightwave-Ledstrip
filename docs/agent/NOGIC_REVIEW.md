# Nogic-style architecture review (Lightwave-Ledstrip)

Generated as a stand-in when Nogic MCP tools are not available in-session. Run `!nogicreview` in your MCP client (with Nogic connected) to refresh Nogic’s index; use this doc as reference.

**Project:** Lightwave-Ledstrip  
**Project ID (Nogic):** `ca363850-8f20-4db4-a767-6d91809c6b38`  
**Workspace:** `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip`

---

## 1. Architecture map summary

| Layer | Components | Key paths |
|-------|------------|-----------|
| **Input** | Web UI, Serial menu | `data/*.html/js`, serial @ 115200 |
| **Control** | WebServer, REST, WebSocket | `firmware/v2/src/network/WebServer.cpp`, V1ApiRoutes, WsGateway |
| **Effect** | Zone Composer, IEffect registry, transitions | `effects/zones/`, `effects/ieffect/*`, `effects/transitions/` |
| **Output** | FastLED, RMT | `hal/esp32s3/LedDriver_S3.cpp`, `leds[320]` |

Data flow: Input → Control → Effect → Output. Centre-origin: LEDs 79/80; 320 LEDs (2×160).

---

## 2. Key symbols / entry points

| Symbol / area | Location | Role |
|---------------|----------|------|
| `main.cpp` | `firmware/v2/src/main.cpp` | Entry, loop, actor wiring |
| `RendererActor` | `core/actors/RendererActor.cpp` | Effect selection, render loop, zone composition |
| `registerAllEffects` | `effects/CoreEffects.cpp` | Effect registration (stable IDs) |
| `IEffect` | `plugins/api/IEffect.h` | Effect contract: `init`, `render`, `name`, `category` |
| `WebServer` | `network/WebServer.cpp` | REST + WebSocket routing |
| `WsGateway` | `network/webserver/WsGateway.cpp` | WebSocket command dispatch |
| `ZoneComposer` | `effects/zones/` | Per-zone buffers, composition, transitions |
| `PipelineCore` | `audio/pipeline/PipelineCore.cpp` | Audio capture, VisualParams for audio-reactive |

---

## 3. Risk areas (blast radius)

| Area | Risk | Mitigation |
|------|------|------------|
| `effect_ids.h` / `display_order.h` | Changing IDs breaks API/iOS parity | See docs/agent; keep Tab5/firmware/v2 in sync |
| `validateEffectId` / bounds | Invalid index → LoadProhibited | All array access via `validate*()`; see DEFENSIVE_BOUNDS_CHECKING |
| Render loop heap | `new`/String in `render()` → frame drops | Static buffers only; `validate` in hot path |
| Centre origin | Linear 0→159 breaks LGP look | CENTER_LEFT/79, CENTER_RIGHT/80; radiate in/out |

---

## 4. Recommendations

1. **Nogic index:** Run `!nogicreview` in the client where Nogic Visualizer is connected so Nogic has an up-to-date architecture map.
2. **Canvas:** Use the `agent_canvas` MCP tool in that client to generate diagrams; point it at `docs/architecture/ARCHITECTURE_MAP.md` and `docs/architecture/00_LIGHTWAVEOS_INFRASTRUCTURE_COMPREHENSIVE.md`.
3. **Local canvas:** Use `docs/architecture/ARCHITECTURE_CANVAS.html` as a browser-viewable architecture diagram when MCP canvas isn’t available.

---

## 5. Related docs

- [ARCHITECTURE.md](/ARCHITECTURE.md) — Invariants, data flow, pitfalls
- [docs/architecture/ARCHITECTURE_MAP.md](../architecture/ARCHITECTURE_MAP.md) — High-level map (Mermaid)
- [docs/architecture/00_LIGHTWAVEOS_INFRASTRUCTURE_COMPREHENSIVE.md](../architecture/00_LIGHTWAVEOS_INFRASTRUCTURE_COMPREHENSIVE.md) — Full infrastructure
- [docs/architecture/ARCHITECTURE_CANVAS.html](../architecture/ARCHITECTURE_CANVAS.html) — Browser-viewable canvas (this review’s stand-in)
- [docs/agent/NOGIC_WORKFLOW.md](NOGIC_WORKFLOW.md) — Nogic review + canvas workflow
