// LGP Organic Wave Patterns
// Living, breathing, biological chaos patterns that simulate life itself
// These effects transform the Light Guide Plate into a living organism

#include <FastLED.h>
#include <math.h>
#include "../../config/hardware_config.h"
#include "../../core/EffectTypes.h"

// Math constants
#ifndef PI
#define PI 3.14159265358979323846
#endif
#ifndef TWO_PI
#define TWO_PI (2.0 * PI)
#endif

// External references
extern CRGB strip1[];
extern CRGB strip2[];
extern CRGBPalette16 currentPalette;
extern uint8_t gHue;
extern uint8_t paletteSpeed;
extern VisualParams visualParams;

// ============== LGP BIOLUMINESCENT PLANKTON WAVES ==============
// Living light ocean dynamics with dinoflagellate simulation
void lgpBioluminescentPlanktonWaves() {
    // ENCODER MAPPING:
    // Speed (3): Ocean current velocity/wave speed
    // Intensity (4): Bioluminescence brightness/trigger sensitivity
    // Saturation (5): Color saturation
    // Complexity (6): Plankton density/population
    // Variation (7): Species type (dinoflagellates/jellyfish/bacteria)
    
    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float saturation = visualParams.getSaturationNorm();
    float complexity = visualParams.getComplexityNorm();
    float variation = visualParams.getVariationNorm();
    
    static float planktonState[320];
    static float glowIntensity[320];
    static float wavePhase = 0;
    static float disturbance[320];
    static bool initialized = false;
    
    if (!initialized) {
        for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            planktonState[i] = random(100) / 100.0f;
            glowIntensity[i] = 0;
            disturbance[i] = 0;
        }
        initialized = true;
    }
    
    wavePhase += speed * 0.05f;
    float planktonDensity = 0.1f + (complexity * 0.8f);  // 0.1-0.9 population density
    
    // CENTER ORIGIN: Ocean wave dynamics - symmetric waves from center
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // Calculate distance from center for symmetry
        float distFromCenter = abs((int)i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;

        // Multiple wave components for realistic ocean (symmetric)
        float wave1 = sin(normalizedDist * 4 * PI + wavePhase) * 0.5f;
        float wave2 = sin(normalizedDist * 7 * PI - wavePhase * 0.7f) * 0.3f;
        float wave3 = sin(normalizedDist * 11 * PI + wavePhase * 1.3f) * 0.2f;
        
        float totalWave = wave1 + wave2 + wave3;
        
        // Wave creates mechanical disturbance
        disturbance[i] = abs(totalWave - planktonState[i]) * intensity;
        
        // Plankton bioluminescence response
        if (disturbance[i] > 0.3f) {
            // Triggered! Luciferin-luciferase reaction
            glowIntensity[i] = 1.0f;
        } else {
            // Exponential decay of glow
            glowIntensity[i] *= 0.95f;
        }
        
        // Different species behaviors
        if (variation < 0.33f) {
            // Dinoflagellates - classic blue flash
            if (glowIntensity[i] > 0.1f) {
                // Propagate glow to neighbors (chemical signaling)
                if (i > 0) glowIntensity[i-1] = max(glowIntensity[i-1], glowIntensity[i] * 0.5f);
                if (i < HardwareConfig::STRIP_LENGTH-1) glowIntensity[i+1] = max(glowIntensity[i+1], glowIntensity[i] * 0.5f);
            }
        } else if (variation < 0.66f) {
            // Jellyfish - pulsing patterns
            float pulsePhase = wavePhase * 2 + i * 0.1f;
            glowIntensity[i] += 0.3f * sin(pulsePhase) * planktonDensity;
        } else {
            // Marine bacteria - steady glow with quorum sensing
            float neighbors = 0;
            for(int j = max(0, i-3); j <= min(HardwareConfig::STRIP_LENGTH-1, i+3); j++) {
                if (planktonState[j] > 0.5f) neighbors++;
            }
            if (neighbors > 4) glowIntensity[i] = planktonDensity;  // Quorum reached
        }
        
        // Update plankton movement
        planktonState[i] += totalWave * speed * 0.1f;
        planktonState[i] = constrain(planktonState[i], 0, 1);
    }
    
    // Visualize bioluminescence
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float glow = glowIntensity[i] * planktonDensity;
        
        uint8_t brightness = constrain(glow * 255 * intensity, 0, 255);
        
        // Bioluminescent colors by species
        uint8_t hue1, hue2;
        if (variation < 0.33f) {
            // Dinoflagellates - blue
            hue1 = 160 + (glow * 20);  // Blue to cyan
            hue2 = 170 + (glow * 20);
        } else if (variation < 0.66f) {
            // Jellyfish - blue-green
            hue1 = 140 + (glow * 40);  // Cyan to green
            hue2 = 150 + (glow * 40);
        } else {
            // Marine bacteria - green
            hue1 = 96 + (glow * 30);   // Green
            hue2 = 96 + (glow * 30);
        }
        
        strip1[i] = CHSV(hue1, saturation * 255, brightness);
        strip2[i] = CHSV(hue2, saturation * 255, brightness);
    }
}

