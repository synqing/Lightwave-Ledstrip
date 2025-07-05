// LGP Quantum-Inspired Effects
// Mind-bending optical effects based on quantum mechanics and exotic physics
// Designed to exploit Light Guide Plate interference for otherworldly visuals

#include <FastLED.h>
#include "../../config/hardware_config.h"
#include "../../core/EffectTypes.h"

// External references
extern CRGB strip1[];
extern CRGB strip2[];
extern CRGB leds[];
extern CRGBPalette16 currentPalette;
extern uint8_t gHue;
extern uint8_t paletteSpeed;
extern VisualParams visualParams;

// ============== QUANTUM TUNNELING ==============
// Particles tunnel through energy barriers with probability waves
void lgpQuantumTunneling() {
    static uint16_t time = 0;
    static uint8_t particlePos[10] = {0};
    static uint8_t particleEnergy[10] = {0};
    static bool particleActive[10] = {false};
    
    time += paletteSpeed >> 1;
    
    // Barrier parameters
    uint8_t barrierCount = 2 + (visualParams.complexity >> 6);  // 2-5 barriers
    uint8_t barrierWidth = 20;
    uint8_t tunnelProbability = visualParams.variation >> 1;  // 0-127
    
    // Fade trails
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 30);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 30);
    
    // Draw energy barriers
    for (uint8_t b = 0; b < barrierCount; b++) {
        uint8_t barrierPos = (b + 1) * HardwareConfig::STRIP_LENGTH / (barrierCount + 1);
        
        for (int8_t w = -barrierWidth/2; w <= barrierWidth/2; w++) {
            int16_t pos = barrierPos + w;
            if (pos >= 0 && pos < HardwareConfig::STRIP_LENGTH) {
                uint8_t barrierBright = 60 - abs(w) * 3;
                strip1[pos] = CHSV(160, 255, barrierBright);  // Cyan barriers
                strip2[pos] = CHSV(160, 255, barrierBright);
            }
        }
    }
    
    // Spawn particles from center
    EVERY_N_MILLISECONDS(500) {
        for (uint8_t p = 0; p < 10; p++) {
            if (!particleActive[p]) {
                particlePos[p] = HardwareConfig::STRIP_CENTER_POINT;
                particleEnergy[p] = 100 + random8(155);
                particleActive[p] = true;
                break;
            }
        }
    }
    
    // Update particles
    for (uint8_t p = 0; p < 10; p++) {
        if (particleActive[p]) {
            // Quantum wave function
            int8_t direction = (p % 2) ? 1 : -1;  // Alternate directions
            
            // Check for barrier collision
            bool atBarrier = false;
            for (uint8_t b = 0; b < barrierCount; b++) {
                uint8_t barrierPos = (b + 1) * HardwareConfig::STRIP_LENGTH / (barrierCount + 1);
                if (abs(particlePos[p] - barrierPos) < barrierWidth/2) {
                    atBarrier = true;
                    
                    // Quantum tunneling probability
                    if (random8() < tunnelProbability) {
                        // TUNNEL THROUGH!
                        particlePos[p] += direction * barrierWidth;
                        
                        // Flash effect at tunnel point
                        for (int8_t f = -5; f <= 5; f++) {
                            int16_t flashPos = particlePos[p] + f;
                            if (flashPos >= 0 && flashPos < HardwareConfig::STRIP_LENGTH) {
                                strip1[flashPos] = CRGB::White;
                                strip2[flashPos] = CRGB::White;
                            }
                        }
                    } else {
                        // Reflect with energy loss
                        direction = -direction;
                        particleEnergy[p] = scale8(particleEnergy[p], 200);
                    }
                    break;
                }
            }
            
            if (!atBarrier) {
                // Normal propagation
                particlePos[p] += direction * 2;
            }
            
            // Deactivate at edges
            if (particlePos[p] <= 0 || particlePos[p] >= HardwareConfig::STRIP_LENGTH - 1) {
                particleActive[p] = false;
                continue;
            }
            
            // Draw particle wave packet
            for (int8_t w = -10; w <= 10; w++) {
                int16_t wavePos = particlePos[p] + w;
                if (wavePos >= 0 && wavePos < HardwareConfig::STRIP_LENGTH) {
                    // Gaussian wave packet
                    uint8_t waveBright = particleEnergy[p] * exp(-abs(w) * 0.2);
                    uint8_t hue = gHue + p * 25;
                    
                    strip1[wavePos] += CHSV(hue, 255, waveBright);
                    strip2[wavePos] += CHSV(hue + 128, 255, waveBright);
                }
            }
            
            // Energy decay
            particleEnergy[p] = scale8(particleEnergy[p], 250);
            if (particleEnergy[p] < 10) {
                particleActive[p] = false;
            }
        }
    }
    
    // Sync to unified buffer
    memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
}

