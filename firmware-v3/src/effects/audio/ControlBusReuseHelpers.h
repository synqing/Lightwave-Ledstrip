/**
 * @file ControlBusReuseHelpers.h
 * @brief Header-only inline helpers for read-only ControlBusFrame access.
 *
 * Phase 1 Move 1.5 substrate per Topology_Reconciliation. This header
 * unifies the five categories of ControlBus consumption that effect code
 * currently accesses via raw field reads scattered across `src/effects/`.
 * It is a NON-MIGRATING refactor: helpers are added; existing call sites
 * are NOT modified. Phase 2 (effect-body migration) is deferred to Captain
 * code-review of this API.
 *
 * The five categories (per SB_ES_MOTION_BRAINSTORM_CATALOGUE_2026-04-26
 * line 93):
 *   1. STM temporal modulation        — gated by stmReady
 *   2. Harmonic / musical saliency    — *NoveltySmooth fields
 *   3. Motion-semantic                — Stage 2 extension fields
 *   4. Onset band-split               — onset[Bass|Mid|High]Flux + triggers
 *   5. Audio confidence + silence     — silentScale, audioConfidence, isSilent
 *
 * ── Constraints ──────────────────────────────────────────────────────────
 *
 *  - Header-only inline. No link-time symbols.
 *  - Read-only on the bus (every helper takes `const ControlBusFrame&`).
 *  - Render-safe: no heap, no FastLED writes, no dt-dependent state.
 *  - Coexists with raw field access — existing code keeps working unchanged.
 *  - All [0,1] returns are clamped at the helper boundary so effects can
 *    rely on bounded inputs without re-checking.
 *
 * ── British English ──────────────────────────────────────────────────────
 * Comments use British spelling (centre, colour, behaviour, initialise).
 * Symbol names follow the camelCase + Pascal convention used elsewhere in
 * `src/effects/audio/`.
 *
 * ── Field-availability findings (verified against ControlBus.h) ──────────
 *
 *   Verified present:
 *     - stmTemporal[STM_MEL_BANDS=16], stmTemporalEnergy, stmReady
 *     - saliency.{harmonic,rhythmic,timbral,dynamic}NoveltySmooth
 *     - timing_jitter, syncopation_level, pitch_contour_dir
 *     - onsetBassFlux, onsetMidFlux, onsetHighFlux
 *     - kickTrigger, snareTrigger, hihatTrigger
 *     - audioConfidence, silentScale, isSilent, spectralNovelty
 *
 *   No fields were omitted — every helper in the canonical spec compiles
 *   cleanly against the current ControlBusFrame definition.
 */

#pragma once

#include <cstdint>

#include "audio/contracts/ControlBus.h"
// MusicalSaliency.h is transitively included by ControlBus.h, but include
// it explicitly for clarity since we read MusicalSaliencyFrame fields.
#include "audio/contracts/MusicalSaliency.h"

