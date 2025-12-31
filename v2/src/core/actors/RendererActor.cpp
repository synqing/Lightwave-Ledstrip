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
 * - FastLED.show(): ~2ms for 320 LEDs
 * - Remaining budget for message processing: ~2-4ms
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "RendererActor.h"
#include "../../effects/zones/ZoneComposer.h"
#include "../../effects/transitions/TransitionEngine.h"
#include "../../effects/PatternRegistry.h"
#include "../../palettes/Palettes_Master.h"
#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../plugins/runtime/LegacyEffectAdapter.h"

// Audio integration (Phase 2)
#if FEATURE_AUDIO_SYNC
#include "../../audio/AudioActor.h"
// K1-Lightwave integration (Phase 3)
#include "../../audio/k1/K1Utils.h"
#endif

using namespace lightwaveos::transitions;
using namespace lightwaveos::palettes;

// Stub for legacy effect ID tracking - no-op when legacy effects are disabled
// When legacy/LegacyEffectWrapper is re-enabled, this will be replaced by the real implementation
namespace lightwaveos { namespace actors {
    void setCurrentLegacyEffectId(uint8_t) {}  // No-op stub
}}

#include <cstring>

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
    , m_ctrl1(nullptr)
    , m_ctrl2(nullptr)
    , m_zoneComposer(nullptr)
    , m_transitionEngine(nullptr)
    , m_pendingEffect(0)
    , m_transitionPending(false)
    , m_captureEnabled(false)
    , m_captureTapMask(0)
    , m_correctionSkipCount(0)
    , m_correctionApplyCount(0)
    , m_captureTapAValid(false)
    , m_captureTapBValid(false)
    , m_captureTapCValid(false)
#if FEATURE_AUDIO_SYNC
    , m_musicalGrid()                // Explicit default init for audio state
    , m_lastControlBus()
    , m_lastMusicalGrid()
    , m_lastAudioTime()
#endif
{
    // Initialize LED buffers to black
    memset(m_strip1, 0, sizeof(m_strip1));
    memset(m_strip2, 0, sizeof(m_strip2));
    memset(m_leds, 0, sizeof(m_leds));
    memset(m_transitionSourceBuffer, 0, sizeof(m_transitionSourceBuffer));
    memset(m_captureTapA, 0, sizeof(m_captureTapA));
    memset(m_captureTapB, 0, sizeof(m_captureTapB));
    memset(m_captureTapC, 0, sizeof(m_captureTapC));

    // Create transition engine
    m_transitionEngine = new TransitionEngine();

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
    m_musicalGrid.setTuning(toMusicalGridTuning(m_audioContractTuning));
    m_musicalGrid.SetTimeSignature(m_audioContractTuning.beatsPerBar, m_audioContractTuning.beatUnit);
#endif
}

