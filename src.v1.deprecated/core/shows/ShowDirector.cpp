#include "ShowDirector.h"
#include "BuiltinShows.h"
#include "../NarrativeEngine.h"
#if FEATURE_NARRATIVE_TENSION
#include "../NarrativeTension.h"
#endif
#include "../../effects/transitions/TransitionEngine.h"
#include "../../config/features.h"

// External globals (defined in main.cpp)
extern int currentEffect;
extern uint8_t currentPalette;
extern TransitionEngine transitionEngine;
extern CRGB leds[];
extern CRGB transitionBuffer[];

// ============================================================================
// SHOW DIRECTOR IMPLEMENTATION
// ============================================================================

// Singleton instance
ShowDirector* ShowDirector::s_instance = nullptr;

ShowDirector& ShowDirector::getInstance() {
    if (s_instance == nullptr) {
        s_instance = new ShowDirector();
    }
    return *s_instance;
}

ShowDirector::ShowDirector()
    : m_currentShow(nullptr) {
    m_state.reset();
}

// ============================================================================
// SHOW LOADING
// ============================================================================

bool ShowDirector::loadShow(const ShowDefinition* show) {
    if (show == nullptr) {
        return false;
    }

    // Stop current show if any
    if (isPlaying() || isPaused()) {
        stop();
    }

    m_currentShow = show;
    m_state.reset();

    // Load cues into scheduler
    ShowDefinition showCopy;
    memcpy_P(&showCopy, show, sizeof(ShowDefinition));
    m_cueScheduler.loadCues(showCopy.cues, showCopy.totalCues);

    return true;
}

bool ShowDirector::loadShowById(uint8_t showId) {
    if (showId >= BUILTIN_SHOW_COUNT) {
        return false;
    }

    m_state.currentShowId = showId;
    return loadShow(&BUILTIN_SHOWS[showId]);
}

void ShowDirector::unloadShow() {
    stop();
    m_currentShow = nullptr;
    m_state.reset();
}

// ============================================================================
// PLAYBACK CONTROL
// ============================================================================

void ShowDirector::start() {
    if (m_currentShow == nullptr) {
        return;
    }

    m_state.playing = true;
    m_state.paused = false;
    m_state.startTimeMs = millis();
    m_state.pauseStartMs = 0;
    m_state.totalPausedMs = 0;
    m_state.currentChapterIndex = 0;
    m_state.nextCueIndex = 0;

    m_cueScheduler.reset();
    m_paramSweeper.cancelAll();

    // Apply initial chapter settings
    updateChapter(0);
}

void ShowDirector::stop() {
    m_state.playing = false;
    m_state.paused = false;
    m_paramSweeper.cancelAll();
}

void ShowDirector::pause() {
    if (!m_state.playing || m_state.paused) {
        return;
    }

    m_state.paused = true;
    m_state.pauseStartMs = millis();
}

void ShowDirector::resume() {
    if (!m_state.playing || !m_state.paused) {
        return;
    }

    m_state.paused = false;
    m_state.totalPausedMs += (millis() - m_state.pauseStartMs);
    m_state.pauseStartMs = 0;
}

void ShowDirector::seek(uint32_t timeMs) {
    if (m_currentShow == nullptr) {
        return;
    }

    // Read show definition
    ShowDefinition showCopy;
    memcpy_P(&showCopy, m_currentShow, sizeof(ShowDefinition));

    // Clamp to show duration
    if (timeMs >= showCopy.totalDurationMs) {
        timeMs = showCopy.totalDurationMs - 1;
    }

    // Update timing
    m_state.startTimeMs = millis() - timeMs;
    m_state.totalPausedMs = 0;
    if (m_state.paused) {
        m_state.pauseStartMs = millis();
    }

    // Seek cue scheduler
    m_cueScheduler.seekTo(timeMs);

    // Update chapter
    m_state.currentChapterIndex = getChapterForTime(timeMs);
    updateChapter(timeMs);

    // Cancel active sweeps (they're time-based and won't be accurate after seek)
    m_paramSweeper.cancelAll();
}

// ============================================================================
// FRAME UPDATE
// ============================================================================

