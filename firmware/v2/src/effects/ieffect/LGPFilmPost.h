/**
 * @file LGPFilmPost.h
 * @brief Cinema-grade post-processing for LGP effects (1D + dual-strip)
 *
 * Usage:
 *   1) In your effect .cpp:  #include "LGPFilmPost.h"
 *   2) In init():            lightwaveos::effects::cinema::reset();
 *   3) At end of render():  lightwaveos::effects::cinema::apply(ctx, speedNorm);
 *
 * What it does:
 * - Takes strip A as the "source image"
 * - Spatially softens (3-tap) to reduce edge shimmer
 * - Filmic tone maps (ACES fitted curve) for highlight rolloff
 * - Gamma LUT encode for perceptual brightness
 * - Temporal EMA to suppress flicker
 * - Writes back to BOTH strips locked (no wing rivalry)
 */

#pragma once

#include "../../plugins/api/EffectContext.h"
#include <FastLED.h>
#include <cstdint>

namespace lightwaveos {
namespace effects {
namespace cinema {

/// Call on effect init (or when switching effects).
void reset();

/// Call after your effect renders into ctx.leds[].
/// speedNorm = ctx.speed / 50.0f (same convention you already use)
void apply(plugins::EffectContext& ctx, float speedNorm);

} // namespace cinema
} // namespace effects
} // namespace lightwaveos