// ============== LGP BACTERIAL COLONY GROWTH ==============
// Quorum sensing and biofilm formation patterns
void lgpBacterialColonyGrowth() {
    // ENCODER MAPPING:
    // Speed (3): Growth rate/generation time
    // Intensity (4): Nutrient concentration/growth vigor
    // Saturation (5): Color saturation
    // Complexity (6): Colony branching complexity
    // Variation (7): Growth pattern (DLA/Eden/biofilm)
    
    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float saturation = visualParams.getSaturationNorm();
    float complexity = visualParams.getComplexityNorm();
    float variation = visualParams.getVariationNorm();
    
    static float bacteriaDensity[320];
    static float nutrientLevel[320];
    static float quorumSignal[320];
    static float biofilmMatrix[320];
    static bool initialized = false;
    
    if (!initialized) {
        for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            bacteriaDensity[i] = 0;
            nutrientLevel[i] = 1.0f;
            quorumSignal[i] = 0;
            biofilmMatrix[i] = 0;
        }
        // Seed colony at center
        bacteriaDensity[HardwareConfig::STRIP_CENTER_POINT] = 1.0f;
        initialized = true;
    }
    
    float growthRate = speed * 0.1f * intensity;
    float branchingProbability = 0.1f + (complexity * 0.4f);  // 0.1-0.5
    
    // Bacterial growth dynamics
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        if (bacteriaDensity[i] > 0.01f) {
            // Consume nutrients
            float consumption = bacteriaDensity[i] * growthRate * 0.1f;
            nutrientLevel[i] -= consumption;
            nutrientLevel[i] = max(0.0f, nutrientLevel[i]);
            
            // Quorum sensing - release signaling molecules
            quorumSignal[i] = bacteriaDensity[i] * bacteriaDensity[i];  // Quadratic with density
            
            // Growth based on nutrients
            if (nutrientLevel[i] > 0.1f) {
                bacteriaDensity[i] += growthRate * nutrientLevel[i];
                
                // Colony expansion
                if (random(1000) < branchingProbability * 1000) {
                    // Grow into neighboring sites
                    if (i > 0 && bacteriaDensity[i-1] < 0.5f) {
                        bacteriaDensity[i-1] += bacteriaDensity[i] * 0.3f;
                    }
                    if (i < HardwareConfig::STRIP_LENGTH-1 && bacteriaDensity[i+1] < 0.5f) {
                        bacteriaDensity[i+1] += bacteriaDensity[i] * 0.3f;
                    }
                }
            }
        }
        
        // Diffusion of quorum signals
        float signalDiffusion = 0;
        if (i > 0) signalDiffusion += quorumSignal[i-1] * 0.3f;
        if (i < HardwareConfig::STRIP_LENGTH-1) signalDiffusion += quorumSignal[i+1] * 0.3f;
        quorumSignal[i] = quorumSignal[i] * 0.7f + signalDiffusion;
        
        // Biofilm formation when quorum is reached
        if (quorumSignal[i] > 0.5f) {
            biofilmMatrix[i] += 0.05f * bacteriaDensity[i];
        }
        
        // Growth patterns based on variation
        if (variation < 0.33f) {
            // Diffusion-limited aggregation (DLA)
            // More branching at edges
            float edgeFactor = 1.0f - abs(bacteriaDensity[i] - 0.5f) * 2;
            bacteriaDensity[i] *= (1 + edgeFactor * 0.1f);
        } else if (variation < 0.66f) {
            // Eden growth model
            // Uniform growth at colony edge
            if (bacteriaDensity[i] > 0.1f && bacteriaDensity[i] < 0.9f) {
                bacteriaDensity[i] += growthRate * 0.5f;
            }
        } else {
            // Biofilm mode
            // Growth influenced by matrix
            bacteriaDensity[i] += biofilmMatrix[i] * growthRate * 0.2f;
        }
        
        // Clamp values
        bacteriaDensity[i] = constrain(bacteriaDensity[i], 0, 1);
        biofilmMatrix[i] = constrain(biofilmMatrix[i], 0, 1);
    }
    
    // Visualize colony
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float density = bacteriaDensity[i];
        float biofilm = biofilmMatrix[i];
        float signal = quorumSignal[i];
        
        // Brightness based on bacterial density
        uint8_t brightness = constrain((density * 0.7f + biofilm * 0.3f) * 255 * intensity, 0, 255);

        // Use palette with smaller gradients (reduced from 60+40 and 80+40)
        uint8_t paletteIndex1 = (density * 15) + (signal * 10);  // Max 25
        uint8_t paletteIndex2 = (biofilm * 20) + (nutrientLevel[i] * 10);  // Max 30

        // Special states add smaller offsets
        if (signal > 0.7f) {
            // Quorum sensing active
            paletteIndex1 += 30;
        }
        if (biofilm > 0.5f) {
            // Biofilm matrix
            paletteIndex1 += 20;
        }

        strip1[i] = ColorFromPalette(currentPalette, gHue + paletteIndex1, brightness);
        strip2[i] = ColorFromPalette(currentPalette, gHue + paletteIndex2, brightness);
    }
}

