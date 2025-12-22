/**
 * @file LGPQuantumEffects.cpp
 * @brief LGP Quantum-inspired effects implementation
 *
 * Mind-bending optical effects based on quantum mechanics and exotic physics.
 * All effects use CENTER ORIGIN pattern (LEDs 79/80 as center).
 */

#include "LGPQuantumEffects.h"
#include "CoreEffects.h"
#include <FastLED.h>
#include <math.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif
#ifndef TWO_PI
#define TWO_PI (2.0f * PI)
#endif

namespace lightwaveos {
namespace effects {

using namespace lightwaveos::actors;

// ==================== Static State ====================
// Quantum Tunneling
static uint16_t tunnelTime = 0;
static uint8_t particlePos[10] = {0};
static uint8_t particleEnergy[10] = {0};
static bool particleActive[10] = {false};

// Gravitational Lensing
static uint16_t lensTime = 0;
static float massPos[3] = {40, 80, 120};
static float massVel[3] = {0.5f, -0.3f, 0.4f};

// Time Crystal
static uint16_t tcPhase1 = 0, tcPhase2 = 0, tcPhase3 = 0;

// Soliton Waves
static float solitonPos[4] = {20, 60, 100, 140};
static float solitonVel[4] = {1.0f, -0.8f, 1.2f, -1.1f};
static uint8_t solitonAmp[4] = {255, 200, 230, 180};
static uint8_t solitonHue[4] = {0, 60, 120, 180};

// Metamaterial Cloaking
static uint16_t cloakTime = 0;
static float cloakPos = 80.0f;
static float cloakVel = 0.5f;

// GRIN Cloak
static float grinPos = 80.0f;
static float grinVel = 0.35f;

// Caustic Fan
static uint16_t causticTime = 0;

// Birefringent Shear
static uint16_t birefringentTime = 0;

// Anisotropic Cloak
static float anisoPos = 80.0f;
static float anisoVel = 0.45f;

// Evanescent Skin
static uint16_t evanescentTime = 0;

// ==================== QUANTUM TUNNELING ====================
void effectQuantumTunneling(RenderContext& ctx) {
    // Particles tunnel through energy barriers with probability waves

    tunnelTime += ctx.speed >> 1;

    uint8_t barrierCount = 3;
    uint8_t barrierWidth = 20;
    uint8_t tunnelProbability = 64;

    fadeToBlackBy(ctx.leds, ctx.numLeds, 30);

    // Draw energy barriers
    for (uint8_t b = 0; b < barrierCount; b++) {
        uint8_t barrierPos = (b + 1) * STRIP_LENGTH / (barrierCount + 1);

        for (int8_t w = -barrierWidth / 2; w <= barrierWidth / 2; w++) {
            int16_t pos = barrierPos + w;
            if (pos >= 0 && pos < STRIP_LENGTH) {
                uint8_t barrierBright = 60 - abs(w) * 3;
                ctx.leds[pos] = CHSV(160, 255, barrierBright);
                if (pos + STRIP_LENGTH < ctx.numLeds) {
                    ctx.leds[pos + STRIP_LENGTH] = CHSV(160, 255, barrierBright);
                }
            }
        }
    }

    // Spawn particles from center periodically
    if (ctx.frameCount % 60 == 0) {
        for (uint8_t p = 0; p < 10; p++) {
            if (!particleActive[p]) {
                particlePos[p] = CENTER_LEFT;
                particleEnergy[p] = 100 + random8(155);
                particleActive[p] = true;
                break;
            }
        }
    }

    // Update particles
    for (uint8_t p = 0; p < 10; p++) {
        if (particleActive[p]) {
            int8_t direction = (p % 2) ? 1 : -1;

            // Check for barrier collision
            bool atBarrier = false;
            for (uint8_t b = 0; b < barrierCount; b++) {
                uint8_t barrierPos = (b + 1) * STRIP_LENGTH / (barrierCount + 1);
                if (abs(particlePos[p] - barrierPos) < barrierWidth / 2) {
                    atBarrier = true;

                    if (random8() < tunnelProbability) {
                        // TUNNEL THROUGH!
                        particlePos[p] += direction * barrierWidth;

                        // Flash effect - use particle color instead of white
                        uint8_t flashHue = ctx.hue + p * 25;
                        for (int8_t f = -5; f <= 5; f++) {
                            int16_t flashPos = particlePos[p] + f;
                            if (flashPos >= 0 && flashPos < STRIP_LENGTH) {
                                uint8_t flashBright = 255 - abs(f) * 20;
                                ctx.leds[flashPos] = CHSV(flashHue, 255, flashBright);
                                if (flashPos + STRIP_LENGTH < ctx.numLeds) {
                                    ctx.leds[flashPos + STRIP_LENGTH] = CHSV(flashHue + 128, 255, flashBright);
                                }
                            }
                        }
                    } else {
                        // Reflect with energy loss
                        particleEnergy[p] = scale8(particleEnergy[p], 200);
                    }
                    break;
                }
            }

            if (!atBarrier) {
                particlePos[p] += direction * 2;
            }

            // Deactivate at edges
            if (particlePos[p] <= 0 || particlePos[p] >= STRIP_LENGTH - 1) {
                particleActive[p] = false;
                continue;
            }

            // Draw particle wave packet
            for (int8_t w = -10; w <= 10; w++) {
                int16_t wavePos = particlePos[p] + w;
                if (wavePos >= 0 && wavePos < STRIP_LENGTH) {
                    uint8_t waveBright = particleEnergy[p] * exp(-abs(w) * 0.2f);
                    uint8_t hue = ctx.hue + p * 25;

                    ctx.leds[wavePos] += CHSV(hue, 255, waveBright);
                    if (wavePos + STRIP_LENGTH < ctx.numLeds) {
                        ctx.leds[wavePos + STRIP_LENGTH] += CHSV(hue + 128, 255, waveBright);
                    }
                }
            }

            // Energy decay
            particleEnergy[p] = scale8(particleEnergy[p], 250);
            if (particleEnergy[p] < 10) {
                particleActive[p] = false;
            }
        }
    }
}

// ==================== GRAVITATIONAL LENSING ====================
void effectGravitationalLensing(RenderContext& ctx) {
    // Light bends around invisible massive objects creating Einstein rings

    lensTime += ctx.speed >> 2;

    float speedNorm = ctx.speed / 50.0f;
    uint8_t massCount = 2;
    float massStrength = ctx.brightness / 255.0f;

    // Update mass positions
    for (uint8_t m = 0; m < massCount; m++) {
        massPos[m] += massVel[m] * speedNorm;

        if (massPos[m] < 20 || massPos[m] > STRIP_LENGTH - 20) {
            massVel[m] = -massVel[m];
        }
    }

    fill_solid(ctx.leds, ctx.numLeds, CRGB::Black);

    // Generate light rays from center
    for (int16_t ray = -40; ray <= 40; ray += 2) {
        for (int8_t direction = -1; direction <= 1; direction += 2) {
            float rayPos = CENTER_LEFT;
            float rayAngle = ray * 0.02f * direction;

            for (uint8_t step = 0; step < 80; step++) {
                float totalDeflection = 0;

                for (uint8_t m = 0; m < massCount; m++) {
                    float dist = abs(rayPos - massPos[m]);
                    if (dist < 40 && dist > 1) {
                        float deflection = massStrength * 20.0f / (dist * dist);
                        if (rayPos > massPos[m]) {
                            deflection = -deflection;
                        }
                        totalDeflection += deflection;
                    }
                }

                rayAngle += totalDeflection * 0.01f;
                rayPos += cos(rayAngle) * 2 * direction;

                int16_t pixelPos = (int16_t)rayPos;
                if (pixelPos >= 0 && pixelPos < STRIP_LENGTH) {
                    uint8_t paletteIndex = (uint8_t)(abs(totalDeflection) * 20);
                    uint8_t brightness = 255 - step * 3;

                    // Clamp brightness to avoid white saturation
                    if (abs(totalDeflection) > 0.5f) {
                        brightness = 240;  // Slightly below 255 to avoid white guardrail
                        paletteIndex = (uint8_t)(abs(totalDeflection) * 30);  // Use deflection-based color
                    }

                    ctx.leds[pixelPos] += ColorFromPalette(*ctx.palette, ctx.hue + paletteIndex, brightness);
                    if (pixelPos + STRIP_LENGTH < ctx.numLeds) {
                        ctx.leds[pixelPos + STRIP_LENGTH] += ColorFromPalette(*ctx.palette, ctx.hue + paletteIndex + 64, brightness);
                    }
                }

                if (rayPos < 0 || rayPos >= STRIP_LENGTH) break;
            }
        }
    }
}

// ==================== TIME CRYSTAL ====================
void effectTimeCrystal(RenderContext& ctx) {
    // Perpetual motion patterns with non-repeating periods

    float speedNorm = ctx.speed / 50.0f;

    tcPhase1 += (uint16_t)(speedNorm * 100);
    tcPhase2 += (uint16_t)(speedNorm * 161.8f);  // Golden ratio
    tcPhase3 += (uint16_t)(speedNorm * 271.8f);  // e

    uint8_t crystallinity = ctx.brightness;
    uint8_t dimensions = 3;

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i) / (float)HALF_LENGTH;

