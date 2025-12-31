/**
 * @file TransitionEngine.cpp
 * @brief CENTER ORIGIN-compliant transitions with per-strip processing
 *
 * LightwaveOS v2 - Transition System (REWRITTEN)
 *
 * Key fixes from v1:
 * 1. Per-strip explicit processing (not single loop with modulo)
 * 2. Stateful physics where appropriate (velocity, decay)
 * 3. Simplified to 4 core transitions first
 */

#include "TransitionEngine.h"
#include <Arduino.h>

namespace lightwaveos {
namespace transitions {

// ==================== Constructor ====================

TransitionEngine::TransitionEngine()
    : m_active(false)
    , m_progress(0.0f)
    , m_rawProgress(0.0f)
    , m_type(TransitionType::FADE)
    , m_curve(EasingCurve::IN_OUT_QUAD)
    , m_startTime(0)
    , m_durationMs(1000)
    , m_outputBuffer(nullptr)
    , m_activePulses(0)
    , m_irisRadius(0)
    , m_shockwaveRadius(0)
    , m_radiationIntensity(0)
    , m_eventHorizonRadius(0)
    , m_chevronAngle(0)
    , m_foldCount(6)
    , m_rotationAngle(0)
{
    memset(m_sourceBuffer, 0, sizeof(m_sourceBuffer));
    memset(m_targetBuffer, 0, sizeof(m_targetBuffer));  // Now an array
    memset(m_dissolveOrder, 0, sizeof(m_dissolveOrder));
    memset(m_particles, 0, sizeof(m_particles));
    memset(m_pulses, 0, sizeof(m_pulses));
    memset(m_ringPhases, 0, sizeof(m_ringPhases));
}

// ==================== Transition Control ====================

void TransitionEngine::startTransition(const CRGB* sourceBuffer,
                                        const CRGB* targetBuffer,
                                        CRGB* outputBuffer,
                                        TransitionType type,
                                        uint16_t durationMs,
                                        EasingCurve curve) {
    // Copy BOTH buffers to prevent aliasing issues
    // (source and target may point to same output buffer)
    memcpy(m_sourceBuffer, sourceBuffer, sizeof(m_sourceBuffer));
    memcpy(m_targetBuffer, targetBuffer, sizeof(m_targetBuffer));
    m_outputBuffer = outputBuffer;
    m_type = type;
    m_durationMs = durationMs;
    m_curve = curve;
    m_startTime = millis();
    m_progress = 0.0f;
    m_rawProgress = 0.0f;
    m_active = true;

    // Initialize effect-specific state
    switch (type) {
        case TransitionType::DISSOLVE:
            initDissolve();
            break;
        case TransitionType::PULSEWAVE:
            initPulsewave();
            break;
        case TransitionType::IMPLOSION:
            initImplosion();
            break;
        case TransitionType::IRIS:
            m_irisRadius = 0.0f;  // Starts closed
            break;
        case TransitionType::NUCLEAR:
            initNuclear();
            break;
        case TransitionType::STARGATE:
            initStargate();
            break;
        case TransitionType::KALEIDOSCOPE:
            initKaleidoscope();
            break;
        case TransitionType::MANDALA:
            initMandala();
            break;
        case TransitionType::PHASE_SHIFT:
            // No special init needed - uses progress directly
            break;
        default:
            break;
    }

    Serial.printf("[Transition] Started: %s (%dms)\n",
                  getTransitionName(type), durationMs);
}

void TransitionEngine::startTransition(const CRGB* sourceBuffer,
                                        const CRGB* targetBuffer,
                                        CRGB* outputBuffer,
                                        TransitionType type) {
    uint16_t duration = getDefaultDuration(type);
    EasingCurve curve = static_cast<EasingCurve>(getDefaultEasing(type));
    startTransition(sourceBuffer, targetBuffer, outputBuffer, type, duration, curve);
}

bool TransitionEngine::update() {
    if (!m_active) return false;

    // Calculate elapsed time
    uint32_t elapsed = millis() - m_startTime;

    // Check completion
    if (elapsed >= m_durationMs) {
        // Copy target to output (final frame)
        memcpy(m_outputBuffer, m_targetBuffer, TOTAL_LEDS * sizeof(CRGB));
        m_active = false;
        m_progress = 1.0f;
        Serial.println("[Transition] Complete");
        return false;
    }

    // Calculate progress with easing
    m_rawProgress = (float)elapsed / (float)m_durationMs;
    m_progress = ease(m_rawProgress, m_curve);

    // Apply transition effect - all 12 types implemented
    switch (m_type) {
        case TransitionType::FADE:
            applyFade();
            break;
        case TransitionType::WIPE_OUT:
            applyWipeOut();
            break;
        case TransitionType::WIPE_IN:
            applyWipeIn();
            break;
        case TransitionType::DISSOLVE:
            applyDissolve();
            break;
        case TransitionType::PHASE_SHIFT:
            applyPhaseShift();
            break;
        case TransitionType::PULSEWAVE:
            applyPulsewave();
            break;
        case TransitionType::IMPLOSION:
            applyImplosion();
            break;
        case TransitionType::IRIS:
            applyIris();
            break;
        case TransitionType::NUCLEAR:
            applyNuclear();
            break;
        case TransitionType::STARGATE:
            applyStargate();
            break;
        case TransitionType::KALEIDOSCOPE:
            applyKaleidoscope();
            break;
        case TransitionType::MANDALA:
            applyMandala();
            break;
        default:
            applyFade();
            break;
    }

    return true;
}

void TransitionEngine::cancel() {
    if (m_active && m_outputBuffer) {
        memcpy(m_outputBuffer, m_targetBuffer, TOTAL_LEDS * sizeof(CRGB));
    }
    m_active = false;
    m_progress = 1.0f;
}

// ==================== State Queries ====================

uint32_t TransitionEngine::getElapsedMs() const {
    if (!m_active) return m_durationMs;
    return millis() - m_startTime;
}

uint32_t TransitionEngine::getRemainingMs() const {
    if (!m_active) return 0;
    uint32_t elapsed = millis() - m_startTime;
    return (elapsed >= m_durationMs) ? 0 : (m_durationMs - elapsed);
}

TransitionType TransitionEngine::getRandomTransition() {
    // All 12 transitions now working - weighted distribution
    uint8_t r = random8(100);

    // Core transitions (60% - most reliable)
    if (r < 15) return TransitionType::FADE;
    if (r < 30) return TransitionType::WIPE_OUT;
    if (r < 45) return TransitionType::WIPE_IN;
    if (r < 60) return TransitionType::DISSOLVE;

    // Physics transitions (25% - visually impressive)
    if (r < 68) return TransitionType::PULSEWAVE;
    if (r < 76) return TransitionType::IMPLOSION;
    if (r < 85) return TransitionType::IRIS;

    // Epic transitions (15% - for special moments)
    if (r < 89) return TransitionType::NUCLEAR;
    if (r < 93) return TransitionType::STARGATE;
    if (r < 96) return TransitionType::PHASE_SHIFT;
    if (r < 98) return TransitionType::KALEIDOSCOPE;
    return TransitionType::MANDALA;
}

// ==================== Helper Methods ====================

CRGB TransitionEngine::lerpColor(const CRGB& from, const CRGB& to, uint8_t blend) const {
    return ::blend(from, to, blend);  // FastLED's optimized blend
}

// ==================== Initializers ====================

void TransitionEngine::initDissolve() {
    // Fisher-Yates shuffle for random pixel order
    for (uint16_t i = 0; i < TOTAL_LEDS; i++) {
        m_dissolveOrder[i] = i;
    }
    for (uint16_t i = TOTAL_LEDS - 1; i > 0; i--) {
        uint16_t j = random16(i + 1);
        uint16_t temp = m_dissolveOrder[i];
        m_dissolveOrder[i] = m_dissolveOrder[j];
        m_dissolveOrder[j] = temp;
    }
}

void TransitionEngine::initImplosion() {
    // Spawn particles at edges, will converge toward center
    for (uint8_t i = 0; i < MAX_PARTICLES; i++) {
        m_particles[i].active = true;
        m_particles[i].strip = i % 2;  // Alternate strips

        // Spawn at random positions along the edges (0-30 or 130-159)
        bool leftEdge = (i % 4) < 2;
        if (leftEdge) {
            m_particles[i].position = random8(30);  // 0-29
        } else {
            m_particles[i].position = 130 + random8(30);  // 130-159
        }

        // Initial velocity toward center (will accelerate)
        m_particles[i].velocity = 0.5f + (random8(50) / 100.0f);
    }
}

void TransitionEngine::initPulsewave() {
    // Initialize concentric pulse rings
    m_activePulses = 0;
    for (uint8_t i = 0; i < MAX_PULSES; i++) {
        m_pulses[i].active = false;
        m_pulses[i].radius = 0.0f;
        m_pulses[i].intensity = 0.0f;
    }
}

void TransitionEngine::initNuclear() {
    // Chain reaction starts at center
    m_shockwaveRadius = 0.0f;
    m_radiationIntensity = 1.0f;  // Full intensity at start
}

void TransitionEngine::initStargate() {
    // Portal event horizon starts closed
    m_eventHorizonRadius = 0.0f;
    m_chevronAngle = 0.0f;
}

void TransitionEngine::initKaleidoscope() {
    // Initialize symmetric fold pattern
    m_foldCount = 6;  // 6-fold symmetry
    m_rotationAngle = 0.0f;
}

void TransitionEngine::initMandala() {
    // Initialize sacred geometry ring phases
    for (uint8_t i = 0; i < 5; i++) {
        m_ringPhases[i] = (float)i * 0.2f;  // Staggered phases
    }
}

// ==================== Core Transitions ====================

void TransitionEngine::applyFade() {
    // CENTER ORIGIN: Fade radiates outward from center
    // Process each strip explicitly (v1 pattern)

    for (uint16_t strip = 0; strip < 2; strip++) {
        uint16_t offset = strip * STRIP_LENGTH;

        for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
            // Calculate distance from center (LED 79)
            float distFromCenter = abs((int)i - (int)CENTER_POINT) / (float)CENTER_POINT;

            // Progress modulated by distance: center leads, edges follow
            // At progress=0.5: center is at 100%, edges are at 0%
            float localProgress = m_progress * 2.0f - distFromCenter;
            localProgress = constrain(localProgress, 0.0f, 1.0f);

            uint8_t blend = (uint8_t)(localProgress * 255.0f);
            m_outputBuffer[offset + i] = lerpColor(
                m_sourceBuffer[offset + i],
                m_targetBuffer[offset + i],
                blend
            );
        }
    }
}

void TransitionEngine::applyWipeOut() {
    // CENTER ORIGIN: Circular wipe expanding from center to edges
    // Process each strip explicitly

    for (uint16_t strip = 0; strip < 2; strip++) {
        uint16_t offset = strip * STRIP_LENGTH;

        for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
            // Calculate distance from center (LED 79)
            float distFromCenter = abs((int)i - (int)CENTER_POINT) / (float)CENTER_POINT;

            // Wipe radius expands from 0 to 1
            float wipeRadius = m_progress;

            // Soft edge for smoother transition
            float edgeWidth = 0.1f;
            float edgeStart = wipeRadius - edgeWidth;

            if (distFromCenter < edgeStart) {
                // Inside wipe - show target
                m_outputBuffer[offset + i] = m_targetBuffer[offset + i];
            } else if (distFromCenter < wipeRadius) {
                // On edge - blend
                float blend = 1.0f - (distFromCenter - edgeStart) / edgeWidth;
                m_outputBuffer[offset + i] = lerpColor(
                    m_sourceBuffer[offset + i],
                    m_targetBuffer[offset + i],
                    (uint8_t)(blend * 255.0f)
                );
            } else {
                // Outside wipe - show source
                m_outputBuffer[offset + i] = m_sourceBuffer[offset + i];
            }
        }
    }
}

