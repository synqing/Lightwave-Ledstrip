// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
#pragma once
#include <stdint.h>

namespace lightwaveos::audio {

/**
 * @brief Detected music style classification.
 *
 * MIS Phase 2: Adaptive style detection uses saliency patterns
 * to classify music into dominant characteristic styles. This
 * enables effects to adapt their behavior based on the type
 * of music being played.
 *
 * Style categories:
 * - UNKNOWN: Insufficient data or mixed characteristics
 * - RHYTHMIC_DRIVEN: EDM, hip-hop, dance (strong beat patterns)
 * - HARMONIC_DRIVEN: Jazz, classical, chord progressions
 * - MELODIC_DRIVEN: Vocal pop, lead instrument focus
 * - TEXTURE_DRIVEN: Ambient, drone, atmospheric
 * - DYNAMIC_DRIVEN: Orchestral, cinematic, dynamic range
 */
enum class MusicStyle : uint8_t {
    UNKNOWN = 0,           ///< Insufficient data or mixed characteristics
    RHYTHMIC_DRIVEN = 1,   ///< EDM, hip-hop, dance music
    HARMONIC_DRIVEN = 2,   ///< Jazz, classical, chord-heavy
    MELODIC_DRIVEN = 3,    ///< Vocal pop, lead melody focus
    TEXTURE_DRIVEN = 4,    ///< Ambient, drone, atmospheric
    DYNAMIC_DRIVEN = 5     ///< Orchestral, cinematic
};

/**
 * @brief Get human-readable name for music style.
 * @param style The music style enum value
 * @return C-string name of the style
 */
inline const char* getMusicStyleName(MusicStyle style) {
    switch (style) {
        case MusicStyle::UNKNOWN:         return "Unknown";
        case MusicStyle::RHYTHMIC_DRIVEN: return "Rhythmic";
        case MusicStyle::HARMONIC_DRIVEN: return "Harmonic";
        case MusicStyle::MELODIC_DRIVEN:  return "Melodic";
        case MusicStyle::TEXTURE_DRIVEN:  return "Texture";
        case MusicStyle::DYNAMIC_DRIVEN:  return "Dynamic";
        default:                          return "Invalid";
    }
}

} // namespace lightwaveos::audio
