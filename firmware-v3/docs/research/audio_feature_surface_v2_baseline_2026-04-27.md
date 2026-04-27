# Audio Feature Surface v2 Baseline Measurement Report

**Date:** 2026-04-27
**Status:** Foundation baseline; hardware p99/p999 capture still required
**Scope:** Phases 0-3 only. No Tier 1 semantic fields, no `96`, no `128`.

## 1. Purpose

This report locks the measurement requirements that must be satisfied before
new production audio feature fields are added. It also records the current
static facts that constrain the Audio Feature Surface v2 work.

## 2. Current Static Baseline

| Item | Current finding | Source |
|------|-----------------|--------|
| Production K1v2 audio rate | 32 kHz | `firmware-v3/src/config/audio_config.h` |
| Audio hop | 256 samples, 8 ms at 32 kHz | `firmware-v3/src/config/audio_config.h` |
| ESV11 chunk | 128 samples, 4 ms at 32 kHz | `firmware-v3/src/config/audio_config.h` |
| Publish cadence | 2 ESV11 chunks per hop, 125 Hz | `firmware-v3/src/audio/AudioActor.cpp` |
| Raw `bins256` presence | `ControlBusFrame` physically carries `bins256[256]` | `firmware-v3/src/audio/contracts/ControlBus.h` |
| ESV11 `bins256` population | ESV11 path builds a 512-point FFT view and copies it into `frame.bins256` for STM | `firmware-v3/src/audio/AudioActor.cpp` |
| Frame-size guard | `ControlBusFrame` and `ControlBusRawInput` must each remain <= 5120 bytes | `firmware-v3/src/audio/contracts/ControlBus.h` |
| Existing effect-facing raw access | `EffectContext` exposes `bins256()`, `binHz()`, and range helpers when PipelineCore is enabled | `firmware-v3/src/plugins/api/EffectContext.h` |

## 3. Required Measurements Before New Fields

Phase 1 is not complete until a hardware-backed report captures:

| Measurement | Required output |
|-------------|-----------------|
| Audio chunk timing | `audio_chunk_work_us` p50, p95, p99, p999, max, `audio_chunk_deadline_miss_total` |
| Publish-hop timing | `audio_hop_us` p50, p95, p99, p999, max, `audio_hop_deadline_miss_total` |
| Render timing | p50, p95, p99, p999, max |
| Projection timing | p50, p95, p99, p999 once projection exists |
| Snapshot read/copy | `audio_snapshot_copy_us`, `audio_snapshot_age_us`, `audio_snapshot_hop_seq_lag`, `snapshot_read_retries_total` |
| ControlBus copy cost | `controlbus_publish_copy_us`, `audio_snapshot_copy_us`, and number of copies per frame |
| Frame size | `audio_snapshot_size_bytes`, bytes remaining before 5120-byte guard |
| Heap | min free heap and largest free block, sampled from 1 Hz/background path only |
| Stack | high-water marks for audio, render, network/show, and loop tasks, sampled from 1 Hz/background path only |
| AP/WebSocket | connected-client impact and queue/drop state |
| LED output | `showSkips`, `avgShowUs`, `maxShowUs`, LED failures, RMT errors/underruns if exposed |
| Stability | burn-in duration, panics, watchdog resets |
| Build size | RAM, flash, and environment name |
| Trace mode | trace build flag and instrumentation mode recorded in every report |

## 4. Hard Gates

No production semantic fields may be added until the following gates are
measured on target hardware:

| Gate | Pass condition |
|------|----------------|
| Audio chunk | p99 <= 3200 us, p999 below the 4 ms chunk deadline, zero chunk deadline misses |
| Publish hop | p99 <= 6400 us, p999 below the 8 ms hop deadline, zero hop deadline misses |
| Render | effect render path remains <= 2.0 ms |
| LED output | `showSkips=0`, no LED/RMT failures; if ESP32-S3 exposes no RMT error source, report that explicitly |
| Memory | stable heap and stack, no progressive collapse |
| Frame size | any size increase justified against measured copy and memory headroom |
| Burn-in | minimum 30 minutes for Phase 1B quick gate; minimum 2 hours before semantic implementation authorisation |
| Trace overhead | no serial/log spam, heap checks, or stack checks in audio/render hot paths |

## 5. Measurement Procedure

Use the trace-enabled build matching the production K1v2 environment:

```bash
cd firmware-v3
pio run -e esp32dev_audio_esv11_k1v2_32khz_trace
pio run -e esp32dev_audio_esv11_k1v2_32khz_trace -t upload --upload-port /dev/tty.usbmodem<port>
```

Capture a trace:

