#include "LGPFluidPlasmaEffects.h"
#include "../../core/EffectTypes.h"
#include <math.h>

// Math constants
#ifndef PI
#define PI 3.14159265358979323846
#endif
#ifndef TWO_PI
#define TWO_PI (2.0 * PI)
#endif

// External references
extern CRGB strip1[HardwareConfig::STRIP1_LED_COUNT];
extern CRGB strip2[HardwareConfig::STRIP2_LED_COUNT];
extern uint8_t paletteSpeed;
extern VisualParams visualParams;
extern CRGBPalette16 currentPalette;

// ============================================================================
// BENARD CONVECTION CELLS
// ============================================================================
// Simulates Rayleigh-Benard convection: heated fluid from below creates
// organized hexagonal circulation cells. Hot fluid rises at cell centers,
// cool fluid descends at cell boundaries.
//
// Physics: Ra = (g * β * ΔT * L³) / (ν * α)
// When Rayleigh number exceeds critical value (~1708), convection begins.
//
// CENTER ORIGIN: Cells radiate outward from LED 79/80
// Visual: Warm colors at cell centers (rising), cool at boundaries (sinking)
// ============================================================================

void lgpBenardConvection() {
    static float phase = 0;
    static float cellPhase = 0;

    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float complexity = visualParams.getComplexityNorm();

    // Phase evolution - cells slowly rotate/evolve
    phase += speed * 0.02f;
    cellPhase += speed * 0.005f;  // Slower cell boundary evolution

    // Number of convection cells (3-7 based on complexity)
    int numCells = 3 + (int)(complexity * 4);
    float cellWidth = (float)HardwareConfig::STRIP_HALF_LENGTH / numCells;

    for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // Distance from CENTER (LED 79/80)
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);

        // Determine which cell this LED belongs to
        float cellPosition = distFromCenter / cellWidth;
        float cellFraction = cellPosition - floor(cellPosition);  // 0-1 within cell

        // Cell center is at 0.5, boundaries at 0.0 and 1.0
        // Hot rising fluid at center, cool sinking at edges
        float cellCenterDist = abs(cellFraction - 0.5f) * 2.0f;  // 0 at center, 1 at edges

        // Temperature profile: hot at center (1.0), cool at edges (0.3)
        float temperature = 1.0f - (cellCenterDist * 0.7f);

        // Add convective motion - fluid circulation within each cell
        float circulation = sin(cellFraction * PI + phase * 2.0f);
        float verticalFlow = circulation * 0.3f;  // Vertical component

        // Turbulent fluctuations (more at cell boundaries)
        float turbulence = sin(distFromCenter * 0.3f + phase * 5.0f) *
                          cellCenterDist * 0.15f;

        // Combined thermal value
        float thermalValue = temperature + verticalFlow + turbulence;
        thermalValue = constrain(thermalValue, 0.0f, 1.0f);

        // Map to palette index (hot = high index, cool = low index)
        // Use warm palette range: deep orange (200) to bright yellow (255)
        uint8_t paletteIndex = 180 + (uint8_t)(thermalValue * 75);

        // Add slow cell boundary shimmer
        float boundaryGlow = (1.0f - cellCenterDist) *
                            sin(cellPhase + floor(cellPosition) * 1.5f) * 0.2f;

        // Brightness: cells glow brighter at centers
        uint8_t brightness = (uint8_t)((0.4f + thermalValue * 0.6f + boundaryGlow) *
                                       255 * intensity);

        // Get color at full saturation, then scale brightness
        CRGB color = ColorFromPalette(currentPalette, paletteIndex, 255);
        color.nscale8(brightness);

        strip1[i] = color;
        strip2[i] = color;
    }
}

// ============================================================================
// RAYLEIGH-TAYLOR INSTABILITY
// ============================================================================
// Heavy fluid above light fluid creates mushroom-shaped plumes that grow
// exponentially. Interface starts at center, plumes grow outward.
//
// Physics: Growth rate σ = sqrt(A * g * k) where A = Atwood number
// Atwood number A = (ρ_heavy - ρ_light) / (ρ_heavy + ρ_light)
//
// CENTER ORIGIN: Interface at LED 79/80, plumes expand outward
// ============================================================================

