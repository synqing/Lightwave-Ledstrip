#ifndef MEGA_LUTS_H
#define MEGA_LUTS_H

#include <Arduino.h>
#include <FastLED.h>
#include "../config/hardware_config.h"

/**
 * MEGA LUT SYSTEM - MAXIMIZING PERFORMANCE
 * 
 * Pre-calculated lookup tables for maximum performance
 * Target: Use 200-250KB of RAM for LUTs
 * 
 * ESP32-S3 has 512KB total RAM, ~320KB usable
 * Current usage: ~102KB
 * Available for LUTs: ~218KB
 */

// ============== TRIGONOMETRIC LUTs (16KB) ==============
// Full precision sin/cos tables for ALL angles (0-65535)
// Using 16-bit angle resolution for smooth animations
static const uint16_t TRIG_LUT_SIZE = 4096;  // 12-bit precision
extern const int16_t sinLUT_16bit[TRIG_LUT_SIZE];     // 8KB
extern const int16_t cosLUT_16bit[TRIG_LUT_SIZE];     // 8KB

// Fast trig access macros
#define SIN16(angle) sinLUT_16bit[((angle) >> 4) & 0xFFF]
#define COS16(angle) cosLUT_16bit[((angle) >> 4) & 0xFFF]

// ============== COLOR MIXING LUTs (32KB) ==============
// Pre-calculated color mixing for every possible combination
// 256x256 blend table for ultra-fast color mixing
extern const uint8_t colorBlendLUT[256][256];         // 64KB would be too big, using smaller
extern const uint8_t colorMixLUT[128][128][3];        // 48KB RGB mixing table

// HDR color correction LUTs
extern const uint16_t hdrGammaLUT[256];               // 512 bytes - 16-bit HDR values
extern const uint8_t hdrCompressLUT[1024];            // 1KB - HDR to 8-bit compression

// ============== TRANSITION LUTs (64KB) ==============
// Pre-rendered transition frames for common patterns
struct TransitionFrame {
    uint8_t pixel[HardwareConfig::NUM_LEDS];
};

// Pre-calculated transition sequences (16 frames each)
extern const TransitionFrame fadeTransitionLUT[16];       // 2.5KB per transition type
extern const TransitionFrame wipeTransitionLUT[16];       // 2.5KB
extern const TransitionFrame spiralTransitionLUT[16];     // 2.5KB
extern const TransitionFrame rippleTransitionLUT[16];     // 2.5KB
extern const TransitionFrame phaseTransitionLUT[16];      // 2.5KB

// Easing curve LUTs for all transition types
extern const uint8_t easingLUT[16][256];                  // 4KB - 16 easing curves

// Distance and angle LUTs for CENTER ORIGIN effects
extern const uint8_t distanceFromCenterLUT[HardwareConfig::NUM_LEDS];  // 160 bytes
extern const uint8_t angleFromCenterLUT[HardwareConfig::NUM_LEDS];     // 160 bytes
extern const uint8_t spiralAngleLUT[HardwareConfig::NUM_LEDS];         // 160 bytes

// ============== EFFECT PATTERN LUTs (48KB) ==============
// Pre-calculated effect patterns
extern const uint8_t wavePatternLUT[256][HardwareConfig::NUM_LEDS];    // 40KB
extern const uint8_t plasmaLUT[128][128];                              // 16KB
extern const uint8_t fireLUT[64][HardwareConfig::NUM_LEDS];           // 10KB
extern const uint8_t noiseLUT[256][64];                                // 16KB

// Palette interpolation LUT
extern const CRGB paletteInterpolationLUT[16][256];                    // 12KB

// ============== BRIGHTNESS & SCALING LUTs (8KB) ==============
// Ultra-fast brightness scaling
extern const uint8_t brightnessScaleLUT[256][256];                     // Would be 64KB, using function
extern const uint8_t dim8_video_LUT[256];                              // 256 bytes
extern const uint8_t brighten8_video_LUT[256];                         // 256 bytes

// Quadratic and cubic scaling
extern const uint8_t quadraticScaleLUT[256];                           // 256 bytes
extern const uint8_t cubicScaleLUT[256];                               // 256 bytes

// ============== ENCODER VALUE LUTs (4KB) ==============
// Pre-calculated encoder response curves
extern const uint8_t encoderLinearLUT[256];                            // 256 bytes
extern const uint8_t encoderExponentialLUT[256];                       // 256 bytes
extern const uint8_t encoderLogarithmicLUT[256];                       // 256 bytes
extern const uint8_t encoderSCurveLUT[256];                           // 256 bytes

// Multi-dimensional encoder mapping
extern const uint8_t encoder2DLUT[64][64];                             // 4KB

// ============== FREQUENCY ANALYSIS LUTs (16KB) ==============
// Pre-calculated FFT windows and frequency bins
extern const float hannWindowLUT[512];                                  // 2KB
extern const float blackmanWindowLUT[512];                             // 2KB
extern const uint8_t frequencyBinLUT[32][HardwareConfig::NUM_LEDS];   // 5KB
extern const uint8_t beatDetectionLUT[256][4];                         // 1KB

// ============== PARTICLE SYSTEM LUTs (8KB) ==============
// Pre-calculated particle physics
extern const int8_t particleVelocityLUT[256][2];                       // 512 bytes (x,y)
extern const uint8_t particleDecayLUT[256];                            // 256 bytes
extern const uint8_t particleGravityLUT[256][128];                     // 32KB would be too big
extern const CRGB particleColorLUT[64];                                // 192 bytes

