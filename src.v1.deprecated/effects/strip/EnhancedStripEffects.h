#ifndef ENHANCED_STRIP_EFFECTS_H
#define ENHANCED_STRIP_EFFECTS_H

#include <FastLED.h>
#include "../../config/hardware_config.h"
#include "../../config/features.h"

#if FEATURE_ENHANCEMENT_ENGINES

// Enhanced effects using ColorEngine for richer visuals
// Week 3: Enhanced effects with cross-palette blending and diffusion

// Fire+ - Dual palette (HeatColors + LavaColors) for deeper fire tones
void fireEnhanced();

// Ocean+ - Triple palette (deep blue/cyan/surface) for layered ocean depth
void stripOceanEnhanced();

// LGP Holographic+ - Color diffusion for smoother shimmer effect
void lgpHolographicEnhanced();

// Week 4: Enhanced effects using MotionEngine for advanced motion
// Shockwave+ - Eased expansion with momentum decay
void shockwaveEnhanced();

// Collision+ - Independent phase per collision particle
void collisionEnhanced();

// LGP Wave Collision+ - Phase-shifted interference
void lgpWaveCollisionEnhanced();

#endif // FEATURE_ENHANCEMENT_ENGINES
#endif // ENHANCED_STRIP_EFFECTS_H
