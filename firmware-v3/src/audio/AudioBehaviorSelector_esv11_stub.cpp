/**
 * @file AudioBehaviorSelector_esv11_stub.cpp
 * @brief Stub implementation for ESV11 audio backend builds.
 *
 * The full MIS AudioBehaviorSelector depends on the LWLS audio pipeline. In
 * ESV11 backend builds we intentionally exclude that pipeline, but effects still
 * link against AudioBehaviorSelector for behaviour switching.
 *
 * This stub keeps the public API stable and provides deterministic, lightweight
 * behaviour selection without allocating memory or depending on removed audio
 * components.
 */

#include "../config/features.h"

#if FEATURE_AUDIO_SYNC && FEATURE_AUDIO_BACKEND_ESV11

#include "AudioBehaviorSelector.h"
#include "../plugins/api/EffectContext.h"

namespace lightwaveos::audio {

AudioBehaviorSelector::AudioBehaviorSelector()
{
    // Keep defaults predictable and allocation-free.
    m_fallbackBehavior = plugins::VisualBehavior::BREATHE_WITH_DYNAMICS;
    reset();
}

void AudioBehaviorSelector::registerBehavior(plugins::VisualBehavior behavior, float priority)
{
    // Update existing entry if present.
    for (auto& entry : m_behaviors) {
        if (entry.enabled && entry.behavior == behavior) {
            entry.priority = priority;
            return;
        }
    }

    // Insert into first free slot.
    for (auto& entry : m_behaviors) {
        if (!entry.enabled) {
            entry.behavior = behavior;
            entry.priority = priority;
            entry.enabled = true;
            // If we had no current behaviour, adopt the first registered one.
            if (m_currentBehavior == m_fallbackBehavior) {
                m_currentBehavior = behavior;
                m_targetBehavior = behavior;
                m_previousBehavior = behavior;
            }
            return;
        }
    }
}

void AudioBehaviorSelector::unregisterBehavior(plugins::VisualBehavior behavior)
{
    for (auto& entry : m_behaviors) {
        if (entry.enabled && entry.behavior == behavior) {
            entry.enabled = false;
        }
    }

    // Ensure current behaviour remains valid.
    bool stillRegistered = false;
    for (const auto& entry : m_behaviors) {
        if (entry.enabled && entry.behavior == m_currentBehavior) {
            stillRegistered = true;
            break;
        }
    }
    if (!stillRegistered) {
        m_currentBehavior = m_fallbackBehavior;
        m_targetBehavior = m_fallbackBehavior;
        m_previousBehavior = m_fallbackBehavior;
        m_transitionProgress = 1.0f;
    }
}

void AudioBehaviorSelector::setFallbackBehavior(plugins::VisualBehavior behavior)
{
    m_fallbackBehavior = behavior;
    if (m_currentBehavior == plugins::VisualBehavior::BREATHE_WITH_DYNAMICS) {
        m_currentBehavior = behavior;
        m_targetBehavior = behavior;
        m_previousBehavior = behavior;
    }
}

void AudioBehaviorSelector::setTransitionTime(uint16_t ms)
{
    m_transitionTimeMs = ms;
}

void AudioBehaviorSelector::setEnergyThresholds(float rest, float build, float hold)
{
    m_restThreshold = rest;
    m_buildThreshold = build;
    m_holdThreshold = hold;
}

void AudioBehaviorSelector::reset()
{
    m_phase = NarrativePhase::REST;
    m_previousPhase = NarrativePhase::REST;

    m_currentBehavior = m_fallbackBehavior;
    m_targetBehavior = m_fallbackBehavior;
    m_previousBehavior = m_fallbackBehavior;

    m_transitionProgress = 1.0f;
    m_transitionTimeMs = 500;
    m_transitionStartMs = 0;

    m_phaseIntensity = 0.0f;
    m_phaseStartEnergy = 0.0f;
    m_phaseStartMs = 0;

    m_energySmoothed = 0.0f;
    m_fluxSmoothed = 0.0f;
    m_previousEnergy = 0.0f;
    m_peakEnergy = 0.0f;

    m_wasOnBeat = false;
    m_wasOnDownbeat = false;
    m_beatPhase = 0.0f;
    m_audioAvailable = false;
    m_lastBeatTick = 0;

    m_restThreshold = 0.15f;
    m_buildThreshold = 0.35f;
    m_holdThreshold = 0.65f;

    m_lastUpdateMs = 0;
}

void AudioBehaviorSelector::update(const plugins::EffectContext& ctx)
{
    // Minimal integration: cache a few values for effect queries, but do not
    // attempt MIS phase detection or behaviour switching.
    m_audioAvailable = ctx.audio.available;
    m_energySmoothed = ctx.audio.controlBus.rms;
    m_fluxSmoothed = ctx.audio.controlBus.flux;
    m_beatPhase = ctx.audio.musicalGrid.beat_phase01;
    m_wasOnBeat = ctx.audio.musicalGrid.beat_tick;
    m_wasOnDownbeat = ctx.audio.musicalGrid.downbeat_tick;

    // Ensure we always return a sane behaviour even if nothing was registered.
    if (m_currentBehavior == plugins::VisualBehavior::BREATHE_WITH_DYNAMICS &&
        m_fallbackBehavior != plugins::VisualBehavior::BREATHE_WITH_DYNAMICS) {
        m_currentBehavior = m_fallbackBehavior;
        m_targetBehavior = m_fallbackBehavior;
        m_previousBehavior = m_fallbackBehavior;
    }

    // No transitions in stub.
    m_transitionProgress = 1.0f;
}

} // namespace lightwaveos::audio

#endif
