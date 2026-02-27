// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file AudioBehaviorSelector.cpp
 * @brief Implementation of narrative-driven behavior selection mixin
 */

#include "AudioBehaviorSelector.h"
#include "../plugins/api/EffectContext.h"
#include <algorithm>

#ifndef NATIVE_BUILD
#include <Arduino.h>
#else
// Mock millis() for native builds
static uint32_t s_mockMillis = 0;
static inline uint32_t millis() { return s_mockMillis; }
#endif

namespace lightwaveos {
namespace audio {

// ============================================================================
// Constructor
// ============================================================================

AudioBehaviorSelector::AudioBehaviorSelector()
    : m_fallbackBehavior(plugins::VisualBehavior::BREATHE_WITH_DYNAMICS)
    , m_phase(NarrativePhase::REST)
    , m_previousPhase(NarrativePhase::REST)
    , m_currentBehavior(plugins::VisualBehavior::BREATHE_WITH_DYNAMICS)
    , m_targetBehavior(plugins::VisualBehavior::BREATHE_WITH_DYNAMICS)
    , m_previousBehavior(plugins::VisualBehavior::BREATHE_WITH_DYNAMICS)
    , m_transitionProgress(1.0f)
    , m_transitionTimeMs(500)
    , m_transitionStartMs(0)
    , m_phaseIntensity(0.0f)
    , m_phaseStartEnergy(0.0f)
    , m_phaseStartMs(0)
    , m_energySmoothed(0.0f)
    , m_fluxSmoothed(0.0f)
    , m_previousEnergy(0.0f)
    , m_peakEnergy(0.0f)
    , m_wasOnBeat(false)
    , m_wasOnDownbeat(false)
    , m_beatPhase(0.0f)
    , m_audioAvailable(false)
    , m_lastBeatTick(0)
    , m_restThreshold(0.15f)
    , m_buildThreshold(0.35f)
    , m_holdThreshold(0.65f)
    , m_lastUpdateMs(0)
{
    // All behaviors start disabled
    for (auto& entry : m_behaviors) {
        entry.enabled = false;
    }
}

// ============================================================================
// Configuration
// ============================================================================

void AudioBehaviorSelector::registerBehavior(
    plugins::VisualBehavior behavior,
    float priority
) {
    // Find an empty slot or existing entry
    for (auto& entry : m_behaviors) {
        if (!entry.enabled || entry.behavior == behavior) {
            entry.behavior = behavior;
            entry.priority = priority;
            entry.enabled = true;
            return;
        }
    }
    // No slot available - behavior array is full
}

void AudioBehaviorSelector::unregisterBehavior(plugins::VisualBehavior behavior) {
    for (auto& entry : m_behaviors) {
        if (entry.enabled && entry.behavior == behavior) {
            entry.enabled = false;
            return;
        }
    }
}

void AudioBehaviorSelector::setFallbackBehavior(plugins::VisualBehavior behavior) {
    m_fallbackBehavior = behavior;
    // Also set current behavior if nothing selected yet
    if (m_transitionProgress >= 1.0f) {
        m_currentBehavior = behavior;
        m_targetBehavior = behavior;
    }
}

void AudioBehaviorSelector::setTransitionTime(uint16_t ms) {
    m_transitionTimeMs = ms;
}

void AudioBehaviorSelector::setEnergyThresholds(float rest, float build, float hold) {
    m_restThreshold = rest;
    m_buildThreshold = build;
    m_holdThreshold = hold;
}

void AudioBehaviorSelector::reset() {
    m_phase = NarrativePhase::REST;
    m_previousPhase = NarrativePhase::REST;
    m_currentBehavior = m_fallbackBehavior;
    m_targetBehavior = m_fallbackBehavior;
    m_previousBehavior = m_fallbackBehavior;
    m_transitionProgress = 1.0f;
    m_phaseIntensity = 0.0f;
    m_energySmoothed = 0.0f;
    m_fluxSmoothed = 0.0f;
    m_previousEnergy = 0.0f;
    m_peakEnergy = 0.0f;
    m_wasOnBeat = false;
    m_wasOnDownbeat = false;
    m_audioAvailable = false;
    m_lastBeatTick = 0;
    m_lastUpdateMs = millis();
}

// ============================================================================
// Per-frame update
// ============================================================================

void AudioBehaviorSelector::update(const plugins::EffectContext& ctx) {
    uint32_t nowMs = millis();
    uint32_t deltaMs = nowMs - m_lastUpdateMs;
    m_lastUpdateMs = nowMs;

    // Clamp deltaMs to avoid huge jumps
    if (deltaMs > 100) deltaMs = 100;

    // Store previous energy for trend detection
    m_previousEnergy = m_energySmoothed;

#if FEATURE_AUDIO_SYNC
    m_audioAvailable = ctx.audio.available;

    if (m_audioAvailable) {
        // Cache audio state
        m_wasOnBeat = ctx.audio.isOnBeat();
        m_wasOnDownbeat = ctx.audio.isOnDownbeat();
        m_beatPhase = ctx.audio.beatPhase();

        // Smooth energy with fast attack, slow decay
        float rms = ctx.audio.rms();
        float flux = ctx.audio.flux();

        // Fast attack (~50ms), slow decay (~200ms)
        constexpr float ATTACK_ALPHA = 0.3f;   // Fast rise
        constexpr float DECAY_ALPHA = 0.05f;   // Slow fall

        if (rms > m_energySmoothed) {
            m_energySmoothed = m_energySmoothed + ATTACK_ALPHA * (rms - m_energySmoothed);
        } else {
            m_energySmoothed = m_energySmoothed + DECAY_ALPHA * (rms - m_energySmoothed);
        }

        if (flux > m_fluxSmoothed) {
            m_fluxSmoothed = m_fluxSmoothed + ATTACK_ALPHA * (flux - m_fluxSmoothed);
        } else {
            m_fluxSmoothed = m_fluxSmoothed + (DECAY_ALPHA * 2.0f) * (flux - m_fluxSmoothed);
        }

        // Track peak energy for HOLD phase
        if (m_energySmoothed > m_peakEnergy) {
            m_peakEnergy = m_energySmoothed;
        } else {
            // Slow decay of peak tracker
            m_peakEnergy = m_peakEnergy * 0.995f;
        }

        // Analyze narrative phase
        NarrativePhase newPhase = analyzeNarrativePhase(ctx);

        if (newPhase != m_phase) {
            m_previousPhase = m_phase;
            m_phase = newPhase;
            m_phaseStartMs = nowMs;
            m_phaseStartEnergy = m_energySmoothed;

            // Reset peak on entering HOLD
            if (newPhase == NarrativePhase::HOLD) {
                m_peakEnergy = m_energySmoothed;
            }
        }

        // Update phase intensity
        updatePhaseIntensity(nowMs);

        // Select behavior for current phase and music style
        plugins::VisualBehavior newBehavior = selectBehaviorForPhase(m_phase, ctx);

        if (newBehavior != m_targetBehavior) {
            beginTransition(newBehavior, nowMs);
        }
    } else {
        // No audio - use fallback
        m_wasOnBeat = false;
        m_wasOnDownbeat = false;

        if (m_targetBehavior != m_fallbackBehavior) {
            beginTransition(m_fallbackBehavior, nowMs);
        }

        // Slowly decay smoothed values
        m_energySmoothed *= 0.95f;
        m_fluxSmoothed *= 0.95f;
        m_phaseIntensity *= 0.98f;
    }
#else
    // Audio sync disabled - always use fallback
    m_audioAvailable = false;
    m_currentBehavior = m_fallbackBehavior;
    m_targetBehavior = m_fallbackBehavior;
    m_transitionProgress = 1.0f;
#endif

    // Update transition progress
    if (m_transitionProgress < 1.0f) {
        uint32_t elapsed = nowMs - m_transitionStartMs;
        m_transitionProgress = static_cast<float>(elapsed) / static_cast<float>(m_transitionTimeMs);
        if (m_transitionProgress >= 1.0f) {
            m_transitionProgress = 1.0f;
            m_currentBehavior = m_targetBehavior;
        }
    }
}

// ============================================================================
// Private methods
// ============================================================================

bool AudioBehaviorSelector::hasBehavior(plugins::VisualBehavior behavior) const {
    for (const auto& entry : m_behaviors) {
        if (entry.enabled && entry.behavior == behavior) {
            return true;
        }
    }
    return false;
}

const BehaviorEntry* AudioBehaviorSelector::getBehaviorEntry(
    plugins::VisualBehavior behavior
) const {
    for (const auto& entry : m_behaviors) {
        if (entry.enabled && entry.behavior == behavior) {
            return &entry;
        }
    }
    return nullptr;
}

void AudioBehaviorSelector::updatePhaseIntensity(uint32_t nowMs) {
    uint32_t phaseElapsedMs = nowMs - m_phaseStartMs;

    switch (m_phase) {
        case NarrativePhase::REST:
            // Low constant intensity
            m_phaseIntensity = 0.1f;
            break;

        case NarrativePhase::BUILD:
            // Rising intensity based on energy increase
            if (m_energySmoothed > m_phaseStartEnergy) {
                float delta = m_energySmoothed - m_phaseStartEnergy;
                float maxDelta = m_holdThreshold - m_phaseStartEnergy;
                m_phaseIntensity = (maxDelta > 0.0f)
                    ? std::min(delta / maxDelta, 1.0f)
                    : 0.5f;
            } else {
                // Time-based fallback
                m_phaseIntensity = std::min(
                    static_cast<float>(phaseElapsedMs) / 2000.0f,
                    1.0f
                );
            }
            break;

        case NarrativePhase::HOLD:
            // High intensity, slowly rising with sustain
            m_phaseIntensity = 0.7f + 0.3f * std::min(
                static_cast<float>(phaseElapsedMs) / 5000.0f,
                1.0f
            );
            break;

        case NarrativePhase::RELEASE:
            // Falling intensity as energy drops
            if (m_peakEnergy > m_restThreshold) {
                float ratio = m_energySmoothed / m_peakEnergy;
                m_phaseIntensity = 1.0f - ratio;
            } else {
                // Time-based fallback
                m_phaseIntensity = 1.0f - std::min(
                    static_cast<float>(phaseElapsedMs) / 3000.0f,
                    1.0f
                );
            }
            break;
    }

    // Clamp to valid range
    m_phaseIntensity = std::max(0.0f, std::min(m_phaseIntensity, 1.0f));
}

NarrativePhase AudioBehaviorSelector::analyzeNarrativePhase(
    const plugins::EffectContext& ctx
) {
    // Get beat proximity (approaching downbeat = BUILD opportunity)
    bool nearDownbeat = (m_beatPhase > 0.75f) || (m_beatPhase < 0.1f);

#if FEATURE_AUDIO_SYNC
    float beatStrength = ctx.audio.beatStrength();
#else
    float beatStrength = 0.0f;
#endif

    // Phase detection with hysteresis
    switch (m_phase) {
        case NarrativePhase::REST:
            // Exit REST when energy rises with flux
            if (m_fluxSmoothed > 0.2f && m_energySmoothed > m_restThreshold) {
                return NarrativePhase::BUILD;
            }
            // Or on sudden energy spike
            if (m_energySmoothed > m_buildThreshold) {
                return NarrativePhase::BUILD;
            }
            break;

        case NarrativePhase::BUILD:
            // Enter HOLD when energy peaks with strong beats
            if (m_energySmoothed > m_holdThreshold && beatStrength > 0.4f) {
                return NarrativePhase::HOLD;
            }
            // Return to REST if energy drops
            if (m_energySmoothed < m_restThreshold && m_fluxSmoothed < 0.1f) {
                return NarrativePhase::REST;
            }
            break;

        case NarrativePhase::HOLD:
            // Enter RELEASE when energy starts dropping
            if (m_energySmoothed < m_previousEnergy * 0.85f &&
                m_energySmoothed < m_holdThreshold) {
                return NarrativePhase::RELEASE;
            }
            // Timeout: can't stay in HOLD forever
            if ((millis() - m_phaseStartMs) > 15000) {  // 15 second max
                return NarrativePhase::RELEASE;
            }
            break;

        case NarrativePhase::RELEASE:
            // Return to REST when energy is low
            if (m_energySmoothed < m_restThreshold && m_fluxSmoothed < 0.1f) {
                return NarrativePhase::REST;
            }
            // Start BUILD if energy rises again
            if (m_fluxSmoothed > 0.3f && nearDownbeat) {
                return NarrativePhase::BUILD;
            }
            // Jump to HOLD on sudden energy spike
            if (m_energySmoothed > m_holdThreshold && beatStrength > 0.5f) {
                return NarrativePhase::HOLD;
            }
            break;
    }

    // Stay in current phase
    return m_phase;
}

plugins::VisualBehavior AudioBehaviorSelector::selectBehaviorForPhase(
    NarrativePhase phase,
    const plugins::EffectContext& ctx
) {
    using namespace plugins;

    // Get music style recommendation if available
    VisualBehavior stylePrimary = m_fallbackBehavior;
    VisualBehavior styleSecondary = m_fallbackBehavior;

#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        stylePrimary = ctx.audio.recommendedBehavior();
        styleSecondary = ctx.audio.behaviorContext.recommendedSecondary;
    }
#endif

