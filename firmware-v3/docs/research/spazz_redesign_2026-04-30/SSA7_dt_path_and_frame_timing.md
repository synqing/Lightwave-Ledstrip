---
abstract: "SSA7 — traces the dt path from RendererActor through ZoneComposer into effect render(). Documents ctx.deltaTimeSeconds (SPEED-scaled), ctx.rawDeltaTimeSeconds (wall-clock between frame starts), the speedFactor curve, the [0.0001, 0.05] s clamp, and AudioReactivePolicy::signalDt vs visualDt. Concludes that dt is reasonably stable on K1v1 (post-pacing wall-clock between frame starts, not work time), but ZERO instrumentation has captured an actual dt distribution under audio load. Recommends phase advance use signalDt (raw); Spring physics MUST use signalDt to remain audio-reactive."
---

# SSA7 — dt Path and Frame Timing

**Date:** 2026-04-30
**Author:** agent:embedded-systems-engineer (Opus 4.7 1M)
**Scope:** READ-ONLY trace of dt origin → EffectContext → effect `render()`
**Hardware target:** K1v1 (ESP32-S3, dual 160-LED strip, ESV11 32 kHz audio)

---

## dt field definitions (verbatim from EffectContext.h)

From `firmware-v3/src/plugins/api/EffectContext.h:896-902`:

```cpp
//--------------------------------------------------------------------------
// Timing
//--------------------------------------------------------------------------

uint32_t deltaTimeMs;       ///< Time since last frame (ms)
float deltaTimeSeconds;     ///< Time since last frame (seconds, high precision)
uint32_t rawDeltaTimeMs;    ///< Unscaled time since last frame (ms)
float rawDeltaTimeSeconds;  ///< Unscaled time since last frame (seconds)
uint32_t frameNumber;       ///< Frame counter (wraps at 2^32)
uint32_t totalTimeMs;       ///< Total effect runtime (ms)
uint32_t rawTotalTimeMs;    ///< Unscaled total effect runtime (ms)
```

Default constructor values (`EffectContext.h:1139-1145`): `deltaTimeMs=8`, `deltaTimeSeconds=0.008f`, `rawDeltaTimeMs=8`, `rawDeltaTimeSeconds=0.008f`. Implies design intent of 125 FPS (8 ms / frame), close to the 120 FPS target (8.33 ms / frame).

Two safe accessors at `EffectContext.h:1101-1119`:

```cpp
float getSafeDeltaSeconds() const {        // SPEED-scaled
    float dt = deltaTimeSeconds;
    if (dt < 0.0001f) dt = 0.0001f;   // Minimum 0.1ms
    if (dt > 0.05f)   dt = 0.05f;     // Maximum 50ms (20 FPS floor)
    return dt;
}
float getSafeRawDeltaSeconds() const {     // Unscaled (wall-clock)
    float dt = rawDeltaTimeSeconds;
    if (dt < 0.0001f) dt = 0.0001f;
    if (dt > 0.05f)   dt = 0.05f;
    return dt;
}
```

---

## rawDt origin (file:line trace from RendererActor)

`rawDeltaTimeSeconds` is wall-clock between the start-of-frame timestamps of two consecutive `onTick()` invocations.

**Step 1 — Record current frame start (`RendererActor.cpp:857`):**
```cpp
void RendererActor::onTick() {
    uint32_t frameStartUs = micros();
```

**Step 2 — Compute delta in `renderFrame()` (`RendererActor.cpp:1655-1662`):**
```cpp
uint32_t now = micros();
uint32_t deltaTimeMs;
if (now >= m_lastFrameTime) {
    deltaTimeMs = (now - m_lastFrameTime) / 1000;   // ms resolution (loses µs!)
} else {
    deltaTimeMs = ((UINT32_MAX - m_lastFrameTime) + now) / 1000;
}
```

**CRITICAL precision loss:** `deltaTimeMs` is computed in milliseconds, not microseconds. Integer division by 1000 truncates. Example: a wall-clock interval of 8333 µs (perfect 120 FPS pacing) becomes `deltaTimeMs = 8`, which converts back to `0.008 s` — a 333 µs (4%) systematic underestimate every frame. At 120 FPS this leaks ~40 ms / sec of effect time. The integer-ms truncation also injects up to ±1 ms of quantisation jitter on `deltaTimeSeconds` even when wall-clock is perfect.

**Step 3 — Convert to seconds (`RendererActor.cpp:1835`):**
```cpp
float rawDeltaSeconds = static_cast<float>(deltaTimeMs) * 0.001f;
```

