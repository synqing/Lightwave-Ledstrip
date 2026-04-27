<!-- SPDX-License-Identifier: Apache-2.0 -->
<!-- Copyright 2025-2026 SpectraSynq -->

---
abstract: "Definitive specification for adding tracing to firmware-v3. Covers 9 surfaces (render budget, audio handoff, audio DSP, network perturbation, memory/thermal, effect lifecycle, bench framework, analyser tool, causal markers). Any agent can pick this up and execute end-to-end without judgment calls. Companion: MABUTRACE_GUIDE.md (capture+view), capture_trace.py (capture tool), analyse_trace.py (analyser tool — to be implemented per §8). Detailed source sections live in trace_spec_sections/01..09."
---

# TRACE_INSTRUMENTATION_SPEC.md

This document is the canonical, executable specification for instrumenting firmware-v3 with mabutrace events. It synthesises 9 surface-level investigations (`trace_spec_sections/01..09`) into a single roadmap. An implementing agent who reads ONLY this file should be able to land the entire instrumentation set, capture meaningful traces, and validate against contracts. The 9 sibling files contain the deep evidence; this file contains the decisions.

Scope: ESP32-S3 K1 firmware (`esp32dev_audio_esv11_k1v2_32khz`). All file:line references are relative to `firmware-v3/`. British English throughout (colour, behaviour, optimised, initialise).

---

## Tier framework

Every instrumentation point sits at one of five tiers. Tiers control build-time inclusion and runtime cost.

| Tier | Cost | Default | Purpose | Examples |
|---|---|---|---|---|
| 0 | None | Always | Macro is `((void)0)` when `FEATURE_MABUTRACE=0` | Canonical production build |
| 1 | ~24 B/event, ~120 events/sec aggregate | ON when `FEATURE_MABUTRACE=1` | Always-on counters and contract gates | `render_frame_work_us`, `audio_snapshot_age_us`, `effect_id_active`, heap gauges |
| 2 | ~40 B/event span, decomposition spans | OFF, opt-in via `FEATURE_TRACE_*` flags | Surface-specific decomposition for investigation | `bus_copy_memcpy`, `onset_detect_span`, `ws_msg_dispatch`, `effect_init_<eid>` |
| 3 | Toggle reads (volatile bool) | OFF until toggled at runtime | Runtime A/B benchmark framework (Surface 7) | `render.async_rmt`, `audio.zone_agc`, etc. |
| 4 | ~16 B/event, <1 Hz | ON when `FEATURE_MABUTRACE=1` | Cheap narrative markers for regime segmentation | `boot_phase_*`, `audio_silence_entered`, `wifi_client_connected_<N>` |

**Tier 0 invariant:** when `FEATURE_MABUTRACE=0`, every TRACE_* macro vanishes at compile time. The shipping production firmware is bit-for-bit identical to the pre-instrumentation firmware. Verified by checking `.map` size and disassembly of `RendererActor::onTick`.

---

## TRACE_* macro reference

All four primitives live in `src/config/Trace.h`:

| Macro | Use | Bytes per event | Notes |
|---|---|---|---|
| `TRACE_SCOPE("name")` | RAII span; `dur` set when scope exits | ~40 | Use for hot-path spans; fires `B`+`E` pair internally |
| `TRACE_BEGIN("name")` / `TRACE_END("name")` | Manual span pair | ~40 | When span boundary cannot be RAII (e.g. crosses functions) |
| `TRACE_INSTANT("name")` | Single-fire timestamped marker | ~16 | Cheapest event type; used for narrative markers and rare events |
| `TRACE_COUNTER("name", int)` | Sampled scalar | ~24 | Time-series counter; analyse with histogram + sparkline |

Ring-buffer arithmetic: 64 KB / 24 B average per event = ~2,700 event capacity. At 120 FPS and ~10 events/frame the ring fills in ~2.25 seconds. Captures longer than that require `--soak` mode in `capture_trace.py` (drains the ring incrementally).

---

## Performance contracts being enforced

This is the master invariants table. Each row is a numerical contract that the trace must verify. Any change in firmware that moves these values out of contract is a regression.

| # | Contract | Owner surface | Today's measured | Threshold | Source |
|---|---|---|---|---|---|
| 1 | `render_frame_work_us` p99 < 2000 µs | 1 | 1957 µs (98% of ceiling) | 2000 µs | RendererActor.cpp:955 |
| 2 | `audio_snapshot_read` p99 < 200 µs | 2 | 836 µs (4× over) — H2 hypothesis | 200 µs (target) | RendererActor.cpp::onTick |
| 3 | `audio_hop_us` p99 < 6000 µs | 3 | TBD — first capture | 6000 µs | AudioActor.cpp:580 onTick |
| 4 | `ws_msg_dispatch` p99 < 500 µs | 4 | TBD | 500 µs | WsGateway::handleMessage |
| 5 | `heap_free_internal_kb` > 100 (any sample) | 5 | TBD | 100 (≈ OOM warn at 20) | main.cpp 1 Hz loop |
| 6 | `effect_init_us` p99 < 10000 µs | 6 | TBD | 10000 µs | RendererActor::handleSetEffect |
| 7 | `render_frame_deadline_miss.count` ≤ 5 per 30 s soak | 1 | TBD | ≤ 5 | RendererActor.cpp:956 |
| 8 | `audio_snapshot_age_us` p99 < 5000 µs | 1, 2 | TBD | 5000 µs | RendererActor.cpp:1404 |

The analyser tool (Surface 8) consumes a `baseline.json` whose `contracts` block lists exactly these rows. Any soak capture must be runnable as `analyse_trace.py <trace> --baseline baselines/<env>_<effect>.json --strict` — exit code 7 means a contract failed.

---

## Cost / ring-buffer budget

Ring fill calculations under different feature-flag combinations (assumes Core 0 audio at 125 Hz, Core 1 render at 120 Hz):

