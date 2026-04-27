# Surface 1 — Render Path Budget

## Contract
`render_frame` p99 = 1909-1957 µs (95–98% of 2000 µs ceiling) at 120 FPS. This invariant must hold across all effect families. Frame time distribution and deadline-miss frequency are the key observables.

## Hypothesis
**Architectural question:** Where does the 2 ms budget go across the render pipeline? Is the bottleneck in effect rendering (22–47% observed), LED transmission (`show_leds`), audio snapshot staleness, or color correction pipeline? What is the p50/p99 distribution, and how often do frames exceed the deadline?

## Existing instrumentation (verified in current source)

| Macro | Name | Type | File:line | Notes |
|---|---|---|---|---|
| TRACE_SCOPE | render_frame | span | RendererActor.cpp:1317 | Top-level frame wrapper; includes everything up to pre_show_yield |
| TRACE_SCOPE | audio_snapshot_read | span | RendererActor.cpp:1387 | Audio ControlBusFrame copy + extrapolation math |
| TRACE_SCOPE | effect_render | span | RendererActor.cpp:1771 | Calls safeReg->effect->render(ctx); effect-family dependent |
| TRACE_SCOPE | color_correction | span | RendererActor.cpp:885 | Post-effect colour correction pipeline (skipped for sensitive effects) |
| TRACE_SCOPE | show_leds | span | RendererActor.cpp:901 | FastLED RMT4 show(); returns quickly, RMT ISR runs in parallel |
| TRACE_SCOPE | pre_show_yield | span | RendererActor.cpp:983 | vTaskDelay(0 or 1) cooperative yield before frame end |
| TRACE_COUNTER | frame_us | counter | RendererActor.cpp:955 | Duration from frameStartUs to frameEndUs (AFTER pacing + yield) |
| TRACE_COUNTER | fps | counter | RendererActor.cpp:1921 | m_stats.currentFPS (rolling average, not per-frame) |

**Gaps identified:**

1. **No per-frame duration counter (raw, before pacing).** `frame_us` is measured AFTER the pacing sleep and yield, so it always appears as ~8.33 ms. We need the RAW duration (before pacing) to see actual frame work time.
2. **No deadline-miss instant.** Missing a TRACE_INSTANT fired when render_frame work exceeds 2000 µs (before pacing).
3. **No audio snapshot age counter.** `audio_snapshot_read` span exists but we don't record how stale the snapshot is (dt_us in the code).
4. **No effect ID counter.** Effect ID should be sampled per-frame so trace analysis can segment by effect family.
5. **No color_correction duration counter.** The span exists but post-trace analysis needs histogram of CC times.
6. **No zone composition visibility.** Zone compose spans exist (line 1608) but not visible at frame level.

## Required additions

| # | Name | Type | Tier | File:line | Args | Cost (events/s) | Hypothesis | Sanity check |
|---|---|---|---|---|---|---|---|---|
| 1 | render_frame_work_us | TRACE_COUNTER | 1 | RendererActor.cpp:955 (replace existing counter) | `(int)(rawFrameTimeUs)` | 120 | Raw work time before pacing; answers "how much of 8.33 ms did effect+correction take?" | p50 1400–1700 µs, p99 < 2000 except documented misses |
| 2 | render_frame_deadline_miss | TRACE_INSTANT | 1 | RendererActor.cpp:956 (conditional) | None (instant only) | 1–10 (on miss) | Marks frames that exceed 2 ms budget; enables post-trace filtering for deadline analysis | Should fire 0–5 times per 30-second capture for healthy effects |
| 3 | audio_snapshot_age_us | TRACE_COUNTER | 1 | RendererActor.cpp:1404 | `(int)(dt_us)` | 120 | Staleness of audio snapshot when rendered; answers "is audio sync lagging?" | p50 < 1000 µs, p99 < 5000 µs |
| 4 | effect_id_active | TRACE_COUNTER | 1 | RendererActor.cpp:970 | `(int)(m_currentEffect)` | 120 | Which effect was active when frame rendered; enables post-hoc regime segmentation | Should match currently-displayed effect in traces |
| 5 | color_correction_duration_us | TRACE_COUNTER | 2 | RendererActor.cpp:893 (end of CC block) | `(int)(colorCorrectionEndUs - colorCorrectionStartUs)` | ~40 (only when CC runs) | Histogram of CC times; identifies if CC is ever hot-path bottleneck | p50 10–50 µs, p99 < 200 µs |