    // Map narrative phase to preferred behaviors
    VisualBehavior phasePrimary = m_fallbackBehavior;
    VisualBehavior phaseSecondary = m_fallbackBehavior;

    switch (phase) {
        case NarrativePhase::REST:
            // Minimal, contemplative behaviors
            phasePrimary = VisualBehavior::TEXTURE_FLOW;
            phaseSecondary = VisualBehavior::BREATHE_WITH_DYNAMICS;
            break;

        case NarrativePhase::BUILD:
            // Tension building - approaching center, rising intensity
            phasePrimary = VisualBehavior::BREATHE_WITH_DYNAMICS;
            phaseSecondary = VisualBehavior::SHIMMER_WITH_MELODY;
            break;

        case NarrativePhase::HOLD:
            // Peak energy - use music style recommendations
            phasePrimary = stylePrimary;
            phaseSecondary = styleSecondary;
            break;

        case NarrativePhase::RELEASE:
            // Resolving - center pulse, fading
            phasePrimary = VisualBehavior::PULSE_ON_BEAT;
            phaseSecondary = VisualBehavior::BREATHE_WITH_DYNAMICS;
            break;
    }

    // Find best match from registered behaviors
    return findBestMatch(phasePrimary, phaseSecondary);
}

plugins::VisualBehavior AudioBehaviorSelector::findBestMatch(
    plugins::VisualBehavior recommended,
    plugins::VisualBehavior secondary
) {
    // Try primary recommendation first
    if (hasBehavior(recommended)) {
        return recommended;
    }

    // Try secondary
    if (hasBehavior(secondary)) {
        return secondary;
    }

    // Find highest priority registered behavior
    plugins::VisualBehavior best = m_fallbackBehavior;
    float bestPriority = -1.0f;

    for (const auto& entry : m_behaviors) {
        if (entry.enabled && entry.priority > bestPriority) {
            best = entry.behavior;
            bestPriority = entry.priority;
        }
    }

    // If nothing registered, use fallback
    if (bestPriority < 0.0f) {
        return m_fallbackBehavior;
    }

    return best;
}

void AudioBehaviorSelector::beginTransition(
    plugins::VisualBehavior newBehavior,
    uint32_t nowMs
) {
    m_previousBehavior = m_currentBehavior;
    m_targetBehavior = newBehavior;
    m_transitionProgress = 0.0f;
    m_transitionStartMs = nowMs;
}

} // namespace audio
} // namespace lightwaveos
