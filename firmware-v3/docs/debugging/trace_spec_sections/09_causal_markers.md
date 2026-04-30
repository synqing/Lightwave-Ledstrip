# Section 9: Causal Markers Catalogue (Tier 4)

## Overview

Tier 4 introduces cheap TRACE_INSTANT events that label key narrative events on the Perfetto timeline without measuring anything. These are single-fire markers at known event boundaries, enabling Captain to correlate visually and the analyser to regime-segment.

**Total cost**: negligible (~1-50 events/min depending on activity, ~16 B per marker).

---

## Inventory of Existing TRACE_INSTANT Calls

**Total existing markers: 16 (excluding Trace.h example and blank no-op define)**

### Audio/Onset (4 markers)
- `ONSET_EVENT` — AudioActor.cpp:729 (generic onset)
- `ONSET_KICK` — AudioActor.cpp:730
- `ONSET_SNARE` — AudioActor.cpp:731
- `ONSET_HIHAT` — AudioActor.cpp:732

### Audio/Beat Recognition (3 markers)
- `BR_KICK` — AudioActor.cpp:806
- `BR_SNARE` — AudioActor.cpp:807
- `BR_HIHAT` — AudioActor.cpp:808

### Effect-Specific (6 markers)
- `bps_silence_gate` — BeatParitySpriteEffect.cpp:182
- `bps_beat_unchanged` — BeatParitySpriteEffect.cpp:192
- `bps_tempo_unlocked` — BeatParitySpriteEffect.cpp:199
- `bps_kick_fired` — BeatParitySpriteEffect.cpp:204
- `bps_even_parity_skip` — BeatParitySpriteEffect.cpp:212
- `bps_sprite_spawn` — BeatParitySpriteEffect.cpp:233

### PVF (Pitch Velocity Field) Effect (2 markers)
- `pvf_init_failed` — AttackOnlyPitchVelocityFieldEffect.cpp:197
- `pvf_field_zero` — AttackOnlyPitchVelocityFieldEffect.cpp:372

### System-wide (1 marker)
- `FALSE_TRIGGER` — AudioActor.cpp:3401 (debug: spurious trigger suppression)

### Renderer/Health (2 markers)
- `frame_drop` — RendererActor.cpp:1897
- `effect_change` — RendererActor.cpp:2022

---

## Marker Frequency Budget

| Category | Marker | Expected Rate | Events/Minute | Cost per Event |
|----------|--------|----------------|----------------|-----------------|
| Audio Onset | ONSET_* | ~0.5–2 Hz | 30–120 | 16 B |
| Beat Recognition | BR_* | ~0.5–2 Hz | 30–120 | 16 B |
| BPS Effect | bps_* | ~0.5–5 Hz | 30–300 | 16 B |
| PVF Effect | pvf_* | ~0.1–0.5 Hz | 6–30 | 16 B |
| System Health | frame_drop | <0.1 Hz | <6 | 16 B |
| System Narrative | effect_change | ~0.02 Hz | ~1 | 16 B |
| **Totals** | **16** | **Peak ~10 Hz** | **~500 events/min (peak)** | **~8 KB/min (peak)** |

**Typical sustained load**: ~50–100 events/min (~1–2 KB/min). **Annual cost at 500 events/min**: ~263 MB (negligible).

---

## Sibling-Surface Ownership & Deferred Markers

| Surface | Owns | Deferred to This Surface | Rationale |
|---------|------|--------------------------|-----------|
| **Surface 4** (WiFi) | `wifi_ap_started`, `wifi_client_connected`, `wifi_client_disconnected`, `ws_client_handshake`, `ota_started`, `ota_completed`, `ota_failed` | None | Network lifecycle is high-level narrative; owned by dedicated WiFi spec |
| **Surface 5** (ControlBus) | None | `boot_phase_audio_first_hop` | ControlBus first publish is an audio subsystem event; inserted in AudioActor boot path |
| **Surface 6** (Effect Lifecycle) | `effect_switch`, `effect_init_failed`, `palette_changed`, `parameter_set_<name>` | None | Effect state transitions are structural; owned by dedicated Effect spec |
| **Surface 7** (Performance) | `heap_pressure_*`, `thermal_warn_*`, `frame_deadline_missed` | Inherited from S7 (no change needed) | Health/performance metrics tracked separately; cited here for completeness |
| **This Surface (9)** | Narrative markers NOT owned by siblings | All Tier 4 additions below | Audio regime transitions, boot phases, user actions, configuration events |

