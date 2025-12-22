/**
 * FastLED Optimization Utilities
 * 
 * Centralized wrapper functions for FastLED's high-performance integer math operations.
 * These utilities provide 10-20x performance improvements over standard floating-point math.
 * 
 * Usage:
 *   #include "effects/utils/FastLEDOptim.h"
 *   
 *   // Replace slow float sin() with fast sin16()
 *   uint8_t brightness = fastSin8(phase);
 *   
 *   // Replace slow division with fast scale8()
 *   uint8_t scaled = fastScale8(value, 128);  // value * 128 / 256
 * 
 * Performance Comparison:
 *   sin()      : ~500 cycles (2.0μs @ 240MHz)
 *   sin16()    : ~50 cycles  (0.2μs @ 240MHz) - 10x faster
 *   sin8()     : ~25 cycles  (0.1μs @ 240MHz) - 20x faster
 * 
 * Memory: Header-only, no additional memory overhead
 */

#ifndef FASTLED_OPTIM_H
#define FASTLED_OPTIM_H

#include <FastLED.h>
#include <stdint.h>

#ifndef TWO_PI
#define TWO_PI 6.28318530717958647692f
#endif

namespace FastLEDOptim {

// ============================================================================
// Trigonometric Functions (16-bit precision)
// ============================================================================

/**
 * Fast 16-bit sine lookup
 * @param phase16 Phase angle (0-65535 = 0 to 2*PI)
 * @return Sine value (-32767 to 32767)
 * 
 * Usage:
 *   uint16_t phase = millis() * 10;
 *   int16_t wave = fastSin16(phase);
 *   uint8_t brightness = (wave >> 8) + 128;  // Convert to 0-255
 */
inline int16_t fastSin16(uint16_t phase16) {
    return sin16(phase16);
}

/**
 * Fast 16-bit cosine lookup
 * @param phase16 Phase angle (0-65535 = 0 to 2*PI)
 * @return Cosine value (-32767 to 32767)
 */
inline int16_t fastCos16(uint16_t phase16) {
    return sin16(phase16 + 16384);  // cos = sin + 90° (16384 = 65536/4)
}

/**
 * Fast 8-bit sine lookup
 * @param phase8 Phase angle (0-255 = 0 to 2*PI)
 * @return Sine value (0-255, with 128 = zero crossing)
 * 
 * Usage:
 *   uint8_t phase = millis() / 10;
 *   uint8_t brightness = fastSin8(phase);
 */
inline uint8_t fastSin8(uint8_t phase8) {
    return sin8(phase8);
}

/**
 * Fast 8-bit cosine lookup
 * @param phase8 Phase angle (0-255 = 0 to 2*PI)
 * @return Cosine value (0-255, with 128 = zero crossing)
 */
inline uint8_t fastCos8(uint8_t phase8) {
    return sin8(phase8 + 64);  // cos = sin + 90° (64 = 256/4)
}

/**
 * Convert float radians to 16-bit phase
 * @param radians Angle in radians
 * @return Phase value (0-65535)
 */
inline uint16_t radiansToPhase16(float radians) {
    // 65536 / (2 * PI) = 10430.378...
    return (uint16_t)(radians * 10430.378f);
}

/**
 * Convert float radians to 8-bit phase
 * @param radians Angle in radians
 * @return Phase value (0-255)
 */
inline uint8_t radiansToPhase8(float radians) {
    // 256 / (2 * PI) = 40.7436...
    return (uint8_t)(radians * 40.7436654315f);
}

/**
 * Convert normalized position (0.0-1.0) to 16-bit phase
 * @param position Normalized position (0.0-1.0)
 * @param frequency Frequency multiplier
 * @return Phase value (0-65535)
 */
inline uint16_t positionToPhase16(float position, float frequency = 1.0f) {
    return (uint16_t)(position * frequency * 65535.0f);
}

// ============================================================================
// Scaling Functions (Division-Free)
// ============================================================================

/**
 * Fast 8-bit scaling (replaces value * scale / 255)
 * @param value Value to scale (0-255)
 * @param scale Scale factor (0-255)
 * @return Scaled value (0-255)
 * 
 * Performance: ~5 cycles vs ~100 cycles for division
 * 
 * Usage:
 *   uint8_t dimmed = fastScale8(brightness, 128);  // 50% brightness
 */
inline uint8_t fastScale8(uint8_t value, uint8_t scale) {
    return scale8(value, scale);
}

/**
 * Fast 16-bit scaling
 * @param value Value to scale (0-65535)
 * @param scale Scale factor (0-255)
 * @return Scaled value (0-65535)
 */
inline uint16_t fastScale16by8(uint16_t value, uint8_t scale) {
    return scale16by8(value, scale);
}

/**
 * Fast video-corrected scaling (perceptual brightness)
 * @param value Value to scale (0-255)
 * @param scale Scale factor (0-255)
 * @return Scaled value with gamma correction
 */
inline uint8_t fastScale8Video(uint8_t value, uint8_t scale) {
    return scale8_video(value, scale);
}

/**
 * Fast in-place scaling (modifies value directly)
 * @param value Reference to value (0-255)
 * @param scale Scale factor (0-255)
 */
inline void fastNScale8(uint8_t& value, uint8_t scale) {
    value = scale8(value, scale);
}

// ============================================================================
// Beat Functions (Automatic Phase Management)
// ============================================================================

/**
 * Beat-synchronized 8-bit sine wave
 * @param bpm Beats per minute
 * @param low Minimum value (0-255)
 * @param high Maximum value (0-255)
 * @return Sine wave value synchronized to beat
 * 
 * Usage:
 *   uint8_t brightness = fastBeatSin8(120, 64, 255);  // 120 BPM, 64-255 range
 */
inline uint8_t fastBeatSin8(uint8_t bpm, uint8_t low, uint8_t high) {
    return beatsin8(bpm, low, high);
}

/**
 * Beat-synchronized 16-bit sine wave
 * @param bpm Beats per minute
 * @param low Minimum value (0-65535)
 * @param high Maximum value (0-65535)
 * @return Sine wave value synchronized to beat
 */
inline uint16_t fastBeatSin16(uint8_t bpm, uint16_t low, uint16_t high) {
    return beatsin16(bpm, low, high);
}

/**
 * Beat-synchronized 8-bit cosine wave
 * @param bpm Beats per minute
 * @param low Minimum value (0-255)
 * @param high Maximum value (0-255)
 * @return Cosine wave value synchronized to beat
 */
inline uint8_t fastBeatCos8(uint8_t bpm, uint8_t low, uint8_t high) {
    // Cosine is sine with 64 phase offset (90 degrees in 0-255 range)
    return beatsin8(bpm, low, high, 0, 64);
}

// ============================================================================
// Arithmetic Functions (Saturated Math)
// ============================================================================

/**
 * Fast saturated addition (prevents overflow)
 * @param i First value (0-255)
 * @param j Second value (0-255)
 * @return Sum clamped to 0-255
 */
inline uint8_t fastQAdd8(uint8_t i, uint8_t j) {
    return qadd8(i, j);
}

/**
 * Fast saturated subtraction (prevents underflow)
 * @param i First value (0-255)
 * @param j Second value (0-255)
 * @return Difference clamped to 0-255
 */
inline uint8_t fastQSub8(uint8_t i, uint8_t j) {
    return qsub8(i, j);
}

/**
 * Fast multiply with saturation
 * @param i First value (0-255)
 * @param j Second value (0-255)
 * @return Product clamped to 0-255
 */
inline uint8_t fastQMul8(uint8_t i, uint8_t j) {
    return qmul8(i, j);
}

// ============================================================================
// Position and Distance Utilities
// ============================================================================

/**
 * Calculate distance from centre (normalized 0.0-1.0)
 * @param position LED position (0-159)
 * @param centerPoint Centre LED position (typically 79 or 80)
 * @return Normalized distance (0.0 = centre, 1.0 = edge)
 */
inline float distanceFromCenter(uint16_t position, uint16_t centerPoint) {
    float dist = abs((float)position - (float)centerPoint);
    return dist / (float)centerPoint;  // Normalize to 0.0-1.0
}

/**
 * Calculate distance from centre using 16-bit math
 * @param position LED position (0-159)
 * @param centerPoint Centre LED position (typically 79 or 80)
 * @return Distance value (0-65535, normalized)
 */
inline uint16_t distanceFromCenter16(uint16_t position, uint16_t centerPoint) {
    uint16_t dist = abs((int16_t)position - (int16_t)centerPoint);
    return (dist * 65535) / centerPoint;
}

/**
 * Map LED position to normalized value (0.0-1.0)
 * @param position LED position (0-159)
 * @param stripLength Total strip length (typically 160)
 * @return Normalized position (0.0-1.0)
 */
inline float normalizePosition(uint16_t position, uint16_t stripLength) {
    return (float)position / (float)stripLength;
}

/**
 * Map LED position to 16-bit phase (0-65535)
 * @param position LED position (0-159)
 * @param stripLength Total strip length (typically 160)
 * @param frequency Frequency multiplier
 * @return Phase value (0-65535)
 */
inline uint16_t positionToPhase16(uint16_t position, uint16_t stripLength, float frequency = 1.0f) {
    float normalized = (float)position / (float)stripLength;
    return (uint16_t)(normalized * frequency * 65535.0f);
}

// ============================================================================
// Wave Generation Utilities
// ============================================================================

/**
 * Generate sine wave brightness from position
 * @param position LED position (0-159)
 * @param frequency Spatial frequency (waves per strip)
 * @param phase Temporal phase offset
 * @return Brightness value (0-255)
 * 
 * Usage:
 *   uint8_t brightness = generateSineWave(i, 3.0f, millis() * 0.001f);
 */
inline uint8_t generateSineWave(uint16_t position, float frequency, float phase) {
    float normalizedPos = (float)position / 160.0f;
    float wavePhase = normalizedPos * frequency * TWO_PI + phase;
    uint16_t phase16 = radiansToPhase16(wavePhase);
    int16_t wave16 = fastSin16(phase16);
    return (uint8_t)((wave16 >> 8) + 128);  // Convert -32767..32767 to 0..255
}

/**
 * Generate sine wave brightness using 8-bit math (faster, less precise)
 * @param position LED position (0-159)
 * @param frequency Spatial frequency (waves per strip)
 * @param phase8 Temporal phase offset (0-255)
 * @return Brightness value (0-255)
 */
inline uint8_t generateSineWave8(uint16_t position, uint8_t frequency, uint8_t phase8) {
    uint8_t pos8 = (position * 255) / 160;
    uint8_t wavePhase = scale8(pos8, frequency) + phase8;
    return fastSin8(wavePhase);
}

// ============================================================================
// Interference Calculation Utilities
// ============================================================================

/**
 * Calculate interference intensity between two wave sources
 * @param phase1 Phase of first wave (0-65535)
 * @param phase2 Phase of second wave (0-65535)
 * @return Interference intensity (0-255)
 * 
 * Uses: I = I1 + I2 + 2√(I1×I2) × cos(Δφ)
 * Simplified: intensity = 128 + 127 × cos(phase_diff)
 */
inline uint8_t calculateInterference(uint16_t phase1, uint16_t phase2) {
    int16_t phaseDiff = (int16_t)phase1 - (int16_t)phase2;
    int16_t cosValue = fastCos16((uint16_t)phaseDiff);
    return (uint8_t)((cosValue >> 8) + 128);  // Convert to 0-255
}

/**
 * Calculate interference with amplitude weighting
 * @param amplitude1 Amplitude of first wave (0-255)
 * @param amplitude2 Amplitude of second wave (0-255)
 * @param phaseDiff Phase difference (0-65535)
 * @return Interference intensity (0-255)
 */
inline uint8_t calculateInterferenceWeighted(uint8_t amplitude1, uint8_t amplitude2, uint16_t phaseDiff) {
    int16_t cosValue = fastCos16(phaseDiff);
    uint8_t interferenceTerm = fastScale8(fastQMul8(amplitude1, amplitude2), (cosValue >> 8) + 128);
    return fastQAdd8(fastQAdd8(amplitude1, amplitude2), interferenceTerm);
}

// ============================================================================
// Colour Space Utilities
// ============================================================================

/**
 * Fast HSV to RGB conversion (uses FastLED's optimized function)
 * @param h Hue (0-255)
 * @param s Saturation (0-255)
 * @param v Value/Brightness (0-255)
 * @return RGB colour
 */
inline CRGB fastHSVtoRGB(uint8_t h, uint8_t s, uint8_t v) {
    CHSV hsv(h, s, v);
    return hsv;
}

/**
 * Fast RGB brightness scaling (perceptual)
 * @param color Input colour
 * @param brightness Brightness factor (0-255)
 * @return Scaled colour
 */
inline CRGB fastScaleRGB(CRGB color, uint8_t brightness) {
    CRGB result = color;
    result.nscale8(brightness);
    return result;
}

/**
 * Fast RGB brightness scaling (video-corrected)
 * @param color Input colour
 * @param brightness Brightness factor (0-255)
 * @return Scaled colour with gamma correction
 */
inline CRGB fastScaleRGBVideo(CRGB color, uint8_t brightness) {
    CRGB result = color;
    result.nscale8_video(brightness);
    return result;
}

// ============================================================================
// Fade Utilities
// ============================================================================

/**
 * Fast fade to black (replaces manual loop)
 * @param leds LED array
 * @param numLeds Number of LEDs
 * @param fadeAmount Fade amount (0-255, higher = faster fade)
 */
inline void fastFadeToBlack(CRGB* leds, uint16_t numLeds, uint8_t fadeAmount) {
    fadeToBlackBy(leds, numLeds, fadeAmount);
}

/**
 * Fast fade light by amount
 * @param leds LED array
 * @param numLeds Number of LEDs
 * @param fadeAmount Fade amount (0-255)
 */
inline void fastFadeLightBy(CRGB* leds, uint16_t numLeds, uint8_t fadeAmount) {
    fadeLightBy(leds, numLeds, fadeAmount);
}

} // namespace FastLEDOptim

#endif // FASTLED_OPTIM_H