// ============== LGP DNA REPLICATION FORK ==============
// Molecular machinery visualization with helicase and polymerase
void lgpDNAReplicationFork() {
    // ENCODER MAPPING:
    // Speed (3): Replication speed/helicase unwinding rate
    // Intensity (4): Replication fidelity/error rate
    // Saturation (5): Color saturation
    // Complexity (6): Number of Okazaki fragments
    // Variation (7): Replication mode (leading/lagging/both)
    
    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float saturation = visualParams.getSaturationNorm();
    float complexity = visualParams.getComplexityNorm();
    float variation = visualParams.getVariationNorm();
    
    static float replicationForkPos = 0;
    static float helicasePos = 0;
    static float leadingStrand[320];
    static float laggingStrand[320];
    static float primerPositions[32];
    static int primerCount = 0;
    static bool initialized = false;
    
    if (!initialized) {
        for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            leadingStrand[i] = 0;
            laggingStrand[i] = 0;
        }
        replicationForkPos = 0;  // Start at center, grows outward
        helicasePos = 0;
        initialized = true;
    }

    // CENTER ORIGIN: Helicase unwinding from center outward
    float unwindingRate = speed * 2.0f;
    helicasePos += unwindingRate;
    if (helicasePos > HardwareConfig::STRIP_HALF_LENGTH) helicasePos = 0;  // Reset when reaching edge

    // Replication fork follows helicase
    float forkLag = 10;  // Distance behind helicase
    replicationForkPos = helicasePos - forkLag;
    if (replicationForkPos < 0) replicationForkPos = 0;
    
    // CENTER ORIGIN: DNA polymerase activity - symmetric replication from center
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // Calculate distance from center
        float distFromCenter = abs((int)i - HardwareConfig::STRIP_CENTER_POINT);

        // Check if within replication zone (behind helicasePos)
        bool inReplicationZone = (distFromCenter < helicasePos) && (distFromCenter > replicationForkPos);

        if (variation < 0.33f || variation > 0.66f) {
            // Leading strand synthesis (continuous)
            if (inReplicationZone) {
                leadingStrand[i] += speed * 0.1f;

                // Replication errors
                if (random(1000) < (1 - intensity) * 10) {
                    leadingStrand[i] *= 0.8f;  // Mismatch
                }
            }
        }

        if (variation > 0.33f) {
            // Lagging strand synthesis (discontinuous)
            if (inReplicationZone) {
                // Okazaki fragments
                int fragmentSize = 20 - (complexity * 10);  // 10-20 bases

                if ((int)distFromCenter % fragmentSize == 0) {
                    // New primer
                    if (primerCount < 32) {
                        primerPositions[primerCount++] = i;
                    }
                }

                // Synthesis from primers
                for(int p = 0; p < primerCount; p++) {
                    if (abs(i - primerPositions[p]) < fragmentSize) {
                        laggingStrand[i] += speed * 0.08f;
                    }
                }
            }
        }
        
        // DNA ligase - join fragments
        if (laggingStrand[i] > 0.8f && i > 0 && laggingStrand[i-1] > 0.8f) {
            laggingStrand[i] = 1.0f;  // Sealed
        }
        
        // Clamp values
        leadingStrand[i] = constrain(leadingStrand[i], 0, 1);
        laggingStrand[i] = constrain(laggingStrand[i], 0, 1);
    }
    
    // Visualize replication - CENTER ORIGIN + use palette
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float leading = leadingStrand[i];
        float lagging = laggingStrand[i];
        float combined = max(leading, lagging);

        // Distance from center for symmetric color
        float distFromCenter = abs((int)i - HardwareConfig::STRIP_CENTER_POINT);

        uint8_t brightness = constrain(combined * 255 * intensity, 0, 255);

        // Use palette offsets instead of discrete hues
        uint8_t paletteOffset1 = 0;
        uint8_t paletteOffset2 = 0;

        // Color based on strand and position
        if (abs(distFromCenter - helicasePos) < 3) {
            // Helicase - first palette color
            paletteOffset1 = 0;
            brightness = 255;
        } else if (abs(distFromCenter - replicationForkPos) < 2) {
            // Replication fork
            paletteOffset1 = 10;
            brightness = 255;
        } else if (leading > 0.1f) {
            // Leading strand (continuous) - palette offset 20-25
            paletteOffset1 = 20 + (leading * 5);
            paletteOffset2 = 20 + (leading * 5);
        } else if (lagging > 0.1f) {
            // Lagging strand (fragments) - palette offset 30-35
            paletteOffset1 = 30 + (lagging * 5);
            paletteOffset2 = 30 + (lagging * 5);
        } else if (distFromCenter > helicasePos) {
            // Unwound DNA - dim
            paletteOffset1 = 0;
            brightness = 30;
        }

        // Show primers
        for(int p = 0; p < primerCount; p++) {
            if (abs(i - primerPositions[p]) < 1) {
                paletteOffset1 = 15;
                brightness = 200;
            }
        }

        strip1[i] = ColorFromPalette(currentPalette, gHue + paletteOffset1, brightness);
        strip2[i] = ColorFromPalette(currentPalette, gHue + paletteOffset2, brightness);
    }
}

