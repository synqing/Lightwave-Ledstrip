#include "../include/audio/multiband_agc_system.h"

// Define static constexpr members (required for C++11)

constexpr float MultibandAGCSystem::BAND_BOUNDARIES[MultibandAGCSystem::NUM_BANDS + 1];
constexpr MultibandAGCSystem::BandConfig MultibandAGCSystem::BAND_CONFIGS[MultibandAGCSystem::NUM_BANDS];
constexpr float MultibandAGCSystem::A_WEIGHTING_COEFFS[MultibandAGCSystem::NUM_BANDS];