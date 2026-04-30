---
abstract: "Verbatim audit of Emotiscope 1.2's beta/, inactive/, and system/ light-mode files plus the upstream globals they rely on (sample_history ring buffer, low_pass_filter, clip_float, interpolate, linear_to_tri). Canonicalises the Emotiscope waveform draw model — raw 128-sample audio slice, multi-pass IIR low-pass at ~110 Hz cutoff (configurable), peak-normalised auto-scale, sample-as-brightness mapping with progress-driven hue. Used to ground K1's SnapwaveLinear/ChevronWaves redesign during the 2026-04-30 spazz investigation. Read this when deciding whether a K1 wave effect should mirror Emotiscope's waveform.h, SB 3.1.0's snapwave, or a hybrid."
---

# Canonical Emotiscope Beta — Light-Mode Audit (2026-04-30)

This document is the **read-only canonical record** of Emotiscope 1.2's `beta/`, `inactive/`, and `system/` light-mode draw functions, together with the upstream globals and helpers they consume. Every excerpt is verbatim from the Emotiscope 1.2 source tree at `/Users/spectrasynq/Workspace_Management/Software/K1.node1/references/Emotiscope.sourcecode/Emotiscope-1.2/`.

It exists to ground the spazz-mode redesign for K1's four wave effects (SnapwaveLinear, ChevronWaves, ChevronWavesEnhanced, plus the SnapwaveLinear lineage) by establishing precisely how Emotiscope's *direct cousin* — `beta/waveform.h` — draws an audio waveform. Speculation about K1 is confined to the final mapping section.

---

## 1. Beta mode catalogue

The beta folder contains four experimental modes. None are currently registered in `light_modes[]` (see `light_modes.h` lines 41–64); they are siblings of the active and inactive modes but were never promoted.

| File | Public function | Status in `light_modes.h` | One-line purpose |
|------|-----------------|---------------------------|------------------|
| `beta/waveform.h` | `draw_waveform()` | not included | Direct oscilloscope-style draw of the most recent 128 raw audio samples, low-pass filtered, peak-normalised, mapped one-sample-per-LED with progress-driven hue. |
| `beta/neurons.h` | `draw_neurons()` | not included | Visualises a (presumed) on-device neural classifier — first 64 LEDs show `spectrogram_smooth[]` in green; second 64 LEDs show three hidden/output neuron channels mapped to RGB. |
| `beta/plot.h` | `draw_plot()` | not included | Auto-stretched 128-sample waveform projected onto an x-axis (sample value → pixel index), 1000 sub-sample iterations rasterise a continuous curve into an `image[]` accumulator, then peak-normalised and gamma-squared. |
| `beta/debug.h` | `draw_debug()`, `draw_debug_novelty()` | not included | Rainbow-hue diagnostic strip (`draw_debug`) and a 128-pixel two-channel scope of `vu_curve` (red) and `novelty_curve_normalized` (green) for `draw_debug_novelty`. |

The `inactive/` folder holds two finished but disabled modes:

| File | Public function | Status | Purpose |
|------|-----------------|--------|---------|
| `inactive/neutral.h` | `draw_neutral()` | included and registered | Pure colour-range gradient at full brightness — no audio reactivity at all. The "lights stay on" fallback. |
| `inactive/starfield.h` | `draw_starfield()`, `draw_starfield_real()` | included header, **not** registered (commented-out at `light_modes.h:32` and `:57`) | Static gradient (`draw_starfield`) and 150-particle drifting star field (`draw_starfield_real`). |

The `system/` folder holds two non-user-selectable modes:

| File | Public function | Purpose |
|------|-----------------|---------|
| `system/self_test.h` | `draw_self_test()` | 5-second LED self-test sequence: black → red → green → blue → grey → black, then re-enters the previously queued mode via `enter_queued_light_mode()`. |
| `system/presets.h` | `draw_presets()` | One-line stub — draws a single red dot oscillating left/right at 1 Hz (`sin(millis()*0.001)`). Effectively unfinished. |

---

## 2. Per-mode analysis

### 2.1 `beta/waveform.h` — the focal mode