| Build configuration | Events/sec | Ring fill | Notes |
|---|---|---|---|
| Canonical (`FEATURE_MABUTRACE=0`) | 0 | n/a | Production baseline |
| Tier 1 only | ~750 | ~3.6 s | Heap gauges + render counters + audio counters |
| Tier 1 + `FEATURE_TRACE_AUDIO_HANDOFF` | ~1,200 | ~2.3 s | Adds `bus_copy_memcpy`, motion engine spans |
| Tier 1 + `FEATURE_TRACE_AUDIO_DSP` | ~1,750 | ~1.6 s | Adds onset/STM/I2S spans |
| Tier 1 + `FEATURE_TRACE_NETWORK` | ~900 | ~3.0 s | WS dispatch only fires on traffic |
| Tier 1 + `FEATURE_TRACE_EFFECT_LIFECYCLE` | ~760 | ~3.6 s | Init/cleanup are rare events |
| All Tier 2 enabled | ~2,950 | ~0.9 s | Tight; only viable for short soaks (5 s) |

Implication: there is no single "everything on" build that captures cleanly for 30 s. Investigations must opt in to one Tier 2 family at a time.

---

## Build flags (consolidated)

All flags are defined in `src/config/features.h` and gated on per-environment via `platformio.ini` `build_flags`.

| Flag | Surface | What it enables | Cost when on | Default |
|---|---|---|---|---|
| `FEATURE_MABUTRACE` | all | TRACE_* macros become real (otherwise `((void)0)`) | Tier 1 baseline | 0 (off) |
| `FEATURE_TRACE_AUDIO_HANDOFF` | 2 | `bus_copy_memcpy`, `bus_retry_check`, `audio_ctx_populate_us`, motion engine/shaper spans | ~450 events/sec | 0 |
| `FEATURE_TRACE_AUDIO_DSP` | 3 | `i2s_dma_read`, `stm_rfft_256`, `onset_detect_span`, `band_ratio_detect`, `controlbus_publish` | ~1,000 events/sec | 0 |
| `FEATURE_TRACE_NETWORK` | 4 | `ws_msg_dispatch`, per-handler spans, REST handler spans, `ws_dispatch_count` | ~150 events/sec under load | 0 |
| `FEATURE_TRACE_EFFECT_LIFECYCLE` | 6 | `effect_init_<eid>`, `effect_cleanup_<eid>`, `effect_render_first_frame_<eid>` | ~10 events per switch | 0 |
| `FEATURE_TRACE_PERFORMANCE` | 1 | `color_correction_us` counter | ~80 events/sec | 0 |

**Required envs to add to `platformio.ini`:**
- `esp32dev_audio_esv11_k1v2_32khz_trace` — extends production env with `FEATURE_MABUTRACE=1` (Tier 1 + Tier 4 only)
- `esp32dev_audio_esv11_k1v2_32khz_trace_full` — adds all Tier 2 flags. Soak budget capped at 5 s.

---

## Master open questions (Captain decisions)

These bubble up from individual surfaces. Captain should resolve these before, or alongside, implementation.

1. **(Surface 1, 2)** `effect_id_active` counter is referenced by Surface 1 (Tier 1) and Surface 6 (cross-reference). Owner = Surface 1. Surface 6 must NOT also write this counter. **Action:** confirm Surface 1 owns; Surface 6 reads only.
2. **(Surface 1)** `FEATURE_TRACE_PERFORMANCE` flag — define in `features.h`? Or fold the single `color_correction_us` counter into Tier 1 unconditionally (cost is low: ~80 events/sec)? **Recommendation: fold into Tier 1.**
3. **(Surface 2) — RESOLVED 2026-04-27 by Captain.** Architectural decision: **`ControlBusFrame` shall be allocated in internal DRAM, not PSRAM.** The 5 KB DRAM cost is approved. Expected outcome: 5–10× speedup on `audio_snapshot_read` (836 µs p99 → ~100–200 µs target). Implementing agent must (a) verify the current `SnapshotBuffer<ControlBusFrame>` allocation site in AudioActor, (b) ensure the buffer is placed in internal DRAM via `MALLOC_CAP_INTERNAL` (or stack/static placement if SnapshotBuffer is a value member of a DRAM-resident actor), (c) capture before/after `audio_snapshot_read` traces to confirm the speedup, (d) record the result in `PERFORMANCE_BASELINE.json`. This decision **supersedes** Surface 2's Tier 2 decomposition as the primary fix; the decomposition spans (`bus_copy_memcpy` etc.) become diagnostic-only after the relocation lands.
4. **(Surface 4, 9)** `frame_deadline_missed` (Surface 9 inheritance) vs `render_frame_deadline_miss` (Surface 1 owner). Names disagree. **Resolution: rename Surface 9's reference to match Surface 1's `render_frame_deadline_miss`. Surface 1 is owner.**
5. **(Surface 5)** ESP-IDF temperature sensor API — needs a one-time init call in modern IDF 5.2.x. Implementing agent must verify with a test read at boot before relying on the gauge.
6. **(Surface 7)** Volatile bool vs `std::atomic<bool>` for cross-core toggle reads. **Recommendation: `volatile bool`.** Switch only if write-ordering bugs surface.
7. **(Surface 7)** Toggling mid-frame: snapshot at frame start or take effect immediately? **Recommendation: snapshot at frame start (read once per frame into local).**
8. **(Surface 8)** `--strict` default OFF for interactive use, ON for CI. CI invocation should be documented in `MABUTRACE_GUIDE.md`.
9. **(Surface 8)** Baseline producer location — recommend `firmware-v3/tools/baselines/<env>_<effect>.json` (one baseline per env+effect pair). Captain to confirm.
10. **(Surface 9)** Loud-regime threshold (RMS > 0.7 sustained 1 s): empirically tuned to current audio pipeline? Re-validate post-Surface 2 fix if H2 changes audio chain timing.

---

## Surfaces

For each surface: deep detail lives in the corresponding `trace_spec_sections/0N_*.md` file. The summary below contains everything an implementing agent needs to plan their PR.

### Surface 1 — Render path budget

**Owner section file:** `trace_spec_sections/01_render_budget.md`
**Tier:** 1 + 2 (`FEATURE_TRACE_PERFORMANCE`)
**Hypothesis:** Where does the 2 ms budget go? Distribution of effect render vs colour correction vs LED show vs audio snapshot.

**Required additions:**

