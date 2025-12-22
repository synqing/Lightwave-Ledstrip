#ifndef TUNED_EFFECTS_H
#define TUNED_EFFECTS_H

// Tuned versions of effects with enhanced encoder parameter control
// These provide more nuanced control over visual parameters

// Fire effect with enhanced controls:
// - Speed (Enc 3): Animation speed
// - Intensity (Enc 4): Fire height and spark frequency  
// - Saturation (Enc 5): Color richness (0=white, 255=colored)
// - Complexity (Enc 6): Turbulence and flame detail
// - Variation (Enc 7): Flame color (0=normal, 128=blue, 255=green)
void fireTuned();

// Strip BPM with enhanced rhythm control:
// - Speed (Enc 3): BPM (30-180)
// - Intensity (Enc 4): Pulse brightness and reach
// - Saturation (Enc 5): Color saturation
// - Complexity (Enc 6): Number of simultaneous pulses (1-4)
// - Variation (Enc 7): Pulse pattern (single/double/triple)
void stripBPMTuned();

// Sinelon with multiple wave controls:
// - Speed (Enc 3): Oscillation speed
// - Intensity (Enc 4): Trail length and brightness
// - Saturation (Enc 5): Color saturation
// - Complexity (Enc 6): Number of dots (1-5)
// - Variation (Enc 7): Movement pattern (sine/double/bounce/chaos)
void sinelonTuned();

// Gravity Well with enhanced physics:
// - Speed (Enc 3): Gravity strength
// - Intensity (Enc 4): Particle brightness and trail
// - Saturation (Enc 5): Color saturation
// - Complexity (Enc 6): Number of particles (5-30)
// - Variation (Enc 7): Physics mode (normal/space/oscillating/chaotic)
void gravityWellTuned();

#endif // TUNED_EFFECTS_H