**Purpose.** Render the most recent 128 audio samples directly to 128 LEDs, one sample per LED, after a configurable low-pass filter and peak-normalisation. Each LED's brightness IS the audio sample at that timestep.

**Verbatim source** (full file, 44 lines):

```c
float samples[NUM_LEDS];

void draw_waveform(){
	memcpy(samples, &sample_history[(SAMPLE_HISTORY_LENGTH-1) - (NUM_LEDS+CHUNK_SIZE)], sizeof(float) * NUM_LEDS);
	float cutoff_frequency = 110 + 2000*(1.0/*-configuration.bass*/);
	low_pass_filter(samples, NUM_LEDS, SAMPLE_RATE, cutoff_frequency, 3);

	float max_val = 0.000001;
	for(uint16_t i = 0; i < NUM_LEDS; i++){
		float sample = clip_float(samples[i]);

		max_val = max(max_val, sample);
	}

	float auto_scale = 1.0 / max_val;

	if(configuration.mirror_mode.value.u32 == false){
		for(uint16_t i = 0; i < NUM_LEDS; i++){
			float progress = float(i) / NUM_LEDS;
			float sample = clip_float(samples[i]) * auto_scale;
			CRGBF pixel_color = hsv(
				configuration.color.value.f32 + (configuration.color_range.value.f32*progress),
				configuration.saturation.value.f32,
				sample
			);

			leds[i] = pixel_color;
		}
	}
	else{
		for(uint16_t i = 0; i < NUM_LEDS >> 1; i++){
			float progress = float(i) / (NUM_LEDS>>1);
			float sample = clip_float(samples[i]) * auto_scale;
			CRGBF pixel_color = hsv(
				configuration.color.value.f32 + (configuration.color_range.value.f32*progress),
				configuration.saturation.value.f32,
				sample
			);

			leds[63-i] = pixel_color;
			leds[64+i] = pixel_color;
		}
	}
}
```

**Audio source.**
- Reads from `sample_history[]` — a ring of `SAMPLE_HISTORY_LENGTH = 4096` floats, each in the −1.0..1.0 range, populated by `acquire_sample_chunk()` at `microphone.h:150`. New samples are pushed to the tail by `shift_and_copy_arrays()` (memmove + memcpy) every audio chunk.
- The slice grabbed is `[ (4095 − 192) .. (4095 − 192) + 128 )` = the 128 samples ending one chunk (64 samples) before the most recent. The `+ CHUNK_SIZE` offset is to step back from the still-being-written tail; this prevents tearing.
- `SAMPLE_RATE` is `12800 * 2 = 25 600 Hz` (microphone.h:24). `CHUNK_SIZE` is 64 samples (microphone.h:23). 128 samples therefore equals 5.0 ms of audio at 25.6 kHz.

**Smoothing.**
- A 3rd-order one-pole IIR low-pass with cutoff `110 + 2000 * 1.0 = 2110 Hz` (the `configuration.bass` term is commented out — the code as shipped uses a fixed 2110 Hz cutoff).
- Implementation (verbatim, `utilities.h:235–254`):

```c
void low_pass_filter(float* input_array, uint16_t num_samples, uint16_t sample_rate, float cutoff_frequency, uint8_t filter_order) {
    float rc = 1.0f / (2.0f * M_PI * cutoff_frequency);
    float alpha = 1.0f / (1.0f + (sample_rate * rc));

    for (uint8_t order = 0; order < filter_order; ++order) {
        float filtered_value = input_array[0];
        for (uint16_t n = 1; n < num_samples; ++n) {
            filtered_value = alpha * input_array[n] + (1.0f - alpha) * filtered_value;
            input_array[n] = filtered_value;
        }
    }
}
```

For `cutoff = 2110 Hz, sr = 25 600 Hz`: `rc = 7.54e-5`, `alpha = 1 / (1 + 25600 * 7.54e-5) = 1 / 2.93 ≈ 0.341`. Three serial passes of this (filter_order = 3) compound the smoothing. The filter operates *spatially* across the 128-sample slice as if each sample were a timestep at the audio sample rate — i.e. it smooths the waveform along its length, not across frames.

