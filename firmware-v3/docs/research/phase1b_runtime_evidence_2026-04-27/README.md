# Phase 1B Runtime Capture Evidence

**Date:** 2026-04-27
**Hardware:** K1v2, MAC `b4:3a:45:a5:87:f8`
**Build env:** `esp32dev_audio_esv11_k1v2_32khz_trace`
**Reference audio source:** `/Users/spectrasynq/Workspace_Management/Software/hybrid-beat-tracker/tests/benchmark`
**Status:** Phase 1B runtime gate failed. Tier 1 HF semantics proceeded on 2026-04-28 by explicit Captain waiver, not by evidence clearance. Do not implement `96` or `128`.

## 2026-04-28 Waiver

Captain explicitly directed the work to skip the remaining Phase 1B runtime gate and move on after repeated hardware-test blockage. This is a documented deviation from the original Phase 1B gate, not a pass result.

Authorised scope after the waiver:

- Tier 1 HF semantic fields behind `FEATURE_AUDIO_HF_SEMANTICS`.
- Existing 64-bin/chroma substrate only.
- No `96` bins, no `128` bins, no new broad arrays, no new raw production `bins256` consumers.

The remaining timing finding still stands: the corrected ESV11 path exceeded the audio chunk and publish-hop gates, with `es_magnitudes_us` as the dominant measured cost.

## Captures

All playback captures used only approved files from the hybrid beat tracker benchmark corpus. No generated audio fixtures are part of this evidence set.

| Scenario | Audio source | Effect |
|---|---|---|
| `silence_idle` | none | `0x2100` |
| `satie_sparse_slow` | `clips/Satie_-_Gymnopédie_No_1.wav` | `0x2100` |
| `portishead_trip_hop_sparse` | `clips/Portishead_-_Glory_Box.wav` | `0x2100` |
| `take_five_jazz_swing` | `clips/Dave_Brubeck_-_Take_Five.wav` | `0x2100` |
| `meshuggah_dense_metal` | `clips/MESHUGGAH_-_Bleed.wav` | `0x2100` |
| `james_brown_funk` | `clips/James_Brown__Papa_s_Got_A_Brand_New_Bag.wav` | `0x2100` |
| `snoop_vocal_gfunk` | `clips/Snoop_Dogg_-_Gin_and_Juice.wav` | `0x2100` |
| `tool_heavy_effect` | `clips/TOOL_-_Lateralus.wav` | `0x2102` |

Raw traces live in `traces/`; per-capture `analyse_trace.py` markdown and JSON summaries live in `reports/`; serial health snapshots live in `health/`.

## Worst Observed Timing

| Metric | Worst scenario | n | p50 us | p95 us | p99 us | p999 us | max us | Gate |
|---|---|---:|---:|---:|---:|---:|---:|---|
| `audio_chunk_work_us` | `satie_sparse_slow` | 100 | 8339 | 9215 | 9627 | 9838 | 9838 | **FAIL**: p99 > 3200, p999 > 4000 |
| `audio_hop_us` | `portishead_trip_hop_sparse` | 50 | 22749 | 24577 | 25230 | 25230 | 25230 | **FAIL**: p99 > 6400, p999 > 8000 |
| `render_frame_work_us` | `silence_idle` | 131 | 3345 | 3994 | 4179 | 4257 | 4257 | **FAIL**: p99 > 2000 |
| `effect_render` | `silence_idle` | 131 | 417 | 487 | 506 | 515 | 515 | PASS |
| `render_frame` span | `james_brown_funk` | 132 | 1502 | 1922 | 1990 | 1993 | 1993 | PASS by span |
| `controlbus_publish_copy_us` | `james_brown_funk` | 50 | 96 | 191 | 283 | 283 | 283 | PASS, not dominant |
| `audio_snapshot_read` | `satie_sparse_slow` | 131 | 555 | 778 | 870 | 879 | 879 | Needs watch, not dominant |
| `onset_detect` | `take_five_jazz_swing` | 50 | 1977 | 2569 | 3038 | 3038 | 3038 | Marginal but under 3200 in this set |
| `onset_process_us` | `take_five_jazz_swing` | 50 | 1832 | 2417 | 2888 | 2888 | 2888 | Marginal |
| `onset_fft_frontend_us` | `take_five_jazz_swing` | 50 | 890 | 1255 | 1604 | 1604 | 1604 | Main onset sub-cost |
| `onset_decision_us` | `silence_idle` | 52 | 820 | 1204 | 1505 | 1505 | 1505 | Secondary onset sub-cost |
| `onset_flux_us` | `silence_idle` | 52 | 755 | 1080 | 1402 | 1402 | 1402 | Secondary onset sub-cost |
| `band_ratio_detect` | `portishead_trip_hop_sparse` | 50 | 74 | 107 | 371 | 371 | 371 | Cheap relative to FFT onset |
| `show_leds` | `silence_idle` | 131 | 845 | 966 | 1004 | 1021 | 1021 | CPU-return timing only |
| `fastled_rmt_show` | `portishead_trip_hop_sparse` | 132 | 555 | 662 | 684 | 693 | 693 | CPU-return timing only |