| # | Name | Type | Tier | File:line | Args | Hypothesis | Sanity |
|---|---|---|---|---|---|---|---|
| 1 | `render_frame_work_us` | TRACE_COUNTER | 1 | RendererActor.cpp:915 (replaces existing `frame_us`) | `(int)rawFrameTimeUs` | Raw work time before pacing | p50 1400–1700, p99 < 2000 |
| 2 | `render_frame_deadline_miss` | TRACE_INSTANT | 1 | RendererActor.cpp:917 (conditional, after #1) | none | Marker for budget overruns | 0–5 per 30 s |
| 3 | `audio_snapshot_age_us` | TRACE_COUNTER | 1 | RendererActor.cpp:1404 | `(int)(dt_us & 0x7FFFFFFF)` | Audio staleness | p50 < 1000, p99 < 5000 |
| 4 | `effect_id_active` | TRACE_COUNTER | 1 | RendererActor.cpp:970 | `(int)m_currentEffect` | Per-frame regime label | matches active effect |
| 5 | `color_correction_us` | TRACE_COUNTER | 2 (`FEATURE_TRACE_PERFORMANCE`) | RendererActor.cpp:893 | `(int)(_cc_end_us - _cc_start_us)` | CC histogram | p50 10–50, p99 < 200 |

**Implementation order (most leveraged first):**
1. #1 (`render_frame_work_us`) — single most important counter; landlines the frame budget contract.
2. #2 (`render_frame_deadline_miss`) — searchable marker; cheap.
3. #4 (`effect_id_active`) — enables regime segmentation in analyser.
4. #3 (`audio_snapshot_age_us`) — staleness diagnostic.
5. #5 (`color_correction_us`) — Tier 2 follow-up.

**Acceptance:** Build `_trace` env, capture 30 s of effect 0x2100, verify ~3600 samples of each counter, p99 within thresholds.

---

### Surface 2 — Audio→Render handoff

**Owner section file:** `trace_spec_sections/02_audio_render_handoff.md`
**Tier:** 1 + 2 (`FEATURE_TRACE_AUDIO_HANDOFF`)
**Hypothesis (H2):** `audio_snapshot_read` measures 836 µs p99 — 10× the expected memcpy cost. Root cause is one of (a) PSRAM cache miss, (b) hidden derivation work, (c) lock-free retry overhead.

**ARCHITECTURAL DECISION (2026-04-27, Captain-approved):** `ControlBusFrame` will be relocated from PSRAM to internal DRAM (5 KB cost approved; expected 5–10× speedup). This is the **primary fix** for H2 and supersedes the Tier 2 decomposition as a remediation. Implementation steps for the next agent:
1. Verify current allocation site of `SnapshotBuffer<ControlBusFrame>` in `AudioActor` and `RendererActor`.
2. Ensure the buffer lives in internal DRAM — either as a value member of a DRAM-resident actor (default for stack/static) or via explicit `heap_caps_malloc(..., MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)`.
3. Confirm no PSRAM placement attribute (e.g. `EXT_RAM_ATTR`) is applied to the SnapshotBuffer or its embedded ControlBusFrame instances.
4. Apply Tier 1 instrumentation (#4 `audio_snapshot_age_us`, #5 hop_seq_lag, #6 size validation) **first** to establish the contract; then make the DRAM change; capture before/after traces; commit `PERFORMANCE_BASELINE.json` deltas.
5. The Tier 2 decomposition spans (rows #1–3, #7–9 below) become **diagnostic-only** post-fix — implement only if the DRAM move does NOT close the gap to <200 µs p99.

**Required additions:**

| # | Name | Type | Tier | File:line | Args | Hypothesis | Sanity |
|---|---|---|---|---|---|---|---|
| 1 | `audio_snapshot_read` (outer span) | TRACE_SCOPE | 2 | RendererActor::onTick (~line 650) | none | Wraps full handoff | p99 = 836 µs baseline |
| 2 | `bus_copy_memcpy` | TRACE_SCOPE | 2 | SnapshotBuffer.h::ReadLatest | none | Just the struct copy | < 100 µs DRAM, > 300 µs = PSRAM |
| 3 | `bus_retry_check` | TRACE_INSTANT + counter | 2 | SnapshotBuffer.h::ReadLatest | seq_changed | Retry frequency | < 5% of calls |
| 4 | `audio_snapshot_age_us` | TRACE_COUNTER | 1 | (shared with Surface 1 #3) | dt_us | Staleness | peak 0–100 µs |
| 5 | `audio_snapshot_hop_seq_lag` | TRACE_COUNTER | 1 | RendererActor::onTick post-read | lag | Renderer lapping audio | small, stable |
| 6 | `audio_snapshot_size_bytes` | TRACE_COUNTER | 1 | RendererActor::onStart | sizeof(ControlBusFrame) | Static validation | exactly 5120 |
| 7 | `audio_ctx_populate_us` | TRACE_SCOPE | 2 | RendererActor::renderFrame post-read | none | EffectContext copy | < 100 µs |
| 8 | `motion_engine_tick_us` | TRACE_SCOPE | 2 | RendererActor::renderFrame | none | Motion-semantic inference | < 50 µs |
| 9 | `motion_shaper_tick_us` | TRACE_SCOPE | 2 | RendererActor::renderFrame | none | Temporal envelope shaping | < 30 µs |
| 10 | `snapshot_read_retries_total` | TRACE_COUNTER | 1 | static atomic incremented on retry | count | Cumulative diagnostic | ratio analysis |

**Implementation order:** #4–6 first (cheap Tier 1, validate frame size and lag). Then #1, #2 (the diagnostic core for H2). Then #7–9 (downstream cost decomposition).

**Acceptance:** Sum of (`bus_copy_memcpy` + `audio_ctx_populate_us` + `motion_engine_tick_us` + `motion_shaper_tick_us`) ≈ `audio_snapshot_read` ±10 µs. If `bus_copy_memcpy` > 300 µs, file ADR for ControlBusFrame DRAM relocation.

---

### Surface 3 — Audio analysis stack (Core 0)

**Owner section file:** `trace_spec_sections/03_audio_core0.md`
**Tier:** 1 + 2 (`FEATURE_TRACE_AUDIO_DSP`)
**Hypothesis:** What is the per-hop cost distribution on Core 0? Onset detector's 1024-point FFT is suspected to be the single largest contributor (~3–5 ms of an 8 ms hop).

**Required additions (Tier 1, always-on, ~6 events/hop):**

| # | Name | Type | File:line | Description |
|---|---|---|---|---|
| 1 | `audio_hop_us` | TRACE_COUNTER | AudioActor.cpp (wrap onTick body) | Total hop duration |
| 2 | `audio_hop_freq` | TRACE_COUNTER | AudioActor.cpp (periodic) | Hz × 100 |
| 3 | `audio_silence_scale` | TRACE_COUNTER | ControlBusFrame.silentScale × 1000 | Silence gate state |
| 4 | `audio_rms_x1000` | TRACE_COUNTER | ControlBusFrame.rms × 1000 | RMS energy |
| 5 | `onset_process_us` | TRACE_COUNTER | AudioActor.cpp:728 | Internal onset timing |
| 6 | `audio_hop_count` | TRACE_COUNTER | AudioActor monotonic seq | Hop index |

**Required additions (Tier 2, opt-in, ~8 spans/hop):**

| # | Name | Type | File:line | Expected duration |
|---|---|---|---|---|
| 7 | `i2s_dma_read` | TRACE_SCOPE | AudioActor.cpp:593–601 | ~5 ms (blocking I2S) |
| 8 | `stm_rfft_256` | TRACE_SCOPE | AudioActor.cpp:642–643 | 2–3 ms |
| 9 | `stm_extract` | TRACE_SCOPE | AudioActor.cpp:663 | ~0.5 ms |
| 10 | `onset_detect_span` | TRACE_SCOPE | AudioActor.cpp:715–717 | 3–5 ms (1024-point FFT) |
| 11 | `band_ratio_detect` | TRACE_SCOPE | AudioActor.cpp:766–790 | ~0.5 ms |
| 12 | `controlbus_update_stage_b` | TRACE_SCOPE | ControlBus.cpp implicit | 0.5–1 ms |
| 13 | `controlbus_publish` | TRACE_SCOPE | AudioActor.cpp:857+ | ~0.1 ms |

**Implementation order:** Tier 1 #1 (`audio_hop_us`) first — single number that proves we're inside or outside the 6 ms ceiling. Then Tier 1 #2–6. Tier 2 only when investigating overruns.

**Acceptance:** Tier 1 capture for 30 s shows `audio_hop_us` p99 < 6000 µs. With `FEATURE_TRACE_AUDIO_DSP=1`, sum of Tier 2 spans ≈ `audio_hop_us` ±0.5 ms.

---

### Surface 4 — WiFi / WebServer perturbation

**Owner section file:** `trace_spec_sections/04_wifi_webserver.md`
**Tier:** 1 + 2 (`FEATURE_TRACE_NETWORK`) + 4
**Hypothesis:** WS dispatch on Core 0 indirectly perturbs Core 1 render via heap contention, cache coherency, and mutex contention. Expected p99 render frame increase of 100–300 µs during active WS traffic.

**Tier 4 instants (always-on with `FEATURE_MABUTRACE=1`):**

| Name | When | File:line |
|---|---|---|
| `wifi_ap_started` | WIFI_EVENT_AP_START | WiFiManager.cpp:~1068 |
| `wifi_ap_stopped` | WIFI_EVENT_AP_STOP | WiFiManager.cpp |
| `wifi_client_connected_<N>` | AP_STA_CONNECTED | WebServer.cpp:~454 |
| `wifi_client_disconnected_<N>` | AP_STA_DISCONNECTED | WebServer.cpp:~454 |
| `ws_client_connected_<id>` | WS handshake accept | WsGateway.cpp:~75 |
| `ws_client_disconnected_<id>` | WS close | WsGateway.cpp:~81 |
| `ota_started` / `ota_chunk` / `ota_completed` / `ota_failed` | OTA lifecycle | WsOtaCommands.cpp |

**Tier 1 counters (1 Hz from health task):**

| Name | Source |
|---|---|
| `wifi_clients` | `WiFi.softAPgetStationNum()` |
| `ws_clients` | `WsGateway::Stats::connectAccepted` (cumulative) |
| `ws_dispatch_count` | per-handleMessage increment |
| `ws_errors` | parseErrors + unknownCommands |

**Tier 2 spans (`FEATURE_TRACE_NETWORK=1`):**
- `ws_msg_dispatch` — wraps `WsGateway::handleMessage`. Args: `msg_type`, `msg_size_bytes`, `client_id`.
- Per-handler spans: `ws_handler_effects_set_current`, `ws_handler_parameters_set`, `ws_handler_palette_set`, `ws_handler_zones_set`, `ws_handler_preset_*`, `ws_handler_ota_*`, `ws_handler_stream_subscribe`.
- REST handler spans: `rest_get_device_status`, `rest_get_effects_metadata`, `rest_put_effects_current`, `rest_put_parameters_<name>`.

**Acceptance:** With `FEATURE_TRACE_NETWORK=1`, run `analyse_trace.py --conditional 'render_frame_us BY ws_msg_dispatch in last 16ms'`. Output must produce a non-null KS D-statistic. Strong correlation (D > 0.2) validates the perturbation hypothesis; weak (D < 0.05) redirects investigation to other vectors (heap/WiFi events).

---

### Surface 5 — Memory/heap + power/thermal

**Owner section file:** `trace_spec_sections/05_memory_thermal.md`
**Tier:** 1 + 4
**Critical safety rule:** All sampling MUST run from the 1 Hz background task in `main.cpp:~450`. NEVER from render path (Core 1) or audio hop (Core 0). Caused d14 Step 3 stutter+corruption (per `feedback_no_heap_scans_in_high_freq_paths.md`).

**Tier 1 gauges (sampled at 1 Hz, ~6 events/sec):**

| Name | Source |
|---|---|
| `heap_free_internal_kb` | `heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024` |
| `heap_free_psram_kb` | `heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024` |
| `heap_largest_internal_kb` | `heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024` |
| `heap_largest_psram_kb` | `heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024` |
| `task_stack_hwm_loop` | `uxTaskGetStackHighWaterMark(xTaskGetCurrentTaskHandle())` |
| `task_stack_hwm_renderer` | requires `g_rendererTaskHandle` global stored in `RendererActor::init` |
| `task_stack_hwm_audio` | requires `g_audioTaskHandle` global stored in `AudioActor::init` |
| `task_stack_hwm_show_director` | requires `g_showDirectorHandle` |
| `temp_celsius_x10` | `temp_sensor_read_celsius()` × 10 (fixed-point) |

**Tier 4 threshold instants:**

| Name | Trigger | Hysteresis |
|---|---|---|
| `oom_warning` | `heap_free_internal_kb < 20` | fire once per crossing |
| `psram_pressure_warn` | `heap_free_psram_kb < 256` | fire once per crossing |
| `thermal_throttle_warn` | `temp_c >= 80.0` | fire once per crossing |
| `heap_alloc_failure_internal` / `_psram` | malloc returns null (requires HeapMonitor wrapper) | per-failure |

**Implementation:** wrap a 1 Hz sub-counter inside the existing 10 s health-check block in `main.cpp:~450`. Store task handles as module-level globals at actor init time.

**ESP-IDF temperature sensor:** ESP-IDF 5.2.x present with `<driver/temperature_sensor.h>`. Verify with one-time test read at boot. ±1 °C accuracy, ~50–100 ms response.

---

### Surface 6 — Effect lifecycle

**Owner section file:** `trace_spec_sections/06_effect_lifecycle.md`
**Tier:** 1 + 2 (`FEATURE_TRACE_EFFECT_LIFECYCLE`)
**Hypothesis:** Effect transitions today have zero visibility. Init/cleanup duration leaks, PSRAM allocation patterns, first-frame cold-cache cost are all unmeasured. Architecture is safe (cleanup→state-update→init pattern, m_effectInitialized gate prevents mid-init render); we just need numbers.

**Tier 1 instants (8) and counters (4):**

| # | Name | Type | File:line | Args |
|---|---|---|---|---|
| 1 | `effect_switch` | TRACE_INSTANT | RendererActor.cpp:570 | old_eid, new_eid |
| 2 | `effect_switch_rejected` | TRACE_INSTANT | RendererActor.cpp:587 | new_eid, reason_code, free_heap |
| 3 | `effect_cleanup_start_<eid>` / `_end_<eid>` | TRACE_INSTANT pair | RendererActor.cpp:597–600 | eid, duration_us |
| 4 | `effect_init_start_<eid>` / `_end_<eid>` | TRACE_INSTANT pair | RendererActor.cpp:619–629 | eid, duration_us, success |
| 5 | `effect_psram_alloc_<eid>` | TRACE_INSTANT | inside effect's init() | eid, bytes, success |
| 6 | `effect_psram_alloc_failed_<eid>` | TRACE_INSTANT | inside effect's init() failure path | eid, requested, available |
| 7 | `effect_init_us` | TRACE_COUNTER (derived) | computed from start/end | per switch |
| 8 | `effect_cleanup_us` | TRACE_COUNTER (derived) | computed from start/end | per switch |
| 9 | `effect_psram_alloc_bytes` | TRACE_COUNTER | latest per active effect | gauge |

`effect_id_active` (per-frame counter) is NOT owned here — Surface 1 owns it. This surface uses it for cross-reference only.

**Tier 2 spans (`FEATURE_TRACE_EFFECT_LIFECYCLE=1`):**
- `effect_init_<eid>` — wraps full init() including vTaskDelay buffers
- `effect_cleanup_<eid>` — wraps cleanup()
- `effect_render_first_frame_<eid>` — first render() call after init becomes valid; surfaces cold-cache cost

**Acceptance:** A capture spanning two effect switches must show paired start/end events in the right order: `effect_switch` → `effect_cleanup_start` → `effect_cleanup_end` → `effect_init_start` → `effect_init_end` → next frame's `effect_id_active` updated.

---

### Surface 7 — Runtime A/B bench framework (Tier 3)

**Owner section file:** `trace_spec_sections/07_bench_framework.md`
**Tier:** 3 (runtime toggles, instrumented via Tier 4 instants)
**Hypothesis:** Captain can A/B-test a setting (e.g. `render.async_rmt`) without rebuilding firmware: toggle → run 30 s → toggle → run 30 s → analyse. Converts 5–10 min rebuild cycles into 30 s interactive cycles.

**Files to add:**
- `src/utils/BenchRegistry.h` — `ToggleDescriptor`, `BenchRegistry` class, `BENCH_REGISTER_TOGGLE` macro, `isToggleEnabled` inline helper. Fixed-size `s_registry[BENCH_REGISTRY_MAX = 32]`. No heap.
- `src/utils/BenchRegistry.cpp` — implementation; `findByName` is linear scan, fine at ≤32 entries.
- `src/serial/SerialCLI.cpp:~1184` (after `trace` handler) — `bench {list|begin|split|end|toggle|reset}` command parser.

**Initial toggle catalogue (THE most actionable list — these are the hypothesis-test variables):**

| Name | Type | Default | Description | Consumer file |
|---|---|---|---|---|
| `render.async_rmt` | bool | true | Async vs sync `FastLED.show()` | RendererActor.cpp |
| `render.color_correction` | bool | true | Colour correction pipeline on/off | RendererActor.cpp |
| `render.dual_strip_parallel` | bool | true | Parallel vs sequential RMT writes | RendererActor.cpp |
| `audio.lookahead` | bool | true | Lookahead spike smoothing | AudioBeatTracker.cpp |
| `audio.zone_agc` | bool | true | Zone AGC on/off | AudioActor.cpp |
| `audio.chroma_zone_agc` | bool | true | Chroma zone AGC on/off | AudioActor.cpp |
| `effect.subpixel` | bool | false | SubpixelRenderer vs integer fallback | (not yet implemented) |
| `effect.fade_to_black` | bool | true | `fadeToBlackBy` per-effect override | EffectBase.cpp |

**Trace markers fired by bench commands (Tier 4):**
- `bench_begin` (with `name` arg), `bench_split` (with `variant` arg), `bench_end`, `bench_toggle_set` (with `name`+`value`), `bench_reset`.

**Surface 8 segmentation contract:** the analyser parses these instants and partitions counter samples into per-variant buckets. See Surface 8 below.

**Hot-path invariant:** `isToggleEnabled("render.async_rmt")` MUST inline to a single volatile bool read. No string lookups in the hot path. Read once per frame into a local bool to avoid mid-frame inconsistency.

---

### Surface 8 — analyse_trace.py post-process tool

**Owner section file:** `trace_spec_sections/08_analyser_tool.md`
**Tier:** n/a (host-side Python tool)
**Status:** TO BE IMPLEMENTED. File path: `firmware-v3/tools/analyse_trace.py`.

**Contract invariants:**
1. **Pure stdlib.** No `numpy`, `scipy`, `matplotlib`. Imports limited to `json`, `statistics`, `argparse`, `pathlib`, `dataclasses`, `math`, `re`, `sys`, `csv`, `html`, `bisect`, `collections`, `typing`.
2. **Determinism.** Two runs with identical flags produce byte-identical `--json` output (excluding any `Generated:` timestamp, which must be omitted from JSON).
3. **Verdict-first output.** First line of every markdown report and `verdict.summary` field of JSON is a `✓` / `✗` / `…` line covering all baseline contract metrics.
4. **Empty-trace tolerance.** Verdict `EMPTY`, exit 0.
5. **Single pass per file.** Read once, hold parsed events in memory.

**Exit codes:** 0 success | 1 input missing | 2 JSON parse error | 3 missing required event field | 4 conditional metric not in trace | 5 baseline unreadable | 6 regime-by matched zero events | 7 contract FAIL with `--strict`.

**Core analyses (A–G):**
- A. Per-counter histograms (16 buckets, nearest-rank percentiles for determinism)
- B. Per-span duration histograms (matching B/E pairs LIFO by `name`+`tid`)
- C. Instant frequency (mean inter-arrival)
- D. Render frame deadline-miss table (joins `effect_id_active` for each miss)
- E. Regime segmentation (default: `bench_begin`/`split`/`end`; `--regime-by PREFIX` for custom)
- F. Conditional stats (the killer feature) — `--conditional 'metric BY event in last Nms'` partitions samples and reports KS D-statistic
- G. Baseline comparison — pairwise per-(metric,stat) deltas, regression flag from contracts

**KS D-statistic implementation:** stdlib only via `bisect_right` on sorted samples. See section 08 for pseudocode.

**Output formats:** markdown (default), `--html` (single self-contained file with `<details>` collapse), `--json` (sorted-key deterministic), `--csv-dir` (one CSV per metric).

**Baseline file format:** same shape as `--json` output plus `contracts` dict. Lives at `firmware-v3/tools/baselines/<env>_<effect>.json` (proposed).

**Validation tests:** `firmware-v3/test/test_native/test_analyse_trace.py` — 12-test matrix (round-trip, determinism, empty trace, missing metric, missing baseline, three-region segmentation, deadline-miss row, effect-ID join, KS=0 on identical distributions, regression detection, CSV row count, B/E without matching E).

---

### Surface 9 — Causal markers catalogue (Tier 4)

**Owner section file:** `trace_spec_sections/09_causal_markers.md`
**Tier:** 4 (cheap narrative markers, ~1–10 events/sec aggregate)
**Hypothesis:** Cheap labelled events at known boundaries enable visual correlation in Perfetto and regime segmentation by the analyser.

**Inventory of existing TRACE_INSTANT calls (16, baseline):** `ONSET_*`, `BR_*` (audio), `bps_*` (BeatParitySprite), `pvf_*` (AttackOnlyPitchVelocityField), `FALSE_TRIGGER`, `frame_drop`, `effect_change` (renderer).

**Net new markers introduced by this surface:**

A. Boot/shutdown narrative (6):
- `boot_phase_idf_done` (SystemInit.cpp post-`initSerial`)
- `boot_phase_actors_started` (ActorSystem.cpp post-start)
- `boot_phase_audio_first_hop` (AudioActor first `ControlBus::publish`)
- `boot_phase_renderer_first_frame` (RendererActor post-init, args `num_zones`)
- `boot_phase_wifi_up` (WebServer AP/STA started, args `mode`)
- `boot_phase_ready` (main.cpp end of `setup()`)

B. Audio regime transitions with hysteresis (6):
- `audio_silence_entered` / `audio_silence_exited` — silentScale crosses 0.2/0.25 with 5-frame debounce
- `audio_loud_entered` — fast_rms > 0.7 sustained 1 s
- `tempo_lock_acquired` / `tempo_lock_lost` — tempoConfidence crosses 0.5/0.4
- `chord_change` — chordState type or rootNote changes (transition-only)

Canonical hysteresis state machine pattern (silence example):
```cpp
static int silenceFrameCount = 0;
static bool inSilenceRegime = false;
const float SILENCE_THRESHOLD = 0.2f, SILENCE_HYSTERESIS = 0.05f;
const int SILENCE_DEBOUNCE_FRAMES = 5;
if (!inSilenceRegime && frame.silentScale < SILENCE_THRESHOLD) {
    if (++silenceFrameCount >= SILENCE_DEBOUNCE_FRAMES) {
        inSilenceRegime = true;
        TRACE_INSTANT("audio_silence_entered");
    }
} else if (inSilenceRegime && frame.silentScale > SILENCE_THRESHOLD + SILENCE_HYSTERESIS) {
    silenceFrameCount = 0; inSilenceRegime = false;
    TRACE_INSTANT("audio_silence_exited");
}
```

C. Health/system regime (2): `heap_pressure_entered` (free_internal < 20 KB) / `heap_pressure_exited` (> 25 KB). Implemented in 1 Hz health task, owned jointly with Surface 5.

D. User actions (optional Tier 4.5, 2): `button_pressed_<id>`, `encoder_turn_<id>_<direction>`. Include only if manual control narrative is critical.

E. OTA / configuration (2): `config_save` (ZoneConfigManager NVS write, args `key`), `firmware_version` (main.cpp boot, args `version`).

**Cross-surface deferral:** Surface 9 does NOT instrument WiFi events (Surface 4 owner), `effect_switch` (Surface 6 owner), heap counters (Surface 5 owner), or `render_frame_deadline_miss` (Surface 1 owner). It defers and references.

---

## Implementation roadmap (the spine)

A single ordered list of every file:line touchpoint across all 9 surfaces. An agent who reads only this list can land the entire spec sequentially. Each entry: `[Surface] [Tier] file:line — change — verification`.

### Phase 1 — Foundation (Tier 1 always-on, no flags, biggest leverage)

1. **[1][T1]** `RendererActor.cpp:915` — replace `frame_us` with `render_frame_work_us` counter; add `render_frame_deadline_miss` instant after. **Verify:** capture shows ~3600 samples in 30 s; deadline-miss count 0–5.
2. **[1][T1]** `RendererActor.cpp:970` — add `effect_id_active` counter at end of frame. **Verify:** counter matches active effect across switches.
3. **[1][T1]** `RendererActor.cpp:1404` — add `audio_snapshot_age_us` counter after `dt_us` calculation. **Verify:** p50 < 1000, p99 < 5000.
4. **[2][T1]** `RendererActor.cpp::onTick` — add `audio_snapshot_hop_seq_lag` counter (post-read). **Verify:** small, stable.
5. **[2][T1]** `RendererActor.cpp::onStart` — add `audio_snapshot_size_bytes` one-shot counter. **Verify:** == 5120.
6. **[2][T1]** `SnapshotBuffer.h::ReadLatest` — add `snapshot_read_retries_total` static atomic increment on retry path. **Verify:** ratio < 5% under steady audio.
7. **[3][T1]** `AudioActor.cpp:580 onTick` — wrap with `audio_hop_us` span/counter. **Verify:** p99 < 6000 µs.
8. **[3][T1]** `AudioActor.cpp` — add `audio_hop_freq`, `audio_silence_scale`, `audio_rms_x1000`, `onset_process_us` (line 728), `audio_hop_count` counters. **Verify:** all populate at ~125 Hz.
9. **[5][T1]** `main.cpp:~450` — add 1 Hz sub-counter inside 10 s health block; emit `heap_free_*`, `heap_largest_*`, stack HWM, temp gauges. **Verify:** ~6–9 events/sec at 1 Hz.
10. **[5][T1]** `RendererActor::init`, `AudioActor::init`, `ShowDirectorActor::init` — store task handle in module-level global. **Verify:** stack HWM samples populate per task.
11. **[5]** Verify `temp_sensor_read_celsius` works at boot via test read; document in `MABUTRACE_GUIDE.md` if init call required.

### Phase 2 — Effect & network lifecycle (Tier 1 always-on + Tier 4)

12. **[6][T1]** `RendererActor.cpp:570` — add `effect_switch` instant (old_eid, new_eid).
13. **[6][T1]** `RendererActor.cpp:587` — add `effect_switch_rejected` instant on heap floor failure.
14. **[6][T1]** `RendererActor.cpp:597–600` — add `effect_cleanup_start_<eid>` / `_end_<eid>` instants and `effect_cleanup_us` derived counter.
15. **[6][T1]** `RendererActor.cpp:619–629` — add `effect_init_start_<eid>` / `_end_<eid>` instants and `effect_init_us` derived counter.
16. **[6][T1]** Each effect's `init()` — emit `effect_psram_alloc_<eid>` (success) or `effect_psram_alloc_failed_<eid>` (failure) and update `effect_psram_alloc_bytes` gauge.
17. **[4][T4]** `WiFiManager.cpp:~1068` — `wifi_ap_started` / `_stopped` instants in event handlers.
18. **[4][T4]** `WebServer.cpp:~454` — `wifi_client_connected_<N>` / `_disconnected_<N>` instants.
19. **[4][T4]** `WsGateway.cpp:~75/81` — `ws_client_connected_<id>` / `_disconnected_<id>` instants.
20. **[4][T4]** `WsOtaCommands.cpp` — `ota_started` / `_chunk` (10% intervals) / `_completed` / `_failed` instants.
21. **[4][T1]** `main.cpp` health task — emit `wifi_clients`, `ws_clients`, `ws_dispatch_count`, `ws_errors` counters at 1 Hz.

### Phase 3 — Causal narrative (Tier 4)

22. **[9][T4]** Boot phase markers in `SystemInit.cpp`, `ActorSystem.cpp`, `AudioActor.cpp`, `RendererActor.cpp`, `WebServer.cpp`, `main.cpp`.
23. **[9][T4]** Audio regime markers in `AudioActor.cpp` (`~line 800+`) — implement hysteresis state machines for silence, loud, tempo lock, chord change.
24. **[9][T4]** Health regime markers (`heap_pressure_*`) in 1 Hz health task — joint with Surface 5.
25. **[9][T4]** `config_save` in `ZoneConfigManager.cpp` NVS write completion. `firmware_version` in `main.cpp:setup()`.
26. **[9][T4 optional]** `button_pressed_<id>` / `encoder_turn_*` in `EncoderManager.cpp` — defer unless user-action narrative is critical.

### Phase 4 — Tier 2 decomposition (opt-in flags)

27. **[1][T2]** `RendererActor.cpp:881–893` — add `color_correction_us` counter inside CC block; gate on `FEATURE_TRACE_PERFORMANCE` (or fold into Tier 1 per Open Question 2).
28. **[2][T2]** `RendererActor::onTick` — add `audio_snapshot_read` outer scope; gate on `FEATURE_TRACE_AUDIO_HANDOFF`.
29. **[2][T2]** `SnapshotBuffer.h::ReadLatest` — wrap struct copy in `bus_copy_memcpy` scope; emit `bus_retry_check` instant on retry; gate on `FEATURE_TRACE_AUDIO_HANDOFF`.
30. **[2][T2]** `RendererActor::renderFrame` — add `audio_ctx_populate_us`, `motion_engine_tick_us`, `motion_shaper_tick_us` scopes; gate on `FEATURE_TRACE_AUDIO_HANDOFF`.
31. **[3][T2]** `AudioActor.cpp:593–601, 642–663, 715–717, 766–790, 857+` — add `i2s_dma_read`, `stm_rfft_256`, `stm_extract`, `onset_detect_span`, `band_ratio_detect`, `controlbus_publish` scopes; gate on `FEATURE_TRACE_AUDIO_DSP`.
32. **[3][T2]** `ControlBus.cpp` — add `controlbus_update_stage_b` scope; gate on `FEATURE_TRACE_AUDIO_DSP`.
33. **[4][T2]** `WsGateway.cpp::handleMessage` — add `ws_msg_dispatch` scope; gate on `FEATURE_TRACE_NETWORK`.
34. **[4][T2]** Each WS handler (`WsEffectsCommands.cpp`, `WsParameterCommands.cpp`, `WsPaletteCommands.cpp`, `WsZonesCommands.cpp`, `WsPresetCommands.cpp`, `WsOtaCommands.cpp`, `WsStreamCommands.cpp`) — wrap in `ws_handler_*` scope.
35. **[4][T2]** `V1ApiRoutes.cpp` (~lines 96, 154, +) — wrap high-frequency REST handlers in `rest_*` scopes.
36. **[6][T2]** `RendererActor.cpp:619–629` — add `effect_init_<eid>` and `effect_cleanup_<eid>` outer scopes; gate on `FEATURE_TRACE_EFFECT_LIFECYCLE`.
37. **[6][T2]** `RendererActor::render` — add `effect_render_first_frame_<eid>` scope on first frame after `m_effectInitialized` becomes true.

### Phase 5 — Bench framework (Tier 3) and analyser (host)

38. **[7]** Create `src/utils/BenchRegistry.h` and `BenchRegistry.cpp`. 32-entry fixed array. `BENCH_REGISTER_TOGGLE` macro. `isToggleEnabled` inline helper.
39. **[7]** Modify `src/serial/SerialCLI.cpp:~1184` — add `bench {list|begin|split|end|toggle|reset}` parser. Fire `bench_*` instants on commands.
40. **[7]** Wire 8 initial toggles in their consumer files: `RendererActor.cpp` (×3), `AudioActor.cpp` / `AudioBeatTracker.cpp` (×3), `EffectBase.cpp` (`fade_to_black`). `effect.subpixel` deferred until SubpixelRenderer integrates.
41. **[7]** Update `platformio.ini` — add `esp32dev_audio_esv11_k1v2_32khz_trace` and `_trace_full` envs with the corresponding `build_flags`.
42. **[8]** Create `firmware-v3/tools/analyse_trace.py` per Surface 8 spec. Stdlib only.
43. **[8]** Create `firmware-v3/test/test_native/test_analyse_trace.py` with the 12-test matrix.
44. **[8]** Create `firmware-v3/tools/baselines/esp32dev_audio_esv11_k1v2_32khz_0x2102.json` from a known-good 30 s soak; commit `contracts` block by hand.

---

## End-to-end acceptance protocol

This single sequence proves the entire spec is correctly implemented.

1. Build `esp32dev_audio_esv11_k1v2_32khz_trace` (Tier 1 + Tier 4 only). Verify size delta from production env is < 4 KB.
2. Build `esp32dev_audio_esv11_k1v2_32khz_trace_full` (all Tier 2 flags on). Soak budget capped at 5 s.
3. For each: capture trace via `python3 firmware-v3/tools/capture_trace.py --soak 30 --effect 0x2102 --output /tmp/k1_trace_t1.json` (Tier 1) and `--soak 5` (Tier 2).
4. Run `python3 firmware-v3/tools/analyse_trace.py /tmp/k1_trace_t1.json --baseline firmware-v3/tools/baselines/esp32dev_audio_esv11_k1v2_32khz_0x2102.json --html /tmp/report.html --json /tmp/report.json --strict`. Exit code MUST be 0.
5. Verify the markdown verdict line begins with `✓` and lists all 8 contract metrics. The Tier 2 capture report must show all decomposition spans (`bus_copy_memcpy`, `onset_detect_span`, `ws_msg_dispatch`, `effect_init_<eid>` etc.) with non-empty histograms.
6. Run a bench A/B test: serial console `bench begin async_off` → `bench toggle render.async_rmt off` → wait 30 s → `bench split async_on` → `bench toggle render.async_rmt on` → wait 30 s → `bench end` → `trace`. Capture the JSON. Run analyser; verify two regions appear with side-by-side `render_frame_work_us` stats.
7. Commit baseline `firmware-v3/tools/baselines/esp32dev_audio_esv11_k1v2_32khz_0x2102.json` so future changes can A/B against it.

---

## Cross-surface consistency invariants

These rules must be respected by every implementing agent.

- **`effect_id_active` counter:** ONLY Surface 1 writes this. Surface 6 reads/cross-references only.
- **`render_frame_deadline_miss`:** ONLY Surface 1 owns. Surface 9's catalogue references but does not duplicate.
- **WiFi/WS event instants:** Surface 4 owner. Surface 9's `boot_phase_wifi_up` is the only Surface 9 marker that touches networking; it sequences (not duplicates) Surface 4's events.
- **Heap counters and `heap_pressure_*`:** Surface 5 owns the gauges. Surface 9 owns the regime markers (joint implementation in 1 Hz health task; gauges + threshold-crossing instants both emit from the same loop block).
- **Bench instants (`bench_begin`, `bench_split`, `bench_end`, `bench_toggle_set`, `bench_reset`):** Surface 7 owner. Surface 8 (analyser) parses but does not emit.
- **`effect_psram_alloc_bytes`:** Surface 6 owns. This is a per-effect gauge, NOT a heap counter. Distinct from Surface 5's `heap_free_psram_kb`.
- **Sampling location for heap/temp/stack:** ONLY the 1 Hz background task in `main.cpp:~450`. NEVER from render path or audio hop. Hard rule per `feedback_no_heap_scans_in_high_freq_paths.md`.
- **Hysteresis state for Surface 9 regime markers:** static-local state inside the audio hop loop. No heap, no global mutex. Resets on reboot.

---

## Sibling docs

- `firmware-v3/docs/debugging/MABUTRACE_GUIDE.md` — capture & view workflow (existing, last updated 2026-04-27). Implementing agent must update it with `_trace` env names, `bench` CLI grammar, and `analyse_trace.py --strict` CI invocation.
- `firmware-v3/tools/capture_trace.py` — capture script (existing).
- `firmware-v3/tools/analyse_trace.py` — analyser (per §8 of this doc, **TO BE IMPLEMENTED**).
- `firmware-v3/tools/baselines/<env>_<effect>.json` — canonical baselines (per Open Question 9).
- `firmware-v3/docs/debugging/trace_spec_sections/01..09_*.md` — full source detail per surface. Subordinate to this master spec.

---

**Document Changelog**

| Date | Author | Change |
|---|---|---|
| 2026-04-27 | claude-opus-4-7 (synthesis SSA) | Created. Synthesised from 9 surface section files in `trace_spec_sections/` (~3854 lines of source). Defines 5-tier framework, 8 contract invariants, 5 build flags, 44-step implementation spine, end-to-end acceptance protocol, cross-surface ownership rules, and 10 master open questions for Captain decision. |