void TransitionEngine::applyWipeIn() {
    // CENTER ORIGIN: Circular wipe collapsing from edges to center
    // Process each strip explicitly

    for (uint16_t strip = 0; strip < 2; strip++) {
        uint16_t offset = strip * STRIP_LENGTH;

        for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
            // Calculate distance from center (LED 79)
            float distFromCenter = abs((int)i - (int)CENTER_POINT) / (float)CENTER_POINT;

            // Wipe radius collapses from 1 to 0
            float wipeRadius = 1.0f - m_progress;

            // Soft edge for smoother transition
            float edgeWidth = 0.1f;
            float edgeEnd = wipeRadius + edgeWidth;

            if (distFromCenter > edgeEnd) {
                // Outside wipe (already passed) - show target
                m_outputBuffer[offset + i] = m_targetBuffer[offset + i];
            } else if (distFromCenter > wipeRadius) {
                // On edge - blend
                float blend = (distFromCenter - wipeRadius) / edgeWidth;
                m_outputBuffer[offset + i] = lerpColor(
                    m_sourceBuffer[offset + i],
                    m_targetBuffer[offset + i],
                    (uint8_t)(blend * 255.0f)
                );
            } else {
                // Inside wipe (not reached yet) - show source
                m_outputBuffer[offset + i] = m_sourceBuffer[offset + i];
            }
        }
    }
}

