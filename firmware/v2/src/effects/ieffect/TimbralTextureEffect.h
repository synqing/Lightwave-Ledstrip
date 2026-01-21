/**
 * @file TimbralTextureEffect.h
 * @brief Timbral Texture - Visual complexity tracks audio texture changes
 *
 * Effect ID: TBD (assigned at registration)
 * Family: AMBIENT
 * Tags: CENTER_ORIGIN | AUDIO_REACTIVE | TIMBRAL_SALIENCY
 *
 * CONCEPT:
 * Visual texture complexity mirrors audio texture complexity using timbralSaliency():
 * - LOW timbralSaliency  -> Simple, smooth Perlin noise (1 octave, large scale)
 * - HIGH timbralSaliency -> Complex, detailed noise (3 octaves, fine scale)
 *
 * This effect is the FIRST to utilize timbralSaliency (0% utilization before this).
 *
 * Audio Features Used:
 * - timbralSaliency(): Primary driver of visual complexity (0.0-1.0)
 *   - HIGH when timbre/texture changes (new instrument, vocal entry, effect)
 *   - LOW during consistent texture passages
 *
 * Visual Mapping:
 * - Timbral saliency -> Octave count (1.0 to 3.0)
 * - Timbral saliency -> Spatial scale (60 to 30 - more detail = smaller scale)
 * - RMS (optional) -> Brightness modulation for energy feedback
 *
 * Instance State:
 * - m_noiseTime: Accumulated time for noise advection
 * - m_noiseX, m_noiseY: Noise field coordinates
 * - m_smoothedTimbre: Smoothed timbral saliency value
 * - m_currentOctaves: Current octave count (1.0-3.0)
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../enhancement/SmoothingEngine.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

/**
 * @brief Visual texture complexity tracks audio timbral novelty
 *
 * Creates Perlin noise patterns where:
 * - Consistent audio texture -> smooth, simple patterns
 * - Changing audio texture -> complex, detailed patterns
 *
 * Uses multi-octave fractal noise with dynamic octave count.
 */
class TimbralTextureEffect : public plugins::IEffect {
public:
    TimbralTextureEffect();
    ~TimbralTextureEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Noise field state
    uint32_t m_noiseTime;       ///< Time accumulator for noise advection
    uint16_t m_noiseX;          ///< Noise X coordinate
    uint16_t m_noiseY;          ///< Noise Y coordinate

    // Timbral tracking
    float m_smoothedTimbre;     ///< Smoothed timbral saliency (0.0-1.0)
    float m_currentOctaves;     ///< Current octave count (1.0-3.0)

    // Audio smoothing (200ms time constant for natural response)
    enhancement::AsymmetricFollower m_timbreFollower{0.0f, 0.08f, 0.25f};

    // Optional RMS for brightness modulation
    enhancement::AsymmetricFollower m_rmsFollower{0.5f, 0.05f, 0.20f};
    float m_smoothedRms;

    // Hop sequence tracking for fresh audio detection
    uint32_t m_lastHopSeq;

    /**
     * @brief Compute fractal noise with variable octave count
     * @param x X coordinate
     * @param y Y coordinate
     * @param octaves Number of octaves (1-3)
     * @return 16-bit noise value (0-65535)
     *
     * Implements fractal Brownian motion (fBm) pattern:
     * - Each octave adds finer detail at half amplitude
     * - More octaves = more visual complexity
     */
    uint16_t fractalNoise(uint32_t x, uint32_t y, uint8_t octaves);
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
