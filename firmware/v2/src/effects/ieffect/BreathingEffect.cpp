/**
 * @file BreathingEffect.cpp
 * @brief Bloom-inspired breathing effect using Sensory Bridge architecture
 *
 * REDESIGNED based on Sensory Bridge Bloom mode principles:
 *
 * Core Architecture (Sensory Bridge Pattern):
 * - Audio → Color/Brightness (AUDIO-REACTIVE)
 * - Time → Motion Speed (TIME-BASED, USER-CONTROLLED)
 *
 * Key innovations:
 * 1. Separation of concerns: Audio drives color/brightness, time drives motion
 * 2. Frame persistence: Alpha blending (0.99) creates smooth motion through accumulation
 * 3. Chromatic color: 12-bin chromagram → summed RGB color (musical colors)
 * 4. Multi-stage smoothing: Chromagram (0.75 alpha) + Energy (0.3 alpha)
 * 5. History buffer: 4-frame rolling average filters single-frame spikes
 * 6. Exponential foreshortening: Visual acceleration toward edges
 *
 * This eliminates jittery motion by removing audio→motion coupling.
 */

#include "BreathingEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

// Unified logging system
#define LW_LOG_TAG "Breathing"
#include "../../utils/Log.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

// Slew rates for smooth motion
static constexpr float RADIUS_SLEW = 0.25f;      // ~33ms at 120fps (faster response)
static constexpr float FLUX_BOOST_DECAY = 0.85f; // ~100ms decay

// Exponential foreshortening exponent (Bloom's secret)
// < 1.0 = acceleration toward edges, 1.0 = linear
static constexpr float FORESHORTEN_EXP = 0.7f;

// Fallback breathing rate when tempo confidence low
static constexpr float FALLBACK_BREATH_RATE = 0.02f;  // ~1.5 second period

// ============================================================================
// Helper: Compute Chromatic Color from 12-bin Chromagram (Sensory Bridge Pattern)
// ============================================================================
static CRGB computeChromaticColor(const float chroma[12], const plugins::EffectContext& ctx) {
    CRGB sum = CRGB::Black;
    float share = 1.0f / 6.0f;  // Divide brightness among notes (Sensory Bridge uses 1/6.0)
    
    for (int i = 0; i < 12; i++) {
        float prog = i / 12.0f;  // 0.0 to 0.917 (0° to 330°)
        float brightness = chroma[i] * chroma[i] * share;  // Quadratic contrast (like Sensory Bridge)
        
        // Clamp brightness to valid range
        if (brightness > 1.0f) brightness = 1.0f;
        
        // Use palette system (matches WaveformEffect/SnapwaveEffect pattern)
        uint8_t paletteIdx = (uint8_t)(prog * 255.0f + ctx.gHue);
        uint8_t brightU8 = (uint8_t)(brightness * 255.0f);
        // Apply brightness scaling
        brightU8 = (uint8_t)((brightU8 * ctx.brightness) / 255);
        CRGB noteColor = ctx.palette.getColor(paletteIdx, brightU8);

        // PRE-SCALE: Prevent white accumulation from 12-bin chromagram sum
        // With 12 bins at full intensity, worst case is 12 * 255 = 3060
        // Scale by ~70% (180/255) to leave headroom for accumulation
        constexpr uint8_t PRE_SCALE = 180;
        noteColor.r = scale8(noteColor.r, PRE_SCALE);
        noteColor.g = scale8(noteColor.g, PRE_SCALE);
        noteColor.b = scale8(noteColor.b, PRE_SCALE);

        // Accumulate color contributions (now with reduced intensity)
        sum.r = qadd8(sum.r, noteColor.r);
        sum.g = qadd8(sum.g, noteColor.g);
        sum.b = qadd8(sum.b, noteColor.b);
    }
    
    // Clamp to valid range (qadd8 already handles overflow, but be safe)
    if (sum.r > 255) sum.r = 255;
    if (sum.g > 255) sum.g = 255;
    if (sum.b > 255) sum.b = 255;
    
    return sum;
}