        float crystal = 0;

        // Dimension 1
        crystal += sin16(tcPhase1 + i * 400) / 32768.0f;

        // Dimension 2
        crystal += sin16(tcPhase2 + i * 650) / 65536.0f;

        // Dimension 3
        crystal += sin16(tcPhase3 + i * 1050) / 131072.0f;

        crystal = crystal / dimensions;
        uint8_t brightness = 128 + (int8_t)(crystal * crystallinity);

        uint8_t paletteIndex = (uint8_t)(crystal * 20) + (uint8_t)(distFromCenter * 20);

        // Use bright color from palette instead of white (paletteIndex=0)
        if (abs(crystal) > 0.9f) {
            brightness = 240;  // Slightly below 255 to avoid white guardrail
            paletteIndex = (uint8_t)(abs(crystal) * 50);  // Use crystal value for color
        }

        ctx.leds[i] = ColorFromPalette(*ctx.palette, ctx.hue + paletteIndex, brightness);
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = ColorFromPalette(*ctx.palette, ctx.hue + paletteIndex + 85, brightness);
        }
    }
}

// ==================== SOLITON WAVES ====================
void effectSolitonWaves(RenderContext& ctx) {
    // Self-reinforcing wave packets that maintain shape

    float speedNorm = ctx.speed / 50.0f;
    uint8_t solitonCount = 4;
    float damping = 0.996f;

    fadeToBlackBy(ctx.leds, ctx.numLeds, 20);

    for (uint8_t s = 0; s < solitonCount; s++) {
        solitonPos[s] += solitonVel[s] * speedNorm;

        if (solitonPos[s] < 0 || solitonPos[s] >= STRIP_LENGTH) {
            solitonVel[s] = -solitonVel[s];
            solitonPos[s] = constrain(solitonPos[s], 0.0f, (float)(STRIP_LENGTH - 1));
        }

        // Check for collisions
        for (uint8_t other = s + 1; other < solitonCount; other++) {
            float dist = abs(solitonPos[s] - solitonPos[other]);
            if (dist < 10) {
                float tempVel = solitonVel[s];
                solitonVel[s] = solitonVel[other];
                solitonVel[other] = tempVel;

                int16_t collisionPos = (solitonPos[s] + solitonPos[other]) / 2;
                if (collisionPos >= 0 && collisionPos < STRIP_LENGTH) {
                    // Use blended color from colliding solitons instead of white
                    uint8_t blendHue = (solitonHue[s] + solitonHue[other]) / 2;
                    uint8_t blendBright = qadd8(solitonAmp[s], solitonAmp[other]) / 2;
                    ctx.leds[collisionPos] = CHSV(blendHue + ctx.hue, 255, blendBright);
                    if (collisionPos + STRIP_LENGTH < ctx.numLeds) {
                        ctx.leds[collisionPos + STRIP_LENGTH] = CHSV(blendHue + ctx.hue + 30, 255, scale8(blendBright, 200));
                    }
                }
            }
        }

        // Draw soliton - sech^2 profile
        for (int16_t dx = -20; dx <= 20; dx++) {
            int16_t pos = (int16_t)solitonPos[s] + dx;
            if (pos >= 0 && pos < STRIP_LENGTH) {
                float sech = 1.0f / cosh(dx * 0.15f);
                float profile = sech * sech;

                uint8_t brightness = (uint8_t)(solitonAmp[s] * profile);
                uint8_t hue = solitonHue[s] + ctx.hue;

                ctx.leds[pos] += CHSV(hue, 255, brightness);
                if (pos + STRIP_LENGTH < ctx.numLeds) {
                    ctx.leds[pos + STRIP_LENGTH] += CHSV(hue + 30, 255, scale8(brightness, 200));
                }
            }
        }

        solitonAmp[s] = (uint8_t)(solitonAmp[s] * damping);

        // Regenerate dead solitons
        if (solitonAmp[s] < 50) {
            solitonPos[s] = random16(STRIP_LENGTH);
            solitonVel[s] = (random8(2) ? 1 : -1) * (0.5f + random8(100) / 100.0f);
            solitonAmp[s] = 200 + random8(55);
            solitonHue[s] = random8();
        }
    }
}