void lgpRayleighTaylorInstability() {
    static float growthPhase = 0;
    static float perturbPhase = 0;
    static uint8_t plumeSeeds[8] = {0};  // Random seeds for plume positions
    static bool initialized = false;

    if (!initialized) {
        for (int i = 0; i < 8; i++) {
            plumeSeeds[i] = random8();
        }
        initialized = true;
    }

    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float complexity = visualParams.getComplexityNorm();

    // Growth rate - plumes grow faster over time (exponential character)
    growthPhase += speed * 0.01f;
    perturbPhase += speed * 0.03f;

    // Number of plume fingers (3-8 based on complexity)
    int numPlumes = 3 + (int)(complexity * 5);

    // Maximum growth extent (grows with time, resets periodically)
    float maxGrowth = fmod(growthPhase, 3.0f) / 3.0f;  // 0-1 cycle
    float growthExtent = maxGrowth * HardwareConfig::STRIP_HALF_LENGTH;

    for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);

        // Base interface at center
        float baseValue = 0;

        if (distFromCenter < growthExtent) {
            // Inside the growing plume region
            float normalizedDist = distFromCenter / max(growthExtent, 1.0f);

            // Find closest plume finger
            float plumeInfluence = 0;
            for (int p = 0; p < numPlumes; p++) {
                // Each plume has a characteristic angular position
                float plumeAngle = (plumeSeeds[p % 8] / 255.0f) * TWO_PI;
                float plumePosition = sin(plumeAngle + perturbPhase * 0.5f);

                // Plume width varies with distance (mushroom cap broadens)
                float plumeWidth = 0.15f + normalizedDist * 0.25f;

                // Distance from this LED's position to plume center
                float ledAngle = ((float)(i - HardwareConfig::STRIP_CENTER_POINT) /
                                 HardwareConfig::STRIP_HALF_LENGTH) * PI;
                float angularDist = abs(sin(ledAngle) - plumePosition);

                if (angularDist < plumeWidth) {
                    float plumeStrength = 1.0f - (angularDist / plumeWidth);
                    plumeInfluence = max(plumeInfluence, plumeStrength);
                }
            }

            // Mushroom cap shape: wider at the top (further from center)
            float capShape = pow(normalizedDist, 0.7f);

            // Mix heavy fluid (plume) with light fluid (background)
            baseValue = plumeInfluence * capShape;

            // Add Kelvin-Helmholtz roll-up at plume edges
            float edgeRollup = sin(distFromCenter * 0.2f + perturbPhase * 3.0f) *
                              plumeInfluence * 0.2f;
            baseValue += edgeRollup;
        }

        baseValue = constrain(baseValue, 0.0f, 1.0f);

        // Heavy fluid = warm colors (red/orange), light fluid = cool (blue/cyan)
        uint8_t paletteIndex = (uint8_t)(baseValue * 180);  // Sweep through palette
        uint8_t brightness = (uint8_t)((0.3f + baseValue * 0.7f) * 255 * intensity);

        CRGB color = ColorFromPalette(currentPalette, paletteIndex, 255);
        color.nscale8(brightness);

        strip1[i] = color;
        strip2[i] = color;
    }
}

// ============================================================================
// PLASMA PINCH (Z-PINCH)
// ============================================================================
// Magnetic field compresses plasma toward center axis.
// Lorentz force J×B creates inward radial pressure.
//
// Physics: Pinch pressure = B²/(2μ₀), balanced by plasma pressure nkT
// Instabilities (sausage, kink) cause periodic bulges
//
// CENTER ORIGIN: Plasma compresses toward LED 79/80
// ============================================================================

