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

using namespace lightwaveos::transitions;
using namespace lightwaveos::palettes;

// Stub for legacy effect ID tracking - no-op when legacy effects are disabled
// When legacy/LegacyEffectWrapper is re-enabled, this will be replaced by the real implementation
namespace lightwaveos { namespace actors {
    void setCurrentLegacyEffectId(uint8_t) {}  // No-op stub
}}

#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <esp_log.h>

static const char* TAG = "Renderer";
#endif

namespace lightwaveos {
namespace actors {

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

#ifndef NATIVE_BUILD
    ESP_LOGD(TAG, "Registered effect %d: %s (legacy -> IEffect adapter)", id, name);
#endif

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

#ifndef NATIVE_BUILD
    ESP_LOGD(TAG, "Registered effect %d: %s (IEffect native)", id, meta.name);
#endif

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
#ifndef NATIVE_BUILD
    ESP_LOGI(TAG, "Initializing LEDs on Core %d", xPortGetCoreID());
#endif

    initLeds();

    // Subscribe to relevant events
    bus::MessageBus::instance().subscribe(MessageType::PALETTE_CHANGED, this);

    // Record start time
    m_lastFrameTime = micros();

#ifndef NATIVE_BUILD
    ESP_LOGI(TAG, "Ready - %d effects, brightness=%d, target=%d FPS",
             m_effectCount, m_brightness, LedConfig::TARGET_FPS);
#endif
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

    // Post-render color correction pipeline (skip for LGP-sensitive and stateful effects)
    // LGP-sensitive effects rely on precise amplitude relationships for interference patterns
    // Stateful effects read previous frame state, and correction mutates buffer in-place
    bool isLGPSensitive = ::PatternRegistry::isLGPSensitive(m_currentEffect);
    bool isStateful = ::PatternRegistry::isStatefulEffect(m_currentEffect);
    
    if (!isLGPSensitive && !isStateful) {
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
#ifndef NATIVE_BUILD
    ESP_LOGI(TAG, "Stopping - rendered %lu frames, %lu drops",
             m_stats.framesRendered, m_stats.frameDrops);
#endif

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
    
    ESP_LOGI("Renderer", "Capture mode %s (tapMask=0x%02X)", 
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

    ESP_LOGI(TAG, "FastLED initialized: 2x%d LEDs on pins %d/%d",
             LedConfig::LEDS_PER_STRIP, LedConfig::STRIP1_PIN, LedConfig::STRIP2_PIN);
#endif
}

void RendererActor::renderFrame()
{
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
        ctx.frameNumber = m_frameCount;
        // Use frame count as proxy for total time (millis() not always available)
        ctx.totalTimeMs = m_frameCount * 8;  // ~8ms per frame at 120 FPS
        ctx.deltaTimeMs = deltaTimeMs;
        ctx.zoneId = 0xFF;  // Global render
        ctx.zoneStart = 0;
        ctx.zoneLength = 0;
        
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
#ifndef NATIVE_BUILD
        ESP_LOGW(TAG, "Invalid effect ID: %d", effectId);
#endif
        return;
    }

    if (m_currentEffect != effectId) {
        uint8_t oldEffect = m_currentEffect;
        
        // Cleanup old effect
        if (m_effects[oldEffect].effect != nullptr) {
#ifndef NATIVE_BUILD
            ESP_LOGI(TAG, "IEffect cleanup: %s (ID %d)", m_effects[oldEffect].name, oldEffect);
#endif
            m_effects[oldEffect].effect->cleanup();
        }
        
        m_currentEffect = effectId;
        
        // Initialize new effect
        if (m_effects[effectId].effect != nullptr) {
#ifndef NATIVE_BUILD
            ESP_LOGI(TAG, "IEffect init: %s (ID %d)", m_effects[effectId].name, effectId);
#endif
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
#ifndef NATIVE_BUILD
                ESP_LOGW(TAG, "IEffect %d init failed, reverting to %d", effectId, oldEffect);
#endif
                return;
            }
#ifndef NATIVE_BUILD
            ESP_LOGI(TAG, "IEffect init: SUCCESS");
#endif
        }

#ifndef NATIVE_BUILD
        ESP_LOGI(TAG, "Effect changed: %d (%s) -> %d (%s)",
                 oldEffect, getEffectName(oldEffect),
                 effectId, getEffectName(effectId));
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
        ESP_LOGD(TAG, "Brightness: %d", m_brightness);
#endif
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

#ifndef NATIVE_BUILD
        ESP_LOGD(TAG, "Speed: %d", m_speed);
#endif
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

#ifndef NATIVE_BUILD
        ESP_LOGD(TAG, "Palette: %d (%s)", m_paletteIndex, getPaletteName(m_paletteIndex));
#endif

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
#ifndef NATIVE_BUILD
        ESP_LOGD(TAG, "Intensity: %d", m_intensity);
#endif
    }
}

void RendererActor::handleSetSaturation(uint8_t saturation)
{
    if (m_saturation != saturation) {
        m_saturation = saturation;
#ifndef NATIVE_BUILD
        ESP_LOGD(TAG, "Saturation: %d", m_saturation);
#endif
    }
}

void RendererActor::handleSetComplexity(uint8_t complexity)
{
    if (m_complexity != complexity) {
        m_complexity = complexity;
#ifndef NATIVE_BUILD
        ESP_LOGD(TAG, "Complexity: %d", m_complexity);
#endif
    }
}

void RendererActor::handleSetVariation(uint8_t variation)
{
    if (m_variation != variation) {
        m_variation = variation;
#ifndef NATIVE_BUILD
        ESP_LOGD(TAG, "Variation: %d", m_variation);
#endif
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

#ifndef NATIVE_BUILD
    ESP_LOGI(TAG, "Transition started: %s -> %s (%s)",
             getEffectName(m_currentEffect),
             getEffectName(newEffectId),
             getTransitionName(type));
#endif
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
