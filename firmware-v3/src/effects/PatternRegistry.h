// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * Pattern Registry
 * 
 * Provides programmatic access to pattern taxonomy metadata, enabling:
 * - Runtime pattern discovery and filtering by family
 * - Composition rules enforcement
 * - Show choreography automation
 * - API-driven pattern selection by family/tags
 * 
 * Based on taxonomy from docs/analysis/b2. LGP_PATTERN_TAXONOMY.md
 * Organizes 118+ patterns into 10 families with relationships.
 * 
 * IMPORTANT: Pattern metadata indices MUST match effect IDs exactly.
 * Effect IDs are stable and match v1's effects[] array order.
 */

#ifndef PATTERN_REGISTRY_H
#define PATTERN_REGISTRY_H

#include <stdint.h>
#include <Arduino.h>
#include "../plugins/api/IEffect.h"

// ============================================================================
// Pattern Family Enumeration (10 families from taxonomy)
// ============================================================================

enum class PatternFamily : uint8_t {
    INTERFERENCE = 0,      // Standing Wave, Moiré Envelope, Phase Collision (13 patterns)
    GEOMETRIC = 1,         // Tiled Boxes, Radial Rings, Fractal Zoom (8 patterns)
    ADVANCED_OPTICAL = 2,  // Chromatic Lens, Diffraction Grating, Caustics (6 patterns)
    ORGANIC = 3,           // Bioluminescence, Mycelium, Plankton Waves (12 patterns)
    QUANTUM = 4,           // Wave Function, Soliton Explorer, Entanglement (11 patterns)
    COLOR_MIXING = 5,      // Warm Crystal, Cold Crystal, Ember Fade (12 patterns)
    PHYSICS_BASED = 6,     // Plasma Flow, Magnetic Field, Electrostatic (6 patterns)
    NOVEL_PHYSICS = 7,     // Rogue Wave, Turing Pattern, Kelvin-Helmholtz (5 patterns)
    FLUID_PLASMA = 8,      // Shockwave, Convection, Vortex (5 patterns)
    MATHEMATICAL = 9,      // Mandelbrot Zoom, Strange Attractor, Kuramoto (5 patterns)
    UNKNOWN = 255          // Unclassified pattern
};

// Pattern family names (PROGMEM)
const char FAMILY_INTERFERENCE[] PROGMEM = "Interference";
const char FAMILY_GEOMETRIC[] PROGMEM = "Geometric";
const char FAMILY_ADVANCED_OPTICAL[] PROGMEM = "Advanced Optical";
const char FAMILY_ORGANIC[] PROGMEM = "Organic";
const char FAMILY_QUANTUM[] PROGMEM = "Quantum";
const char FAMILY_COLOR_MIXING[] PROGMEM = "Color Mixing";
const char FAMILY_PHYSICS_BASED[] PROGMEM = "Physics-Based";
const char FAMILY_NOVEL_PHYSICS[] PROGMEM = "Novel Physics";
const char FAMILY_FLUID_PLASMA[] PROGMEM = "Fluid & Plasma";
const char FAMILY_MATHEMATICAL[] PROGMEM = "Mathematical";
const char FAMILY_UNKNOWN[] PROGMEM = "Unknown";

const char* const PATTERN_FAMILY_NAMES[] PROGMEM = {
    FAMILY_INTERFERENCE,
    FAMILY_GEOMETRIC,
    FAMILY_ADVANCED_OPTICAL,
    FAMILY_ORGANIC,
    FAMILY_QUANTUM,
    FAMILY_COLOR_MIXING,
    FAMILY_PHYSICS_BASED,
    FAMILY_NOVEL_PHYSICS,
    FAMILY_FLUID_PLASMA,
    FAMILY_MATHEMATICAL
};

// ============================================================================
// Pattern Tags (bitfield)
// ============================================================================

namespace PatternTags {
    constexpr uint8_t STANDING = 0x01;      // Standing wave pattern
    constexpr uint8_t TRAVELING = 0x02;     // Traveling wave pattern
    constexpr uint8_t MOIRE = 0x04;          // Moiré interference pattern
    constexpr uint8_t DEPTH = 0x08;          // Depth illusion effect
    constexpr uint8_t SPECTRAL = 0x10;       // Spectral/chromatic effect
    constexpr uint8_t CENTER_ORIGIN = 0x20;  // Originates from LEDs 79/80
    constexpr uint8_t DUAL_STRIP = 0x40;     // Uses dual-strip interference
    constexpr uint8_t PHYSICS = 0x80;        // Physics simulation based
}

