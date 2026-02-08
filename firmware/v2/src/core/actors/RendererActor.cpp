/**
 * @file RendererActor.cpp
 * @brief Implementation of the RendererActor
 *
 * The RendererActor runs the main render loop at 120 FPS on Core 1.
 * It processes incoming commands (effect changes, brightness, etc.)
 * between frames and publishes FRAME_RENDERED events.
 *
 * Performance notes:
 * - Frame budget: 8.33ms (120 FPS)
 * - Typical render: 2-4ms (effect dependent)
 * - LED driver show(): ~2ms for 320 LEDs
 * - Remaining budget for message processing: ~2-4ms
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "RendererActor.h"
#include "../../effects/zones/ZoneComposer.h"
#if FEATURE_TRANSITIONS
#include "../../effects/transitions/TransitionEngine.h"
#endif
#include "../../effects/PatternRegistry.h"
#include "../../palettes/Palettes_Master.h"
#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../plugins/runtime/LegacyEffectAdapter.h"
#include <math.h>
#if FEATURE_VALIDATION_PROFILING
#include "../../core/system/ValidationProfiler.h"
#endif
#ifndef NATIVE_BUILD
#include "esp_rom_sys.h"
#include <esp_task_wdt.h>
#include <esp_heap_caps.h>
#endif

// Audio integration (Phase 2)
#if FEATURE_AUDIO_SYNC
#include "../../audio/AudioActor.h"
#if !FEATURE_AUDIO_BACKEND_ESV11
// TempoTracker integration (replaces K1)
#include "../../audio/tempo/TempoTracker.h"
#endif
#endif

#if FEATURE_TRANSITIONS
using namespace lightwaveos::transitions;
#endif
using namespace lightwaveos::palettes;

namespace {
constexpr float kMinSpeedTimeFactor = 0.04f;  // ~3-5 FPS feel at speed 1

float computeSpeedTimeFactor(uint8_t speed) {
    if (lightwaveos::actors::LedConfig::MAX_SPEED <= 1) {
        return 1.0f;
    }
    float norm = 0.0f;
    if (speed > 1) {
        norm = (static_cast<float>(speed - 1) /
                static_cast<float>(lightwaveos::actors::LedConfig::MAX_SPEED - 1));
        if (norm > 1.0f) norm = 1.0f;
    }
    // Use a gentle curve to preserve mid/high speeds while slowing the low end.
    float curved = sqrtf(norm);
    return kMinSpeedTimeFactor + (1.0f - kMinSpeedTimeFactor) * curved;
}
}  // namespace

// Stub for legacy effect ID tracking - no-op when legacy effects are disabled
// When legacy/LegacyEffectWrapper is re-enabled, this will be replaced by the real implementation
namespace lightwaveos { namespace actors {
    void setCurrentLegacyEffectId(uint8_t) {}  // No-op stub
}}

#include <cstring>
#include <cstdio>

#ifndef NATIVE_BUILD
#include <Arduino.h>
#endif

// Unified logging system (preserves colored output conventions)
#define LW_LOG_TAG "Renderer"
#include "utils/Log.h"

namespace lightwaveos {
namespace actors {

#if FEATURE_AUDIO_SYNC
static audio::MusicalGridTuning toMusicalGridTuning(const audio::AudioContractTuning& tuning) {
    audio::MusicalGridTuning grid;
    grid.bpmMin = tuning.bpmMin;
    grid.bpmMax = tuning.bpmMax;
    grid.bpmTau = tuning.bpmTau;
    grid.confidenceTau = tuning.confidenceTau;
    grid.phaseCorrectionGain = tuning.phaseCorrectionGain;
    grid.barCorrectionGain = tuning.barCorrectionGain;
    return grid;
}
#endif

// ============================================================================
// Constructor / Destructor
// ============================================================================

RendererActor::RendererActor()
    : Actor(ActorConfigs::Renderer())
    , m_currentEffect(0)
    , m_brightness(LedConfig::DEFAULT_BRIGHTNESS)
    , m_speed(LedConfig::DEFAULT_SPEED)
    , m_paletteIndex(0)
    , m_hue(0)
    , m_intensity(128)
    , m_saturation(255)
    , m_complexity(128)
    , m_variation(0)
    , m_mood(128)  // Default: balanced reactive/smooth
    , m_fadeAmount(20)
    , m_effectCount(0)
    , m_lastFrameTime(0)
    , m_frameCount(0)
    , m_effectTimeSeconds(0.0f)
    , m_effectFrameAccumulator(0.0f)
    , m_effectFrameCount(0)
    , m_zoneComposer(nullptr)
#if FEATURE_TRANSITIONS
    , m_transitionEngine(nullptr)
    , m_pendingEffect(0)
    , m_transitionPending(false)
#endif
    , m_captureEnabled(false)
    , m_captureTapMask(0)
    , m_correctionSkipCount(0)
    , m_correctionApplyCount(0)
    , m_captureBlock(nullptr)
    , m_captureTapA(nullptr)
    , m_captureTapB(nullptr)
    , m_captureTapC(nullptr)
    , m_captureTapAValid(false)
    , m_captureTapBValid(false)
    , m_captureTapCValid(false)
#if FEATURE_AUDIO_SYNC
#if FEATURE_AUDIO_BACKEND_ESV11
    , m_esBeatClock()                // Explicit default init for audio state
#else
    , m_musicalGrid()                // Explicit default init for audio state
#endif
    , m_lastControlBus()
    , m_lastMusicalGrid()
    , m_lastAudioTime()
#endif
{
    // Initialize LED buffers to black
    m_strip1 = nullptr;
    m_strip2 = nullptr;
    memset(m_leds, 0, sizeof(m_leds));
#if FEATURE_TRANSITIONS
    memset(m_transitionSourceBuffer, 0, sizeof(m_transitionSourceBuffer));
    m_transitionEngine = new TransitionEngine();
#endif

    // Initialize effect registry
    for (uint8_t i = 0; i < MAX_EFFECTS; i++) {
        m_effects[i].name = nullptr;
        m_effects[i].effect = nullptr;
        m_effects[i].active = false;
        m_legacyAdapters[i] = nullptr;
    }

    // Default palette - load from master palette system (index 0: Sunset Real)
    m_currentPalette = gMasterPalettes[0];

#if FEATURE_AUDIO_SYNC
    m_audioContractTuning = audio::clampAudioContractTuning(audio::AudioContractTuning{});
    m_audioContractPending = m_audioContractTuning;
#if !FEATURE_AUDIO_BACKEND_ESV11
    m_musicalGrid.setTuning(toMusicalGridTuning(m_audioContractTuning));
    m_musicalGrid.SetTimeSignature(m_audioContractTuning.beatsPerBar, m_audioContractTuning.beatUnit);
#endif
#endif
}

RendererActor::~RendererActor()
{
#if FEATURE_TRANSITIONS
    if (m_transitionEngine) {
        delete m_transitionEngine;
        m_transitionEngine = nullptr;
    }
#endif
    if (m_captureBlock) {
        free(m_captureBlock);
        m_captureBlock = nullptr;
        m_captureTapA = nullptr;
        m_captureTapB = nullptr;
        m_captureTapC = nullptr;
    }
    // Actor base class handles task cleanup
}

// ============================================================================
// State Accessors
// ============================================================================

void RendererActor::getBufferCopy(CRGB* outBuffer) const
{
    if (outBuffer != nullptr) {
        // Note: This is a snapshot - the buffer may change during copy
        // For strict consistency, use a mutex or double-buffer
        memcpy(outBuffer, m_leds, sizeof(m_leds));
    }
}

// ============================================================================
// Effect Registration
// ============================================================================

bool RendererActor::registerEffect(uint8_t id, const char* name, EffectRenderFn fn)
{
    if (id >= MAX_EFFECTS || name == nullptr || fn == nullptr) {
        return false;
    }

    // Create LegacyEffectAdapter to wrap function pointer
    // Allocate adapter (owned by RendererActor)
    if (m_legacyAdapters[id] != nullptr) {
        // Already registered - clean up old adapter
        delete m_legacyAdapters[id];
    }
    
    m_legacyAdapters[id] = new plugins::runtime::LegacyEffectAdapter(name, fn);
    if (m_legacyAdapters[id] == nullptr) {
        return false;  // Allocation failed
    }

    m_effects[id].name = name;
    m_effects[id].effect = m_legacyAdapters[id];
    m_effects[id].active = true;

    // Update count
    if (id >= m_effectCount) {
        m_effectCount = id + 1;
    }

    LW_LOGD("Registered effect %d: %s (legacy -> IEffect adapter)", id, name);

    return true;
}

bool RendererActor::registerEffect(uint8_t id, plugins::IEffect* effect)
{
    if (id >= MAX_EFFECTS || effect == nullptr) {
        return false;
    }

    // Clean up any existing legacy adapter for this ID
    if (m_legacyAdapters[id] != nullptr) {
        delete m_legacyAdapters[id];
        m_legacyAdapters[id] = nullptr;
    }

    // Get name from effect metadata
    const plugins::EffectMetadata& meta = effect->getMetadata();
    
    m_effects[id].name = meta.name;
    m_effects[id].effect = effect;
    m_effects[id].active = true;

    // Update count
    if (id >= m_effectCount) {
        m_effectCount = id + 1;
    }

    LW_LOGD("Registered effect %d: %s (IEffect native)", id, meta.name);

    return true;
}

// ============================================================================
// IEffectRegistry Implementation
// ============================================================================

bool RendererActor::unregisterEffect(uint8_t id)
{
    if (id >= MAX_EFFECTS) {
        return false;
    }

    if (m_effects[id].active) {
        m_effects[id].active = false;
        m_effects[id].effect = nullptr;
        m_effects[id].name = nullptr;

        // Clean up legacy adapter if present
        if (m_legacyAdapters[id] != nullptr) {
            delete m_legacyAdapters[id];
            m_legacyAdapters[id] = nullptr;
        }

        // Update count (recalculate from active effects)
        // This is safe but may not be exact if effects are unregistered out of order
        // The count represents the highest registered ID, not the actual active count
        uint8_t highestActive = 0;
        for (uint8_t i = 0; i < MAX_EFFECTS; i++) {
            if (m_effects[i].active && i >= highestActive) {
                highestActive = i + 1;
            }
        }
        m_effectCount = highestActive;

        LW_LOGD("Unregistered effect %d", id);
        return true;
    }

    return false;
}

bool RendererActor::isEffectRegistered(uint8_t id) const
{
    if (id >= MAX_EFFECTS) {
        return false;
    }
    return m_effects[id].active;
}

uint8_t RendererActor::getRegisteredCount() const
{
    // Count actual active effects (not just highest ID)
    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_EFFECTS; i++) {
        if (m_effects[i].active) {
            count++;
        }
    }
    return count;
}

const char* RendererActor::getEffectName(uint8_t id) const
{
    if (id < MAX_EFFECTS && m_effects[id].active) {
        return m_effects[id].name;
    }
    return "Unknown";
}

uint8_t RendererActor::getPaletteCount() const
{
    return MASTER_PALETTE_COUNT;
}

const char* RendererActor::getPaletteName(uint8_t id) const
{
    return lightwaveos::palettes::getPaletteName(id);
}

plugins::IEffect* RendererActor::getEffectInstance(uint8_t id) const
{
    if (id < MAX_EFFECTS && m_effects[id].active) {
        return m_effects[id].effect;
    }
    return nullptr;
}

/**
 * @brief Validate and clamp effectId to safe range [0, MAX_EFFECTS-1]
 * 
 * DEFENSIVE CHECK: Prevents LoadProhibited crashes from corrupted effect ID.
 * 
 * RendererActor uses m_effects[MAX_EFFECTS] array where MAX_EFFECTS = 100. If
 * effectId is corrupted (e.g., by memory corruption, invalid input, or race
 * condition), accessing m_effects[effectId] would cause out-of-bounds access
 * and crash.
 * 
 * This validation ensures we always access valid array indices, returning safe
 * default (effect 0) if corruption is detected.
 * 
 * @param effectId Effect ID to validate
 * @return Valid effect ID in [0, MAX_EFFECTS-1], defaults to 0 if out of bounds
 */