// ============== LGP PROTEIN FOLDING DYNAMICS ==============
// Molecular chaos as proteins find their native state
void lgpProteinFoldingDynamics() {
    // ENCODER MAPPING:
    // Speed (3): Folding rate/molecular dynamics speed
    // Intensity (4): Temperature/kinetic energy
    // Saturation (5): Color saturation
    // Complexity (6): Protein length/complexity
    // Variation (7): Folding pathway (direct/molten globule/misfolded)
    
    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float saturation = visualParams.getSaturationNorm();
    float complexity = visualParams.getComplexityNorm();
    float variation = visualParams.getVariationNorm();
    
    static float aminoAcidChain[320];
    static float secondaryStructure[320];
    static float tertiaryContacts[320];
    static float hydrophobicity[320];
    static float foldingProgress = 0;
    static bool initialized = false;
    
    if (!initialized) {
        for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            // Random amino acid sequence
            aminoAcidChain[i] = random(20) / 20.0f;  // 20 amino acids
            hydrophobicity[i] = (aminoAcidChain[i] > 0.5f) ? 1.0f : 0.0f;
            secondaryStructure[i] = 0;
            tertiaryContacts[i] = 0;
        }
        initialized = true;
    }
    
    foldingProgress += speed * 0.02f;
    float temperature = 0.5f + (intensity * 0.5f);  // 0.5-1.0
    float proteinComplexity = complexity;  // 0-1 determines folding radius

    // CENTER ORIGIN: Protein folding simulation - folding from/to center
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // Calculate distance from center - protein folds from center outward
        float distFromCenter = abs((int)i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;

        // Only process residues within folding radius
        if (normalizedDist > proteinComplexity) continue;
        // Secondary structure formation (alpha helices, beta sheets)
        if (foldingProgress > 0.2f) {
            // Check for helix-forming regions
            if (i >= 3 && i < HardwareConfig::STRIP_LENGTH - 3) {
                float helixPropensity = 0;
                for(int j = -3; j <= 3; j++) {
                    if (i+j >= 0 && i+j < HardwareConfig::STRIP_LENGTH) {
                        helixPropensity += aminoAcidChain[i+j] * 0.14f;
                    }
                }
                secondaryStructure[i] += helixPropensity * speed * 0.1f;
            }
        }

        // Tertiary structure - hydrophobic collapse toward center
        if (foldingProgress > 0.5f) {
            if (hydrophobicity[i] > 0.5f) {
                // Hydrophobic residues seek interior (center)
                float buryScore = 0;
                for(int j = 0; j < HardwareConfig::STRIP_LENGTH; j++) {
                    float jDistFromCenter = abs((int)j - HardwareConfig::STRIP_CENTER_POINT);
                    float jNormalizedDist = jDistFromCenter / HardwareConfig::STRIP_HALF_LENGTH;

                    // Skip if outside folding radius
                    if (jNormalizedDist > proteinComplexity) continue;

                    if (abs(i - j) > 10) {  // Non-local contacts
                        // Favor contacts toward center
                        float centerWeight = 1.0f - (distFromCenter / HardwareConfig::STRIP_HALF_LENGTH);
                        if (hydrophobicity[j] > 0.5f) {
                            buryScore += exp(-abs(i - j) * 0.05f) * temperature * centerWeight;
                        }
                    }
                }
                tertiaryContacts[i] = buryScore;
            }
        }
        
        // Folding pathways based on variation
        if (variation < 0.33f) {
            // Direct folding - smooth pathway
            secondaryStructure[i] = min(secondaryStructure[i], foldingProgress);
            tertiaryContacts[i] = min(tertiaryContacts[i], foldingProgress - 0.5f);
        } else if (variation < 0.66f) {
            // Molten globule intermediate
            if (foldingProgress > 0.3f && foldingProgress < 0.7f) {
                // Compact but disordered
                tertiaryContacts[i] += random(100) / 1000.0f * temperature;
                secondaryStructure[i] *= 0.98f;  // Partial unfolding
            }
        } else {
            // Misfolding pathway
            if (random(1000) < 5) {
                // Aggregation seeds
                tertiaryContacts[i] = 1.0f;
                if (i > 0) tertiaryContacts[i-1] = 0.8f;
                if (i < HardwareConfig::STRIP_LENGTH-1) tertiaryContacts[i+1] = 0.8f;
            }
        }
        
        // Thermal fluctuations
        secondaryStructure[i] += (random(100) - 50) / 1000.0f * temperature;
        tertiaryContacts[i] += (random(100) - 50) / 1000.0f * temperature;
        
        // Clamp values
        secondaryStructure[i] = constrain(secondaryStructure[i], 0, 1);
        tertiaryContacts[i] = constrain(tertiaryContacts[i], 0, 1);
    }
    
    // Visualize folding - use palette instead of discrete hues
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float secondary = secondaryStructure[i];
        float tertiary = tertiaryContacts[i];
        float folded = (secondary + tertiary) / 2;

        uint8_t brightness = constrain((0.3f + folded * 0.7f) * 255 * intensity, 0, 255);

        // Color based on structure type - use palette offsets
        uint8_t paletteOffset1 = 0;
        uint8_t paletteOffset2 = 0;

        if (secondary > 0.7f && tertiary < 0.3f) {
            // Alpha helix - palette offset 0-5
            paletteOffset1 = 0 + (secondary * 5);
            paletteOffset2 = 0 + (secondary * 5);
        } else if (secondary > 0.5f && secondary < 0.7f) {
            // Beta sheet - palette offset 10-15
            paletteOffset1 = 10 + (secondary * 5);
            paletteOffset2 = 10 + (secondary * 5);
        } else if (tertiary > 0.7f) {
            // Hydrophobic core - palette offset 20-25
            paletteOffset1 = 20 + (tertiary * 5);
            paletteOffset2 = 20 + (tertiary * 5);
        } else {
            // Random coil - palette offset 30-35
            paletteOffset1 = 30 + (folded * 5);
            paletteOffset2 = 30 + (folded * 5);
        }

        // Misfolded proteins - warning offset
        if (variation > 0.66f && tertiaryContacts[i] > 0.9f) {
            paletteOffset1 = 40;
            paletteOffset2 = 40;
            brightness = 255;
        }

        strip1[i] = ColorFromPalette(currentPalette, gHue + paletteOffset1, brightness);
        strip2[i] = ColorFromPalette(currentPalette, gHue + paletteOffset2, brightness);
    }
}