void TransitionEngine::applyDissolve() {
    // Random pixel transition (order-agnostic, CENTER ORIGIN compatible)
    uint16_t pixelsToShow = (uint16_t)(m_progress * TOTAL_LEDS);

    // Start with source, then reveal target pixels in random order
    memcpy(m_outputBuffer, m_sourceBuffer, TOTAL_LEDS * sizeof(CRGB));

    for (uint16_t i = 0; i < pixelsToShow; i++) {
        uint16_t idx = m_dissolveOrder[i];
        m_outputBuffer[idx] = m_targetBuffer[idx];
    }
}

// ==================== Advanced Physics-Based Transitions ====================

void TransitionEngine::applyPhaseShift() {
    // CENTER ORIGIN: Frequency-based morph between source and target
    // Creates standing wave interference pattern that transitions colors

    // Phase accumulates faster than linear progress for wave effect
    float phase = m_progress * PI * 4.0f;  // Multiple wave cycles during transition

    for (uint16_t strip = 0; strip < 2; strip++) {
        uint16_t offset = strip * STRIP_LENGTH;

        for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
            // Distance from center normalized to 0-1
            float distFromCenter = abs((int)i - (int)CENTER_POINT) / (float)CENTER_POINT;

            // Create frequency-modulated blend factor
            // Wave travels outward from center with increasing frequency
            float waveFreq = 3.0f + distFromCenter * 5.0f;  // Frequency increases toward edges
            float wavePhase = phase - distFromCenter * PI * 2.0f;  // Phase delay by distance

            // Sinusoidal modulation creates "morphing" appearance
            float sinMod = (sinf(wavePhase * waveFreq) + 1.0f) * 0.5f;

            // Blend factor combines progress with wave modulation
            // Early: mostly source with wave ripples of target
            // Late: mostly target with wave ripples of source
            float blendBase = m_progress;
            float waveInfluence = 0.3f * (1.0f - fabsf(m_progress - 0.5f) * 2.0f);  // Max at midpoint
            float blend = blendBase + (sinMod - 0.5f) * waveInfluence;
            blend = constrain(blend, 0.0f, 1.0f);

            uint8_t blendByte = (uint8_t)(blend * 255.0f);
            m_outputBuffer[offset + i] = lerpColor(
                m_sourceBuffer[offset + i],
                m_targetBuffer[offset + i],
                blendByte
            );
        }
    }
}