uint8_t RendererActor::validateEffectId(uint8_t effectId) const {
#if FEATURE_VALIDATION_PROFILING
#ifndef NATIVE_BUILD
    int64_t start = esp_timer_get_time();
#else
    int64_t start = 0;
#endif
#endif
    // Validate effectId is within bounds [0, MAX_EFFECTS-1]
    if (effectId >= MAX_EFFECTS) {
#if FEATURE_VALIDATION_PROFILING
#ifndef NATIVE_BUILD
        lightwaveos::core::system::ValidationProfiler::recordCall("validateEffectId", 
                                                                    esp_timer_get_time() - start);
#else
        lightwaveos::core::system::ValidationProfiler::recordCall("validateEffectId", 0);
#endif
#endif
        return 0;  // Return safe default (effect 0)
    }
#if FEATURE_VALIDATION_PROFILING
#ifndef NATIVE_BUILD
    lightwaveos::core::system::ValidationProfiler::recordCall("validateEffectId", 
                                                                esp_timer_get_time() - start);
#else
    lightwaveos::core::system::ValidationProfiler::recordCall("validateEffectId", 0);
#endif
#endif
    return effectId;
}

// ============================================================================
// Actor Lifecycle
// ============================================================================

void RendererActor::onStart()
{
    LW_LOGI("Initializing LEDs on Core %d", xPortGetCoreID());

#ifndef NATIVE_BUILD
    // CRITICAL: Add this task to the watchdog
    // Without this, esp_task_wdt_reset() calls in onTick() have no effect
    esp_task_wdt_add(nullptr);  // nullptr means current task
    LW_LOGI("Renderer task added to watchdog");
#endif

    initLeds();

    // Subscribe to relevant events
    bus::MessageBus::instance().subscribe(MessageType::PALETTE_CHANGED, this);

    // Record start time
    m_lastFrameTime = micros();

    LW_LOGI("Ready - %d effects, brightness=%d, target=%d FPS",
             m_effectCount, m_brightness, LedConfig::TARGET_FPS);
}

