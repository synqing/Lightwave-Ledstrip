# SNAPWAVE ORIGINAL IMPLEMENTATION - DO NOT LOSE THIS

## CRITICAL: This is the EXACT implementation of the original Snapwave mode

### Location: `/src/lightshow_modes.h` starting at line 1393

```cpp
void light_mode_snapwave() {
  static float waveform_peak_scaled_last = 0.0f;
  static CRGB16 last_color = {0, 0, 0};

  // Smooth the waveform peak with more aggressive smoothing
  SQ15x16 smoothed_peak_fixed = SQ15x16(waveform_peak_scaled) * 0.02 + SQ15x16(waveform_peak_scaled_last) * 0.98;
  waveform_peak_scaled_last = float(smoothed_peak_fixed);

  // --- Color Calculation from Chromagram ---
  CRGB16 current_sum_color = {0, 0, 0};
  SQ15x16 total_magnitude = 0.0;
  
  for (uint8_t c = 0; c < 12; c++) {
    float prog = c / 12.0f;
    float bin = float(chromagram_smooth[c]);

    float bright = bin;
    for (uint8_t s = 0; s < int(frame_config.SQUARE_ITER); s++) {
      bright *= bright;
    }
    float fract_iter = frame_config.SQUARE_ITER - floor(frame_config.SQUARE_ITER);
    if (fract_iter > 0.01) {
      float squared = bright * bright;
      bright = bright * (1.0f - fract_iter) + squared * fract_iter;
    }
    
    // Only add colors from bins above threshold for better color clarity
    if (bright > 0.05) {
      CRGB16 note_col = hsv(SQ15x16(prog), frame_config.SATURATION, SQ15x16(bright));
      current_sum_color.r += note_col.r;
      current_sum_color.g += note_col.g;
      current_sum_color.b += note_col.b;
      total_magnitude += bright;
    }
  }
  
  if (chromatic_mode == true && total_magnitude > 0.01) {
    // Normalize by total magnitude to get pure color, then scale by brightness
    current_sum_color.r /= total_magnitude;
    current_sum_color.g /= total_magnitude;
    current_sum_color.b /= total_magnitude;
    
    // Scale back up by total magnitude for proper brightness
    current_sum_color.r *= total_magnitude;
    current_sum_color.g *= total_magnitude;
    current_sum_color.b *= total_magnitude;
  } else if (chromatic_mode == false) {
    // Use single hue with total_magnitude for brightness
    current_sum_color = hsv(chroma_val + hue_position, frame_config.SATURATION, total_magnitude);
  }

  // Apply PHOTONS brightness scaling
  current_sum_color.r *= frame_config.PHOTONS;
  current_sum_color.g *= frame_config.PHOTONS;
  current_sum_color.b *= frame_config.PHOTONS;
  
  // Use the chromagram color mix for the waveform
  last_color = current_sum_color;

  // --- Dynamic Fading for Trails ---
  float abs_amp = fabs(waveform_peak_scaled); 
  if (abs_amp > 1.0f) abs_amp = 1.0f; 
  
  float max_fade_reduction = 0.10; 
  SQ15x16 dynamic_fade_amount = 1.0 - (max_fade_reduction * abs_amp);

  // Apply the dynamic fade
  for (uint16_t i = 0; i < NATIVE_RESOLUTION; i++) {
    leds_16[i].r *= dynamic_fade_amount;
    leds_16[i].g *= dynamic_fade_amount;
    leds_16[i].b *= dynamic_fade_amount;
  }

  // --- Waveform Display --- 
  shift_leds_up(leds_16, 1);
  
  // Use smoothed peak instead of raw peak
  float amp = waveform_peak_scaled_last;
  
  // Apply threshold to ignore tiny movements
  float threshold = 0.05f;
  if (fabs(amp) < threshold) {
    amp = 0.0f;
  }
  
  // CRITICAL FIX: waveform_peak_scaled is absolute value (0 to 1)
  // We need to use the actual waveform data to get signed values
  // For now, use a simple oscillation based on chromagram phase
  float oscillation = 0.0f;
  
  // Create oscillation from dominant chromagram notes
  for (uint8_t i = 0; i < 12; i++) {
    if (chromagram_smooth[i] > 0.1) {
      // Each note contributes to position with different phase
      oscillation += float(chromagram_smooth[i]) * sin(millis() * 0.001f * (1.0f + i * 0.5f));
    }
  }
  
  // Normalize oscillation to -1 to +1 range
  oscillation = tanh(oscillation * 2.0f);
  
  // Mix oscillation with amplitude for more dynamic movement
  amp = oscillation * waveform_peak_scaled_last * 0.7f;
  
  if (amp > 1.0f) amp = 1.0f;
  else if (amp < -1.0f) amp = -1.0f;
  
  int center = NATIVE_RESOLUTION / 2;
  float pos_f = center + amp * (NATIVE_RESOLUTION / 2.0f); 
  int pos = int(pos_f + (pos_f >= 0 ? 0.5 : -0.5));
  if (pos < 0) pos = 0;
  if (pos >= NATIVE_RESOLUTION) pos = NATIVE_RESOLUTION - 1;
  
  // Set the new dot with the calculated color
  leds_16[pos] = last_color;
  
  if (CONFIG.MIRROR_ENABLED) {
    mirror_image_downwards(leds_16);
  }
}
```

## KEY CHARACTERISTICS OF SNAPWAVE:

1. **TIME-BASED OSCILLATION** (lines 1483-1489):
   ```cpp
   oscillation += float(chromagram_smooth[i]) * sin(millis() * 0.001f * (1.0f + i * 0.5f));
   ```
   - Uses `sin(millis())` for time-based motion
   - Each chromagram note contributes with different phase offset (i * 0.5f)

2. **SNAPPY ATTACK** (line 1492):
   ```cpp
   oscillation = tanh(oscillation * 2.0f);
   ```
   - The `tanh()` function creates the characteristic "snap"

3. **SCROLLING EFFECT** (line 1467):
   ```cpp
   shift_leds_up(leds_16, 1);
   ```
   - LEDs shift up creating the scrolling waveform effect

4. **DYNAMIC TRAILS** (lines 1453-1464):
   - Fade based on amplitude for dynamic trail length
   - More amplitude = less fade = longer trails

## CRITICAL NOTES:
- This mode was accidentally created when trying to fix the broken waveform mode
- The characteristic "snapwave" motion comes from the time-based sine oscillations
- DO NOT MODIFY THIS IMPLEMENTATION - Create new modes (Snapwave2, Snapwave3, etc.) for variations
- Mode number: LIGHT_MODE_SNAPWAVE (enum value 7)

## LAST KNOWN WORKING COMMIT:
- Current implementation in: 8532ec4 refactor(audio): Complete Phase 1 of Operation Frame-Restore