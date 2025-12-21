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

} // namespace PatternRegistry

// ============================================================================
// Pattern Metadata Registry (defined in PatternRegistry.cpp)
// ============================================================================

extern const PatternMetadata PATTERN_METADATA[];
extern const uint8_t PATTERN_METADATA_COUNT;

#endif // PATTERN_REGISTRY_H

