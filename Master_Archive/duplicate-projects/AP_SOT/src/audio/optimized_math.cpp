// optimized_math.cpp
#include "audio/optimized_math.h"
#include <math.h>

// Static member initialization
int16_t FastMath::sin_lut[SIN_LUT_SIZE];
uint8_t FastMath::sqrt_lut[SQRT_LUT_SIZE];
int16_t FastMath::log_lut[LOG_LUT_SIZE];
uint16_t FastMath::exp_lut[EXP_LUT_SIZE];
uint8_t FastMath::gamma_lut[256];
uint8_t FastMath::sin_interp[SIN_LUT_SIZE];
uint8_t FastMath::sqrt_interp[SQRT_LUT_SIZE];
bool FastMath::tables_initialized = false;

void FastMath::init() {
    if (tables_initialized) return;
    
    Serial.println("Initializing fast math lookup tables...");
    
    generateSinTable();
    generateSqrtTable();
    generateLogTable();
    generateExpTable();
    generateGammaTable();
    
    tables_initialized = true;
    Serial.println("Fast math tables initialized");
}

void FastMath::generateSinTable() {
    for (int i = 0; i < SIN_LUT_SIZE; i++) {
        float angle = (2.0f * PI * i) / SIN_LUT_SIZE;
        float value = sin(angle);
        sin_lut[i] = (int16_t)(value * 32767);
        
        // Calculate interpolation factor for smoother results
        float next_angle = (2.0f * PI * (i + 1)) / SIN_LUT_SIZE;
        float next_value = sin(next_angle);
        float slope = (next_value - value) * SIN_LUT_SIZE / (2.0f * PI);
        sin_interp[i] = (uint8_t)(abs(slope) * 255);
    }
}

void FastMath::generateSqrtTable() {
    // For 8-bit input (0-255)
    for (int i = 0; i < SQRT_LUT_SIZE; i++) {
        sqrt_lut[i] = (uint8_t)(sqrt(i) * 16);  // Scale up for precision
        
        // Interpolation factor
        if (i < SQRT_LUT_SIZE - 1) {
            float curr = sqrt(i);
            float next = sqrt(i + 1);
            sqrt_interp[i] = (uint8_t)((next - curr) * 255);
        }
    }
}

void FastMath::generateLogTable() {
    for (int i = 1; i < LOG_LUT_SIZE; i++) {
        float value = log2(i);
        log_lut[i] = (int16_t)(value * 256);  // 8.8 fixed point
    }
    log_lut[0] = -32768;  // log(0) = -inf
}

void FastMath::generateExpTable() {
    for (int i = 0; i < EXP_LUT_SIZE; i++) {
        float x = (i - 128) / 16.0f;  // Range -8 to +8
        float value = pow(2, x);
        exp_lut[i] = (uint16_t)(value * 256);  // 8.8 fixed point
    }
}

void FastMath::generateGammaTable() {
    for (int i = 0; i < 256; i++) {
        float normalized = i / 255.0f;
        float corrected = pow(normalized, 2.2f);
        gamma_lut[i] = (uint8_t)(corrected * 255);
    }
}

int16_t FastMath::fastSin(uint16_t angle) {
    // angle is 0-65535 for full circle
    uint32_t index = (angle * SIN_LUT_SIZE) >> 16;
    uint32_t fraction = ((angle * SIN_LUT_SIZE) & 0xFFFF) >> 8;  // 0-255
    
    int16_t value1 = sin_lut[index];
    int16_t value2 = sin_lut[(index + 1) & (SIN_LUT_SIZE - 1)];
    
    // Linear interpolation
    int32_t result = value1 + (((value2 - value1) * fraction) >> 8);
    return (int16_t)result;
}

int16_t FastMath::fastCos(uint16_t angle) {
    return fastSin(angle + 16384);  // cos(x) = sin(x + π/2)
}

fixed_t FastMath::fastSinFixed(fixed_t angle) {
    // Convert fixed-point radians to lookup table index
    // angle is in fixed-point radians (0 to 2π = 0 to 2*FIXED_ONE*π)
    uint32_t index = (angle * SIN_LUT_SIZE) / (2 * FLOAT_TO_FIXED(PI));
    index &= (SIN_LUT_SIZE - 1);
    
    return (fixed_t)sin_lut[index] << 1;  // Convert to fixed-point
}

uint8_t FastMath::fastSqrt(uint16_t x) {
    if (x < SQRT_LUT_SIZE) {
        return sqrt_lut[x] >> 4;  // Remove scaling
    }
    
    // For larger values, use bit manipulation
    uint8_t result = 0;
    uint16_t bit = 0x80;
    
    while (bit > 0) {
        uint16_t temp = (result | bit);
        if (temp * temp <= x) {
            result = temp;
        }
        bit >>= 1;
    }
    
    return result;
}

uint16_t FastMath::fastSqrt32(uint32_t x) {
    if (x < SQRT_LUT_SIZE) {
        return sqrt_lut[x];
    }
    
    // Newton-Raphson with good initial guess
    uint32_t result = x;
    uint8_t shifts = 0;
    
    // Normalize to 16-bit range
    while (x > 65535) {
        x >>= 2;
        shifts++;
    }
    
    // Use 8-bit sqrt as initial guess
    result = fastSqrt(x);
    
    // One iteration of Newton-Raphson
    result = (result + x / result) >> 1;
    
    // Denormalize
    return result << shifts;
}

