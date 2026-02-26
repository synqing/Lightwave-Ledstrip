/**
 * @file CoreEffects.cpp
 * @brief Core v2 effects implementation with CENTER PAIR compliance
 *
 * Ported from v1 with proper CENTER ORIGIN patterns.
 * All effects use RenderContext for Actor-based rendering.
 */

#include "CoreEffects.h"
#include "../config/limits.h"  // For limits::MAX_EFFECTS validation
#include "../config/effect_ids.h"
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
#include "ieffect/LGPHolyShitBangersPack.h"
#include "ieffect/LGPExperimentalAudioPack.h"
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
#include "ieffect/LGPTimeReversalMirrorEffect.h"
#include "ieffect/LGPKdVSolitonPairEffect.h"
#include "ieffect/LGPGoldCodeSpeckleEffect.h"
#include "ieffect/LGPQuasicrystalLatticeEffect.h"
#include "ieffect/LGPFresnelCausticSweepEffect.h"
#include "ieffect/LGPTimeReversalMirrorEffect_AR.h"
#include "ieffect/LGPTimeReversalMirrorEffect_Mod1.h"
#include "ieffect/LGPTimeReversalMirrorEffect_Mod2.h"
#include "ieffect/LGPTimeReversalMirrorEffect_Mod3.h"
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

uint16_t registerCoreEffects(RendererActor* renderer) {
    if (!renderer) return 0;

    uint16_t count = 0;

    // Register all core effects
    // Fire - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // Ocean - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // Plasma - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // Confetti - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // Sinelon - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // Juggle - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // BPM - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // Wave - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // Ripple - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // Heartbeat - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // Interference - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // Breathing - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // Pulse - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence

    return count;
}

