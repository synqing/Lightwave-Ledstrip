// LGP Organic Wave Patterns
// Living, breathing, biological chaos patterns that simulate life itself
// These effects transform the Light Guide Plate into a living organism

#include <FastLED.h>
#include "../../config/hardware_config.h"
#include "../../core/EffectTypes.h"

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
    
    // Ocean wave dynamics
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float position = (float)i / HardwareConfig::STRIP_LENGTH;
        
        // Multiple wave components for realistic ocean
        float wave1 = sin(position * 4 * PI + wavePhase) * 0.5f;
        float wave2 = sin(position * 7 * PI - wavePhase * 0.7f) * 0.3f;
        float wave3 = sin(position * 11 * PI + wavePhase * 1.3f) * 0.2f;
        
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
        
        uint8_t brightness = glow * 255 * intensity;
        
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
        uint8_t brightness = (density * 0.7f + biofilm * 0.3f) * 255 * intensity;
        
        // Color based on colony state
        uint8_t hue1 = gHue + (density * 60) + (signal * 40);
        uint8_t hue2 = gHue + (biofilm * 80) + (nutrientLevel[i] * 40);
        
        // Special colors for different states
        if (signal > 0.7f) {
            // Quorum sensing active - purple shift
            hue1 += 140;
        }
        if (biofilm > 0.5f) {
            // Biofilm matrix - green shift
            hue1 = 96 + (biofilm * 30);
        }
        
        strip1[i] = CHSV(hue1, saturation * 255, brightness);
        strip2[i] = CHSV(hue2, saturation * 255, brightness);
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
        replicationForkPos = HardwareConfig::STRIP_CENTER_POINT;
        helicasePos = replicationForkPos;
        initialized = true;
    }
    
    // Helicase unwinding
    float unwindingRate = speed * 2.0f;
    helicasePos += unwindingRate;
    if (helicasePos > HardwareConfig::STRIP_LENGTH) helicasePos = 0;
    
    // Replication fork follows helicase
    float forkLag = 10;  // Distance behind helicase
    replicationForkPos = helicasePos - forkLag;
    if (replicationForkPos < 0) replicationForkPos = 0;
    
    // DNA polymerase activity
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromFork = i - replicationForkPos;
        
        if (variation < 0.33f || variation > 0.66f) {
            // Leading strand synthesis (continuous)
            if (distFromFork < 0 && distFromFork > -50) {
                leadingStrand[i] += speed * 0.1f;
                
                // Replication errors
                if (random(1000) < (1 - intensity) * 10) {
                    leadingStrand[i] *= 0.8f;  // Mismatch
                }
            }
        }
        
        if (variation > 0.33f) {
            // Lagging strand synthesis (discontinuous)
            if (distFromFork < 0 && distFromFork > -50) {
                // Okazaki fragments
                int fragmentSize = 20 - (complexity * 10);  // 10-20 bases
                
                if ((int)abs(distFromFork) % fragmentSize == 0) {
                    // New primer
                    if (primerCount < 32) {
                        primerPositions[primerCount++] = i;
                    }
                }
                
                // Synthesis from primers
                for(int p = 0; p < primerCount; p++) {
                    if (i <= primerPositions[p] && i > primerPositions[p] - fragmentSize) {
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
    
    // Visualize replication
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float leading = leadingStrand[i];
        float lagging = laggingStrand[i];
        float combined = max(leading, lagging);
        
        uint8_t brightness = combined * 255 * intensity;
        
        // DNA base colors (A=red, T=yellow, G=green, C=blue)
        uint8_t hue1 = gHue;
        uint8_t hue2 = gHue;
        
        // Color based on strand and position
        if (abs(i - helicasePos) < 3) {
            // Helicase - bright white
            hue1 = 0;
            brightness = 255;
        } else if (abs(i - replicationForkPos) < 2) {
            // Replication fork - orange
            hue1 = 32;
            brightness = 255;
        } else if (leading > 0.1f) {
            // Leading strand - blue (continuous)
            hue1 = 160 + (leading * 40);
            hue2 = 160 + (leading * 40);
        } else if (lagging > 0.1f) {
            // Lagging strand - green (fragments)
            hue1 = 96 + (lagging * 40);
            hue2 = 96 + (lagging * 40);
        } else if (i > helicasePos) {
            // Unwound DNA - dim red
            hue1 = 0;
            brightness = 30;
        }
        
        // Show primers as yellow dots
        for(int p = 0; p < primerCount; p++) {
            if (abs(i - primerPositions[p]) < 1) {
                hue1 = 64;  // Yellow
                brightness = 200;
            }
        }
        
        strip1[i] = CHSV(hue1, saturation * 255, brightness);
        strip2[i] = CHSV(hue2, saturation * 255, brightness);
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
    int proteinLength = 50 + (complexity * 200);  // 50-250 residues
    
    // Protein folding simulation
    for(int i = 0; i < min(proteinLength, HardwareConfig::STRIP_LENGTH); i++) {
        // Secondary structure formation (alpha helices, beta sheets)
        if (foldingProgress > 0.2f) {
            // Check for helix-forming regions
            if (i >= 3 && i < proteinLength - 3) {
                float helixPropensity = 0;
                for(int j = -3; j <= 3; j++) {
                    helixPropensity += aminoAcidChain[i+j] * 0.14f;
                }
                secondaryStructure[i] += helixPropensity * speed * 0.1f;
            }
        }
        
        // Tertiary structure - hydrophobic collapse
        if (foldingProgress > 0.5f) {
            if (hydrophobicity[i] > 0.5f) {
                // Hydrophobic residues seek interior
                float buryScore = 0;
                for(int j = 0; j < proteinLength; j++) {
                    if (abs(i - j) > 10) {  // Non-local contacts
                        float distance = abs(i - j) / (float)proteinLength;
                        if (hydrophobicity[j] > 0.5f) {
                            buryScore += exp(-distance * 5) * temperature;
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
                if (i < proteinLength-1) tertiaryContacts[i+1] = 0.8f;
            }
        }
        
        // Thermal fluctuations
        secondaryStructure[i] += (random(100) - 50) / 1000.0f * temperature;
        tertiaryContacts[i] += (random(100) - 50) / 1000.0f * temperature;
        
        // Clamp values
        secondaryStructure[i] = constrain(secondaryStructure[i], 0, 1);
        tertiaryContacts[i] = constrain(tertiaryContacts[i], 0, 1);
    }
    
    // Visualize folding
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float secondary = secondaryStructure[i];
        float tertiary = tertiaryContacts[i];
        float folded = (secondary + tertiary) / 2;
        
        uint8_t brightness = (0.3f + folded * 0.7f) * 255 * intensity;
        
        // Color based on structure type
        uint8_t hue1 = gHue;
        uint8_t hue2 = gHue;
        
        if (secondary > 0.7f && tertiary < 0.3f) {
            // Alpha helix - magenta
            hue1 = 192 + (secondary * 32);
            hue2 = 192 + (secondary * 32);
        } else if (secondary > 0.5f && secondary < 0.7f) {
            // Beta sheet - yellow
            hue1 = 64 + (secondary * 32);
            hue2 = 64 + (secondary * 32);
        } else if (tertiary > 0.7f) {
            // Hydrophobic core - blue
            hue1 = 160 + (tertiary * 40);
            hue2 = 160 + (tertiary * 40);
        } else {
            // Random coil - green
            hue1 = 96 + (folded * 40);
            hue2 = 96 + (folded * 40);
        }
        
        // Misfolded proteins - red warning
        if (variation > 0.66f && tertiaryContacts[i] > 0.9f) {
            hue1 = 0;
            hue2 = 0;
            brightness = 255;
        }
        
        strip1[i] = CHSV(hue1, saturation * 255, brightness);
        strip2[i] = CHSV(hue2, saturation * 255, brightness);
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
        brightness = min(255, brightness);
        
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
        uint8_t brightness = (density * 0.6f + tube * 0.4f) * 255 * intensity;
        
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