BreathingEffect::BreathingEffect()
    : m_currentRadius(0.0f)
    , m_prevRadius(0.0f)
    , m_pulseIntensity(0.0f)
    , m_phase(0.0f)
    , m_fallbackPhase(0.0f)
    , m_lastFlux(0.0f)
    , m_fluxBoost(0.0f)
    , m_texturePhase(0.0f)
    , m_energySmoothed(0.0f)
    , m_radiusTargetSum(0.0f)
    , m_histIdx(0)
{
    // Initialize history buffer
    for (uint8_t i = 0; i < HISTORY_SIZE; ++i) {
        m_radiusTargetHist[i] = 0.0f;
    }
    
    // Initialize chromagram smoothing
    for (uint8_t i = 0; i < 12; ++i) {
        m_chromaSmoothed[i] = 0.0f;
    }
}

bool BreathingEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;

    // Initialize state
    m_currentRadius = 0.0f;
    m_prevRadius = 0.0f;
    m_pulseIntensity = 0.0f;
    m_phase = 0.0f;
    m_fallbackPhase = 0.0f;
    m_lastFlux = 0.0f;
    m_fluxBoost = 0.0f;
    m_texturePhase = 0.0f;
    m_energySmoothed = 0.0f;

    // Initialize history buffer
    for (uint8_t i = 0; i < HISTORY_SIZE; ++i) {
        m_radiusTargetHist[i] = 0.0f;
    }
    m_radiusTargetSum = 0.0f;
    m_histIdx = 0;

    // Initialize chromagram smoothing
    for (uint8_t i = 0; i < 12; ++i) {
        m_chromaSmoothed[i] = 0.0f;
    }

    // Register behaviors this effect supports
    m_selector.registerBehavior(plugins::VisualBehavior::BREATHE_WITH_DYNAMICS, 2.0f);
    m_selector.registerBehavior(plugins::VisualBehavior::PULSE_ON_BEAT, 1.5f);
    m_selector.registerBehavior(plugins::VisualBehavior::TEXTURE_FLOW, 1.0f);

    // Set fallback for when audio unavailable
    m_selector.setFallbackBehavior(plugins::VisualBehavior::BREATHE_WITH_DYNAMICS);

    // Smooth 400ms transitions between behaviors
    m_selector.setTransitionTime(400);

    // Reset selector state
    m_selector.reset();

    return true;
}

void BreathingEffect::render(plugins::EffectContext& ctx) {
    // Update audio behavior selector
    m_selector.update(ctx);

    // Get current behavior
    plugins::VisualBehavior behavior = m_selector.currentBehavior();

    // Dispatch to appropriate render method
    switch (behavior) {
        case plugins::VisualBehavior::PULSE_ON_BEAT:
            renderPulsing(ctx);
            break;

        case plugins::VisualBehavior::TEXTURE_FLOW:
            renderTexture(ctx);
            break;

        case plugins::VisualBehavior::BREATHE_WITH_DYNAMICS:
        default:
            renderBreathing(ctx);
            break;
    }
}

void BreathingEffect::renderBreathing(plugins::EffectContext& ctx) {
    // BLOOM_BREATHE: Sensory Bridge Pattern
    // Audio → Color/Brightness (AUDIO-REACTIVE)
    // Time → Motion Speed (TIME-BASED, USER-CONTROLLED)

    fadeToBlackBy(ctx.leds, ctx.ledCount, 15);

    // ========================================================================
    // PHASE 1: TIME-BASED MOTION (User-controlled speed, NOT audio-reactive)
    // ========================================================================
    float dt = ctx.deltaTimeMs * 0.001f;  // Convert to seconds
    float baseSpeed = ctx.speed / 200.0f;  // User speed parameter (like CONFIG.MOOD)
    m_phase += baseSpeed * dt;  // Time-based phase accumulation
    
    // Generate breathing cycle from phase (sine wave)
    float timeBasedRadius = (sinf(m_phase) * 0.5f + 0.5f) * (float)HALF_LENGTH * 0.6f;

    // ========================================================================
    // PHASE 2: AUDIO-REACTIVE COLOR & BRIGHTNESS
    // ========================================================================
    CRGB chromaticColor = CRGB::Black;
    float energyEnvelope = 0.0f;
    float brightness = 0.0f;

#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        // ====================================================================
        // Multi-Stage Smoothing Pipeline (Sensory Bridge Pattern)
        // ====================================================================
        
        // Stage 1: Smooth chromagram (0.75 alpha - fast attack/release)
        for (int i = 0; i < 12; i++) {
            float alpha = 0.75f;  // Like spectrogram_smooth in Sensory Bridge
            m_chromaSmoothed[i] = ctx.audio.controlBus.heavy_chroma[i] * alpha + 
                                  m_chromaSmoothed[i] * (1.0f - alpha);
        }
        
        // Stage 2: Smooth energy envelope (0.3 alpha - slower smoothing)
        float alpha_energy = 0.3f;  // Like magnitudes_normalized_avg in Sensory Bridge
        m_energySmoothed = ctx.audio.rms() * alpha_energy + 
                           m_energySmoothed * (1.0f - alpha_energy);
        
        // ====================================================================
        // Compute Chromatic Color from Smoothed Chromagram
        // ====================================================================
        chromaticColor = computeChromaticColor(m_chromaSmoothed, ctx);
        
        // ====================================================================
        // Compute Energy Envelope for Brightness Modulation
        // ====================================================================
        energyEnvelope = m_energySmoothed;
        brightness = energyEnvelope * energyEnvelope;  // Quadratic contrast (like Sensory Bridge)
        
        // Optional: Beat sync - reset phase on beat (optional feature)
        if (ctx.audio.isOnBeat()) {
            // Optional: Could reset phase to 0.0f for beat sync, but Sensory Bridge doesn't do this
            // m_phase = 0.0f;  // Uncomment if beat sync desired
        }
    } else