// ============== GRAVITATIONAL LENSING ==============
// Light bends around invisible massive objects creating Einstein rings
void lgpGravitationalLensing() {
    static uint16_t time = 0;
    static float massPos[3] = {40, 80, 120};  // Invisible mass positions
    static float massVel[3] = {0.5, -0.3, 0.4};
    
    time += paletteSpeed >> 2;
    
    // Mass parameters
    uint8_t massCount = 1 + (visualParams.complexity >> 7);  // 1-3 masses
    float massStrength = visualParams.intensity / 255.0f;
    
    // Update mass positions
    for (uint8_t m = 0; m < massCount; m++) {
        massPos[m] += massVel[m];
        
        // Bounce at edges
        if (massPos[m] < 20 || massPos[m] > HardwareConfig::STRIP_LENGTH - 20) {
            massVel[m] = -massVel[m];
        }
    }
    
    // Clear strips
    fill_solid(strip1, HardwareConfig::STRIP_LENGTH, CRGB::Black);
    fill_solid(strip2, HardwareConfig::STRIP_LENGTH, CRGB::Black);
    
    // Generate light rays from center
    for (int16_t ray = -40; ray <= 40; ray += 2) {
        float rayPos = HardwareConfig::STRIP_CENTER_POINT;
        float rayAngle = ray * 0.02f;  // Initial angle
        
        // Trace ray path
        for (uint8_t step = 0; step < 80; step++) {
            // Calculate total gravitational deflection
            float totalDeflection = 0;
            
            for (uint8_t m = 0; m < massCount; m++) {
                float dist = abs(rayPos - massPos[m]);
                if (dist < 40 && dist > 1) {
                    // Einstein deflection angle ≈ 4GM/rc²
                    float deflection = massStrength * 20.0f / (dist * dist);
                    if (rayPos > massPos[m]) {
                        deflection = -deflection;
                    }
                    totalDeflection += deflection;
                }
            }
            
            // Update ray angle
            rayAngle += totalDeflection * 0.01f;
            
            // Update ray position
            rayPos += cos(rayAngle) * 2;
            
            // Draw ray point
            int16_t pixelPos = (int16_t)rayPos;
            if (pixelPos >= 0 && pixelPos < HardwareConfig::STRIP_LENGTH) {
                // Color based on deflection amount (gravitational redshift)
                uint8_t hue = gHue + abs(totalDeflection) * 50;
                uint8_t brightness = 255 - step * 3;
                
                // Einstein ring effect - brighter near critical angles
                if (abs(totalDeflection) > 0.5f) {
                    brightness = 255;
                }
                
                strip1[pixelPos] += CHSV(hue, 255, brightness);
                strip2[pixelPos] += CHSV(hue + 64, 255, brightness);
            }
            
            // Stop if ray exits
            if (rayPos < 0 || rayPos >= HardwareConfig::STRIP_LENGTH) break;
        }
    }
    
    // Draw mass indicators (subtle)
    if (visualParams.variation > 128) {
        for (uint8_t m = 0; m < massCount; m++) {
            int16_t mPos = (int16_t)massPos[m];
            if (mPos >= 0 && mPos < HardwareConfig::STRIP_LENGTH) {
                strip1[mPos] = CHSV(0, 255, 40);  // Dim red
                strip2[mPos] = CHSV(0, 255, 40);
            }
        }
    }
    
    // Sync to unified buffer
    memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
}

