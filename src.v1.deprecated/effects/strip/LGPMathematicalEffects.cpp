#include "LGPMathematicalEffects.h"
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
// CELLULAR AUTOMATA
// ============================================================================
// 1D elementary cellular automata (Wolfram rules)
// Displays time evolution as spatial pattern, initialized from center
//
// Rule 30: Chaotic, used in Mathematica's random number generator
// Rule 110: Turing complete, complex patterns
//
// CENTER ORIGIN: Initial cell at LED 79/80, generations evolve visually
// ============================================================================

// Static state for cellular automata
static uint8_t caState[HardwareConfig::STRIP_LENGTH];
static uint8_t caNextState[HardwareConfig::STRIP_LENGTH];
static uint8_t caHistory[32][HardwareConfig::STRIP_LENGTH];  // 32 generations
static uint8_t caHistoryIndex = 0;

void lgpCellularAutomata() {
    static float phase = 0;
    static uint8_t currentRule = 30;
    static uint32_t lastUpdate = 0;
    static bool initialized = false;

    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float complexity = visualParams.getComplexityNorm();

    // Initialize from center
    if (!initialized) {
        memset(caState, 0, sizeof(caState));
        caState[HardwareConfig::STRIP_CENTER_POINT] = 1;
        caState[HardwareConfig::STRIP_CENTER_POINT - 1] = 1;
        memset(caHistory, 0, sizeof(caHistory));
        initialized = true;
    }

    // Choose rule based on complexity
    // Rule 30 (chaotic), Rule 90 (Sierpinski), Rule 110 (complex)
    if (complexity < 0.33f) {
        currentRule = 30;
    } else if (complexity < 0.66f) {
        currentRule = 90;
    } else {
        currentRule = 110;
    }

    // Update rate based on speed
    uint32_t updateInterval = 200 - (uint32_t)(speed * 150);
    uint32_t now = millis();

    if (now - lastUpdate > updateInterval) {
        // Store current generation in history
        memcpy(caHistory[caHistoryIndex], caState, HardwareConfig::STRIP_LENGTH);
        caHistoryIndex = (caHistoryIndex + 1) % 32;

        // Compute next generation
        for (int i = 1; i < HardwareConfig::STRIP_LENGTH - 1; i++) {
            uint8_t left = caState[i - 1];
            uint8_t center = caState[i];
            uint8_t right = caState[i + 1];

            // 3-cell neighborhood -> 3-bit number (0-7)
            uint8_t neighborhood = (left << 2) | (center << 1) | right;

            // Apply rule: if bit 'neighborhood' of rule is set, cell is alive
            caNextState[i] = (currentRule >> neighborhood) & 1;
        }

        // Edge conditions - wrap or inject from center
        uint8_t leftNeighbor = (caState[HardwareConfig::STRIP_LENGTH - 1] << 2) |
                              (caState[0] << 1) | caState[1];
        uint8_t rightNeighbor = (caState[HardwareConfig::STRIP_LENGTH - 2] << 2) |
                               (caState[HardwareConfig::STRIP_LENGTH - 1] << 1) | caState[0];
        caNextState[0] = (currentRule >> leftNeighbor) & 1;
        caNextState[HardwareConfig::STRIP_LENGTH - 1] = (currentRule >> rightNeighbor) & 1;

        // Copy next state to current
        memcpy(caState, caNextState, HardwareConfig::STRIP_LENGTH);

        // Occasionally reinitialize from center to prevent death
        if (random8() < 5) {
            caState[HardwareConfig::STRIP_CENTER_POINT] = 1;
        }

        lastUpdate = now;
    }

    // Display: blend current state with recent history for depth
    phase += speed * 0.02f;

    for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;

        // Current state
        float value = caState[i];

        // Blend with history for trailing effect
        for (int h = 0; h < 8; h++) {
            int histIdx = (caHistoryIndex - h - 1 + 32) % 32;
            float histWeight = 0.5f / (h + 1);
            value += caHistory[histIdx][i] * histWeight;
        }

        value = constrain(value, 0.0f, 1.5f) / 1.5f;

        // Color: position-based palette index (no rainbow)
        uint8_t paletteIndex = (uint8_t)(normalizedDist * 128 + phase * 20);
        uint8_t brightness = (uint8_t)(value * 255 * intensity);

        CRGB color = ColorFromPalette(currentPalette, paletteIndex, 255);
        color.nscale8(brightness);

        strip1[i] = color;
        strip2[i] = color;
    }
}