## Code blocks to insert

### Addition 1 — render_frame_work_us counter (Tier 1)

**Location:** RendererActor.cpp, lines 903–916 (raw frame time calculation). Replace existing `frame_us` counter call.

```cpp
    // Calculate frame time (pre-throttle)
    uint32_t frameEndUs = micros();
    uint32_t rawFrameTimeUs = frameEndUs - frameStartUs;

    // Handle micros() overflow (unlikely but possible)
    if (frameEndUs < frameStartUs) {
        rawFrameTimeUs = (UINT32_MAX - frameStartUs) + frameEndUs;
    }

    // INSERT: Log raw work time BEFORE pacing/yield
    TRACE_COUNTER("render_frame_work_us", (int)rawFrameTimeUs);

    uint32_t frameTimeUs = frameEndUs - frameStartUs;
    if (frameEndUs < frameStartUs) {
        frameTimeUs = (UINT32_MAX - frameStartUs) + frameEndUs;
    }
```

**Rationale:** This is the critical measurement—the actual CPU time spent in render work. The existing `frame_us` (now line 955) is measured after pacing and yield, so it always reads ~8.33 ms. Moving the counter to line 915 captures the RAW work duration before any throttling.

---

### Addition 2 — render_frame_deadline_miss instant (Tier 1)

**Location:** RendererActor.cpp, lines 915–920 (immediately after raw frame time recorded).

```cpp
    // INSERT: Fire deadline-miss instant if raw work exceeded 2 ms
    if (rawFrameTimeUs > 2000) {
        TRACE_INSTANT("render_frame_deadline_miss");
    }

    uint32_t frameTimeUs = frameEndUs - frameStartUs;
```

**Rationale:** Provides a searchable marker in the trace for frames that missed the hard deadline. Post-hoc analysis can filter by effect ID or audio state to correlate miss causes.

**Cost estimate:** ~1–10 events per 30-second capture (depending on effect complexity); ~16 bytes per event when fired.

---

### Addition 3 — audio_snapshot_age_us counter (Tier 1)

**Location:** RendererActor.cpp, lines 1400–1408 (after `dt_us` is calculated).

```cpp
        uint64_t dt_us = (now_us >= m_lastAudioMicros) ? (now_us - m_lastAudioMicros) : 0;
        uint64_t extrapolated_samples = m_lastAudioTime.sample_index +
            (dt_us * m_lastAudioTime.sample_rate_hz / 1000000);

        // INSERT: Log audio snapshot staleness
        TRACE_COUNTER("audio_snapshot_age_us", (int)(dt_us & 0x7FFFFFFF));

        audio::AudioTime render_now(
            extrapolated_samples,
            m_lastAudioTime.sample_rate_hz,
```

**Rationale:** Answers "how old is the audio data the renderer just read?" Staleness > 5 ms indicates the audio pipeline is lagging or effects are waiting on snapshot updates.

**Cost estimate:** 120 events/sec × 24 bytes = 2880 bytes/sec.

---

### Addition 4 — effect_id_active counter (Tier 1)

**Location:** RendererActor.cpp, lines 965–971 (end of frame, after all work is complete).

```cpp
        Message evt(MessageType::FRAME_RENDERED);
        evt.param1 = static_cast<uint8_t>(m_currentEffect & 0xFF);
        evt.param2 = static_cast<uint8_t>((m_currentEffect >> 8) & 0xFF);
        evt.param3 = static_cast<uint8_t>((m_stats.currentFPS > 255U) ? 255U : m_stats.currentFPS);
        evt.param4 = m_frameCount;
        bus::MessageBus::instance().publish(evt);
    }

    // INSERT: Log active effect ID every frame (cheaper than message bus)
    TRACE_COUNTER("effect_id_active", (int)(m_currentEffect));

    m_lastFrameTime = frameStartUs;
```

**Rationale:** Perfetto post-processing can segment timeline by effect, correlating work distribution (render_frame_work_us, color_correction_duration_us) to effect family. This counter is sampled every frame at near-zero cost.

**Cost estimate:** 120 events/sec × 24 bytes = 2880 bytes/sec.

---