void RendererActor::onMessage(const Message& msg)
{
    switch (msg.type) {
        case MessageType::SET_EFFECT:
            handleSetEffect(msg.param1);
            break;

        case MessageType::SET_BRIGHTNESS:
            handleSetBrightness(msg.param1);
            break;

        case MessageType::SET_SPEED:
            handleSetSpeed(msg.param1);
            break;

        case MessageType::SET_PALETTE:
            // #region agent log
            {
                FILE* f = fopen("/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/.cursor/debug.log", "a");
                if (f) {
                    fprintf(f,
                            "{\"sessionId\":\"debug-session\",\"runId\":\"palette-loop-1\",\"hypothesisId\":\"H2\","
                            "\"location\":\"RendererActor.cpp:onMessage\",\"message\":\"SET_PALETTE received\","
                            "\"data\":{\"paletteIndex\":%u,\"currentPalette\":%u},\"timestamp\":%lu}\n",
                            static_cast<unsigned>(msg.param1),
                            static_cast<unsigned>(m_paletteIndex),
                            static_cast<unsigned long>(millis()));
                    fclose(f);
                }
            }
            // #endregion
            handleSetPalette(msg.param1);
            break;

        case MessageType::SET_INTENSITY:
            handleSetIntensity(msg.param1);
            break;

        case MessageType::SET_SATURATION:
            handleSetSaturation(msg.param1);
            break;

        case MessageType::SET_COMPLEXITY:
            handleSetComplexity(msg.param1);
            break;

        case MessageType::SET_VARIATION:
            handleSetVariation(msg.param1);
            break;

        case MessageType::SET_HUE:
            handleSetHue(msg.param1);
            break;

        case MessageType::SET_MOOD:
            handleSetMood(msg.param1);
            break;

        case MessageType::SET_FADE_AMOUNT:
            handleSetFadeAmount(msg.param1);
            break;

        case MessageType::START_TRANSITION:
            handleStartTransition(msg.param1, msg.param2);
            break;

        case MessageType::HEALTH_CHECK:
            // Respond with health status
            {
                Message response(MessageType::HEALTH_STATUS);
                response.param1 = 1; // Healthy
                response.param2 = m_stats.currentFPS;
                response.param3 = m_stats.cpuPercent;
                response.param4 = m_stats.framesRendered;
                bus::MessageBus::instance().publish(response);
            }
            break;

        case MessageType::PALETTE_CHANGED:
            // External palette change notification
            // #region agent log
            {
                FILE* f = fopen("/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/.cursor/debug.log", "a");
                if (f) {
                    fprintf(f,
                            "{\"sessionId\":\"debug-session\",\"runId\":\"palette-loop-1\",\"hypothesisId\":\"H2\","
                            "\"location\":\"RendererActor.cpp:onMessage\",\"message\":\"PALETTE_CHANGED received\","
                            "\"data\":{\"paletteIndex\":%u,\"currentPalette\":%u},\"timestamp\":%lu}\n",
                            static_cast<unsigned>(msg.param1),
                            static_cast<unsigned>(m_paletteIndex),
                            static_cast<unsigned long>(millis()));
                    fclose(f);
                }
            }
            // #endregion
            handleSetPalette(msg.param1);
            break;

        case MessageType::PING:
            // Respond with pong
            {
                Message pong(MessageType::PONG);
                pong.param4 = msg.timestamp; // Echo original timestamp
                bus::MessageBus::instance().publish(pong);
            }
            break;

#if FEATURE_AUDIO_SYNC
        case MessageType::TRINITY_BEAT:
            {
                // Unpack BPM (param1=hi, param2=lo)
                uint16_t bpmFixed = ((uint16_t)msg.param1 << 8) | msg.param2;
                float bpm = (float)bpmFixed / 100.0f;
                
                // Unpack phase (param3)
                float phase01 = (float)msg.param3 / 255.0f;
                
                // Unpack flags (param4)
                bool tick = (msg.param4 & 0x01) != 0;
                bool downbeat = (msg.param4 & 0x02) != 0;
                int beatInBar = (int)((msg.param4 >> 2) & 0x03);

#if FEATURE_AUDIO_BACKEND_ESV11
                // Inject into renderer-domain beat clock (no MusicalGrid in ESV11 builds).
                uint64_t now_us = micros();
                uint32_t sr = m_lastAudioTime.sample_rate_hz ? m_lastAudioTime.sample_rate_hz : 12800;
                m_esBeatClock.injectExternalBeat(
                    bpm,
                    phase01,
                    tick,
                    downbeat,
                    static_cast<uint8_t>(beatInBar),
                    now_us,
                    sr
                );
#else
                m_musicalGrid.injectExternalBeat(bpm, phase01, tick, downbeat, beatInBar);
#endif
            }
            break;

        case MessageType::TRINITY_MACRO:
            {
                // Unpack macro values (all uint8_t, convert to float)
                float energy = (float)msg.param1 / 255.0f;
                float vocal = (float)msg.param2 / 255.0f;
                float bass = (float)msg.param3 / 255.0f;
                float perc = (float)((msg.param4 >> 24) & 0xFF) / 255.0f;
                float bright = (float)((msg.param4 >> 16) & 0xFF) / 255.0f;
                
                m_trinityProxy.setMacros(energy, vocal, bass, perc, bright);
            }
            break;

        case MessageType::TRINITY_SYNC:
            {
                uint8_t action = msg.param1;
                float positionSec = (float)msg.param4 / 1000.0f;
                uint16_t bpmFixed = ((uint16_t)msg.param2 << 8) | msg.param3;
                float bpm = (float)bpmFixed / 100.0f;

                switch (action) {
                    case 0: // start
                        m_trinitySyncActive = true;
                        m_trinitySyncPaused = false;
                        m_trinitySyncPosition = positionSec;
#if FEATURE_AUDIO_BACKEND_ESV11
                        m_esBeatClock.setExternalSyncMode(true);
#else
                        m_musicalGrid.setExternalSyncMode(true);
#endif
                        // Prime the proxy so isActive() returns true before first macro arrives
                        m_trinityProxy.markActive();
                        if (bpm > 0.0f) {
#if FEATURE_AUDIO_BACKEND_ESV11
                            uint64_t now_us = micros();
                            uint32_t sr = m_lastAudioTime.sample_rate_hz ? m_lastAudioTime.sample_rate_hz : 12800;
                            m_esBeatClock.injectExternalBeat(bpm, 0.0f, false, false, 0, now_us, sr);
#else
                            m_musicalGrid.injectExternalBeat(bpm, 0.0f, false, false, 0);
#endif
                        }
                        LW_LOGI("TRINITY_SYNC: START active=1 paused=0 pos=%.2fs bpm=%.1f", positionSec, bpm);
                        break;
                    case 1: // stop
                        m_trinitySyncActive = false;
                        m_trinitySyncPaused = false;
                        m_trinitySyncPosition = 0.0f;
#if FEATURE_AUDIO_BACKEND_ESV11
                        m_esBeatClock.setExternalSyncMode(false);
#else
                        m_musicalGrid.setExternalSyncMode(false);
#endif
                        m_trinityProxy.reset();
                        LW_LOGI("TRINITY_SYNC: STOP active=0 paused=0");
                        break;
                    case 2: // pause
                        m_trinitySyncPaused = true;
                        LW_LOGI("TRINITY_SYNC: PAUSE paused=1");
                        break;
                    case 3: // resume
                        m_trinitySyncPaused = false;
                        LW_LOGI("TRINITY_SYNC: RESUME paused=0");
                        break;
                    case 4: // seek
                        m_trinitySyncPosition = positionSec;
                        if (bpm > 0.0f) {
#if FEATURE_AUDIO_BACKEND_ESV11
                            uint64_t now_us = micros();
                            uint32_t sr = m_lastAudioTime.sample_rate_hz ? m_lastAudioTime.sample_rate_hz : 12800;
                            m_esBeatClock.injectExternalBeat(bpm, 0.0f, false, false, 0, now_us, sr);
#else
                            m_musicalGrid.injectExternalBeat(bpm, 0.0f, false, false, 0);
#endif
                        }
                        LW_LOGD("TRINITY_SYNC: SEEK pos=%.2fs bpm=%.1f", positionSec, bpm);
                        break;
                }
            }
            break;

        case MessageType::TRINITY_SEGMENT:
            {
                uint8_t index = msg.param1;
                uint16_t labelHash16 = ((uint16_t)msg.param2 << 8) | (uint16_t)msg.param3;
                uint32_t startMs = msg.param4;
                uint32_t endMs = msg._reserved;

                bool changed = (index != m_trinitySegmentIndex) ||
                               (labelHash16 != m_trinitySegmentLabelHash) ||
                               (startMs != m_trinitySegmentStartMs) ||
                               (endMs != m_trinitySegmentEndMs);

                m_trinitySegmentIndex = index;
                m_trinitySegmentLabelHash = labelHash16;
                m_trinitySegmentStartMs = startMs;
                m_trinitySegmentEndMs = endMs;

                if (changed) {
                    // Broadcast to any interested actors (semantic adapters, diagnostics, etc.)
                    bus::MessageBus::instance().publish(msg);
                    LW_LOGI("TRINITY_SEGMENT: idx=%u labelHash=0x%04X start=%lums end=%lums",
                            static_cast<unsigned>(index),
                            static_cast<unsigned>(labelHash16),
                            static_cast<unsigned long>(startMs),
                            static_cast<unsigned long>(endMs));
                }
            }
            break;
#endif

        default:
            // Unknown message - ignore
            break;
    }
}