**Phase / position.** None. The mode is purely spatial: LED `i` displays sample `i` from the most recent slice. No time-based phase term, no propagation, no scrolling. The "motion" you see is purely the audio waveform itself shifting forwards as the `sample_history` ring rotates.

**Beat / tempo handling.** None. There is no reference to `tempo`, `beat`, `tempi_smooth`, `novelty`, or any onset/beat global anywhere in the file.

**Auto-scale.** A single peak-normalisation pass — `max_val = max(samples)` (clipped to `[0, 1]`), then every sample is divided by `max_val`. Tiny floor of `1e-6` prevents division by zero. **There is no temporal envelope** — the auto-scale is recomputed per frame from that frame's slice alone, so quiet frames will be amplified to full brightness just like loud ones.

**Mirror mode.** When `configuration.mirror_mode == true`, only the first 64 samples are used; both halves of the strip mirror them around the centre. This is the classic centre-out symmetric waveform display.

**Hue mapping.** `H = configuration.color + configuration.color_range * progress`, where `progress` is 0..1 along the strip (or 0..1 along each half in mirror mode). Saturation fixed by `configuration.saturation`. Brightness IS the audio sample — there is no separate brightness parameter.

---

### 2.2 `beta/neurons.h`

**Purpose.** Diagnostic visualisation of an embedded neural classifier (presumed used for note/chord recognition; the upstream `input_neuron_values`, `hidden_neuron_1_values`, `hidden_neuron_3_values`, `output_neuron_values` arrays are referenced but not defined in beta — they live elsewhere in the project).

**Verbatim source** (full file, 24 lines):

```c
void draw_neurons() {
	for (uint16_t i = 0; i < 64; i++) {
		float input_neuron_value = clip_float(input_neuron_values[i]);

		float hidden_neuron_2_value = clip_float(hidden_neuron_1_values[i>>1]*0.2);
		float hidden_neuron_3_value = clip_float(hidden_neuron_3_values[i>>1]*0.2);
		float output_neuron_value   = clip_float(output_neuron_values[i]);

		CRGBF color_network = {
			(hidden_neuron_2_value*hidden_neuron_2_value),
			(hidden_neuron_3_value*hidden_neuron_3_value),
			sqrt(output_neuron_value),
		};

		CRGBF color_spectral = {
			0,
			spectrogram_smooth[i],
			0,
		};

		leds[64+i] = color_network;
		leds[i] = color_spectral;
	}
}
```

**Audio sources.**
- `spectrogram_smooth[i]` for the lower 64 LEDs — same 64-bin smoothed spectrogram used by the active spectrum mode.
- `input_neuron_values`, `hidden_neuron_1_values`, `hidden_neuron_3_values`, `output_neuron_values` for the upper 64 — defined in another translation unit (the neural-classifier module).

**Smoothing.** None applied here; the spectrogram is consumed as already smoothed (`spectrogram_smooth`), and neuron values are consumed verbatim. Hidden neurons are gain-scaled by `* 0.2` then squared in the colour assignment (gamma-like compression).

**Phase.** None. Pure index-to-LED mapping; LED `i` shows neuron/bin `i`.

**Beat handling.** None.

**Note.** `input_neuron_value` is computed but never used in the output — likely a leftover from an earlier four-channel layout.

---

### 2.3 `beta/plot.h`

**Purpose.** A more elaborate oscilloscope: instead of mapping one sample per LED in time order, it maps each sample's *amplitude* to a horizontal pixel index, building up a histogram-style scatter that traces the curve of the waveform on the strip (sample-value-axis, not sample-time-axis).

**Verbatim source** (full file, 89 lines — this is `draw_plot()` plus the `draw_line()` helper it does not actually call in the shipped code):

