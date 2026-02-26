/**
 * @file LGPPhotonicCrystalEnhancedEffect.h
 * @brief LGP Photonic Crystal Enhanced - Enhanced version with heavy_chroma, 64-bin sub-bass, enhanced snare flash
 *
 * Effect ID: 92
 * Family: ADVANCED_OPTICAL
 * Tags: CENTER_ORIGIN, AUDIO_REACTIVE
 *
 * Enhancements over LGPPhotonicCrystalEffect (ID 33):
 * - Uses heavy_chroma consistently for color offset
 * - 64-bin sub-bass (bins 0-5) for bass-driven speed modulation
 * - Enhanced isSnareHit() for collision flash
 * - Uses beatPhase() for pattern synchronization
 * - Improved Spring smoothing parameters
 * - Better use of beatStrength() for brightness modulation
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../enhancement/SmoothingEngine.h"
#include "ChromaUtils.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPPhotonicCrystalEnhancedEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_PHOTONIC_CRYSTAL_ENHANCED;

    LGPPhotonicCrystalEnhancedEffect();
    ~LGPPhotonicCrystalEnhancedEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    // =========================================================================
    // PHASE ACCUMULATOR (float for dt-based advancement)
    // =========================================================================
    float m_phase = 0.0f;

    // =========================================================================
    // AUDIO SAMPLING (per-hop only, NOT per-frame)
    // =========================================================================
    uint32_t m_lastHopSeq = 0;

    // =========================================================================
    // ENERGY HISTORY (4-hop rolling average - proven pattern)
    // =========================================================================
    static constexpr uint8_t ENERGY_HISTORY = 4;
    float m_energyHist[ENERGY_HISTORY] = {0};
    float m_energySum = 0.0f;
    uint8_t m_energyHistIdx = 0;
    float m_energyAvg = 0.0f;      // Rolling average of recent energy
    float m_energyDelta = 0.0f;    // Current energy ABOVE average (positive only)

    // =========================================================================
    // ASYMMETRIC FOLLOWERS (frame-rate independent smoothing)
    // riseTau = fast attack, fallTau = slow decay
    // =========================================================================
    enhancement::AsymmetricFollower m_energyAvgFollower{0.5f, 0.08f, 0.20f};
    enhancement::AsymmetricFollower m_energyDeltaFollower{0.0f, 0.05f, 0.15f};

    // =========================================================================
    // SPRING PHYSICS FOR SPEED (stiffness=50, critically damped)
    // Prevents lurching from rapid audio changes
    // =========================================================================
    enhancement::Spring m_speedSpring;

    // =========================================================================
    // COLLISION FLASH (snare-triggered, spatial decay from center)
    // =========================================================================
    float m_collisionBoost = 0.0f;

    // =========================================================================
    // CHROMA DOMINANT BIN (smoothed over 250ms for color offset)
    // =========================================================================
    // Chromagram smoothing (AsymmetricFollower for natural attack/release)
    enhancement::AsymmetricFollower m_chromaFollowers[12];
    float m_chromaSmoothed[12] = {0.0f};
    float m_chromaTargets[12] = {0.0f};
    
    float m_chromaAngle = 0.0f;       // Circular EMA angle for chroma hue (radians)
    
    // Enhanced: 64-bin sub-bass tracking
    enhancement::AsymmetricFollower m_subBassFollower{0.0f, 0.05f, 0.30f};
    float m_subBassEnergy = 0.0f;
    float m_targetSubBass = 0.0f;
    
    // Tempo lock hysteresis (Schmitt trigger: 0.6 lock / 0.4 unlock)
    bool m_tempoLocked = false;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