void ShowDirector::update() {
    if (!m_state.playing || m_state.paused) {
        return;
    }

    uint32_t elapsedMs = m_state.getElapsedMs();

    // Read show definition
    ShowDefinition showCopy;
    memcpy_P(&showCopy, m_currentShow, sizeof(ShowDefinition));

    // Check for show end
    if (elapsedMs >= showCopy.totalDurationMs) {
        handleShowEnd();
        return;
    }

    // Update chapter if needed
    uint8_t newChapter = getChapterForTime(elapsedMs);
    if (newChapter != m_state.currentChapterIndex) {
        m_state.currentChapterIndex = newChapter;
        updateChapter(elapsedMs);
        
        // Trigger NarrativeTension phase change on chapter transition
#if FEATURE_NARRATIVE_TENSION
        ShowChapter chapter;
        memcpy_P(&chapter, &showCopy.chapters[newChapter], sizeof(ShowChapter));
        // Convert chapter phase to NarrativePhase and set it
        NarrativePhase narrativePhase = PHASE_BUILD;
        if (chapter.narrativePhase == 0) narrativePhase = PHASE_BUILD;
        else if (chapter.narrativePhase == 1) narrativePhase = PHASE_HOLD;
        else if (chapter.narrativePhase == 2) narrativePhase = PHASE_RELEASE;
        else if (chapter.narrativePhase == 3) narrativePhase = PHASE_REST;
        
        // Use chapter duration or default
        uint32_t chapterDurationMs = chapter.durationMs > 0 ? chapter.durationMs : 15000;
        setNarrativePhase(narrativePhase, chapterDurationMs);
#endif
    }

    // Process ready cues
    uint8_t cueCount = m_cueScheduler.getReadyCues(elapsedMs, m_cueBuffer);
    for (uint8_t i = 0; i < cueCount; i++) {
        executeCue(m_cueBuffer[i]);
    }

    // Update parameter sweeps
    m_paramSweeper.update(millis());
}

// ============================================================================
// STATE QUERIES
// ============================================================================

float ShowDirector::getProgress() const {
    if (m_currentShow == nullptr || !m_state.playing) {
        return 0.0f;
    }

    ShowDefinition showCopy;
    memcpy_P(&showCopy, m_currentShow, sizeof(ShowDefinition));

    if (showCopy.totalDurationMs == 0) {
        return 0.0f;
    }

    uint32_t elapsed = m_state.getElapsedMs();
    return (float)elapsed / (float)showCopy.totalDurationMs;
}

const char* ShowDirector::getCurrentChapterName() const {
    if (m_currentShow == nullptr) {
        return nullptr;
    }

    ShowDefinition showCopy;
    memcpy_P(&showCopy, m_currentShow, sizeof(ShowDefinition));

    if (m_state.currentChapterIndex >= showCopy.chapterCount) {
        return nullptr;
    }

    ShowChapter chapter;
    memcpy_P(&chapter, &showCopy.chapters[m_state.currentChapterIndex], sizeof(ShowChapter));
    return chapter.name;
}

uint32_t ShowDirector::getRemainingMs() const {
    if (m_currentShow == nullptr || !m_state.playing) {
        return 0;
    }

    ShowDefinition showCopy;
    memcpy_P(&showCopy, m_currentShow, sizeof(ShowDefinition));

    uint32_t elapsed = m_state.getElapsedMs();
    if (elapsed >= showCopy.totalDurationMs) {
        return 0;
    }

    return showCopy.totalDurationMs - elapsed;
}

const char* ShowDirector::getCurrentShowName() const {
    if (m_currentShow == nullptr) {
        return nullptr;
    }

    ShowDefinition showCopy;
    memcpy_P(&showCopy, m_currentShow, sizeof(ShowDefinition));
    return showCopy.name;
}

uint8_t ShowDirector::getCurrentTension() const {
    if (m_currentShow == nullptr) {
        return 0;
    }

    ShowDefinition showCopy;
    memcpy_P(&showCopy, m_currentShow, sizeof(ShowDefinition));

    if (m_state.currentChapterIndex >= showCopy.chapterCount) {
        return 0;
    }

    ShowChapter chapter;
    memcpy_P(&chapter, &showCopy.chapters[m_state.currentChapterIndex], sizeof(ShowChapter));
    return chapter.tensionLevel;
}

// ============================================================================
// API SUPPORT
// ============================================================================

