/**
 * @file LGPPhotonicCrystalEffect.cpp
 * @brief Audio-reactive LGP Photonic Crystal effect implementation
 *
 * v6: PURE DSP - No Saliency/Narrative/Style/Chord Detection
 * All "musical intelligence" APIs were outputting corrupted, unusable data.
 *
 * Audio-visual mapping (Direct DSP only):
 * - Lattice size ↔ heavyBass() breathing (6-14 range)
 * - Bandgap ratio ↔ heavyMid() modulation (inverse - more mid = tighter gap)
 * - Motion speed ↔ flux() + hi-hat burst (slew-limited)
 * - Brightness ↔ rms() + K1 beat-locked pulse (gated by tempoConfidence)
 * - Shimmer ↔ heavyTreble() (was timbralSaliency - CORRUPTED)
 * - Hue split ↔ flux() modulation (was chord type - CORRUPTED)
 * - Onset pulse ↔ snare/hihat (percussion triggers only)
 */

#include "LGPPhotonicCrystalEffect.h"
#include "../CoreEffects.h"
#include "../enhancement/MotionEngine.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

// Beat pulse parameters
static constexpr float BEAT_PULSE_STRENGTH = 0.35f;  // 35% brightness boost on beat

// Treble shimmer strength
static constexpr float TIMBRAL_SHIMMER_STRENGTH = 60.0f;

LGPPhotonicCrystalEffect::LGPPhotonicCrystalEffect()
    : m_phase(0.0f)
{
}

bool LGPPhotonicCrystalEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;

    // Initialize phase
    m_phase = 0.0f;

    // Reset smoothing followers to initial values
    m_latticeFollower.reset(8.0f);
    m_bandgapFollower.reset(0.5f);
    m_brightnessFollower.reset(0.8f);
    m_beatPulseDecay.reset(0.0f);
    m_hueSplitFollower.reset(48.0f);
    m_trebleFollower.reset(0.0f);  // v6: Renamed from timbralFollower
    m_breathingFollower.reset(0.0f);

    // v6: Initialize new state variables
    m_speedBurst = 1.0f;
    m_beatPulse = 0.0f;

    // Initialize spring for phase speed with critical damping
    // stiffness=50 gives responsive but smooth following
    m_phaseSpeedSpring.init(50.0f, 1.0f);
    m_phaseSpeedSpring.reset(1.0f);

    // Initialize history buffers (spike filtering)
    for (uint8_t i = 0; i < HISTORY_SIZE; ++i) {
        m_latticeTargetHist[i] = 8.0f;
        m_bandgapTargetHist[i] = 0.5f;
    }
    m_latticeTargetSum = 8.0f * HISTORY_SIZE;
    m_bandgapTargetSum = 0.5f * HISTORY_SIZE;
    m_histIdx = 0;

    // Hop tracking
    m_lastHopSeq = 0;

    // WAVE COLLISION PATTERN: Initialize energy history buffers
    for (uint8_t i = 0; i < ENERGY_HISTORY; ++i) {
        m_energyHist[i] = 0.0f;
    }
    m_energySum = 0.0f;
    m_energyHistIdx = 0;
    m_energyAvg = 0.0f;
    m_energyDelta = 0.0f;
    m_energyAvgSmooth = 0.0f;
    m_energyDeltaSmooth = 0.0f;

    // Slew-limited speed
    m_speedScaleSmooth = 1.0f;

    // Onset pulse
    m_onsetPulse = 0.0f;

    // v6: AudioBehaviorSelector REMOVED - narrative phase detection was CORRUPTED

    return true;
}

