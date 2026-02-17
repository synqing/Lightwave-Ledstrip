/**
 * @file KuramotoTransportEffect.cpp
 * @brief Implementation of event-driven transport effect
 *
 * The key insight: Kuramoto field is INVISIBLE.
 * What you SEE is transported light substance injected at events.
 */

#include "KuramotoTransportEffect.h"
#include "AudioReactivePolicy.h"

#include <string.h>
#include <math.h>

#include "../../utils/Log.h"

namespace lightwaveos::effects::ieffect {

// -------------------- Static metadata --------------------

const lightwaveos::plugins::EffectMetadata KuramotoTransportEffect::s_meta{
    "Kuramoto Transport",
    "Invisible oscillators -> event injections -> transported light substance",
    lightwaveos::plugins::EffectCategory::QUANTUM,
    1,
    "LightwaveOS"
};

const lightwaveos::plugins::EffectParameter KuramotoTransportEffect::s_params[2] = {
    // name, displayName, min, max, default
    {"sync_ratio", "Sync Ratio", 0.0f, 1.0f, 0.55f},
    {"radius",     "Coupling Radius", 0.0f, 1.0f, 0.50f},
};

// -------------------- Lifecycle --------------------

KuramotoTransportEffect::KuramotoTransportEffect() {
    // Scratch buffers are allocated in init(), not here.
}

bool KuramotoTransportEffect::init(lightwaveos::plugins::EffectContext& ctx) {
    // Allocate PSRAM for sub-components
    if (!m_field.allocatePsram()) {
        LW_LOGE("KuramotoTransport: field PSRAM alloc failed");
        return false;
    }
    if (!m_transport.allocatePsram()) {
        LW_LOGE("KuramotoTransport: transport PSRAM alloc failed");
        return false;
    }
    if (!m_scratch) {
        m_scratch = static_cast<PsramScratch*>(heap_caps_malloc(sizeof(PsramScratch), MALLOC_CAP_SPIRAM));
        if (!m_scratch) {
            LW_LOGE("KuramotoTransport: scratch PSRAM alloc failed");
            return false;
        }
    }
    memset(m_scratch, 0, sizeof(PsramScratch));

    // Seed from time to avoid identical behaviour across boots.
    // Seed from raw runtime + frame context to avoid SPEED-dependent seeding.
    const uint32_t seed =
        static_cast<uint32_t>(ctx.rawTotalTimeMs) ^
        static_cast<uint32_t>(ctx.frameNumber * 2654435761u) ^
        0xC0FFEEu;
    m_field.resetAll(seed);
    m_transport.resetAll();
    m_palettePhase = 0.0f;
    (void)ctx;
    return true;
}

void KuramotoTransportEffect::cleanup() {
    m_field.freePsram();
    m_transport.freePsram();
    if (m_scratch) { heap_caps_free(m_scratch); m_scratch = nullptr; }
}

const lightwaveos::plugins::EffectMetadata& KuramotoTransportEffect::getMetadata() const {
    return s_meta;
}

const lightwaveos::plugins::EffectParameter* KuramotoTransportEffect::getParameter(uint8_t index) const {
    if (index >= 2) return nullptr;
    return &s_params[index];
}

bool KuramotoTransportEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "sync_ratio") == 0) {
        m_syncRatio01 = clamp01(value);
        return true;
    }
    if (strcmp(name, "radius") == 0) {
        m_radius01 = clamp01(value);
        return true;
    }
    return false;
}

float KuramotoTransportEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "sync_ratio") == 0) return m_syncRatio01;
    if (strcmp(name, "radius") == 0) return m_radius01;
    return 0.0f;
}

// -------------------- Render --------------------