void ShowDirector::getStatus(cJSON* doc) const {
    if (doc == nullptr) return;

    cJSON_AddBoolToObject(doc, "playing", m_state.playing);
    cJSON_AddBoolToObject(doc, "paused", m_state.paused);

    if (m_currentShow != nullptr) {
        ShowDefinition showCopy;
        memcpy_P(&showCopy, m_currentShow, sizeof(ShowDefinition));

        cJSON_AddNumberToObject(doc, "showId", m_state.currentShowId);
        cJSON_AddStringToObject(doc, "showName", showCopy.name);
        cJSON_AddNumberToObject(doc, "duration", showCopy.totalDurationMs);
        cJSON_AddNumberToObject(doc, "elapsed", m_state.getElapsedMs());
        cJSON_AddNumberToObject(doc, "remaining", getRemainingMs());
        cJSON_AddNumberToObject(doc, "progress", getProgress());
        cJSON_AddNumberToObject(doc, "chapter", m_state.currentChapterIndex);
        cJSON_AddStringToObject(doc, "chapterName", getCurrentChapterName());
        cJSON_AddNumberToObject(doc, "tension", getCurrentTension());
        cJSON_AddBoolToObject(doc, "looping", showCopy.looping);
    } else {
        cJSON_AddNumberToObject(doc, "showId", -1);
        cJSON_AddNullToObject(doc, "showName");
    }
}

void ShowDirector::getShowList(cJSON* doc) {
    if (doc == nullptr) return;

    cJSON* shows = cJSON_AddArrayToObject(doc, "shows");
    for (uint8_t i = 0; i < BUILTIN_SHOW_COUNT; i++) {
        ShowDefinition showCopy;
        memcpy_P(&showCopy, &BUILTIN_SHOWS[i], sizeof(ShowDefinition));

        cJSON* show = cJSON_CreateObject();
        cJSON_AddNumberToObject(show, "id", i);
        cJSON_AddStringToObject(show, "name", showCopy.name);
        cJSON_AddNumberToObject(show, "duration", showCopy.totalDurationMs);
        cJSON_AddBoolToObject(show, "looping", showCopy.looping);
        cJSON_AddItemToArray(shows, show);
    }
}

uint8_t ShowDirector::getShowCount() {
    return BUILTIN_SHOW_COUNT;
}

// ============================================================================
// INTERNAL METHODS
// ============================================================================

void ShowDirector::executeCue(const ShowCue& cue) {
    switch (cue.type) {
        case CUE_EFFECT:
            if (cue.effectTransition() != 0) {
                // Use transition engine
                // Copy current state to transition buffer
                memcpy(transitionBuffer, leds, sizeof(CRGB) * 320);

                // Start transition
                transitionEngine.startTransition(
                    transitionBuffer,
                    leds,
                    leds,
                    static_cast<TransitionType>(cue.effectTransition()),
                    1000,  // 1 second transition
                    EASE_IN_OUT_QUAD
                );

                // Set new effect (will render to target)
                currentEffect = cue.effectId();
            } else {
                // Instant change
                currentEffect = cue.effectId();
            }
            break;

        case CUE_PARAMETER_SWEEP:
            m_paramSweeper.startSweepFromCurrent(
                static_cast<ParamId>(cue.sweepParamId()),
                cue.targetZone,
                cue.sweepTargetValue(),
                cue.sweepDurationMs()
            );
            break;

        case CUE_PALETTE:
            currentPalette = cue.paletteId();
            break;

        case CUE_NARRATIVE:
            modulateNarrative(cue.narrativePhase(), cue.narrativeTempoMs());
            break;

        case CUE_TRANSITION:
            // Trigger a transition without changing effect
            transitionEngine.startTransition(
                leds,
                leds,
                leds,
                static_cast<TransitionType>(cue.transitionType()),
                cue.transitionDurationMs(),
                EASE_IN_OUT_QUAD
            );
            break;

        case CUE_ZONE_CONFIG:
            // Zone configuration would require ZoneComposer integration
            // For now, just log it
            break;

        case CUE_MARKER:
            // Markers are just sync points, no action needed
            break;
    }
}