void RendererActor::onTick()
{
    uint32_t frameStartUs = micros();
    static uint16_t s_wdtResetFrames = 0;

    // Render the current effect
    renderFrame();

    // TAP A: Capture pre-correction (after renderFrame, before processBuffer)
    if (m_captureEnabled && (m_captureTapMask & 0x01)) {
        captureFrame(CaptureTap::TAP_A_PRE_CORRECTION, m_leds);
    }

    // Post-render color correction pipeline (skip for sensitive effects)
    // Includes: LGP-sensitive, stateful, PHYSICS_BASED, MATHEMATICAL families
    // See PatternRegistry::shouldSkipColorCorrection() for full list
    uint8_t safeEffect = validateEffectId(m_currentEffect);
    if (!::PatternRegistry::shouldSkipColorCorrection(safeEffect)) {
        enhancement::ColorCorrectionEngine::getInstance().processBuffer(m_leds, LedConfig::TOTAL_LEDS);
        m_correctionApplyCount++;
    } else {
        m_correctionSkipCount++;
    }

    // TAP B: Capture post-correction (after processBuffer, before showLeds)
    if (m_captureEnabled && (m_captureTapMask & 0x02)) {
        captureFrame(CaptureTap::TAP_B_POST_CORRECTION, m_leds);
    }

#ifndef NATIVE_BUILD
    // CRITICAL: Yield BEFORE showLeds() to let IDLE1 reset its watchdog
    // FastLED.show() blocks for ~9.6ms, preventing IDLE1 from running
    // We must yield here so IDLE1 gets CPU time before the blocking call
    // Use vTaskDelay(1) not vTaskDelay(0) - vTaskDelay(0) may not yield if nothing else is ready
    vTaskDelay(1);
#endif

    // Push to strips
    showLeds();

    // Calculate frame time (pre-throttle)
    uint32_t frameEndUs = micros();
    uint32_t rawFrameTimeUs = frameEndUs - frameStartUs;

    // Handle micros() overflow (unlikely but possible)
    if (frameEndUs < frameStartUs) {
        rawFrameTimeUs = (UINT32_MAX - frameStartUs) + frameEndUs;
    }

#ifndef NATIVE_BUILD
    // Frame-rate throttle to ~120 FPS on targets with high tick rates.
    if (rawFrameTimeUs < LedConfig::FRAME_TIME_US) {
        uint32_t remainingUs = LedConfig::FRAME_TIME_US - rawFrameTimeUs;
        if (remainingUs > 0) {
            esp_rom_delay_us(remainingUs);
        }
        frameEndUs = micros();
    }

    // Reset watchdog every 10 frames (~83ms at 120 FPS)
    // This prevents watchdog timeout since RendererActor monopolizes CPU 1
    // and prevents IDLE1 from running to reset the watchdog.
    if (++s_wdtResetFrames >= 10) {
        s_wdtResetFrames = 0;
        esp_task_wdt_reset();
    }
#endif
    uint32_t frameTimeUs = frameEndUs - frameStartUs;
    if (frameEndUs < frameStartUs) {
        frameTimeUs = (UINT32_MAX - frameStartUs) + frameEndUs;
    }

    // Update statistics (use raw time for drops, throttled time for FPS)
    updateStats(frameTimeUs, rawFrameTimeUs);

    // Publish FRAME_RENDERED event (every 10 frames to reduce overhead)
    if ((m_frameCount % 10) == 0) {
        Message evt(MessageType::FRAME_RENDERED);
        evt.param1 = m_currentEffect;
        evt.param2 = m_stats.currentFPS;
        evt.param4 = m_frameCount;
        bus::MessageBus::instance().publish(evt);
    }

    m_lastFrameTime = frameStartUs;
    m_frameCount++;

#ifndef NATIVE_BUILD
    // CRITICAL: Yield at END of frame to let IDLE1 reset its watchdog
    // The Actor system calls onTick() synchronously when queue times out,
    // so we must explicitly yield here to give IDLE1 CPU time
    // Use vTaskDelay(1) to ensure at least one tick of yield (vTaskDelay(0) may not yield)
    vTaskDelay(1);
#endif
}

void RendererActor::onStop()
{
    LW_LOGI("Stopping - rendered %lu frames, %lu drops",
             m_stats.framesRendered, m_stats.frameDrops);

    // Unsubscribe from events
    bus::MessageBus::instance().unsubscribeAll(this);

    // Turn off all LEDs
    memset(m_leds, 0, sizeof(m_leds));
    showLeds();
}

// ============================================================================
// Frame Capture System (for testbed)
// ============================================================================