// ============================================================================
// Pattern Metadata Structure
// ============================================================================

struct PatternMetadata {
    const char* name;              // Pattern name (e.g., "LGP Standing Wave")
    PatternFamily family;          // Pattern family classification
    uint8_t tags;                  // Bitfield: STANDING|TRAVELING|MOIRE|DEPTH|SPECTRAL|...
    const char* story;             // One-sentence narrative description
    const char* opticalIntent;     // Which optical levers are used
    const char* relatedPatterns;   // Comma-separated list of related pattern names (optional)
    
    // Helper functions
    bool hasTag(uint8_t tag) const { return (tags & tag) != 0; }
    bool isStanding() const { return hasTag(PatternTags::STANDING); }
    bool isTraveling() const { return hasTag(PatternTags::TRAVELING); }
    bool isMoire() const { return hasTag(PatternTags::MOIRE); }
    bool hasDepth() const { return hasTag(PatternTags::DEPTH); }
    bool isSpectral() const { return hasTag(PatternTags::SPECTRAL); }
    bool isCenterOrigin() const { return hasTag(PatternTags::CENTER_ORIGIN); }
    bool isDualStrip() const { return hasTag(PatternTags::DUAL_STRIP); }
    bool isPhysicsBased() const { return hasTag(PatternTags::PHYSICS); }
};

// ============================================================================
// Pattern Registry Functions
// ============================================================================