---

## Net New Markers Introduced by This Surface (Tier 4)

**Principle**: Add only markers that:
1. Are NOT owned by Surfaces 4–8
2. Fire at **frequency << 1/sec** (once per minute or per session, not per frame)
3. Require **hysteresis/debouncing** to prevent flapping on timeline
4. Enable **regime segmentation** and **narrative reading**

### A. Boot / Shutdown Narrative (6 markers)

| Name | File:Line | Trigger Condition | Args | Expected Rate | Hypothesis |
|------|-----------|-------------------|------|----------------|------------|
| `boot_phase_idf_done` | core/SystemInit.cpp (new location) | After `initSerial()` completes | None | 1 per boot | ESP-IDF core ready; Serial logging online |
| `boot_phase_actors_started` | core/actors/ActorSystem.cpp (post-start) | All actors running, before loop | None | 1 per boot | Threading ready; message bus active |
| `boot_phase_audio_first_hop` | audio/AudioActor.cpp (post-ControlBus-init) | First `ControlBus::publish()` in AudioActor main loop | None | 1 per boot | Audio pipeline producing first data; effects can now react |
| `boot_phase_renderer_first_frame` | core/actors/RendererActor.cpp (post-init) | First frame rendered and copied to hardware | `num_zones` (int) | 1 per boot | LED output live; visual feedback ready |
| `boot_phase_wifi_up` | network/WebServer.cpp (on AP/STA started) | WiFi AP or STA connected | `mode` (string: "AP" or "STA") | 1 per boot | Remote control + OTA channel ready |
| `boot_phase_ready` | main.cpp (end of setup()) | All subsystems online | None | 1 per boot | System ready for production use |

### B. Audio Regime Transitions (6 markers + hysteresis rules)

| Name | File:Line | Trigger Condition | Debounce/Hysteresis | Expected Rate | Hypothesis |
|------|-----------|-------------------|----------------------|----------------|------------|
| `audio_silence_entered` | audio/AudioActor.cpp (~line 800+) | `silentScale` crosses **below 0.2** AND sustained for **≥5 frames** | Hysteresis: 0.05 (exit only if > 0.25) | ~0.1–0.3 Hz | Music stopped; fade to idle or static effect |
| `audio_silence_exited` | audio/AudioActor.cpp | `silentScale` rises **above 0.25** (from silence) | Hysteresis: 0.05 | ~0.1–0.3 Hz | Music resumed; activate responsive effects |
| `audio_loud_entered` | audio/AudioActor.cpp | `fast_rms > 0.7` **sustained for ≥1 second** (60 frames) | Debounce: avoid transient peaks | ~0.05–0.2 Hz | Loud passage detected; switch to high-energy mode |
| `tempo_lock_acquired` | audio/AudioActor.cpp (~TempoTracker integration) | `tempoConfidence` crosses **0.5 → up** | Hysteresis: 0.1 (exit if < 0.4) | ~0.01–0.1 Hz | Beat locked; tempo-driven effects safe |
| `tempo_lock_lost` | audio/AudioActor.cpp | `tempoConfidence` drops **below 0.4** (from lock) | Hysteresis: 0.1 | ~0.01–0.1 Hz | Beat unlocked; tempo-driven effects pause/fallback |
| `chord_change` | audio/AudioActor.cpp (~line 3400+) | `chordState.type` OR `chordState.rootNote` changes | Fire only on state transition (not every frame) | ~0.01–0.05 Hz | Chord detected; palette or key-based effects update |