### Addition 5 — color_correction_duration_us counter (Tier 2, opt-in)

**Location:** RendererActor.cpp, lines 881–893 (color correction block). Add timer before and counter after the block.

```cpp
    // Post-render color correction pipeline (skip for sensitive effects)
    // Includes: LGP-sensitive, stateful, PHYSICS_BASED, MATHEMATICAL families
    // See PatternRegistry::shouldSkipColorCorrection() for full list
    {
        // INSERT: Start timer (only when FEATURE_TRACE_PERFORMANCE enabled)
#if FEATURE_TRACE_PERFORMANCE
        uint64_t _cc_start_us = esp_timer_get_time();
#endif
        TRACE_SCOPE("color_correction");
        const EffectId safeEffectTick = m_currentEffectValid ? m_validatedEffectId : validateEffectId(m_currentEffect);
        if (!::PatternRegistry::shouldSkipColorCorrection(safeEffectTick)) {
            enhancement::ColorCorrectionEngine::getInstance().processBuffer(m_leds, LedConfig::TOTAL_LEDS);
            m_correctionApplyCount++;
        } else {
            m_correctionSkipCount++;
        }
        // INSERT: Log duration when CC actually ran
#if FEATURE_TRACE_PERFORMANCE
        uint64_t _cc_end_us = esp_timer_get_time();
        TRACE_COUNTER("color_correction_us", (int)(_cc_end_us - _cc_start_us));
#endif
    }
```

**Rationale:** Tier 2 because the span already exists; this counter is for convenience histograms. Only compile in when `FEATURE_TRACE_PERFORMANCE=1` to avoid bloat in always-on builds.

**Cost estimate (when enabled):** ~40–80 events/sec (only when CC runs) × 24 bytes = ~960–1920 bytes/sec.

---

## Tier rationale

- **Tier 1 (Additions 1–4):** Critical for understanding render budget distribution. `render_frame_work_us` is the invariant measurement; `audio_snapshot_age_us` and `effect_id_active` enable post-hoc analysis; `deadline_miss` instant is a searchable marker. Combined cost: ~120×(24+24+24) = ~8640 bytes/sec, well within 64 KB / 500 ms = 131 KB/sec budget.

- **Tier 2 (Addition 5):** `color_correction_us` is a convenience for histograms; the TRACE_SCOPE already exists. Only compile when explicitly investigating colour correction bottlenecks. Guarded by `FEATURE_TRACE_PERFORMANCE` to keep always-on builds lean.

## Acceptance criteria (for the implementing agent)

1. **Build variant:** `esp32dev_audio_esv11_k1v2_32khz_trace` with all additions compiled in.
2. **Capture:** 30-second trace of effect 0x2100 (or any stable effect) at 120 FPS.
3. **Verification:**
   - JSON trace file contains ≥3600 samples of `render_frame_work_us` (120 FPS × 30 sec).
   - `render_frame_deadline_miss` instants appear 0–5 times (healthy effects should not miss).
   - `audio_snapshot_age_us` samples show p50 < 1000 µs, p99 < 5000 µs.
   - `effect_id_active` counter matches the active effect throughout the capture.
   - Run `analyse_trace.py` (Surface 9) to generate histograms for `render_frame_work_us` and confirm p50 ∈ [1400, 1700] µs, p99 < 2000 µs except documented deadline misses.
4. **Sanity check:** At 120 FPS, a 30-second capture produces ~3600 frames. Expected event count in JSON:
   - `render_frame_work_us`: 3600
   - `audio_snapshot_age_us`: 3600
   - `effect_id_active`: 3600
   - `render_frame_deadline_miss`: 0–5 (0 is ideal)
   - `color_correction_us` (Tier 2): ~2160–3600 (only when CC runs)

## Open questions

1. **FEATURE_TRACE_PERFORMANCE flag:** Should this be defined in `features.h` or is a simpler `#if FEATURE_MABUTRACE` guard preferred for Addition 5?
2. **Overflow handling for dt_us:** In Addition 3, should we mask to 31 bits or allow full 32-bit range? Current code uses `uint64_t dt_us` but counter is `int` (signed 32-bit).
3. **Effect ID encoding:** Is `(int)(m_currentEffect)` safe, or should we cast to `uint16_t` to preserve full ID space? Current code stores as EffectId (likely 16-bit).
