#ifndef SHOW_TYPES_H
#define SHOW_TYPES_H

#include <Arduino.h>

// ============================================================================
// SHOW TYPES - Data structures for choreographed light shows
// ============================================================================
// The ShowDirector system orchestrates multi-minute light shows with
// timed cues, parameter sweeps, and chapter-based narrative structure.
//
// Memory Budget:
// - ShowPlaybackState: 20 bytes RAM
// - CueScheduler: 8 bytes RAM
// - ParameterSweeper: 80 bytes RAM
// - ShowDefinitions: ~2KB PROGMEM (10 shows)
// ============================================================================

// Zone target constant
static constexpr uint8_t ZONE_GLOBAL = 0xFF;

// ============================================================================
// CUE TYPES
// ============================================================================
// Types of cues that can be scheduled during a show

enum CueType : uint8_t {
    CUE_EFFECT = 0,           // Change effect on zone or global
    CUE_PARAMETER_SWEEP,      // Interpolate parameter over time
    CUE_ZONE_CONFIG,          // Configure zone settings
    CUE_TRANSITION,           // Trigger TransitionEngine type
    CUE_NARRATIVE,            // Modulate NarrativeEngine tempo/phase
    CUE_PALETTE,              // Change color palette
    CUE_MARKER                // Sync point marker (no action)
};

// ============================================================================
// PARAMETER IDS
// ============================================================================
// Sweepable parameters for smooth transitions

enum ParamId : uint8_t {
    PARAM_BRIGHTNESS = 0,
    PARAM_SPEED,
    PARAM_INTENSITY,
    PARAM_SATURATION,
    PARAM_COMPLEXITY,
    PARAM_VARIATION,
    PARAM_COUNT               // Sentinel for bounds checking
};

// ============================================================================
// NARRATIVE PHASE
// ============================================================================
// Matches NarrativeEngine phases for integration

enum ShowNarrativePhase : uint8_t {
    SHOW_PHASE_BUILD = 0,     // Intensity rising
    SHOW_PHASE_HOLD,          // Peak intensity
    SHOW_PHASE_RELEASE,       // Intensity falling
    SHOW_PHASE_REST           // Low intensity pause
};

// ============================================================================
// SHOW CUE (10 bytes)
// ============================================================================
// Single timed action within a show.
// Uses a simple byte array for data to avoid C++ union initialization issues.
// Access data via inline helper methods for type safety.

struct ShowCue {
    uint32_t timeMs;          // Execution time from show start
    CueType type;             // Action type
    uint8_t targetZone;       // Zone ID (ZONE_GLOBAL = all zones)
    uint8_t data[4];          // Cue-specific data (interpreted based on type)

    // Data layout by type:
    // CUE_EFFECT:          data[0]=effectId, data[1]=transitionType
    // CUE_PARAMETER_SWEEP: data[0]=paramId, data[1]=targetValue, data[2-3]=durationMs (little-endian)
    // CUE_ZONE_CONFIG:     data[0]=zoneCount, data[1]=enabledMask
    // CUE_PALETTE:         data[0]=paletteId
    // CUE_NARRATIVE:       data[0]=phase, data[1-2]=tempoMs (little-endian)
    // CUE_TRANSITION:      data[0]=transitionType, data[1-2]=durationMs (little-endian)

    // Accessors for CUE_EFFECT
    inline uint8_t effectId() const { return data[0]; }
    inline uint8_t effectTransition() const { return data[1]; }

    // Accessors for CUE_PARAMETER_SWEEP
    inline uint8_t sweepParamId() const { return data[0]; }
    inline uint8_t sweepTargetValue() const { return data[1]; }
    inline uint16_t sweepDurationMs() const { return data[2] | (data[3] << 8); }

    // Accessors for CUE_ZONE_CONFIG
    inline uint8_t zoneCount() const { return data[0]; }
    inline uint8_t zoneEnabled() const { return data[1]; }

    // Accessors for CUE_PALETTE
    inline uint8_t paletteId() const { return data[0]; }

    // Accessors for CUE_NARRATIVE
    inline uint8_t narrativePhase() const { return data[0]; }
    inline uint16_t narrativeTempoMs() const { return data[1] | (data[2] << 8); }