void TransitionEngine::applyPulsewave() {
    // CENTER ORIGIN: Concentric energy pulses expanding from center
    // Each pulse carries the target effect outward like ripples

    // Spawn new pulses at intervals during first 70% of transition
    if (m_rawProgress < 0.7f) {
        float spawnThreshold = m_activePulses * 0.15f;  // Spawn every ~15% progress
        if (m_rawProgress > spawnThreshold && m_activePulses < MAX_PULSES) {
            m_pulses[m_activePulses].active = true;
            m_pulses[m_activePulses].radius = 0.0f;
            m_pulses[m_activePulses].intensity = 1.0f;
            m_activePulses++;
        }
    }

    // Update pulse physics - expand outward with velocity
    float pulseVelocity = 2.0f;  // Radius units per progress unit
    float pulseDecay = 0.15f;     // Intensity decay rate
    float pulseWidth = 15.0f;     // Width of pulse ring in LED units

    for (uint8_t p = 0; p < MAX_PULSES; p++) {
        if (m_pulses[p].active) {
            // Expand radius
            m_pulses[p].radius += pulseVelocity * (m_progress - (p * 0.15f));
            if (m_pulses[p].radius < 0) m_pulses[p].radius = 0;

            // Decay intensity as pulse expands
            m_pulses[p].intensity = 1.0f - (m_pulses[p].radius / (float)CENTER_POINT) * pulseDecay;
            if (m_pulses[p].intensity < 0) m_pulses[p].intensity = 0;
        }
    }

    // Render: Start with source, add pulse rings that reveal target
    for (uint16_t strip = 0; strip < 2; strip++) {
        uint16_t offset = strip * STRIP_LENGTH;

        for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
            float distFromCenter = (float)abs((int)i - (int)CENTER_POINT);

            // Calculate blend from all active pulses
            float totalBlend = 0.0f;

            for (uint8_t p = 0; p < m_activePulses; p++) {
                if (!m_pulses[p].active) continue;

                float pulseRadius = m_pulses[p].radius * (float)CENTER_POINT;
                float distFromPulse = fabsf(distFromCenter - pulseRadius);

                // Gaussian-like falloff within pulse width
                if (distFromPulse < pulseWidth) {
                    float pulseStrength = 1.0f - (distFromPulse / pulseWidth);
                    pulseStrength *= pulseStrength;  // Square for sharper edges
                    pulseStrength *= m_pulses[p].intensity;

                    // Pulse reveals target behind it
                    if (distFromCenter < pulseRadius) {
                        totalBlend = fmaxf(totalBlend, pulseStrength + (1.0f - pulseStrength) * totalBlend);
                    } else {
                        totalBlend = fmaxf(totalBlend, pulseStrength * 0.5f);
                    }
                }

                // Everything inside passed pulses becomes target
                if (distFromCenter < pulseRadius - pulseWidth * 0.5f) {
                    totalBlend = fmaxf(totalBlend, m_pulses[p].intensity);
                }
            }

            // Progress floor - transition must complete
            totalBlend = fmaxf(totalBlend, m_progress * m_progress);
            totalBlend = constrain(totalBlend, 0.0f, 1.0f);

            uint8_t blendByte = (uint8_t)(totalBlend * 255.0f);
            m_outputBuffer[offset + i] = lerpColor(
                m_sourceBuffer[offset + i],
                m_targetBuffer[offset + i],
                blendByte
            );
        }
    }
}

