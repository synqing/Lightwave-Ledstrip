#ifndef DC_BLOCKING_FILTER_H
#define DC_BLOCKING_FILTER_H

#include <stdint.h>

// Simple but effective DC blocking filter
// y[n] = x[n] - x[n-1] + 0.995 * y[n-1]
class DCBlockingFilter {
private:
    float x_prev = 0.0f;
    float y_prev = 0.0f;
    static constexpr float ALPHA = 0.995f;  // Controls the cutoff frequency
    
public:
    void reset() {
        x_prev = 0.0f;
        y_prev = 0.0f;
    }
    
    int16_t process(int16_t input) {
        float x = (float)input;
        float y = x - x_prev + ALPHA * y_prev;
        
        x_prev = x;
        y_prev = y;
        
        // Clamp to int16 range
        if (y > 32767.0f) y = 32767.0f;
        else if (y < -32768.0f) y = -32768.0f;
        
        return (int16_t)y;
    }
    
    // Process a buffer in place
    void processBuffer(int16_t* buffer, int samples) {
        for (int i = 0; i < samples; i++) {
            buffer[i] = process(buffer[i]);
        }
    }
};

#endif // DC_BLOCKING_FILTER_H