// ==================== METAMATERIAL CLOAKING ====================
void effectMetamaterialCloaking(RenderContext& ctx) {
    // Negative refractive index creates invisibility effects

    cloakTime += ctx.speed >> 2;

    float speedNorm = ctx.speed / 50.0f;
    float cloakRadius = 15.0f;
    float refractiveIndex = -1.5f;

    cloakPos += cloakVel * speedNorm;
    if (cloakPos < cloakRadius || cloakPos > STRIP_LENGTH - cloakRadius) {
        cloakVel = -cloakVel;
    }

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        uint8_t wave = sin8(i * 4 + (cloakTime >> 2));
        uint8_t hue = ctx.hue + (i >> 2);

        float distFromCloak = abs((float)i - cloakPos);

        if (distFromCloak < cloakRadius) {
            float bendAngle = (distFromCloak / cloakRadius) * PI;
            wave = sin8((int)(i * 4 * refractiveIndex) + (cloakTime >> 2) + (int)(bendAngle * 128));

            if (distFromCloak < cloakRadius * 0.5f) {
                wave = scale8(wave, (uint8_t)(255 * (distFromCloak / (cloakRadius * 0.5f))));
            }

            if (abs(distFromCloak - cloakRadius) < 2) {
                wave = 255;
                hue = 160;
            }
        }

        ctx.leds[i] = CHSV(hue, 200, wave);
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = CHSV(hue + 128, 200, wave);
        }
    }
}