void KuramotoTransportEffect::render(lightwaveos::plugins::EffectContext& ctx) {
    if (!m_scratch) return;

    // Map zone id (0xFF means "not a zone render").
    uint8_t zid = (ctx.zoneId == 0xFF) ? 0 : ctx.zoneId;
    if (zid >= KuramotoOscillatorField::MAX_ZONES) zid = 0;

    // dt
    const float dt = AudioReactivePolicy::signalDt(ctx);

    // --- Audio steering (NO direct audio -> per-LED brightness mapping)
    // We ONLY steer regime parameters.
    float overall = ctx.audio.available ? clamp01(ctx.audio.overallSaliency()) : 0.25f;
    float rhythm  = ctx.audio.available ? clamp01(ctx.audio.rhythmicSaliency()) : 0.20f;
    float timbre  = ctx.audio.available ? clamp01(ctx.audio.timbralSaliency()) : 0.20f;

    // Custom knob sets the baseline regime; audio nudges it.
    const float sync01 = clamp01(0.70f * m_syncRatio01 + 0.30f * rhythm);

    float K = 0.0f;
    float spread = 0.0f;
    computeRegime(sync01, K, spread);

    // Nonlocal radius: knob + energy. (Energy increases interaction range.)
    const uint16_t radius = (uint16_t)fminf((float)KuramotoOscillatorField::MAX_R,
                                           (float)radiusFrom01(0.80f * m_radius01 + 0.20f * overall));

    // Noise and kicks: audio steers emergent structure without direct VU brightness mapping.
    const float noiseSigma   = 0.12f + 0.25f * timbre;              // rad/sqrt(s)
    const float flux01       = ctx.audio.available ? clamp01(ctx.audio.flux()) : 0.0f;
    const float kickRateHz   = 1.5f + 6.0f * flux01;                // events/sec (baseline 1.5 Hz for guaranteed liveliness)
    const float kickStrength = 1.2f + 1.8f * overall;              // radians [1.2,3.0] exceeds phase slip threshold

    // Step the invisible field.
    m_field.step(zid, dt, K, spread, radius, noiseSigma, kickRateHz, kickStrength);

    // --- Derived features used by transport (including velocity-driven advection)
    KuramotoFeatureExtractor::extract(
        m_field.theta(zid),
        m_field.prevTheta(zid),
        m_field.radius(),
        m_field.kernel(),
        m_scratch->velocity,
        m_scratch->coherence,
        m_scratch->event
    );

    // --- Transport parameters (dt-correct inside transport)
    const float mood01 = ctx.getMoodNormalized();

    // ========================================================================
    // SENSORY BRIDGE REFERENCE PATTERN: Unidirectional OUTWARD motion
    // ========================================================================
    // Bloom mode uses: offset = 0.25 + 1.75 * MOOD
    // - MOOD=0 (reactive): offset=0.25 (fast outward motion)
    // - MOOD=1 (dreamy): offset=2.0 (slower, more viscous)
    // Audio saliency adds extra push for energetic response.
    const float baseOffset60 = 0.25f + 1.75f * mood01 + 0.50f * overall;

    // Persistence per-frame @60fps. Higher MOOD = more persistence (dreamy trails).
    // Sensory Bridge uses alpha=0.99 baseline.
    const float persistence60 = 0.96f + 0.03f * mood01;  // [0.96..0.99]

    // Diffusion: cheap bloom/viscosity. Higher when incoherent (creates creamy fog).
    const float diffusion = clamp01(0.05f + 0.25f * (1.0f - sync01) + 0.10f * mood01);

    // The visible buffer length is centre->edge samples.
    // In LightwaveOS centre-origin convention for 160-LED strip: centrePoint=79 => radialLen=80.
    const uint16_t radialLen = (ctx.centerPoint + 1u);

    // Use extracted velocity for coherence-edge transport structure.
    m_transport.advectWithVelocity(zid, radialLen, baseOffset60, persistence60, diffusion, dt, m_scratch->velocity);

    // --- Inject events with CENTER BIAS
    // ========================================================================
    // SENSORY BRIDGE REFERENCE: Inject ONLY at center, let it flow outward
    // We compromise: inject anywhere events occur, but weight heavily toward center
    // ========================================================================
    const float injectGain = clamp01(static_cast<float>(ctx.intensity) / 255.0f);

    // Slow palette drift: event colours shift over time.
    m_palettePhase += dt * (0.08f + 0.20f * overall);

    // Base hue comes from global hue + slow drift.
    const uint8_t baseHue = (uint8_t)(ctx.gHue + (uint8_t)(m_palettePhase * 29.0f));

    // Accumulate center injection: sum all event energy, inject at center
    float centerEnergy = 0.0f;
    float totalEventEnergy = 0.0f;
    float centerPhaseSum = 0.0f;
    float centerCohSum = 0.0f;
    float totalWeight = 0.0f;

    // Iterate oscillators, accumulate energy with center bias
    for (uint16_t i = 0; i < radialLen && i < KuramotoOscillatorField::N; ++i) {
        const float e = m_scratch->event[i];
        if (e <= 0.001f) continue;

        // Center bias: exponential falloff from center (i=0)
        // Events near center contribute fully, events at edge contribute less
        const float distNorm = (float)i / (float)radialLen;  // 0=center, 1=edge
        const float centerWeight = expf(-3.0f * distNorm);   // edge events contribute less to center

        // Weighted contribution to center injection
        const float weightedE = e * centerWeight;
        centerEnergy += weightedE;
        totalEventEnergy += e;

        // Accumulate phase and coherence for color averaging
        const float th = m_field.theta(zid)[i];
        const float phase01 = (KuramotoOscillatorField::wrapPi(th) + KuramotoOscillatorField::PI_F) * (1.0f / (2.0f * KuramotoOscillatorField::PI_F));
        const float coh = m_scratch->coherence[i];

        centerPhaseSum += phase01 * weightedE;
        centerCohSum += coh * weightedE;
        totalWeight += weightedE;
    }

    // Inject accumulated energy at CENTER (position 0)
    if (centerEnergy > 0.01f && totalWeight > 0.001f) {
        // Cap centre injection so coherence-edge injections remain visible.
        const float centerCap = totalEventEnergy * 0.40f;
        const float centerEnergyLimited = fminf(centerEnergy, centerCap);
        if (centerEnergyLimited > 0.0f) {
            // Average phase and coherence
            const float avgPhase = centerPhaseSum / totalWeight;
            const float avgCoh = centerCohSum / totalWeight;

            // Color from averaged phase
            const uint8_t palIdx = (uint8_t)(baseHue + (uint8_t)(avgPhase * 255.0f) + (uint8_t)(avgCoh * 48.0f));
            const uint8_t bri = (uint8_t)(255.0f * clamp01(0.25f + 0.75f * clamp01(centerEnergyLimited)));
            CRGB c = ctx.palette.getColor(palIdx, bri);

            // Spread based on average coherence
            const float spreadVal = 0.5f + 1.0f * (1.0f - avgCoh);

            // Amount: accumulated energy * injection gain, clamped
            const float amount = clamp01(centerEnergyLimited * (0.5f + 0.5f * injectGain));

            // Inject at CENTER (position 0 = center of radial buffer)
            m_transport.injectAtPos(zid, radialLen, 0.0f, c, amount, spreadVal);
        }
    }

    // Inject at event locations to preserve coherence-edge structure.
    for (uint16_t i = 1; i < radialLen && i < KuramotoOscillatorField::N; ++i) {
        const float e = m_scratch->event[i];
        if (e <= 0.05f) continue;

        // Weak edge texture: flat 0.15x falloff preserves coherence-edge visibility.
        const float falloff = 0.15f;

        const float th = m_field.theta(zid)[i];
        const float phase01 = (KuramotoOscillatorField::wrapPi(th) + KuramotoOscillatorField::PI_F) * (1.0f / (2.0f * KuramotoOscillatorField::PI_F));
        const float coh = m_scratch->coherence[i];

        const uint8_t palIdx = (uint8_t)(baseHue + (uint8_t)(phase01 * 255.0f) + (uint8_t)(coh * 48.0f));
        const uint8_t bri = (uint8_t)(255.0f * clamp01(0.18f + 0.45f * e));
        CRGB c = ctx.palette.getColor(palIdx, bri);

        const float spreadVal = 0.3f + 0.7f * (1.0f - coh);
        const float amount = clamp01(e * falloff * injectGain);

        m_transport.injectAtPos(zid, radialLen, (float)i, c, amount, spreadVal);
    }

    // --- Readout to LEDs with tone mapping + centre-origin symmetry
    // Brightness is applied inside TransportBuffer::readoutToLeds() via ctx.brightness.
    // We control only "scene exposure" here (ties to intensity, not audio).
    const float exposure = 1.2f + 1.8f * injectGain;                   // brighter output
    const float satBoost = 0.10f + 0.35f * (1.0f - sync01) + 0.10f*mood01; // more haze in incoherent regimes

    m_transport.readoutToLeds(zid, ctx, radialLen, exposure, satBoost);
}

} // namespace lightwaveos::effects::ieffect