fixed_t FastMath::fastSqrtFixed(fixed_t x) {
    if (x <= 0) return 0;
    
    // Initial guess using bit manipulation
    fixed_t result = x;
    fixed_t last;
    
    // Newton-Raphson iteration
    do {
        last = result;
        result = (result + FIXED_DIV(x, result)) >> 1;
    } while (abs(result - last) > 1);
    
    return result;
}

int16_t FastMath::fastLog2(uint16_t x) {
    if (x == 0) return -32768;
    if (x < LOG_LUT_SIZE) return log_lut[x];
    
    // For larger values, use bit counting
    int16_t result = 0;
    while (x >= LOG_LUT_SIZE) {
        x >>= 1;
        result += 256;  // Add 1 in 8.8 fixed point
    }
    
    return result + log_lut[x];
}

uint16_t FastMath::fastExp2(int16_t x) {
    // x is in 8.8 fixed point
    int16_t integer = x >> 8;
    uint8_t fraction = x & 0xFF;
    
    if (integer < -8) return 0;
    if (integer > 7) return 65535;
    
    uint16_t base = 1 << (integer + 8);
    uint16_t mult = exp_lut[128 + (fraction >> 4)];
    
    return (base * mult) >> 8;
}

uint8_t FastMath::fastGamma(uint8_t x, float gamma) {
    return gamma_lut[x];
}

uint32_t FastMath::fastHSVtoRGB(uint8_t h, uint8_t s, uint8_t v) {
    if (s == 0) {
        return (v << 16) | (v << 8) | v;  // Grayscale
    }
    
    uint8_t region = h / 43;  // 256/6 regions
    uint8_t remainder = (h - (region * 43)) * 6;
    
    uint8_t p = (v * (255 - s)) >> 8;
    uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;
    
    switch (region) {
        case 0: return (v << 16) | (t << 8) | p;
        case 1: return (q << 16) | (v << 8) | p;
        case 2: return (p << 16) | (v << 8) | t;
        case 3: return (p << 16) | (q << 8) | v;
        case 4: return (t << 16) | (p << 8) | v;
        default: return (v << 16) | (p << 8) | q;
    }
}

int16_t FastMath::fastCompressor(int16_t sample, uint8_t threshold, uint8_t ratio) {
    uint16_t abs_sample = abs(sample);
    uint16_t thresh16 = threshold << 7;  // Scale to 16-bit
    
    if (abs_sample <= thresh16) {
        return sample;
    }
    
    // Simple compression above threshold
    uint16_t excess = abs_sample - thresh16;
    excess = excess / ratio;
    
    uint16_t compressed = thresh16 + excess;
    return (sample < 0) ? -compressed : compressed;
}

uint8_t FastMath::fastEnvelopeFollower(uint8_t current, uint8_t target, uint8_t rate) {
    if (current == target) return current;
    
    int16_t diff = target - current;
    int16_t step = (diff * rate) >> 8;
    
    if (step == 0) {
        step = (diff > 0) ? 1 : -1;
    }
    
    return current + step;
}

// Fixed-point math implementations
fixed_t fixed_sqrt(fixed_t x) {
    if (x <= 0) return 0;
    
    fixed_t result = x;
    fixed_t last;
    
    // Initial guess
    int shifts = 0;
    fixed_t temp = x;
    while (temp > FIXED_ONE * 16) {
        temp >>= 2;
        shifts++;
    }
    
    result = FIXED_ONE;
    if (shifts > 0) {
        result <<= shifts;
    }
    
    // Newton-Raphson iteration
    for (int i = 0; i < 4; i++) {
        last = result;
        result = (result + FIXED_DIV(x, result)) >> 1;
        if (abs(result - last) <= 1) break;
    }
    
    return result;
}

fixed_t fixed_sin(fixed_t angle) {
    // Normalize angle to 0-2π
    const fixed_t TWO_PI_VAL = FLOAT_TO_FIXED(2 * PI);
    while (angle < 0) angle += TWO_PI_VAL;
    while (angle >= TWO_PI_VAL) angle -= TWO_PI_VAL;
    
    // Use FastMath lookup
    return FastMath::fastSinFixed(angle);
}

fixed_t fixed_cos(fixed_t angle) {
    return fixed_sin(angle + FLOAT_TO_FIXED(PI / 2));
}

// SIMD-friendly operations (ESP32-S3 has some SIMD capabilities)
void simd_multiply_add(float* result, const float* a, const float* b, float scalar, int count) {
    // Process 4 floats at a time for better cache usage
    int i = 0;
    for (; i < count - 3; i += 4) {
        result[i] = a[i] * b[i] + scalar;
        result[i+1] = a[i+1] * b[i+1] + scalar;
        result[i+2] = a[i+2] * b[i+2] + scalar;
        result[i+3] = a[i+3] * b[i+3] + scalar;
    }
    
    // Handle remaining elements
    for (; i < count; i++) {
        result[i] = a[i] * b[i] + scalar;
    }
}

void simd_complex_multiply(float* real_out, float* imag_out, 
                          const float* real_a, const float* imag_a,
                          const float* real_b, const float* imag_b, int count) {
    // Complex multiplication: (a + bi)(c + di) = (ac - bd) + (ad + bc)i
    for (int i = 0; i < count; i++) {
        real_out[i] = real_a[i] * real_b[i] - imag_a[i] * imag_b[i];
        imag_out[i] = real_a[i] * imag_b[i] + imag_a[i] * real_b[i];
    }
}