// ==================== GRIN CLOAK ====================
void effectGrinCloak(RenderContext& ctx) {
    // Smooth gradient refractive profile emulating GRIN optics

    static uint16_t grinTime = 0;
    grinTime += ctx.speed >> 1;

    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    float cloakRadius = 20.0f;
    float exponent = 2.0f;
    float gradient = 1.5f;

    grinPos += grinVel * speedNorm;
    if (grinPos < cloakRadius || grinPos > STRIP_LENGTH - cloakRadius) {
        grinVel = -grinVel;
    }

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float dist = abs((float)i - grinPos);
        float norm = (cloakRadius > 0.001f) ? (dist / cloakRadius) : 0.0f;
        norm = constrain(norm, 0.0f, 1.0f);

        float lensStrength = gradient * pow(norm, exponent);
        float direction = (i < grinPos) ? -1.0f : 1.0f;
        float sample = (float)i + direction * lensStrength * cloakRadius * 0.6f;
        sample = constrain(sample, 0.0f, (float)(STRIP_LENGTH - 1));

        uint8_t wave = sin8((int16_t)(sample * 4.0f) + (grinTime >> 2));

        float focusGain = 1.0f + (1.0f - norm) * gradient * 0.3f;
        float brightnessF = wave * focusGain;

        if (norm < 0.3f) {
            brightnessF *= norm / 0.3f;
        }

        if (abs(norm - 1.0f) < 0.08f) {
            brightnessF = 255.0f;
        }

        uint8_t brightness = (uint8_t)constrain(brightnessF, 0.0f, 255.0f);
        uint8_t hue = ctx.hue + (uint8_t)(sample * 1.5f);

        ctx.leds[i] = CHSV(hue, 200, brightness);
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = CHSV(hue + 128, 200, brightness);
        }
    }
}