**Step 4 — Update `m_lastFrameTime` at end of tick (`RendererActor.cpp:1000`):**
```cpp
m_lastFrameTime = frameStartUs;
```

This is the **start-of-frame** timestamp, recorded BEFORE `renderFrame()` runs. Because the frame pacer (`RendererActor.cpp:936-961`) sleeps until `LedConfig::FRAME_TIME_US = 8333 µs` has elapsed since `frameStartUs`, the next frame's `frameStartUs` is approximately `previous_frameStartUs + 8333 µs` whenever work fits inside the budget.

**Therefore: `rawDeltaTimeSeconds` ≈ post-pacing inter-frame interval (≈ 8.33 ms at 120 FPS), NOT raw effect work time.** When work overruns 8.33 ms (~`render_frame_work_us > 8333`), pacing skips and the next frame's interval expands to whatever the actual work took (plus tiny WDT yield overhead). The `> 2000 µs` deadline-miss instant logged at `RendererActor.cpp:979` does NOT represent dt jitter — it just flags that the effect render exceeded the 2 ms render-contract ceiling. The frame pacer absorbs misses up to 8.33 ms.

---

## scaledDt formula (how speedFactor multiplies in)

Computed at `RendererActor.cpp:87-100`:

```cpp
constexpr float kMinSpeedTimeFactor = 0.04f;  // ~3-5 FPS feel at speed 1

float computeSpeedTimeFactor(uint8_t speed) {
    if (LedConfig::MAX_SPEED <= 1) return 1.0f;
    float norm = 0.0f;
    if (speed > 1) {
        norm = (static_cast<float>(speed - 1) /
                static_cast<float>(LedConfig::MAX_SPEED - 1));
        if (norm > 1.0f) norm = 1.0f;
    }
    float curved = sqrtf(norm);                                  // sqrt curve
    return kMinSpeedTimeFactor + (1.0f - kMinSpeedTimeFactor) * curved;
}
```

`LedConfig::MAX_SPEED = 100` (RendererActor.h:117), `DEFAULT_SPEED = 10` (RendererActor.h:116).

Sample table (`speed → speedFactor`):

| speed | norm  | sqrt(norm) | speedFactor |
|------:|------:|-----------:|------------:|
| 1     | 0.000 | 0.000      | 0.040       |
| 5     | 0.040 | 0.201      | 0.233       |
| 10    | 0.091 | 0.301      | 0.329 (default) |
| 25    | 0.242 | 0.492      | 0.512       |
| 50    | 0.495 | 0.703      | 0.717       |
| 100   | 1.000 | 1.000      | 1.000       |

Application at `RendererActor.cpp:1834-1853`:

```cpp
float speedFactor = computeSpeedTimeFactor(ctx.speed);
float rawDeltaSeconds = static_cast<float>(deltaTimeMs) * 0.001f;
float scaledDeltaSeconds = rawDeltaSeconds * speedFactor;

m_effectTimeSecondsRaw += rawDeltaSeconds;
ctx.rawDeltaTimeSeconds = rawDeltaSeconds;
ctx.rawDeltaTimeMs      = deltaTimeMs;
ctx.rawTotalTimeMs      = ...;

m_effectTimeSeconds       += scaledDeltaSeconds;
m_effectFrameAccumulator  += speedFactor;
// ...
ctx.deltaTimeSeconds = scaledDeltaSeconds;
ctx.deltaTimeMs      = static_cast<uint32_t>(scaledDeltaSeconds * 1000.0f + 0.5f);
ctx.frameNumber      = m_effectFrameCount;
ctx.totalTimeMs      = static_cast<uint32_t>(m_effectTimeSeconds * 1000.0f + 0.5f);
```

**At default speed=10, scaledDt ≈ 0.329 × rawDt ≈ 2.7 ms.** At speed=1, scaledDt ≈ 0.04 × rawDt ≈ 0.33 ms (clamped to 0.0001 s minimum is fine; clamp engages only if rawDt < 0.0025 s, which never happens at 120 FPS).

ZoneComposer applies an identical formula per zone using `zone.speed` (`ZoneComposer.cpp:36-48, 321-333`). Each zone gets its own `m_zoneTimeSecondsRaw[safeZone]` and `m_zoneTimeSeconds[safeZone]` accumulators (`ZoneComposer.cpp:318, 324`).

---

## getSafeDeltaSeconds clamp (range and rationale)

Range: **[0.0001 s, 0.05 s]** — i.e. [100 µs, 50 ms].

Two definitions exist:

1. **Member method** (`EffectContext.h:1101`) — clamps `deltaTimeSeconds` (the SPEED-scaled value).
2. **Free function** (`SmoothingEngine.h:304`) — same clamp, takes any `deltaSeconds` argument.