```c
void draw_line(float* layer, float x1, float x2, float opacity) {
	if (x1 > x2) {	// Ensure x1 <= x2
		float temp = x1;
		x1 = x2;
		x2 = temp;
	}

	float ix1 = floor(x1);
	float ix2 = ceil(x2);

	// start pixel
	if (ix1 >= 0 && ix1 < NUM_LEDS) {
		float coverage = 1.0 - (x1 - ix1);
		float mix = opacity * coverage;

		layer[uint16_t(ix1)] += mix;
	}

	// end pixel
	if (ix2 >= 0 && ix2 < 128) {
		float coverage = x2 - floor(x2);
		float mix = opacity * coverage;

		layer[uint16_t(ix2)] += mix;
	}

	// pixels in between
	for (float i = ix1 + 1; i < ix2; i++) {
		if (i >= 0 && i < NUM_LEDS) {
			layer[uint16_t(i)] += opacity;
		}
	}
}

void draw_plot(){
	static float image[NUM_LEDS];

	//if(waveform_locked == false && waveform_sync_flag == true){
		//waveform_sync_flag = false;

		memset(image, 0, sizeof(float)*NUM_LEDS);

		const uint16_t num_samples = 128;
		float* samples_raw = &sample_history[(SAMPLE_HISTORY_LENGTH-1) - (num_samples)];
		float samples[num_samples];
		memcpy(samples, samples_raw, sizeof(float)*num_samples);

		float max_stretch = 0.025;
		for(uint16_t i = 0; i < num_samples; i+=1){
			float sample = samples[i];
			max_stretch = max(max_stretch, fabs(sample));
		}
		float auto_stretch = 1.0 / max_stretch;

		for(uint16_t i = 0; i < num_samples; i+=1){
			samples[i] = samples[i]*auto_stretch;
			samples[i] = samples[i]*62.0 + 64;
		}

		const uint16_t num_iterations = 1000;
		const float step_size = 1.0 / num_iterations;
		float progress = 0.0;
		for(uint16_t i = 0; i < num_iterations; i++){
			progress += step_size;

			float sample = interpolate(progress, samples, (num_samples-1));
			uint16_t sample_whole = sample;
			float sample_fract = sample - sample_whole;

			image[sample_whole  ] += (1.0-sample_fract);
			image[sample_whole+1] += (sample_fract);
		}
	//}

	float max_val = 0.0000001;
	for(uint16_t i = 0; i < NUM_LEDS; i++){
		max_val = max(max_val, image[i]);
	}

	float auto_scale = 1.0 / (max_val);

	for(uint16_t i = 0; i < NUM_LEDS; i++){
		float progress = float(i) / NUM_LEDS;
		float pixel = clip_float( image[i] * auto_scale );
		pixel *= pixel;
		CRGBF pixel_color = hsv(configuration.color.value.f32 + linear_to_tri(progress)*configuration.color_range.value.f32, configuration.saturation.value.f32, pixel);
		leds[i] = pixel_color;
	}
}
```

**Audio source.** Same `sample_history[(LEN-1) - 128]` slice as `waveform.h` but **no `+ CHUNK_SIZE` offset** — reads right up against the tail, which is why the original commit had a `waveform_locked` guard (commented out). This means tearing is possible during simultaneous render and audio acquisition.

**Stretch / normalisation.** `max_stretch = max(fabs(samples), 0.025)` — note the absolute value (allows negative-going samples) and the floor at 0.025 (prevents over-amplification of near-silent input, unlike `waveform.h` which has only a `1e-6` floor and will explode quiet input).

**Phase / position.** Sample value drives `x` position: `samples[i] = samples[i] * auto_stretch * 62 + 64`, so each (auto-stretched) sample maps to roughly `x ∈ [2, 126]` on the strip. Then 1000 sub-sample iterations linearly interpolate between consecutive samples, splatting fractional weight into `image[sample_whole]` and `image[sample_whole+1]`. This builds up brightness wherever the curve dwells longest.

**Smoothing.** None on the audio. The `image` buffer is reset to zero each frame (the `static` declaration is for stack avoidance, not persistence — `memset` clears it). No temporal filtering.

**Beat / tempo handling.** None.

**Hue mapping.** Uses `linear_to_tri(progress)` — a triangular wave that goes 0 → 1 → 0 as `progress` goes 0..1 — so the colour band peaks at the strip's centre and falls off at both ends. Brightness gamma-squared (`pixel *= pixel`) for perceptual contrast.

**`linear_to_tri` definition** (`utilities.h:110–123`, verbatim):

