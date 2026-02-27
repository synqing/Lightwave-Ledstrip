/**
 * @file ShowDirectorActor.h
 * @brief Actor responsible for orchestrating choreographed light shows
 *
 * LightwaveOS v2 - Show System
 *
 * The ShowDirectorActor manages multi-minute light shows with:
 * - Timed cues (effect changes, parameter sweeps, transitions)
 * - Chapter-based narrative structure
 * - Integration with NarrativeEngine for tension modulation
 * - Message-based communication with RendererActor
 *
 * Architecture:
 *   Commands (from other actors):
 *     SHOW_LOAD, SHOW_START, SHOW_STOP, SHOW_PAUSE, SHOW_RESUME, SHOW_SEEK
 *
 *   Events (published to MessageBus):
 *     SHOW_STARTED, SHOW_STOPPED, SHOW_PAUSED, SHOW_RESUMED, SHOW_CHAPTER_CHANGED
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "Actor.h"
#include "../bus/MessageBus.h"
#include "../shows/ShowTypes.h"
#include "../shows/CueScheduler.h"
#include "../shows/ParameterSweeper.h"
#include "../narrative/NarrativeEngine.h"
#include "../../config/features.h"

// Forward declarations
struct ShowDefinition;

namespace lightwaveos {
namespace actors {

/**
 * @brief Actor responsible for show orchestration
 *
 * Runs on Core 0 at priority 2 (background processing).
 * Updates at 20Hz (50ms tick interval) to process cues and parameter sweeps.
 */
class ShowDirectorActor : public Actor {
public:
    /**
     * @brief Construct the ShowDirectorActor
     */
    ShowDirectorActor();

    /**
     * @brief Destructor
     */
    ~ShowDirectorActor() override;

    // ========================================================================
    // State Accessors (read-only, for diagnostics)
    // ========================================================================

    bool hasShow() const { return m_currentShow != nullptr; }
    bool isPlaying() const { return m_state.playing && !m_state.paused; }
    bool isPaused() const { return m_state.paused; }
    float getProgress() const;
    uint8_t getCurrentChapter() const { return m_state.currentChapterIndex; }
    uint8_t getCurrentShowId() const { return m_state.currentShowId; }
    uint32_t getElapsedMs() const { return m_state.getElapsedMs(); }
    uint32_t getRemainingMs() const;

protected:
    // ========================================================================
    // Actor Lifecycle
    // ========================================================================

    void onStart() override;
    void onMessage(const Message& msg) override;
    void onTick() override;
    void onStop() override;

private:
    // ========================================================================
    // Show Control
    // ========================================================================

    bool loadShow(const ShowDefinition* show);
    bool loadShowById(uint8_t showId);
    void unloadShow();
    void startShow();
    void stopShow();
    void pauseShow();
    void resumeShow();
    void seekShow(uint32_t timeMs);

    // ========================================================================
    // Show Update
    // ========================================================================

    void updateShow();
    void executeCue(const ShowCue& cue);
    void updateChapter(uint32_t elapsedMs);
    void handleShowEnd();
    uint8_t getChapterForTime(uint32_t timeMs) const;

    // ========================================================================
    // Narrative Integration
    // ========================================================================

    void modulateNarrative(uint8_t phase, uint8_t tension);
    void setNarrativePhase(lightwaveos::effects::NarrativePhase phase, uint32_t durationMs);

#if FEATURE_AUDIO_SYNC
    // ========================================================================
    // Trinity Semantic Bridge (PRISM/Trinity → ShowDirector)
    // ========================================================================

    /**
     * @brief Apply semantic segment changes (trinity.segment) when no show is playing.
     *
     * Segment events are published by RendererActor on MessageBus when the host
     * updates the current PRISM/Trinity structural segment.
     *
     * The bridge maps segment labels (hashed) into:
     * - NarrativeEngine phase + tempo
     * - Smooth parameter sweeps (brightness/speed) via ParameterSweeper
     *
     * This provides a lightweight semantic → parameter pipeline without any
     * heap allocation in render paths.
     */
    void handleTrinitySegment(const Message& msg);

    uint8_t m_lastTrinitySegmentIndex = 0xFF;
    uint16_t m_lastTrinitySegmentLabelHash = 0;
#endif

    // ========================================================================
    // Parameter Sweeper Callbacks
    // ========================================================================

    static void applyParamValue(ParamId param, uint8_t zone, uint8_t value);
    static uint8_t getParamValue(ParamId param, uint8_t zone);

    // ========================================================================
    // Message Sending Helpers
    // ========================================================================

    void sendToRenderer(const Message& msg);
    void publishShowEvent(MessageType eventType, uint8_t param1 = 0, uint8_t param2 = 0);

    // ========================================================================
    // Member Variables
    // ========================================================================

    const ShowDefinition* m_currentShow;  // PROGMEM pointer
    ShowPlaybackState m_state;
    shows::CueScheduler m_cueScheduler;
    shows::ParameterSweeper m_paramSweeper;
    ShowCue m_cueBuffer[shows::CueScheduler::MAX_CUES_PER_FRAME];

    // Reference to RendererActor for sending commands
    Actor* m_rendererActor;

    // NarrativeEngine reference
    narrative::NarrativeEngine* m_narrative;
};

} // namespace actors
} // namespace lightwaveos
