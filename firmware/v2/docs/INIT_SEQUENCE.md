# Initialisation Sequence (ESP32-S3 / v2 firmware)

This document describes the intended startup order for `firmware/v2/src/main.cpp` so that PSRAM-backed subsystems are initialised before render/network tasks begin.

## Goals

- Keep **internal SRAM** available for WiFi / lwIP / `esp_timer` / AsyncTCP.
- Allocate large tables/buffers **once** during startup (or explicit feature enable).
- Avoid heap allocations in any `render()` hot path.

## Startup order (high level)

1) **Boot verification**
   - `OtaBootVerifier::init()` (before any other work)

2) **WiFi pre-clean (web builds only)**
   - `esp_wifi_deinit()` to avoid stale init state across reboots (only when enabled)

3) **System monitoring**
   - `StackMonitor::init()` / start profiling (if enabled)
   - `HeapMonitor::init()` (if enabled)
   - `MemoryLeakDetector::init()` + `resetBaseline()` (if enabled)
   - `ValidationProfiler::init()` (if enabled)

4) **Actor system initialisation**
   - `actors.init()` (creates `RendererActor`)

5) **Effect registration**
   - `registerAllEffects(renderer)`

6) **PSRAM-backed tables that must exist before render tasks**
   - `AudioMappingRegistry::instance().begin()`
     - If allocation fails: the registry stays disabled and all accessors degrade gracefully.

7) **Persistent storage**
   - `NVS_MANAGER.init()`
   - `OtaTokenManager::init()` (if enabled)

8) **Zone system**
   - `zoneComposer.init(renderer)`
     - Allocates zone buffers (prefer PSRAM). If allocation fails, zone mode stays disabled.

9) **Start actors (render task begins)**
   - `actors.start()` (Renderer runs at 120 FPS on Core 1)

10) **Plugin manager + network**
   - Plugin manager initialises and loads manifests.
   - Web server begins afterwards (Core 0).

## Notes on graceful degradation

Some features are optional and must never crash the device if PSRAM allocation fails:

- `AudioMappingRegistry` (audio→visual mapping table)
- Zone buffers (multi-zone mode)
- Frame capture buffers (debug feature)

When disabled, the system must continue to render and expose the core UI/API.