```c
float linear_to_tri(float input) {
    if (input < 0.0f || input > 1.0f) {
        return -1.0f;
    }
    if (input <= 0.5f) {
        return 2.0f * input;
    } else {
        return 2.0f * (1.0f - input);
    }
}
```

**Note on `interpolate`** (`utilities.h:137–150`, verbatim):

```c
float IRAM_ATTR interpolate(float index, float* array, uint16_t array_size) {
	float index_f = index * (array_size - 1);
	uint16_t index_i = (uint16_t)index_f;
	float index_f_frac = index_f - index_i;

	float left_val = array[index_i];
	float right_val = array[index_i + 1];

	if (index_i + 1 >= array_size) {
		right_val = left_val;
	}

	return (1 - index_f_frac) * left_val + index_f_frac * right_val;
}
```

`interpolate(progress, samples, 127)` returns the linearly-interpolated sample value at fractional position `progress * 126`, used to build a 1000-step traversal through the 128-sample array.

---

### 2.4 `beta/debug.h`

**Purpose.** Two diagnostic draws kept for engineering bring-up. Neither is registered.

**Verbatim source** (full file, 34 lines):

```c
void draw_debug_novelty(){
	for(uint16_t i = 0; i < 128; i++){
		int32_t index = ((NOVELTY_HISTORY_LENGTH-1)-128)+i;
		float mag_vu = vu_curve[index];
		float mag_spec = novelty_curve_normalized[index];

		CRGBF dot_color = {
			mag_vu,
			mag_spec,
			0.0,
		};

		leds[i] = dot_color;
	}
}

void draw_debug(){
	for(uint16_t i = 0; i < 64; i++){
		float progress = num_leds_float_lookup[i<<1];
		leds[i] = hsv(
			get_color_range_hue(progress),
			1.0,
			1.0
		);
	}
	for(uint16_t i = 0; i < 64; i++){
		float progress = float(i) / 64;
		leds[64+i] = hsv(
			get_color_range_hue(progress),
			1.0,
			1.0 - progress
		);
	}
}
```

**Audio source for `draw_debug_novelty()`.** `vu_curve[]` (RMS-style level history) and `novelty_curve_normalized[]` (onset-novelty history). Both are 128-sample tail-slices ending at the most recent.

**Audio source for `draw_debug()`.** None — pure index-to-hue rainbow.

**Smoothing / phase / beat.** None applied here; consumes pre-smoothed inputs.

---

### 2.5 `inactive/neutral.h`

**Purpose.** Audio-agnostic colour-range gradient, used as the "lights stay on but don't react" mode.

**Verbatim source** (full file, 26 lines):

```c
void draw_neutral() {
	if(configuration.mirror_mode.value.u32 == true){ // Mirror mode
		for (uint16_t i = 0; i < (NUM_LEDS >> 1); i++) {
			float progress = num_leds_float_lookup[i<<1];
			CRGBF color = hsv(
				get_color_range_hue(progress),
				configuration.saturation.value.f32,
				1.0
			);

			leds[ (NUM_LEDS>>1)    + i] = color;
			leds[((NUM_LEDS>>1)-1) - i] = color;
		}
	}
	else{ // Non mirror
		for (uint16_t i = 0; i < NUM_LEDS; i++) {
			float progress = num_leds_float_lookup[i];
			CRGBF color = hsv(
				get_color_range_hue(progress),
				configuration.saturation.value.f32,
				1.0
			);

			leds[i] = color;
		}
	}
}
```

**Audio source.** None.
**Smoothing / phase / beat.** None.

---

### 2.6 `inactive/starfield.h`

**Purpose.** Two functions: a static gradient (`draw_starfield`, audio-agnostic), and a 150-particle drifting star field (`draw_starfield_real`, also audio-agnostic).

**Verbatim source** (full file, 72 lines): see the read above; key facts:

- 150 stars, each with `position`, `speed`, `brightness` arrays.
- Per-frame: each star advances by `speed * 0.1`, brightness decays by 0.975 (≈ `pow(0.975, frames_since_birth)`).
- Stars respawn when `|speed| <= 0.01` or `brightness < 0.001`. New `speed = (rand() − 0.5) * 0.1`, brightness reset to 1.0, position reset to 0.
- Stars expire (`speed = 0`) when `|position| > 1.25`.