```bash
~/.platformio/penv/bin/python3 firmware-v3/tools/capture_trace.py \
    --port /dev/tty.usbmodem<port> \
    --effect 0x2102 \
    --soak 10 \
    --output /tmp/k1_trace_audio_feature_surface_v2.json
```

Minimum capture matrix:

| Scenario | Purpose |
|----------|---------|
| Silence | floor, false trigger, idle CPU/memory |
| Pink/white noise | high-frequency false-positive stress |
| Closed/open hi-hat pattern | positive transient HF target |
| Cymbal wash / ride / crash | sustained HF texture target |
| Jazz drums | mixed hat/snare/cymbal positive fixture |
| Dense compressed music | worst-case musical density |
| Bass-heavy music | bass dominance and low-band stability |
| Spoken S/T/SH sibilance | critical false-positive fixture for future `hatEvent` |
| Vocal-heavy sibilant music | musical false-positive fixture for future HF separation |
| AP client connected, telemetry disabled | production network baseline |
| AP client connected, debug telemetry enabled | debug-mode stress |
| Heavy visual effect | render and RMT headroom |

## 6. HF Fixture Expectations

These expectations are for contracts and future validation only. They do not
authorise new production semantic fields in Phase 1B.

| Future field | Fixture expectation |
|--------------|---------------------|
| `hfEnergy` | May rise on bright material, sibilance, noise, and dense mixes without implying hats. |
| `hfFlux` | Tracks fast HF change but is not a trigger by itself. |
| `hatEvent` | Does not continuously fire on cymbal wash, sibilance, noise, clipped music, compressed bright music, or dense bass. |
| `cymbalSustain` | Holds and decays smoothly on cymbal wash. |
| `airEnergy` | Behaves as smooth upper shimmer, not a transient trigger. |

## 7. Definition Of Done

Phase 1 is complete only when this report is updated with:

- measured p50/p95/p99/p999/max values;
- explicit pass/fail against each hard gate;
- trace file path or capture identifier;
- hardware identity and build environment;
- burn-in duration;
- current `ControlBusFrame` size and copy count;
- copy cost for publish and render-side snapshot read;
- trace mode and instrumentation flags;
- open risks and next authorised phase.

Until then, this document is a baseline plan plus static source audit, not a
hardware performance proof.

## 8. Capture 1 -- Music, Current Active Effect

**Trace:** `/tmp/k1_trace_afs_v2_music_current.json`
**Capture status:** valid MabuTrace JSON, 5,471 events, 518.8 KB
**Trace window:** ~1.77 s from the 64 KB trace ring after a 10 s soak
**Audio condition:** music already playing near K1v2
**Effect:** current active effect, not changed by capture command

| Span / counter | n | p50 us | p95 us | p99 us | p999 us | max us |
|----------------|--:|-------:|-------:|-------:|--------:|-------:|
| `effect_render` | 199 | 423.0 | 470.0 | 492.1 | 502.2 | 504.0 |
| `render_frame` | 199 | 1515.0 | 1889.7 | 1942.2 | 1955.4 | 1956.0 |
| `audio_snapshot_read` | 199 | 540.0 | 746.6 | 841.0 | 851.8 | 854.0 |
| `snapshot_publish` | 77 | 97.0 | 208.4 | 271.2 | 294.4 | 297.0 |
| `onset_detect` | 153 | 2323.0 | 3122.4 | 3199.5 | 3286.1 | 3297.0 |
| `onset_process_us` | 77 | 1824.0 | 2376.2 | 2416.5 | 2417.8 | 2418.0 |
| `i2s_dma_read` | 153 | 8401.0 | 9206.4 | 9486.7 | 9630.0 | 9652.0 |
| `frame_us` | 199 | 8380.0 | 8394.0 | 8409.2 | 8592.1 | 8598.0 |
| `show_leds` | 200 | 819.0 | 937.1 | 952.0 | 966.2 | 969.0 |
| `fastled_rmt_show` | 200 | 564.0 | 647.0 | 674.0 | 679.0 | 680.0 |

Instant event counts:

| Event | Count |
|-------|------:|
| `ONSET_HIHAT` | 10 |
| `BR_SNARE` | 7 |
| `ONSET_SNARE` | 6 |
| `BR_HIHAT` | 6 |
| `ONSET_KICK` | 6 |
| `BR_KICK` | 4 |

Preliminary read:

- `effect_render` is comfortably under the 2.0 ms effect budget in this trace.
- `onset_detect` p99 is just inside the 3.2 ms target, while p999/max exceed it.
- `i2s_dma_read` includes blocking DMA wait time and must not be treated as pure DSP compute without deeper trace separation.
- `show_leds` / `fastled_rmt_show` spans are CPU-return timings, not proof of the full WS2812 wire-time safety invariant.
- `showSkips`, RMT error count, heap, largest free block, and stack high-water marks were not captured in this trace.