    // Accessors for CUE_TRANSITION
    inline uint8_t transitionType() const { return data[0]; }
    inline uint16_t transitionDurationMs() const { return data[1] | (data[2] << 8); }
};

// ============================================================================
// SHOW CHAPTER (20 bytes)
// ============================================================================
// Narrative chapter within a show (e.g., "Night Sky", "Sunrise")

struct ShowChapter {
    const char* name;             // PROGMEM string pointer
    uint32_t startTimeMs;         // When chapter starts
    uint32_t durationMs;          // Chapter duration
    uint8_t narrativePhase;       // ShowNarrativePhase for this chapter
    uint8_t tensionLevel;         // 0-255 (influences tempo/intensity)
    uint8_t cueStartIndex;        // First cue index for this chapter
    uint8_t cueCount;             // Number of cues in this chapter
};

// ============================================================================
// SHOW DEFINITION (Stored in PROGMEM)
// ============================================================================
// Complete show definition with chapters and cues

struct ShowDefinition {
    const char* id;               // Short identifier (e.g., "dawn")
    const char* name;             // Display name (e.g., "Dawn")
    uint32_t totalDurationMs;     // Total show length
    uint8_t chapterCount;         // Number of chapters
    uint8_t totalCues;            // Total cues across all chapters
    bool looping;                 // Whether show loops
    const ShowChapter* chapters;  // PROGMEM array of chapters
    const ShowCue* cues;          // PROGMEM array of all cues
};

// ============================================================================
// SHOW PLAYBACK STATE (20 bytes RAM)
// ============================================================================
// Current playback state for the active show

struct ShowPlaybackState {
    uint8_t currentShowId;        // Index of current show (0-9 for builtins)
    uint8_t currentChapterIndex;  // Current chapter
    uint8_t nextCueIndex;         // Next cue to execute
    bool playing;                 // Show is active
    bool paused;                  // Show is paused
    uint8_t _padding;             // Alignment
    uint32_t startTimeMs;         // millis() when show started
    uint32_t pauseStartMs;        // millis() when paused
    uint32_t totalPausedMs;       // Accumulated pause time

    // Reset to initial state
    void reset() {
        currentShowId = 0xFF;
        currentChapterIndex = 0;
        nextCueIndex = 0;
        playing = false;
        paused = false;
        startTimeMs = 0;
        pauseStartMs = 0;
        totalPausedMs = 0;
    }

    // Get elapsed time accounting for pauses
    uint32_t getElapsedMs() const {
        if (!playing) return 0;
        uint32_t now = millis();
        uint32_t elapsed = now - startTimeMs - totalPausedMs;
        if (paused) {
            elapsed -= (now - pauseStartMs);
        }
        return elapsed;
    }
};

// ============================================================================
// ACTIVE SWEEP (10 bytes)
// ============================================================================
// Single parameter interpolation in progress

struct ActiveSweep {
    uint8_t paramId;              // ParamId being swept
    uint8_t targetZone;           // Zone (ZONE_GLOBAL = all)
    uint8_t startValue;           // Value at sweep start
    uint8_t targetValue;          // Target value
    uint32_t startTimeMs;         // When sweep started
    uint16_t durationMs;          // Sweep duration

    bool isActive() const { return durationMs > 0; }

    void clear() {
        durationMs = 0;
    }

    // Get current interpolated value (0-255)
    uint8_t getCurrentValue(uint32_t currentMs) const {
        if (!isActive()) return startValue;

        uint32_t elapsed = currentMs - startTimeMs;
        if (elapsed >= durationMs) {
            return targetValue;
        }

        // Linear interpolation
        int32_t delta = (int32_t)targetValue - (int32_t)startValue;
        int32_t progress = (delta * (int32_t)elapsed) / (int32_t)durationMs;
        return (uint8_t)(startValue + progress);
    }

    // Check if sweep is complete
    bool isComplete(uint32_t currentMs) const {
        if (!isActive()) return true;
        return (currentMs - startTimeMs) >= durationMs;
    }
};

// ============================================================================
// SHOW INFO (for API responses)
// ============================================================================
// Lightweight struct for listing shows

struct ShowInfo {
    uint8_t id;
    const char* name;
    uint32_t durationMs;
    bool looping;
};

#endif // SHOW_TYPES_H