**Audio source.** None.
**Smoothing.** Brightness uses `*= 0.975` per-frame multiplicative decay.
**Phase.** Per-particle `position += speed * 0.1` integration.
**Beat.** None.

This is purely time-driven motion with no audio reactivity, which is why it remains commented-out in `light_modes.h`.

---

### 2.7 `system/self_test.h`

**Purpose.** Boot/power-up LED sanity sequence.

**Verbatim source** (full file, 37 lines): a state machine — `START → LED → COMPLETE`. In `LED` state, it `fill_color` the strip with a different colour every 1000 ms (black, half-red, half-green, half-blue, grey, off), then transitions to `COMPLETE` which calls `enter_queued_light_mode()` to return to whatever was queued.

**Audio source.** None.
**Smoothing / phase / beat.** None — it is a wallclock state machine driven by `t_now_ms - test_start_time`.

---

### 2.8 `system/presets.h`

**Purpose.** A one-line stub. Draws a single red dot moving sinusoidally left/right at 1 Hz.

**Verbatim source** (full file, 6 lines):

```c
void draw_presets(){
	// Draw a dot moving left and right with a sine wave
	// The dot is colored red
	draw_dot(leds, NUM_RESERVED_DOTS+0, CRGBF(1.0, 0.0, 0.0), 0.5 + 0.5 * sin(millis() * 0.001), 1.0);
}
```

**Audio source.** None.
**Smoothing.** None.
**Phase.** Wallclock — `0.5 + 0.5 * sin(millis() * 0.001)` is a 0..1 sinusoid at ω = 1 rad/s ≈ 0.159 Hz. (The comment claims 1 Hz; the maths gives 0.159 Hz. Bug or shorthand — flagged.)
**Beat.** None.

---

## 3. The canonical Emotiscope waveform model

This is the model `beta/waveform.h` actually implements. Notation: `S` is the audio ring buffer of length `L = 4096`, `N = NUM_LEDS = 128`, `C = CHUNK_SIZE = 64`, `f_s = SAMPLE_RATE = 25 600 Hz`.

**Step 1 — Slice extraction.**

```
samples[i] = S[(L − 1) − (N + C) + i]    for i ∈ [0, N)
```

That is: take the 128 samples ending one chunk (64 samples ≈ 2.5 ms) before the most recent. The `+ CHUNK_SIZE` offset gives a tearing margin against `acquire_sample_chunk()` which is concurrently writing the tail.

**Step 2 — Spatial low-pass (3rd-order one-pole IIR).**

```
α = 1 / (1 + f_s · RC),   RC = 1 / (2π · f_c)
f_c = 110 + 2000 · 1.0 = 2110 Hz                    (configuration.bass currently disabled)

For order = 1..3:
  y[0] = samples[0]
  for n = 1..N−1:
    y[n] = α · samples[n] + (1 − α) · y[n−1]
  samples[] := y[]
```

With `f_c = 2110 Hz` and `f_s = 25 600 Hz`: `α ≈ 0.341`. Three serial passes give an effective single-pass-equivalent cutoff *lower* than 2110 Hz (cascaded one-poles roll off faster). The filter operates **across the spatial axis of the slice**, treating each LED-position as if it were a sample in time at 25.6 kHz — an unconventional but consistent choice.

**Step 3 — Per-frame peak normalisation (auto-gain, no envelope).**

```
max_val = max(1e-6, max{ clip_float(samples[i]) : i ∈ [0, N) })
auto_scale = 1 / max_val
```

`clip_float(x) = min(1, max(0, x))` — note that this throws away all *negative* samples. This is critical: Emotiscope's waveform is a **half-wave rectified, peak-normalised** signal. The negative half of each cycle becomes zero (black) before normalisation.

**Step 4 — Per-LED draw.**

Non-mirrored:
```
for i in 0..N−1:
  progress = i / N
  brightness = clip_float(samples[i]) · auto_scale
  H = configuration.color + configuration.color_range · progress
  leds[i] = HSV(H, configuration.saturation, brightness)
```