**Detection Algorithm for Audio Silence (Canonical Example)**:
```cpp
static float prevSilentScale = -1.0f;
static int silenceFrameCount = 0;
static bool inSilenceRegime = false;

// In AudioActor::onTick() or ControlBusFrame publish loop:
const float SILENCE_THRESHOLD = 0.2f;
const float SILENCE_HYSTERESIS = 0.05f;
const int SILENCE_DEBOUNCE_FRAMES = 5;

bool wasSilent = inSilenceRegime;
if (!inSilenceRegime && frame.silentScale < SILENCE_THRESHOLD) {
    silenceFrameCount++;
    if (silenceFrameCount >= SILENCE_DEBOUNCE_FRAMES) {
        inSilenceRegime = true;
        TRACE_INSTANT("audio_silence_entered");
    }
} else if (inSilenceRegime && frame.silentScale > (SILENCE_THRESHOLD + SILENCE_HYSTERESIS)) {
    silenceFrameCount = 0;
    inSilenceRegime = false;
    TRACE_INSTANT("audio_silence_exited");
} else if (!inSilenceRegime) {
    silenceFrameCount = 0;  // Reset counter outside silence range
}
prevSilentScale = frame.silentScale;
```

### C. Health/System Regime (2 markers)

| Name | File:Line | Trigger Condition | Hysteresis | Expected Rate | Hypothesis |
|------|-----------|-------------------|------------|----------------|------------|
| `heap_pressure_entered` | hardware/PerformanceMonitor.cpp | `free_internal_bytes < 20 KB` | Fire once; exit when > 25 KB | <0.01 Hz (rare) | Memory fragmentation risk; throttle effects |
| `heap_pressure_exited` | hardware/PerformanceMonitor.cpp | `free_internal_bytes > 25 KB` (from low) | Fire once | <0.01 Hz (rare) | Memory recovered; resume full load |

### D. User Action Markers (Optional — Tier 4.5)

| Name | File:Line | Trigger Condition | Args | Expected Rate | Hypothesis |
|------|-----------|-------------------|------|----------------|------------|
| `button_pressed_<id>` | hardware/EncoderManager.cpp | Button press detected | `button_id` (0–N) | ~0.1–1 Hz (user-driven) | User triggered action; correlate with effect change |
| `encoder_turn_<id>_<direction>` | hardware/EncoderManager.cpp | Encoder rotation detected | `encoder_id`, `direction` ("cw"/"ccw") | ~0.1–1 Hz (user-driven) | Parameter adjustment; label parameter space navigation |

*Note: These are optional (Tier 4.5). Include only if manual control narrative is critical.*

### E. OTA / Configuration (2 markers)

| Name | File:Line | Trigger Condition | Args | Expected Rate | Hypothesis |
|------|-----------|-------------------|------|----------------|------------|
| `config_save` | core/persistence/ZoneConfigManager.cpp | NVS write completes | `key` (string: effect/palette/expr/zone) | ~0.02–0.1 Hz (on change) | Configuration persisted; state snapshot |
| `firmware_version` | main.cpp:setup() (or SystemInit.cpp) | Boot initialization | `version` (string, e.g. "v3.1.4") | 1 per boot | Firmware identity for log correlation |

---

## Implementation Roadmap

### Phase 1 (Immediate — Boot & Audio Regime)
1. Add boot phase markers in `core/SystemInit.cpp` and `main.cpp`
2. Add audio regime transition markers in `audio/AudioActor.cpp`
3. Wire `boot_phase_audio_first_hop` to ControlBus initialization
4. **File changes**: ~5 files, ~80 LOC net (mostly marker firings + hysteresis state)

### Phase 2 (Soon — Health & Config)
1. Add health markers in `hardware/PerformanceMonitor.cpp`
2. Add config save marker in `core/persistence/ZoneConfigManager.cpp`
3. Add firmware version marker in `main.cpp`
4. **File changes**: ~3 files, ~40 LOC net

### Phase 3 (Optional — User Actions)
1. Add button/encoder markers in `hardware/EncoderManager.cpp`
2. **File changes**: ~1 file, ~30 LOC net