void TransitionEngine::applyImplosion() {
    // CENTER ORIGIN: Particles at edges collapse toward center
    // As particles pass LEDs, they flip from source to target

    // Physics constants
    float gravity = 0.03f;      // Acceleration toward center
    float maxVelocity = 8.0f;   // Terminal velocity

    // Update particle physics
    for (uint8_t p = 0; p < MAX_PARTICLES; p++) {
        if (!m_particles[p].active) continue;

        float pos = m_particles[p].position;
        float distToCenter = fabsf(pos - (float)CENTER_POINT);

        // Acceleration toward center (stronger when far)
        float accel = gravity * (distToCenter / (float)CENTER_POINT + 0.5f);

        // Apply acceleration based on which side of center
        if (pos < CENTER_POINT) {
            m_particles[p].velocity += accel;
        } else {
            m_particles[p].velocity -= accel;
        }

        // Clamp velocity
        m_particles[p].velocity = constrain(m_particles[p].velocity, -maxVelocity, maxVelocity);

        // Update position
        if (pos < CENTER_POINT) {
            m_particles[p].position += m_particles[p].velocity;
        } else {
            m_particles[p].position -= fabsf(m_particles[p].velocity);
        }

        // Deactivate when reaching center
        if (fabsf(m_particles[p].position - CENTER_POINT) < 3.0f) {
            m_particles[p].active = false;
        }
    }

    // Render: Track furthest particle from center per strip
    float furthestLeft[2] = {0.0f, 0.0f};
    float furthestRight[2] = {(float)STRIP_LENGTH - 1, (float)STRIP_LENGTH - 1};

    for (uint8_t p = 0; p < MAX_PARTICLES; p++) {
        if (!m_particles[p].active) continue;
        uint8_t s = m_particles[p].strip;
        float pos = m_particles[p].position;

        if (pos < CENTER_POINT) {
            furthestLeft[s] = fmaxf(furthestLeft[s], pos);
        } else {
            furthestRight[s] = fminf(furthestRight[s], pos);
        }
    }

    // Process both strips
    for (uint16_t strip = 0; strip < 2; strip++) {
        uint16_t offset = strip * STRIP_LENGTH;

        for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
            float blend = 0.0f;

            // LEDs between the particle fronts show target (collapsed area)
            if (i >= (uint16_t)furthestLeft[strip] && i <= (uint16_t)furthestRight[strip]) {
                // Inside the collapsing region - blend based on proximity to center
                float distFromCenter = fabsf((float)i - (float)CENTER_POINT);
                float collapseProgress = 1.0f - (distFromCenter / (float)CENTER_POINT);
                blend = collapseProgress * m_progress * 2.0f;
            } else {
                // Outside - particles have passed, show target
                blend = 1.0f;
            }

            // Add particle glow effect
            for (uint8_t p = 0; p < MAX_PARTICLES; p++) {
                if (!m_particles[p].active || m_particles[p].strip != strip) continue;

                float distToParticle = fabsf((float)i - m_particles[p].position);
                if (distToParticle < 5.0f) {
                    // Bright glow at particle position
                    float glow = 1.0f - (distToParticle / 5.0f);
                    blend = fmaxf(blend, glow);
                }
            }

            // Ensure completion
            blend = fmaxf(blend, m_progress);
            blend = constrain(blend, 0.0f, 1.0f);

            uint8_t blendByte = (uint8_t)(blend * 255.0f);
            m_outputBuffer[offset + i] = lerpColor(
                m_sourceBuffer[offset + i],
                m_targetBuffer[offset + i],
                blendByte
            );
        }
    }
}