// ============================================================================
// GRAY-SCOTT REACTION-DIFFUSION
// ============================================================================
// Two-chemical Turing pattern system
// u: substrate chemical, v: catalyst chemical
// du/dt = Du∇²u - uv² + F(1-u)
// dv/dt = Dv∇²v + uv² - (F+k)v
//
// CENTER ORIGIN: Seed pattern at center, grows outward
// ============================================================================

// State buffers for reaction-diffusion
static float gsU[HardwareConfig::STRIP_LENGTH];
static float gsV[HardwareConfig::STRIP_LENGTH];

void lgpGrayScottReactionDiffusion() {
    static bool initialized = false;
    static float seedPhase = 0;

    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float complexity = visualParams.getComplexityNorm();

    // Initialize with uniform u, seeded v at center
    if (!initialized) {
        for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            gsU[i] = 1.0f;
            gsV[i] = 0.0f;
        }
        // Seed catalyst at center
        for (int i = HardwareConfig::STRIP_CENTER_POINT - 5;
             i <= HardwareConfig::STRIP_CENTER_POINT + 5; i++) {
            if (i >= 0 && i < HardwareConfig::STRIP_LENGTH) {
                gsV[i] = 0.5f + random8() / 512.0f;
            }
        }
        initialized = true;
    }

    // Gray-Scott parameters (tune for different patterns)
    // F: feed rate, k: kill rate
    // Different F,k values create spots, stripes, waves, etc.
    float F = 0.04f + complexity * 0.02f;   // 0.04-0.06
    float k = 0.06f + complexity * 0.02f;   // 0.06-0.08

    // Diffusion coefficients
    float Du = 1.0f;
    float Dv = 0.5f;

    // Time step
    float dt = speed * 0.5f;

    // Laplacian buffers
    static float lapU[HardwareConfig::STRIP_LENGTH];
    static float lapV[HardwareConfig::STRIP_LENGTH];

    // Compute Laplacians (1D discrete: L[i] = [i-1] - 2*[i] + [i+1])
    for (int i = 1; i < HardwareConfig::STRIP_LENGTH - 1; i++) {
        lapU[i] = gsU[i-1] - 2.0f * gsU[i] + gsU[i+1];
        lapV[i] = gsV[i-1] - 2.0f * gsV[i] + gsV[i+1];
    }
    // Boundary conditions (Neumann: zero flux)
    lapU[0] = gsU[1] - gsU[0];
    lapU[HardwareConfig::STRIP_LENGTH-1] = gsU[HardwareConfig::STRIP_LENGTH-2] -
                                           gsU[HardwareConfig::STRIP_LENGTH-1];
    lapV[0] = gsV[1] - gsV[0];
    lapV[HardwareConfig::STRIP_LENGTH-1] = gsV[HardwareConfig::STRIP_LENGTH-2] -
                                           gsV[HardwareConfig::STRIP_LENGTH-1];

    // Update reaction-diffusion equations
    for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float uvv = gsU[i] * gsV[i] * gsV[i];
        gsU[i] += (Du * lapU[i] - uvv + F * (1.0f - gsU[i])) * dt;
        gsV[i] += (Dv * lapV[i] + uvv - (F + k) * gsV[i]) * dt;

        // Clamp values
        gsU[i] = constrain(gsU[i], 0.0f, 1.0f);
        gsV[i] = constrain(gsV[i], 0.0f, 1.0f);
    }

    // Occasionally reseed from center to maintain activity
    seedPhase += speed * 0.005f;
    if (fmod(seedPhase, 5.0f) < 0.1f) {
        int seedPos = HardwareConfig::STRIP_CENTER_POINT + random8(10) - 5;
        if (seedPos >= 0 && seedPos < HardwareConfig::STRIP_LENGTH) {
            gsV[seedPos] = 0.5f;
        }
    }

    // Render: v concentration determines pattern visibility
    for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;

        // Use v concentration for brightness/pattern
        float value = gsV[i];

        // Position-based color (no rainbow)
        uint8_t paletteIndex = (uint8_t)(normalizedDist * 180 + gsU[i] * 75);
        uint8_t brightness = (uint8_t)((0.2f + value * 0.8f) * 255 * intensity);

        CRGB color = ColorFromPalette(currentPalette, paletteIndex, 255);
        color.nscale8(brightness);

        strip1[i] = color;
        strip2[i] = color;
    }
}

