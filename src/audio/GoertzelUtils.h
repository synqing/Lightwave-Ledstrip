#pragma once

#include "../config/features.h"

#if FEATURE_AUDIO_SYNC
// GoertzelUtils – lightweight helpers for accessing 96-bin musical‐spaced
// spectral data produced by the SpectraSynq Goertzel engine.
//
// HOW TO USE
// ----------
// 1. Call GoertzelUtils::setBinsPointer(ptr) once per audio frame, passing the
//    *AGC-normalised* 96-element float array (values 0–1).
// 2. Render code can then fetch the latest magnitudes via:
//        const float* bins = GoertzelUtils::getBins96();
//    or individual bins via GoertzelUtils::getBinMagnitude(idx).
// 3. For zone-based visuals, call mapBinsToZones() to aggregate the 96 bins
//    into an arbitrary number of perceptual zones, optionally logarithmic.
//
// All functions are constexpr/inline where appropriate for zero overhead.

#include <Arduino.h>

namespace GoertzelUtils {

constexpr int BIN_COUNT = 96;  // musical semitone bins A0–A7

// Provide the latest AGC-normalised magnitude buffer.
// Pointer MUST remain valid until the next call (normally the global buffer in
// the audio pipeline).
void setBinsPointer(const float* bins);  // O(1)

// Retrieve pointer to the current 96-element magnitude buffer (may be nullptr
// during early startup).
const float* getBins96();  // O(1)

// Convenience – bounds-checked single-bin fetch (returns 0 on nullptr or OOB).
float getBinMagnitude(int idx);  // O(1)

// Map the 96 bins into <zoneCount> aggregated values (simple average).
// If `logarithmic` is true, uses a perceptual log-mapping giving more
// resolution to bass.
// `out` must point to a float array of length >= zoneCount.
void mapBinsToZones(int zoneCount, float* out, bool logarithmic = true);

}  // namespace GoertzelUtils

#endif // FEATURE_AUDIO_SYNC