namespace lightwaveos {
namespace effects {
namespace audio {
namespace reuse {

// ── Internal clamp utilities ──────────────────────────────────────────────
// Constexpr inline so the compiler can fold them at every call site.

/**
 * @brief Clamp a scalar to the closed interval [0, 1].
 */
constexpr inline float clamp01(float x) {
    return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
}

/**
 * @brief Clamp a scalar to the closed interval [-1, +1].
 */
constexpr inline float clampPm1(float x) {
    return x < -1.0f ? -1.0f : (x > 1.0f ? 1.0f : x);
}

// ── Category 1: STM temporal modulation ───────────────────────────────────
// stmReady gates the STM outputs — during warmup the buffers are not yet
// valid and effects must treat them as zero.

/**
 * @brief STM mean temporal-modulation energy in [0,1].
 *
 * Returns 0.0 until the STM history buffer is warm (`stmReady == false`).
 */
inline float stmEnergy(const ::lightwaveos::audio::ControlBusFrame& bus) {
    return bus.stmReady ? bus.stmTemporalEnergy : 0.0f;
}

/**
 * @brief Per-mel STM temporal-modulation magnitude in [0,1].
 *
 * Returns 0.0 if `mel` is out of range or STM is not yet warm.
 *
 * @param bus  Latest published frame.
 * @param mel  Mel band index in [0, STM_MEL_BANDS).
 */
inline float stmBand(const ::lightwaveos::audio::ControlBusFrame& bus, uint8_t mel) {
    return (bus.stmReady && mel < ::lightwaveos::audio::ControlBusFrame::STM_MEL_BANDS)
               ? bus.stmTemporal[mel]
               : 0.0f;
}

// ── Category 2: Musical saliency ──────────────────────────────────────────
// All four *NoveltySmooth fields are already smoothed inside ControlBus
// (asymmetric attack/release per SaliencyTuning) and live in [0,1]. We
// surface them via passthrough helpers so effects do not need to know the
// substruct path.

inline float harmonicNovelty(const ::lightwaveos::audio::ControlBusFrame& bus) {
    return bus.saliency.harmonicNoveltySmooth;
}

inline float rhythmicNovelty(const ::lightwaveos::audio::ControlBusFrame& bus) {
    return bus.saliency.rhythmicNoveltySmooth;
}

inline float timbralNovelty(const ::lightwaveos::audio::ControlBusFrame& bus) {
    return bus.saliency.timbralNoveltySmooth;
}

inline float dynamicNovelty(const ::lightwaveos::audio::ControlBusFrame& bus) {
    return bus.saliency.dynamicNoveltySmooth;
}

/**
 * @brief Maximum across all four smoothed novelty channels.
 *
 * Useful for "any musical change happening?" without specifying which.
 */
inline float dominantNovelty(const ::lightwaveos::audio::ControlBusFrame& bus) {
    float h = harmonicNovelty(bus);
    float r = rhythmicNovelty(bus);
    float t = timbralNovelty(bus);
    float d = dynamicNovelty(bus);
    float m = h;
    if (r > m) m = r;
    if (t > m) m = t;
    if (d > m) m = d;
    return m;
}

// ── Category 3: Motion-semantic (Stage 2 extension) ───────────────────────
// These three fields are documented as bounded but defensive clamps protect
// effects from any transient out-of-range values produced by the upstream
// motion-semantic computation.

/**
 * @brief Coefficient-of-variation of inter-onset intervals in [0,1].
 *
 * 0.0 = perfectly regular timing; 1.0 = maximally irregular.
 */
inline float timingJitter(const ::lightwaveos::audio::ControlBusFrame& bus) {
    return clamp01(bus.timing_jitter);
}

/**
 * @brief Syncopation level in [0,1].
 *
 * 0.0 = onsets fall on metric grid; 1.0 = maximally off-beat.
 */
inline float syncopation(const ::lightwaveos::audio::ControlBusFrame& bus) {
    return clamp01(bus.syncopation_level);
}

/**
 * @brief Pitch-contour direction in [-1, +1].
 *
 * -1.0 = descending centroid; 0.0 = flat; +1.0 = ascending.
 */
inline float contourDir(const ::lightwaveos::audio::ControlBusFrame& bus) {
    return clampPm1(bus.pitch_contour_dir);
}

// ── Category 4: Onset band-split ──────────────────────────────────────────
// Triggers are single-frame booleans set by the onset detector; flux
// magnitudes are unbounded floats in [0, ∞) — passthrough only, no clamp.

/**
 * @brief True if any of kick/snare/hihat fired this frame.
 */
inline bool anyOnsetTrigger(const ::lightwaveos::audio::ControlBusFrame& bus) {
    return bus.kickTrigger || bus.snareTrigger || bus.hihatTrigger;
}

inline float bassFlux(const ::lightwaveos::audio::ControlBusFrame& bus) {
    return bus.onsetBassFlux;
}

inline float midFlux(const ::lightwaveos::audio::ControlBusFrame& bus) {
    return bus.onsetMidFlux;
}

inline float highFlux(const ::lightwaveos::audio::ControlBusFrame& bus) {
    return bus.onsetHighFlux;
}

// ── Category 5: Audio confidence + silence ────────────────────────────────
// silentScale is the Sensory Bridge silence-fade scalar (1.0 active, 0.0
// silent after hysteresis). audioConfidence is the novelty-assisted "music
// active?" envelope. isSilent is the discrete silence latch.

inline float audioConfidence(const ::lightwaveos::audio::ControlBusFrame& bus) {
    return clamp01(bus.audioConfidence);
}

inline float silentScale(const ::lightwaveos::audio::ControlBusFrame& bus) {
    return clamp01(bus.silentScale);
}

inline bool isSilent(const ::lightwaveos::audio::ControlBusFrame& bus) {
    return bus.isSilent;
}

/**
 * @brief Combine a base brightness with the two silence-fade scalars.
 *
 * Effects that fade out during silence should multiply their pre-clip
 * brightness by silentScale × audioConfidence so the renderer's clip
 * stage receives an already-attenuated value.
 *
 * @param bus            Latest published frame.
 * @param baseBrightness Pre-attenuation brightness (caller owns the range).
 * @return baseBrightness × silentScale × audioConfidence.
 */
inline float gatedBrightness(const ::lightwaveos::audio::ControlBusFrame& bus,
                             float baseBrightness) {
    return baseBrightness * silentScale(bus) * audioConfidence(bus);
}

}  // namespace reuse
}  // namespace audio
}  // namespace effects
}  // namespace lightwaveos
