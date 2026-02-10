/**
 * @file CoreEffects.cpp
 * @brief Core v2 effects implementation with CENTER PAIR compliance
 *
 * Ported from v1 with proper CENTER ORIGIN patterns.
 * All effects use RenderContext for Actor-based rendering.
 */

#include "CoreEffects.h"
#include "../config/limits.h"  // For limits::MAX_EFFECTS validation
#include "LGPInterferenceEffects.h"
#include "LGPGeometricEffects.h"
#include "LGPAdvancedEffects.h"
#include "LGPOrganicEffects.h"
#include "LGPQuantumEffects.h"
#include "LGPColorMixingEffects.h"
#include "LGPNovelPhysicsEffects.h"
#include "LGPChromaticEffects.h"
// Legacy effects disabled - see plugins/legacy/ when ready to integrate
// #include "../plugins/legacy/LegacyEffectRegistration.h"
#include "ieffect/ChevronWavesEffect.h"
#include "ieffect/ModalResonanceEffect.h"
#include "ieffect/ChromaticInterferenceEffect.h"
#include "ieffect/FireEffect.h"
#include "ieffect/OceanEffect.h"
#include "ieffect/PlasmaEffect.h"
#include "ieffect/ConfettiEffect.h"
#include "ieffect/SinelonEffect.h"
#include "ieffect/JuggleEffect.h"
#include "ieffect/BPMEffect.h"
#include "ieffect/WaveEffect.h"
#include "ieffect/WaveAmbientEffect.h"
#include "ieffect/WaveReactiveEffect.h"
#include "ieffect/RippleEffect.h"
#include "ieffect/HeartbeatEffect.h"
#include "ieffect/HeartbeatEsTunedEffect.h"
#include "ieffect/InterferenceEffect.h"
#include "ieffect/BreathingEffect.h"
#include "ieffect/PulseEffect.h"
#include "ieffect/LGPBoxWaveEffect.h"
#include "ieffect/LGPHolographicEffect.h"
#include "ieffect/LGPOpalFilmEffect.h"
#include "ieffect/LGPGratingScanEffect.h"
#include "ieffect/LGPGratingScanBreakupEffect.h"
#include "ieffect/LGPStressGlassEffect.h"
#include "ieffect/LGPStressGlassMeltEffect.h"
#include "ieffect/LGPMoireSilkEffect.h"
#include "ieffect/LGPCausticShardsEffect.h"
#include "ieffect/LGPParallaxDepthEffect.h"
#include "ieffect/LGPHolographicEsTunedEffect.h"
#include "ieffect/LGPWaterCausticsEffect.h"
#include "ieffect/LGPSchlierenFlowEffect.h"
#include "ieffect/LGPReactionDiffusionEffect.h"
#include "ieffect/LGPReactionDiffusionTriangleEffect.h"
#include "ieffect/LGPShapeBangersPack.h"
#include "ieffect/LGPInterferenceScannerEffect.h"
#include "ieffect/LGPWaveCollisionEffect.h"
#include "ieffect/LGPDiamondLatticeEffect.h"
#include "ieffect/LGPHexagonalGridEffect.h"
#include "ieffect/LGPSpiralVortexEffect.h"
#include "ieffect/LGPSierpinskiEffect.h"
#include "ieffect/LGPConcentricRingsEffect.h"
#include "ieffect/LGPStarBurstEffect.h"
#include "ieffect/LGPMeshNetworkEffect.h"
#include "ieffect/LGPMoireCurtainsEffect.h"
#include "ieffect/LGPRadialRippleEffect.h"
#include "ieffect/LGPHolographicVortexEffect.h"
#include "ieffect/LGPEvanescentDriftEffect.h"
#include "ieffect/LGPChromaticShearEffect.h"
#include "ieffect/LGPModalCavityEffect.h"
#include "ieffect/LGPFresnelZonesEffect.h"
#include "ieffect/LGPPhotonicCrystalEffect.h"
#include "ieffect/LGPAuroraBorealisEffect.h"
#include "ieffect/LGPBioluminescentWavesEffect.h"
#include "ieffect/LGPPlasmaMembraneEffect.h"
#include "ieffect/LGPNeuralNetworkEffect.h"
#include "ieffect/LGPCrystallineGrowthEffect.h"
#include "ieffect/LGPFluidDynamicsEffect.h"
#include "ieffect/LGPQuantumTunnelingEffect.h"
#include "ieffect/LGPGravitationalLensingEffect.h"
#include "ieffect/LGPTimeCrystalEffect.h"
#include "ieffect/LGPSolitonWavesEffect.h"
#include "ieffect/LGPMetamaterialCloakEffect.h"
#include "ieffect/LGPGrinCloakEffect.h"
#include "ieffect/LGPCausticFanEffect.h"
#include "ieffect/LGPBirefringentShearEffect.h"
#include "ieffect/LGPAnisotropicCloakEffect.h"
#include "ieffect/LGPEvanescentSkinEffect.h"
#include "ieffect/LGPColorTemperatureEffect.h"
#include "ieffect/LGPRGBPrismEffect.h"
#include "ieffect/LGPComplementaryMixingEffect.h"
#include "ieffect/LGPQuantumColorsEffect.h"
#include "ieffect/LGPDopplerShiftEffect.h"
#include "ieffect/LGPColorAcceleratorEffect.h"
#include "ieffect/LGPDNAHelixEffect.h"
#include "ieffect/LGPPhaseTransitionEffect.h"
#include "ieffect/LGPChromaticAberrationEffect.h"
#include "ieffect/LGPPerceptualBlendEffect.h"
#include "ieffect/LGPChladniHarmonicsEffect.h"
#include "ieffect/LGPGravitationalWaveChirpEffect.h"
#include "ieffect/LGPQuantumEntanglementEffect.h"
#include "ieffect/LGPMycelialNetworkEffect.h"
#include "ieffect/LGPRileyDissonanceEffect.h"
#include "ieffect/LGPChromaticLensEffect.h"
#include "ieffect/LGPChromaticPulseEffect.h"
#include "ieffect/LGPAudioTestEffect.h"
#include "ieffect/LGPBeatPulseEffect.h"
#include "ieffect/BeatPulseStackEffect.h"
#include "ieffect/BeatPulseShockwaveEffect.h"
#include "ieffect/BeatPulseVoidEffect.h"
#include "ieffect/BeatPulseResonantEffect.h"
#include "ieffect/BeatPulseRippleEffect.h"
#include "ieffect/BeatPulseShockwaveCascadeEffect.h"
#include "ieffect/BeatPulseSpectralEffect.h"
#include "ieffect/BeatPulseSpectralPulseEffect.h"
#include "ieffect/BeatPulseBreatheEffect.h"
#include "ieffect/BeatPulseLGPInterferenceEffect.h"
#include "ieffect/BeatPulseBloomEffect.h"
#include "ieffect/BloomParityEffect.h"
#include "ieffect/KuramotoTransportEffect.h"
#include "ieffect/WaveformParityEffect.h"
#include "ieffect/LGPSpectrumBarsEffect.h"
#include "ieffect/LGPBassBreathEffect.h"
#include "ieffect/AudioWaveformEffect.h"
#include "ieffect/AudioBloomEffect.h"
#include "ieffect/SnapwaveLinearEffect.h"
#include "ieffect/LGPStarBurstNarrativeEffect.h"
#include "ieffect/LGPChordGlowEffect.h"
#include "ieffect/LGPPerlinVeilEffect.h"
#include "ieffect/LGPPerlinShocklinesEffect.h"
#include "ieffect/LGPPerlinCausticsEffect.h"
#include "ieffect/LGPPerlinInterferenceWeaveEffect.h"
#include "ieffect/LGPPerlinBackendFastLEDEffect.h"
#include "ieffect/LGPPerlinBackendEmotiscopeFullEffect.h"
#include "ieffect/LGPPerlinBackendEmotiscopeQuarterEffect.h"
#include "ieffect/LGPPerlinVeilAmbientEffect.h"
#include "ieffect/LGPPerlinShocklinesAmbientEffect.h"
#include "ieffect/LGPPerlinCausticsAmbientEffect.h"
#include "ieffect/LGPPerlinInterferenceWeaveAmbientEffect.h"
#include "ieffect/BPMEnhancedEffect.h"
#include "ieffect/BreathingEnhancedEffect.h"
#include "ieffect/ChevronWavesEffectEnhanced.h"
#include "ieffect/LGPInterferenceScannerEffectEnhanced.h"
#include "ieffect/LGPPhotonicCrystalEffectEnhanced.h"
#include "ieffect/LGPSpectrumDetailEffect.h"
#include "ieffect/LGPSpectrumDetailEnhancedEffect.h"
#include "ieffect/LGPStarBurstEffectEnhanced.h"
#include "ieffect/LGPWaveCollisionEffectEnhanced.h"
#include "ieffect/RippleEnhancedEffect.h"
#include "ieffect/RippleEsTunedEffect.h"
#include "ieffect/TrinityTestEffect.h"
#include "ieffect/LGPHolographicAutoCycleEffect.h"
#include "ieffect/esv11_reference/EsAnalogRefEffect.h"
#include "ieffect/esv11_reference/EsSpectrumRefEffect.h"
#include "ieffect/esv11_reference/EsOctaveRefEffect.h"
#include "ieffect/esv11_reference/EsBloomRefEffect.h"
#include "ieffect/esv11_reference/EsWaveformRefEffect.h"
#include "ieffect/sensorybridge_reference/SbWaveform310RefEffect.h"
#include "utils/FastLEDOptim.h"
#include "../core/narrative/NarrativeEngine.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {

