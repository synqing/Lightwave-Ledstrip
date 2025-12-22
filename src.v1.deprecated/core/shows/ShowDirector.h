#ifndef SHOW_DIRECTOR_H
#define SHOW_DIRECTOR_H

#include <Arduino.h>
#include <cJSON.h>
#include "ShowTypes.h"
#include "CueScheduler.h"
#include "ParameterSweeper.h"
#include "../NarrativeTension.h"  // For NarrativePhase enum

// ============================================================================
// SHOW DIRECTOR
// ============================================================================
// Main orchestrator for choreographed multi-minute light shows.
// Manages playback state, executes cues, and integrates with:
// - NarrativeEngine: Modulates intensity breathing cycle
// - TransitionEngine: Triggers visual transitions between effects
// - ParameterSweeper: Smooth parameter interpolations
//
// Usage:
//   ShowDirector::getInstance().loadShowById(0);  // Load "Dawn"
//   ShowDirector::getInstance().start();
//   // In main loop:
//   ShowDirector::getInstance().update();  // Call BEFORE effect render
//
// RAM: ~120 bytes (state + scheduler + sweeper references)
// ============================================================================

// Forward declarations
class NarrativeEngine;
class NarrativeTension;

class ShowDirector {
public:
    // Singleton access
    static ShowDirector& getInstance();

    // ========================================================================
    // SHOW LOADING
    // ========================================================================

    // Load a show by direct pointer (PROGMEM)
    bool loadShow(const ShowDefinition* show);

    // Load a show by ID (index into builtin shows)
    bool loadShowById(uint8_t showId);

    // Unload current show
    void unloadShow();

    // Check if a show is loaded
    bool hasShow() const { return m_currentShow != nullptr; }

    // ========================================================================
    // PLAYBACK CONTROL
    // ========================================================================

    // Start playback from beginning
    void start();

    // Stop playback and reset
    void stop();

    // Pause playback
    void pause();

    // Resume from pause
    void resume();

    // Seek to specific time position (ms)
    void seek(uint32_t timeMs);

    // ========================================================================
    // FRAME UPDATE
    // ========================================================================

    // Call once per frame, BEFORE effect rendering
    // Processes cues and parameter sweeps
    void update();

    // ========================================================================
    // STATE QUERIES
    // ========================================================================

    // Check if show is actively playing (not paused)
    bool isPlaying() const { return m_state.playing && !m_state.paused; }

    // Check if show is paused
    bool isPaused() const { return m_state.paused; }

    // Get progress as 0.0-1.0
    float getProgress() const;

    // Get current chapter index
    uint8_t getCurrentChapter() const { return m_state.currentChapterIndex; }

    // Get current chapter name (PROGMEM string)
    const char* getCurrentChapterName() const;

    // Get elapsed time in milliseconds
    uint32_t getElapsedMs() const { return m_state.getElapsedMs(); }
    
    // ========================================================================
    // NARRATIVE TENSION INTEGRATION (Phase 3.1)
    // ========================================================================
    
    /**
     * Set narrative phase with duration (integrates with NarrativeTension)
     * @param phase NarrativePhase enum
     * @param durationMs Phase duration in milliseconds
     */
    void setNarrativePhase(NarrativePhase phase, uint32_t durationMs);
    
    /**
     * Get current narrative tension value (0.0-1.0)
     * @return Tension value from NarrativeTension system
     */
    float getNarrativeTension() const;
    
    /**
     * Enable/disable tension modulation
     * @param enable true to enable, false to disable
     */
    void enableTensionModulation(bool enable);

    // Get remaining time in milliseconds
    uint32_t getRemainingMs() const;

    // Get current show ID
    uint8_t getCurrentShowId() const { return m_state.currentShowId; }

    // Get current show name (PROGMEM string)
    const char* getCurrentShowName() const;

    // Get current tension level (0-255)
    uint8_t getCurrentTension() const;

    // ========================================================================
    // API SUPPORT
    // ========================================================================

    // Populate JSON object with current status
    void getStatus(cJSON* doc) const;

    // Get list of available shows (populates "shows" array in the object)
    static void getShowList(cJSON* doc);

    // Get number of builtin shows
    static uint8_t getShowCount();

private:
    // Private constructor for singleton
    ShowDirector();

    // Singleton instance
    static ShowDirector* s_instance;

    // Current show (PROGMEM pointer)
    const ShowDefinition* m_currentShow;

    // Playback state
    ShowPlaybackState m_state;

    // Cue scheduler
    CueScheduler m_cueScheduler;

    // Parameter sweeper
    ParameterSweeper m_paramSweeper;

    // Cue buffer for getReadyCues
    ShowCue m_cueBuffer[CueScheduler::MAX_CUES_PER_FRAME];

    // ========================================================================
    // INTERNAL METHODS
    // ========================================================================

    // Execute a single cue
    void executeCue(const ShowCue& cue);

    // Update current chapter based on elapsed time
    void updateChapter(uint32_t elapsedMs);

    // Modulate NarrativeEngine based on chapter settings
    void modulateNarrative(uint8_t phase, uint8_t tension);

    // Handle show completion (stop or loop)
    void handleShowEnd();

    // Get chapter for a given time
    uint8_t getChapterForTime(uint32_t timeMs) const;
};

#endif // SHOW_DIRECTOR_H