## 9. Capture 2 -- Music, Effect `0x2100`

**Trace:** `/tmp/k1_trace_afs_v2_music_0x2100.json`
**Capture status:** valid MabuTrace JSON, 5,468 events, 518.1 KB
**Trace window:** ~1.77 s from the 64 KB trace ring after a 10 s soak
**Audio condition:** music already playing near K1v2
**Effect command:** `effect 0x2100`

| Span / counter | n | p50 us | p95 us | p99 us | p999 us | max us |
|----------------|--:|-------:|-------:|-------:|--------:|-------:|
| `effect_render` | 200 | 412.0 | 476.0 | 486.1 | 500.2 | 502.0 |
| `render_frame` | 200 | 1516.0 | 1889.4 | 1943.1 | 1984.8 | 1993.0 |
| `audio_snapshot_read` | 199 | 532.0 | 712.2 | 833.2 | 846.6 | 847.0 |
| `snapshot_publish` | 77 | 103.0 | 203.6 | 243.7 | 262.9 | 265.0 |
| `onset_detect` | 154 | 2280.5 | 3060.1 | 3211.9 | 3226.4 | 3227.0 |
| `onset_process_us` | 77 | 1832.0 | 2354.4 | 2398.8 | 2461.1 | 2468.0 |
| `i2s_dma_read` | 154 | 8374.0 | 9297.0 | 9450.4 | 9582.6 | 9595.0 |
| `frame_us` | 199 | 8380.0 | 8392.0 | 8398.0 | 8405.4 | 8407.0 |
| `show_leds` | 200 | 820.0 | 931.6 | 955.1 | 970.2 | 971.0 |
| `fastled_rmt_show` | 200 | 562.0 | 654.0 | 675.0 | 678.2 | 679.0 |

Instant event counts:

| Event | Count |
|-------|------:|
| `ONSET_HIHAT` | 7 |
| `ONSET_SNARE` | 6 |
| `ONSET_KICK` | 3 |
| `BR_HIHAT` | 2 |

Preliminary read:

- `effect_render` remains comfortably under the 2.0 ms effect budget.
- `render_frame` p999 remains under 2.0 ms in this short trace.
- `onset_detect` p99/p999 exceed the provisional 3.2 ms p99 target by a small margin in this trace.
- The trace proves MabuTrace capture is working on K1v2 over `/dev/tty.usbmodem2101`; it does not complete the Phase 1 gate because burn-in, heap/stack, show-skip, and RMT error telemetry are still missing.

## 10. Capture 3 -- Asset Playback, Dense/Bass

**Audio asset:** `/Users/spectrasynq/Desktop/Anchor_Point.mp3`
**Trace:** `/tmp/k1_trace_afs_v2_anchor_dense_bass_0x2100.json`
**Capture status:** valid MabuTrace JSON, 5,471 events, 523.9 KB
**Trace window:** ~1.76 s from the 64 KB trace ring after local asset playback warmup + 10 s soak
**Effect command:** `effect 0x2100`

| Span / counter | n | p50 us | p95 us | p99 us | p999 us | max us |
|----------------|--:|-------:|-------:|-------:|--------:|-------:|
| `effect_render` | 199 | 420.0 | 472.0 | 481.2 | 496.2 | 498.0 |
| `render_frame` | 199 | 1512.0 | 1900.3 | 1967.1 | 1975.2 | 1976.0 |
| `audio_snapshot_read` | 199 | 530.0 | 711.2 | 843.1 | 847.6 | 848.0 |
| `snapshot_publish` | 77 | 104.0 | 196.8 | 207.2 | 207.9 | 208.0 |
| `onset_detect` | 153 | 2333.0 | 3147.2 | 3322.4 | 3389.9 | 3399.0 |
| `onset_process_us` | 77 | 1936.0 | 2346.4 | 2420.9 | 2529.0 | 2541.0 |
| `i2s_dma_read` | 153 | 8375.0 | 9149.0 | 9628.4 | 9647.0 | 9650.0 |
| `frame_us` | 199 | 8379.0 | 8392.1 | 8401.2 | 8539.7 | 8571.0 |
| `show_leds` | 199 | 818.0 | 924.6 | 958.0 | 963.0 | 964.0 |
| `fastled_rmt_show` | 199 | 559.0 | 654.4 | 680.0 | 686.6 | 688.0 |

Audio/event counters:

| Counter / event | Value |
|-----------------|------:|
| `audio_rms` p50 / p95 / max | 6250.0 / 9547.0 / 10000.0 |
| `br_kick_energy` p50 / p95 / max | 249.0 / 692.2 / 833.0 |
| `br_snare_energy` p50 / p95 / max | 300.0 / 1039.0 / 1123.0 |
| `br_hihat_energy` p50 / p95 / max | 589.0 / 1909.2 / 2041.0 |
| `onset_high_flux` p50 / p95 / max | 0.0 / 609.8 / 923.0 |
| `spectral_novelty` p50 / p95 / max | 283.0 / 847.0 / 935.0 |
| `BR_KICK` / `BR_SNARE` / `BR_HIHAT` | 7 / 6 / 8 |
| `ONSET_KICK` / `ONSET_SNARE` / `ONSET_HIHAT` | 6 / 7 / 5 |

Preliminary read:

- Effect/render timing remains stable and under the 2.0 ms effect budget.
- This dense/bass asset still drives strong high-frequency counters, so future HF semantics need false-positive separation rather than simply treating high-band energy as hats.
- `onset_detect` p99/p999 exceed the provisional 3.2 ms target in this capture.

## 11. Capture 4 -- Asset Playback, Cymbal/Hat

**Audio asset:** `/Users/spectrasynq/Downloads/Jazz Drums Loop - 160 BPM - Kiro tv.mp3`
**Trace:** `/tmp/k1_trace_afs_v2_jazz_drums_hat_cymbal_0x2100.json`
**Capture status:** valid MabuTrace JSON, 5,471 events, 523.4 KB
**Trace window:** ~1.77 s from the 64 KB trace ring after local asset playback warmup + 10 s soak
**Effect command:** `effect 0x2100`

| Span / counter | n | p50 us | p95 us | p99 us | p999 us | max us |
|----------------|--:|-------:|-------:|-------:|--------:|-------:|
| `effect_render` | 200 | 424.5 | 473.1 | 489.0 | 491.8 | 492.0 |
| `render_frame` | 200 | 1510.0 | 1930.3 | 1968.1 | 1981.4 | 1983.0 |
| `audio_snapshot_read` | 199 | 535.0 | 809.8 | 843.5 | 878.0 | 881.0 |
| `snapshot_publish` | 77 | 105.0 | 198.2 | 213.0 | 230.1 | 232.0 |
| `onset_detect` | 153 | 2309.0 | 3119.8 | 3259.9 | 3278.5 | 3279.0 |
| `onset_process_us` | 76 | 1801.5 | 2361.8 | 2388.5 | 2395.2 | 2396.0 |
| `i2s_dma_read` | 152 | 8380.0 | 9344.4 | 9528.9 | 9740.3 | 9777.0 |
| `frame_us` | 199 | 8379.0 | 8390.0 | 8395.0 | 8399.0 | 8400.0 |
| `show_leds` | 200 | 822.5 | 943.0 | 953.0 | 958.6 | 960.0 |
| `fastled_rmt_show` | 200 | 564.5 | 654.0 | 680.1 | 691.2 | 692.0 |

Audio/event counters:

| Counter / event | Value |
|-----------------|------:|
| `audio_rms` p50 / p95 / max | 0.0 / 8569.4 / 9196.0 |
| `br_kick_energy` p50 / p95 / max | 286.0 / 1082.4 / 1213.0 |
| `br_snare_energy` p50 / p95 / max | 293.0 / 1297.4 / 1426.0 |
| `br_hihat_energy` p50 / p95 / max | 816.0 / 2003.8 / 2216.0 |
| `onset_high_flux` p50 / p95 / max | 4.0 / 533.2 / 696.0 |
| `spectral_novelty` p50 / p95 / max | 294.0 / 831.0 / 1114.0 |
| `BR_KICK` / `BR_SNARE` / `BR_HIHAT` | 6 / 4 / 5 |
| `ONSET_KICK` / `ONSET_SNARE` / `ONSET_HIHAT` | 5 / 5 / 9 |

Preliminary read:

- The drum loop produces more `ONSET_HIHAT` events than the dense/bass asset, which is directionally correct.
- `br_hihat_energy` is high in both asset captures, so energy alone is not enough for hat/cymbal semantics.
- The future Tier 1 split should distinguish `hfEnergy`, `hfFlux`, `hatEvent`, and `cymbalSustain` rather than collapsing them into one high-frequency scalar.

## 12. Deferred Scenario

AP-client-connected stress testing is intentionally deferred until further
notice. Do not treat Phase 1 as complete without eventually restoring an AP
load scenario or explicitly changing the gate.