using namespace lightwaveos::actors;

// ==================== Static State for Stateful Effects ====================
// Note: In v2, effect state should eventually move to EffectContext
// For now, we use static variables as in v1
// Core effects migrated to IEffect classes - state moved to instance members

// ==================== FIRE ====================

// Fire effect migrated to FireEffect IEffect class
// Legacy function kept for reference but not registered
void effectFire(RenderContext& ctx) {
    // MIGRATED TO FireEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== OCEAN ====================

// Ocean effect migrated to OceanEffect IEffect class
// Legacy function kept for reference but not registered
void effectOcean(RenderContext& ctx) {
    // MIGRATED TO OceanEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== PLASMA ====================

// Plasma effect migrated to PlasmaEffect IEffect class
// Legacy function kept for reference but not registered
void effectPlasma(RenderContext& ctx) {
    // MIGRATED TO PlasmaEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== CONFETTI ====================

// Confetti effect migrated to ConfettiEffect IEffect class
// Legacy function kept for reference but not registered
void effectConfetti(RenderContext& ctx) {
    // MIGRATED TO ConfettiEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== SINELON ====================

// Sinelon effect migrated to SinelonEffect IEffect class
// Legacy function kept for reference but not registered
void effectSinelon(RenderContext& ctx) {
    // MIGRATED TO SinelonEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== JUGGLE ====================

// Juggle effect migrated to JuggleEffect IEffect class
// Legacy function kept for reference but not registered
void effectJuggle(RenderContext& ctx) {
    // MIGRATED TO JuggleEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== BPM ====================

// BPM effect migrated to BPMEffect IEffect class
// Legacy function kept for reference but not registered
void effectBPM(RenderContext& ctx) {
    // MIGRATED TO BPMEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== WAVE ====================

// Wave effect migrated to WaveEffect IEffect class
// Legacy function kept for reference but not registered
void effectWave(RenderContext& ctx) {
    // MIGRATED TO WaveEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== RIPPLE ====================

// Ripple effect migrated to RippleEffect IEffect class
// Legacy function kept for reference but not registered
void effectRipple(RenderContext& ctx) {
    // MIGRATED TO RippleEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== HEARTBEAT ====================

// Heartbeat effect migrated to HeartbeatEffect IEffect class
// Legacy function kept for reference but not registered
void effectHeartbeat(RenderContext& ctx) {
    // MIGRATED TO HeartbeatEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== INTERFERENCE ====================

void effectInterference(RenderContext& ctx) {
    // MIGRATED TO InterferenceEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== BREATHING ====================

void effectBreathing(RenderContext& ctx) {
    // MIGRATED TO BreathingEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== PULSE (from main.cpp) ====================

void effectPulse(RenderContext& ctx) {
    // MIGRATED TO PulseEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== EFFECT REGISTRATION ====================

uint8_t registerCoreEffects(RendererActor* renderer) {
    if (!renderer) return 0;

    uint8_t count = 0;

    // Register all core effects
    // Fire (ID 0) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // Ocean (ID 1) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // Plasma (ID 2) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // Confetti (ID 3) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // Sinelon (ID 4) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // Juggle (ID 5) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // BPM (ID 6) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // Wave (ID 7) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // Ripple (ID 8) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // Heartbeat (ID 9) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // Interference (ID 10) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // Breathing (ID 11) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // Pulse (ID 12) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence

    return count;
}

uint8_t registerAllEffects(RendererActor* renderer) {
    if (!renderer) return 0;

    uint8_t total = 0;

    // =================================================================
    // MEMORY NOTE: Static effect instances go into .bss (DRAM).
    // Keep member data tiny (<64 bytes). Large buffers MUST use PSRAM
    // allocation in init() -- see MEMORY_ALLOCATION.md section 3.5.
    // =================================================================

    // =============== REGISTER ALL NATIVE V2 EFFECTS ===============
    // 68 total effects organized by category
    // Legacy v1 effects are disabled until plugins/legacy is properly integrated

    // Core effects (13) - IDs 0-12
    uint8_t coreStart = total;
    total += registerCoreEffects(renderer);
    
    // Pilot: Fire (ID 0) - IEffect native
    static ieffect::FireEffect fireInstance;
    if (renderer->registerEffect(coreStart + 0, &fireInstance)) {
        // Effect already counted in registerCoreEffects (count++), just register it
    }
    
    // Pilot: Ocean (ID 1) - IEffect native
    static ieffect::OceanEffect oceanInstance;
    if (renderer->registerEffect(coreStart + 1, &oceanInstance)) {
        // Effect already counted in registerCoreEffects (count++), just register it
    }
    
    // Pilot: Plasma (ID 2) - IEffect native
    static ieffect::PlasmaEffect plasmaInstance;
    if (renderer->registerEffect(coreStart + 2, &plasmaInstance)) {
        // Effect already counted in registerCoreEffects (count++), just register it
    }
    
    // Pilot: Confetti (ID 3) - IEffect native
    static ieffect::ConfettiEffect confettiInstance;
    if (renderer->registerEffect(coreStart + 3, &confettiInstance)) {
        // Effect already counted in registerCoreEffects (count++), just register it
    }
    
    // Pilot: Sinelon (ID 4) - IEffect native
    static ieffect::SinelonEffect sinelonInstance;
    if (renderer->registerEffect(coreStart + 4, &sinelonInstance)) {
        // Effect already counted in registerCoreEffects (count++), just register it
    }
    
    // Pilot: Juggle (ID 5) - IEffect native
    static ieffect::JuggleEffect juggleInstance;
    if (renderer->registerEffect(coreStart + 5, &juggleInstance)) {
        // Effect already counted in registerCoreEffects (count++), just register it
    }
    
    // Pilot: BPM (ID 6) - IEffect native
    static ieffect::BPMEffect bpmInstance;
    if (renderer->registerEffect(coreStart + 6, &bpmInstance)) {
        // Effect already counted in registerCoreEffects (count++), just register it
    }
    
    // Pilot: Wave Ambient (ID 7) - IEffect native (replaces WaveEffect)
    // Uses time-driven motion with audio amplitude modulation (AMBIENT pattern)
    static ieffect::WaveAmbientEffect waveAmbientInstance;
    if (renderer->registerEffect(coreStart + 7, &waveAmbientInstance)) {
        // Effect already counted in registerCoreEffects (count++), just register it
    }
    
    // Pilot: Ripple (ID 8) - IEffect native
    static ieffect::RippleEffect rippleInstance;
    if (renderer->registerEffect(coreStart + 8, &rippleInstance)) {
        // Effect already counted in registerCoreEffects (count++), just register it
    }
    
    // Pilot: Heartbeat (ID 9) - IEffect native
    static ieffect::HeartbeatEffect heartbeatInstance;
    if (renderer->registerEffect(coreStart + 9, &heartbeatInstance)) {
        // Effect already counted in registerCoreEffects (count++), just register it
    }

    // Interference (ID 10) - IEffect native
    static ieffect::InterferenceEffect interferenceInstance;
    if (renderer->registerEffect(coreStart + 10, &interferenceInstance)) {
        // Effect already counted in registerCoreEffects (count++), just register it
    }

    // Breathing (ID 11) - IEffect native
    static ieffect::BreathingEffect breathingInstance;
    if (renderer->registerEffect(coreStart + 11, &breathingInstance)) {
        // Effect already counted in registerCoreEffects (count++), just register it
    }

    // Pulse (ID 12) - IEffect native
    static ieffect::PulseEffect pulseInstance;
    if (renderer->registerEffect(coreStart + 12, &pulseInstance)) {
        // Effect already counted in registerCoreEffects (count++), just register it
    }

    // LGP Interference effects (5) - IDs 13-17
    uint8_t interferenceStart = total;
    uint8_t interferenceCount = registerLGPInterferenceEffects(renderer, total);
    total += interferenceCount;
    
    // LGP Box Wave (ID 13) - IEffect native
    static ieffect::LGPBoxWaveEffect boxWaveInstance;
    if (renderer->registerEffect(interferenceStart + 0, &boxWaveInstance)) {
        // Effect already counted in registerLGPInterferenceEffects (count++), just register it
    }
    
    // LGP Holographic (ID 14) - IEffect native
    static ieffect::LGPHolographicEffect holographicInstance;
    if (renderer->registerEffect(interferenceStart + 1, &holographicInstance)) {
        // Effect already counted in registerLGPInterferenceEffects (count++), just register it
    }

    // Pilot: LGP Modal Resonance (ID 15) - IEffect native
    static ieffect::ModalResonanceEffect modalResonanceInstance;
    if (renderer->registerEffect(interferenceStart + 2, &modalResonanceInstance)) {
        // Effect already counted in registerLGPInterferenceEffects (count++), just register it
    }
    
    // LGP Interference Scanner (ID 16) - IEffect native
    static ieffect::LGPInterferenceScannerEffect interferenceScannerInstance;
    if (renderer->registerEffect(interferenceStart + 3, &interferenceScannerInstance)) {
        // Effect already counted in registerLGPInterferenceEffects (count++), just register it
    }
    
    // LGP Wave Collision (ID 17) - IEffect native
    static ieffect::LGPWaveCollisionEffect waveCollisionInstance;
    if (renderer->registerEffect(interferenceStart + 4, &waveCollisionInstance)) {
        // Effect already counted in registerLGPInterferenceEffects (count++), just register it
    }

    // LGP Geometric effects (8) - IDs 18-25
    uint8_t geometricStart = total;
    uint8_t geometricCount = registerLGPGeometricEffects(renderer, total);
    total += geometricCount;
    
    // LGP Diamond Lattice (ID 18) - IEffect native
    static ieffect::LGPDiamondLatticeEffect diamondLatticeInstance;
    if (renderer->registerEffect(geometricStart + 0, &diamondLatticeInstance)) {
        // Effect already counted in registerLGPGeometricEffects (count++), just register it
    }
    
    // LGP Hexagonal Grid (ID 19) - IEffect native
    static ieffect::LGPHexagonalGridEffect hexagonalGridInstance;
    if (renderer->registerEffect(geometricStart + 1, &hexagonalGridInstance)) {
        // Effect already counted in registerLGPGeometricEffects (count++), just register it
    }
    
    // LGP Spiral Vortex (ID 20) - IEffect native
    static ieffect::LGPSpiralVortexEffect spiralVortexInstance;
    if (renderer->registerEffect(geometricStart + 2, &spiralVortexInstance)) {
        // Effect already counted in registerLGPGeometricEffects (count++), just register it
    }
    
    // LGP Sierpinski (ID 21) - IEffect native
    static ieffect::LGPSierpinskiEffect sierpinskiInstance;
    if (renderer->registerEffect(geometricStart + 3, &sierpinskiInstance)) {
        // Effect already counted in registerLGPGeometricEffects (count++), just register it
    }
    
    // Pilot: LGP Chevron Waves (ID 22) - IEffect native
    static ieffect::ChevronWavesEffect chevronWavesInstance;
    if (renderer->registerEffect(geometricStart + 4, &chevronWavesInstance)) {
        // Effect already counted in registerLGPGeometricEffects (count++), just register it
    }
    
    // LGP Concentric Rings (ID 23) - IEffect native
    static ieffect::LGPConcentricRingsEffect concentricRingsInstance;
    if (renderer->registerEffect(geometricStart + 5, &concentricRingsInstance)) {
        // Effect already counted in registerLGPGeometricEffects (count++), just register it
    }
    
    // LGP Star Burst (ID 24) - IEffect native
    static ieffect::LGPStarBurstEffect starBurstInstance;
    if (renderer->registerEffect(geometricStart + 6, &starBurstInstance)) {
        // Effect already counted in registerLGPGeometricEffects (count++), just register it
    }
    
    // LGP Mesh Network (ID 25) - IEffect native
    static ieffect::LGPMeshNetworkEffect meshNetworkInstance;
    if (renderer->registerEffect(geometricStart + 7, &meshNetworkInstance)) {
        // Effect already counted in registerLGPGeometricEffects (count++), just register it
    }

    // LGP Advanced effects (8) - IDs 26-33
    uint8_t advancedStart = total;
    uint8_t advancedCount = registerLGPAdvancedEffects(renderer, total);
    total += advancedCount;
    
    // LGP Moire Curtains (ID 26) - IEffect native
    static ieffect::LGPMoireCurtainsEffect moireCurtainsInstance;
    if (renderer->registerEffect(advancedStart + 0, &moireCurtainsInstance)) {
        // Effect already counted in registerLGPAdvancedEffects (count++), just register it
    }
    
    // LGP Radial Ripple (ID 27) - IEffect native
    static ieffect::LGPRadialRippleEffect radialRippleInstance;
    if (renderer->registerEffect(advancedStart + 1, &radialRippleInstance)) {
        // Effect already counted in registerLGPAdvancedEffects (count++), just register it
    }
    
    // LGP Holographic Vortex (ID 28) - IEffect native
    static ieffect::LGPHolographicVortexEffect holographicVortexInstance;
    if (renderer->registerEffect(advancedStart + 2, &holographicVortexInstance)) {
        // Effect already counted in registerLGPAdvancedEffects (count++), just register it
    }
    
    // LGP Evanescent Drift (ID 29) - IEffect native
    static ieffect::LGPEvanescentDriftEffect evanescentDriftInstance;
    if (renderer->registerEffect(advancedStart + 3, &evanescentDriftInstance)) {
        // Effect already counted in registerLGPAdvancedEffects (count++), just register it
    }
    
    // LGP Chromatic Shear (ID 30) - IEffect native
    static ieffect::LGPChromaticShearEffect chromaticShearInstance;
    if (renderer->registerEffect(advancedStart + 4, &chromaticShearInstance)) {
        // Effect already counted in registerLGPAdvancedEffects (count++), just register it
    }
    
    // LGP Modal Cavity (ID 31) - IEffect native
    static ieffect::LGPModalCavityEffect modalCavityInstance;
    if (renderer->registerEffect(advancedStart + 5, &modalCavityInstance)) {
        // Effect already counted in registerLGPAdvancedEffects (count++), just register it
    }
    
    // LGP Fresnel Zones (ID 32) - IEffect native
    static ieffect::LGPFresnelZonesEffect fresnelZonesInstance;
    if (renderer->registerEffect(advancedStart + 6, &fresnelZonesInstance)) {
        // Effect already counted in registerLGPAdvancedEffects (count++), just register it
    }
    
    // LGP Photonic Crystal (ID 33) - IEffect native
    static ieffect::LGPPhotonicCrystalEffect photonicCrystalInstance;
    if (renderer->registerEffect(advancedStart + 7, &photonicCrystalInstance)) {
        // Effect already counted in registerLGPAdvancedEffects (count++), just register it
    }

    // LGP Organic effects (6) - IDs 34-39
    uint8_t organicStart = total;
    uint8_t organicCount = registerLGPOrganicEffects(renderer, total);
    total += organicCount;
    
    // LGP Aurora Borealis (ID 34) - IEffect native
    static ieffect::LGPAuroraBorealisEffect auroraBorealisInstance;
    if (renderer->registerEffect(organicStart + 0, &auroraBorealisInstance)) {
        // Effect already counted in registerLGPOrganicEffects (count++), just register it
    }
    
    // LGP Bioluminescent Waves (ID 35) - IEffect native
    static ieffect::LGPBioluminescentWavesEffect bioluminescentWavesInstance;
    if (renderer->registerEffect(organicStart + 1, &bioluminescentWavesInstance)) {
        // Effect already counted in registerLGPOrganicEffects (count++), just register it
    }
    
    // LGP Plasma Membrane (ID 36) - IEffect native
    static ieffect::LGPPlasmaMembraneEffect plasmaMembraneInstance;
    if (renderer->registerEffect(organicStart + 2, &plasmaMembraneInstance)) {
        // Effect already counted in registerLGPOrganicEffects (count++), just register it
    }
    
    // LGP Neural Network (ID 37) - IEffect native
    static ieffect::LGPNeuralNetworkEffect neuralNetworkInstance;
    if (renderer->registerEffect(organicStart + 3, &neuralNetworkInstance)) {
        // Effect already counted in registerLGPOrganicEffects (count++), just register it
    }
    
    // LGP Crystalline Growth (ID 38) - IEffect native
    static ieffect::LGPCrystallineGrowthEffect crystallineGrowthInstance;
    if (renderer->registerEffect(organicStart + 4, &crystallineGrowthInstance)) {
        // Effect already counted in registerLGPOrganicEffects (count++), just register it
    }
    
    // LGP Fluid Dynamics (ID 39) - IEffect native
    static ieffect::LGPFluidDynamicsEffect fluidDynamicsInstance;
    if (renderer->registerEffect(organicStart + 5, &fluidDynamicsInstance)) {
        // Effect already counted in registerLGPOrganicEffects (count++), just register it
    }

    // LGP Quantum effects (10) - IDs 40-49
    uint8_t quantumStart = total;
    uint8_t quantumCount = registerLGPQuantumEffects(renderer, total);
    total += quantumCount;

    // LGP Quantum Tunneling (ID 40) - IEffect native
    static ieffect::LGPQuantumTunnelingEffect quantumTunnelingInstance;
    if (renderer->registerEffect(quantumStart + 0, &quantumTunnelingInstance)) {
        // Effect already counted in registerLGPQuantumEffects (count++), just register it
    }

    // LGP Gravitational Lensing (ID 41) - IEffect native
    static ieffect::LGPGravitationalLensingEffect gravitationalLensingInstance;
    if (renderer->registerEffect(quantumStart + 1, &gravitationalLensingInstance)) {
        // Effect already counted in registerLGPQuantumEffects (count++), just register it
    }

    // LGP Time Crystal (ID 42) - IEffect native
    static ieffect::LGPTimeCrystalEffect timeCrystalInstance;
    if (renderer->registerEffect(quantumStart + 2, &timeCrystalInstance)) {
        // Effect already counted in registerLGPQuantumEffects (count++), just register it
    }

    // LGP Soliton Waves (ID 43) - IEffect native
    static ieffect::LGPSolitonWavesEffect solitonWavesInstance;
    if (renderer->registerEffect(quantumStart + 3, &solitonWavesInstance)) {
        // Effect already counted in registerLGPQuantumEffects (count++), just register it
    }

    // LGP Metamaterial Cloak (ID 44) - IEffect native
    static ieffect::LGPMetamaterialCloakEffect metamaterialCloakInstance;
    if (renderer->registerEffect(quantumStart + 4, &metamaterialCloakInstance)) {
        // Effect already counted in registerLGPQuantumEffects (count++), just register it
    }

    // LGP GRIN Cloak (ID 45) - IEffect native
    static ieffect::LGPGrinCloakEffect grinCloakInstance;
    if (renderer->registerEffect(quantumStart + 5, &grinCloakInstance)) {
        // Effect already counted in registerLGPQuantumEffects (count++), just register it
    }

    // LGP Caustic Fan (ID 46) - IEffect native
    static ieffect::LGPCausticFanEffect causticFanInstance;
    if (renderer->registerEffect(quantumStart + 6, &causticFanInstance)) {
        // Effect already counted in registerLGPQuantumEffects (count++), just register it
    }

    // LGP Birefringent Shear (ID 47) - IEffect native
    static ieffect::LGPBirefringentShearEffect birefringentShearInstance;
    if (renderer->registerEffect(quantumStart + 7, &birefringentShearInstance)) {
        // Effect already counted in registerLGPQuantumEffects (count++), just register it
    }

    // LGP Anisotropic Cloak (ID 48) - IEffect native
    static ieffect::LGPAnisotropicCloakEffect anisotropicCloakInstance;
    if (renderer->registerEffect(quantumStart + 8, &anisotropicCloakInstance)) {
        // Effect already counted in registerLGPQuantumEffects (count++), just register it
    }

    // LGP Evanescent Skin (ID 49) - IEffect native
    static ieffect::LGPEvanescentSkinEffect evanescentSkinInstance;
    if (renderer->registerEffect(quantumStart + 9, &evanescentSkinInstance)) {
        // Effect already counted in registerLGPQuantumEffects (count++), just register it
    }

    // LGP Color Mixing effects (10) - IDs 50-59
    uint8_t colorMixStart = total;
    uint8_t colorMixCount = registerLGPColorMixingEffects(renderer, total);
    total += colorMixCount;

    // LGP Color Temperature (ID 50) - IEffect native
    static ieffect::LGPColorTemperatureEffect colorTemperatureInstance;
    if (renderer->registerEffect(colorMixStart + 0, &colorTemperatureInstance)) {
        // Effect already counted in registerLGPColorMixingEffects (count++), just register it
    }

    // LGP RGB Prism (ID 51) - IEffect native
    static ieffect::LGPRGBPrismEffect rgbPrismInstance;
    if (renderer->registerEffect(colorMixStart + 1, &rgbPrismInstance)) {
        // Effect already counted in registerLGPColorMixingEffects (count++), just register it
    }

    // LGP Complementary Mixing (ID 52) - IEffect native
    static ieffect::LGPComplementaryMixingEffect complementaryMixingInstance;
    if (renderer->registerEffect(colorMixStart + 2, &complementaryMixingInstance)) {
        // Effect already counted in registerLGPColorMixingEffects (count++), just register it
    }

    // LGP Quantum Colors (ID 53) - IEffect native
    static ieffect::LGPQuantumColorsEffect quantumColorsInstance;
    if (renderer->registerEffect(colorMixStart + 3, &quantumColorsInstance)) {
        // Effect already counted in registerLGPColorMixingEffects (count++), just register it
    }

    // LGP Doppler Shift (ID 54) - IEffect native
    static ieffect::LGPDopplerShiftEffect dopplerShiftInstance;
    if (renderer->registerEffect(colorMixStart + 4, &dopplerShiftInstance)) {
        // Effect already counted in registerLGPColorMixingEffects (count++), just register it
    }

    // LGP Color Accelerator (ID 55) - IEffect native
    static ieffect::LGPColorAcceleratorEffect colorAcceleratorInstance;
    if (renderer->registerEffect(colorMixStart + 5, &colorAcceleratorInstance)) {
        // Effect already counted in registerLGPColorMixingEffects (count++), just register it
    }

    // LGP DNA Helix (ID 56) - IEffect native
    static ieffect::LGPDNAHelixEffect dnaHelixInstance;
    if (renderer->registerEffect(colorMixStart + 6, &dnaHelixInstance)) {
        // Effect already counted in registerLGPColorMixingEffects (count++), just register it
    }

    // LGP Phase Transition (ID 57) - IEffect native
    static ieffect::LGPPhaseTransitionEffect phaseTransitionInstance;
    if (renderer->registerEffect(colorMixStart + 7, &phaseTransitionInstance)) {
        // Effect already counted in registerLGPColorMixingEffects (count++), just register it
    }

    // LGP Chromatic Aberration (ID 58) - IEffect native
    static ieffect::LGPChromaticAberrationEffect chromaticAberrationInstance;
    if (renderer->registerEffect(colorMixStart + 8, &chromaticAberrationInstance)) {
        // Effect already counted in registerLGPColorMixingEffects (count++), just register it
    }

    // LGP Perceptual Blend (ID 59) - IEffect native
    static ieffect::LGPPerceptualBlendEffect perceptualBlendInstance;
    if (renderer->registerEffect(colorMixStart + 9, &perceptualBlendInstance)) {
        // Effect already counted in registerLGPColorMixingEffects (count++), just register it
    }

    // LGP Novel Physics effects (5) - IDs 60-64
    uint8_t novelStart = total;
    uint8_t novelCount = registerLGPNovelPhysicsEffects(renderer, total);
    total += novelCount;

    // LGP Chladni Harmonics (ID 60) - IEffect native
    static ieffect::LGPChladniHarmonicsEffect chladniHarmonicsInstance;
    if (renderer->registerEffect(novelStart + 0, &chladniHarmonicsInstance)) {
        // Effect already counted in registerLGPNovelPhysicsEffects (count++), just register it
    }

    // LGP Gravitational Wave Chirp (ID 61) - IEffect native
    static ieffect::LGPGravitationalWaveChirpEffect gravitationalWaveChirpInstance;
    if (renderer->registerEffect(novelStart + 1, &gravitationalWaveChirpInstance)) {
        // Effect already counted in registerLGPNovelPhysicsEffects (count++), just register it
    }

    // LGP Quantum Entanglement (ID 62) - IEffect native
    static ieffect::LGPQuantumEntanglementEffect quantumEntanglementInstance;
    if (renderer->registerEffect(novelStart + 2, &quantumEntanglementInstance)) {
        // Effect already counted in registerLGPNovelPhysicsEffects (count++), just register it
    }

    // LGP Mycelial Network (ID 63) - IEffect native
    static ieffect::LGPMycelialNetworkEffect mycelialNetworkInstance;
    if (renderer->registerEffect(novelStart + 3, &mycelialNetworkInstance)) {
        // Effect already counted in registerLGPNovelPhysicsEffects (count++), just register it
    }

    // LGP Riley Dissonance (ID 64) - IEffect native
    static ieffect::LGPRileyDissonanceEffect rileyDissonanceInstance;
    if (renderer->registerEffect(novelStart + 4, &rileyDissonanceInstance)) {
        // Effect already counted in registerLGPNovelPhysicsEffects (count++), just register it
    }

    // LGP Chromatic effects (3) - IDs 65-67 (physics-accurate Cauchy dispersion)
    uint8_t chromaticStart = total;
    uint8_t chromaticCount = registerLGPChromaticEffects(renderer, total);
    total += chromaticCount;

    // LGP Chromatic Lens (ID 65) - IEffect native
    static ieffect::LGPChromaticLensEffect chromaticLensInstance;
    if (renderer->registerEffect(chromaticStart + 0, &chromaticLensInstance)) {
        // Effect already counted in registerLGPChromaticEffects (count++), just register it
    }

    // LGP Chromatic Pulse (ID 66) - IEffect native
    static ieffect::LGPChromaticPulseEffect chromaticPulseInstance;
    if (renderer->registerEffect(chromaticStart + 1, &chromaticPulseInstance)) {
        // Effect already counted in registerLGPChromaticEffects (count++), just register it
    }

    // Pilot: LGP Chromatic Interference (ID 67) - IEffect native
    static ieffect::ChromaticInterferenceEffect chromaticInterferenceInstance;
    if (renderer->registerEffect(chromaticStart + 2, &chromaticInterferenceInstance)) {
        // Effect already counted in registerLGPChromaticEffects (count++), just register it
    }

    // Audio-Reactive effects (Phase 2) - ID 68+
    // LGP Audio Test (ID 68) - IEffect native, demonstrates audio pipeline
    static ieffect::LGPAudioTestEffect audioTestInstance;
    if (renderer->registerEffect(total, &audioTestInstance)) {
        total++;  // Count this effect
    }

    // LGP Beat Pulse (ID 69) - Beat-synchronized radial pulse
    static ieffect::LGPBeatPulseEffect beatPulseInstance;
    if (renderer->registerEffect(total, &beatPulseInstance)) {
        total++;
    }

    // LGP Spectrum Bars (ID 70) - 8-band spectrum analyzer
    static ieffect::LGPSpectrumBarsEffect spectrumBarsInstance;
    if (renderer->registerEffect(total, &spectrumBarsInstance)) {
        total++;
    }

    // LGP Bass Breath (ID 71) - Organic breathing driven by bass
    static ieffect::LGPBassBreathEffect bassBreathInstance;
    if (renderer->registerEffect(total, &bassBreathInstance)) {
        total++;
    }

    // Audio Waveform (ID 72) - True time-domain waveform visualization
    static ieffect::AudioWaveformEffect audioWaveformInstance;
    if (renderer->registerEffect(total, &audioWaveformInstance)) {
        total++;
    }

    // Audio Bloom (ID 73) - Centre bloom pulses triggered by audio transients
    static ieffect::AudioBloomEffect audioBloomInstance;
    if (renderer->registerEffect(total, &audioBloomInstance)) {
        total++;
    }

    // LGP Star Burst Narrative (ID 74) - Story conductor + chord-based color
    static ieffect::LGPStarBurstNarrativeEffect starBurstNarrativeInstance;
    if (renderer->registerEffect(total, &starBurstNarrativeInstance)) {
        total++;
    }

    // LGP Chord Glow (ID 75) - Full chord detection showcase effect
    static ieffect::LGPChordGlowEffect chordGlowInstance;
    if (renderer->registerEffect(total, &chordGlowInstance)) {
        total++;
    }

    // Wave Reactive (ID 76) - Energy-accumulating wave with audio-driven motion
    // Uses Kaleidoscope-style energy accumulation (REACTIVE pattern)
    static ieffect::WaveReactiveEffect waveReactiveInstance;
    if (renderer->registerEffect(total, &waveReactiveInstance)) {
        total++;
    }

    // Perlin-based LGP effects (IDs 77-80) - Audio-reactive noise field patterns
    // LGP Perlin Veil (ID 77) - Slow drifting curtains from centre, audio-driven advection
    static ieffect::LGPPerlinVeilEffect perlinVeilInstance;
    if (renderer->registerEffect(total, &perlinVeilInstance)) {
        total++;
    }

    // LGP Perlin Shocklines (ID 78) - Beat-driven travelling ridges
    static ieffect::LGPPerlinShocklinesEffect perlinShocklinesInstance;
    if (renderer->registerEffect(total, &perlinShocklinesInstance)) {
        total++;
    }

    // LGP Perlin Caustics (ID 79) - Sparkling caustic lobes, treble→sparkle, bass→scale
    static ieffect::LGPPerlinCausticsEffect perlinCausticsInstance;
    if (renderer->registerEffect(total, &perlinCausticsInstance)) {
        total++;
    }

    // LGP Perlin Interference Weave (ID 80) - Dual-strip moiré interference
    static ieffect::LGPPerlinInterferenceWeaveEffect perlinInterferenceWeaveInstance;
    if (renderer->registerEffect(total, &perlinInterferenceWeaveInstance)) {
        total++;
    }

    // Perlin-based LGP effects (IDs 81-84) - Ambient (time-driven) variants
    // LGP Perlin Veil Ambient (ID 81) - Time-driven drifting curtains
    static ieffect::LGPPerlinVeilAmbientEffect perlinVeilAmbientInstance;
    if (renderer->registerEffect(total, &perlinVeilAmbientInstance)) {
        total++;
    }

    // LGP Perlin Shocklines Ambient (ID 82) - Time-driven travelling ridges
    static ieffect::LGPPerlinShocklinesAmbientEffect perlinShocklinesAmbientInstance;
    if (renderer->registerEffect(total, &perlinShocklinesAmbientInstance)) {
        total++;
    }

    // LGP Perlin Caustics Ambient (ID 83) - Time-driven caustic lobes
    static ieffect::LGPPerlinCausticsAmbientEffect perlinCausticsAmbientInstance;
    if (renderer->registerEffect(total, &perlinCausticsAmbientInstance)) {
        total++;
    }

    // LGP Perlin Interference Weave Ambient (ID 84) - Time-driven moiré
    static ieffect::LGPPerlinInterferenceWeaveAmbientEffect perlinInterferenceWeaveAmbientInstance;
    if (renderer->registerEffect(total, &perlinInterferenceWeaveAmbientInstance)) {
        total++;
    }

    // Perlin Backend Test Effects (IDs 85-87) - A/B/C comparison harness
    // Perlin Backend Test A (ID 85) - FastLED inoise8 baseline
    static ieffect::LGPPerlinBackendFastLEDEffect perlinBackendFastLEDInstance;
    if (renderer->registerEffect(total, &perlinBackendFastLEDInstance)) {
        total++;
    }

    // Perlin Backend Test B (ID 86) - Emotiscope 2.0 Perlin full-res
    static ieffect::LGPPerlinBackendEmotiscopeFullEffect perlinBackendEmotiscopeFullInstance;
    if (renderer->registerEffect(total, &perlinBackendEmotiscopeFullInstance)) {
        total++;
    }

    // Perlin Backend Test C (ID 87) - Emotiscope 2.0 Perlin quarter-res + interpolation
    static ieffect::LGPPerlinBackendEmotiscopeQuarterEffect perlinBackendEmotiscopeQuarterInstance;
    if (renderer->registerEffect(total, &perlinBackendEmotiscopeQuarterInstance)) {
        total++;
    }

    // =============== ENHANCED AUDIO-REACTIVE EFFECTS (88-99) ===============

    // BPM Enhanced (ID 88) - Tempo-locked pulse rings
    static ieffect::BPMEnhancedEffect bpmEnhancedInstance;
    if (renderer->registerEffect(total, &bpmEnhancedInstance)) {
        total++;
    }

    // Breathing Enhanced (ID 89) - Style-adaptive breathing
    static ieffect::BreathingEnhancedEffect breathingEnhancedInstance;
    if (renderer->registerEffect(total, &breathingEnhancedInstance)) {
        total++;
    }

    // Chevron Waves Enhanced (ID 90) - Beat-synced chevron propagation
    static ieffect::ChevronWavesEnhancedEffect chevronWavesEnhancedInstance;
    if (renderer->registerEffect(total, &chevronWavesEnhancedInstance)) {
        total++;
    }

    // LGP Interference Scanner Enhanced (ID 91) - Audio-reactive scan speed
    static ieffect::LGPInterferenceScannerEnhancedEffect interferenceScannerEnhancedInstance;
    if (renderer->registerEffect(total, &interferenceScannerEnhancedInstance)) {
        total++;
    }

    // LGP Photonic Crystal Enhanced (ID 92) - Harmonic lattice modulation
    static ieffect::LGPPhotonicCrystalEnhancedEffect photonicCrystalEnhancedInstance;
    if (renderer->registerEffect(total, &photonicCrystalEnhancedInstance)) {
        total++;
    }

    // LGP Spectrum Detail (ID 93) - 64-bin FFT direct visualisation
    static ieffect::LGPSpectrumDetailEffect spectrumDetailInstance;
    if (renderer->registerEffect(total, &spectrumDetailInstance)) {
        total++;
    }

    // LGP Spectrum Detail Enhanced (ID 94) - Saliency-weighted spectrum
    static ieffect::LGPSpectrumDetailEnhancedEffect spectrumDetailEnhancedInstance;
    if (renderer->registerEffect(total, &spectrumDetailEnhancedInstance)) {
        total++;
    }

    // LGP Star Burst Enhanced (ID 95) - Beat-triggered bursts
    static ieffect::LGPStarBurstEnhancedEffect starBurstEnhancedInstance;
    if (renderer->registerEffect(total, &starBurstEnhancedInstance)) {
        total++;
    }

    // LGP Wave Collision Enhanced (ID 96) - Audio-driven wave interference
    static ieffect::LGPWaveCollisionEnhancedEffect waveCollisionEnhancedInstance;
    if (renderer->registerEffect(total, &waveCollisionEnhancedInstance)) {
        total++;
    }

    // Ripple Enhanced (ID 97) - Beat-synced ripple propagation
    static ieffect::RippleEnhancedEffect rippleEnhancedInstance;
    if (renderer->registerEffect(total, &rippleEnhancedInstance)) {
        total++;
    }

    // Snapwave Linear (ID 98) - v1 parity, LINEAR exempt from CENTER_ORIGIN
    static ieffect::SnapwaveLinearEffect snapwaveLinearInstance;
    if (renderer->registerEffect(total, &snapwaveLinearInstance)) {
        total++;
    }

    // =============== DIAGNOSTIC EFFECTS ===============

    // Trinity Test (ID 99) - Diagnostic effect for PRISM Trinity data flow verification
    static ieffect::TrinityTestEffect trinityTestInstance;
    if (renderer->registerEffect(total, &trinityTestInstance)) {
        total++;
    }

    // =============== PALETTE AUTO-CYCLE EFFECTS ===============

    // LGP Holographic Auto-Cycle (ID 100) - Holographic with internal palette cycling
    static ieffect::LGPHolographicAutoCycleEffect holographicAutoCycleInstance;
    if (renderer->registerEffect(total, &holographicAutoCycleInstance)) {
        total++;
    }

    // =============== ES v1.1 REFERENCE SHOWS ===============
    // These are ports of Emotiscope v1.1_320 light modes for parity comparisons.

    // ES Analog (Ref) (ID 101)
    static ieffect::esv11_reference::EsAnalogRefEffect esAnalogRefInstance;
    if (renderer->registerEffect(total, &esAnalogRefInstance)) {
        total++;
    }

    // ES Spectrum (Ref) (ID 102)
    static ieffect::esv11_reference::EsSpectrumRefEffect esSpectrumRefInstance;
    if (renderer->registerEffect(total, &esSpectrumRefInstance)) {
        total++;
    }

    // ES Octave (Ref) (ID 103)
    static ieffect::esv11_reference::EsOctaveRefEffect esOctaveRefInstance;
    if (renderer->registerEffect(total, &esOctaveRefInstance)) {
        total++;
    }

    // ES Bloom (Ref) (ID 104)
    static ieffect::esv11_reference::EsBloomRefEffect esBloomRefInstance;
    if (renderer->registerEffect(total, &esBloomRefInstance)) {
        total++;
    }

    // ES Waveform (Ref) (ID 105)
    static ieffect::esv11_reference::EsWaveformRefEffect esWaveformRefInstance;
    if (renderer->registerEffect(total, &esWaveformRefInstance)) {
        total++;
    }

    // Ripple (ES tuned) (ID 106)
    static ieffect::RippleEsTunedEffect rippleEsTunedInstance;
    if (renderer->registerEffect(total, &rippleEsTunedInstance)) {
        total++;
    }

    // Heartbeat (ES tuned) (ID 107)
    static ieffect::HeartbeatEsTunedEffect heartbeatEsTunedInstance;
    if (renderer->registerEffect(total, &heartbeatEsTunedInstance)) {
        total++;
    }

    // LGP Holographic (ES tuned) (ID 108)
    static ieffect::LGPHolographicEsTunedEffect holographicEsTunedInstance;
    if (renderer->registerEffect(total, &holographicEsTunedInstance)) {
        total++;
    }

    // =============== SENSORY BRIDGE REFERENCE SHOWS ===============
    // These are ports of Sensory Bridge 3.1.0 light show modes for parity comparisons.

    // SB Waveform (Ref) (ID 109)
    static ieffect::sensorybridge_reference::SbWaveform310RefEffect sbWaveform310RefInstance;
    if (renderer->registerEffect(total, &sbWaveform310RefInstance)) {
        total++;
    }

    // Beat Pulse (Stack) (ID 110) - UI preview parity (static gradient + white push)
    static ieffect::BeatPulseStackEffect beatPulseStackInstance;
    if (renderer->registerEffect(total, &beatPulseStackInstance)) {
        total++;
    }

    // Beat Pulse (Shockwave) (ID 111) - outward pressure wave with sharp front and exponential trail
    static ieffect::BeatPulseShockwaveEffect beatPulseShockwaveInstance(false);  // false = outward
    if (renderer->registerEffect(total, &beatPulseShockwaveInstance)) {
        total++;
    }

    // NOTE: ID 112 (Shockwave In) has been removed. Use Parity/Stack for inward effects.
    // The slot is left empty to preserve effect ID stability for existing presets.

    // =============== BEAT PULSE FAMILY (113-119) ===============

    // Beat Pulse (Void) (ID 113) - Parity ring in darkness, palette-coloured detonation against black
    static ieffect::BeatPulseVoidEffect beatPulseVoidInstance;
    if (renderer->registerEffect(total, &beatPulseVoidInstance)) {
        total++;
    }

    // Beat Pulse (Resonant) (ID 114) - Double ring contracting inward: attack snap + resonant body
    static ieffect::BeatPulseResonantEffect beatPulseResonantInstance;
    if (renderer->registerEffect(total, &beatPulseResonantInstance)) {
        total++;
    }

    // Beat Pulse (Ripple) (ID 115) - 3-slot ring buffer of cascading implosion rings
    static ieffect::BeatPulseRippleEffect beatPulseRippleInstance;
    if (renderer->registerEffect(total, &beatPulseRippleInstance)) {
        total++;
    }

    // Beat Pulse (Shockwave Cascade) (ID 116) - Outward expansion with trailing echo rings
    static ieffect::BeatPulseShockwaveCascadeEffect beatPulseShockwaveCascadeInstance;
    if (renderer->registerEffect(total, &beatPulseShockwaveCascadeInstance)) {
        total++;
    }

    // Beat Pulse (Spectral) (ID 117) - Three frequency-driven rings: bass outer, mid middle, treble centre
    static ieffect::BeatPulseSpectralEffect beatPulseSpectralInstance;
    if (renderer->registerEffect(total, &beatPulseSpectralInstance)) {
        total++;
    }

    // Beat Pulse (Spectral Pulse) (ID 118) - Stationary zones pulsing by frequency band
    static ieffect::BeatPulseSpectralPulseEffect beatPulseSpectralPulseInstance;
    if (renderer->registerEffect(total, &beatPulseSpectralPulseInstance)) {
        total++;
    }

    // Beat Pulse (Breathe) (ID 119) - Whole-strip amplitude pump with centre-weighted glow
    static ieffect::BeatPulseBreatheEffect beatPulseBreatheInstance;
    if (renderer->registerEffect(total, &beatPulseBreatheInstance)) {
        total++;
    }

    // Beat Pulse (LGP Interference) (ID 120) - Dual-strip phase control for optical interference
    static ieffect::BeatPulseLGPInterferenceEffect beatPulseLGPInterferenceInstance;
    if (renderer->registerEffect(total, &beatPulseLGPInterferenceInstance)) {
        total++;
    }

    // Beat Pulse (Bloom) (ID 121) - Sensory Bridge Bloom parity: subpixel advection transport
    // Transport is the brush. History is the canvas. Audio is just pigment injection.
    static ieffect::BloomParityEffect bloomParityInstance;
    if (renderer->registerEffect(total, &bloomParityInstance)) {
        total++;
    }

    // Kuramoto Transport (ID 122) - Invisible oscillators → event injection → transported light
    // Architecture: Kuramoto field generates phase dynamics, feature extractor finds events,
    // transport buffer advects visible "light substance". Audio steers only engine params.
    static ieffect::KuramotoTransportEffect kuramotoTransportInstance;
    if (renderer->registerEffect(total, &kuramotoTransportInstance)) {
        total++;
    }

    // SB Waveform Parity (ID 123) - Sensory Bridge 3.1.0 waveform mode
    // Intensity-only rendering + palette at output. dt-corrected. Dynamic normalisation.
    static ieffect::WaveformParityEffect waveformParityInstance;
    if (renderer->registerEffect(total, &waveformParityInstance)) {
        total++;
    }

    // =============== HOLOGRAPHIC VARIANTS PACK (124-129) ===============

    // LGP Opal Film (ID 124) - Thin-film iridescence bands
    static ieffect::LGPOpalFilmEffect opalFilmInstance;
    if (renderer->registerEffect(total, &opalFilmInstance)) {
        total++;
    }

    // LGP Grating Scan (ID 125) - Diffraction scan highlight
    static ieffect::LGPGratingScanEffect gratingScanInstance;
    if (renderer->registerEffect(total, &gratingScanInstance)) {
        total++;
    }

    // LGP Stress Glass (ID 126) - Photoelastic fringe field
    static ieffect::LGPStressGlassEffect stressGlassInstance;
    if (renderer->registerEffect(total, &stressGlassInstance)) {
        total++;
    }

    // LGP Moire Silk (ID 127) - Two-lattice beat pattern
    static ieffect::LGPMoireSilkEffect moireSilkInstance;
    if (renderer->registerEffect(total, &moireSilkInstance)) {
        total++;
    }

    // LGP Caustic Shards (ID 128) - Interference with prismatic glints
    static ieffect::LGPCausticShardsEffect causticShardsInstance;
    if (renderer->registerEffect(total, &causticShardsInstance)) {
        total++;
    }

    // LGP Parallax Depth (ID 129) - Two-layer refractive parallax
    static ieffect::LGPParallaxDepthEffect parallaxDepthInstance;
    if (renderer->registerEffect(total, &parallaxDepthInstance)) {
        total++;
    }

    // LGP Stress Glass (Melt) (ID 130) - Phase-locked wings
    static ieffect::LGPStressGlassMeltEffect stressGlassMeltInstance;
    if (renderer->registerEffect(total, &stressGlassMeltInstance)) {
        total++;
    }

    // LGP Grating Scan (Breakup) (ID 131) - Halo breakup
    static ieffect::LGPGratingScanBreakupEffect gratingScanBreakupInstance;
    if (renderer->registerEffect(total, &gratingScanBreakupInstance)) {
        total++;
    }

    // LGP Water Caustics (ID 132) - Ray-envelope caustic filaments
    static ieffect::LGPWaterCausticsEffect waterCausticsInstance;
    if (renderer->registerEffect(total, &waterCausticsInstance)) {
        total++;
    }

    // LGP Schlieren Flow (ID 133) - Knife-edge gradient flow
    static ieffect::LGPSchlierenFlowEffect schlierenFlowInstance;
    if (renderer->registerEffect(total, &schlierenFlowInstance)) {
        total++;
    }

    // LGP Reaction Diffusion (ID 134) - Gray-Scott slime
    static ieffect::LGPReactionDiffusionEffect reactionDiffusionInstance;
    if (renderer->registerEffect(total, &reactionDiffusionInstance)) {
        total++;
    }

    // LGP RD Triangle (ID 135) - Reaction-diffusion front wedge isolation
    static ieffect::LGPReactionDiffusionTriangleEffect reactionDiffusionTriangleInstance;
    if (renderer->registerEffect(total, &reactionDiffusionTriangleInstance)) {
        total++;
    }

    // --- Shape Bangers Pack (136-146) ---

    // LGP Talbot Carpet (ID 136) - Self-imaging lattice rug
    static ieffect::LGPTalbotCarpetEffect talbotCarpetInstance;
    if (renderer->registerEffect(total, &talbotCarpetInstance)) {
        total++;
    }

    // LGP Airy Comet (ID 137) - Self-accelerating comet with trailing lobes
    static ieffect::LGPAiryCometEffect airyCometInstance;
    if (renderer->registerEffect(total, &airyCometInstance)) {
        total++;
    }

    // LGP Moire Cathedral (ID 138) - Interference arches from close gratings
    static ieffect::LGPMoireCathedralEffect moireCathedralInstance;
    if (renderer->registerEffect(total, &moireCathedralInstance)) {
        total++;
    }

    // LGP Living Glyph (ID 139) - Superformula morphing supershapes
    static ieffect::LGPSuperformulaGlyphEffect superformulaGlyphInstance;
    if (renderer->registerEffect(total, &superformulaGlyphInstance)) {
        total++;
    }

    // LGP Spirograph Crown (ID 140) - Hypotrochoid gear-flower loops
    static ieffect::LGPSpirographCrownEffect spirographCrownInstance;
    if (renderer->registerEffect(total, &spirographCrownInstance)) {
        total++;
    }

    // LGP Rose Bloom (ID 141) - Rhodonea petal engine
    static ieffect::LGPRoseBloomEffect roseBloomInstance;
    if (renderer->registerEffect(total, &roseBloomInstance)) {
        total++;
    }

    // LGP Harmonograph Halo (ID 142) - Lissajous aura orbitals
    static ieffect::LGPHarmonographHaloEffect harmonographHaloInstance;
    if (renderer->registerEffect(total, &harmonographHaloInstance)) {
        total++;
    }

    // LGP Rule 30 Cathedral (ID 143) - Elementary CA textile
    static ieffect::LGPRule30CathedralEffect rule30CathedralInstance;
    if (renderer->registerEffect(total, &rule30CathedralInstance)) {
        total++;
    }

    // LGP Langton Highway (ID 144) - Emergent ant-to-highway projection
    static ieffect::LGPLangtonHighwayEffect langtonHighwayInstance;
    if (renderer->registerEffect(total, &langtonHighwayInstance)) {
        total++;
    }

    // LGP Cymatic Ladder (ID 145) - Standing-wave sculpture
    static ieffect::LGPCymaticLadderEffect cymaticLadderInstance;
    if (renderer->registerEffect(total, &cymaticLadderInstance)) {
        total++;
    }

    // LGP Mach Diamonds (ID 146) - Shock-diamond jewellery
    static ieffect::LGPMachDiamondsEffect machDiamondsInstance;
    if (renderer->registerEffect(total, &machDiamondsInstance)) {
        total++;
    }

    // =============== EFFECT COUNT PARITY VALIDATION ===============
    // Runtime validation: ensure registered count matches expected
    // Single source of truth: limits::MAX_EFFECTS from config/limits.h
    constexpr uint8_t EXPECTED_EFFECT_COUNT = limits::MAX_EFFECTS;
    if (total != EXPECTED_EFFECT_COUNT) {
        Serial.printf("[WARNING] Effect count mismatch: registered %d, expected %d\n", total, EXPECTED_EFFECT_COUNT);
        Serial.printf("[WARNING] This may indicate missing effect registrations or metadata drift\n");
    } else {
        Serial.printf("[OK] Effect count validated: %d effects registered (matches expected)\n", total);
    }

    return total;
}

} // namespace effects
} // namespace lightwaveos