---

## Coordination with Sibling Surfaces

### With Surface 4 (WiFi)
- Surface 4 owns `wifi_ap_started`, `wifi_client_connected`, etc.
- Surface 9 inserts **one marker** `boot_phase_wifi_up` (higher-level narrative)
- No conflict: S4 is detailed, S9 is sequencing

### With Surface 5 (ControlBus)
- Surface 5 may own ControlBus message flow detail
- Surface 9 inserts `boot_phase_audio_first_hop` at the **first publish event**
- Location: `audio/AudioActor.cpp`, right after first `ControlBus::publish()`

### With Surface 6 (Effect Lifecycle)
- Surface 6 owns `effect_switch`, `effect_init_failed`, `palette_changed`
- Surface 9 does **not** re-instrument these; defers to S6
- Surface 9 may add `effect_regime_changed` (e.g., "effect now responsive to audio") in future iterations

### With Surface 7 (Performance)
- Surface 7 owns detailed performance counters and sampling
- Surface 9 **inherits** `frame_deadline_missed` (already in RendererActor.cpp:1897)
- Surface 9 adds `heap_pressure_*` regimes for high-level narrative

---

## Narrative Enablement: What Captain & Analyser See

### Timeline View (Perfetto)
1. Boot sequence: `boot_phase_idf_done` → `boot_phase_actors_started` → `boot_phase_audio_first_hop` → `boot_phase_renderer_first_frame` → `boot_phase_wifi_up` → `boot_phase_ready`
   - **Insight**: Identify bottleneck phases (e.g., WiFi takes 2s, audio hangs 500ms)
2. Audio regime cascades:
   - `audio_silence_entered` → effect defaults to static/idle
   - `tempo_lock_acquired` → effect enables BPM sync (e.g., BeatParitySprite)
   - `chord_change` → palette updates (key-aware rendering)
   - **Insight**: Correlate visual changes with audio state; validate effect responsiveness

### Analyser Regime Segmentation
1. **Silent regime**: 0 to `audio_silence_entered`
2. **Music regime**: `audio_silence_exited` to end
3. **High-energy regime**: `audio_loud_entered` to `audio_loud_exited` (inverse)
4. **Locked regime**: `tempo_lock_acquired` to `tempo_lock_lost`
5. **Pressure regime**: `heap_pressure_entered` to `heap_pressure_exited`
   - **Insight**: Compare effect performance/smoothness across regimes

---

## Cost Summary

| Layer | Count | Events/Min (Typical) | Annual (8h/day) |
|-------|-------|----------------------|-----------------|
| Boot markers | 6 | 0.01 | ~3 KB |
| Audio regime | 6 | ~100 | ~52 MB |
| Health regime | 2 | ~1 | ~500 KB |
| Config markers | 2 | ~5 | ~2.6 MB |
| User actions (optional) | 2 | ~10 | ~5.2 MB |
| **Total (with S4/S6 deferred)** | **18** | **~116** | **~60 MB** |

**Conclusion**: Negligible. Even with 500 events/min (peak music + active user), annual cost is <300 MB.

---

## Open Questions & Future Work

1. **Chord detection integration**: Does `TempoTracker` or another module expose `chordState.type`/`rootNote`? Verify before implementing.
2. **Thermal warning threshold**: Is 80°C accurate for this ESP32-S3 board? Verify in hardware docs / PerformanceMonitor.cpp.
3. **Loud regime threshold (0.7 RMS, 1 second)**: Are these tuned to the current audio pipeline? May need adjustment post-Surface 5 validation.
4. **Button/Encoder ID mapping**: How many buttons/encoders on physical K1? Ensure ID consistency with hardware definitions.

---

## References

- **Section 4** (WiFi): Deferred network markers
- **Section 5** (ControlBus): Deferred message-level detail
- **Section 6** (Effect Lifecycle): Deferred effect state markers
- **Section 7** (Performance): Deferred performance counters
- **Section 9** (This): Tier 4 narrative labels for regime & boot sequence
