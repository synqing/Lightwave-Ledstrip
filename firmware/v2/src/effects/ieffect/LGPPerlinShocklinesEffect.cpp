/**
 * @file LGPPerlinShocklinesEffect.cpp
 * @brief LGP Perlin Shocklines - Beat/flux injects sharp travelling ridges
 * 
 * Audio-reactive Perlin effect:
 * - Base noise field provides organic texture
 * - Beat/flux events inject shockwaves at centre
 * - Shockwaves propagate outward and dissolve
 * - Treble modulates ridge sharpness
 */

#include "LGPPerlinShocklinesEffect.h"
#include "../CoreEffects.h"
#include "../../config/features.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPPerlinShocklinesEffect::LGPPerlinShocklinesEffect()
    : m_noiseX(0)
    , m_noiseY(0)
    , m_waveFront(0)
    , m_waveEnergy(0.0f)
    , m_time(0)
    , m_momentum(0.0f)
    , m_lastHopSeq(0)
{
}

bool LGPPerlinShocklinesEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_noiseX = random16();
    m_noiseY = random16();
    m_waveFront = 0;
    m_waveEnergy = 0.0f;
    m_time = 0;
    m_momentum = 0.0f;
    m_lastHopSeq = 0;
    return true;
}

void LGPPerlinShocklinesEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN - Shockwaves propagate outward from centre
    const bool hasAudio = ctx.audio.available;
    float dt = ctx.getSafeDeltaSeconds();
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    // =========================================================================
    // Audio Analysis - Inject Shockwaves
    // =========================================================================
#if FEATURE_AUDIO_SYNC
    if (hasAudio) {
        bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;
            
            // Beat/flux → inject shockwave at centre
            float trigger = ctx.audio.flux() * 0.5f + ctx.audio.beatStrength() * 0.5f;
            if (trigger > 0.3f) {
                // Start a new travelling ridge from the centre.
                // Keep it stable and predictable (no accumulating buffer explosion).
                m_waveFront = 0;
                float shockEnergy = trigger * trigger; // emphasise strong events
                if (shockEnergy > m_waveEnergy) {
                    m_waveEnergy = shockEnergy;
                }
            }
        }
    }
#endif

    // =========================================================================
    // Travelling Ridge Update (centre-origin)
    // =========================================================================
    // Advance wavefront outward (0..79), decay energy smoothly.
    float waveSpeed = (0.45f + 0.85f * speedNorm); // per-second in "distance units"
    uint8_t advance = (uint8_t)(waveSpeed * (dt * 60.0f)); // dt-scaled step
    if (advance < 1) advance = 1;
    if (m_waveEnergy > 0.01f) {
        uint16_t next = (uint16_t)m_waveFront + advance;
        m_waveFront = (next > 79) ? 79 : (uint8_t)next;
        // Energy decay (avoid strobe) - dt-corrected
        m_waveEnergy *= powf(0.90f, dt * 60.0f);
    } else {
        m_waveEnergy = 0.0f;
    }

    // =========================================================================
    // Audio-Driven Momentum (Emotiscope-style)
    // =========================================================================
    float push = 0.0f;
#if FEATURE_AUDIO_SYNC
    if (hasAudio) {
        float energy = ctx.audio.rms();
        push = energy * energy * energy * energy * speedNorm * 0.1f; // Heavy emphasis on loud
    }
#endif
    m_momentum *= powf(0.99f, dt * 60.0f); // Smooth decay (dt-corrected)
    if (push > m_momentum) {
        m_momentum = push; // Only boost, no jarring drops
    }

    // =========================================================================
    // Noise Field Updates (REVERSED for center→edges flow)
    // =========================================================================
    // Keep coordinates large enough to actually vary noise across dist.
    // SUBTRACT to reverse direction (center→edges instead of edges→center)
    uint16_t baseStep = (uint16_t)(6 + (uint16_t)(speedNorm * 22.0f));
    uint16_t momentumStep = (uint16_t)(m_momentum * 800.0f); // Scale momentum to step size
    uint16_t tStep = baseStep + momentumStep;
    
    // Reverse: subtract instead of add (makes pattern flow center→edges)
    m_time = (uint16_t)(m_time - tStep);
    m_noiseX = (uint16_t)(m_noiseX - (uint16_t)(13 + (tStep >> 1)));
    m_noiseY = (uint16_t)(m_noiseY - (uint16_t)(9 + (tStep >> 2)));

    // =========================================================================
    // Rendering (centre-origin pattern)
    // =========================================================================
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    // Get treble for ridge sharpness
    float trebleNorm = 0.0f;
#if FEATURE_AUDIO_SYNC
    if (hasAudio) {
        trebleNorm = ctx.audio.treble();
    }
#endif
    float sharpness = 0.3f + trebleNorm * 0.7f; // 0.3-1.0

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        // Calculate distance from centre pair
        uint16_t dist = centerPairDistance(i);
        uint8_t dist8 = (uint8_t)dist;
        
        // Sample base noise field
        // Use centre distance as the spatial axis (no linear sweeps).
        uint8_t baseNoise = inoise8((uint16_t)(m_noiseX + dist * 23), (uint16_t)(m_time));

        // Travelling ridge at the wavefront (simple, fast profile)
        uint8_t ridge = qsub8(255, (uint8_t)(abs((int16_t)dist8 - (int16_t)m_waveFront) * 9));
        ridge = scale8(ridge, (uint8_t)(m_waveEnergy * 255.0f));

        // Combine noise + ridge
        uint8_t combined8 = qadd8((uint8_t)(baseNoise >> 1), scale8(ridge, (uint8_t)(180 + sharpness * 75.0f)));
        
        // Map to palette and brightness
        uint8_t paletteIndex = combined8 + ctx.gHue;
        uint8_t brightness = scale8(qadd8(48, combined8), (uint8_t)(255.0f * intensityNorm));
        
        CRGB color = ctx.palette.getColor(paletteIndex, brightness);
        
        // Apply to strip 1
        ctx.leds[i] = color;
        
        // Apply to strip 2 (mirrored, phase offset)
        if (i + STRIP_LENGTH < ctx.ledCount) {
            uint8_t paletteIndex2 = (uint8_t)(paletteIndex + 48);
            CRGB color2 = ctx.palette.getColor(paletteIndex2, brightness);
            ctx.leds[i + STRIP_LENGTH] = color2;
        }
    }
}

void LGPPerlinShocklinesEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPPerlinShocklinesEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Perlin Shocklines",
        "Beat-driven travelling ridges propagating from centre",
        plugins::EffectCategory::PARTY,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