// ============== SONIC BOOM SHOCKWAVES ==============
// Mach cone patterns with shock diamonds
void lgpSonicBoom() {
    static uint16_t time = 0;
    static float objectPos = HardwareConfig::STRIP_CENTER_POINT;
    static float objectVel = 2.0f;
    static uint16_t shockHistory[HardwareConfig::STRIP_LENGTH] = {0};
    
    time += paletteSpeed >> 1;
    
    // Object parameters
    float machNumber = 1.0f + (visualParams.intensity / 255.0f) * 3.0f;  // Mach 1-4
    uint8_t shockPersistence = 200 + (visualParams.complexity >> 2);
    
    // Update object position
    objectPos += objectVel * machNumber;
    
    // Bounce at edges
    if (objectPos < 10 || objectPos > HardwareConfig::STRIP_LENGTH - 10) {
        objectVel = -objectVel;
        objectPos = constrain(objectPos, 10, HardwareConfig::STRIP_LENGTH - 10);
    }
    
    // Fade shock history
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        if (shockHistory[i] > 0) {
            shockHistory[i] = scale8(shockHistory[i], shockPersistence);
        }
    }
    
    // Create new shock at object position
    int16_t objPixel = (int16_t)objectPos;
    if (objPixel >= 0 && objPixel < HardwareConfig::STRIP_LENGTH) {
        shockHistory[objPixel] = 255;
    }
    
    // Render shockwaves
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        if (shockHistory[i] > 0) {
            // Calculate Mach cone angle
            float distFromObject = abs((float)i - objectPos);
            float coneAngle = asin(1.0f / machNumber);
            float coneWidth = distFromObject * tan(coneAngle);
            
            // Shock intensity with diamond pattern
            uint8_t shockIntensity = shockHistory[i];
            
            // Add shock diamonds (periodic compressions)
            float diamondPhase = distFromObject * 0.3f - time * 0.1f;
            uint8_t diamondIntensity = 128 + 127 * sin(diamondPhase);
            shockIntensity = scale8(shockIntensity, diamondIntensity);
            
            // Color shift based on shock strength (hotter = bluer)
            uint8_t hue = 32 - (shockIntensity >> 3);  // Orange to blue
            
            strip1[i] = CHSV(hue, 255, shockIntensity);
            strip2[i] = CHSV(hue + 16, 255, scale8(shockIntensity, 200));
        }
    }
    
    // Draw supersonic object
    for (int8_t w = -3; w <= 3; w++) {
        int16_t pos = objPixel + w;
        if (pos >= 0 && pos < HardwareConfig::STRIP_LENGTH) {
            strip1[pos] = CRGB::White;
            strip2[pos] = CRGB::White;
        }
    }
    
    // Sync to unified buffer
    memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
}

// ============== TIME CRYSTAL OSCILLATOR ==============
// Perpetual motion patterns with non-repeating periods
void lgpTimeCrystal() {
    static uint16_t phase1 = 0;
    static uint16_t phase2 = 0;
    static uint16_t phase3 = 0;
    
    // Non-commensurate frequencies for quasi-periodic behavior
    phase1 += paletteSpeed * 1.0f;
    phase2 += paletteSpeed * 1.618f;  // Golden ratio
    phase3 += paletteSpeed * 2.718f;  // e
    
    // Crystal parameters
    uint8_t crystallinity = visualParams.intensity;
    uint8_t dimensions = 1 + (visualParams.complexity >> 6);  // 1-4D
    
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT) / HardwareConfig::STRIP_CENTER_POINT;
        
        // Multi-dimensional crystal oscillations
        float crystal = 0;
        
        // Dimension 1: Basic oscillation
        crystal += sin16(phase1 + i * 400) / 32768.0f;
        
        if (dimensions >= 2) {
            // Dimension 2: Modulated by golden ratio
            crystal += sin16(phase2 + i * 650) / 65536.0f;
        }
        
        if (dimensions >= 3) {
            // Dimension 3: Modulated by e
            crystal += sin16(phase3 + i * 1050) / 131072.0f;
        }
        
        if (dimensions >= 4) {
            // Dimension 4: Coupled oscillators
            crystal += sin16(phase1 + phase2 - i * 250) / 262144.0f;
        }
        
        // Normalize and apply crystallinity
        crystal = crystal / dimensions;
        uint8_t brightness = 128 + (int8_t)(crystal * crystallinity);
        
        // Time crystal refraction creates rainbow effects
        uint8_t hue = gHue + (uint8_t)(crystal * 100) + i/2;
        
        // Phase-locked regions create structure
        if (abs(crystal) > 0.9f) {
            brightness = 255;
            hue = gHue;  // Lock color in resonant zones
        }
        
        strip1[i] = CHSV(hue, 200, brightness);
        strip2[i] = CHSV(hue + 85, 200, brightness);
    }
    
    // Sync to unified buffer
    memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
}