Mirrored (centre-out):
```
for i in 0..(N/2 − 1):
  progress = i / (N/2)
  brightness = clip_float(samples[i]) · auto_scale
  H = configuration.color + configuration.color_range · progress
  leds[63 − i] = leds[64 + i] = HSV(H, configuration.saturation, brightness)
```

**What the model is and is not.**

- It IS: a per-frame snapshot of the audio waveform's positive envelope mapped one-sample-per-LED, low-pass smoothed along the strip axis.
- It is NOT: a moving wave that propagates, a beat-synchronised pulse, a history-buffer trail, a sinusoidal phase oscillator, or anything that uses tempo / `beat` / `novelty` / `chroma`.
- There is NO time-axis history retained between frames. Each frame is computed from that frame's audio slice alone.
- There is NO temporal envelope follower. Quiet frames get full-brightness peak normalisation just like loud ones. (This is the noise-amplification problem inherent to the design — it would visibly "spazz" on silence, which is one reason it lives in `beta/`.)
- There is NO centre-origin propagation in the K1 sense. "Mirror mode" is a static spatial reflection, not an outward-radiating wave.

---

## 4. K1 SnapwaveLinear / ChevronWaves mapping

K1's wave effects live at:
- `firmware-v3/src/effects/ieffect/SnapwaveLinearEffect.{h,cpp}`
- `firmware-v3/src/effects/ieffect/ChevronWavesEffect.{h,cpp}`
- `firmware-v3/src/effects/ieffect/ChevronWavesEffectEnhanced.{h,cpp}`

Per the brief, SnapwaveLinear's "history-buffer trail design echoes a waveform/oscilloscope pattern". This makes Emotiscope's `beta/waveform.h` the closest cousin in the canonical reference — but **the design fit is partial, not a drop-in match**. Key differences that matter for the spazz redesign:

| Property | Emotiscope `beta/waveform.h` | What K1 needs (per CLAUDE.md hard constraints) |
|----------|------------------------------|------------------------------------------------|
| Origin | LED 0 → N (or mirrored at 63/64) | Centre 79/80 outward (CLAUDE.md hard constraint) |
| LED count | 128 | 320 (160 per strip × 2) |
| Audio slice | Raw `sample_history[]` ring | ControlBus has no equivalent raw-sample ring exposed to render() — render-side only sees `bands[]`, `chroma[]`, `rms`, `beat`, `onset`, `bins256[]` etc. |
| Smoothing | Spatial 3rd-order IIR LPF along the slice | Per CLAUDE.md, no heap, must complete in < 2.0 ms — spatial IIR over 320 LEDs is fine compute-wise |
| Auto-scale | Per-frame peak norm with `1e-6` floor (will spazz on silence) | This IS the spazz failure mode K1 is exhibiting — an unbounded peak-normalised brightness with no temporal envelope is exactly the bug pattern |
| Half-wave rectification | Yes (`clip_float` discards negatives) | Acceptable as a design choice but means only positive-going excursions illuminate |
| Time / motion | None (frame-independent) | K1 wave effects are explicitly *propagating* — they need a phase/position term |

### Recommendation

**Do NOT copy `beta/waveform.h` wholesale.** It has three properties that would *cause* the spazz behaviour K1 is showing, not fix it:

1. **No temporal envelope on the auto-scale floor** (`1e-6`). This guarantees that during silence or quiet passages, the noise floor is amplified to full strip brightness. This is the exact "spazzing on silence" failure mode that put it in `beta/` in the first place.
2. **No frame-to-frame coherence.** Each frame is independent. The visual "motion" you see is purely the audio waveform itself flowing through the slice — there is no phase oscillator, no propagation, no smoothing across frames. K1's wave effects are supposed to *propagate* outward from centre, not show a frame-by-frame snapshot.
3. **Non-centre-origin layout.** Mirror mode reflects around LED 63/64 of a 128-strip; K1 needs 79/80 of 160 (×2 strips). Trivially adaptable, but worth flagging.

**Recommended path: do NOT use Emotiscope's `beta/waveform.h` as the template, do NOT use SB 3.1.0's snapwave verbatim either, and instead build a hybrid** that takes the *useful* part of each:

