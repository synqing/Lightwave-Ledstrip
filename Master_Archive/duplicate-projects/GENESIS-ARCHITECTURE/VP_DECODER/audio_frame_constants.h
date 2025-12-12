#pragma once

/**
 * AudioFrame Energy Constants
 * 
 * Unified scaling factors to ensure consistent energy values
 * across all data sources (VP Decoder, Aether Engine, Live Audio)
 */

namespace AudioFrameConstants {
    
    // Energy scaling factors for consistent visual impact
    namespace EnergyScale {
        // VP Decoder scales (current standard)
        constexpr float BASS_MULTIPLIER = 1000.0f;
        constexpr float MID_MULTIPLIER = 800.0f;
        constexpr float HIGH_MULTIPLIER = 600.0f;
        
        // Aether Engine should use these reduced scales for ambient
        constexpr float AETHER_BASS_MULTIPLIER = 300.0f;
        constexpr float AETHER_MID_MULTIPLIER = 400.0f;
        constexpr float AETHER_HIGH_MULTIPLIER = 240.0f;
    }
    
    // Expected energy ranges for visual effects
    namespace EnergyRange {
        // Typical ranges
        constexpr float BASS_MIN = 0.0f;
        constexpr float BASS_TYPICAL = 400.0f;
        constexpr float BASS_MAX = 1000.0f;
        
        constexpr float MID_MIN = 0.0f;
        constexpr float MID_TYPICAL = 300.0f;
        constexpr float MID_MAX = 800.0f;
        
        constexpr float HIGH_MIN = 0.0f;
        constexpr float HIGH_TYPICAL = 200.0f;
        constexpr float HIGH_MAX = 600.0f;
        
        constexpr float TOTAL_MIN = 0.0f;
        constexpr float TOTAL_TYPICAL = 900.0f;
        constexpr float TOTAL_MAX = 2400.0f;
    }
    
    // Thresholds for effects
    namespace Thresholds {
        constexpr float SILENCE_THRESHOLD = 10.0f;
        constexpr float TRANSIENT_DELTA = 200.0f;
        constexpr float BEAT_ENERGY = 600.0f;
    }
    
    // Smoothing factors
    namespace Smoothing {
        constexpr float ENERGY_SMOOTHING = 0.85f;
        constexpr float FREQUENCY_BIN_SMOOTHING = 0.3f;
    }
}