// ============== SOLITON WAVES ==============
// Self-reinforcing wave packets that maintain shape
void lgpSolitonWaves() {
    static float solitonPos[4] = {20, 60, 100, 140};
    static float solitonVel[4] = {1.0f, -0.8f, 1.2f, -1.1f};
    static uint8_t solitonAmp[4] = {255, 200, 230, 180};
    static uint8_t solitonHue[4] = {0, 60, 120, 180};
    
    // Soliton parameters
    uint8_t solitonCount = 2 + (visualParams.complexity >> 6);  // 2-5 solitons
    float damping = 1.0f - (visualParams.variation / 25500.0f);  // Energy conservation
    
    // Clear with fade
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 20);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 20);
    
    // Update solitons
    for (uint8_t s = 0; s < solitonCount; s++) {
        // Update position
        solitonPos[s] += solitonVel[s] * (paletteSpeed / 128.0f);
        
        // Boundary reflection
        if (solitonPos[s] < 0 || solitonPos[s] >= HardwareConfig::STRIP_LENGTH) {
            solitonVel[s] = -solitonVel[s];
            solitonPos[s] = constrain(solitonPos[s], 0, HardwareConfig::STRIP_LENGTH - 1);
        }
        
        // Check for collisions
        for (uint8_t other = s + 1; other < solitonCount; other++) {
            float dist = abs(solitonPos[s] - solitonPos[other]);
            if (dist < 10) {
                // Soliton collision - exchange properties
                float tempVel = solitonVel[s];
                solitonVel[s] = solitonVel[other];
                solitonVel[other] = tempVel;
                
                // Energy flash at collision point
                int16_t collisionPos = (solitonPos[s] + solitonPos[other]) / 2;
                if (collisionPos >= 0 && collisionPos < HardwareConfig::STRIP_LENGTH) {
                    strip1[collisionPos] = CRGB::White;
                    strip2[collisionPos] = CRGB::White;
                }
            }
        }
        
        // Draw soliton - sech² profile
        for (int16_t dx = -20; dx <= 20; dx++) {
            int16_t pos = (int16_t)solitonPos[s] + dx;
            if (pos >= 0 && pos < HardwareConfig::STRIP_LENGTH) {
                // Hyperbolic secant squared profile
                float sech = 1.0f / cosh(dx * 0.15f);
                float profile = sech * sech;
                
                uint8_t brightness = solitonAmp[s] * profile;
                uint8_t hue = solitonHue[s] + gHue;
                
                strip1[pos] += CHSV(hue, 255, brightness);
                strip2[pos] += CHSV(hue + 30, 255, scale8(brightness, 200));
            }
        }
        
        // Apply damping
        solitonAmp[s] = solitonAmp[s] * damping;
        
        // Regenerate dead solitons
        if (solitonAmp[s] < 50) {
            solitonPos[s] = random16(HardwareConfig::STRIP_LENGTH);
            solitonVel[s] = (random8(2) ? 1 : -1) * (0.5f + random8(100) / 100.0f);
            solitonAmp[s] = 200 + random8(55);
            solitonHue[s] = random8();
        }
    }
    
    // Sync to unified buffer
    memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
}

// ============== METAMATERIAL CLOAKING ==============
// Negative refractive index creates invisibility effects
void lgpMetamaterialCloaking() {
    static uint16_t time = 0;
    static float cloakPos = HardwareConfig::STRIP_CENTER_POINT;
    static float cloakVel = 0.5f;
    
    time += paletteSpeed >> 2;
    
    // Cloak parameters
    float cloakRadius = 10 + (visualParams.complexity >> 4);  // 10-25 pixels
    float refractiveIndex = -1.0f - (visualParams.intensity / 255.0f);  // -1 to -2
    
    // Update cloak position
    cloakPos += cloakVel;
    if (cloakPos < cloakRadius || cloakPos > HardwareConfig::STRIP_LENGTH - cloakRadius) {
        cloakVel = -cloakVel;
    }
    
    // Background wave pattern
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // Plane waves from left
        uint8_t wave = sin8(i * 4 + (time >> 2));
        uint8_t hue = gHue + (i >> 2);
        
        // Check if within cloak region
        float distFromCloak = abs((float)i - cloakPos);
        
        if (distFromCloak < cloakRadius) {
            // Inside metamaterial - negative refraction
            float bendAngle = (distFromCloak / cloakRadius) * PI;
            
            // Light bends backwards
            wave = sin8(i * 4 * refractiveIndex + (time >> 2) + bendAngle * 128);
            
            // Phase shift creates invisibility
            if (distFromCloak < cloakRadius * 0.5f) {
                // Perfect cloaking region - destructive interference
                wave = scale8(wave, 255 * (distFromCloak / (cloakRadius * 0.5f)));
            }
            
            // Edge glow from trapped surface waves
            if (abs(distFromCloak - cloakRadius) < 2) {
                wave = 255;
                hue = 160;  // Cyan edge
            }
        }
        
        strip1[i] = CHSV(hue, 200, wave);
        strip2[i] = CHSV(hue + 128, 200, wave);
    }
    
    // Sync to unified buffer
    memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
}