#endif
    {
        // NO AUDIO: Use default color and minimal brightness
        chromaticColor = ctx.palette.getColor(ctx.gHue, 128);
        brightness = 0.3f;  // Dim fallback
    }

    // ========================================================================
    // PHASE 3: COMBINE MOTION + AUDIO (Audio modulates size, not speed)
    // ========================================================================
    // Audio brightness modulates the radius size (0.4-1.0 range)
    // This creates breathing that expands with music, but motion speed is constant
    float targetRadius = timeBasedRadius * (0.4f + 0.6f * brightness);
    targetRadius = fmaxf(0.0f, fminf(targetRadius, (float)HALF_LENGTH));

    // ========================================================================
    // PHASE 4: ROLLING AVERAGE - Filter single-frame spikes
    // ========================================================================
    m_radiusTargetSum -= m_radiusTargetHist[m_histIdx];
    m_radiusTargetHist[m_histIdx] = targetRadius;
    m_radiusTargetSum += targetRadius;
    float avgTargetRadius = m_radiusTargetSum / (float)HISTORY_SIZE;
    m_histIdx = (m_histIdx + 1) % HISTORY_SIZE;

    // ========================================================================
    // PHASE 5: FRAME PERSISTENCE (Sensory Bridge Bloom Pattern)
    // ========================================================================
    // Alpha blending with previous frame (0.99 alpha = 99% persistence)
    // This creates smooth motion through frame accumulation, not exponential decay
    float alpha = 0.99f;  // Like Bloom's draw_sprite() alpha
    m_currentRadius = m_prevRadius * alpha + avgTargetRadius * (1.0f - alpha);
    m_prevRadius = m_currentRadius;  // Store for next frame

    // Wrap phase to prevent overflow
    if (m_phase > 2.0f * 3.14159f * 10.0f) {
        m_phase -= 2.0f * 3.14159f * 10.0f;
    }

    // ========================================================================
    // PHASE 6: RENDERING with Chromatic Color & Energy-Modulated Brightness
    // ========================================================================
    if (m_currentRadius > 0.001f) {
        for (int i = 0; i < STRIP_LENGTH; i++) {
            float dist = (float)centerPairDistance((uint16_t)i);

            // Only render within current radius
            if (dist <= m_currentRadius) {
                // Simple linear falloff (smooth propagation)
                float intensity = 1.0f - (dist / m_currentRadius) * 0.5f;
                intensity = fmaxf(0.0f, intensity);
                
                // Apply subtle exponential foreshortening for visual depth
                float normalizedDist = dist / (float)HALF_LENGTH;
                float foreshortened = powf(normalizedDist, FORESHORTEN_EXP);
                float expMod = expf(-foreshortened * 1.5f);
                intensity *= (0.7f + 0.3f * expMod);

                // Apply energy-modulated brightness (audio drives brightness, not motion)
                float finalBrightness = intensity * brightness;  // brightness from audio energy
                uint8_t ledBrightness = (uint8_t)(255.0f * finalBrightness);

                // Use chromatic color (from chromagram) instead of palette
                CRGB color = chromaticColor;
                
                // Scale color by brightness
                color.r = scale8(color.r, ledBrightness);
                color.g = scale8(color.g, ledBrightness);
                color.b = scale8(color.b, ledBrightness);

                ctx.leds[i] = color;
                if (i + STRIP_LENGTH < ctx.ledCount) {
                    ctx.leds[i + STRIP_LENGTH] = color;
                }
            }
        }
    }
    
    // ========================================================================
    // PHASE 7: SPATIAL FALLOFF (Quadratic Edge Fade - like Sensory Bridge Bloom)
    // ========================================================================
    // Fade outer 32 LEDs using quadratic curve for smooth edge
    for (uint8_t i = 0; i < 32; i++) {
        float prog = i / 31.0f;  // 0.0 to 1.0
        float falloff = prog * prog;  // Quadratic fade (like Bloom mode)
        
        // Apply to outer LEDs (from edge inward)
        uint16_t edgeIdx = STRIP_LENGTH - 1 - i;
        if (edgeIdx < ctx.ledCount) {
            ctx.leds[edgeIdx].r = scale8(ctx.leds[edgeIdx].r, (uint8_t)(255.0f * (1.0f - falloff)));
            ctx.leds[edgeIdx].g = scale8(ctx.leds[edgeIdx].g, (uint8_t)(255.0f * (1.0f - falloff)));
            ctx.leds[edgeIdx].b = scale8(ctx.leds[edgeIdx].b, (uint8_t)(255.0f * (1.0f - falloff)));
        }
    }
}

