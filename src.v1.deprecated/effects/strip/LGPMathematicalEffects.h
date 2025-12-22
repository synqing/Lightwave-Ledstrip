#ifndef LGP_MATHEMATICAL_EFFECTS_H
#define LGP_MATHEMATICAL_EFFECTS_H

#include <FastLED.h>
#include "../../config/hardware_config.h"
#include "../../config/features.h"

// ============================================================================
// LGP MATHEMATICAL & GEOMETRIC EFFECTS
// ============================================================================
// Effects based on mathematical systems and geometric patterns
// All effects follow CENTER ORIGIN principle (originate from LED 79/80)
//
// Implements:
// - Cellular Automata (Rule 110/30 patterns)
// - Gray-Scott Reaction-Diffusion (Turing patterns)
// - Mandelbrot Zoom (fractal iteration)
// - Strange Attractor 1D (Lorenz/Rossler projection)
// - Kuramoto Coupled Oscillators (phase synchronization)
// ============================================================================

// Cellular Automata
// 1D elementary automata (Wolfram rules) initialized from center
// Displays multiple generations as time-varying pattern
void lgpCellularAutomata();

// Gray-Scott Reaction-Diffusion
// Two-chemical system creating Turing patterns
// du/dt = Du∇²u - uv² + F(1-u)
// dv/dt = Dv∇²v + uv² - (F+k)v
void lgpGrayScottReactionDiffusion();

// Mandelbrot Zoom
// z = z² + c fractal iteration mapped to strip
// Zoom centered on LED 79/80 with escape-time coloring
void lgpMandelbrotZoom();

// Strange Attractor 1D
// Lorenz or Rossler attractor projected to 1D
// Chaotic trajectory creates unpredictable patterns from center
void lgpStrangeAttractor();

// Kuramoto Coupled Oscillators
// Phase synchronization of coupled oscillators
// dθ/dt = ω + (K/N)Σsin(θj - θi)
// Frequency varies from center, shows synchronization dynamics
void lgpKuramotoOscillators();

#endif // LGP_MATHEMATICAL_EFFECTS_H