// ==================== CAUSTIC FAN ====================
void effectCausticFan(RenderContext& ctx) {
    // Two virtual focusing fans creating drifting caustic envelopes

    causticTime += ctx.speed >> 2;

    float intensityNorm = ctx.brightness / 255.0f;
    float curvature = 1.5f;
    float separation = 1.5f;
    float gain = 12.0f;
    float animPhase = causticTime / 256.0f;

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float x = (float)i - CENTER_LEFT;

        float def1 = curvature * (x - separation) + sin(animPhase);
        float def2 = -curvature * (x + separation) + sin(animPhase * 1.21f);
        float diff = abs(def1 - def2);

        float caustic = 1.0f / (1.0f + diff * diff * gain);
        float envelope = 1.0f / (1.0f + abs(x) * 0.08f);
        float brightnessF = caustic * envelope * 255.0f;

        brightnessF = constrain(brightnessF + (sin8(i * 3 + (causticTime >> 2)) >> 2), 0.0f, 255.0f);

        uint8_t brightness = (uint8_t)brightnessF;
        uint8_t hue = ctx.hue + (uint8_t)(x * 1.5f) + (causticTime >> 4);

        ctx.leds[i] = CHSV(hue, 200, brightness);
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = CHSV(hue + 96, 200, brightness);
        }
    }
}

// ==================== BIREFRINGENT SHEAR ====================
void effectBirefringentShear(RenderContext& ctx) {
    // Dual spatial modes slipping past one another

    birefringentTime += ctx.speed >> 1;

    float intensityNorm = ctx.brightness / 255.0f;
    float baseFrequency = 3.5f;
    float deltaK = 1.5f;
    float drift = 0.3f;

    uint8_t mixWave = (uint8_t)(intensityNorm * 255.0f);
    uint8_t mixCarrier = 255 - mixWave;

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float idx = (float)i;

        float phaseBase = birefringentTime / 128.0f;
        float phase1 = idx * (baseFrequency + deltaK) + phaseBase;
        float phase2 = idx * (baseFrequency - deltaK) - phaseBase + drift * idx * 0.05f;

        uint8_t wave1 = sin8((int16_t)(phase1 * 16.0f));
        uint8_t wave2 = sin8((int16_t)(phase2 * 16.0f));

        uint8_t combined = qadd8(scale8(wave1, mixCarrier), scale8(wave2, mixWave));
        uint8_t beat = (uint8_t)abs((int)wave1 - (int)wave2);
        uint8_t brightness = qadd8(combined, scale8(beat, 96));

        uint8_t hue1 = ctx.hue + (uint8_t)(idx) + (birefringentTime >> 4);
        uint8_t hue2 = hue1 + 128;

        ctx.leds[i] = CHSV(hue1, 200, brightness);
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = CHSV(hue2, 200, brightness);
        }
    }
}

// ==================== ANISOTROPIC CLOAK ====================
void effectAnisotropicCloak(RenderContext& ctx) {
    // Directionally biased refractive shell

    static uint16_t anisoTime = 0;
    anisoTime += ctx.speed >> 2;

    float speedNorm = ctx.speed / 50.0f;
    float cloakRadius = 20.0f;
    float baseIndex = 1.0f;
    float anisotropy = 0.5f;

    anisoPos += anisoVel * speedNorm;
    if (anisoPos < cloakRadius || anisoPos > STRIP_LENGTH - cloakRadius) {
        anisoVel = -anisoVel;
    }

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float dist = abs((float)i - anisoPos);
        float norm = (cloakRadius > 0.001f) ? (dist / cloakRadius) : 0.0f;
        norm = constrain(norm, 0.0f, 1.0f);

        float sideBias = (i < anisoPos) ? (1.0f + anisotropy) : (1.0f - anisotropy);
        sideBias = constrain(sideBias, -2.0f, 2.0f);

        float offset = baseIndex * pow(norm, 1.5f) * sideBias * cloakRadius * 0.5f;
        float sample = (float)i + ((i < anisoPos) ? -offset : offset);
        sample = constrain(sample, 0.0f, (float)(STRIP_LENGTH - 1));

        uint8_t wave = sin8((int16_t)(sample * 4.0f) + (anisoTime >> 2));
        float brightnessF = wave;

        if (norm < 0.25f) {
            brightnessF *= norm / 0.25f;
        }
        if (abs(norm - 1.0f) < 0.06f) {
            brightnessF = 255.0f;
        }

        uint8_t hue = ctx.hue + (uint8_t)(sample) + (uint8_t)(sideBias * 20.0f);

        ctx.leds[i] = CHSV(hue, 200, (uint8_t)constrain(brightnessF, 0.0f, 255.0f));
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = CHSV(hue + 128, 200, (uint8_t)constrain(brightnessF, 0.0f, 255.0f));
        }
    }
}