void TransitionEngine::applyIris() {
    // CENTER ORIGIN: Mechanical aperture opening from center
    // Like a camera iris revealing target from center outward

    // Iris opens with eased progress (0 = closed, CENTER_POINT = fully open)
    m_irisRadius = m_progress * (float)CENTER_POINT;

    // Iris has sharp outer edge with slight feathering
    float featherWidth = 5.0f;

    // Aperture blades create angular variation
    uint8_t bladeCount = 8;
    float bladeAngle = (m_progress * PI * 0.5f);  // Blades rotate as iris opens

    for (uint16_t strip = 0; strip < 2; strip++) {
        uint16_t offset = strip * STRIP_LENGTH;

        for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
            float distFromCenter = fabsf((float)i - (float)CENTER_POINT);

            // Simulate aperture blade pattern
            // Map LED position to angle (treat strip as diameter)
            float normalizedPos = ((float)i / (float)STRIP_LENGTH) * 2.0f - 1.0f;
            float angle = atan2f(normalizedPos, 0.5f) + bladeAngle;

            // Blade modulation creates scalloped edge
            float bladeModulation = (sinf(angle * bladeCount) + 1.0f) * 0.5f;
            float effectiveRadius = m_irisRadius * (0.85f + bladeModulation * 0.15f);

            float blend;
            if (distFromCenter < effectiveRadius - featherWidth) {
                // Inside iris - show target
                blend = 1.0f;
            } else if (distFromCenter < effectiveRadius) {
                // Feathered edge
                blend = 1.0f - (distFromCenter - (effectiveRadius - featherWidth)) / featherWidth;
            } else {
                // Outside iris - show source
                blend = 0.0f;
            }

            // Ensure transition completes
            blend = fmaxf(blend, m_progress * m_progress * m_progress);
            blend = constrain(blend, 0.0f, 1.0f);

            uint8_t blendByte = (uint8_t)(blend * 255.0f);
            m_outputBuffer[offset + i] = lerpColor(
                m_sourceBuffer[offset + i],
                m_targetBuffer[offset + i],
                blendByte
            );
        }
    }
}

void TransitionEngine::applyNuclear() {
    // CENTER ORIGIN: Chain reaction explosion from center
    // Shockwave expands with radiation glow, leaving target in wake

    // Shockwave expands with accelerating velocity
    float shockwaveProgress = m_progress * m_progress;  // Accelerating
    m_shockwaveRadius = shockwaveProgress * (float)CENTER_POINT * 1.3f;  // Overshoot edges

    // Radiation intensity peaks early then decays
    m_radiationIntensity = sinf(m_progress * PI);  // Bell curve

    // Shockwave characteristics
    float shockWidth = 12.0f + m_progress * 8.0f;  // Widens as it expands
    float afterglowDecay = 0.7f;

    for (uint16_t strip = 0; strip < 2; strip++) {
        uint16_t offset = strip * STRIP_LENGTH;

        for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
            float distFromCenter = fabsf((float)i - (float)CENTER_POINT);
            float blend = 0.0f;
            float radiation = 0.0f;

            // Distance from shockwave front
            float distFromShock = distFromCenter - m_shockwaveRadius;

            if (distFromShock < -shockWidth) {
                // Behind shockwave - transition complete with afterglow decay
                float behindDistance = fabsf(distFromShock + shockWidth);
                float afterglow = fmaxf(0.0f, 1.0f - behindDistance * afterglowDecay * 0.05f);
                radiation = afterglow * m_radiationIntensity * 0.3f;
                blend = 1.0f;
            } else if (distFromShock < shockWidth) {
                // In shockwave - bright flash, partial blend
                float shockPos = (distFromShock + shockWidth) / (shockWidth * 2.0f);
                radiation = (1.0f - fabsf(shockPos - 0.5f) * 2.0f) * m_radiationIntensity;
                blend = 1.0f - shockPos;
            } else {
                // Ahead of shockwave - source still visible
                blend = 0.0f;
                radiation = 0.0f;
            }

            // Ensure completion
            blend = fmaxf(blend, m_progress);
            blend = constrain(blend, 0.0f, 1.0f);

            // Apply blend
            CRGB color = lerpColor(
                m_sourceBuffer[offset + i],
                m_targetBuffer[offset + i],
                (uint8_t)(blend * 255.0f)
            );

            // Add radiation glow (white/yellow flash)
            // PRE-SCALE: Reduce intensity by 65% to prevent white accumulation
            if (radiation > 0.01f) {
                uint8_t radByte = (uint8_t)(radiation * 200.0f * 0.65f);
                color.r = qadd8(color.r, radByte);
                color.g = qadd8(color.g, scale8(radByte, 230));  // 0.9 * 255
                color.b = qadd8(color.b, scale8(radByte, 77));   // 0.3 * 255
            }

            m_outputBuffer[offset + i] = color;
        }
    }
}

