#ifndef GOERTZEL_ENGINE_H
#define GOERTZEL_ENGINE_H

#include <Arduino.h>
#include "../config.h"

// Music-spaced frequency bins for Goertzel
// Based on musical notes, not linear frequency spacing
class GoertzelEngine {
public:
    GoertzelEngine();
    ~GoertzelEngine();
    
    void init();
    void process(int16_t* samples, int num_samples);
    float* getMagnitudes() { return magnitudes; }
    
    // Get the actual frequency for a bin
    float getBinFrequency(int bin) const { return target_frequencies[bin]; }
    
private:
    // Goertzel state for each frequency bin
    struct GoertzelState {
        float q1;
        float q2;
        float coeff;
    };
    
    GoertzelState* states;
    float* magnitudes;
    float* target_frequencies;
    
    // Initialize music-spaced frequencies
    void initializeFrequencies();
    
    // Calculate Goertzel coefficient for a frequency
    float calculateCoefficient(float frequency);
    
    // Process a single frequency bin
    void processBin(int bin, int16_t* samples, int num_samples);
};

#endif // GOERTZEL_ENGINE_H