// ==================== EVANESCENT SKIN ====================
void effectEvanescentSkin(RenderContext& ctx) {
    // Thin shimmering layers hugging rims or edges

    evanescentTime += ctx.speed >> 2;

    float intensityNorm = ctx.brightness / 255.0f;
    bool rimMode = true;
    float lambda = 4.0f;
    float skinFreq = 7.0f;
    float anim = evanescentTime / 256.0f;

    float ringRadius = HALF_LENGTH * 0.6f;

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float brightnessF;
        float hue = ctx.hue + (i >> 1);

        if (rimMode) {
            float distFromCenter = (float)centerPairDistance(i);
            float skinDistance = abs(distFromCenter - ringRadius);
            float envelope = 1.0f / (1.0f + lambda * skinDistance);
            float carrier = sin(distFromCenter * skinFreq * 0.05f + anim * TWO_PI);
            brightnessF = envelope * (carrier * 0.5f + 0.5f) * 255.0f;
        } else {
            uint16_t edgeDistance = min((uint16_t)i, (uint16_t)(STRIP_LENGTH - 1 - i));
            float distToEdge = (float)edgeDistance;
            float envelope = 1.0f / (1.0f + lambda * distToEdge * 0.4f);
            float carrier = sin((STRIP_LENGTH - distToEdge) * skinFreq * 0.04f - anim * TWO_PI);
            brightnessF = envelope * (carrier * 0.5f + 0.5f) * 255.0f;
        }

        brightnessF = constrain(brightnessF, 0.0f, 255.0f);
        uint8_t brightness = (uint8_t)brightnessF;

        ctx.leds[i] = CHSV((uint8_t)hue, 200, brightness);
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = CHSV((uint8_t)hue + 128, 200, brightness);
        }
    }
}

// ==================== EFFECT REGISTRATION ====================

uint8_t registerLGPQuantumEffects(RendererActor* renderer, uint8_t startId) {
    if (!renderer) return 0;

    uint8_t count = 0;

    if (renderer->registerEffect(startId + count, "LGP Quantum Tunneling", effectQuantumTunneling)) count++;
    if (renderer->registerEffect(startId + count, "LGP Gravitational Lensing", effectGravitationalLensing)) count++;
    if (renderer->registerEffect(startId + count, "LGP Time Crystal", effectTimeCrystal)) count++;
    if (renderer->registerEffect(startId + count, "LGP Soliton Waves", effectSolitonWaves)) count++;
    if (renderer->registerEffect(startId + count, "LGP Metamaterial Cloak", effectMetamaterialCloaking)) count++;
    if (renderer->registerEffect(startId + count, "LGP GRIN Cloak", effectGrinCloak)) count++;
    if (renderer->registerEffect(startId + count, "LGP Caustic Fan", effectCausticFan)) count++;
    if (renderer->registerEffect(startId + count, "LGP Birefringent Shear", effectBirefringentShear)) count++;
    if (renderer->registerEffect(startId + count, "LGP Anisotropic Cloak", effectAnisotropicCloak)) count++;
    if (renderer->registerEffect(startId + count, "LGP Evanescent Skin", effectEvanescentSkin)) count++;

    return count;
}

} // namespace effects
} // namespace lightwaveos