void BreathingEffect::renderPulsing(plugins::EffectContext& ctx) {
    // BLOOM_PULSE: Sharp radial expansion on beat with BPM-adaptive decay
    // Falls back to flux-driven transients when beat tracking unreliable

    fadeToBlackBy(ctx.leds, ctx.ledCount, 30);  // Faster fade for snappy feel

    float decayRate = 0.92f;  // Default: ~200ms decay

#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        float tempoConf = ctx.audio.tempoConfidence();

        if (tempoConf > 0.4f) {
            // RELIABLE BEAT TRACKING: BPM-adaptive decay
            // Clamp to 60-180 BPM to prevent wild decay rates from jitter
            float bpm = ctx.audio.bpm();
            if (bpm < 60.0f) bpm = 60.0f;   // Floor: prevents very slow decay
            if (bpm > 180.0f) bpm = 180.0f; // Cap: prevents very fast decay

            // Decay should reach ~20% by next beat
            float beatPeriodFrames = (60.0f / bpm) * 120.0f;
            decayRate = powf(0.2f, 1.0f / beatPeriodFrames);

            // Trigger pulse on beat, weighted by beat strength
            if (m_selector.isOnBeat()) {
                float strength = ctx.audio.beatStrength();
                m_pulseIntensity = fmaxf(m_pulseIntensity, strength);
            }

            // Stronger pulse on downbeat
            if (ctx.audio.isOnDownbeat()) {
                m_pulseIntensity = 1.0f;
            }
        } else {
            // UNRELIABLE BEAT: Use flux for transient detection
            float flux = ctx.audio.flux();
            float fluxDelta = flux - m_lastFlux;

            // Rising flux above threshold triggers pulse
            if (fluxDelta > 0.12f && flux > 0.2f) {
                m_pulseIntensity = fmaxf(m_pulseIntensity, fminf(flux * 1.5f, 1.0f));
            }

            m_lastFlux = flux;
        }
    }
#endif

    // Apply decay
    m_pulseIntensity *= decayRate;
    if (m_pulseIntensity < 0.01f) m_pulseIntensity = 0.0f;

    // Compute effective radius from pulse
    float pulseRadius = m_pulseIntensity * (float)HALF_LENGTH;

    // Render with exponential foreshortening
    if (pulseRadius > 1.0f) {
        for (int i = 0; i < STRIP_LENGTH; i++) {
            float dist = (float)centerPairDistance((uint16_t)i);
            float normalizedDist = dist / (float)HALF_LENGTH;

            // Exponential foreshortening: acceleration toward edges
            float foreshortened = powf(normalizedDist, FORESHORTEN_EXP);

            // Sharp exponential falloff for punchy pulse
            float intensity = m_pulseIntensity * expf(-foreshortened * 4.0f);

            // Quadratic intensity curve for extra punch
            intensity = intensity * intensity;

            if (dist <= pulseRadius * 1.5f && intensity > 0.01f) {
                uint8_t brightness = (uint8_t)(255.0f * intensity);

                // Warmer color on pulse (shift toward red)
                uint8_t hueOffset = (uint8_t)(20.0f * m_pulseIntensity);

                CRGB color = ctx.palette.getColor(
                    ctx.gHue + hueOffset,
                    brightness
                );

                ctx.leds[i] = color;
                if (i + STRIP_LENGTH < ctx.ledCount) {
                    ctx.leds[i + STRIP_LENGTH] = color;
                }
            }
        }
    }

    // Keep fallback phase moving for smooth transition back
    m_fallbackPhase += ctx.speed / 300.0f;
    if (m_fallbackPhase > 2.0f * 3.14159f * 10.0f) {
        m_fallbackPhase -= 2.0f * 3.14159f * 10.0f;
    }
}

