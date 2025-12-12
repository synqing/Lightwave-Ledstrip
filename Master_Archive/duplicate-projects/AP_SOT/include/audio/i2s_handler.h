#pragma once
#include "audio_frame.h"
#include <cstdint>

void initializeI2S();
bool readSamples(int16_t* sample_buffer, uint32_t* i2s_buffer); 