#pragma once

#include <cstdint>

// ES Goertzel frequency bin state
struct freq {
    float target_freq;
    float coeff;
    float window_step;
    float magnitude;
    float magnitude_full_scale;
    float magnitude_last;
    float novelty;
    uint16_t block_size;
};

// ES tempo resonator state
struct tempo {
    float target_tempo_hz;
    float coeff;
    float sine;
    float cosine;
    float window_step;
    float phase;
    float phase_target;
    bool  phase_inverted;
    float phase_radians_per_reference_frame;
    float beat;
    float magnitude;
    float magnitude_full_scale;
    uint32_t block_size;
};