## Deadline Miss Counters

The trace counters are cumulative. The deltas below are within each trace window.

| Scenario | Chunk miss delta | Hop miss delta | Render-frame miss count |
|---|---:|---:|---:|
| `silence_idle` | 103 | 51 | 1 |
| `satie_sparse_slow` | 99 | 50 | 0 |
| `portishead_trip_hop_sparse` | 100 | 49 | 0 |
| `take_five_jazz_swing` | 99 | 49 | 0 |
| `meshuggah_dense_metal` | 100 | 49 | 0 |
| `james_brown_funk` | 99 | 49 | 0 |
| `snoop_vocal_gfunk` | 100 | 50 | 0 |
| `tool_heavy_effect` | 104 | 51 | 0 |

This fails the Phase 1B timing gate. The chunk/hop counters appear to count wall-clock periods including blocking capture time, so the next engineering task is to decide whether the gate definition or instrumentation is wrong. Until that is resolved, the gate must be treated as failed rather than waived.

## Health And Stability

Post-run serial health:

- AP mode active, IP `192.168.4.1`; the health snapshots show `Clients: 0`, so AP idle/client-connected Gate A was not completed.
- `showSkips=0`; LED show average around `535 us`, max `2091 us`.
- Renderer stack watermark after captures: `10256` words.
- Internal heap is critically low: `Free heap: 17824 bytes`, `Min free heap: 16304 bytes`, `Max alloc heap: 8180 bytes`.
- A clean device reboot reproduced the failure before playback: `internal=18248` then `17980`, `largest=8180`, with `shed<18432,resume>28672`.
- WebServer low-heap shedding was not merely active; it was force-clearing while heap was still below the hard shed threshold, then re-enabling on the next probe. That is an invalid hysteresis-controller loop, not a harmless warning.

This fails the memory stability gate and blocks semantic expansion.

Follow-up code inspection found two separate issues:

1. `WebServer::updateLowHeapShedState()` allowed max-latch force-clear even when `freeInternal < shed<`. This has been corrected so force-clear is only possible after heap has climbed into the hysteresis band.
2. MabuTrace's 64 KB ring buffer only uses PSRAM when `USE_PSRAM_IF_AVAILABLE` is defined. The trace PlatformIO environments now define it so evidence builds allocate that buffer from PSRAM first.

The first post-fix reflash stopped the invalid force-clear/re-enable loop, but did not fix the underlying DRAM pressure: `Free heap: 18064 bytes`, `Min free heap: 16292 bytes`, `Max alloc heap: 8180 bytes`.

The follow-up lean trace build disables optional WebSocket/UDP streaming and heavy debug/profiling surfaces in `esp32dev_audio_esv11_k1v2_32khz_trace` while preserving REST/WebSocket control and serial MabuTrace capture. After flashing that build to MAC `b4:3a:45:a5:87:f8`, the 60-second serial watch showed no low-heap shedding warnings and reported:

- `Free heap: 30716 bytes`
- `Min free heap: 28920 bytes`
- `Max alloc heap: 20468 bytes`
- `FPS: 119`
- `LED show: avg=540 us, max=1925 us, skips=0`

Evidence: `health/post_lean_trace_flash_60s_watch.txt`.

This clears the immediate trace-firmware heap-shedding symptom for idle AP/no-client observation. It does not close Phase 1B because chunk/hop timing gates, AP client Gate A, and AP telemetry/debug Gate B remain unresolved.

## Lean Trace Rerun

After the lean trace firmware was flashed, the approved benchmark corpus capture matrix was rerun under `lean_trace_rerun/`. All eight captures completed and all eight `analyse_trace.py` summaries were produced. Playback sources were limited to the approved benchmark files under `/Users/spectrasynq/Workspace_Management/Software/hybrid-beat-tracker/tests/benchmark/clips`.

