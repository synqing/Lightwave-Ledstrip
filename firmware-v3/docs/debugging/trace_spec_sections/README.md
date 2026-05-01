<!-- SPDX-License-Identifier: Apache-2.0 -->
<!-- Copyright 2025-2026 SpectraSynq -->

---
abstract: "Source detail files for TRACE_INSTRUMENTATION_SPEC.md. Each file contains the full investigation evidence, code blocks, and rationale for one of 9 instrumentation surfaces. The master spec (../TRACE_INSTRUMENTATION_SPEC.md) is the executable roadmap; these files are the deep reference. Read the master first; consult these only when you need the exact rationale or full code block for a specific surface."
---

# Trace Spec Sections — Source Detail

This directory holds the per-surface detail files that fed the canonical [TRACE_INSTRUMENTATION_SPEC.md](../TRACE_INSTRUMENTATION_SPEC.md). The master spec contains the decisions, invariants, contracts, and the 44-step implementation spine. These files contain the full investigation evidence and verbose code blocks distilled into the master.

**Read order:** Master first (`../TRACE_INSTRUMENTATION_SPEC.md`). Consult the relevant section file only when you need the full rationale or untruncated code listing for a specific surface.

## Index

| File | Surface | Owner contract metric | Key files referenced |
|---|---|---|---|
| `01_render_budget.md` | 1 — Render path budget | `render_frame_work_us` p99 < 2000 µs | RendererActor.cpp |
| `02_audio_render_handoff.md` | 2 — Audio→Render handoff | `audio_snapshot_read` p99 < 200 µs (target) | RendererActor.cpp, SnapshotBuffer.h |
| `03_audio_core0.md` | 3 — Audio analysis stack (Core 0) | `audio_hop_us` p99 < 6000 µs | AudioActor.cpp, OnsetDetector.cpp |
| `04_wifi_webserver.md` | 4 — WiFi / WebServer perturbation | `ws_msg_dispatch` p99 < 500 µs | WiFiManager.cpp, WsGateway.cpp, V1ApiRoutes.cpp |
| `05_memory_thermal.md` | 5 — Memory/heap + power/thermal | `heap_free_internal_kb` > 100 | main.cpp, HeapMonitor.cpp, StackMonitor.h |
| `06_effect_lifecycle.md` | 6 — Effect lifecycle | `effect_init_us` p99 < 10000 µs | RendererActor.cpp, IEffect.h |
| `07_bench_framework.md` | 7 — Runtime A/B bench framework (Tier 3) | n/a (toggle infrastructure) | BenchRegistry.h/cpp (new), SerialCLI.cpp |
| `08_analyser_tool.md` | 8 — analyse_trace.py post-process tool | n/a (host-side) | tools/analyse_trace.py (new) |
| `09_causal_markers.md` | 9 — Causal markers catalogue (Tier 4) | n/a (narrative) | AudioActor.cpp, SystemInit.cpp, main.cpp |

## Status

These files are the immutable source-of-evidence for the synthesis. The master spec supersedes any apparent contradiction with these files. If you find a contradiction, update the master (with a changelog entry) — do not edit these.

---

**Document Changelog**

| Date | Author | Change |
|---|---|---|
| 2026-04-27 | claude-opus-4-7 (synthesis SSA) | Created. Indexes the 9 source section files copied from /tmp/trace_spec/ into this directory. Establishes precedence: master spec wins over section files. |