namespace PatternRegistry {

/**
 * Get pattern metadata by name
 * @param name Pattern name (case-sensitive)
 * @return PatternMetadata pointer, or nullptr if not found
 */
const PatternMetadata* getPatternMetadata(const char* name);

/**
 * Get pattern metadata by index (effect ID)
 * @param index Effect ID (matches v1/v2 effect registration order)
 * @return PatternMetadata pointer, or nullptr if index invalid
 */
const PatternMetadata* getPatternMetadata(uint8_t index);

/**
 * Get all patterns in a family
 * @param family Pattern family to filter by
 * @param output Array to store pattern indices (must be large enough)
 * @param maxOutput Maximum number of patterns to return
 * @return Number of patterns found
 */
uint8_t getPatternsByFamily(PatternFamily family, uint8_t* output, uint8_t maxOutput);

/**
 * Get related patterns for a given pattern
 * @param name Pattern name
 * @param output Array to store related pattern names (must be large enough)
 * @param maxOutput Maximum number of patterns to return
 * @return Number of related patterns found
 */
uint8_t getRelatedPatterns(const char* name, const char** output, uint8_t maxOutput);

/**
 * Get pattern family name
 * @param family Pattern family
 * @param buffer Output buffer (should be at least 30 chars)
 * @param bufferSize Size of buffer
 */
void getFamilyName(PatternFamily family, char* buffer, size_t bufferSize);

/**
 * Check if pattern belongs to family
 * @param name Pattern name
 * @param family Pattern family to check
 * @return true if pattern belongs to family
 */
bool patternInFamily(const char* name, PatternFamily family);

/**
 * Get total number of registered patterns
 * @return Number of patterns with metadata
 */
uint8_t getPatternCount();

/**
 * Get number of patterns in a family
 * @param family Pattern family
 * @return Number of patterns in family
 */
uint8_t getFamilyCount(PatternFamily family);

/**
 * Check if an effect is LGP-sensitive (should skip ColorCorrectionEngine)
 * @param effectId Effect ID to check
 * @return true if effect is LGP-sensitive and should skip color correction
 * 
 * LGP-sensitive effects are those that rely on precise amplitude relationships
 * for interference patterns. Color correction breaks these relationships.
 * 
 * Includes:
 * - INTERFERENCE family effects (10, 13-17)
 * - ADVANCED_OPTICAL family effects with CENTER_ORIGIN tag (26-33, 65-67)
 * - Specific IDs: 10, 13, 16, 26, 32, 65, 66, 67
 */
bool isLGPSensitive(uint8_t effectId);

/**
 * Check if an effect is stateful (reads previous frame buffer)
 * @param effectId Effect ID to check
 * @return true if effect is stateful and should skip color correction
 * 
 * Stateful effects read from ctx.leds in the previous frame for propagation.
 * ColorCorrectionEngine mutates m_leds in-place, corrupting the feedback loop.
 * 
 * Currently includes: 3 (Confetti), 8 (Ripple)
 */
bool isStatefulEffect(uint8_t effectId);

/**
 * Validate and clamp effect ID to safe range
 * @param effectId Effect ID to validate
 * @return Valid effect ID, defaults to 0 if out of bounds
 */
uint8_t validateEffectId(uint8_t effectId);

/**
 * Get IEffect metadata for an effect (if available)
 * @param effectId Effect ID to query
 * @return Pointer to EffectMetadata, or nullptr if not available or not an IEffect
 */
const lightwaveos::plugins::EffectMetadata* getIEffectMetadata(uint8_t effectId);

/**
 * Check if an effect has IEffect metadata available
 * @param effectId Effect ID to check
 * @return true if effect is an IEffect instance with metadata
 */
bool hasIEffectMetadata(uint8_t effectId);

/**
 * Check if an effect should skip ColorCorrectionEngine processing
 * @param effectId Effect ID to check
 * @return true if effect should skip color correction
 *
 * Consolidates all color correction skip logic:
 * - LGP-sensitive effects (INTERFERENCE, ADVANCED_OPTICAL families)
 * - Stateful effects (Confetti, Ripple - read previous frame buffer)
 * - PHYSICS_BASED family (precise amplitude for physics simulations)
 * - MATHEMATICAL family (exact RGB values for mathematical mappings)
 *
 * Performance: Skipping saves ~1,500µs per frame for eligible effects
 */
bool shouldSkipColorCorrection(uint8_t effectId);

// ============================================================================
// Effect Register Functions (for filtered effect cycling)
// ============================================================================

/**
 * @brief Check if an effect is audio-reactive
 * @param effectId Effect ID to check (effect ID space; see RendererActor::MAX_EFFECTS)
 * @return true if effect actively uses ctx.audio features
 *
 * Audio-reactive effects respond to real-time audio input via:
 * - Beat tracking (beatPhase, isOnBeat, bpm)
 * - Energy metrics (rms, flux, bass, mid, treble)
 * - Spectrum analysis (getBand, bins64)
 * - Chord detection (chordType, rootNote)
 */
bool isAudioReactive(uint8_t effectId);

/**
 * @brief Get the number of audio-reactive effects
 * @return Count of effects in REACTIVE_EFFECT_IDS array
 */
uint8_t getReactiveEffectCount();

/**
 * @brief Get reactive effect ID by index
 * @param index Index within reactive effects array (0 to getReactiveEffectCount()-1)
 * @return Effect ID, or 0xFF if index out of bounds
 */
uint8_t getReactiveEffectId(uint8_t index);

/**
 * @brief Build ambient effect ID array (call once at startup)
 * @param outputArray Array to populate with ambient effect IDs
 * @param maxOutput Maximum size of output array
 * @param effectCount Total number of registered effects
 * @return Number of ambient effects found
 *
 * Ambient effects are all effects NOT in the audio-reactive list.
 * They use time-based animation or user parameters only.
 */
uint8_t buildAmbientEffectArray(uint8_t* outputArray, uint8_t maxOutput, uint8_t effectCount);

} // namespace PatternRegistry

// ============================================================================
// Effect Register Enum (for filtered effect cycling hotkeys)
// ============================================================================

/**
 * @brief Effect register mode for filtering effect cycling
 *
 * Controls which effects are cycled through when pressing spacebar/n/N:
 * - ALL: Default behavior, cycles through all registered effects
 * - REACTIVE: Audio-reactive effects only (~19 effects that use ctx.audio)
 * - AMBIENT: Time-based effects only (~58 effects without audio coupling)
 *
 * Hotkeys:
 * - 'r' = Switch to REACTIVE register
 * - 'm' = Switch to AMBIENT register
 * - 'A' = Switch back to ALL (default)
 */
enum class EffectRegister : uint8_t {
    ALL = 0,      // All effects (default, current behavior)
    REACTIVE = 1, // Audio-reactive effects only
    AMBIENT = 2   // Time-based/ambient effects only
};

// ============================================================================
// Pattern Metadata Registry (defined in PatternRegistry.cpp)
// ============================================================================

extern const PatternMetadata PATTERN_METADATA[];
extern const uint8_t PATTERN_METADATA_COUNT;

#endif // PATTERN_REGISTRY_H
