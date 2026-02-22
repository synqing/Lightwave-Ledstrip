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
#include "../../config/effect_ids.h"
#include <cstring>

#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <esp_log.h>
#include <pgmspace.h>

static const char* TAG = "ShowDirector";
#endif

namespace lightwaveos {
namespace actors {

#if FEATURE_AUDIO_SYNC
namespace {

struct TrinitySegmentIntent {
    bool known = false;
    uint8_t showPhase = SHOW_PHASE_BUILD;  // 0-3 (ShowNarrativePhase)
    uint8_t tension = 128;                // 0-255
    uint8_t brightness = 96;              // 0-255 (Renderer clamps to MAX_BRIGHTNESS)
    uint8_t speed = 10;                   // 1-255 (Renderer clamps to MAX_SPEED)
    uint8_t mood = 160;                   // 0-255 (0=reactive, 255=smooth)
    uint16_t sweepMs = 500;
};

static uint16_t hashLabel16(const char* label) {
    // 32-bit FNV-1a, folded to 16-bit for compact comparisons.
    uint32_t hash = 2166136261u;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(label);
    while (*p) {
        hash ^= *p++;
        hash *= 16777619u;
    }
    return static_cast<uint16_t>((hash & 0xFFFFu) ^ (hash >> 16));
}

static TrinitySegmentIntent intentForLabelHash(uint16_t labelHash16) {
    const uint16_t h_start = hashLabel16("start");
    const uint16_t h_intro = hashLabel16("intro");
    const uint16_t h_verse = hashLabel16("verse");
    const uint16_t h_chorus = hashLabel16("chorus");
    const uint16_t h_solo = hashLabel16("solo");
    const uint16_t h_inst = hashLabel16("inst");
    const uint16_t h_bridge = hashLabel16("bridge");
    const uint16_t h_breakdown = hashLabel16("breakdown");
    const uint16_t h_drop = hashLabel16("drop");
    const uint16_t h_end = hashLabel16("end");

    TrinitySegmentIntent i;

    if (labelHash16 == h_start) {
        i.known = true;
        i.showPhase = SHOW_PHASE_REST;
        i.tension = 30;
        i.brightness = 60;
        i.speed = 8;
        i.mood = 210;
        i.sweepMs = 800;
        return i;
    }
    if (labelHash16 == h_intro) {
        i.known = true;
        i.showPhase = SHOW_PHASE_BUILD;
        i.tension = 70;
        i.brightness = 80;
        i.speed = 12;
        i.mood = 190;
        i.sweepMs = 700;
        return i;
    }
    if (labelHash16 == h_verse) {
        i.known = true;
        i.showPhase = SHOW_PHASE_BUILD;
        i.tension = 110;
        i.brightness = 95;
        i.speed = 18;
        i.mood = 170;
        i.sweepMs = 600;
        return i;
    }
    if (labelHash16 == h_chorus) {
        i.known = true;
        i.showPhase = SHOW_PHASE_HOLD;
        i.tension = 200;
        i.brightness = 150;
        i.speed = 60;
        i.mood = 70;
        i.sweepMs = 350;
        return i;
    }
    if (labelHash16 == h_drop) {
        i.known = true;
        i.showPhase = SHOW_PHASE_HOLD;
        i.tension = 230;
        i.brightness = 160;
        i.speed = 80;
        i.mood = 40;
        i.sweepMs = 250;
        return i;
    }
    if (labelHash16 == h_solo) {
        i.known = true;
        i.showPhase = SHOW_PHASE_HOLD;
        i.tension = 170;
        i.brightness = 135;
        i.speed = 55;
        i.mood = 110;
        i.sweepMs = 400;
        return i;
    }
    if (labelHash16 == h_bridge) {
        i.known = true;
        i.showPhase = SHOW_PHASE_RELEASE;
        i.tension = 140;
        i.brightness = 110;
        i.speed = 35;
        i.mood = 150;
        i.sweepMs = 650;
        return i;
    }
    if (labelHash16 == h_breakdown || labelHash16 == h_inst) {
        i.known = true;
        i.showPhase = SHOW_PHASE_REST;
        i.tension = 90;
        i.brightness = 85;
        i.speed = 22;
        i.mood = 185;
        i.sweepMs = 700;
        return i;
    }
    if (labelHash16 == h_end) {
        i.known = true;
        i.showPhase = SHOW_PHASE_REST;
        i.tension = 0;
        i.brightness = 0;
        i.speed = 10;
        i.mood = 235;
        i.sweepMs = 1500;
        return i;
    }

    return i;
}

static lightwaveos::effects::NarrativePhase toNarrativePhase(uint8_t showPhase) {
    using namespace lightwaveos::effects;
    switch (showPhase) {
        case SHOW_PHASE_BUILD: return PHASE_BUILD;
        case SHOW_PHASE_HOLD: return PHASE_HOLD;
        case SHOW_PHASE_RELEASE: return PHASE_RELEASE;
        case SHOW_PHASE_REST: return PHASE_REST;
        default: return PHASE_BUILD;
    }
}

} // namespace
#endif

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
          3072,            // stackSize (12KB) - reduced for memory constraints
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

#if FEATURE_AUDIO_SYNC
    // Subscribe to Trinity semantic segment events (published by RendererActor).
    bus::MessageBus::instance().subscribe(MessageType::TRINITY_SEGMENT, this);
#endif

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

#if FEATURE_AUDIO_SYNC
        case MessageType::TRINITY_SEGMENT:
            handleTrinitySegment(msg);
            break;
#endif

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
#if FEATURE_AUDIO_SYNC
    bus::MessageBus::instance().unsubscribeAll(this);
#endif
    stopShow();
    unloadShow();
}

#if FEATURE_AUDIO_SYNC
void ShowDirectorActor::handleTrinitySegment(const Message& msg) {
    // Do not interfere with choreographed show playback.
    if (m_state.playing) {
        return;
    }

    uint8_t index = msg.param1;
    uint16_t labelHash16 = (static_cast<uint16_t>(msg.param2) << 8) | static_cast<uint16_t>(msg.param3);
    uint32_t startMs = msg.param4;
    uint32_t endMs = msg._reserved;

    // Belt-and-braces de-dupe (RendererActor already only publishes on change).
    if (index == m_lastTrinitySegmentIndex && labelHash16 == m_lastTrinitySegmentLabelHash) {
        return;
    }
    m_lastTrinitySegmentIndex = index;
    m_lastTrinitySegmentLabelHash = labelHash16;

    TrinitySegmentIntent intent = intentForLabelHash(labelHash16);
    if (!intent.known || !m_narrative) {
        return;
    }

    uint32_t segDurationMs = (endMs > startMs) ? (endMs - startMs) : 0;

    // Tempo mapping matches show chapter logic: higher tension -> faster tempo.
    float tempoMs = 8000.0f - (intent.tension / 255.0f) * 6000.0f;
    float tempoSeconds = tempoMs / 1000.0f;
    m_narrative->setTempo(tempoSeconds);

    // Prefer using the PRISM segment duration for phase timing (clamped).
    uint32_t phaseDurationMs = segDurationMs;
    if (phaseDurationMs < 1000) {
        phaseDurationMs = 30000 - (intent.tension / 255.0f) * 25000;
        if (phaseDurationMs < 1000) phaseDurationMs = 1000;
    }
    if (phaseDurationMs > 60000) phaseDurationMs = 60000;

    setNarrativePhase(toNarrativePhase(intent.showPhase), phaseDurationMs);

    uint16_t sweepMs = intent.sweepMs;
    if (segDurationMs > 0 && sweepMs > segDurationMs) {
        sweepMs = static_cast<uint16_t>(segDurationMs);
    }

    // Smooth global parameter shifts.
    m_paramSweeper.startSweepFromCurrent(PARAM_BRIGHTNESS, ZONE_GLOBAL, intent.brightness, sweepMs);
    m_paramSweeper.startSweepFromCurrent(PARAM_SPEED, ZONE_GLOBAL, intent.speed, sweepMs);

    // Mood is not a sweepable show parameter; apply immediately.
    sendToRenderer(Message(MessageType::SET_MOOD, intent.mood));

#ifndef NATIVE_BUILD
    ESP_LOGI(TAG,
             "Trinity segment intent: idx=%u phase=%u tension=%u bright=%u speed=%u mood=%u dur=%lums sweep=%ums",
             static_cast<unsigned>(index),
             static_cast<unsigned>(intent.showPhase),
             static_cast<unsigned>(intent.tension),
             static_cast<unsigned>(intent.brightness),
             static_cast<unsigned>(intent.speed),
             static_cast<unsigned>(intent.mood),
             static_cast<unsigned long>(phaseDurationMs),
             static_cast<unsigned>(sweepMs));
#endif
}
#endif

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
            EffectId effectId = cue.effectId();
            uint8_t transitionType = cue.effectTransition();
            
            if (transitionType != 0 && m_rendererActor) {
                // Use transition - call RendererActor's startTransition method directly
                RendererActor* renderer = static_cast<RendererActor*>(m_rendererActor);
                renderer->startTransition(effectId, transitionType);
            } else {
                // Instant change - pack EffectId as 2 bytes (little-endian)
                Message msg(MessageType::SET_EFFECT,
                            static_cast<uint8_t>(effectId & 0xFF),
                            static_cast<uint8_t>((effectId >> 8) & 0xFF));
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