// ============================================================================
// MANDELBROT ZOOM
// ============================================================================
// z = z² + c fractal iteration mapped to LED strip
// Escape time determines color/brightness
//
// CENTER ORIGIN: Zoom centered on LED 79/80, explores fractal boundary
// ============================================================================

void lgpMandelbrotZoom() {
    static float zoomLevel = 1.0f;
    static float centerReal = -0.75f;  // Interesting region of Mandelbrot set
    static float centerImag = 0.0f;
    static float phase = 0;

    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float complexity = visualParams.getComplexityNorm();

    // Zoom in slowly, then reset
    phase += speed * 0.01f;
    zoomLevel = 1.0f + fmod(phase, 4.0f) * 2.0f;  // Zoom 1x to 9x

    // Pan slowly to explore boundary
    centerReal = -0.75f + sin(phase * 0.3f) * 0.2f;
    centerImag = sin(phase * 0.2f) * 0.3f;

    // Maximum iterations based on complexity
    int maxIter = 20 + (int)(complexity * 60);

    for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedPos = (float)(i - HardwareConfig::STRIP_CENTER_POINT) /
                             HardwareConfig::STRIP_HALF_LENGTH;

        // Map LED position to complex plane
        // Real part varies along strip, imaginary is fixed (1D slice)
        float scale = 3.0f / zoomLevel;
        float cReal = centerReal + normalizedPos * scale;
        float cImag = centerImag;

        // Mandelbrot iteration: z = z² + c
        float zReal = 0;
        float zImag = 0;
        int iter = 0;

        while (iter < maxIter && (zReal * zReal + zImag * zImag) < 4.0f) {
            float newReal = zReal * zReal - zImag * zImag + cReal;
            float newImag = 2.0f * zReal * zImag + cImag;
            zReal = newReal;
            zImag = newImag;
            iter++;
        }

        // Smooth escape time for smoother coloring
        float escapeTime;
        if (iter == maxIter) {
            escapeTime = 0;  // Inside set
        } else {
            // Smooth iteration count
            float absZ = sqrt(zReal * zReal + zImag * zImag);
            escapeTime = (float)iter + 1.0f - log2(log2(absZ));
            escapeTime = escapeTime / maxIter;
        }

        // Color mapping: escape time determines palette position
        uint8_t paletteIndex = (uint8_t)(escapeTime * 255);
        uint8_t brightness;

        if (iter == maxIter) {
            // Inside Mandelbrot set - dark but not black
            brightness = (uint8_t)(30 * intensity);
            paletteIndex = 0;
        } else {
            brightness = (uint8_t)((0.3f + escapeTime * 0.7f) * 255 * intensity);
        }

        CRGB color = ColorFromPalette(currentPalette, paletteIndex, 255);
        color.nscale8(brightness);

        strip1[i] = color;
        strip2[i] = color;
    }
}