void LGPPhotonicCrystalEffect::render(plugins::EffectContext& ctx) {
    // ========================================================================
    // CRITICAL: Use safe delta time (clamped for physics stability)
    // ========================================================================
    float dt = ctx.getSafeDeltaSeconds();
    float moodNorm = ctx.getMoodNormalized();

    // ========================================================================
    // v6: PURE DSP - Base parameters (no narrative phase - was CORRUPTED)
    // All parameter targets computed from DIRECT DSP signals only
    // ========================================================================
    float targetLattice = 8.0f;       // Will be modulated by heavyBass()
    float targetBandgap = 0.5f;       // Will be modulated by heavyMid()
    float targetBrightness = 0.8f;    // Will be modulated by rms() + beat pulse
    float phaseSpeedMult = 1.0f;      // Will be modulated by flux() + hi-hat burst
    float energyBrightnessScale = 1.0f;
    float targetHueSplit = 48.0f;     // Will be modulated by flux()

#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        // ================================================================
        // MOOD ADJUSTMENT - Adjust time constants per hop
        // ================================================================
        bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;

            // Mood 0 = reactive: fast attack, fast decay
            // Mood 255 = smooth: slow attack, slow decay
            // Adjust follower time constants based on mood
            float riseMult = 1.0f + moodNorm * 0.5f;   // 1.0x to 1.5x slower rise
            float fallMult = 0.6f + moodNorm * 0.8f;   // 0.6x to 1.4x fall adjustment

            m_latticeFollower.riseTau = 0.15f * riseMult;
            m_latticeFollower.fallTau = 0.40f * fallMult;
            m_bandgapFollower.riseTau = 0.15f * riseMult;
            m_bandgapFollower.fallTau = 0.40f * fallMult;

            // Beat pulse decay: slower at higher mood for dreamier feel
            // lambda = 12 at mood 0, lambda = 6 at mood 255
            m_beatPulseDecay.lambda = 12.0f - 6.0f * moodNorm;
        }

        // ================================================================
        // v6: STYLE DETECTION REMOVED - was outputting CORRUPTED data
        // ================================================================

        // ================================================================
        // v6: PURE DSP - Lattice Size from heavyBass()
        // Direct bass energy → lattice breathing (range: 6-14)
        // ================================================================
        float bassEnergy = ctx.audio.heavyBass();
        targetLattice = 6.0f + bassEnergy * 8.0f;

        // ================================================================
        // v6: PURE DSP - Bandgap from heavyMid() (INVERTED)
        // More mid-frequency energy = tighter bandgap = more complex texture
        // ================================================================
        float midEnergy = ctx.audio.heavyMid();
        targetBandgap = 0.6f - midEnergy * 0.3f;  // Range: 0.3-0.6

        // ================================================================
        // v6: PURE DSP - Speed from flux() + hi-hat burst
        // dynamicSaliency REMOVED - was outputting CORRUPTED data
        // ================================================================
        float flux = ctx.audio.flux();

        // Base speed from flux (range: 0.5 to 1.5)
        float rawSpeedTarget = 0.5f + flux * 1.0f;

        // Hi-hat speed burst (Wave Collision pattern)
        if (ctx.audio.isHihatHit()) {
            m_speedBurst = 1.6f;  // Temporary boost
        }
        // Decay toward baseline (0.95 decay per frame + 0.05 toward 1.0)
        m_speedBurst = m_speedBurst * 0.95f + 1.0f * 0.05f;
        rawSpeedTarget *= m_speedBurst;

        // Slew limit to 0.25 units/sec max change rate
        const float maxSlewRate = 0.25f;
        float slewLimit = maxSlewRate * dt;
        float speedDelta = rawSpeedTarget - m_speedScaleSmooth;
        if (speedDelta > slewLimit) speedDelta = slewLimit;
        if (speedDelta < -slewLimit) speedDelta = -slewLimit;
        m_speedScaleSmooth += speedDelta;

        // Use slew-limited speed for subsequent processing
        phaseSpeedMult = m_speedScaleSmooth;

        // ================================================================
        // WAVE COLLISION PATTERN: Onset Detection via Relative Energy
        // Uses energy delta (above-average) instead of flux (too sensitive)
        // Spatial decay applied in render loop for center-focused flash
        // ================================================================
        constexpr float DELTA_THRESHOLD = 0.15f;  // Above-average threshold

        // Trigger on significant energy spike OR percussive hit
        bool energySpike = m_energyDelta > DELTA_THRESHOLD;
        bool percussiveHit = ctx.audio.isSnareHit() || ctx.audio.isHihatHit();

        if (energySpike || percussiveHit) {
            // Percussion gets full strength, energy spike scales with delta
            float onsetStrength = percussiveHit ? 1.0f : fminf(m_energyDelta / 0.3f, 1.0f);
            if (onsetStrength > m_onsetPulse) {
                m_onsetPulse = onsetStrength;
            }
        }

        // Exponential decay (0.88 per frame = ~85ms half-life at 60fps)
        m_onsetPulse *= 0.88f;

        // Transfer to existing decay system for rendering
        m_beatPulseDecay.value = m_onsetPulse;

        // ================================================================
        // v6: PURE DSP - Brightness from rms() + K1 beat-locked pulse
        // ================================================================
        float rmsEnergy = ctx.audio.rms();
        float baseIntensity = 0.4f + 0.5f * rmsEnergy;

        // K1 beat-locked pulse (gated by tempo confidence)
        if (ctx.audio.tempoConfidence() > 0.5f && ctx.audio.isOnBeat()) {
            m_beatPulse = ctx.audio.beatStrength();
        }
        m_beatPulse *= 0.88f;  // Decay per frame

        energyBrightnessScale = baseIntensity + 0.3f * m_beatPulse;

        // ================================================================
        // v6: PURE DSP - Shimmer from heavyTreble()
        // timbralSaliency REMOVED - was outputting CORRUPTED data
        // ================================================================
        float trebleEnergy = ctx.audio.heavyTreble();
        m_trebleFollower.update(trebleEnergy, dt);

        // ================================================================
        // v6: PURE DSP - Hue Split from flux()
        // Chord detection REMOVED - was outputting CORRUPTED data
        // ================================================================
        targetHueSplit = 48.0f + flux * 32.0f;  // Range: 48-80
    }