uint16_t registerAllEffects(RendererActor* renderer) {
    if (!renderer) return 0;

    using namespace lightwaveos;

    uint16_t total = 0;

    // =================================================================
    // MEMORY NOTE: Static effect instances go into .bss (DRAM).
    // Keep member data tiny (<64 bytes). Large buffers MUST use PSRAM
    // allocation in init() -- see MEMORY_ALLOCATION.md section 3.5.
    // =================================================================

    // =============== REGISTER ALL NATIVE V2 EFFECTS ===============

    // --- Core / Classic ---

    static ieffect::FireEffect fireInstance;
    renderer->registerEffect(EID_FIRE, &fireInstance);
    total++;

    static ieffect::OceanEffect oceanInstance;
    renderer->registerEffect(EID_OCEAN, &oceanInstance);
    total++;

    static ieffect::PlasmaEffect plasmaInstance;
    renderer->registerEffect(EID_PLASMA, &plasmaInstance);
    total++;

    static ieffect::ConfettiEffect confettiInstance;
    renderer->registerEffect(EID_CONFETTI, &confettiInstance);
    total++;

    static ieffect::SinelonEffect sinelonInstance;
    renderer->registerEffect(EID_SINELON, &sinelonInstance);
    total++;

    static ieffect::JuggleEffect juggleInstance;
    renderer->registerEffect(EID_JUGGLE, &juggleInstance);
    total++;

    static ieffect::BPMEffect bpmInstance;
    renderer->registerEffect(EID_BPM, &bpmInstance);
    total++;

    // Wave Ambient - Uses time-driven motion with audio amplitude modulation (AMBIENT pattern)
    static ieffect::WaveAmbientEffect waveAmbientInstance;
    renderer->registerEffect(EID_WAVE_AMBIENT, &waveAmbientInstance);
    total++;

    static ieffect::RippleEffect rippleInstance;
    renderer->registerEffect(EID_RIPPLE, &rippleInstance);
    total++;

    static ieffect::HeartbeatEffect heartbeatInstance;
    renderer->registerEffect(EID_HEARTBEAT, &heartbeatInstance);
    total++;

    static ieffect::InterferenceEffect interferenceInstance;
    renderer->registerEffect(EID_INTERFERENCE, &interferenceInstance);
    total++;

    static ieffect::BreathingEffect breathingInstance;
    renderer->registerEffect(EID_BREATHING, &breathingInstance);
    total++;

    static ieffect::PulseEffect pulseInstance;
    renderer->registerEffect(EID_PULSE, &pulseInstance);
    total++;

    // --- LGP Interference ---

    static ieffect::LGPBoxWaveEffect boxWaveInstance;
    renderer->registerEffect(EID_LGP_BOX_WAVE, &boxWaveInstance);
    total++;

    static ieffect::LGPHolographicEffect holographicInstance;
    renderer->registerEffect(EID_LGP_HOLOGRAPHIC, &holographicInstance);
    total++;

    static ieffect::ModalResonanceEffect modalResonanceInstance;
    renderer->registerEffect(EID_MODAL_RESONANCE, &modalResonanceInstance);
    total++;

    static ieffect::LGPInterferenceScannerEffect interferenceScannerInstance;
    renderer->registerEffect(EID_LGP_INTERFERENCE_SCANNER, &interferenceScannerInstance);
    total++;

    static ieffect::LGPWaveCollisionEffect waveCollisionInstance;
    renderer->registerEffect(EID_LGP_WAVE_COLLISION, &waveCollisionInstance);
    total++;

    // --- LGP Geometric ---

    static ieffect::LGPDiamondLatticeEffect diamondLatticeInstance;
    renderer->registerEffect(EID_LGP_DIAMOND_LATTICE, &diamondLatticeInstance);
    total++;

    static ieffect::LGPHexagonalGridEffect hexagonalGridInstance;
    renderer->registerEffect(EID_LGP_HEXAGONAL_GRID, &hexagonalGridInstance);
    total++;

    static ieffect::LGPSpiralVortexEffect spiralVortexInstance;
    renderer->registerEffect(EID_LGP_SPIRAL_VORTEX, &spiralVortexInstance);
    total++;

    static ieffect::LGPSierpinskiEffect sierpinskiInstance;
    renderer->registerEffect(EID_LGP_SIERPINSKI, &sierpinskiInstance);
    total++;

    static ieffect::ChevronWavesEffect chevronWavesInstance;
    renderer->registerEffect(EID_CHEVRON_WAVES, &chevronWavesInstance);
    total++;

    static ieffect::LGPConcentricRingsEffect concentricRingsInstance;
    renderer->registerEffect(EID_LGP_CONCENTRIC_RINGS, &concentricRingsInstance);
    total++;

    static ieffect::LGPStarBurstEffect starBurstInstance;
    renderer->registerEffect(EID_LGP_STAR_BURST, &starBurstInstance);
    total++;

    static ieffect::LGPMeshNetworkEffect meshNetworkInstance;
    renderer->registerEffect(EID_LGP_MESH_NETWORK, &meshNetworkInstance);
    total++;

    // --- LGP Advanced Optical ---

    static ieffect::LGPMoireCurtainsEffect moireCurtainsInstance;
    renderer->registerEffect(EID_LGP_MOIRE_CURTAINS, &moireCurtainsInstance);
    total++;

    static ieffect::LGPRadialRippleEffect radialRippleInstance;
    renderer->registerEffect(EID_LGP_RADIAL_RIPPLE, &radialRippleInstance);
    total++;

    static ieffect::LGPHolographicVortexEffect holographicVortexInstance;
    renderer->registerEffect(EID_LGP_HOLOGRAPHIC_VORTEX, &holographicVortexInstance);
    total++;

    static ieffect::LGPEvanescentDriftEffect evanescentDriftInstance;
    renderer->registerEffect(EID_LGP_EVANESCENT_DRIFT, &evanescentDriftInstance);
    total++;

    static ieffect::LGPChromaticShearEffect chromaticShearInstance;
    renderer->registerEffect(EID_LGP_CHROMATIC_SHEAR, &chromaticShearInstance);
    total++;

    static ieffect::LGPModalCavityEffect modalCavityInstance;
    renderer->registerEffect(EID_LGP_MODAL_CAVITY, &modalCavityInstance);
    total++;

    static ieffect::LGPFresnelZonesEffect fresnelZonesInstance;
    renderer->registerEffect(EID_LGP_FRESNEL_ZONES, &fresnelZonesInstance);
    total++;

    static ieffect::LGPPhotonicCrystalEffect photonicCrystalInstance;
    renderer->registerEffect(EID_LGP_PHOTONIC_CRYSTAL, &photonicCrystalInstance);
    total++;

    // --- LGP Organic ---

    static ieffect::LGPAuroraBorealisEffect auroraBorealisInstance;
    renderer->registerEffect(EID_LGP_AURORA_BOREALIS, &auroraBorealisInstance);
    total++;

    static ieffect::LGPBioluminescentWavesEffect bioluminescentWavesInstance;
    renderer->registerEffect(EID_LGP_BIOLUMINESCENT_WAVES, &bioluminescentWavesInstance);
    total++;

    static ieffect::LGPPlasmaMembraneEffect plasmaMembraneInstance;
    renderer->registerEffect(EID_LGP_PLASMA_MEMBRANE, &plasmaMembraneInstance);
    total++;

    static ieffect::LGPNeuralNetworkEffect neuralNetworkInstance;
    renderer->registerEffect(EID_LGP_NEURAL_NETWORK, &neuralNetworkInstance);
    total++;

    static ieffect::LGPCrystallineGrowthEffect crystallineGrowthInstance;
    renderer->registerEffect(EID_LGP_CRYSTALLINE_GROWTH, &crystallineGrowthInstance);
    total++;

    static ieffect::LGPFluidDynamicsEffect fluidDynamicsInstance;
    renderer->registerEffect(EID_LGP_FLUID_DYNAMICS, &fluidDynamicsInstance);
    total++;

    // --- LGP Quantum ---

    static ieffect::LGPQuantumTunnelingEffect quantumTunnelingInstance;
    renderer->registerEffect(EID_LGP_QUANTUM_TUNNELING, &quantumTunnelingInstance);
    total++;

    static ieffect::LGPGravitationalLensingEffect gravitationalLensingInstance;
    renderer->registerEffect(EID_LGP_GRAVITATIONAL_LENSING, &gravitationalLensingInstance);
    total++;

    static ieffect::LGPTimeCrystalEffect timeCrystalInstance;
    renderer->registerEffect(EID_LGP_TIME_CRYSTAL, &timeCrystalInstance);
    total++;

    static ieffect::LGPSolitonWavesEffect solitonWavesInstance;
    renderer->registerEffect(EID_LGP_SOLITON_WAVES, &solitonWavesInstance);
    total++;

    static ieffect::LGPMetamaterialCloakEffect metamaterialCloakInstance;
    renderer->registerEffect(EID_LGP_METAMATERIAL_CLOAK, &metamaterialCloakInstance);
    total++;

    static ieffect::LGPGrinCloakEffect grinCloakInstance;
    renderer->registerEffect(EID_LGP_GRIN_CLOAK, &grinCloakInstance);
    total++;

    static ieffect::LGPCausticFanEffect causticFanInstance;
    renderer->registerEffect(EID_LGP_CAUSTIC_FAN, &causticFanInstance);
    total++;

    static ieffect::LGPBirefringentShearEffect birefringentShearInstance;
    renderer->registerEffect(EID_LGP_BIREFRINGENT_SHEAR, &birefringentShearInstance);
    total++;

    static ieffect::LGPAnisotropicCloakEffect anisotropicCloakInstance;
    renderer->registerEffect(EID_LGP_ANISOTROPIC_CLOAK, &anisotropicCloakInstance);
    total++;

    static ieffect::LGPEvanescentSkinEffect evanescentSkinInstance;
    renderer->registerEffect(EID_LGP_EVANESCENT_SKIN, &evanescentSkinInstance);
    total++;

    // --- LGP Colour Mixing ---

    static ieffect::LGPColorTemperatureEffect colorTemperatureInstance;
    renderer->registerEffect(EID_LGP_COLOR_TEMPERATURE, &colorTemperatureInstance);
    total++;

    static ieffect::LGPRGBPrismEffect rgbPrismInstance;
    renderer->registerEffect(EID_LGP_RGB_PRISM, &rgbPrismInstance);
    total++;

    static ieffect::LGPComplementaryMixingEffect complementaryMixingInstance;
    renderer->registerEffect(EID_LGP_COMPLEMENTARY_MIXING, &complementaryMixingInstance);
    total++;

    static ieffect::LGPQuantumColorsEffect quantumColorsInstance;
    renderer->registerEffect(EID_LGP_QUANTUM_COLORS, &quantumColorsInstance);
    total++;

    static ieffect::LGPDopplerShiftEffect dopplerShiftInstance;
    renderer->registerEffect(EID_LGP_DOPPLER_SHIFT, &dopplerShiftInstance);
    total++;

    static ieffect::LGPColorAcceleratorEffect colorAcceleratorInstance;
    renderer->registerEffect(EID_LGP_COLOR_ACCELERATOR, &colorAcceleratorInstance);
    total++;

    static ieffect::LGPDNAHelixEffect dnaHelixInstance;
    renderer->registerEffect(EID_LGP_DNA_HELIX, &dnaHelixInstance);
    total++;

    static ieffect::LGPPhaseTransitionEffect phaseTransitionInstance;
    renderer->registerEffect(EID_LGP_PHASE_TRANSITION, &phaseTransitionInstance);
    total++;

    static ieffect::LGPChromaticAberrationEffect chromaticAberrationInstance;
    renderer->registerEffect(EID_LGP_CHROMATIC_ABERRATION, &chromaticAberrationInstance);
    total++;

    static ieffect::LGPPerceptualBlendEffect perceptualBlendInstance;
    renderer->registerEffect(EID_LGP_PERCEPTUAL_BLEND, &perceptualBlendInstance);
    total++;

    // --- LGP Novel Physics ---

    static ieffect::LGPChladniHarmonicsEffect chladniHarmonicsInstance;
    renderer->registerEffect(EID_LGP_CHLADNI_HARMONICS, &chladniHarmonicsInstance);
    total++;

    static ieffect::LGPGravitationalWaveChirpEffect gravitationalWaveChirpInstance;
    renderer->registerEffect(EID_LGP_GRAVITATIONAL_WAVE_CHIRP, &gravitationalWaveChirpInstance);
    total++;

    static ieffect::LGPQuantumEntanglementEffect quantumEntanglementInstance;
    renderer->registerEffect(EID_LGP_QUANTUM_ENTANGLEMENT, &quantumEntanglementInstance);
    total++;

    static ieffect::LGPMycelialNetworkEffect mycelialNetworkInstance;
    renderer->registerEffect(EID_LGP_MYCELIAL_NETWORK, &mycelialNetworkInstance);
    total++;

    static ieffect::LGPRileyDissonanceEffect rileyDissonanceInstance;
    renderer->registerEffect(EID_LGP_RILEY_DISSONANCE, &rileyDissonanceInstance);
    total++;

    // --- LGP Chromatic (physics-accurate Cauchy dispersion) ---

    static ieffect::LGPChromaticLensEffect chromaticLensInstance;
    renderer->registerEffect(EID_LGP_CHROMATIC_LENS, &chromaticLensInstance);
    total++;

    static ieffect::LGPChromaticPulseEffect chromaticPulseInstance;
    renderer->registerEffect(EID_LGP_CHROMATIC_PULSE, &chromaticPulseInstance);
    total++;

    static ieffect::ChromaticInterferenceEffect chromaticInterferenceInstance;
    renderer->registerEffect(EID_CHROMATIC_INTERFERENCE, &chromaticInterferenceInstance);
    total++;

    // --- Audio-Reactive ---

    // LGP Audio Test - demonstrates audio pipeline
    static ieffect::LGPAudioTestEffect audioTestInstance;
    renderer->registerEffect(EID_LGP_AUDIO_TEST, &audioTestInstance);
    total++;

    // LGP Beat Pulse - Beat-synchronized radial pulse
    static ieffect::LGPBeatPulseEffect beatPulseInstance;
    renderer->registerEffect(EID_LGP_BEAT_PULSE, &beatPulseInstance);
    total++;

    // LGP Spectrum Bars - 8-band spectrum analyzer
    static ieffect::LGPSpectrumBarsEffect spectrumBarsInstance;
    renderer->registerEffect(EID_LGP_SPECTRUM_BARS, &spectrumBarsInstance);
    total++;

    // LGP Bass Breath - Organic breathing driven by bass
    static ieffect::LGPBassBreathEffect bassBreathInstance;
    renderer->registerEffect(EID_LGP_BASS_BREATH, &bassBreathInstance);
    total++;

    // Audio Waveform - True time-domain waveform visualization
    static ieffect::AudioWaveformEffect audioWaveformInstance;
    renderer->registerEffect(EID_AUDIO_WAVEFORM, &audioWaveformInstance);
    total++;

    // Audio Bloom - Centre bloom pulses triggered by audio transients
    static ieffect::AudioBloomEffect audioBloomInstance;
    renderer->registerEffect(EID_AUDIO_BLOOM, &audioBloomInstance);
    total++;

    // LGP Star Burst Narrative - Legacy core with phrase-gated harmonic colour
    static ieffect::LGPStarBurstNarrativeEffect starBurstNarrativeInstance;
    renderer->registerEffect(EID_LGP_STAR_BURST_NARRATIVE, &starBurstNarrativeInstance);
    total++;

    // LGP Chord Glow - Full chord detection showcase effect
    static ieffect::LGPChordGlowEffect chordGlowInstance;
    renderer->registerEffect(EID_LGP_CHORD_GLOW, &chordGlowInstance);
    total++;

    // Wave Reactive - Energy-accumulating wave with audio-driven motion
    // Uses Kaleidoscope-style energy accumulation (REACTIVE pattern)
    static ieffect::WaveReactiveEffect waveReactiveInstance;
    renderer->registerEffect(EID_WAVE_REACTIVE, &waveReactiveInstance);
    total++;

    // --- Perlin Reactive (audio-reactive noise field patterns) ---

    // LGP Perlin Veil - Slow drifting curtains from centre, audio-driven advection
    static ieffect::LGPPerlinVeilEffect perlinVeilInstance;
    renderer->registerEffect(EID_LGP_PERLIN_VEIL, &perlinVeilInstance);
    total++;

    // LGP Perlin Shocklines - Beat-driven travelling ridges
    static ieffect::LGPPerlinShocklinesEffect perlinShocklinesInstance;
    renderer->registerEffect(EID_LGP_PERLIN_SHOCKLINES, &perlinShocklinesInstance);
    total++;

    // LGP Perlin Caustics - Sparkling caustic lobes, treble->sparkle, bass->scale
    static ieffect::LGPPerlinCausticsEffect perlinCausticsInstance;
    renderer->registerEffect(EID_LGP_PERLIN_CAUSTICS, &perlinCausticsInstance);
    total++;

    // LGP Perlin Interference Weave - Dual-strip moire interference
    static ieffect::LGPPerlinInterferenceWeaveEffect perlinInterferenceWeaveInstance;
    renderer->registerEffect(EID_LGP_PERLIN_INTERFERENCE_WEAVE, &perlinInterferenceWeaveInstance);
    total++;

    // --- Perlin Ambient (time-driven variants) ---

    // LGP Perlin Veil Ambient - Time-driven drifting curtains
    static ieffect::LGPPerlinVeilAmbientEffect perlinVeilAmbientInstance;
    renderer->registerEffect(EID_LGP_PERLIN_VEIL_AMBIENT, &perlinVeilAmbientInstance);
    total++;

    // LGP Perlin Shocklines Ambient - Time-driven travelling ridges
    static ieffect::LGPPerlinShocklinesAmbientEffect perlinShocklinesAmbientInstance;
    renderer->registerEffect(EID_LGP_PERLIN_SHOCKLINES_AMBIENT, &perlinShocklinesAmbientInstance);
    total++;

    // LGP Perlin Caustics Ambient - Time-driven caustic lobes
    static ieffect::LGPPerlinCausticsAmbientEffect perlinCausticsAmbientInstance;
    renderer->registerEffect(EID_LGP_PERLIN_CAUSTICS_AMBIENT, &perlinCausticsAmbientInstance);
    total++;

    // LGP Perlin Interference Weave Ambient - Time-driven moire
    static ieffect::LGPPerlinInterferenceWeaveAmbientEffect perlinInterferenceWeaveAmbientInstance;
    renderer->registerEffect(EID_LGP_PERLIN_INTERFERENCE_WEAVE_AMBIENT, &perlinInterferenceWeaveAmbientInstance);
    total++;

    // --- Perlin Backend Test (A/B/C comparison harness) ---

    // Perlin Backend Test A - FastLED inoise8 baseline
    static ieffect::LGPPerlinBackendFastLEDEffect perlinBackendFastLEDInstance;
    renderer->registerEffect(EID_LGP_PERLIN_BACKEND_FAST_LED, &perlinBackendFastLEDInstance);
    total++;

    // Perlin Backend Test B - Emotiscope 2.0 Perlin full-res
    static ieffect::LGPPerlinBackendEmotiscopeFullEffect perlinBackendEmotiscopeFullInstance;
    renderer->registerEffect(EID_LGP_PERLIN_BACKEND_EMOTISCOPE_FULL, &perlinBackendEmotiscopeFullInstance);
    total++;

    // Perlin Backend Test C - Emotiscope 2.0 Perlin quarter-res + interpolation
    static ieffect::LGPPerlinBackendEmotiscopeQuarterEffect perlinBackendEmotiscopeQuarterInstance;
    renderer->registerEffect(EID_LGP_PERLIN_BACKEND_EMOTISCOPE_QUARTER, &perlinBackendEmotiscopeQuarterInstance);
    total++;

    // =============== ENHANCED AUDIO-REACTIVE EFFECTS ===============

    // BPM Enhanced - Tempo-locked pulse rings
    static ieffect::BPMEnhancedEffect bpmEnhancedInstance;
    renderer->registerEffect(EID_BPM_ENHANCED, &bpmEnhancedInstance);
    total++;

    // Breathing Enhanced - Style-adaptive breathing
    static ieffect::BreathingEnhancedEffect breathingEnhancedInstance;
    renderer->registerEffect(EID_BREATHING_ENHANCED, &breathingEnhancedInstance);
    total++;

    // Chevron Waves Enhanced - Beat-synced chevron propagation
    static ieffect::ChevronWavesEnhancedEffect chevronWavesEnhancedInstance;
    renderer->registerEffect(EID_CHEVRON_WAVES_ENHANCED, &chevronWavesEnhancedInstance);
    total++;

    // LGP Interference Scanner Enhanced - Audio-reactive scan speed
    static ieffect::LGPInterferenceScannerEnhancedEffect interferenceScannerEnhancedInstance;
    renderer->registerEffect(EID_LGP_INTERFERENCE_SCANNER_ENHANCED, &interferenceScannerEnhancedInstance);
    total++;

    // LGP Photonic Crystal Enhanced - Harmonic lattice modulation
    static ieffect::LGPPhotonicCrystalEnhancedEffect photonicCrystalEnhancedInstance;
    renderer->registerEffect(EID_LGP_PHOTONIC_CRYSTAL_ENHANCED, &photonicCrystalEnhancedInstance);
    total++;

    // LGP Spectrum Detail - 64-bin FFT direct visualisation
    static ieffect::LGPSpectrumDetailEffect spectrumDetailInstance;
    renderer->registerEffect(EID_LGP_SPECTRUM_DETAIL, &spectrumDetailInstance);
    total++;

    // LGP Spectrum Detail Enhanced - Saliency-weighted spectrum
    static ieffect::LGPSpectrumDetailEnhancedEffect spectrumDetailEnhancedInstance;
    renderer->registerEffect(EID_LGP_SPECTRUM_DETAIL_ENHANCED, &spectrumDetailEnhancedInstance);
    total++;

    // LGP Star Burst Enhanced - Beat-triggered bursts
    static ieffect::LGPStarBurstEnhancedEffect starBurstEnhancedInstance;
    renderer->registerEffect(EID_LGP_STAR_BURST_ENHANCED, &starBurstEnhancedInstance);
    total++;

    // LGP Wave Collision Enhanced - Audio-driven wave interference
    static ieffect::LGPWaveCollisionEnhancedEffect waveCollisionEnhancedInstance;
    renderer->registerEffect(EID_LGP_WAVE_COLLISION_ENHANCED, &waveCollisionEnhancedInstance);
    total++;

    // Ripple Enhanced - Beat-synced ripple propagation
    static ieffect::RippleEnhancedEffect rippleEnhancedInstance;
    renderer->registerEffect(EID_RIPPLE_ENHANCED, &rippleEnhancedInstance);
    total++;

    // Snapwave Linear - v1 parity, LINEAR exempt from CENTER_ORIGIN
    static ieffect::SnapwaveLinearEffect snapwaveLinearInstance;
    renderer->registerEffect(EID_SNAPWAVE_LINEAR, &snapwaveLinearInstance);
    total++;

    // =============== DIAGNOSTIC EFFECTS ===============

    // Trinity Test - Diagnostic effect for PRISM Trinity data flow verification
    static ieffect::TrinityTestEffect trinityTestInstance;
    renderer->registerEffect(EID_TRINITY_TEST, &trinityTestInstance);
    total++;

    // =============== PALETTE AUTO-CYCLE EFFECTS ===============

    // LGP Holographic Auto-Cycle - Holographic with internal palette cycling
    static ieffect::LGPHolographicAutoCycleEffect holographicAutoCycleInstance;
    renderer->registerEffect(EID_LGP_HOLOGRAPHIC_AUTO_CYCLE, &holographicAutoCycleInstance);
    total++;

    // =============== ES v1.1 REFERENCE SHOWS ===============
    // These are ports of Emotiscope v1.1_320 light modes for parity comparisons.

    static ieffect::esv11_reference::EsAnalogRefEffect esAnalogRefInstance;
    renderer->registerEffect(EID_ES_ANALOG, &esAnalogRefInstance);
    total++;

    static ieffect::esv11_reference::EsSpectrumRefEffect esSpectrumRefInstance;
    renderer->registerEffect(EID_ES_SPECTRUM, &esSpectrumRefInstance);
    total++;

    static ieffect::esv11_reference::EsOctaveRefEffect esOctaveRefInstance;
    renderer->registerEffect(EID_ES_OCTAVE, &esOctaveRefInstance);
    total++;

    static ieffect::esv11_reference::EsBloomRefEffect esBloomRefInstance;
    renderer->registerEffect(EID_ES_BLOOM, &esBloomRefInstance);
    total++;

    static ieffect::esv11_reference::EsWaveformRefEffect esWaveformRefInstance;
    renderer->registerEffect(EID_ES_WAVEFORM, &esWaveformRefInstance);
    total++;

    // --- ES Tuned ---

    static ieffect::RippleEsTunedEffect rippleEsTunedInstance;
    renderer->registerEffect(EID_RIPPLE_ES_TUNED, &rippleEsTunedInstance);
    total++;

    static ieffect::HeartbeatEsTunedEffect heartbeatEsTunedInstance;
    renderer->registerEffect(EID_HEARTBEAT_ES_TUNED, &heartbeatEsTunedInstance);
    total++;

    static ieffect::LGPHolographicEsTunedEffect holographicEsTunedInstance;
    renderer->registerEffect(EID_LGP_HOLOGRAPHIC_ES_TUNED, &holographicEsTunedInstance);
    total++;

    // =============== SENSORY BRIDGE REFERENCE SHOWS ===============
    // These are ports of Sensory Bridge 3.1.0 light show modes for parity comparisons.

    static ieffect::sensorybridge_reference::SbWaveform310RefEffect sbWaveform310RefInstance;
    renderer->registerEffect(EID_SB_WAVEFORM310, &sbWaveform310RefInstance);
    total++;

    // =============== BEAT PULSE FAMILY ===============

    // Beat Pulse (Stack) - UI preview parity (static gradient + white push)
    static ieffect::BeatPulseStackEffect beatPulseStackInstance;
    renderer->registerEffect(EID_BEAT_PULSE_STACK, &beatPulseStackInstance);
    total++;

    // Beat Pulse (Shockwave) - HTML parity outward ring (amplitude-driven position)
    static ieffect::BeatPulseShockwaveEffect beatPulseShockwaveInstance(false);  // false = outward
    renderer->registerEffect(EID_BEAT_PULSE_SHOCKWAVE, &beatPulseShockwaveInstance);
    total++;

    // NOTE: Old ID 112 (Shockwave In) has been RETIRED. Use Parity/Stack for inward effects.

    // Beat Pulse (Void) - Parity ring in darkness, palette-coloured detonation against black
    static ieffect::BeatPulseVoidEffect beatPulseVoidInstance;
    renderer->registerEffect(EID_BEAT_PULSE_VOID, &beatPulseVoidInstance);
    total++;

    // Beat Pulse (Resonant) - Double ring contracting inward: attack snap + resonant body
    static ieffect::BeatPulseResonantEffect beatPulseResonantInstance;
    renderer->registerEffect(EID_BEAT_PULSE_RESONANT, &beatPulseResonantInstance);
    total++;

    // Beat Pulse (Ripple) - 3-slot ring buffer of cascading implosion rings
    static ieffect::BeatPulseRippleEffect beatPulseRippleInstance;
    renderer->registerEffect(EID_BEAT_PULSE_RIPPLE, &beatPulseRippleInstance);
    total++;

    // Beat Pulse (Shockwave Cascade) - Outward expansion with trailing echo rings
    static ieffect::BeatPulseShockwaveCascadeEffect beatPulseShockwaveCascadeInstance;
    renderer->registerEffect(EID_BEAT_PULSE_SHOCKWAVE_CASCADE, &beatPulseShockwaveCascadeInstance);
    total++;

    // Beat Pulse (Spectral) - Three frequency-driven rings: bass outer, mid middle, treble centre
    static ieffect::BeatPulseSpectralEffect beatPulseSpectralInstance;
    renderer->registerEffect(EID_BEAT_PULSE_SPECTRAL, &beatPulseSpectralInstance);
    total++;

    // Beat Pulse (Spectral Pulse) - Stationary zones pulsing by frequency band
    static ieffect::BeatPulseSpectralPulseEffect beatPulseSpectralPulseInstance;
    renderer->registerEffect(EID_BEAT_PULSE_SPECTRAL_PULSE, &beatPulseSpectralPulseInstance);
    total++;

    // Beat Pulse (Breathe) - Whole-strip amplitude pump with centre-weighted glow
    static ieffect::BeatPulseBreatheEffect beatPulseBreatheInstance;
    renderer->registerEffect(EID_BEAT_PULSE_BREATHE, &beatPulseBreatheInstance);
    total++;

    // Beat Pulse (LGP Interference) - Dual-strip phase control for optical interference
    static ieffect::BeatPulseLGPInterferenceEffect beatPulseLGPInterferenceInstance;
    renderer->registerEffect(EID_BEAT_PULSE_LGP_INTERFERENCE, &beatPulseLGPInterferenceInstance);
    total++;

    // Beat Pulse (Bloom) - Beat Pulse transport bloom variant.
    static ieffect::BeatPulseBloomEffect beatPulseBloomInstance;
    renderer->registerEffect(EID_BEAT_PULSE_BLOOM, &beatPulseBloomInstance);
    total++;

    // =============== TRANSPORT / PARITY ===============

    // Bloom (Parity) - Sensory Bridge Bloom parity: subpixel advection transport
    // Transport is the brush. History is the canvas. Audio is just pigment injection.
    static ieffect::BloomParityEffect bloomParityInstance;
    renderer->registerEffect(EID_BLOOM_PARITY, &bloomParityInstance);
    total++;

    // Kuramoto Transport - Invisible oscillators -> event injection -> transported light
    // Architecture: Kuramoto field generates phase dynamics, feature extractor finds events,
    // transport buffer advects visible "light substance". Audio steers only engine params.
    static ieffect::KuramotoTransportEffect kuramotoTransportInstance;
    renderer->registerEffect(EID_KURAMOTO_TRANSPORT, &kuramotoTransportInstance);
    total++;

    // Waveform Parity - Sensory Bridge 3.1.0 waveform mode
    // Intensity-only rendering + palette at output. dt-corrected. Dynamic normalisation.
    // NOTE: Mapped to EID_LGP_OPAL_FILM in the stable ID scheme.
    static ieffect::WaveformParityEffect waveformParityInstance;
    renderer->registerEffect(EID_LGP_OPAL_FILM, &waveformParityInstance);
    total++;

    // =============== HOLOGRAPHIC VARIANTS ===============

    // LGP Opal Film - Thin-film iridescence bands
    static ieffect::LGPOpalFilmEffect opalFilmInstance;
    renderer->registerEffect(EID_LGP_GRATING_SCAN, &opalFilmInstance);
    total++;

    // LGP Grating Scan - Diffraction scan highlight
    static ieffect::LGPGratingScanEffect gratingScanInstance;
    renderer->registerEffect(EID_LGP_STRESS_GLASS, &gratingScanInstance);
    total++;

    // LGP Stress Glass - Photoelastic fringe field
    static ieffect::LGPStressGlassEffect stressGlassInstance;
    renderer->registerEffect(EID_LGP_MOIRE_SILK, &stressGlassInstance);
    total++;

    // LGP Moire Silk - Two-lattice beat pattern
    static ieffect::LGPMoireSilkEffect moireSilkInstance;
    renderer->registerEffect(EID_LGP_CAUSTIC_SHARDS, &moireSilkInstance);
    total++;

    // LGP Caustic Shards - Interference with prismatic glints
    static ieffect::LGPCausticShardsEffect causticShardsInstance;
    renderer->registerEffect(EID_LGP_PARALLAX_DEPTH, &causticShardsInstance);
    total++;

    // LGP Parallax Depth - Two-layer refractive parallax
    static ieffect::LGPParallaxDepthEffect parallaxDepthInstance;
    renderer->registerEffect(EID_LGP_STRESS_GLASS_MELT, &parallaxDepthInstance);
    total++;

    // LGP Stress Glass (Melt) - Phase-locked wings
    static ieffect::LGPStressGlassMeltEffect stressGlassMeltInstance;
    renderer->registerEffect(EID_LGP_GRATING_SCAN_BREAKUP, &stressGlassMeltInstance);
    total++;

    // LGP Grating Scan (Breakup) - Halo breakup
    static ieffect::LGPGratingScanBreakupEffect gratingScanBreakupInstance;
    renderer->registerEffect(EID_LGP_WATER_CAUSTICS, &gratingScanBreakupInstance);
    total++;

    // LGP Water Caustics - Ray-envelope caustic filaments
    static ieffect::LGPWaterCausticsEffect waterCausticsInstance;
    renderer->registerEffect(EID_LGP_SCHLIEREN_FLOW, &waterCausticsInstance);
    total++;

    // LGP Schlieren Flow - Knife-edge gradient flow
    static ieffect::LGPSchlierenFlowEffect schlierenFlowInstance;
    renderer->registerEffect(EID_LGP_REACTION_DIFFUSION, &schlierenFlowInstance);
    total++;

    // --- Reaction Diffusion ---

    // LGP Reaction Diffusion - Gray-Scott slime
    static ieffect::LGPReactionDiffusionEffect reactionDiffusionInstance;
    renderer->registerEffect(EID_LGP_REACTION_DIFFUSION_TRIANGLE, &reactionDiffusionInstance);
    total++;

    // LGP RD Triangle - Reaction-diffusion front wedge isolation
    static ieffect::LGPReactionDiffusionTriangleEffect reactionDiffusionTriangleInstance;
    renderer->registerEffect(EID_LGP_TALBOT_CARPET, &reactionDiffusionTriangleInstance);
    total++;

    // --- Shape Bangers Pack ---

    // LGP Talbot Carpet - Self-imaging lattice rug
    static ieffect::LGPTalbotCarpetEffect talbotCarpetInstance;
    renderer->registerEffect(EID_LGP_AIRY_COMET, &talbotCarpetInstance);
    total++;

    // LGP Airy Comet - Self-accelerating comet with trailing lobes
    static ieffect::LGPAiryCometEffect airyCometInstance;
    renderer->registerEffect(EID_LGP_MOIRE_CATHEDRAL, &airyCometInstance);
    total++;

    // LGP Moire Cathedral - Interference arches from close gratings
    static ieffect::LGPMoireCathedralEffect moireCathedralInstance;
    renderer->registerEffect(EID_LGP_SUPERFORMULA_GLYPH, &moireCathedralInstance);
    total++;

    // LGP Living Glyph - Superformula morphing supershapes
    static ieffect::LGPSuperformulaGlyphEffect superformulaGlyphInstance;
    renderer->registerEffect(EID_LGP_SPIROGRAPH_CROWN, &superformulaGlyphInstance);
    total++;

    // LGP Spirograph Crown - Hypotrochoid gear-flower loops
    static ieffect::LGPSpirographCrownEffect spirographCrownInstance;
    renderer->registerEffect(EID_LGP_ROSE_BLOOM, &spirographCrownInstance);
    total++;

    // LGP Rose Bloom - Rhodonea petal engine
    static ieffect::LGPRoseBloomEffect roseBloomInstance;
    renderer->registerEffect(EID_LGP_HARMONOGRAPH_HALO, &roseBloomInstance);
    total++;

    // LGP Harmonograph Halo - Lissajous aura orbitals
    static ieffect::LGPHarmonographHaloEffect harmonographHaloInstance;
    renderer->registerEffect(EID_LGP_RULE30_CATHEDRAL, &harmonographHaloInstance);
    total++;

    // LGP Rule 30 Cathedral - Elementary CA textile
    static ieffect::LGPRule30CathedralEffect rule30CathedralInstance;
    renderer->registerEffect(EID_LGP_LANGTON_HIGHWAY, &rule30CathedralInstance);
    total++;

    // LGP Langton Highway - Emergent ant-to-highway projection
    static ieffect::LGPLangtonHighwayEffect langtonHighwayInstance;
    renderer->registerEffect(EID_LGP_CYMATIC_LADDER, &langtonHighwayInstance);
    total++;

    // LGP Cymatic Ladder - Standing-wave sculpture
    static ieffect::LGPCymaticLadderEffect cymaticLadderInstance;
    renderer->registerEffect(EID_LGP_MACH_DIAMONDS, &cymaticLadderInstance);
    total++;

    // LGP Mach Diamonds - Shock-diamond jewellery
    static ieffect::LGPMachDiamondsEffect machDiamondsInstance;
    renderer->registerEffect(EID_LGP_CHIMERA_CROWN, &machDiamondsInstance);
    total++;

    // --- Holy Shit Bangers Pack ---

    static ieffect::LGPChimeraCrownEffect chimeraCrownInstance;
    renderer->registerEffect(EID_LGP_CATASTROPHE_CAUSTICS, &chimeraCrownInstance);
    total++;

    static ieffect::LGPCatastropheCausticsEffect catastropheCausticsInstance;
    renderer->registerEffect(EID_LGP_HYPERBOLIC_PORTAL, &catastropheCausticsInstance);
    total++;

    static ieffect::LGPHyperbolicPortalEffect hyperbolicPortalInstance;
    renderer->registerEffect(EID_LGP_LORENZ_RIBBON, &hyperbolicPortalInstance);
    total++;

    static ieffect::LGPLorenzRibbonEffect lorenzRibbonInstance;
    renderer->registerEffect(EID_LGP_IFS_BIO_RELIC, &lorenzRibbonInstance);
    total++;

    static ieffect::LGPIFSBioRelicEffect ifsBioRelicInstance;
    renderer->registerEffect(EID_LGP_FLUX_RIFT, &ifsBioRelicInstance);
    total++;

    // --- Experimental Audio Pack ---

    static ieffect::LGPFluxRiftEffect fluxRiftInstance;
    renderer->registerEffect(EID_LGP_BEAT_PRISM, &fluxRiftInstance);
    total++;

    static ieffect::LGPBeatPrismEffect beatPrismInstance;
    renderer->registerEffect(EID_LGP_HARMONIC_TIDE, &beatPrismInstance);
    total++;

    static ieffect::LGPHarmonicTideEffect harmonicTideInstance;
    renderer->registerEffect(EID_LGP_BASS_QUAKE, &harmonicTideInstance);
    total++;

    static ieffect::LGPBassQuakeEffect bassQuakeInstance;
    renderer->registerEffect(EID_LGP_TREBLE_NET, &bassQuakeInstance);
    total++;

    static ieffect::LGPTrebleNetEffect trebleNetInstance;
    renderer->registerEffect(EID_LGP_RHYTHMIC_GATE, &trebleNetInstance);
    total++;

    static ieffect::LGPRhythmicGateEffect rhythmicGateInstance;
    renderer->registerEffect(EID_LGP_SPECTRAL_KNOT, &rhythmicGateInstance);
    total++;

    static ieffect::LGPSpectralKnotEffect spectralKnotInstance;
    renderer->registerEffect(EID_LGP_SALIENCY_BLOOM, &spectralKnotInstance);
    total++;

    static ieffect::LGPSaliencyBloomEffect saliencyBloomInstance;
    renderer->registerEffect(EID_LGP_TRANSIENT_LATTICE, &saliencyBloomInstance);
    total++;

    static ieffect::LGPTransientLatticeEffect transientLatticeInstance;
    renderer->registerEffect(EID_LGP_WAVELET_MIRROR, &transientLatticeInstance);
    total++;

    // NOTE: waveletMirrorInstance has no mapping entry (shifted out by retired ID 112 gap)
    // It will be assigned a proper EID when the next batch of IDs is allocated.
    // For now, it is not registered.
    // static ieffect::LGPWaveletMirrorEffect waveletMirrorInstance;

    // --- Showpiece Pack 3 ---
    static ieffect::LGPTimeReversalMirrorEffect timeReversalMirrorInstance;
    renderer->registerEffect(EID_LGP_TIME_REVERSAL_MIRROR, &timeReversalMirrorInstance);
    total++;

    static ieffect::LGPKdVSolitonPairEffect kdvSolitonPairInstance;
    renderer->registerEffect(EID_LGP_KDV_SOLITON_PAIR, &kdvSolitonPairInstance);
    total++;

    static ieffect::LGPGoldCodeSpeckleEffect goldCodeSpeckleInstance;
    renderer->registerEffect(EID_LGP_GOLD_CODE_SPECKLE, &goldCodeSpeckleInstance);
    total++;

    static ieffect::LGPQuasicrystalLatticeEffect quasicrystalLatticeInstance;
    renderer->registerEffect(EID_LGP_QUASICRYSTAL_LATTICE, &quasicrystalLatticeInstance);
    total++;

    static ieffect::LGPFresnelCausticSweepEffect fresnelCausticSweepInstance;
    renderer->registerEffect(EID_LGP_FRESNEL_CAUSTIC_SWEEP, &fresnelCausticSweepInstance);
    total++;

    static ieffect::LGPTimeReversalMirrorEffect_AR timeReversalMirrorARInstance;
    renderer->registerEffect(EID_LGP_TIME_REVERSAL_MIRROR_AR, &timeReversalMirrorARInstance);
    total++;

    static ieffect::LGPTimeReversalMirrorEffect_Mod1 timeReversalMirrorMod1Instance;
    renderer->registerEffect(EID_LGP_TIME_REVERSAL_MIRROR_MOD1, &timeReversalMirrorMod1Instance);
    total++;

    static ieffect::LGPTimeReversalMirrorEffect_Mod2 timeReversalMirrorMod2Instance;
    renderer->registerEffect(EID_LGP_TIME_REVERSAL_MIRROR_MOD2, &timeReversalMirrorMod2Instance);
    total++;

    static ieffect::LGPTimeReversalMirrorEffect_Mod3 timeReversalMirrorMod3Instance;
    renderer->registerEffect(EID_LGP_TIME_REVERSAL_MIRROR_MOD3, &timeReversalMirrorMod3Instance);
    total++;

    // =============== EFFECT COUNT PARITY VALIDATION ===============
    constexpr uint16_t EXPECTED_EFFECT_COUNT = 170;
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
