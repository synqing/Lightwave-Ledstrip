# K1 Composer Dashboard — What It Is and Why It Exists

**Last updated:** 2026-02-26.  
**Audience:** Anyone asking “why are we adding another software layer?” or “what is the K1 Composer?”

---

## What it is

The **K1 Composer** (`tools/k1-composer-dashboard/`) is a **browser-based “dashboard-as-brains”** companion for the LightwaveOS K1 (ESP32) device. It:

- Connects to the device over REST + WebSocket (configurable host, e.g. `192.168.1.100`).
- **Takes a control lease** (“Take Control”) so it can drive effects, parameters, and the render stream exclusively.
- **Streams framebuffer data to the device** instead of the device running effects internally: the dashboard runs the effect pipeline (today in JS, with a planned WASM parity tier) and sends only LED frame bytes; the K1 renders them with a stale-timeout fallback to internal mode.
- Provides **timeline + transport** (play/pause, speed), **parameter tuner**, **LED preview**, **algorithm flow**, **live code visualiser** (effect source + trace), **stream diagnostics** (queue, cadence, drop ratio), and a **diagnostic panel** (e.g. `k1-diagnostic.html`) for pipeline/audio/LED inspection.

So it’s “another software layer” in the sense: a **web app that sits between the user and the firmware**, owning control and (when active) the render stream, while the device remains the dumb renderer + lease enforcer.

---

## Why it exists

- **Explainability and iteration:** Effect logic, code view, variables, and traces live in the dashboard and in generated artefacts from firmware source. You can see what’s running and tweak parameters without reflashing.
- **Offload and flexibility:** Heavy or experimental pipelines can run in the browser (or later WASM); the device just displays frames. Different “brains” (e.g. other dashboards, Trinity/PRISM) can target the same device contract.
- **Single contract:** Control lease + render stream + device status form one contract. The firmware stays focused on rendering, timing, and enforcement; the dashboard owns “what to show” when it has the lease.
- **Diagnostics and telemetry:** The dashboard is the right place for queue pressure, send cadence, drop ratio, and event traces — and for a **diagnostic panel** that visualises real LED/audio streams (and derived pipeline stages) for debugging.

So the “extra layer” is intentional: **brains in the dashboard, device as renderer and enforcer**, with a clear API and telemetry surface for development and debugging.

---

## Where the “why” is written in more detail

- **Implementation path (phases):** [k1-composer-implementation-path.md](k1-composer-implementation-path.md)  
- **Wireframe and UX states:** [k1-composer-wireframe.md](k1-composer-wireframe.md)  
- **Telemetry schema:** [k1-composer-telemetry-schema.md](k1-composer-telemetry-schema.md)  
- **Diagnostic panel spec (data source swap, binary streams):** see claude-mem / agent context for the “Composer Diagnostic Panel — Implementation Spec” (proto-combined.jsx → real WebSocket LED/audio streams).

---

## Quick reference

| Item | Location |
|------|----------|
| Main UI | `tools/k1-composer-dashboard/index.html` + `app.js` |
| Diagnostic panel | `tools/k1-composer-dashboard/k1-diagnostic.html` |
| Control lease API | `docs/api/control-lease-v1.md` |
| Render stream API | `docs/api/render-stream-v1.md` |
| Device status (incl. `freeInternalHeap`) | `GET /api/v1/device/status` — see `docs/api/api-v1.md` |
