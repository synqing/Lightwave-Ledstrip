#ifndef SIMD_GOERTZEL_H
#define SIMD_GOERTZEL_H

#include <stdint.h>

class SIMDGoertzel {
private:
    static const int NUM_BINS = 96;
    static const int SIMD_GROUP_SIZE = 8;
    
    float goertzel_coeffs[NUM_BINS] __attribute__((aligned(16)));
    float q1[NUM_BINS] __attribute__((aligned(16)));
    float q2[NUM_BINS] __attribute__((aligned(16)));
    float magnitudes[NUM_BINS] __attribute__((aligned(16)));
    
public:
    void init();
    void process(int16_t* samples, int num_samples);
    float* getMagnitudes() { return magnitudes; }
    
private:
    void calculateCoefficients();
    void processGroup(int16_t* samples, int start_bin, int num_samples);
};

#endif 