Documented rationale (`EffectContext.h:1088-1099`, `SmoothingEngine.h:299-309`):

> Prevents physics explosion on frame drops (>50 ms) and ensures a minimum timestep for stability (0.1 ms). Essential for true exponential smoothing formulas: `alpha = 1 - exp(-lambda * dt)`.

The 50 ms ceiling = "20 FPS floor" — if a frame stall exceeds 50 ms, the clamp prevents a single huge integration step blowing up Spring physics or causing visible jumps. The 0.1 ms floor protects against `expf(-lambda * 0)` returning 0 (no smoothing happens) when speed=1 and rawDt happens to round to 0 ms (it cannot at 120 FPS but the clamp is defensive).

**Critical:** the clamp is ONLY tripped on actual frame stalls (>50 ms). Under normal 120 FPS rendering, both `getSafeDeltaSeconds()` and `getSafeRawDeltaSeconds()` pass the value through unchanged.

---

## AudioReactivePolicy::signalDt (what it returns, when it differs from getSafeDeltaSeconds)

Defined at `AudioReactivePolicy.h:33-45`:

```cpp
/// Delta seconds for audio-coupled maths (unscaled by SPEED).
static inline float signalDt(const plugins::EffectContext& ctx) {
    return ctx.getSafeRawDeltaSeconds();
}

/// Delta seconds for visual-only motion (SPEED-scaled).
static inline float visualDt(const plugins::EffectContext& ctx) {
    return ctx.getSafeDeltaSeconds();
}
```

**Design intent (verbatim from header comment, `AudioReactivePolicy.h:1-7`):**

> This header centralises the "audio uses raw time" contract so effect implementations stay consistent and SPEED does not distort DSP-coupled maths.

Translation:

- `signalDt(ctx)` = `getSafeRawDeltaSeconds()` = wall-clock dt clamped to [0.0001, 0.05]. **NOT smoothed, NOT speed-scaled.** Use for any state that must remain time-aligned to the audio stream (RMS followers, beat-driven envelopes, onset decays). At default speed=10 on K1v1, this is ~8.33 ms per frame.
- `visualDt(ctx)` = `getSafeDeltaSeconds()` = SPEED-scaled dt. Use for purely cosmetic motion that the user expects to slow down with the SPEED knob. At default speed=10, this is ~2.7 ms per frame.

The two diverge **whenever speed ≠ 100**. At speed=100 they are identical (speedFactor=1.0). At default speed=10 the visual dt is ~3.0× smaller than the signal dt.

There is no smoothing inside `signalDt`. The clamp is the only modification. If the underlying frame loop is stable, `signalDt` is stable (subject to the integer-ms quantisation noted above).

---

## K1v1 actual dt distribution

**Not instrumented for dt distribution under audio load.**

The codebase logs RAW work time but never logs the actual interval between frames:

- `TRACE_COUNTER("render_frame_work_us", rawFrameTimeUs)` at `RendererActor.cpp:976` — RAW pre-pacing CPU work time. NOT the dt fed to effects.
- `TRACE_INSTANT("render_frame_deadline_miss")` at `RendererActor.cpp:980` — fires when work > 2 ms. Tells you the effect blew the budget; does not tell you what dt the next frame received.
- `m_stats.cpuPercent`, `m_stats.currentFPS` — derived running averages, not per-frame deltas.

`grep -rn "TRACE_COUNTER.*delta" src/` returns zero hits. There is no histogram, no sample stream, no log of actual `deltaTimeMs` distribution at runtime — neither under silence nor under audio playback.

**What we can infer from architecture:**

1. The frame pacer (`esp_timer_start_once` + `ulTaskNotifyTake`) targets exactly 8333 µs since `frameStartUs`, so when work fits, dt is locked to 8 ms (with ±1 ms integer-ms quantisation).
2. When an effect overruns, the next dt expands to ~`rawFrameTimeUs / 1000` rounded down. Effects flagged in `RendererActor.cpp:1009-1012` as routinely busting budget (Chimera Crown, Kuramoto Transport, Talbot Carpet) see dt jitter of unknown magnitude.
3. K1v1 vs K1v2: identical pacing logic; only difference relevant here is GPIO pinout (no impact on dt).
4. AudioActor lives on Core 0; RendererActor on Core 1. Audio cannot directly steal cycles from the renderer except via shared resources (heap mutex on `heap_caps_*` calls — see existing project memory `feedback_no_heap_scans_in_high_freq_paths.md`).