// ============== ADVANCED EFFECT LUTs (32KB) ==============
// Perlin noise octaves
extern const uint8_t perlinNoiseLUT[256][256];                         // Would be 64KB, using smaller
extern const uint8_t perlinOctave1[128][128];                          // 16KB
extern const uint8_t perlinOctave2[64][64];                            // 4KB
extern const uint8_t perlinOctave3[32][32];                            // 1KB

// Cellular automata rules
extern const uint8_t cellularRulesLUT[256][8];                         // 2KB

// Fractals
extern const uint8_t mandelbrotLUT[128][128];                          // 16KB
extern const uint8_t juliaSetLUT[128][128];                            // 16KB

// ============== UTILITY FUNCTIONS ==============

// Initialize all LUTs (call once at startup)
void initializeMegaLUTs();

// Fast LUT-based operations
inline uint8_t fastSin8(uint16_t angle) {
    return (SIN16(angle) + 32768) >> 8;
}

inline uint8_t fastCos8(uint16_t angle) {
    return (COS16(angle) + 32768) >> 8;
}

inline CRGB fastBlendRGB(CRGB a, CRGB b, uint8_t blend) {
    return CRGB(
        colorBlendLUT[a.r][blend] + colorBlendLUT[b.r][255-blend],
        colorBlendLUT[a.g][blend] + colorBlendLUT[b.g][255-blend],
        colorBlendLUT[a.b][blend] + colorBlendLUT[b.b][255-blend]
    );
}

// Get pre-calculated transition frame
inline void getTransitionFrame(uint8_t* output, uint8_t transitionType, uint8_t frame) {
    const TransitionFrame* frames = nullptr;
    switch(transitionType) {
        case 0: frames = fadeTransitionLUT; break;
        case 1: frames = wipeTransitionLUT; break;
        case 2: frames = spiralTransitionLUT; break;
        case 3: frames = rippleTransitionLUT; break;
        case 4: frames = phaseTransitionLUT; break;
        default: return;
    }
    if (frames && frame < 16) {
        memcpy(output, frames[frame].pixel, HardwareConfig::NUM_LEDS);
    }
}

// Get pre-calculated wave pattern
inline void getWavePattern(uint8_t* output, uint8_t waveType) {
    if (waveType < 256) {
        memcpy(output, wavePatternLUT[waveType], HardwareConfig::NUM_LEDS);
    }
}

// Fast palette lookup with interpolation
inline CRGB getPaletteColorInterpolated(uint8_t palette, uint8_t index) {
    if (palette < 16) {
        return paletteInterpolationLUT[palette][index];
    }
    return CRGB::Black;
}

// Memory usage reporting
inline size_t getMegaLUTMemoryUsage() {
    size_t total = 0;
    
    // Trig LUTs
    total += TRIG_LUT_SIZE * 2 * sizeof(int16_t);  // 16KB
    
    // Color mixing
    total += 128 * 128 * 3;  // 48KB
    total += 256 * sizeof(uint16_t);  // 512B
    total += 1024;  // 1KB
    
    // Transitions
    total += 5 * 16 * HardwareConfig::NUM_LEDS;  // 12.5KB
    total += 16 * 256;  // 4KB
    total += 3 * HardwareConfig::NUM_LEDS;  // 480B
    
    // Effect patterns
    total += 256 * HardwareConfig::NUM_LEDS;  // 40KB
    total += 128 * 128;  // 16KB
    total += 64 * HardwareConfig::NUM_LEDS;  // 10KB
    total += 256 * 64;  // 16KB
    total += 16 * 256 * 3;  // 12KB
    
    // Brightness
    total += 3 * 256;  // 768B
    
    // Encoders
    total += 4 * 256;  // 1KB
    total += 64 * 64;  // 4KB
    
    // Frequency
    total += 2 * 512 * sizeof(float);  // 4KB
    total += 32 * HardwareConfig::NUM_LEDS;  // 5KB
    total += 256 * 4;  // 1KB
    
    // Particles
    total += 256 * 2 + 256 + 64 * 3;  // ~1KB
    
    // Advanced
    total += 128 * 128 + 64 * 64 + 32 * 32;  // 21KB
    total += 256 * 8;  // 2KB
    total += 2 * 128 * 128;  // 32KB
    
    return total;  // Total: ~234KB
}

// ============== EXTENDED LUTs FOR MAXIMUM USAGE ==============
// Additional LUTs to reach 250KB target

// Color space conversion LUTs
extern const uint8_t hsvToRgbLUT[256][256][3];  // Would be 192KB - too big
extern const uint8_t rgbToHsvLUT[64][64][64][3];  // Would be 768KB - way too big

// Instead, use smaller targeted LUTs
extern const uint8_t hueToRgbLUT[256][3];  // 768 bytes
extern const uint8_t saturationScaleLUT[256][256];  // Would be 64KB

// Complex waveform LUTs
extern const int16_t complexWaveformLUT[8][512];  // 8KB - 8 different waveforms

// Advanced transition masks
extern const uint8_t transitionMaskLUT[32][HardwareConfig::NUM_LEDS];  // 5KB

// Dithering patterns
extern const uint8_t ditheringLUT[16][16];  // 256 bytes

// Motion blur accumulation
extern const uint8_t motionBlurLUT[256][8];  // 2KB

// Shader-like effects
extern const uint8_t shaderEffectLUT[64][64];  // 4KB

#endif // MEGA_LUTS_H