void TransitionEngine::applyStargate() {
    // CENTER ORIGIN: Wormhole portal effect at center
    // Event horizon expands with swirling kawoosh, target emerges from portal

    // Event horizon expands then stabilizes
    float horizonTarget = (float)CENTER_POINT * 0.8f;
    if (m_progress < 0.3f) {
        // Initial expansion with overshoot
        float expandProgress = m_progress / 0.3f;
        m_eventHorizonRadius = horizonTarget * (1.0f + 0.3f * sinf(expandProgress * PI)) * expandProgress;
    } else if (m_progress < 0.7f) {
        // Stable portal
        m_eventHorizonRadius = horizonTarget;
    } else {
        // Final expansion to complete transition
        float collapseProgress = (m_progress - 0.7f) / 0.3f;
        m_eventHorizonRadius = horizonTarget + (CENTER_POINT - horizonTarget) * collapseProgress;
    }

    // Chevron rotation for swirl effect
    m_chevronAngle += 0.15f;
    float kawooshIntensity = 0.0f;
    if (m_progress < 0.2f) {
        // Kawoosh burst at start
        kawooshIntensity = sinf((m_progress / 0.2f) * PI);
    }

    for (uint16_t strip = 0; strip < 2; strip++) {
        uint16_t offset = strip * STRIP_LENGTH;

        for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
            float distFromCenter = fabsf((float)i - (float)CENTER_POINT);
            float blend = 0.0f;
            float portalGlow = 0.0f;

            // Swirl pattern based on position
            float normalizedDist = distFromCenter / (float)CENTER_POINT;
            float swirlAngle = m_chevronAngle + normalizedDist * PI * 2.0f;
            float swirl = (sinf(swirlAngle * 9.0f) + 1.0f) * 0.5f;

            if (distFromCenter < m_eventHorizonRadius * 0.8f) {
                // Inside event horizon - target visible through portal
                blend = 1.0f;
                // Inner glow diminishes toward center
                portalGlow = (distFromCenter / (m_eventHorizonRadius * 0.8f)) * 0.3f;
            } else if (distFromCenter < m_eventHorizonRadius) {
                // Event horizon edge - maximum glow with swirl
                float edgePos = (distFromCenter - m_eventHorizonRadius * 0.8f) / (m_eventHorizonRadius * 0.2f);
                blend = 1.0f - edgePos;
                portalGlow = (1.0f - fabsf(edgePos - 0.5f) * 2.0f) * (0.5f + swirl * 0.5f);
            } else {
                // Outside portal - source with kawoosh effect
                blend = 0.0f;
                if (kawooshIntensity > 0.0f) {
                    float kawooshRange = 30.0f * kawooshIntensity;
                    float distFromHorizon = distFromCenter - m_eventHorizonRadius;
                    if (distFromHorizon < kawooshRange) {
                        portalGlow = kawooshIntensity * (1.0f - distFromHorizon / kawooshRange);
                    }
                }
            }

            // Ensure completion
            blend = fmaxf(blend, m_progress * m_progress);
            blend = constrain(blend, 0.0f, 1.0f);

            // Apply blend
            CRGB color = lerpColor(
                m_sourceBuffer[offset + i],
                m_targetBuffer[offset + i],
                (uint8_t)(blend * 255.0f)
            );

            // Add portal glow (blue/white)
            // PRE-SCALE: Reduce intensity by 70% to prevent white accumulation
            if (portalGlow > 0.01f) {
                uint8_t glowByte = (uint8_t)(portalGlow * 180.0f * 0.70f);
                color.r = qadd8(color.r, scale8(glowByte, 102));  // 0.4 * 255
                color.g = qadd8(color.g, scale8(glowByte, 179));  // 0.7 * 255
                color.b = qadd8(color.b, glowByte);
            }

            m_outputBuffer[offset + i] = color;
        }
    }
}