**Bottom line:** under healthy conditions dt should be 8 ms ± integer-ms quantisation. Under audio load specifically there is no recorded evidence of pathological dt jitter. Any claim that "dt is unstable on K1v1 under music" is currently uncalibrated and would need a Captain-approved capture run with a new TRACE_COUNTER on `deltaTimeMs` (or, better, on the µs-resolution delta before the /1000 truncation).

---

## Verdict

**Using `ctx.deltaTimeSeconds` (SPEED-scaled) for phase advance is the WRONG choice for the four broken effects** — but the failure mode is not "spazz" from dt instability. It is a different bug:

1. **At low SPEED settings, scaled dt becomes tiny** (0.04× at speed=1, 0.33× at speed=10). Phase advance per frame becomes:
   - `phase += freq * scaledDt`
   - At speed=10: phase advances at 33 % of audio-aligned rate. Beat-locked oscillators drift relative to the music.
   - At speed=1: phase advances at 4 %. Practically frozen.
2. **At high SPEED, scaled dt = raw dt.** Phase appears musical at speed=100 only.
3. **If the four effects also drive Spring physics with the same scaled dt**, the spring stiffness/damping tuning is implicitly speed-dependent — at low SPEED the spring takes ~25× longer to converge, which causes visible "lag" or "stutter" on beat impulses, and at high SPEED can overshoot/oscillate. **This is the most plausible mechanism for "spazz/jerk during music":** beat impulses arrive at audio rate, but the spring integrates with a dt that is inconsistent with the impulse timing.

The 8 ms integer-ms quantisation on dt is a real (4 % systematic underestimate of total time + ±1 ms per-frame jitter) but small effect that does not cause visible spazz. It does, however, slowly desync `ctx.totalTimeMs` from wall-clock — irrelevant to per-frame phase advance.

`AudioReactivePolicy::signalDt` was *deliberately introduced* as the contract for audio-coupled maths so effects do not have this exact bug. SnapwaveLinearEffect already uses it correctly (`SnapwaveLinearEffect.cpp:206`). The four broken effects almost certainly bypass it.

---

## Recommendation

| Concern | Use | Why |
|---|---|---|
| **Phase advance** for any audio-reactive oscillator (beat-locked, tempo-aligned, onset-triggered) | `AudioReactivePolicy::signalDt(ctx)` (= raw dt clamped) | Phase must track wall-clock so the visual stays musically aligned. SPEED knob is a *visual taste* knob, not a *time dilation* knob for music. |
| **Spring physics** integrating audio impulses (kick/snare-driven motion, beat-pulse springs) | `AudioReactivePolicy::signalDt(ctx)` | Spring stiffness/damping is tuned in real-world seconds. Feeding it scaled dt at speed=10 makes the spring 3× softer than designed; at speed=1 it is 25× softer. **This is the spazz mechanism.** Use raw dt so spring tuning matches the audio time base. |
| **Spring physics** integrating purely cosmetic targets (no audio coupling) | `AudioReactivePolicy::visualDt(ctx)` (= scaled dt) | User-controllable slowdown is the design intent. |
| **Trail fade / `fadeToBlackByDt`** | `ctx.getSafeDeltaSeconds()` (scaled) | Trails are a visual taste; SPEED should slow them. Note SnapwaveLinearEffect already does this at line 227. |
| **EMA followers on audio data** (`m_rmsFollower.update(rms, dt)`, etc.) | `AudioReactivePolicy::signalDt(ctx)` | The follower's `riseTau`/`fallTau` are tuned in real seconds. Scaled dt distorts the actual time constant by the speedFactor, which is exactly the bug the AudioReactivePolicy header warns about. SnapwaveLinearEffect line 220 follows this rule. |

**Conclusion for the four broken effects:** every audio-coupled state update — phase, spring integration, EMA follower update — must use `AudioReactivePolicy::signalDt(ctx)`. If the spazz reproduces only at low/mid SPEED settings and disappears at SPEED=100, the bug is confirmed as scaled-dt fed to a spring or phase that should be using signalDt. If the spazz persists at SPEED=100, dt scaling is not the cause and we need actual dt instrumentation before further work — that is a NEEDS-CAPTAIN trace capture, not a code change.

A next step (separate SSA) is to grep the four effects for `ctx.deltaTimeSeconds`, `ctx.getSafeDeltaSeconds`, or `visualDt` calls feeding `Spring::update` / phase accumulation, and replace with `signalDt` per the policy contract.

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-04-30 | agent:embedded-systems-engineer (Opus 4.7 1M) | Created — SSA7 dt path trace and frame-timing audit for spazz redesign. |