// ============== LGP MYCELIUM NETWORK GROWTH ==============
// Fungal communication waves through hyphal networks
void lgpMyceliumNetworkGrowth() {
    // ENCODER MAPPING:
    // Speed (3): Growth rate/hyphal extension speed
    // Intensity (4): Network density/branching frequency
    // Saturation (5): Color saturation
    // Complexity (6): Network connectivity/anastomosis
    // Variation (7): Growth strategy (explorative/exploitative/pulsed)
    
    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float saturation = visualParams.getSaturationNorm();
    float complexity = visualParams.getComplexityNorm();
    float variation = visualParams.getVariationNorm();
    
    static float hyphalDensity[320];
    static float nutrientFlow[320];
    static float communicationSignal[320];
    static float sporeFormation[320];
    static float networkAge[320];
    static bool initialized = false;
    
    if (!initialized) {
        for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            hyphalDensity[i] = 0;
            nutrientFlow[i] = 0;
            communicationSignal[i] = 0;
            sporeFormation[i] = 0;
            networkAge[i] = 0;
        }
        // Spore germination points
        hyphalDensity[HardwareConfig::STRIP_LENGTH/4] = 0.5f;
        hyphalDensity[HardwareConfig::STRIP_LENGTH*3/4] = 0.5f;
        initialized = true;
    }
    
    float growthRate = speed * intensity * 0.05f;
    float branchingRate = 0.05f + (complexity * 0.15f);  // 0.05-0.2
    
    // Mycelial growth dynamics
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        if (hyphalDensity[i] > 0.01f) {
            // Age the network
            networkAge[i] += speed * 0.01f;
            
            // Nutrient transport through hyphae
            float nutrientGradient = 0;
            if (i > 0) nutrientGradient += nutrientFlow[i-1] - nutrientFlow[i];
            if (i < HardwareConfig::STRIP_LENGTH-1) nutrientGradient += nutrientFlow[i+1] - nutrientFlow[i];
            nutrientFlow[i] += nutrientGradient * 0.1f;
            
            // Hyphal growth and branching
            if (nutrientFlow[i] > 0.1f || networkAge[i] < 0.5f) {
                // Extend hyphae
                if (i > 0 && hyphalDensity[i-1] < 1.0f) {
                    hyphalDensity[i-1] += growthRate * (1 - hyphalDensity[i-1]);
                }
                if (i < HardwareConfig::STRIP_LENGTH-1 && hyphalDensity[i+1] < 1.0f) {
                    hyphalDensity[i+1] += growthRate * (1 - hyphalDensity[i+1]);
                }
                
                // Branching
                if (random(1000) < branchingRate * 1000) {
                    int branchPos = i + random(10) - 5;
                    if (branchPos >= 0 && branchPos < HardwareConfig::STRIP_LENGTH) {
                        hyphalDensity[branchPos] += 0.3f;
                    }
                }
            }
            
            // Communication through network
            communicationSignal[i] = hyphalDensity[i] * sin(networkAge[i] * 10);
            
            // Anastomosis (hyphal fusion) for high connectivity
            if (complexity > 0.7f) {
                for(int j = max(0, i-10); j <= min(HardwareConfig::STRIP_LENGTH-1, i+10); j++) {
                    if (j != i && hyphalDensity[j] > 0.5f && hyphalDensity[i] > 0.5f) {
                        // Fusion creates stronger connection
                        float connection = 0.1f * exp(-abs(i-j) * 0.1f);
                        nutrientFlow[i] += connection;
                        nutrientFlow[j] += connection;
                    }
                }
            }
            
            // Spore formation in mature regions
            if (networkAge[i] > 1.0f && hyphalDensity[i] > 0.8f) {
                sporeFormation[i] += 0.01f;
            }
        }
        
        // Growth strategies based on variation
        if (variation < 0.33f) {
            // Explorative - fast, sparse growth
            growthRate *= 1.5f;
            hyphalDensity[i] *= 0.98f;  // Thinner hyphae
        } else if (variation < 0.66f) {
            // Exploitative - dense, slow growth
            growthRate *= 0.7f;
            if (hyphalDensity[i] > 0.3f) {
                hyphalDensity[i] += 0.01f;  // Thickening
            }
        } else {
            // Pulsed growth - oscillating
            float pulse = sin(networkAge[i] * 5);
            growthRate *= (1 + pulse * 0.5f);
        }
        
        // Clamp values
        hyphalDensity[i] = constrain(hyphalDensity[i], 0, 1);
        nutrientFlow[i] = constrain(nutrientFlow[i], 0, 1);
        sporeFormation[i] = constrain(sporeFormation[i], 0, 1);
    }
    
    // Propagate communication signals
    for(int i = 1; i < HardwareConfig::STRIP_LENGTH-1; i++) {
        float signal = communicationSignal[i];
        communicationSignal[i] = signal * 0.7f + 
                                (communicationSignal[i-1] + communicationSignal[i+1]) * 0.15f;
    }
    
    // Visualize mycelium network
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float density = hyphalDensity[i];
        float signal = abs(communicationSignal[i]);
        float spores = sporeFormation[i];
        
        // Base brightness from hyphal density
        uint8_t brightness = density * 200 * intensity;
        
        // Add communication pulses
        brightness += signal * 55;
        brightness = min(255, (int)brightness);
        
        // Color based on network state
        uint8_t hue1 = gHue + (networkAge[i] * 40);  // Age gradient
        uint8_t hue2 = gHue + (nutrientFlow[i] * 60);  // Nutrient flow
        
        // Special colors
        if (spores > 0.5f) {
            // Fruiting bodies - orange/red
            hue1 = 16 + (spores * 32);
            brightness = 255;
        } else if (signal > 0.5f) {
            // Active communication - blue pulse
            hue1 = 160 + (signal * 40);
        } else if (density > 0.8f && complexity > 0.7f) {
            // Dense anastomosed regions - purple
            hue1 = 192 + (density * 32);
        }
        
        strip1[i] = CHSV(hue1, saturation * 255, brightness);
        strip2[i] = CHSV(hue2, saturation * 255, brightness);
    }
}