void lgpPlasmaPinch() {
    static float pinchPhase = 0;
    static float instabilityPhase = 0;

    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float complexity = visualParams.getComplexityNorm();

    pinchPhase += speed * 0.025f;
    instabilityPhase += speed * 0.08f;

    // Pinch oscillation - plasma compresses then relaxes
    float pinchStrength = 0.3f + 0.7f * (0.5f + 0.5f * sin(pinchPhase));

    // Sausage instability wavelength (m=0 mode)
    float sausageWavelength = 20.0f + complexity * 30.0f;

    // Kink instability (m=1 mode) - helical distortion
    float kinkAmplitude = complexity * 0.3f;

    for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;

        // Base plasma column profile (Bennett pinch: n(r) = n₀/(1 + r²/a²)²)
        float columnRadius = 0.2f + (1.0f - pinchStrength) * 0.5f;
        float plasmaProfile = 1.0f / pow(1.0f + pow(normalizedDist / columnRadius, 2), 2);

        // Sausage instability - periodic radial perturbations
        float sausage = sin(distFromCenter / sausageWavelength * TWO_PI + instabilityPhase);
        float sausageEffect = sausage * 0.2f * (1.0f - normalizedDist);

        // Kink instability - helical wobble
        float kink = sin(distFromCenter * 0.1f + instabilityPhase * 1.5f) * kinkAmplitude;

        // Combined plasma density
        float plasmaDensity = plasmaProfile + sausageEffect + kink * (1.0f - normalizedDist);
        plasmaDensity = constrain(plasmaDensity, 0.0f, 1.0f);

        // Hot plasma core = bright white/blue, cooler edges = purple/violet
        uint8_t paletteIndex = 160 + (uint8_t)((1.0f - plasmaDensity) * 60);
        uint8_t brightness = (uint8_t)(plasmaDensity * 255 * intensity);

        CRGB color = ColorFromPalette(currentPalette, paletteIndex, 255);
        color.nscale8(brightness);

        // Add plasma glow/corona effect at edges
        if (normalizedDist > columnRadius * 0.8f && normalizedDist < columnRadius * 1.5f) {
            float coronaGlow = (1.0f - abs(normalizedDist - columnRadius) / (columnRadius * 0.5f));
            coronaGlow = max(0.0f, coronaGlow) * 0.3f;
            color += CRGB(coronaGlow * 100, coronaGlow * 50, coronaGlow * 150);
        }

        strip1[i] = color;
        strip2[i] = color;
    }
}

// ============================================================================
// MAGNETIC RECONNECTION
// ============================================================================
// Oppositely directed magnetic field lines approach, reconnect at X-point,
// releasing stored magnetic energy as plasma jets.
//
// Physics: Sweet-Parker/Petschek reconnection models
// Energy release: ΔE = B²V/(2μ₀)
//
// CENTER ORIGIN: X-point at LED 79/80, jets propagate outward
// ============================================================================

void lgpMagneticReconnection() {
    static float reconnectPhase = 0;
    static float jetPhase = 0;
    static float burstTimer = 0;
    static bool inBurst = false;

    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float complexity = visualParams.getComplexityNorm();

    reconnectPhase += speed * 0.015f;

    // Reconnection is bursty - energy builds then releases
    burstTimer += speed * 0.02f;
    if (burstTimer > 3.0f) {
        inBurst = true;
        jetPhase = 0;
        burstTimer = 0;
    }

    if (inBurst) {
        jetPhase += speed * 0.15f;
        if (jetPhase > 1.5f) {
            inBurst = false;
        }
    }

    for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;

        // Incoming magnetic field lines (approaching X-point from edges)
        float incomingField = (1.0f - normalizedDist) *
                             sin(normalizedDist * 5.0f + reconnectPhase);

        // X-point current sheet at center (very thin, bright)
        float currentSheet = exp(-pow(distFromCenter / 5.0f, 2)) * 0.8f;

        // Outgoing reconnected field lines
        float outgoingField = normalizedDist *
                             sin(normalizedDist * 3.0f - reconnectPhase * 0.5f) * 0.5f;

        // Plasma jet (when burst active)
        float jet = 0;
        if (inBurst) {
            // Jets propagate outward from X-point
            float jetFront = jetPhase * HardwareConfig::STRIP_HALF_LENGTH;
            float jetWidth = 10.0f + complexity * 15.0f;

            if (distFromCenter < jetFront && distFromCenter > jetFront - jetWidth) {
                float jetProfile = 1.0f - abs(distFromCenter - (jetFront - jetWidth/2)) / (jetWidth/2);
                jet = max(0.0f, jetProfile) * (1.0f - jetPhase * 0.5f);  // Fade as it expands
            }
        }

        // Combine components
        float fieldValue = abs(incomingField) * 0.3f + currentSheet +
                          abs(outgoingField) * 0.3f + jet;
        fieldValue = constrain(fieldValue, 0.0f, 1.0f);

        // Current sheet = hot white, field lines = blue/cyan, jets = orange/yellow
        uint8_t paletteIndex;
        if (currentSheet > 0.3f) {
            paletteIndex = 240;  // Hot center
        } else if (jet > 0.3f) {
            paletteIndex = 32;   // Orange jets
        } else {
            paletteIndex = 160 + (uint8_t)(normalizedDist * 40);  // Blue field lines
        }

        uint8_t brightness = (uint8_t)(fieldValue * 255 * intensity);

        CRGB color = ColorFromPalette(currentPalette, paletteIndex, 255);
        color.nscale8(brightness);

        strip1[i] = color;
        strip2[i] = color;
    }
}