static bool ensureCaptureBuffers(CRGB*& block, CRGB*& tapA, CRGB*& tapB, CRGB*& tapC) {
    if (block != nullptr && tapA != nullptr && tapB != nullptr && tapC != nullptr) {
        return true;
    }

    const size_t bytes = static_cast<size_t>(lightwaveos::actors::LedConfig::TOTAL_LEDS) *
                         sizeof(CRGB) * 3U;

#ifndef NATIVE_BUILD
    block = static_cast<CRGB*>(
        heap_caps_calloc(1, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!block) {
        block = static_cast<CRGB*>(
            heap_caps_calloc(1, bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }
#else
    block = static_cast<CRGB*>(calloc(1, bytes));
#endif

    if (!block) {
        tapA = nullptr;
        tapB = nullptr;
        tapC = nullptr;
        return false;
    }

    tapA = block;
    tapB = block + lightwaveos::actors::LedConfig::TOTAL_LEDS;
    tapC = block + (2U * lightwaveos::actors::LedConfig::TOTAL_LEDS);
    return true;
}

void RendererActor::setCaptureMode(bool enabled, uint8_t tapMask) {
    const uint8_t masked = tapMask & 0x07;  // Only bits 0-2 are valid

    if (enabled) {
        if (!ensureCaptureBuffers(m_captureBlock, m_captureTapA, m_captureTapB, m_captureTapC)) {
            LW_LOGW("Capture enable refused: buffer allocation failed");
            m_captureEnabled = false;
            m_captureTapMask = 0;
            return;
        }
    }

    m_captureEnabled = enabled;
    m_captureTapMask = masked;
    
    if (!enabled) {
        m_captureTapAValid = false;
        m_captureTapBValid = false;
        m_captureTapCValid = false;
    }
    
    LW_LOGI("Capture mode %s (tapMask=0x%02X)",
             enabled ? "enabled" : "disabled", m_captureTapMask);
}

bool RendererActor::getCapturedFrame(CaptureTap tap, CRGB* outBuffer) const {
    if (!m_captureEnabled || outBuffer == nullptr) {
        return false;
    }
    
    bool valid = false;
    const CRGB* source = nullptr;
    
    switch (tap) {
        case CaptureTap::TAP_A_PRE_CORRECTION:
            valid = m_captureTapAValid;
            source = m_captureTapA;
            break;
        case CaptureTap::TAP_B_POST_CORRECTION:
            valid = m_captureTapBValid;
            source = m_captureTapB;
            break;
        case CaptureTap::TAP_C_PRE_WS2812:
            valid = m_captureTapCValid;
            source = m_captureTapC;
            break;
        default:
            return false;
    }
    
    if (valid && source != nullptr) {
        memcpy(outBuffer, source, sizeof(CRGB) * LedConfig::TOTAL_LEDS);
        return true;
    }
    
    return false;
}

RendererActor::CaptureMetadata RendererActor::getCaptureMetadata() const {
    return m_captureMetadata;
}

#if FEATURE_AUDIO_SYNC
audio::AudioContractTuning RendererActor::getAudioContractTuning() const {
    audio::AudioContractTuning out;
    uint32_t v0;
    uint32_t v1 = 0;
    do {
        v0 = m_audioContractSeq.load(std::memory_order_acquire);
        if (v0 & 1U) continue;
        out = m_audioContractPending;
        v1 = m_audioContractSeq.load(std::memory_order_acquire);
    } while (v0 != v1 || (v1 & 1U));
    return out;
}

void RendererActor::setAudioContractTuning(const audio::AudioContractTuning& tuning) {
    audio::AudioContractTuning clamped = audio::clampAudioContractTuning(tuning);
    uint32_t v = m_audioContractSeq.load(std::memory_order_relaxed);
    m_audioContractSeq.store(v + 1U, std::memory_order_release);
    m_audioContractPending = clamped;
    m_audioContractSeq.store(v + 2U, std::memory_order_release);
    m_audioContractDirty.store(true, std::memory_order_release);
}

void RendererActor::applyPendingAudioContractTuning() {
    if (!m_audioContractDirty.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    audio::AudioContractTuning pending = getAudioContractTuning();
    m_audioContractTuning = pending;
#if !FEATURE_AUDIO_BACKEND_ESV11
    m_musicalGrid.setTuning(toMusicalGridTuning(m_audioContractTuning));
    m_musicalGrid.SetTimeSignature(m_audioContractTuning.beatsPerBar, m_audioContractTuning.beatUnit);
#endif
}

#endif

bool RendererActor::enqueueEffectParameterUpdate(uint8_t effectId, const char* name, float value) {
    if (!name || name[0] == '\0') {
        return false;
    }

    uint8_t head = m_paramQueueHead.load(std::memory_order_relaxed);
    uint8_t next = static_cast<uint8_t>((head + 1) % PARAM_QUEUE_SIZE);
    uint8_t tail = m_paramQueueTail.load(std::memory_order_acquire);
    if (next == tail) {
        return false;
    }

    EffectParamUpdate& slot = m_paramQueue[head];
    slot.effectId = effectId;
    strncpy(slot.name, name, sizeof(slot.name) - 1);
    slot.name[sizeof(slot.name) - 1] = '\0';
    slot.value = value;

    m_paramQueueHead.store(next, std::memory_order_release);
    return true;
}

void RendererActor::applyPendingEffectParameterUpdates() {
    uint8_t tail = m_paramQueueTail.load(std::memory_order_relaxed);
    uint8_t head = m_paramQueueHead.load(std::memory_order_acquire);
    while (tail != head) {
        const EffectParamUpdate& update = m_paramQueue[tail];
        if (update.effectId < m_effectCount) {
            // Validate effectId before access
            uint8_t safeEffectId = validateEffectId(update.effectId);
            plugins::IEffect* effect = m_effects[safeEffectId].effect;
            if (effect) {
                effect->setParameter(update.name, update.value);
            }
        }
        tail = static_cast<uint8_t>((tail + 1) % PARAM_QUEUE_SIZE);
        m_paramQueueTail.store(tail, std::memory_order_release);
        head = m_paramQueueHead.load(std::memory_order_acquire);
    }
}

void RendererActor::forceOneShotCapture(CaptureTap tap) {
    // forceOneShotCapture may be called even when capture mode is disabled (e.g. serial dump),
    // so ensure buffers exist for the requested tap.
    if (!ensureCaptureBuffers(m_captureBlock, m_captureTapA, m_captureTapB, m_captureTapC)) {
        LW_LOGW("One-shot capture skipped: buffer allocation failed");
        return;
    }

    // Preserve the live LED state buffer so buffer-feedback effects are not disturbed.
    CRGB savedLeds[LedConfig::TOTAL_LEDS];
    memcpy(savedLeds, m_leds, sizeof(savedLeds));

    // Preserve hue increment side-effect inside renderFrame()
    const uint8_t savedHue = m_hue;

    // Render one frame into m_leds (based on the preserved previous state)
    renderFrame();

    if (tap == CaptureTap::TAP_A_PRE_CORRECTION) {
        captureFrame(CaptureTap::TAP_A_PRE_CORRECTION, m_leds);
    } else {
        // For Tap B/C we need the post-correction buffer, but we must not mutate m_leds.
        CRGB corrected[LedConfig::TOTAL_LEDS];
        memcpy(corrected, m_leds, sizeof(corrected));

        enhancement::ColorCorrectionEngine::getInstance().processBuffer(corrected, LedConfig::TOTAL_LEDS);

        if (tap == CaptureTap::TAP_B_POST_CORRECTION) {
            captureFrame(CaptureTap::TAP_B_POST_CORRECTION, corrected);
        } else if (tap == CaptureTap::TAP_C_PRE_WS2812) {
            // Tap C is "pre-WS2812" after strip split. The split is a straight copy in showLeds(),
            // so the unified interleaved buffer is equivalent to the corrected buffer.
            captureFrame(CaptureTap::TAP_C_PRE_WS2812, corrected);
        }
    }

    // Restore state so this on-demand capture does not perturb effect behaviour.
    memcpy(m_leds, savedLeds, sizeof(savedLeds));
    m_hue = savedHue;
}

void RendererActor::captureFrame(CaptureTap tap, const CRGB* sourceBuffer) {
    if (sourceBuffer == nullptr) {
        return;
    }

    if (m_captureTapA == nullptr || m_captureTapB == nullptr || m_captureTapC == nullptr) {
        return;
    }
    
    // Update metadata
    m_captureMetadata.effectId = m_currentEffect;
    m_captureMetadata.paletteId = m_paletteIndex;
    m_captureMetadata.brightness = m_brightness;
    m_captureMetadata.speed = m_speed;
    m_captureMetadata.frameIndex = m_frameCount;
    m_captureMetadata.timestampUs = micros();
    
    // Copy frame data
    switch (tap) {
        case CaptureTap::TAP_A_PRE_CORRECTION:
            memcpy(m_captureTapA, sourceBuffer, sizeof(CRGB) * LedConfig::TOTAL_LEDS);
            m_captureTapAValid = true;
            break;
        case CaptureTap::TAP_B_POST_CORRECTION:
            memcpy(m_captureTapB, sourceBuffer, sizeof(CRGB) * LedConfig::TOTAL_LEDS);
            m_captureTapBValid = true;
            break;
        case CaptureTap::TAP_C_PRE_WS2812:
            memcpy(m_captureTapC, sourceBuffer, sizeof(CRGB) * LedConfig::TOTAL_LEDS);
            m_captureTapCValid = true;
            break;
        default:
            break;
    }
}

// ============================================================================
// Private Methods
// ============================================================================

void RendererActor::initLeds()
{
    hal::LedStripConfig config1;
    hal::LedStripConfig config2;

    config1.ledCount = LedConfig::LEDS_PER_STRIP;
    config1.dataPin = LedConfig::STRIP1_PIN;
    config1.brightness = m_brightness;
    config1.reverseOrder = false;
    config1.colorCorrection = TypicalLEDStrip;

    config2 = config1;
    config2.dataPin = LedConfig::STRIP2_PIN;

    if (!m_ledDriver.initDual(config1, config2)) {
        LW_LOGE("LED driver init failed");
        return;
    }

    m_strip1 = m_ledDriver.getBuffer(0);
    m_strip2 = m_ledDriver.getBuffer(1);

    if (m_strip1 == nullptr || m_strip2 == nullptr) {
        LW_LOGE("LED buffers not available after init");
        return;
    }

    m_ledDriver.setMaxPower(5, 3000);
    m_ledDriver.clear(true);

    LW_LOGI("LED driver initialized: 2x%d LEDs on pins %d/%d",
             LedConfig::LEDS_PER_STRIP, LedConfig::STRIP1_PIN, LedConfig::STRIP2_PIN);
}

void RendererActor::renderFrame()
{
#if FEATURE_AUDIO_SYNC && !FEATURE_AUDIO_BACKEND_ESV11
    applyPendingAudioContractTuning();

    // Advance TempoTracker phase at 120 FPS
    // This must happen every frame for smooth beat tracking
    if (m_tempo != nullptr) {
        // Calculate delta time in seconds (from micros)
        uint32_t now = micros();
        uint32_t deltaMicros;
        if (now >= m_lastFrameTime) {
            deltaMicros = now - m_lastFrameTime;
        } else {
            deltaMicros = (UINT32_MAX - m_lastFrameTime) + now;
        }
        float deltaSec = static_cast<float>(deltaMicros) / 1000000.0f;

        // Advance tempo phase - this detects beat ticks
        m_tempo->advancePhase(deltaSec);

        // Get tempo output to update MusicalGrid
        lightwaveos::audio::TempoTrackerOutput tempoOut = m_tempo->getOutput();
        if (tempoOut.locked) {
            // Feed tempo to MusicalGrid for effects to use
            m_musicalGrid.OnTempoEstimate(
                m_lastAudioTime,
                tempoOut.bpm,
                tempoOut.confidence
            );
            if (tempoOut.beat_tick) {
                m_musicalGrid.OnBeatObservation(
                    m_lastAudioTime,
                    tempoOut.beat_strength,
                    false  // is_downbeat - not tracked by TempoTracker
                );
            }
        }
    }
#endif
    applyPendingEffectParameterUpdates();

#if FEATURE_TRANSITIONS
    // EXCLUSIVE MODE: If transition active, ONLY update transition
    // v1 pattern: effect OR transition, never both
    if (m_transitionEngine && m_transitionEngine->isActive()) {
        m_transitionEngine->update();
        m_hue += 1;
        return;  // Skip all effect rendering
    }
#endif

    // =========================================================================
    // Audio Context Preparation (used by both zone mode and single-effect mode)
    // =========================================================================
#if FEATURE_AUDIO_SYNC
    // NOTE: Keep AudioContext off the task stack. It is large (contains control bus,
    // waveform, bins, etc.) and combined with deep effect call stacks it can trigger
    // FreeRTOS stack overflow in the Renderer task.
    bool audioAvailable = false;
    if (m_controlBusBuffer != nullptr) {
        // 1. Read latest ControlBusFrame BY VALUE (thread-safe)
        uint32_t seq = m_controlBusBuffer->ReadLatest(m_lastControlBus);

        // Store previous sequence BEFORE updating (for availability gate)
        uint32_t prevSeq = m_lastControlBusSeq;

        // 2. Extrapolate AudioTime from audio snapshot
        uint64_t now_us = micros();
        if (seq != m_lastControlBusSeq) {
            // New audio frame arrived - resync extrapolation base
            m_lastAudioTime = m_lastControlBus.t;
            m_lastAudioMicros = now_us;
            m_lastControlBusSeq = seq;
        }

        // 3. Build extrapolated render-time AudioTime
        uint64_t dt_us = now_us - m_lastAudioMicros;
        uint64_t extrapolated_samples = m_lastAudioTime.sample_index +
            (dt_us * m_lastAudioTime.sample_rate_hz / 1000000);
        audio::AudioTime render_now(
            extrapolated_samples,
            m_lastAudioTime.sample_rate_hz,
            now_us
        );

        // 4. Compute freshness + "new frame" detection
        float age_s = audio::AudioTime_SecondsBetween(m_lastControlBus.t, render_now);
        float staleness_s = m_audioContractTuning.audioStalenessMs / 1000.0f;
        bool sequence_changed = (seq != prevSeq);
        bool age_within_tolerance = (age_s >= -0.01f && age_s < staleness_s);
        audioAvailable = sequence_changed || age_within_tolerance;

        // 5. Beat phase at 120 FPS (renderer-domain integration)
#if FEATURE_AUDIO_BACKEND_ESV11
        m_esBeatClock.tick(m_lastControlBus, sequence_changed, render_now);
        m_lastMusicalGrid = m_esBeatClock.snapshot();
#else
        m_musicalGrid.Tick(render_now);
        m_musicalGrid.ReadLatest(m_lastMusicalGrid);
#endif
        
        // Debug: Log audio availability issues every 4 seconds (reduced frequency)
        static uint32_t lastAudioDbg = 0;
        uint32_t nowDbg = millis();
        if (nowDbg - lastAudioDbg >= 4000) {
            lastAudioDbg = nowDbg;
            if (!audioAvailable) {
                LW_LOGW("Audio unavailable: seq=%u prevSeq=%u age_s=%.3f staleness_s=%.3f hop_seq=%u",
                        seq, prevSeq, age_s, staleness_s, m_lastControlBus.hop_seq);
            } else {
                // Include ES raw signal peaks to aid parity debugging against Emotiscope.
#if FEATURE_AUDIO_BACKEND_ESV11
                float maxBinRaw = 0.0f;
                float maxChromaRaw = 0.0f;
                for (uint8_t i = 0; i < audio::ControlBusFrame::BINS_64_COUNT; ++i) {
                    if (m_lastControlBus.es_bins64_raw[i] > maxBinRaw) maxBinRaw = m_lastControlBus.es_bins64_raw[i];
                }
                for (uint8_t i = 0; i < audio::CONTROLBUS_NUM_CHROMA; ++i) {
                    if (m_lastControlBus.es_chroma_raw[i] > maxChromaRaw) maxChromaRaw = m_lastControlBus.es_chroma_raw[i];
                }
                LW_LOGI("Audio OK: seq=%u hop_seq=%u rms=%.3f flux=%.3f es_vu=%.3f es_binMax=%.3f es_chrMax=%.3f bpm=%.1f conf=%.2f",
                        seq, m_lastControlBus.hop_seq, m_lastControlBus.rms, m_lastControlBus.flux,
                        m_lastControlBus.es_vu_level_raw, maxBinRaw, maxChromaRaw,
                        m_lastControlBus.es_bpm, m_lastControlBus.es_tempo_confidence);
#else
                LW_LOGI("Audio OK: seq=%u hop_seq=%u rms=%.3f flux=%.3f",
                        seq, m_lastControlBus.hop_seq, m_lastControlBus.rms, m_lastControlBus.flux);
#endif
            }
        }

        // 6. Populate shared AudioContext (member, reused across zone + single-effect mode)
        // Check if Trinity sync is active and proxy is fresh
        bool trinityActive = (m_trinitySyncActive && m_trinityProxy.isActive() && !m_trinitySyncPaused);

        // Periodic Trinity state debug (every 2 seconds when trinity sync flag is set)
        static uint32_t lastTrinityDbg = 0;
        uint32_t nowMs = millis();
        if (m_trinitySyncActive && (nowMs - lastTrinityDbg >= 2000)) {
            lastTrinityDbg = nowMs;
            LW_LOGD("Trinity state: syncActive=%d proxyActive=%d paused=%d => trinityActive=%d",
                    m_trinitySyncActive, m_trinityProxy.isActive(), m_trinitySyncPaused, trinityActive);
        }

        if (trinityActive) {
            // Use Trinity proxy for offline ML analysis sync
            m_sharedAudioCtx.controlBus = m_trinityProxy.getFrame();
            m_sharedAudioCtx.musicalGrid = m_lastMusicalGrid;
            m_sharedAudioCtx.available = true;
            m_sharedAudioCtx.trinityActive = true;
        } else {
            // Use live audio data
            m_sharedAudioCtx.controlBus = m_lastControlBus;
            m_sharedAudioCtx.musicalGrid = m_lastMusicalGrid;
            m_sharedAudioCtx.available = audioAvailable;
            m_sharedAudioCtx.trinityActive = false;
        }
    } else {
        // No audio buffer - check Trinity proxy as fallback
        bool trinityActive = (m_trinitySyncActive && m_trinityProxy.isActive() && !m_trinitySyncPaused);
        if (trinityActive) {
            m_sharedAudioCtx.controlBus = m_trinityProxy.getFrame();
            m_sharedAudioCtx.musicalGrid = m_lastMusicalGrid;
            m_sharedAudioCtx.available = true;
            m_sharedAudioCtx.trinityActive = true;
        } else {
            m_sharedAudioCtx.available = false;
            m_sharedAudioCtx.trinityActive = false;
        }
    }
#endif

    // Calculate delta time (in ms) - needed for both zone mode and single-effect mode
    uint32_t now = micros();
    uint32_t deltaTimeMs;
    if (now >= m_lastFrameTime) {
        deltaTimeMs = (now - m_lastFrameTime) / 1000;
    } else {
        deltaTimeMs = ((UINT32_MAX - m_lastFrameTime) + now) / 1000;
    }

    // Check if zone composer is enabled
    if (m_zoneComposer != nullptr && m_zoneComposer->isEnabled()) {
        // Use ZoneComposer for multi-zone rendering
#if FEATURE_AUDIO_SYNC
        m_zoneComposer->render(m_leds, LedConfig::TOTAL_LEDS,
                               &m_currentPalette, m_hue, m_frameCount, deltaTimeMs, &m_sharedAudioCtx);
#else
        m_zoneComposer->render(m_leds, LedConfig::TOTAL_LEDS,
                               &m_currentPalette, m_hue, m_frameCount, deltaTimeMs, nullptr);
#endif
        m_hue += 1;
        return;
    }

    // Single-effect mode
    // Validate current effect ID before access
    uint8_t safeEffect = validateEffectId(m_currentEffect);
    if (safeEffect >= MAX_EFFECTS || !m_effects[safeEffect].active) {
        // No effect - clear buffer
        memset(m_leds, 0, sizeof(m_leds));
        return;
    }

    // IEffect-only path (all effects are IEffect instances)
    //
    // IMPORTANT: Always use the validated/clamped `safeEffect` for both:
    // - indexing m_effects[]
    // - passing effectId into any downstream subsystems (e.g. audio mapping)
    // Using m_currentEffect directly here is unsafe if it ever gets corrupted.
    if (m_effects[safeEffect].effect != nullptr) {
        plugins::EffectContext& ctx = m_effectContext;
        ctx.leds = m_leds;
        ctx.ledCount = LedConfig::TOTAL_LEDS;
        ctx.centerPoint = LedConfig::CENTER_POINT;
        ctx.palette = plugins::PaletteRef(&m_currentPalette);
        ctx.brightness = m_brightness;
        ctx.speed = m_speed;
        ctx.gHue = m_hue;
        ctx.intensity = m_intensity;
        ctx.saturation = m_saturation;
        ctx.complexity = m_complexity;
        ctx.variation = m_variation;
        ctx.mood = m_mood;
        ctx.fadeAmount = m_fadeAmount;
        ctx.frameNumber = m_frameCount;
        ctx.totalTimeMs = 0;
        ctx.deltaTimeMs = 0;
        ctx.deltaTimeSeconds = 0.0f;
        ctx.zoneId = 0xFF;  // Global render
        ctx.zoneStart = 0;
        ctx.zoneLength = 0;

        // =====================================================================
        // Phase 2: Audio Context Integration
        // Reuse shared audio context prepared before zone composer check
        // =====================================================================
#if FEATURE_AUDIO_SYNC
        ctx.audio = m_sharedAudioCtx;
#else
        ctx.audio.available = false;
#endif

        // =====================================================================
        // Phase 4: Audio-Effect Parameter Mapping
        // Apply configured audio→visual mappings BEFORE effect->render()
        // Uses cached m_effectHasAudioMappings (updated on effect change only)
        // =====================================================================
#if FEATURE_AUDIO_SYNC
        if (m_effectHasAudioMappings) {
            // Extract current parameters as local copies for mapping
            uint8_t mappedBrightness = ctx.brightness;
            uint8_t mappedSpeed = ctx.speed;
            uint8_t mappedIntensity = ctx.intensity;
            uint8_t mappedSaturation = ctx.saturation;
            uint8_t mappedComplexity = ctx.complexity;
            uint8_t mappedVariation = ctx.variation;
            uint8_t mappedHue = ctx.gHue;

            audio::AudioMappingRegistry::instance().applyMappings(
                safeEffect,
                m_lastControlBus,
                m_lastMusicalGrid,
                ctx.audio.available,
                mappedBrightness,
                mappedSpeed,
                mappedIntensity,
                mappedSaturation,
                mappedComplexity,
                mappedVariation,
                mappedHue
            );

            // Write mapped values back to context
            ctx.brightness = mappedBrightness;
            ctx.speed = mappedSpeed;
            ctx.intensity = mappedIntensity;
            ctx.saturation = mappedSaturation;
            ctx.complexity = mappedComplexity;
            ctx.variation = mappedVariation;
            ctx.gHue = mappedHue;
        }
#endif

#if FEATURE_AUTO_SPEED
        // =====================================================================
        // Global auto-speed trim (tempo + spectral flux liveliness)
        // User SPEED acts as trim; audio drives base rate
        // =====================================================================
        float liveliness = 0.5f;
#if FEATURE_AUDIO_SYNC
        if (ctx.audio.available) {
            liveliness = m_lastControlBus.liveliness;
        }
#endif
        // User trim from speed knob (1..50 -> 0.0..1.0)
        float speedKnobNorm = (ctx.speed <= 1) ? 0.0f : (static_cast<float>(ctx.speed - 1) / 49.0f);
        float userTrim = 0.7f + 0.6f * speedKnobNorm;  // 0.7..1.3

        // Audio-driven base speed (10..40) scaled by liveliness
        float autoBase = 10.0f + (40.0f - 10.0f) * liveliness;
        float finalSpeed = autoBase * userTrim;
        if (finalSpeed < 1.0f) finalSpeed = 1.0f;
        if (finalSpeed > 50.0f) finalSpeed = 50.0f;
        ctx.speed = static_cast<uint8_t>(finalSpeed + 0.5f);
#endif

        // =====================================================================
        // Speed-scaled timing (slow motion at low speed settings)
        // =====================================================================
        float speedFactor = computeSpeedTimeFactor(ctx.speed);
        float deltaSeconds = static_cast<float>(deltaTimeMs) * 0.001f;
        float scaledDeltaSeconds = deltaSeconds * speedFactor;

        m_effectTimeSeconds += scaledDeltaSeconds;
        m_effectFrameAccumulator += speedFactor;
        if (m_effectFrameAccumulator >= 1.0f) {
            uint32_t advance = static_cast<uint32_t>(m_effectFrameAccumulator);
            m_effectFrameAccumulator -= static_cast<float>(advance);
            m_effectFrameCount += advance;
        }

        ctx.deltaTimeSeconds = scaledDeltaSeconds;
        ctx.deltaTimeMs = static_cast<uint32_t>(scaledDeltaSeconds * 1000.0f + 0.5f);
        ctx.frameNumber = m_effectFrameCount;
        ctx.totalTimeMs = static_cast<uint32_t>(m_effectTimeSeconds * 1000.0f + 0.5f);

        m_effects[safeEffect].effect->render(ctx);
    }

    // Increment hue for effects that use it
    m_hue += 1;  // Slow rotation
}

void RendererActor::showLeds()
{
    if (m_strip1 == nullptr || m_strip2 == nullptr) {
        return;
    }

    // Copy from unified buffer to strip buffers
    memcpy(m_strip1, &m_leds[0], sizeof(CRGB) * LedConfig::LEDS_PER_STRIP);
    memcpy(m_strip2, &m_leds[LedConfig::LEDS_PER_STRIP],
           sizeof(CRGB) * LedConfig::LEDS_PER_STRIP);

    // =========================================================================
    // Silence-based brightness gate (Sensory Bridge silent_scale pattern)
    // Fades ALL output to black after sustained silence
    // =========================================================================
#if FEATURE_AUDIO_SYNC
    if (m_controlBusBuffer != nullptr && m_lastControlBus.silentScale < 0.999f) {
        uint8_t scale = static_cast<uint8_t>(m_lastControlBus.silentScale * 255.0f);
        for (uint16_t i = 0; i < LedConfig::LEDS_PER_STRIP; ++i) {
            m_strip1[i].nscale8(scale);
            m_strip2[i].nscale8(scale);
        }
    }
#endif

    // TAP C: Capture pre-WS2812 (after strip split, before show)
    if (m_captureEnabled && (m_captureTapMask & 0x04) && m_captureTapC != nullptr) {
        // Interleave strip1 and strip2 into unified format for capture
        for (uint16_t i = 0; i < LedConfig::LEDS_PER_STRIP; i++) {
            m_captureTapC[i] = m_strip1[i];
            m_captureTapC[i + LedConfig::LEDS_PER_STRIP] = m_strip2[i];
        }
        captureFrame(CaptureTap::TAP_C_PRE_WS2812, m_captureTapC);
    }

    // Push to hardware
    m_ledDriver.show();

#if FEATURE_VALIDATION_PROFILING
    // Update validation profiling frame statistics
    lightwaveos::core::system::ValidationProfiler::updateFrame();
#endif
}

void RendererActor::updateStats(uint32_t frameTimeUs, uint32_t rawFrameTimeUs)
{
    m_stats.framesRendered++;

    // Check for frame drop (exceeded budget)
    if (rawFrameTimeUs > LedConfig::FRAME_TIME_US) {
        m_stats.frameDrops++;
    }

    // Update min/max
    if (frameTimeUs < m_stats.minFrameTimeUs) {
        m_stats.minFrameTimeUs = frameTimeUs;
    }
    if (frameTimeUs > m_stats.maxFrameTimeUs) {
        m_stats.maxFrameTimeUs = frameTimeUs;
    }

    // Rolling average (simple exponential smoothing)
    if (m_stats.avgFrameTimeUs == 0) {
        m_stats.avgFrameTimeUs = frameTimeUs;
    } else {
        // alpha = 0.1 (favor stability)
        m_stats.avgFrameTimeUs = (m_stats.avgFrameTimeUs * 9 + frameTimeUs) / 10;
    }

    // Calculate FPS every second (120 frames)
    if ((m_stats.framesRendered % 120) == 0) {
        // FPS = 1000000 / avgFrameTimeUs
        if (m_stats.avgFrameTimeUs > 0) {
            m_stats.currentFPS = 1000000 / m_stats.avgFrameTimeUs;
        }

        // CPU percent = (frameTime / frameBudget) * 100
        m_stats.cpuPercent = (m_stats.avgFrameTimeUs * 100) / LedConfig::FRAME_TIME_US;
        if (m_stats.cpuPercent > 100) {
            m_stats.cpuPercent = 100;
        }
    }
}

void RendererActor::handleSetEffect(uint8_t effectId)
{
    if (effectId >= MAX_EFFECTS || !m_effects[effectId].active) {
        LW_LOGW("Invalid effect ID: %d", effectId);
        return;
    }

    if (m_currentEffect != effectId) {
        // Validate oldEffect before accessing array
        uint8_t oldEffect = validateEffectId(m_currentEffect);

        // Cleanup old effect (only if valid and active)
        if (oldEffect < MAX_EFFECTS && m_effects[oldEffect].active && m_effects[oldEffect].effect != nullptr) {
            LW_LOGI("IEffect cleanup: %s (ID %d)", m_effects[oldEffect].name, oldEffect);
            m_effects[oldEffect].effect->cleanup();
        }

        m_currentEffect = effectId;

        // Initialize new effect
        if (m_effects[effectId].effect != nullptr) {
            LW_LOGI("IEffect init: %s (ID %d)", m_effects[effectId].name, effectId);
            plugins::EffectContext initCtx;
            initCtx.leds = m_leds;
            initCtx.ledCount = LedConfig::TOTAL_LEDS;
            initCtx.centerPoint = LedConfig::CENTER_POINT;
            initCtx.palette = plugins::PaletteRef(&m_currentPalette);
            initCtx.brightness = m_brightness;
            initCtx.speed = m_speed;
            initCtx.gHue = m_hue;
            initCtx.intensity = m_intensity;
            initCtx.saturation = m_saturation;
            initCtx.complexity = m_complexity;
            initCtx.variation = m_variation;
            initCtx.frameNumber = m_frameCount;
            initCtx.totalTimeMs = m_frameCount * 8;  // Approximate
            initCtx.deltaTimeMs = 8;  // Default
            initCtx.zoneId = 0xFF;
            initCtx.zoneStart = 0;
            initCtx.zoneLength = 0;
            
            if (!m_effects[effectId].effect->init(initCtx)) {
                // Initialization failed - revert to previous effect
                m_currentEffect = oldEffect;
                LW_LOGW("IEffect %d init failed, reverting to %d", effectId, oldEffect);
                return;
            }
            LW_LOGI("IEffect init: SUCCESS");
        }

        LW_LOGI("Effect changed: %d (%s) -> %d (" LW_CLR_GREEN "%s" LW_ANSI_RESET ")",
                 oldEffect, getEffectName(oldEffect),
                 effectId, getEffectName(effectId));

#if FEATURE_AUDIO_SYNC
        // Cache audio mapping check - avoids registry lookup every frame
        m_effectHasAudioMappings = audio::AudioMappingRegistry::instance().hasActiveMappings(effectId);
#endif

        // Publish EFFECT_CHANGED event
        Message evt(MessageType::EFFECT_CHANGED);
        evt.param1 = effectId;
        evt.param2 = oldEffect;
        bus::MessageBus::instance().publish(evt);
    }
}

void RendererActor::handleSetBrightness(uint8_t brightness)
{
    // Clamp to max brightness
    if (brightness > LedConfig::MAX_BRIGHTNESS) {
        brightness = LedConfig::MAX_BRIGHTNESS;
    }

    if (m_brightness != brightness) {
        m_brightness = brightness;

        m_ledDriver.setBrightness(m_brightness);
        LW_LOGD("Brightness: %d", m_brightness);
    }
}

void RendererActor::handleSetSpeed(uint8_t speed)
{
    // Clamp to valid range
    if (speed == 0) speed = 1;
    if (speed > LedConfig::MAX_SPEED) {
        speed = LedConfig::MAX_SPEED;
    }

    if (m_speed != speed) {
        m_speed = speed;

        LW_LOGD("Speed: %d", m_speed);
    }
}

void RendererActor::handleSetPalette(uint8_t paletteIndex)
{
    // Validate palette ID before access
    uint8_t safe_palette = lightwaveos::palettes::validatePaletteId(paletteIndex);

    if (m_paletteIndex != safe_palette) {
        // #region agent log
        {
            FILE* f = fopen("/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/.cursor/debug.log", "a");
            if (f) {
                fprintf(f,
                        "{\"sessionId\":\"debug-session\",\"runId\":\"palette-loop-1\",\"hypothesisId\":\"H3\","
                        "\"location\":\"RendererActor.cpp:handleSetPalette\",\"message\":\"palette update\","
                        "\"data\":{\"incoming\":%u,\"safe\":%u,\"previous\":%u},\"timestamp\":%lu}\n",
                        static_cast<unsigned>(paletteIndex),
                        static_cast<unsigned>(safe_palette),
                        static_cast<unsigned>(m_paletteIndex),
                        static_cast<unsigned long>(millis()));
                fclose(f);
            }
        }
        // #endregion
        m_paletteIndex = safe_palette;

        // Load palette from master palette array (75 palettes)
        m_currentPalette = gMasterPalettes[safe_palette];

        // Apply color correction for WHITE_HEAVY palettes
        uint8_t flags = master_palette_flags[safe_palette];
        enhancement::ColorCorrectionEngine::getInstance().correctPalette(m_currentPalette, flags);

        LW_LOGD("Palette: %d (%s)", m_paletteIndex, getPaletteName(m_paletteIndex));

        // Publish PALETTE_CHANGED event (for other actors)
        Message evt(MessageType::PALETTE_CHANGED);
        evt.param1 = m_paletteIndex;
        bus::MessageBus::instance().publish(evt);
    }
}

void RendererActor::handleSetIntensity(uint8_t intensity)
{
    if (m_intensity != intensity) {
        m_intensity = intensity;
        LW_LOGD("Intensity: %d", m_intensity);
    }
}

void RendererActor::handleSetSaturation(uint8_t saturation)
{
    if (m_saturation != saturation) {
        m_saturation = saturation;
        LW_LOGD("Saturation: %d", m_saturation);
    }
}

void RendererActor::handleSetComplexity(uint8_t complexity)
{
    if (m_complexity != complexity) {
        m_complexity = complexity;
        LW_LOGD("Complexity: %d", m_complexity);
    }
}

void RendererActor::handleSetVariation(uint8_t variation)
{
    if (m_variation != variation) {
        m_variation = variation;
        LW_LOGD("Variation: %d", m_variation);
    }
}

void RendererActor::handleSetHue(uint8_t hue)
{
    if (m_hue != hue) {
        m_hue = hue;
        LW_LOGD("Hue: %d", m_hue);
    }
}

void RendererActor::handleSetMood(uint8_t mood)
{
    if (m_mood != mood) {
        m_mood = mood;
        LW_LOGD("Mood: %d (%.1f%% smooth)", m_mood, m_mood * 100.0f / 255.0f);
    }
}

void RendererActor::handleSetFadeAmount(uint8_t fadeAmount)
{
    if (m_fadeAmount != fadeAmount) {
        m_fadeAmount = fadeAmount;
        LW_LOGD("FadeAmount: %d", m_fadeAmount);
    }
}

// ============================================================================
// Transition Methods
// ============================================================================

void RendererActor::handleStartTransition(uint8_t newEffectId, uint8_t transitionType)
{
    // Validate effectId before access
    uint8_t safeEffectId = validateEffectId(newEffectId);
    if (safeEffectId >= MAX_EFFECTS || !m_effects[safeEffectId].active) return;

#if FEATURE_TRANSITIONS
    // Thread-safe handler called from message queue (Core 1)
    if (!m_transitionEngine) return;
    if (transitionType >= static_cast<uint8_t>(TransitionType::TYPE_COUNT)) {
        transitionType = 0;  // Default to FADE
    }

    uint8_t oldEffect = validateEffectId(m_currentEffect);

    // Copy current LED state as source
    memcpy(m_transitionSourceBuffer, m_leds, sizeof(m_transitionSourceBuffer));

    // Switch to new effect
    m_currentEffect = safeEffectId;

    // Render one frame of new effect to get target
    renderFrame();

    // Start transition
    TransitionType type = static_cast<TransitionType>(transitionType);
    m_transitionEngine->startTransition(
        m_transitionSourceBuffer,
        m_leds,
        m_leds,
        type
    );

    LW_LOGI("Transition started: %s -> %s (%s)",
             getEffectName(oldEffect),
             getEffectName(safeEffectId),
             getTransitionName(type));
#else
    // Instant switch (no transition engine on FH4)
    m_currentEffect = safeEffectId;
#endif
}

void RendererActor::startTransition(uint8_t newEffectId, uint8_t transitionType)
{
    // DEPRECATED for external callers: unsafe from Core 0. Use ActorSystem::startTransition() instead.
    // Kept for internal Core 1 usage (ShowDirectorActor) only; request handlers must not call this directly.
    handleStartTransition(newEffectId, transitionType);
}

void RendererActor::startRandomTransition(uint8_t newEffectId)
{
#if FEATURE_TRANSITIONS
    TransitionType type = TransitionEngine::getRandomTransition();
    startTransition(newEffectId, static_cast<uint8_t>(type));
#else
    startTransition(newEffectId, 0);  // Instant switch
#endif
}

bool RendererActor::isTransitionActive() const
{
#if FEATURE_TRANSITIONS
    return m_transitionEngine && m_transitionEngine->isActive();
#else
    return false;
#endif
}

} // namespace actors
} // namespace lightwaveos
