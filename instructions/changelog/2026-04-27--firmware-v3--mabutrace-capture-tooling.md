---
id: 2026-04-27--firmware-v3--mabutrace-capture-tooling
date_utc: 2026-04-27
agent: codex
scope: firmware-v3
type: chore
summary: Add MabuTrace capture, analysis, and runtime bench tooling.
files_changed:
  - firmware-v3/docs/debugging/TRACE_INSTRUMENTATION_SPEC.md
  - firmware-v3/docs/debugging/trace_spec_sections/
  - firmware-v3/docs/debugging/MABUTRACE_GUIDE.md
  - firmware-v3/tools/capture_trace.py
  - firmware-v3/tools/analyse_trace.py
  - firmware-v3/test/test_native/test_analyse_trace.py
  - firmware-v3/src/utils/BenchRegistry.cpp
  - firmware-v3/src/utils/BenchRegistry.h
  - firmware-v3/src/serial/SerialCLI.cpp
  - firmware-v3/src/core/actors/RendererActor.cpp
validation: git diff --check; python3 firmware-v3/tools/check_effect_contracts.py
breaking_change: false
follow_ups: []
---

## Details

Adds serial trace capture automation, post-processing reports, the trace instrumentation specification, and a fixed-size runtime bench toggle registry wired to the serial CLI.
