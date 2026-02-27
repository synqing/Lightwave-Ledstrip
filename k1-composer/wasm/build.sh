#!/usr/bin/env bash
set -euo pipefail

if ! command -v emcc >/dev/null 2>&1; then
  echo "emcc not found. Install and activate Emscripten SDK first." >&2
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT_JS="${SCRIPT_DIR}/composer_engine.js"
SRC="${SCRIPT_DIR}/engine_entry.cpp"

emcc "${SRC}" \
  -O2 \
  -s WASM=1 \
  -s MODULARIZE=1 \
  -s EXPORT_ES6=1 \
  -s ENVIRONMENT='web' \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s EXPORTED_FUNCTIONS='["_malloc","_free","_composer_load_effect","_composer_set_param","_composer_tick","_composer_scrub","_composer_get_frame_ptr","_composer_get_frame_size","_composer_get_trace_ptr"]' \
  -s EXPORTED_RUNTIME_METHODS='["UTF8ToString"]' \
  -o "${OUT_JS}"

echo "WASM build complete: ${OUT_JS}"
