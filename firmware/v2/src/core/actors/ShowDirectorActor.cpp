/**
 * @file ShowDirectorActor.cpp
 * @brief Implementation of the ShowDirectorActor
 *
 * LightwaveOS v2 - Show System
 */

#include "ShowDirectorActor.h"
#include "ActorSystem.h"
#include "RendererActor.h"
#include "../shows/BuiltinShows.h"
#include "../narrative/NarrativeEngine.h"
#include <cstring>

#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <esp_log.h>
#include <pgmspace.h>

static const char* TAG = "ShowDirector";
#endif

namespace lightwaveos {
namespace actors {

// ============================================================================
// Static Callbacks for ParameterSweeper
// ============================================================================

// Static instance pointer for callbacks
static ShowDirectorActor* s_instance = nullptr;

void ShowDirectorActor::applyParamValue(ParamId param, uint8_t zone, uint8_t value) {
    if (!s_instance) return;

    // Send message to RendererActor based on parameter type
    Message msg;
    switch (param) {
        case PARAM_BRIGHTNESS:
            msg = Message(MessageType::SET_BRIGHTNESS, value);
            break;
        case PARAM_SPEED:
            msg = Message(MessageType::SET_SPEED, value);
            break;
        case PARAM_INTENSITY:
            msg = Message(MessageType::SET_INTENSITY, value);
            break;
        case PARAM_SATURATION:
            msg = Message(MessageType::SET_SATURATION, value);
            break;
        case PARAM_COMPLEXITY:
            msg = Message(MessageType::SET_COMPLEXITY, value);
            break;
        case PARAM_VARIATION:
            msg = Message(MessageType::SET_VARIATION, value);
            break;
        default:
            return;
    }
    s_instance->sendToRenderer(msg);
}

uint8_t ShowDirectorActor::getParamValue(ParamId param, uint8_t zone) {
    if (!s_instance || !s_instance->m_rendererActor) {
        return 128;  // Default mid-value
    }

    // Get current value from RendererActor
    RendererActor* renderer = static_cast<RendererActor*>(s_instance->m_rendererActor);
    switch (param) {
        case PARAM_BRIGHTNESS:
            return renderer->getBrightness();
        case PARAM_SPEED:
            return renderer->getSpeed();
        case PARAM_INTENSITY:
            // Future: getIntensity() method
            return 128;
        case PARAM_SATURATION:
            // Future: getSaturation() method
            return 255;
        case PARAM_COMPLEXITY:
            // Future: getComplexity() method
            return 128;
        case PARAM_VARIATION:
            // Future: getVariation() method
            return 0;
        default:
            return 128;
    }
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

ShowDirectorActor::ShowDirectorActor()
    : Actor(ActorConfig(
          "ShowDirector",  // name
          4096,            // stackSize (16KB) - increased from 2048 due to PROGMEM copy operations
          2,               // priority
          0,                // coreId (Core 0)
          16,               // queueSize
          pdMS_TO_TICKS(50) // tickInterval (20Hz = 50ms)
      ))
    , m_currentShow(nullptr)
    , m_paramSweeper(applyParamValue, getParamValue)
    , m_rendererActor(nullptr)
    , m_narrative(nullptr)
{
    m_state.reset();
    s_instance = this;
}

ShowDirectorActor::~ShowDirectorActor() {
    s_instance = nullptr;
}

// ============================================================================
// Actor Lifecycle
// ============================================================================

void ShowDirectorActor::onStart() {
#ifndef NATIVE_BUILD
    ESP_LOGI(TAG, "ShowDirectorActor starting on Core %d", xPortGetCoreID());
#endif

    // Get RendererActor reference from ActorSystem
    m_rendererActor = ActorSystem::instance().getRenderer();
    if (!m_rendererActor) {
#ifndef NATIVE_BUILD
        ESP_LOGW(TAG, "RendererActor not available");
#endif
    }

    // Get NarrativeEngine reference
    m_narrative = &narrative::NarrativeEngine::getInstance();

#ifndef NATIVE_BUILD
    ESP_LOGI(TAG, "ShowDirectorActor ready");
#endif
}

void ShowDirectorActor::onMessage(const Message& msg) {
    switch (msg.type) {
        case MessageType::SHOW_LOAD:
            if (msg.param1 < BUILTIN_SHOW_COUNT) {
                loadShowById(msg.param1);
            }
            break;

        case MessageType::SHOW_START:
            startShow();
            break;

        case MessageType::SHOW_STOP:
            stopShow();
            break;

        case MessageType::SHOW_PAUSE:
            pauseShow();
            break;

        case MessageType::SHOW_RESUME:
            resumeShow();
            break;

        case MessageType::SHOW_SEEK:
            seekShow(msg.param4);  // param4 = timeMs
            break;

        case MessageType::SHOW_UNLOAD:
            unloadShow();
            break;

        case MessageType::SHUTDOWN:
            stopShow();
            break;

        default:
            // Ignore unknown messages
            break;
    }
}

void ShowDirectorActor::onTick() {
    // Update show playback (called at 20Hz)
    if (m_state.playing && !m_state.paused) {
        updateShow();
    }

    // Update parameter sweeps
    m_paramSweeper.update(millis());
}

void ShowDirectorActor::onStop() {
    stopShow();
    unloadShow();
}

// ============================================================================
// Show Control
// ============================================================================

bool ShowDirectorActor::loadShow(const ShowDefinition* show) {
    if (show == nullptr) {
        return false;
    }

    // Stop current show if any
    if (isPlaying() || isPaused()) {
        stopShow();
    }

    m_currentShow = show;
    m_state.reset();

    // Load cues into scheduler
    ShowDefinition showCopy;
    memcpy_P(&showCopy, show, sizeof(ShowDefinition));
    m_cueScheduler.loadCues(showCopy.cues, showCopy.totalCues);

#ifndef NATIVE_BUILD
    ESP_LOGI(TAG, "Show loaded: %s", showCopy.name);
#endif

    return true;
}

bool ShowDirectorActor::loadShowById(uint8_t showId) {
    if (showId >= BUILTIN_SHOW_COUNT) {
        return false;
    }

    m_state.currentShowId = showId;
    return loadShow(&BUILTIN_SHOWS[showId]);
}

void ShowDirectorActor::unloadShow() {
    stopShow();
    m_currentShow = nullptr;
    m_state.reset();
    m_cueScheduler.reset();
}

void ShowDirectorActor::startShow() {
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

    publishShowEvent(MessageType::SHOW_STARTED, m_state.currentShowId);

#ifndef NATIVE_BUILD
    ShowDefinition showCopy;
    memcpy_P(&showCopy, m_currentShow, sizeof(ShowDefinition));
    ESP_LOGI(TAG, "Show started: %s", showCopy.name);
#endif
}

void ShowDirectorActor::stopShow() {
    m_state.playing = false;
    m_state.paused = false;
    m_paramSweeper.cancelAll();

    publishShowEvent(MessageType::SHOW_STOPPED, m_state.currentShowId);

#ifndef NATIVE_BUILD
    ESP_LOGI(TAG, "Show stopped");
#endif
}

void ShowDirectorActor::pauseShow() {
    if (!m_state.playing || m_state.paused) {
        return;
    }

    m_state.paused = true;
    m_state.pauseStartMs = millis();

    publishShowEvent(MessageType::SHOW_PAUSED, m_state.currentShowId);

#ifndef NATIVE_BUILD
    ESP_LOGI(TAG, "Show paused");
#endif
}

void ShowDirectorActor::resumeShow() {
    if (!m_state.playing || !m_state.paused) {
        return;
    }

    m_state.paused = false;
    m_state.totalPausedMs += (millis() - m_state.pauseStartMs);
    m_state.pauseStartMs = 0;

    publishShowEvent(MessageType::SHOW_RESUMED, m_state.currentShowId);

#ifndef NATIVE_BUILD
    ESP_LOGI(TAG, "Show resumed");
#endif
}

void ShowDirectorActor::seekShow(uint32_t timeMs) {
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

#ifndef NATIVE_BUILD
    ESP_LOGI(TAG, "Show seeked to %lu ms", timeMs);
#endif
}

// ============================================================================
// Show Update
// ============================================================================

void ShowDirectorActor::updateShow() {
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

        publishShowEvent(MessageType::SHOW_CHAPTER_CHANGED, newChapter, m_state.currentShowId);

        // Trigger NarrativeEngine phase change on chapter transition
        ShowChapter chapter;
        memcpy_P(&chapter, &showCopy.chapters[newChapter], sizeof(ShowChapter));
        
        // Convert chapter phase to NarrativePhase
        using namespace lightwaveos::effects;
        NarrativePhase narrativePhase = PHASE_BUILD;
        if (chapter.narrativePhase == 0) narrativePhase = PHASE_BUILD;
        else if (chapter.narrativePhase == 1) narrativePhase = PHASE_HOLD;
        else if (chapter.narrativePhase == 2) narrativePhase = PHASE_RELEASE;
        else if (chapter.narrativePhase == 3) narrativePhase = PHASE_REST;
        
        // Use chapter duration or default
        uint32_t chapterDurationMs = chapter.durationMs > 0 ? chapter.durationMs : 15000;
        setNarrativePhase(narrativePhase, chapterDurationMs);
    }

    // Process ready cues
    uint8_t cueCount = m_cueScheduler.getReadyCues(elapsedMs, m_cueBuffer);
    for (uint8_t i = 0; i < cueCount; i++) {
        executeCue(m_cueBuffer[i]);
    }
}

void ShowDirectorActor::executeCue(const ShowCue& cue) {
    switch (cue.type) {
        case CUE_EFFECT: {
            uint8_t effectId = cue.effectId();
            uint8_t transitionType = cue.effectTransition();
            
            if (transitionType != 0 && m_rendererActor) {
                // Use transition - call RendererActor's startTransition method directly
                RendererActor* renderer = static_cast<RendererActor*>(m_rendererActor);
                renderer->startTransition(effectId, transitionType);
            } else {
                // Instant change
                Message msg(MessageType::SET_EFFECT, effectId);
                sendToRenderer(msg);
            }
            break;
        }

        case CUE_PARAMETER_SWEEP:
            m_paramSweeper.startSweepFromCurrent(
                static_cast<ParamId>(cue.sweepParamId()),
                cue.targetZone,
                cue.sweepTargetValue(),
                cue.sweepDurationMs()
            );
            break;

        case CUE_PALETTE: {
            Message msg(MessageType::SET_PALETTE, cue.paletteId());
            sendToRenderer(msg);
            break;
        }

        case CUE_NARRATIVE:
            modulateNarrative(cue.narrativePhase(), cue.narrativeTempoMs());
            break;

        case CUE_TRANSITION: {
            // CUE_TRANSITION triggers a transition without changing effect
            // For now, we'll skip this - RendererActor doesn't have a direct transition-only method
            // Future: Add TRIGGER_TRANSITION message type or method
            (void)cue;  // Unused for now
            break;
        }

        case CUE_ZONE_CONFIG:
            // Zone configuration would require ZoneComposer integration
            // For now, just log it
            break;

        case CUE_MARKER:
            // Markers are just sync points, no action needed
            break;
    }
}

void ShowDirectorActor::updateChapter(uint32_t elapsedMs) {
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

void ShowDirectorActor::handleShowEnd() {
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
        stopShow();
        publishShowEvent(MessageType::SHOW_COMPLETED, m_state.currentShowId);
    }
}

uint8_t ShowDirectorActor::getChapterForTime(uint32_t timeMs) const {
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

// ============================================================================
// Narrative Integration
// ============================================================================

void ShowDirectorActor::modulateNarrative(uint8_t phase, uint8_t tension) {
    if (!m_narrative) return;

    using namespace lightwaveos::effects;

    // Tension (0-255) maps to tempo (8000ms at 0 -> 2000ms at 255)
    // Higher tension = faster tempo
    // Convert milliseconds to seconds for setTempo()
    float tempoMs = 8000.0f - (tension / 255.0f) * 6000.0f;
    float tempoSeconds = tempoMs / 1000.0f;
    m_narrative->setTempo(tempoSeconds);

    // Convert chapter phase to NarrativePhase and set it
    NarrativePhase narrativePhase = PHASE_BUILD;
    if (phase == 0) narrativePhase = PHASE_BUILD;
    else if (phase == 1) narrativePhase = PHASE_HOLD;
    else if (phase == 2) narrativePhase = PHASE_RELEASE;
    else if (phase == 3) narrativePhase = PHASE_REST;
    
    // Convert tension (0-255) to duration (higher tension = shorter duration)
    // Tension 0 = 30s, tension 255 = 5s
    uint32_t durationMs = 30000 - (tension / 255.0f) * 25000;
    if (durationMs < 1000) durationMs = 1000;  // Minimum 1 second
    
    setNarrativePhase(narrativePhase, durationMs);
}

void ShowDirectorActor::setNarrativePhase(lightwaveos::effects::NarrativePhase phase, uint32_t durationMs) {
    if (!m_narrative) return;
    m_narrative->setPhase(phase, durationMs);
}

// ============================================================================
// Helper Methods
// ============================================================================

void ShowDirectorActor::sendToRenderer(const Message& msg) {
    if (m_rendererActor) {
        m_rendererActor->send(msg, pdMS_TO_TICKS(10));
    }
}

void ShowDirectorActor::publishShowEvent(MessageType eventType, uint8_t param1, uint8_t param2) {
    Message evt(eventType, param1, param2);
    bus::MessageBus::instance().publish(evt);
}

float ShowDirectorActor::getProgress() const {
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

uint32_t ShowDirectorActor::getRemainingMs() const {
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

} // namespace actors
} // namespace lightwaveos