void BreathingEffect::renderTexture(plugins::EffectContext& ctx) {
    // BLOOM_TEXTURE: Slow organic drift with audio-modulated amplitude
    // Motion is TIME-BASED (Sensory Bridge pattern), audio→amplitude only

    fadeToBlackBy(ctx.leds, ctx.ledCount, 8);  // Slow fade for dreamy feel

    // Audio-modulated wave AMPLITUDE parameters (not speed!)
    float timbralMod = 0.5f;   // Default if no audio
    float fluxMod = 0.3f;
    float rhythmMod = 0.5f;

#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        // Timbral saliency modulates wave amplitude (high-freq content → more texture)
        timbralMod = 0.3f + 0.5f * ctx.audio.timbralSaliency();

        // Flux modulates secondary wave amplitude (spectral change → wave strength)
        fluxMod = 0.2f + 0.4f * ctx.audio.flux();

        // Rhythmic saliency modulates primary wave amplitude (NOT speed!)
        rhythmMod = 0.5f + ctx.audio.rhythmicSaliency();

        // RMS provides baseline brightness modulation
        float rms = ctx.audio.rms();
        timbralMod *= 0.6f + 0.4f * rms;
    }
#endif

    // Speed is TIME-BASED only (Sensory Bridge pattern - no audio→speed coupling)
    float driftSpeed = ctx.speed / 500.0f;
    // REMOVED: driftSpeed *= rhythmMod;  // This was audio→speed coupling!

    // Multiple overlapping waves - rhythmMod now affects amplitude, not speed
    float wave1 = sinf(m_texturePhase) * timbralMod * rhythmMod;  // rhythmMod here
    float wave2 = sinf(m_texturePhase * 1.3f + 1.0f) * fluxMod;
    float wave3 = sinf(m_texturePhase * 0.7f + 2.0f) * 0.2f;

    float combinedWave = (wave1 + wave2 + wave3) / 3.0f + 0.5f;
    float waveRadius = combinedWave * (float)HALF_LENGTH * 0.7f;

    // Render with exponential foreshortening
    for (int i = 0; i < STRIP_LENGTH; i++) {
        float dist = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = dist / (float)HALF_LENGTH;

        // Exponential foreshortening
        float foreshortened = powf(normalizedDist, FORESHORTEN_EXP);

        // Soft gaussian-like falloff
        float intensity = expf(-foreshortened * foreshortened * 2.0f);

        // Modulate by wave position
        float waveInfluence = 1.0f - fabsf(dist - waveRadius) / 25.0f;
        waveInfluence = fmaxf(0.0f, waveInfluence);
        intensity = intensity * 0.5f + waveInfluence * 0.5f;

        // Apply combined wave and timbral modulation
        intensity *= combinedWave;
        intensity *= timbralMod;

        uint8_t brightness = (uint8_t)(200.0f * intensity);

        if (brightness > 10) {
            // Cooler colors for texture (blue-green shift)
            uint8_t hueOffset = (uint8_t)(dist * 0.5f);
            CRGB color = ctx.palette.getColor(
                ctx.gHue + 128 + hueOffset,
                brightness
            );

            ctx.leds[i] = color;
            if (i + STRIP_LENGTH < ctx.ledCount) {
                ctx.leds[i + STRIP_LENGTH] = color;
            }
        }
    }

    // Update texture phase
    m_texturePhase += driftSpeed;
    if (m_texturePhase > 2.0f * 3.14159f * 10.0f) {
        m_texturePhase -= 2.0f * 3.14159f * 10.0f;
    }

    // Keep fallback phase synced for smooth transitions
    m_fallbackPhase = combinedWave * 2.0f * 3.14159f;
}

void BreathingEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& BreathingEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Breathing",
        "Bloom-inspired radial expansion with robust audio reactivity",
        plugins::EffectCategory::AMBIENT,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
