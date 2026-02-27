# K1 Composer WASM Scaffold

This directory hosts the Tier 1 WebAssembly bridge for running firmware-aligned effect logic in the dashboard.

## Current state

- `engine_entry.cpp` defines a minimal C ABI surface.
- `build.sh` compiles the module with Emscripten.
- `wasm-loader.js` loads the generated artefact when present.

## Build

```bash
cd k1-composer/wasm
./build.sh
```

Expected outputs:

- `composer_engine.js`
- `composer_engine.wasm`

## ABI design target

The ABI is intentionally narrow and deterministic:

- `composer_load_effect(effect_id)`
- `composer_set_param(param_id, value)`
- `composer_tick(dt_ms)`
- `composer_scrub(t_norm)`
- `composer_get_frame_ptr()` (`960` byte RGB888 payload)
- `composer_get_trace_ptr()` (UTF-8 JSON line)

This allows JS to run transport and visualisation logic while C/C++ remains the source of truth for effect computation.