| Metric | Worst scenario | n | p50 us | p95 us | p99 us | p999 us | max us | Gate |
|---|---|---:|---:|---:|---:|---:|---:|---|
| `audio_chunk_work_us` | `james_brown_funk` | 101 | 8449 | 9236 | 9589 | 9607 | 9607 | **FAIL** |
| `audio_hop_us` | `meshuggah_dense_metal` | 50 | 22757 | 24582 | 25365 | 25365 | 25365 | **FAIL** |
| `render_frame_work_us` | `tool_heavy_effect` | 136 | 3532 | 4060 | 4272 | 4317 | 4317 | **FAIL** |
| `effect_render` | `silence_idle` | 131 | 410 | 489 | 498 | 499 | 499 | PASS |
| `render_frame` span | `james_brown_funk` | 132 | 1527 | 1937 | 2041 | 2044 | 2044 | **FAIL** |
| `controlbus_publish_copy_us` | `meshuggah_dense_metal` | 50 | 94 | 183 | 269 | 269 | 269 | PASS, not dominant |
| `snapshot_publish` | `take_five_jazz_swing` | 50 | 118 | 204 | 288 | 288 | 288 | PASS, not dominant |
| `audio_snapshot_read` | `take_five_jazz_swing` | 131 | 544 | 817 | 870 | 906 | 906 | Watch, not dominant |
| `onset_detect` | `tool_heavy_effect` | 53 | 1995 | 2578 | 2817 | 2817 | 2817 | Provisional PASS |
| `onset_process_us` | `tool_heavy_effect` | 52 | 1851 | 2433 | 2668 | 2668 | 2668 | Provisional PASS |
| `onset_fft_frontend_us` | `snoop_vocal_gfunk` | 50 | 904 | 1253 | 1400 | 1400 | 1400 | Main onset sub-cost |
| `onset_decision_us` | `tool_heavy_effect` | 52 | 913 | 1180 | 1378 | 1378 | 1378 | Secondary onset sub-cost |
| `onset_flux_us` | `tool_heavy_effect` | 53 | 829 | 1073 | 1260 | 1260 | 1260 | Secondary onset sub-cost |
| `band_ratio_detect` | `james_brown_funk` | 50 | 76 | 108 | 128 | 128 | 128 | PASS |

| Scenario | Chunk miss delta | Hop miss delta | Render-frame miss count |
|---|---:|---:|---:|
| `james_brown_funk` | 100 | 49 | 2 |
| `meshuggah_dense_metal` | 99 | 49 | 1 |
| `portishead_trip_hop_sparse` | 100 | 49 | 0 |
| `satie_sparse_slow` | 100 | 49 | 0 |
| `silence_idle` | 107 | 52 | 0 |
| `snoop_vocal_gfunk` | 100 | 50 | 0 |
| `take_five_jazz_swing` | 100 | 49 | 2 |
| `tool_heavy_effect` | 103 | 52 | 2 |

Post-capture health remained above the heap shed threshold:

- `Free heap: 30520 bytes`
- `Min free heap: 25280 bytes`
- `Max alloc heap: 20468 bytes`
- `FPS: 119`
- `LED show: avg=542 us, max=1916 us, skips=0`

The 30-minute idle AP/no-client burn-in completed for 1805 seconds with:

- `low_heap_shedding_lines=0`
- `panic_watchdog_lines=0`
- `free_heap_min_max=30520..30520`
- `min_free_heap_min_max=25280..25280`
- `max_alloc_heap_min_max=20468..20468`
- `fps_min_max=119..119`
- `led_avg_us_min_max=530..574`
- `led_max_us_min_max=1916..1916`
- `showSkips_min_max=0..0`
- `clients_min_max=0..0`

Evidence:

- `lean_trace_rerun/capture_matrix_manifest.json`
- `lean_trace_rerun/reports/*.report.json`
- `lean_trace_rerun/health/post_capture_health.txt`
- `lean_trace_rerun/health/burnin_30min_idle_ap_no_client_summary.txt`
- `lean_trace_rerun/health/burnin_30min_idle_ap_no_client.txt`

AP Gate A was attempted from the host, but it is not accepted as passed. `networksetup -setairportnetwork en0 LightwaveOS-AP ''` did not leave the Mac associated with the K1 AP, `curl http://192.168.4.1/api/v1/status` timed out, and the burn-in evidence reports `clients_min_max=0..0`. Gate A therefore remains blocked by host association/client evidence, not by the idle AP heap result.

## Corrected Timing Rerun

The original `audio_chunk_work_us` and `audio_hop_us` counters mixed blocking I2S cadence time with CPU work. The trace firmware was updated to split:

- `audio_chunk_wall_us`
- `audio_chunk_capture_us`
- `audio_chunk_work_us`
- `audio_hop_wall_us`
- `audio_hop_us`