void TransitionEngine::applyKaleidoscope() {
    // CENTER ORIGIN: Symmetric crystal fold patterns radiating from center
    // Creates mirrored segments that rotate and reveal target

    // Rotation accelerates then decelerates
    float rotationSpeed = sinf(m_progress * PI);  // Bell curve
    m_rotationAngle += rotationSpeed * 0.1f;

    // Fold boundaries rotate with transition
    float foldAngle = (2.0f * PI) / (float)m_foldCount;

    for (uint16_t strip = 0; strip < 2; strip++) {
        uint16_t offset = strip * STRIP_LENGTH;

        for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
            float distFromCenter = fabsf((float)i - (float)CENTER_POINT);
            float normalizedDist = distFromCenter / (float)CENTER_POINT;

            // Map position to angle in kaleidoscope space
            float posAngle = ((float)i / (float)STRIP_LENGTH) * PI + m_rotationAngle;

            // Fold into single segment
            float foldedAngle = fmodf(posAngle, foldAngle);
            if ((int)(posAngle / foldAngle) % 2 == 1) {
                foldedAngle = foldAngle - foldedAngle;  // Mirror alternate segments
            }

            // Blend factor varies by fold position and progress
            float foldProgress = foldedAngle / foldAngle;  // 0-1 within fold

            // Crystal facet edges reveal target first
            float edgeFactor = 1.0f - fabsf(foldProgress - 0.5f) * 2.0f;  // Peaks at fold center

            // Distance modulation - center reveals before edges
            float distModulation = 1.0f - normalizedDist;

            // Combined blend
            float blend = m_progress * (0.5f + edgeFactor * 0.3f + distModulation * 0.2f);

            // Sharp crystalline transitions at fold boundaries
            float boundaryDist = fminf(foldProgress, 1.0f - foldProgress) * foldAngle;
            if (boundaryDist < 0.1f && m_progress > 0.2f && m_progress < 0.8f) {
                // Flash at fold boundaries during mid-transition
                blend = fminf(1.0f, blend + 0.4f);
            }

            // Ensure completion
            blend = fmaxf(blend, m_progress * m_progress);
            blend = constrain(blend, 0.0f, 1.0f);

            uint8_t blendByte = (uint8_t)(blend * 255.0f);
            m_outputBuffer[offset + i] = lerpColor(
                m_sourceBuffer[offset + i],
                m_targetBuffer[offset + i],
                blendByte
            );
        }
    }
}

void TransitionEngine::applyMandala() {
    // CENTER ORIGIN: Sacred geometry with concentric ring phases
    // Multiple rings expand outward with staggered timing

    // Update ring phases - each ring has its own timing
    for (uint8_t r = 0; r < 5; r++) {
        // Rings activate sequentially from center outward
        float ringDelay = (float)r * 0.15f;
        float ringProgress = fmaxf(0.0f, (m_progress - ringDelay) / (1.0f - ringDelay));
        m_ringPhases[r] = ringProgress;
    }

    // Ring boundaries (normalized 0-1 from center)
    float ringBoundaries[6] = {0.0f, 0.15f, 0.35f, 0.55f, 0.75f, 1.0f};

    for (uint16_t strip = 0; strip < 2; strip++) {
        uint16_t offset = strip * STRIP_LENGTH;

        for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
            float distFromCenter = fabsf((float)i - (float)CENTER_POINT);
            float normalizedDist = distFromCenter / (float)CENTER_POINT;

            // Determine which ring this LED belongs to
            uint8_t ringIndex = 0;
            for (uint8_t r = 0; r < 5; r++) {
                if (normalizedDist >= ringBoundaries[r] && normalizedDist < ringBoundaries[r + 1]) {
                    ringIndex = r;
                    break;
                }
            }

            // Position within ring (0-1)
            float ringStart = ringBoundaries[ringIndex];
            float ringEnd = ringBoundaries[ringIndex + 1];
            float posInRing = (normalizedDist - ringStart) / (ringEnd - ringStart);

            // Ring phase affects blend with geometric pattern
            float ringPhase = m_ringPhases[ringIndex];

            // Sacred geometry pattern - lotus petal effect
            float petalCount = 4.0f + (float)ringIndex * 2.0f;  // More petals in outer rings
            float petalAngle = ((float)i / (float)STRIP_LENGTH) * PI * 2.0f;
            float petalPattern = (sinf(petalAngle * petalCount + ringPhase * PI) + 1.0f) * 0.5f;

            // Blend combines ring phase with petal pattern
            float patternInfluence = 0.25f * (1.0f - fabsf(ringPhase - 0.5f) * 2.0f);
            float blend = ringPhase * (1.0f - patternInfluence) + petalPattern * patternInfluence;

            // Smooth transition at ring boundaries
            float boundaryWidth = 0.08f;
            float distToInner = posInRing;
            float distToOuter = 1.0f - posInRing;

            if (distToInner < boundaryWidth && ringIndex > 0) {
                // Blend with inner ring
                float innerBlend = m_ringPhases[ringIndex - 1];
                float t = distToInner / boundaryWidth;
                blend = blend * t + innerBlend * (1.0f - t);
            }
            if (distToOuter < boundaryWidth && ringIndex < 4) {
                // Blend with outer ring
                float outerBlend = m_ringPhases[ringIndex + 1];
                float t = distToOuter / boundaryWidth;
                blend = blend * t + outerBlend * (1.0f - t);
            }

            // Ensure completion
            blend = fmaxf(blend, m_progress * m_progress);
            blend = constrain(blend, 0.0f, 1.0f);

            uint8_t blendByte = (uint8_t)(blend * 255.0f);
            m_outputBuffer[offset + i] = lerpColor(
                m_sourceBuffer[offset + i],
                m_targetBuffer[offset + i],
                blendByte
            );
        }
    }
}

} // namespace transitions
} // namespace lightwaveos
