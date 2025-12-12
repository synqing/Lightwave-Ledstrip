// optimized_math.h
#ifndef OPTIMIZED_MATH_H
#define OPTIMIZED_MATH_H

#include <Arduino.h>

// Fixed-point configuration
typedef int32_t fixed_t;
typedef int64_t fixed_long_t;

#define FIXED_SHIFT 16
#define FIXED_ONE (1 << FIXED_SHIFT)
#define FIXED_HALF (1 << (FIXED_SHIFT - 1))

// Fixed-point conversion macros
#define FLOAT_TO_FIXED(x) ((fixed_t)((x) * FIXED_ONE))
#define FIXED_TO_FLOAT(x) ((float)(x) / FIXED_ONE)
#define INT_TO_FIXED(x) ((fixed_t)(x) << FIXED_SHIFT)
#define FIXED_TO_INT(x) ((x) >> FIXED_SHIFT)

// Fixed-point operations
#define FIXED_MUL(a, b) ((fixed_t)(((fixed_long_t)(a) * (b)) >> FIXED_SHIFT))
#define FIXED_DIV(a, b) ((fixed_t)(((fixed_long_t)(a) << FIXED_SHIFT) / (b)))
#define FIXED_SQRT(x) (fixed_sqrt(x))

// Lookup table sizes
#define SIN_LUT_SIZE 1024
#define SQRT_LUT_SIZE 256
#define LOG_LUT_SIZE 256
#define EXP_LUT_SIZE 256

// Fast math class with lookup tables
class FastMath {
private:
    // Lookup tables
    static int16_t sin_lut[SIN_LUT_SIZE];
    static uint8_t sqrt_lut[SQRT_LUT_SIZE];
    static int16_t log_lut[LOG_LUT_SIZE];
    static uint16_t exp_lut[EXP_LUT_SIZE];
    static uint8_t gamma_lut[256];
    
    // Interpolation tables for higher precision
    static uint8_t sin_interp[SIN_LUT_SIZE];
    static uint8_t sqrt_interp[SQRT_LUT_SIZE];
    
    static bool tables_initialized;
    
public:
    static void init();
    
    // Fast trigonometric functions
    static int16_t fastSin(uint16_t angle);  // angle in 0-65535 (full circle)
    static int16_t fastCos(uint16_t angle);
    static fixed_t fastSinFixed(fixed_t angle);  // angle in fixed-point radians
    
    // Fast square root
    static uint8_t fastSqrt(uint16_t x);
    static uint16_t fastSqrt32(uint32_t x);
    static fixed_t fastSqrtFixed(fixed_t x);
    
    // Fast logarithm and exponential
    static int16_t fastLog2(uint16_t x);
    static uint16_t fastExp2(int16_t x);
    
    // Fast gamma correction
    static uint8_t fastGamma(uint8_t x, float gamma = 2.2f);
    
    // Color space conversions
    static uint32_t fastHSVtoRGB(uint8_t h, uint8_t s, uint8_t v);
    
    // Audio processing helpers
    static int16_t fastCompressor(int16_t sample, uint8_t threshold, uint8_t ratio);
    static uint8_t fastEnvelopeFollower(uint8_t current, uint8_t target, uint8_t rate);
    
private:
    static void generateSinTable();
    static void generateSqrtTable();
    static void generateLogTable();
    static void generateExpTable();
    static void generateGammaTable();
};

// Fixed-point math functions
fixed_t fixed_sqrt(fixed_t x);
fixed_t fixed_sin(fixed_t angle);
fixed_t fixed_cos(fixed_t angle);

// SIMD-friendly operations
void simd_multiply_add(float* result, const float* a, const float* b, float scalar, int count);
void simd_complex_multiply(float* real_out, float* imag_out, 
                          const float* real_a, const float* imag_a,
                          const float* real_b, const float* imag_b, int count);

#endif