// ============================================================================
// STRANGE ATTRACTOR 1D
// ============================================================================
// Lorenz or Rossler chaotic attractor projected to 1D
// Chaotic trajectory creates unpredictable but deterministic patterns
//
// Lorenz: dx/dt = σ(y-x), dy/dt = x(ρ-z)-y, dz/dt = xy-βz
// Rossler: dx/dt = -y-z, dy/dt = x+ay, dz/dt = b+z(x-c)
//
// CENTER ORIGIN: Attractor center at LED 79/80
// ============================================================================

void lgpStrangeAttractor() {
    // Lorenz attractor state
    static float lx = 1.0f, ly = 1.0f, lz = 1.0f;
    // Trail history
    static float trail[HardwareConfig::STRIP_LENGTH];
    static uint8_t trailHead = 0;

    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float complexity = visualParams.getComplexityNorm();

    // Lorenz parameters
    float sigma = 10.0f;
    float rho = 28.0f;
    float beta = 8.0f / 3.0f;

    // Time step (smaller = more accurate but slower evolution)
    float dt = speed * 0.01f;

    // Multiple integration steps per frame for smoothness
    for (int step = 0; step < 5; step++) {
        float dx = sigma * (ly - lx);
        float dy = lx * (rho - lz) - ly;
        float dz = lx * ly - beta * lz;

        lx += dx * dt;
        ly += dy * dt;
        lz += dz * dt;
    }

    // Project 3D attractor to 1D strip position
    // Use x coordinate, normalized to strip range
    // Lorenz x typically ranges from -20 to +20
    float normalizedX = lx / 25.0f;  // -0.8 to +0.8
    normalizedX = constrain(normalizedX, -1.0f, 1.0f);

    // Convert to LED position centered on strip center
    int ledPos = HardwareConfig::STRIP_CENTER_POINT +
                (int)(normalizedX * HardwareConfig::STRIP_HALF_LENGTH * 0.9f);
    ledPos = constrain(ledPos, 0, HardwareConfig::STRIP_LENGTH - 1);

    // Update trail (circular buffer)
    trail[trailHead] = ledPos;
    trailHead = (trailHead + 1) % HardwareConfig::STRIP_LENGTH;

    // Fade all LEDs
    for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        strip1[i].fadeToBlackBy(15 + (uint8_t)((1.0f - complexity) * 30));
        strip2[i].fadeToBlackBy(15 + (uint8_t)((1.0f - complexity) * 30));
    }

    // Draw trail with age-based coloring
    int trailLength = 40 + (int)(complexity * 80);  // 40-120 LEDs
    for (int t = 0; t < trailLength; t++) {
        int trailIdx = (trailHead - t - 1 + HardwareConfig::STRIP_LENGTH) %
                       HardwareConfig::STRIP_LENGTH;
        int pos = (int)trail[trailIdx];

        if (pos >= 0 && pos < HardwareConfig::STRIP_LENGTH) {
            float age = (float)t / trailLength;
            float distFromCenter = abs(pos - HardwareConfig::STRIP_CENTER_POINT);
            float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;

            // Younger = brighter, older = dimmer
            uint8_t brightness = (uint8_t)((1.0f - age * 0.7f) * 255 * intensity);

            // Color based on position and age (no rainbow)
            uint8_t paletteIndex = (uint8_t)(normalizedDist * 128 + age * 127);

            CRGB color = ColorFromPalette(currentPalette, paletteIndex, 255);
            color.nscale8(brightness);

            strip1[pos] += color;
            strip2[pos] += color;
        }
    }

    // Bright head of the attractor
    CRGB headColor = ColorFromPalette(currentPalette, 200, 255);
    headColor.nscale8((uint8_t)(255 * intensity));
    strip1[ledPos] = headColor;
    strip2[ledPos] = headColor;
}

