#ifndef LGP_INTERFERENCE_EFFECTS_H
#define LGP_INTERFERENCE_EFFECTS_H

// Light Guide Plate Interference Effects
// These effects are specifically designed to exploit the optical properties
// of edge-lit light guide plates to create unique visual phenomena

// Box Wave Controller - Creates 3-12 controllable standing wave boxes
// Encoder 3 (Speed): Box oscillation speed
// Encoder 4 (Intensity): Box contrast/sharpness  
// Encoder 5 (Saturation): Color saturation
// Encoder 6 (Complexity): Number of boxes (3-12)
// Encoder 7 (Variation): Motion type (standing/traveling/rotating)
void lgpBoxWave();

// Holographic Shimmer - Multi-layer interference for depth illusion
// Encoder 3 (Speed): Shimmer animation speed
// Encoder 4 (Intensity): Effect brightness/visibility
// Encoder 5 (Saturation): Color richness
// Encoder 6 (Complexity): Number of depth layers (2-5)
// Encoder 7 (Variation): Layer interaction mode
void lgpHolographic();

// Modal Resonance - Explores optical cavity modes (1-20)
// Encoder 3 (Speed): Mode sweep speed
// Encoder 4 (Intensity): Mode amplitude
// Encoder 5 (Saturation): Color saturation
// Encoder 6 (Complexity): Mode number selection
// Encoder 7 (Variation): Mode blend type
void lgpModalResonance();

// Interference Scanner - Creates scanning interference patterns
// Encoder 3 (Speed): Scan speed
// Encoder 4 (Intensity): Pattern contrast
// Encoder 5 (Saturation): Color depth
// Encoder 6 (Complexity): Interference complexity
// Encoder 7 (Variation): Scan pattern type
void lgpInterferenceScanner();

// Wave Collision - Simulates colliding wave packets
// Encoder 3 (Speed): Wave velocity
// Encoder 4 (Intensity): Wave amplitude
// Encoder 5 (Saturation): Color saturation
// Encoder 6 (Complexity): Wave packet width
// Encoder 7 (Variation): Collision behavior
void lgpWaveCollision();

#endif // LGP_INTERFERENCE_EFFECTS_H