#endif

    // Clamp targets
    if (targetLattice < 4.0f) targetLattice = 4.0f;
    if (targetLattice > 16.0f) targetLattice = 16.0f;
    if (targetBandgap < 0.25f) targetBandgap = 0.25f;
    if (targetBandgap > 0.75f) targetBandgap = 0.75f;
    if (targetBrightness < 0.4f) targetBrightness = 0.4f;
    if (targetBrightness > 1.0f) targetBrightness = 1.0f;

    // ========================================================================
    // ROLLING AVERAGE - Filter single-frame audio spikes
    // ========================================================================
    m_latticeTargetSum -= m_latticeTargetHist[m_histIdx];
    m_latticeTargetHist[m_histIdx] = targetLattice;
    m_latticeTargetSum += targetLattice;
    float avgLatticeTarget = m_latticeTargetSum / HISTORY_SIZE;

    m_bandgapTargetSum -= m_bandgapTargetHist[m_histIdx];
    m_bandgapTargetHist[m_histIdx] = targetBandgap;
    m_bandgapTargetSum += targetBandgap;
    float avgBandgapTarget = m_bandgapTargetSum / HISTORY_SIZE;

    m_histIdx = (m_histIdx + 1) % HISTORY_SIZE;

    // ========================================================================
    // TRUE EXPONENTIAL SMOOTHING via SmoothingEngine
    // Formula: alpha = 1.0f - expf(-dt / tau) - Frame-rate INDEPENDENT
    // ========================================================================

    // Structure smoothing with mood adjustment
    float latticeSize = m_latticeFollower.updateWithMood(avgLatticeTarget, dt, moodNorm);
    float bandgapRatio = m_bandgapFollower.updateWithMood(avgBandgapTarget, dt, moodNorm);

    // Brightness smoothing (fast response for beat reactivity)
    float brightnessMod = m_brightnessFollower.update(targetBrightness, dt);

    // Beat pulse: TRUE exponential decay toward zero
    float beatPulse = m_beatPulseDecay.update(0.0f, dt);

    // Hue split smoothing
    float hueSplit = m_hueSplitFollower.update(targetHueSplit, dt);

    // Phase speed: SPRING PHYSICS for natural momentum (no lurching)
    float smoothedPhaseSpeed = m_phaseSpeedSpring.update(phaseSpeedMult, dt);

    // v6: Compute final brightness with energy scaling + onset pulse
    float finalBrightness = brightnessMod * energyBrightnessScale + BEAT_PULSE_STRENGTH * beatPulse;
    if (finalBrightness > 1.0f) finalBrightness = 1.0f;
    if (finalBrightness < 0.15f) finalBrightness = 0.15f;  // Never fully dark

    // Integer lattice size for rendering
    uint8_t latticeSizeInt = (uint8_t)(latticeSize + 0.5f);
    if (latticeSizeInt < 4) latticeSizeInt = 4;
    if (latticeSizeInt > 16) latticeSizeInt = 16;

    // Bandgap threshold
    uint8_t bandgapThreshold = (uint8_t)(latticeSizeInt * bandgapRatio);
    if (bandgapThreshold < 1) bandgapThreshold = 1;

    // Phase advancement with smoothed speed
    float speedNorm = ctx.speed / 50.0f;
    m_phase = lightwaveos::enhancement::advancePhase(m_phase, speedNorm, smoothedPhaseSpeed, dt);

    uint16_t phaseInt = (uint16_t)(m_phase * 256.0f);

    // Integer hue split
    uint8_t hueSplitInt = (uint8_t)(hueSplit + 0.5f);

    // v6: Get smoothed treble value for shimmer (was timbralSaliency - CORRUPTED)
    float trebleSmooth = m_trebleFollower.value;

    // ========================================================================
    // RENDER the photonic crystal pattern
    // ========================================================================
    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        // CENTER ORIGIN: distance from center
        uint16_t distFromCenter = centerPairDistance(i);

        // Periodic structure with audio-modulated lattice size
        uint8_t cellPosition = (uint8_t)(distFromCenter % latticeSizeInt);
        bool inBandgap = cellPosition < bandgapThreshold;

        // Photonic band structure
        uint8_t brightness = 0;
        if (inBandgap) {
            // Allowed modes - outward from center
            brightness = sin8((distFromCenter << 2) - (phaseInt >> 7));

#if FEATURE_AUDIO_SYNC
            // v6: Treble shimmer (was timbralSaliency - now heavyTreble)
            if (ctx.audio.available) {
                float distMod = 1.0f - (float)distFromCenter / HALF_LENGTH;
                brightness = qadd8(brightness, (uint8_t)(trebleSmooth * distMod * TIMBRAL_SHIMMER_STRENGTH));
            }
#endif
        } else {
            // Forbidden gap - evanescent decay
            uint8_t decayFactor = (latticeSizeInt > bandgapThreshold)
                ? (cellPosition - bandgapThreshold) * (200 / (latticeSizeInt - bandgapThreshold))
                : 0;
            uint8_t decay = (decayFactor < 255) ? (255 - decayFactor) : 0;
            brightness = scale8(sin8((distFromCenter << 1) - (phaseInt >> 8)), decay);
        }

        // Apply brightness modulation
        brightness = scale8(brightness, (uint8_t)(ctx.brightness * finalBrightness));

#if FEATURE_AUDIO_SYNC
        // ================================================================
        // WAVE COLLISION PATTERN: Spatial Decay on Onset Pulse
        // Center-focused explosion that fades toward edges
        // exp(-dist * 0.12f) creates natural falloff from center
        // ================================================================
        if (ctx.audio.available && m_onsetPulse > 0.01f) {
            float collisionFlash = m_onsetPulse * expf(-(float)distFromCenter * 0.12f);
            brightness = qadd8(brightness, (uint8_t)(collisionFlash * 60.0f));
        }
#endif

        // Spatial gradient for palette (no rainbows)
        uint8_t palettePos = (distFromCenter * 3);

        // Hue split for forbidden zones
        if (!inBandgap) {
            palettePos += hueSplitInt;
        }

        // Slow gHue drift
        palettePos += (ctx.gHue >> 2);

        // Render to both strips
        ctx.leds[i] = ctx.palette.getColor(palettePos, brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(
                (uint8_t)(palettePos + 64),
                brightness
            );
        }
    }
}

void LGPPhotonicCrystalEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPPhotonicCrystalEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Photonic Crystal",
        "v6: Pure DSP - heavyBass/Mid/Treble, flux speed, K1 beat pulse, no saliency",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