// ============================================================================
// KURAMOTO COUPLED OSCILLATORS
// ============================================================================
// Phase synchronization of N coupled oscillators
// dθᵢ/dt = ωᵢ + (K/N)Σⱼsin(θⱼ - θᵢ)
//
// Each LED is an oscillator with natural frequency varying from center
// Shows spontaneous synchronization above critical coupling
//
// CENTER ORIGIN: Frequency distribution centered at LED 79/80
// ============================================================================

// Oscillator phases
static float kPhases[HardwareConfig::STRIP_LENGTH];
static bool kInitialized = false;

void lgpKuramotoOscillators() {
    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float complexity = visualParams.getComplexityNorm();

    // Initialize phases randomly
    if (!kInitialized) {
        for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            kPhases[i] = random8() / 255.0f * TWO_PI;
        }
        kInitialized = true;
    }

    // Coupling strength (above critical = synchronization)
    float K = 1.0f + complexity * 4.0f;  // 1.0 to 5.0

    // Time step
    float dt = speed * 0.05f;

    // Natural frequencies: vary with distance from center
    // Center oscillators are faster, edges are slower (or vice versa)
    static float naturalFreqs[HardwareConfig::STRIP_LENGTH];
    static bool freqsInit = false;
    if (!freqsInit) {
        for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
            float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;
            // Gaussian distribution of frequencies centered at 1.0
            naturalFreqs[i] = 1.0f + (1.0f - normalizedDist) * 0.5f;
        }
        freqsInit = true;
    }

    // Compute order parameter (measure of synchronization)
    float sumCos = 0, sumSin = 0;
    for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        sumCos += cos(kPhases[i]);
        sumSin += sin(kPhases[i]);
    }
    float orderMag = sqrt(sumCos * sumCos + sumSin * sumSin) /
                     HardwareConfig::STRIP_LENGTH;
    float orderPhase = atan2(sumSin, sumCos);

    // Update each oscillator
    // Full N-to-N coupling is expensive, use mean-field approximation
    for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // Mean-field Kuramoto: dθ/dt = ω + K*R*sin(Ψ - θ)
        float dTheta = naturalFreqs[i] + K * orderMag * sin(orderPhase - kPhases[i]);
        kPhases[i] += dTheta * dt;

        // Wrap phase to [0, 2π]
        while (kPhases[i] > TWO_PI) kPhases[i] -= TWO_PI;
        while (kPhases[i] < 0) kPhases[i] += TWO_PI;
    }

    // Render: phase determines color, sync level affects brightness
    for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;

        // Phase as color (normalized to palette)
        uint8_t paletteIndex = (uint8_t)(kPhases[i] / TWO_PI * 255);

        // Brightness: modulated by phase relative to mean field
        float phaseDeviation = abs(sin(kPhases[i] - orderPhase));
        float syncBrightness = 0.5f + 0.5f * (1.0f - phaseDeviation);

        uint8_t brightness = (uint8_t)(syncBrightness * 255 * intensity);

        CRGB color = ColorFromPalette(currentPalette, paletteIndex, 255);
        color.nscale8(brightness);

        strip1[i] = color;
        strip2[i] = color;
    }

    // Visual indicator of synchronization at center
    if (orderMag > 0.7f) {
        // High sync - bright pulse at center
        uint8_t syncPulse = (uint8_t)((orderMag - 0.7f) * 3.0f * 255 * intensity);
        CRGB syncColor = CRGB(syncPulse, syncPulse, syncPulse);
        strip1[HardwareConfig::STRIP_CENTER_POINT] += syncColor;
        strip2[HardwareConfig::STRIP_CENTER_POINT] += syncColor;
        strip1[HardwareConfig::STRIP_CENTER_POINT - 1] += syncColor;
        strip2[HardwareConfig::STRIP_CENTER_POINT - 1] += syncColor;
    }
}