// ============== LGP SLIME MOLD OPTIMIZATION ==============
// Physarum polycephalum solving optimization problems
void lgpSlimeMoldOptimization() {
    // ENCODER MAPPING:
    // Speed (3): Protoplasmic flow rate
    // Intensity (4): Chemotaxis strength/food attraction
    // Saturation (5): Color saturation
    // Complexity (6): Number of food sources
    // Variation (7): Behavior mode (exploration/exploitation/pulsation)
    
    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float saturation = visualParams.getSaturationNorm();
    float complexity = visualParams.getComplexityNorm();
    float variation = visualParams.getVariationNorm();
    
    static float slimeDensity[320];
    static float tubeThickness[320];
    static float chemoattractant[320];
    static float flowDirection[320];
    static float foodSources[8];
    static int numFoodSources = 0;
    static float oscillationPhase = 0;
    static bool initialized = false;
    
    if (!initialized) {
        for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            slimeDensity[i] = 0;
            tubeThickness[i] = 0;
            chemoattractant[i] = 0;
            flowDirection[i] = 0;
        }
        // Initial plasmodium
        slimeDensity[HardwareConfig::STRIP_CENTER_POINT] = 1.0f;
        
        // Place food sources
        numFoodSources = 2 + (complexity * 6);  // 2-8 sources
        for(int f = 0; f < numFoodSources; f++) {
            foodSources[f] = (f + 1) * HardwareConfig::STRIP_LENGTH / (numFoodSources + 1);
            chemoattractant[(int)foodSources[f]] = 1.0f;
        }
        initialized = true;
    }
    
    oscillationPhase += speed * 0.1f;
    float flowRate = speed * intensity * 0.1f;
    
    // Slime mold dynamics
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // Chemotaxis - move toward food
        float gradientLeft = (i > 0) ? chemoattractant[i-1] - chemoattractant[i] : 0;
        float gradientRight = (i < HardwareConfig::STRIP_LENGTH-1) ? chemoattractant[i+1] - chemoattractant[i] : 0;
        
        flowDirection[i] = gradientLeft + gradientRight;
        
        // Protoplasmic flow
        if (slimeDensity[i] > 0.01f) {
            float flow = flowDirection[i] * flowRate;
            
            // Oscillatory flow (characteristic of Physarum)
            flow *= (1 + 0.5f * sin(oscillationPhase + i * 0.1f));
            
            // Move slime
            if (flow > 0 && i < HardwareConfig::STRIP_LENGTH-1) {
                float transfer = min(slimeDensity[i] * 0.1f, abs(flow));
                slimeDensity[i+1] += transfer;
                slimeDensity[i] -= transfer;
            } else if (flow < 0 && i > 0) {
                float transfer = min(slimeDensity[i] * 0.1f, abs(flow));
                slimeDensity[i-1] += transfer;
                slimeDensity[i] -= transfer;
            }
            
            // Tube formation/thickening on used paths
            tubeThickness[i] += abs(flow) * 0.01f;
            tubeThickness[i] *= 0.99f;  // Slow decay
        }
        
        // Exploration vs exploitation
        if (variation < 0.33f) {
            // Exploration mode - random extensions
            if (slimeDensity[i] > 0.5f && random(1000) < 10) {
                int explorePos = i + random(20) - 10;
                if (explorePos >= 0 && explorePos < HardwareConfig::STRIP_LENGTH) {
                    slimeDensity[explorePos] += 0.2f;
                }
            }
        } else if (variation < 0.66f) {
            // Exploitation mode - strengthen existing paths
            if (tubeThickness[i] > 0.3f) {
                slimeDensity[i] += 0.05f;
            }
        } else {
            // Pulsation mode - rhythmic expansion/contraction
            float pulse = sin(oscillationPhase * 2);
            slimeDensity[i] *= (1 + pulse * 0.1f);
        }
        
        // Decay unused paths
        if (tubeThickness[i] < 0.1f) {
            slimeDensity[i] *= 0.95f;
        }
        
        // Clamp values
        slimeDensity[i] = constrain(slimeDensity[i], 0, 1);
        tubeThickness[i] = constrain(tubeThickness[i], 0, 1);
    }
    
    // Diffuse chemoattractant
    for(int i = 1; i < HardwareConfig::STRIP_LENGTH-1; i++) {
        chemoattractant[i] = chemoattractant[i] * 0.98f + 
                            (chemoattractant[i-1] + chemoattractant[i+1]) * 0.01f;
    }
    
    // Replenish food sources
    for(int f = 0; f < numFoodSources; f++) {
        chemoattractant[(int)foodSources[f]] = 1.0f;
    }
    
    // Visualize slime mold
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float density = slimeDensity[i];
        float tube = tubeThickness[i];
        float food = chemoattractant[i];
        
        // Brightness based on slime presence and tube thickness
        uint8_t brightness = constrain((density * 0.6f + tube * 0.4f) * 255 * intensity, 0, 255);
        
        // Color based on state
        uint8_t hue1 = gHue + 64;  // Base yellow for slime
        uint8_t hue2 = gHue + 64;
        
        // Food sources - bright green
        for(int f = 0; f < numFoodSources; f++) {
            if (abs(i - foodSources[f]) < 2) {
                hue1 = 96;
                brightness = 255;
            }
        }
        
        // Active flow regions - orange
        if (abs(flowDirection[i]) > 0.5f) {
            hue1 = 32 + (abs(flowDirection[i]) * 32);
        }
        
        // Thick transport tubes - darker yellow
        if (tube > 0.5f) {
            hue1 = 48 + (tube * 32);
            brightness = min(255, brightness + 50);
        }
        
        // Pulsation visualization
        if (variation > 0.66f) {
            brightness *= (1 + 0.2f * sin(oscillationPhase + i * 0.05f));
        }
        
        strip1[i] = CHSV(hue1, saturation * 255, brightness);
        strip2[i] = CHSV(hue2, saturation * 255, brightness);
    }
}