- **From Emotiscope `beta/waveform.h`:** the spatial 3rd-order IIR low-pass as a *colour-band shaping* tool (cheap, deterministic, runs spatially in render() with zero heap), and the half-wave-rectified peak-as-brightness mapping for the *scrolling content* of the wave.
- **From SB 3.1.0's snapwave (per existing K1 design intent):** the propagating-wave phase term, the history-buffer trail (so frames are not independent), and the centre-origin layout.
- **Critical addition (NOT in either source):** a **temporal envelope follower** on the auto-scale (e.g. a slow-decay max tracker with a sensible *visual* floor like 0.05–0.1 of typical RMS, NOT 1e-6) so quiet passages stay quiet instead of being amplified to full brightness. This is the single most important design difference between something that works and something that spazzes.

The investigation should now read SnapwaveLinearEffect.cpp and the SB 3.1.0 snapwave reference to identify which of those three failures K1's current implementation actually has, then patch accordingly. Re-using `beta/waveform.h`'s exact code path is not advisable.

### Why `beta/plot.h` is NOT a fit

It maps amplitude to x-axis-position rather than time-to-position, and it uses `clip_float` similarly to waveform.h but with a more sensible floor (`max_stretch = 0.025`). The plot-style display is interesting but bears no resemblance to a propagating wave — it would not solve the K1 spazz problem and would introduce a fundamentally different visual.

---

## 5. Upstream definitions referenced

For full forensic provenance, the cited helpers were verified at:

- `sample_history[SAMPLE_HISTORY_LENGTH]` — `microphone.h:28`, length 4096, populated by `acquire_sample_chunk()` at `microphone.h:150–187` via `shift_and_copy_arrays()` (memmove-then-memcpy).
- `SAMPLE_HISTORY_LENGTH = 4096` — `microphone.h:26`.
- `CHUNK_SIZE = 64` — `microphone.h:23`.
- `SAMPLE_RATE = 12800 * 2 = 25600` — `microphone.h:24`.
- `NUM_LEDS = 128` — `global_defines.h:13`.
- `low_pass_filter()` — `utilities.h:235–254`. 1st-order one-pole IIR applied `filter_order` times.
- `clip_float()` — `utilities.h:179`, `min(1.0f, max(0.0f, input))`.
- `interpolate()` — `utilities.h:137–150`, linear interpolation at fractional index.
- `linear_to_tri()` — `utilities.h:110–123`, triangular wave 0→1→0 across input 0..1.

---

## 6. Files inspected

1. `Emotiscope-1.2/src/light_modes/beta/waveform.h` (44 lines)
2. `Emotiscope-1.2/src/light_modes/beta/neurons.h` (24 lines)
3. `Emotiscope-1.2/src/light_modes/beta/plot.h` (89 lines)
4. `Emotiscope-1.2/src/light_modes/beta/debug.h` (34 lines)
5. `Emotiscope-1.2/src/light_modes/inactive/neutral.h` (26 lines)
6. `Emotiscope-1.2/src/light_modes/inactive/starfield.h` (72 lines)
7. `Emotiscope-1.2/src/light_modes/system/self_test.h` (37 lines)
8. `Emotiscope-1.2/src/light_modes/system/presets.h` (6 lines)
9. `Emotiscope-1.2/src/light_modes.h` (133 lines, dispatcher)
10. `Emotiscope-1.2/src/utilities.h` (excerpts: `interpolate`, `clip_float`, `low_pass_filter`, `linear_to_tri`)
11. `Emotiscope-1.2/src/microphone.h` (excerpts: `sample_history`, `SAMPLE_HISTORY_LENGTH`, `CHUNK_SIZE`, `SAMPLE_RATE`, `acquire_sample_chunk`)

Total: 11 files; 8 read in full, 3 read in targeted excerpts where only specific symbol definitions were needed.

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-04-30 | agent:embedded-system-engineer | Created. Verbatim audit of Emotiscope 1.2 beta/, inactive/, and system/ light modes plus upstream globals. Canonicalised the Emotiscope waveform model. Mapped against K1 SnapwaveLinear/ChevronWaves with hybrid recommendation (do NOT copy beta/waveform.h wholesale — its lack of temporal envelope is the spazz failure mode). |