The corrected approved-corpus rerun completed under `corrected_timing_rerun/`. It confirmed the previous gate was partly mislabelled, but it did not clear Phase 1B: ESV11 CPU work is still over budget.

| Metric | Worst scenario | n | p50 us | p95 us | p99 us | p999 us | max us | Gate |
|---|---|---:|---:|---:|---:|---:|---:|---|
| `audio_chunk_work_us` | `snoop_vocal_gfunk` | 96 | 5701 | 6720 | 7093 | 7093 | 7093 | **FAIL** |
| `audio_hop_us` | `satie_sparse_slow` | 48 | 15789 | 17637 | 18230 | 18230 | 18230 | **FAIL** |
| `audio_chunk_wall_us` | `satie_sparse_slow` | 96 | 8421 | 9284 | 10004 | 10004 | 10004 | Cadence only |
| `audio_hop_wall_us` | `satie_sparse_slow` | 48 | 22760 | 25173 | 25672 | 25672 | 25672 | Cadence only |
| `onset_detect` | `portishead_trip_hop_sparse` | 48 | 1928 | 2589 | 2905 | 2905 | 2905 | Provisional PASS |
| `band_ratio_detect` | `snoop_vocal_gfunk` | 48 | 77 | 116 | 532 | 532 | 532 | PASS |

Post-capture health remained stable: `Free heap: 30704 bytes`, `Min free heap: 28920 bytes`, `Max alloc heap: 20468 bytes`, `FPS: 119`, `LED show: avg=537 us, max=1911 us, skips=0`.

The follow-up ES component timing probe under `es_component_timing_probe/` identifies the dominant cost:

| Metric | Worst scenario | n | p50 us | p95 us | p99 us | max us |
|---|---|---:|---:|---:|---:|---:|
| `es_magnitudes_us` | `tool_heavy_effect` | 89 | 3901 | 4833 | 5420 | 5420 |
| `es_tempo_us` | `tool_heavy_effect` | 89 | 829 | 1539 | 1854 | 1854 |
| `es_gpu_tick_us` | `snoop_vocal_gfunk` | 86 | 823 | 1170 | 1245 | 1245 |
| `es_refresh_us` | `silence_idle` | 87 | 142 | 225 | 272 | 272 |
| `es_chroma_us` | `silence_idle` | 87 | 14 | 21 | 25 | 25 |
| `es_vu_us` | `silence_idle` | 87 | 26 | 47 | 73 | 73 |

Decision: onset is not the current blocker. ControlBus copy is not the current blocker. Heap is not the current blocker on the lean trace build. The remaining audio blocker is the ESV11 musical magnitude path, followed by tempo and ES render-domain tick work. Phase 1B remains blocked until that work is decimated, spread across frames, or otherwise replaced with a cheaper 64-bin production extractor.

## Gate Answers

1. **Audio chunk safe?** No. `audio_chunk_work_us` p99 is far above the 3200 us gate and miss counters increase in every trace.
2. **Publish-hop safe?** No. `audio_hop_us` p99 is far above the 6400 us gate and hop miss counters increase in every trace.
3. **Onset safe?** Not cleared. `onset_detect` stayed below 3200 us in this approved-corpus set, but it remains near the budget and prior traces exceeded it.
4. **Costly onset subcomponent?** FFT frontend is the largest subcomponent, followed by decision/flux scan.
5. **FFT onset production-critical or debug-only?** Not decided. Current evidence says do not expand semantics until the chunk/hop instrumentation and heap gates are fixed.
6. **ControlBus/snapshot copy acceptable?** Publish copy p99 stayed under 300 us; snapshot read p99 stayed under 900 us. Not the primary blocker.
7. **Heap/stack stable?** Stack looked acceptable in this run; heap is not acceptable because free internal heap is below the shed threshold and the old shedding controller was force-clearing under genuine pressure.
8. **AP/client telemetry safe?** Not proven. AP idle/client-connected Gate A was not completed, and Gate B telemetry stress was not run.
9. **Cleared for Tier 1 HF semantic implementation?** No by evidence. Proceeded only by explicit Captain waiver on 2026-04-28.

## Next Required Work

1. Fix or re-scope the chunk/hop deadline instrumentation so it separates blocking DMA/cadence time from DSP work time, then rerun timing gates.
2. Reflash only when authorised, then verify that the PSRAM trace-buffer fix and hysteresis-controller fix remove the below-threshold relatch loop.
3. Rerun AP Gate A with an actual connected client using the approved benchmark corpus.
4. Only after those pass, run AP Gate B telemetry/debug stress.