// ============================================================================
// KELVIN-HELMHOLTZ ENHANCED
// ============================================================================
// Velocity shear creates rolling cat's eye vortices at interface.
// Enhancement over base version with more detailed vortex structure.
//
// Physics: Instability when Richardson number Ri < 0.25
// Growth rate: σ = k * ΔU / 2 (for equal density layers)
//
// CENTER ORIGIN: Shear interface at LED 79/80, vortices roll outward
// ============================================================================

void lgpKelvinHelmholtzEnhanced() {
    static float vortexPhase = 0;
    static float rollPhase = 0;

    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float complexity = visualParams.getComplexityNorm();

    vortexPhase += speed * 0.03f;
    rollPhase += speed * 0.08f;

    // Number of vortices (cat's eyes)
    int numVortices = 4 + (int)(complexity * 4);
    float vortexSpacing = (float)HardwareConfig::STRIP_HALF_LENGTH / numVortices;

    for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;

        // Find which vortex this position is in
        float vortexPosition = distFromCenter / vortexSpacing;
        float vortexIndex = floor(vortexPosition);
        float withinVortex = (vortexPosition - vortexIndex) * TWO_PI;  // 0 to 2π within vortex

        // Cat's eye streamline pattern
        // Streamfunction: ψ = y - (k/2)log(cosh(y) + cos(x-ct))
        float x = withinVortex - rollPhase;
        float y = (distFromCenter - vortexIndex * vortexSpacing - vortexSpacing/2) / (vortexSpacing/2);

        // Simplified cat's eye pattern
        float catsEye = cos(x) * exp(-y * y);

        // Vortex core intensity (brighter at center of each cat's eye)
        float coreIntensity = exp(-pow(abs(y), 2) * 2.0f);

        // Shear layer mixing (entrainment of fluids)
        float mixing = 0.5f + 0.5f * sin(withinVortex + vortexIndex * 0.7f + vortexPhase);

        // Spiral arms within vortex (secondary instabilities)
        float spiralArms = sin(withinVortex * 2 + rollPhase * 3.0f) *
                          coreIntensity * 0.3f * complexity;

        // Combined value
        float vortexValue = catsEye * 0.4f + coreIntensity * 0.4f +
                           mixing * 0.2f + spiralArms;
        vortexValue = (vortexValue + 1.0f) / 2.0f;  // Normalize to 0-1
        vortexValue = constrain(vortexValue, 0.0f, 1.0f);

        // Upper fluid = one color, lower fluid = another, mix at interface
        // Creates visual distinction between the two shearing fluids
        uint8_t paletteIndex;
        if (y > 0.3f) {
            paletteIndex = 120 + (uint8_t)(vortexValue * 60);  // Upper fluid (green-cyan)
        } else if (y < -0.3f) {
            paletteIndex = 200 + (uint8_t)(vortexValue * 55);  // Lower fluid (magenta-pink)
        } else {
            // Interface mixing zone
            paletteIndex = 160 + (uint8_t)(mixing * 80);
        }

        uint8_t brightness = (uint8_t)((0.4f + vortexValue * 0.6f) * 255 * intensity);

        CRGB color = ColorFromPalette(currentPalette, paletteIndex, 255);
        color.nscale8(brightness);

        strip1[i] = color;
        strip2[i] = color;
    }
}