RendererActor::~RendererActor()
{
    // Clean up transition engine
    if (m_transitionEngine) {
        delete m_transitionEngine;
        m_transitionEngine = nullptr;
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

// ============================================================================
// Actor Lifecycle
// ============================================================================

void RendererActor::onStart()
{
    LW_LOGI("Initializing LEDs on Core %d", xPortGetCoreID());

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

        default:
            // Unknown message - ignore
            break;
    }
}

void RendererActor::onTick()
{
    uint32_t frameStartUs = micros();

    // Render the current effect
    renderFrame();

    // TAP A: Capture pre-correction (after renderFrame, before processBuffer)
    if (m_captureEnabled && (m_captureTapMask & 0x01)) {
        captureFrame(CaptureTap::TAP_A_PRE_CORRECTION, m_leds);
    }

    // Post-render color correction pipeline (skip for sensitive effects)
    // Includes: LGP-sensitive, stateful, PHYSICS_BASED, MATHEMATICAL families
    // See PatternRegistry::shouldSkipColorCorrection() for full list
    if (!::PatternRegistry::shouldSkipColorCorrection(m_currentEffect)) {
        enhancement::ColorCorrectionEngine::getInstance().processBuffer(m_leds, LedConfig::TOTAL_LEDS);
        m_correctionApplyCount++;
    } else {
        m_correctionSkipCount++;
    }

    // TAP B: Capture post-correction (after processBuffer, before showLeds)
    if (m_captureEnabled && (m_captureTapMask & 0x02)) {
        captureFrame(CaptureTap::TAP_B_POST_CORRECTION, m_leds);
    }

    // Push to strips
    showLeds();

    // Calculate frame time
    uint32_t frameEndUs = micros();
    uint32_t frameTimeUs = frameEndUs - frameStartUs;

    // Handle micros() overflow (unlikely but possible)
    if (frameEndUs < frameStartUs) {
        frameTimeUs = (UINT32_MAX - frameStartUs) + frameEndUs;
    }

    // Update statistics
    updateStats(frameTimeUs);

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

void RendererActor::setCaptureMode(bool enabled, uint8_t tapMask) {
    m_captureEnabled = enabled;
    m_captureTapMask = tapMask & 0x07;  // Only bits 0-2 are valid
    
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
    uint32_t v1;
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
    m_musicalGrid.setTuning(toMusicalGridTuning(m_audioContractTuning));
    m_musicalGrid.SetTimeSignature(m_audioContractTuning.beatsPerBar, m_audioContractTuning.beatUnit);
}

// ============================================================================
// K1-Lightwave Integration (Phase 3)
// ============================================================================

void RendererActor::processK1Updates() {
    // Drain tempo updates from K1 pipeline
    audio::k1::K1TempoUpdate tempoUpdate;
    while (m_k1TempoQueue.pop(tempoUpdate)) {
        // Update MusicalGrid with tempo from K1
        m_musicalGrid.updateFromK1(tempoUpdate.bpm, tempoUpdate.confidence, tempoUpdate.is_locked);
    }

    // Drain beat events from K1 pipeline
    audio::k1::K1BeatEvent beatEvent;
    while (m_k1BeatQueue.pop(beatEvent)) {
        // Trigger beat-reactive effects via MusicalGrid
        m_musicalGrid.onK1Beat(beatEvent.beat_in_bar, beatEvent.is_downbeat, beatEvent.strength);
    }
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
            plugins::IEffect* effect = m_effects[update.effectId].effect;
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
#ifndef NATIVE_BUILD
    // Initialize FastLED for dual strips
    m_ctrl1 = &FastLED.addLeds<WS2812, LedConfig::STRIP1_PIN, GRB>(
        m_strip1, LedConfig::LEDS_PER_STRIP);
    m_ctrl2 = &FastLED.addLeds<WS2812, LedConfig::STRIP2_PIN, GRB>(
        m_strip2, LedConfig::LEDS_PER_STRIP);

    // Configure FastLED
    FastLED.setBrightness(m_brightness);
    FastLED.setCorrection(TypicalLEDStrip);
    FastLED.setDither(1);  // Temporal dithering
    FastLED.setMaxRefreshRate(0, true);  // Non-blocking
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 3000);  // 5V / 3A limit

    // Start with all LEDs off
    FastLED.clear(true);

    LW_LOGI("FastLED initialized: 2x%d LEDs on pins %d/%d",
             LedConfig::LEDS_PER_STRIP, LedConfig::STRIP1_PIN, LedConfig::STRIP2_PIN);
#endif
}

void RendererActor::renderFrame()
{
#if FEATURE_AUDIO_SYNC
    applyPendingAudioContractTuning();
    // Drain K1 tempo/beat events from lock-free queues
    processK1Updates();
#endif
    applyPendingEffectParameterUpdates();

    // EXCLUSIVE MODE: If transition active, ONLY update transition
    // v1 pattern: effect OR transition, never both
    if (m_transitionEngine && m_transitionEngine->isActive()) {
        m_transitionEngine->update();
        m_hue += 1;
        return;  // Skip all effect rendering
    }

    // Check if zone composer is enabled
    if (m_zoneComposer != nullptr && m_zoneComposer->isEnabled()) {
        // Use ZoneComposer for multi-zone rendering
        m_zoneComposer->render(m_leds, LedConfig::TOTAL_LEDS,
                               &m_currentPalette, m_hue, m_frameCount);
        m_hue += 1;
        return;
    }

    // Single-effect mode
    // Check if we have a valid effect
    if (m_currentEffect >= MAX_EFFECTS || !m_effects[m_currentEffect].active) {
        // No effect - clear buffer
        memset(m_leds, 0, sizeof(m_leds));
        return;
    }

    // Calculate delta time (in ms)
    uint32_t now = micros();
    uint32_t deltaTimeMs;
    if (now >= m_lastFrameTime) {
        deltaTimeMs = (now - m_lastFrameTime) / 1000;
    } else {
        deltaTimeMs = ((UINT32_MAX - m_lastFrameTime) + now) / 1000;
    }

    // IEffect-only path (all effects are IEffect instances)
    if (m_effects[m_currentEffect].effect != nullptr) {
        plugins::EffectContext ctx;
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
        // Use frame count as proxy for total time (millis() not always available)
        ctx.totalTimeMs = m_frameCount * 8;  // ~8ms per frame at 120 FPS
        ctx.deltaTimeMs = deltaTimeMs;
        ctx.zoneId = 0xFF;  // Global render
        ctx.zoneStart = 0;
        ctx.zoneLength = 0;

        // =====================================================================
        // Phase 2: Audio Context Integration
        // =====================================================================
#if FEATURE_AUDIO_SYNC
        if (m_controlBusBuffer != nullptr) {
            // 1. Read latest ControlBusFrame BY VALUE (thread-safe)
            uint32_t seq = m_controlBusBuffer->ReadLatest(m_lastControlBus);

            // 2. Extrapolate AudioTime from audio snapshot
            uint64_t now_us = micros();
            if (seq != m_lastControlBusSeq) {
                // New audio frame arrived - resync extrapolation base
                m_lastAudioTime = m_lastControlBus.t;
                m_lastAudioMicros = now_us;
                m_lastControlBusSeq = seq;
            }

            // 3. Build extrapolated render-time AudioTime
            //    Render runs at 120 FPS, audio at 62.5 Hz - extrapolate samples
            uint64_t dt_us = now_us - m_lastAudioMicros;
            uint64_t extrapolated_samples = m_lastAudioTime.sample_index +
                (dt_us * m_lastAudioTime.sample_rate_hz / 1000000);
            audio::AudioTime render_now(
                extrapolated_samples,
                m_lastAudioTime.sample_rate_hz,
                now_us
            );

            // 4. Tick MusicalGrid at 120 FPS (PLL freewheel pattern)
            //    This keeps beat phase smooth even if audio stalls
            m_musicalGrid.Tick(render_now);
            m_musicalGrid.ReadLatest(m_lastMusicalGrid);

            // 5. Compute staleness for ctx.audio.available
            // FIXED: More robust freshness detection combining sequence-based and age-based checks
            // - Allow small negative ages (-0.01s tolerance) to account for extrapolation timing precision
            // - If sequence changed, data is definitely fresh (new audio frame arrived)
            // - Otherwise, check if age is within staleness threshold
            float age_s = audio::AudioTime_SecondsBetween(m_lastControlBus.t, render_now);
            float staleness_s = m_audioContractTuning.audioStalenessMs / 1000.0f;
            bool sequence_changed = (seq != m_lastControlBusSeq);
            bool age_within_tolerance = (age_s >= -0.01f && age_s < staleness_s);
            bool is_fresh = sequence_changed || age_within_tolerance;

            // 6. Populate AudioContext (by-value copies for thread safety)
            ctx.audio.controlBus = m_lastControlBus;
            ctx.audio.musicalGrid = m_lastMusicalGrid;
            ctx.audio.available = is_fresh;
        } else {
            // AudioActor not connected - effects should fall back to time-based
            ctx.audio.available = false;
        }
#else
        // Audio sync disabled at compile time
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
                m_currentEffect,
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

        m_effects[m_currentEffect].effect->render(ctx);
    }

    // Increment hue for effects that use it
    m_hue += 1;  // Slow rotation
}

void RendererActor::showLeds()
{
#ifndef NATIVE_BUILD
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

    // TAP C: Capture pre-WS2812 (after strip split, before FastLED.show)
    if (m_captureEnabled && (m_captureTapMask & 0x04)) {
        // Interleave strip1 and strip2 into unified format for capture
        for (uint16_t i = 0; i < LedConfig::LEDS_PER_STRIP; i++) {
            m_captureTapC[i] = m_strip1[i];
            m_captureTapC[i + LedConfig::LEDS_PER_STRIP] = m_strip2[i];
        }
        captureFrame(CaptureTap::TAP_C_PRE_WS2812, m_captureTapC);
    }

    // Push to hardware
    FastLED.show();
#endif
}

void RendererActor::updateStats(uint32_t frameTimeUs)
{
    m_stats.framesRendered++;

    // Check for frame drop (exceeded budget)
    if (frameTimeUs > LedConfig::FRAME_TIME_US) {
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
        uint8_t oldEffect = m_currentEffect;

        // Cleanup old effect
        if (m_effects[oldEffect].effect != nullptr) {
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

#ifndef NATIVE_BUILD
        FastLED.setBrightness(m_brightness);
#endif
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
    // Clamp to valid palette range
    if (paletteIndex >= MASTER_PALETTE_COUNT) {
        paletteIndex = paletteIndex % MASTER_PALETTE_COUNT;
    }

    if (m_paletteIndex != paletteIndex) {
        m_paletteIndex = paletteIndex;

        // Load palette from master palette array (75 palettes)
        m_currentPalette = gMasterPalettes[paletteIndex];

        // Apply color correction for WHITE_HEAVY palettes
        uint8_t flags = master_palette_flags[paletteIndex];
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

void RendererActor::startTransition(uint8_t newEffectId, uint8_t transitionType)
{
    if (!m_transitionEngine) return;
    if (newEffectId >= MAX_EFFECTS || !m_effects[newEffectId].active) return;
    if (transitionType >= static_cast<uint8_t>(TransitionType::TYPE_COUNT)) {
        transitionType = 0;  // Default to FADE
    }

    // Copy current LED state as source
    memcpy(m_transitionSourceBuffer, m_leds, sizeof(m_transitionSourceBuffer));

    // Switch to new effect
    m_currentEffect = newEffectId;

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
             getEffectName(m_currentEffect),
             getEffectName(newEffectId),
             getTransitionName(type));
}

void RendererActor::startRandomTransition(uint8_t newEffectId)
{
    TransitionType type = TransitionEngine::getRandomTransition();
    startTransition(newEffectId, static_cast<uint8_t>(type));
}

bool RendererActor::isTransitionActive() const
{
    return m_transitionEngine && m_transitionEngine->isActive();
}

} // namespace actors
} // namespace lightwaveos