void ShowDirector::updateChapter(uint32_t elapsedMs) {
    (void)elapsedMs;  // May be used for interpolation later

    if (m_currentShow == nullptr) {
        return;
    }

    ShowDefinition showCopy;
    memcpy_P(&showCopy, m_currentShow, sizeof(ShowDefinition));

    if (m_state.currentChapterIndex >= showCopy.chapterCount) {
        return;
    }

    ShowChapter chapter;
    memcpy_P(&chapter, &showCopy.chapters[m_state.currentChapterIndex], sizeof(ShowChapter));

    // Modulate narrative engine
    modulateNarrative(chapter.narrativePhase, chapter.tensionLevel);
}

void ShowDirector::modulateNarrative(uint8_t phase, uint8_t tension) {
#if FEATURE_NARRATIVE_ENGINE
    NarrativeEngine& narrative = NarrativeEngine::getInstance();

    // Tension (0-255) maps to tempo (8000ms at 0 -> 2000ms at 255)
    // Higher tension = faster tempo
    float tempo = 8000.0f - (tension / 255.0f) * 6000.0f;
    narrative.setTempo(tempo);

    // Phase could be used to force NarrativeEngine phase if needed
    // For now, we let NarrativeEngine run its own cycle but at the tempo we set
    (void)phase;
#else
    (void)phase;
    (void)tension;
#endif

#if FEATURE_NARRATIVE_TENSION
    // Integrate with NarrativeTension system (Phase 3.1)
    NarrativeTension& narrativeTension = NarrativeTension::getInstance();
    
    // Convert chapter phase (uint8_t) to NarrativePhase enum
    NarrativePhase narrativePhase = PHASE_BUILD;
    if (phase == 0) narrativePhase = PHASE_BUILD;
    else if (phase == 1) narrativePhase = PHASE_HOLD;
    else if (phase == 2) narrativePhase = PHASE_RELEASE;
    else if (phase == 3) narrativePhase = PHASE_REST;
    
    // Convert tension (0-255) to duration (higher tension = shorter duration for faster pacing)
    // Tension 0 = 30s, tension 255 = 5s
    uint32_t durationMs = 30000 - (tension / 255.0f) * 25000;
    if (durationMs < 1000) durationMs = 1000;  // Minimum 1 second
    
    narrativeTension.setPhase(narrativePhase, durationMs);
#endif
}

void ShowDirector::setNarrativePhase(NarrativePhase phase, uint32_t durationMs) {
#if FEATURE_NARRATIVE_TENSION
    NarrativeTension& narrativeTension = NarrativeTension::getInstance();
    narrativeTension.setPhase(phase, durationMs);
#else
    (void)phase;
    (void)durationMs;
#endif
}

float ShowDirector::getNarrativeTension() const {
#if FEATURE_NARRATIVE_TENSION
    const NarrativeTension& narrativeTension = NarrativeTension::getInstance();
    return narrativeTension.getTension();
#else
    return 0.0f;
#endif
}

void ShowDirector::enableTensionModulation(bool enable) {
#if FEATURE_NARRATIVE_TENSION
    NarrativeTension& narrativeTension = NarrativeTension::getInstance();
    narrativeTension.setEnabled(enable);
#else
    (void)enable;
#endif
}

void ShowDirector::handleShowEnd() {
    ShowDefinition showCopy;
    memcpy_P(&showCopy, m_currentShow, sizeof(ShowDefinition));

    if (showCopy.looping) {
        // Reset and restart
        m_state.startTimeMs = millis();
        m_state.totalPausedMs = 0;
        m_state.currentChapterIndex = 0;
        m_cueScheduler.reset();
        m_paramSweeper.cancelAll();
        updateChapter(0);
    } else {
        // Stop the show
        stop();
    }
}

uint8_t ShowDirector::getChapterForTime(uint32_t timeMs) const {
    if (m_currentShow == nullptr) {
        return 0;
    }

    ShowDefinition showCopy;
    memcpy_P(&showCopy, m_currentShow, sizeof(ShowDefinition));

    for (uint8_t i = 0; i < showCopy.chapterCount; i++) {
        ShowChapter chapter;
        memcpy_P(&chapter, &showCopy.chapters[i], sizeof(ShowChapter));

        if (timeMs >= chapter.startTimeMs &&
            timeMs < chapter.startTimeMs + chapter.durationMs) {
            return i;
        }
    }

    // Return last chapter if past end
    return showCopy.chapterCount > 0 ? showCopy.chapterCount - 1 : 0;
}
