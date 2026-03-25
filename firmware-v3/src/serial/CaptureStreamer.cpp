/**
 * CaptureStreamer — implementation.
 *
 * Extracted from main.cpp (Phase 2 decomposition).  All capture streaming
 * globals, the async FreeRTOS task, esp_timer pacing, frame assembly, and
 * `capture *` CLI dispatch live here.
 */

#include "CaptureStreamer.h"

#include "core/actors/RendererActor.h"
#include "audio/contracts/ControlBus.h"

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#include <esp_system.h>
#endif

#define LW_LOG_TAG "Capture"
#include "utils/Log.h"

using CaptureTap     = lightwaveos::actors::RendererActor::CaptureTap;
using BandsSnapshot  = lightwaveos::actors::RendererActor::BandsDebugSnapshot;

namespace lightwaveos::serial {

// ── Helper: convert raw uint8_t tap value to typed enum ──────────────
static inline CaptureTap tapFromRaw(uint8_t raw) {
    return static_cast<CaptureTap>(raw);
}

// ══════════════════════════════════════════════════════════════════════
// init — allocate PSRAM scratch buffers
// ══════════════════════════════════════════════════════════════════════

void CaptureStreamer::init() {
    if (m_initialised) return;
    m_initialised = true;

#if !defined(NATIVE_BUILD) && defined(BOARD_HAS_PSRAM)
    if (auto* capture = static_cast<CRGB*>(
            heap_caps_calloc(kLedCount, sizeof(CRGB),
                             MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT))) {
        m_dumpFrameScratch = capture;
    }
    if (auto* frameBuf = static_cast<uint8_t*>(
            heap_caps_calloc(kFrameBufSize, 1,
                             MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT))) {
        m_frameBuf = frameBuf;
    }
#endif

    LW_LOGI("Capture buffers: dumpFrame=%s frameBuf=%s",
            (m_dumpFrameScratch != m_dumpFrameFallback) ? "PSRAM" : "DRAM",
            (m_frameBuf != m_frameBufFallback) ? "PSRAM" : "DRAM");
}

// ══════════════════════════════════════════════════════════════════════
// assembleFrame — build a binary capture frame into m_frameBuf
// ══════════════════════════════════════════════════════════════════════

size_t CaptureStreamer::assembleFrame(
        CRGB* frame,
        lightwaveos::actors::RendererActor* ren) {
    uint8_t* buf = m_frameBuf;
    size_t pos = 0;

    // Determine RGB payload size based on version/mode:
    //   v1/v2: full 960B (320 LEDs x 3)
    //   v3: metadata-only, 0B RGB
    //   v4: slim, 480B (160 LEDs x 3, every other LED)
    uint16_t rgbLen;
    if (m_streamVersion == 3) {
        rgbLen = 0;
    } else if (m_streamVersion == 4) {
        rgbLen = 160 * 3;  // 480 bytes
    } else {
        rgbLen = 320 * 3;  // 960 bytes
    }

    auto metadata = ren->getCaptureMetadata();

    // Header (17 bytes)
    buf[pos++] = 0xFD;
    buf[pos++] = m_streamVersion;
    buf[pos++] = m_streamTapRaw;
    buf[pos++] = metadata.effectId;
    buf[pos++] = metadata.paletteId;
    buf[pos++] = metadata.brightness;
    buf[pos++] = metadata.speed;
    buf[pos++] = (uint8_t)(metadata.frameIndex & 0xFF);
    buf[pos++] = (uint8_t)((metadata.frameIndex >> 8) & 0xFF);
    buf[pos++] = (uint8_t)((metadata.frameIndex >> 16) & 0xFF);
    buf[pos++] = (uint8_t)((metadata.frameIndex >> 24) & 0xFF);
    buf[pos++] = (uint8_t)(metadata.timestampUs & 0xFF);
    buf[pos++] = (uint8_t)((metadata.timestampUs >> 8) & 0xFF);
    buf[pos++] = (uint8_t)((metadata.timestampUs >> 16) & 0xFF);
    buf[pos++] = (uint8_t)((metadata.timestampUs >> 24) & 0xFF);
    buf[pos++] = (uint8_t)(rgbLen & 0xFF);
    buf[pos++] = (uint8_t)((rgbLen >> 8) & 0xFF);

    // RGB payload
    if (m_streamVersion == 4) {
        // Slim: sample every other LED
        const uint8_t* src = (const uint8_t*)frame;
        for (int i = 0; i < 320; i += 2) {
            buf[pos++] = src[i * 3];
            buf[pos++] = src[i * 3 + 1];
            buf[pos++] = src[i * 3 + 2];
        }
    } else if (rgbLen > 0) {
        memcpy(&buf[pos], (uint8_t*)frame, rgbLen);
        pos += rgbLen;
    }

    // v2+ metrics trailer (32 bytes)
    if (m_streamVersion >= 2) {
        auto quantiseUnitFloatU16 = [](float value) -> uint16_t {
            if (!(value > 0.0f)) return 0;
            if (value >= 1.0f) return 65535;
            return static_cast<uint16_t>(value * 65535.0f + 0.5f);
        };

        auto& ledStats = ren->getLedDriverStats();
        BandsSnapshot bandsSnap;
        ren->getBandsDebugSnapshot(bandsSnap);

        lightwaveos::audio::ControlBusFrame cbf;
        ren->copyCachedAudioFrame(cbf);

        // [0:2] showTimeUs (u16 LE)
        uint16_t showUs = (ledStats.lastShowUs > 65535) ? 65535 : (uint16_t)ledStats.lastShowUs;
        buf[pos++] = (uint8_t)(showUs & 0xFF);
        buf[pos++] = (uint8_t)((showUs >> 8) & 0xFF);

        // [2:4] rms (u16 LE, float * 65535)
        uint16_t rmsU16 = (uint16_t)(bandsSnap.rms * 65535.0f);
        buf[pos++] = (uint8_t)(rmsU16 & 0xFF);
        buf[pos++] = (uint8_t)((rmsU16 >> 8) & 0xFF);

        // [4:12] bands[8] (u8 each, float * 255)
        for (int i = 0; i < 8; i++) {
            float v = bandsSnap.bands[i];
            buf[pos++] = (uint8_t)(v > 1.0f ? 255 : (uint8_t)(v * 255.0f));
        }

        // [12] beatTick (u8)
        bool beat = cbf.tempoBeatTick || cbf.es_beat_tick;
        buf[pos++] = beat ? 1 : 0;

        // [13] onsetTick (u8)
        bool onset = cbf.onsetEvent > 0.0f;
        buf[pos++] = onset ? 1 : 0;

        // [14:16] flux (u16 LE, float * 65535)
        uint16_t fluxU16 = (uint16_t)(bandsSnap.flux * 65535.0f);
        buf[pos++] = (uint8_t)(fluxU16 & 0xFF);
        buf[pos++] = (uint8_t)((fluxU16 >> 8) & 0xFF);

        // [16:20] heapFree (u32 LE)
#ifndef NATIVE_BUILD
        uint32_t heapFree = esp_get_free_heap_size();
#else
        uint32_t heapFree = 0;
#endif
        buf[pos++] = (uint8_t)(heapFree & 0xFF);
        buf[pos++] = (uint8_t)((heapFree >> 8) & 0xFF);
        buf[pos++] = (uint8_t)((heapFree >> 16) & 0xFF);
        buf[pos++] = (uint8_t)((heapFree >> 24) & 0xFF);

        // [20:22] showSkips (u16 LE)
        uint16_t skips = (ledStats.showSkips > 65535) ? 65535 : (uint16_t)ledStats.showSkips;
        buf[pos++] = (uint8_t)(skips & 0xFF);
        buf[pos++] = (uint8_t)((skips >> 8) & 0xFF);

        // [22:24] bpm (u16 LE, BPM * 100)
        float bpmVal = (cbf.es_bpm > 0.0f && cbf.es_bpm != 120.0f)
                     ? cbf.es_bpm : cbf.tempoBpm;
        uint16_t bpmU16 = (uint16_t)(bpmVal * 100.0f);
        buf[pos++] = (uint8_t)(bpmU16 & 0xFF);
        buf[pos++] = (uint8_t)((bpmU16 >> 8) & 0xFF);

        // [24:26] beatConfidence (u16 LE, float * 65535)
        float conf = (cbf.es_tempo_confidence > 0.0f)
                   ? cbf.es_tempo_confidence : cbf.tempoConfidence;
        if (conf > 1.0f) conf = 1.0f;
        uint16_t confU16 = (uint16_t)(conf * 65535.0f);
        buf[pos++] = (uint8_t)(confU16 & 0xFF);
        buf[pos++] = (uint8_t)((confU16 >> 8) & 0xFF);

        // [26:28] onsetEnv (u16 LE, float * 65535, clamped 0..1)
        uint16_t onsetEnvU16 = quantiseUnitFloatU16(cbf.onsetEnv);
        buf[pos++] = (uint8_t)(onsetEnvU16 & 0xFF);
        buf[pos++] = (uint8_t)((onsetEnvU16 >> 8) & 0xFF);

        // [28:30] onsetEvent (u16 LE, float * 65535, clamped 0..1)
        uint16_t onsetEventU16 = quantiseUnitFloatU16(cbf.onsetEvent);
        buf[pos++] = (uint8_t)(onsetEventU16 & 0xFF);
        buf[pos++] = (uint8_t)((onsetEventU16 >> 8) & 0xFF);

        // [30] onset trigger bits (bit0=kick, bit1=snare, bit2=hihat)
        uint8_t onsetBits = 0;
        if (cbf.kickTrigger)  onsetBits |= 0x01;
        if (cbf.snareTrigger) onsetBits |= 0x02;
        if (cbf.hihatTrigger) onsetBits |= 0x04;
        buf[pos++] = onsetBits;

        // [31] onsetProcessUs quantised to 16 us steps (0..4080 us)
        uint16_t onsetProcessUs = cbf.onsetProcessUs;
        if (onsetProcessUs > 4080) onsetProcessUs = 4080;
        buf[pos++] = static_cast<uint8_t>((onsetProcessUs + 8) / 16);
    }

    return pos;
}

// ══════════════════════════════════════════════════════════════════════
// Async capture path — FreeRTOS task + esp_timer ISR
// ══════════════════════════════════════════════════════════════════════

#ifndef NATIVE_BUILD

void IRAM_ATTR CaptureStreamer::captureTimerCallbackISR(void* arg) {
    auto* self = static_cast<CaptureStreamer*>(arg);
    TaskHandle_t h = self->m_taskHandle;
    if (h) {
        xTaskNotifyGive(h);
    }
}

void CaptureStreamer::captureProducerTaskFn(void* param) {
    auto* self = static_cast<CaptureStreamer*>(param);
    auto* ren  = self->m_renderer;
    CaptureTap tap = tapFromRaw(self->m_streamTapRaw);

    uint32_t writeCount    = 0;
    uint32_t writeMaxUs    = 0;
    uint32_t writeTotalUs  = 0;
    uint32_t assembleMaxUs = 0;
    uint32_t assembleTotalUs = 0;
    uint16_t availMin      = UINT16_MAX;
    uint32_t txDropped     = 0;
    uint32_t lastFrameIdx  = UINT32_MAX;

    while (self->m_taskRunning) {
        // Block until the esp_timer fires a notification.
        // Timeout after 100ms to check the running flag.
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));

        if (!self->m_taskRunning) break;

        CRGB* frame = self->m_taskFrameBuf;
        if (!ren->getCapturedFrame(tap, frame)) {
            continue;
        }

        auto metadata = ren->getCaptureMetadata();
        if (metadata.frameIndex == lastFrameIdx) {
            continue;
        }
        lastFrameIdx = metadata.frameIndex;

        // --- Assemble frame into bulk buffer ---
        uint32_t assembleStart = micros();
        size_t pos = self->assembleFrame(frame, ren);
        uint32_t assembleDur = micros() - assembleStart;
        assembleTotalUs += assembleDur;
        if (assembleDur > assembleMaxUs) assembleMaxUs = assembleDur;

        // TX
        uint16_t avail = Serial.availableForWrite();
        if (avail < availMin) availMin = avail;

        uint32_t writeStart = micros();
        size_t written = Serial.write(self->m_frameBuf, pos);
        uint32_t writeDur = micros() - writeStart;

        writeCount++;
        writeTotalUs += writeDur;
        if (writeDur > writeMaxUs) writeMaxUs = writeDur;

        if (written < pos) txDropped++;
    }

    // Copy final stats to members for reporting.
    self->m_writeCount      = writeCount;
    self->m_writeMaxUs      = writeMaxUs;
    self->m_writeTotalUs    = writeTotalUs;
    self->m_assembleMaxUs   = assembleMaxUs;
    self->m_assembleTotalUs = assembleTotalUs;
    self->m_availMin        = availMin;
    self->m_streamDropped   = txDropped;

    self->m_taskHandle = nullptr;
    vTaskDelete(nullptr);
}

#endif  // !NATIVE_BUILD

// ══════════════════════════════════════════════════════════════════════
// tick — sync fallback capture (called from loop())
// ══════════════════════════════════════════════════════════════════════

void CaptureStreamer::tick(uint32_t nowUs) {
#ifndef NATIVE_BUILD
    if (!m_streamActive || m_taskHandle || !m_renderer) return;
#else
    if (!m_streamActive || !m_renderer) return;
#endif

    if (nowUs - m_streamLastPushUs < m_streamIntervalUs) return;

    CRGB* frame = m_dumpFrameScratch;
    CaptureTap tap = tapFromRaw(m_streamTapRaw);
    if (!m_renderer->getCapturedFrame(tap, frame)) return;

    auto metadata = m_renderer->getCaptureMetadata();
    // Only push if this is a new frame (avoid duplicates)
    if (metadata.frameIndex == m_streamLastFrameIdx) return;
    m_streamLastFrameIdx = metadata.frameIndex;
    m_streamLastPushUs   = nowUs;

    // --- Assemble frame into bulk buffer ---
    uint32_t assembleStart = micros();
    size_t pos = assembleFrame(frame, m_renderer);
    uint32_t assembleDur = micros() - assembleStart;
    m_assembleTotalUs += assembleDur;
    if (assembleDur > m_assembleMaxUs) m_assembleMaxUs = assembleDur;

    // Record TX buffer state before write.
    uint16_t avail = Serial.availableForWrite();
    if (avail < m_availMin) m_availMin = avail;

    // Instrumented blocking bulk write.
    uint32_t writeStart = micros();
    size_t written = Serial.write(m_frameBuf, pos);
    uint32_t writeDur = micros() - writeStart;

    m_writeCount++;
    m_writeTotalUs += writeDur;
    if (writeDur > m_writeMaxUs) m_writeMaxUs = writeDur;

    if (written < pos) {
        m_streamDropped++;
    }
}

// ══════════════════════════════════════════════════════════════════════
// handleCommand — dispatch `capture *` CLI subcommands
// ══════════════════════════════════════════════════════════════════════

bool CaptureStreamer::handleCommand(const String& input) {
    String lower = input;
    lower.toLowerCase();
    if (!lower.startsWith("capture")) return false;

    // Ack what we received
    Serial.printf("[CAPTURE] recv='%s'\n", input.c_str());

    String subcmd = lower.substring(7);  // after "capture"
    subcmd.trim();

    using RendererActor = lightwaveos::actors::RendererActor;

    // ── capture off ──────────────────────────────────────────────────
    if (subcmd == "off") {
        if (m_renderer) m_renderer->setCaptureMode(false, 0);
        Serial.println("Capture mode disabled");
    }
    // ── capture on [abc] ─────────────────────────────────────────────
    else if (subcmd.startsWith("on")) {
        uint8_t tapMask = 0x07;  // default all taps
        if (subcmd.length() > 2) {
            tapMask = 0;
            if (subcmd.indexOf('a') >= 0) tapMask |= 0x01;
            if (subcmd.indexOf('b') >= 0) tapMask |= 0x02;
            if (subcmd.indexOf('c') >= 0) tapMask |= 0x04;
        }
        if (m_renderer) m_renderer->setCaptureMode(true, tapMask);
        Serial.printf("Capture mode enabled (tapMask=0x%02X: %s%s%s)\n",
                      tapMask,
                      (tapMask & 0x01) ? "A" : "",
                      (tapMask & 0x02) ? "B" : "",
                      (tapMask & 0x04) ? "C" : "");
    }
    // ── capture dump <a|b|c> ─────────────────────────────────────────
    else if (subcmd.startsWith("dump")) {
        CaptureTap tap;
        bool valid = false;

        if (subcmd.indexOf('a') >= 0) { tap = CaptureTap::TAP_A_PRE_CORRECTION; valid = true; }
        else if (subcmd.indexOf('b') >= 0) { tap = CaptureTap::TAP_B_POST_CORRECTION; valid = true; }
        else if (subcmd.indexOf('c') >= 0) { tap = CaptureTap::TAP_C_PRE_WS2812; valid = true; }

        if (valid && m_renderer) {
            CRGB* frame = m_dumpFrameScratch;
            // If we have no captured frame yet, force a one-shot capture and retry.
            if (!m_renderer->getCapturedFrame(tap, frame)) {
                m_renderer->forceOneShotCapture(tap);
                delay(10);  // allow capture to complete
            }

            if (m_renderer->getCapturedFrame(tap, frame)) {
                auto metadata = m_renderer->getCaptureMetadata();

                Serial.write(0xFD);  // Magic
                Serial.write(0x01);  // Version
                Serial.write((uint8_t)tap);
                Serial.write(metadata.effectId);
                Serial.write(metadata.paletteId);
                Serial.write(metadata.brightness);
                Serial.write(metadata.speed);
                Serial.write((uint8_t)(metadata.frameIndex & 0xFF));
                Serial.write((uint8_t)((metadata.frameIndex >> 8) & 0xFF));
                Serial.write((uint8_t)((metadata.frameIndex >> 16) & 0xFF));
                Serial.write((uint8_t)((metadata.frameIndex >> 24) & 0xFF));
                Serial.write((uint8_t)(metadata.timestampUs & 0xFF));
                Serial.write((uint8_t)((metadata.timestampUs >> 8) & 0xFF));
                Serial.write((uint8_t)((metadata.timestampUs >> 16) & 0xFF));
                Serial.write((uint8_t)((metadata.timestampUs >> 24) & 0xFF));
                uint16_t frameLen = 320 * 3;
                Serial.write((uint8_t)(frameLen & 0xFF));
                Serial.write((uint8_t)((frameLen >> 8) & 0xFF));

                // Payload
                Serial.write((uint8_t*)frame, frameLen);

                Serial.printf("\nFrame dumped: tap=%d, effect=%d, palette=%d, frame=%u\n",
                              (int)tap, metadata.effectId, metadata.paletteId, (unsigned int)metadata.frameIndex);
            } else {
                Serial.println("No frame captured for this tap");
            }
        } else if (!valid) {
            Serial.println("Usage: capture dump <a|b|c>");
        }
    }
    // ── capture stream <a|b|c> [fps] ─────────────────────────────────
    else if (subcmd.startsWith("stream")) {
        String streamArgs = subcmd.substring(6);
        streamArgs.trim();

        CaptureTap tap = CaptureTap::TAP_B_POST_CORRECTION;
        uint8_t tapMask = 0x02;
        int targetFps = 15;

        if (streamArgs.indexOf('a') >= 0) { tap = CaptureTap::TAP_A_PRE_CORRECTION; tapMask = 0x01; }
        else if (streamArgs.indexOf('c') >= 0) { tap = CaptureTap::TAP_C_PRE_WS2812; tapMask = 0x04; }

        // Parse optional FPS after tap letter
        int spaceIdx = streamArgs.indexOf(' ');
        if (spaceIdx >= 0) {
            int parsed = streamArgs.substring(spaceIdx + 1).toInt();
            if (parsed >= 1 && parsed <= 60) targetFps = parsed;
        }

        // Enable capture mode for the requested tap
        if (m_renderer) m_renderer->setCaptureMode(true, tapMask);

        m_streamTapRaw       = (uint8_t)tap;
        m_streamTapMask      = tapMask;
        m_streamIntervalUs   = 1000000 / targetFps;
        m_streamLastPushUs   = 0;
        m_streamLastFrameIdx = UINT32_MAX;
        m_streamDropped      = 0;
        m_writeMaxUs         = 0;
        m_writeTotalUs       = 0;
        m_writeCount         = 0;
        m_availMin           = UINT16_MAX;
        m_backpressureSkips  = 0;
        m_assembleMaxUs      = 0;
        m_assembleTotalUs    = 0;
        m_streamActive       = true;

#ifndef NATIVE_BUILD
        // Path B: launch dedicated capture task on Core 0.
        m_taskRunning = true;
        BaseType_t taskOk = xTaskCreatePinnedToCore(
            captureProducerTaskFn,
            "captureTx",
            4096,           // stack (frame buf is member, not on stack)
            this,           // param — CaptureStreamer*
            2,              // priority (above loopTask=1, below audio)
            &m_taskHandle,
            0               // Core 0
        );
        if (taskOk != pdPASS) {
            m_taskRunning = false;
            m_taskHandle = nullptr;
            Serial.println("[CAPTURE] WARN: task create failed, using sync fallback");
        }

        // Create esp_timer for microsecond-resolution pacing.
        if (m_taskHandle && !m_timer) {
            esp_timer_create_args_t timerArgs = {};
            timerArgs.callback = captureTimerCallbackISR;
            timerArgs.arg = this;
            timerArgs.name = "capTimer";
            if (esp_timer_create(&timerArgs, &m_timer) == ESP_OK) {
                esp_timer_start_periodic(m_timer, m_streamIntervalUs);
            }
        }

        Serial.printf("[CAPTURE] Streaming tap %c at %d FPS (%d us interval) [%s]\n",
                      (tapMask & 0x01) ? 'A' : (tapMask & 0x04) ? 'C' : 'B',
                      targetFps, (int)m_streamIntervalUs,
                      m_taskHandle ? "async+timer" : "sync");
#else
        Serial.printf("[CAPTURE] Streaming tap %c at %d FPS (%d us interval) [sync]\n",
                      (tapMask & 0x01) ? 'A' : (tapMask & 0x04) ? 'C' : 'B',
                      targetFps, (int)m_streamIntervalUs);
#endif
    }
    // ── capture stop ─────────────────────────────────────────────────
    else if (subcmd == "stop") {
#ifndef NATIVE_BUILD
        // Stop timer first (prevents further notifications).
        if (m_timer) {
            esp_timer_stop(m_timer);
            esp_timer_delete(m_timer);
            m_timer = nullptr;
        }
        // Signal task to stop and wait for it to flush stats.
        if (m_taskHandle) {
            m_taskRunning = false;
            xTaskNotifyGive(m_taskHandle);
            for (int w = 0; w < 50 && m_taskHandle; w++) {
                delay(10);
            }
        }
#endif
        m_streamActive = false;
        uint32_t avgWriteUs = m_writeCount ? (m_writeTotalUs / m_writeCount) : 0;
        uint32_t avgAssemUs = m_writeCount ? (m_assembleTotalUs / m_writeCount) : 0;
        Serial.printf("[CAPTURE] Stopped: %lu writes, write max/avg %lu/%lu us, "
                      "assemble max/avg %lu/%lu us, "
                      "min_avail %u B, bp_skip %lu, tx_drop %lu\n",
                      (unsigned long)m_writeCount,
                      (unsigned long)m_writeMaxUs,
                      (unsigned long)avgWriteUs,
                      (unsigned long)m_assembleMaxUs,
                      (unsigned long)avgAssemUs,
                      (unsigned)m_availMin,
                      (unsigned long)m_backpressureSkips,
                      (unsigned long)m_streamDropped);
    }
    // ── capture fps <1-60> ───────────────────────────────────────────
    else if (subcmd.startsWith("fps")) {
        String fpsArg = subcmd.substring(3);
        fpsArg.trim();
        int newFps = fpsArg.toInt();
        if (newFps >= 1 && newFps <= 60) {
            m_streamIntervalUs = 1000000 / newFps;
#ifndef NATIVE_BUILD
            if (m_timer) {
                esp_timer_stop(m_timer);
                esp_timer_start_periodic(m_timer, m_streamIntervalUs);
            }
#endif
            Serial.printf("[CAPTURE] FPS set to %d (%d us interval)\n", newFps, (int)m_streamIntervalUs);
        } else {
            Serial.println("Usage: capture fps <1-60>");
        }
    }
    // ── capture tap <a|b|c> ──────────────────────────────────────────
    else if (subcmd.startsWith("tap")) {
        String tapArg = subcmd.substring(3);
        tapArg.trim();
        if (tapArg == "a") {
            m_streamTapRaw  = (uint8_t)CaptureTap::TAP_A_PRE_CORRECTION;
            m_streamTapMask = 0x01;
        } else if (tapArg == "b") {
            m_streamTapRaw  = (uint8_t)CaptureTap::TAP_B_POST_CORRECTION;
            m_streamTapMask = 0x02;
        } else if (tapArg == "c") {
            m_streamTapRaw  = (uint8_t)CaptureTap::TAP_C_PRE_WS2812;
            m_streamTapMask = 0x04;
        } else {
            Serial.println("Usage: capture tap <a|b|c>");
            tapArg = "";  // mark invalid
        }
        if (tapArg.length() > 0) {
            if (m_renderer && m_renderer->isCaptureModeEnabled()) {
                m_renderer->setCaptureMode(true, m_streamTapMask);
            }
            Serial.printf("[CAPTURE] Tap switched to %s\n", tapArg.c_str());
        }
    }
    // ── capture format <v1|v2|meta|slim> ─────────────────────────────
    else if (subcmd.startsWith("format")) {
        String fmtArg = subcmd.substring(6);
        fmtArg.trim();
        if (fmtArg == "v1" || fmtArg == "1") {
            m_streamVersion = 1;
            Serial.println("[CAPTURE] Format set to v1 (977 bytes, no metrics)");
        } else if (fmtArg == "v2" || fmtArg == "2") {
            m_streamVersion = 2;
            Serial.println("[CAPTURE] Format set to v2 (1009 bytes, with metrics)");
        } else if (fmtArg == "meta" || fmtArg == "v3" || fmtArg == "3") {
            m_streamVersion = 3;
            Serial.println("[CAPTURE] Format set to v3/meta (49 bytes, metrics only)");
        } else if (fmtArg == "slim" || fmtArg == "v4" || fmtArg == "4") {
            m_streamVersion = 4;
            Serial.println("[CAPTURE] Format set to v4/slim (529 bytes, half-res RGB)");
        } else {
            Serial.println("Usage: capture format <v1|v2|meta|slim>");
        }
    }
    // ── capture status ───────────────────────────────────────────────
    else if (subcmd == "status") {
        uint32_t avgWriteUs = m_writeCount ? (m_writeTotalUs / m_writeCount) : 0;
        uint32_t avgAssemUs = m_writeCount ? (m_assembleTotalUs / m_writeCount) : 0;
        Serial.println("\n=== Capture Status ===");
        if (m_renderer) {
            Serial.printf("  Enabled: %s\n", m_renderer->isCaptureModeEnabled() ? "YES" : "NO");
        }
        Serial.printf("  Streaming: %s (v%d, %d us interval, %s)\n",
                      m_streamActive ? "YES" : "NO",
                      m_streamVersion, (int)m_streamIntervalUs,
#ifndef NATIVE_BUILD
                      m_taskHandle ? "async" : "sync");
#else
                      "sync");
#endif
        Serial.printf("  Write: %lu, max/avg %lu/%lu us\n",
                      (unsigned long)m_writeCount,
                      (unsigned long)m_writeMaxUs,
                      (unsigned long)avgWriteUs);
        Serial.printf("  Assemble: max/avg %lu/%lu us\n",
                      (unsigned long)m_assembleMaxUs,
                      (unsigned long)avgAssemUs);
        Serial.printf("  Min avail: %u B, bp_skip: %lu, tx_drop: %lu\n",
                      (unsigned)m_availMin,
                      (unsigned long)m_backpressureSkips,
                      (unsigned long)m_streamDropped);
        if (m_renderer) {
            auto metadata = m_renderer->getCaptureMetadata();
            Serial.printf("  Last capture: effect=%d, palette=%d, frame=%lu\n",
                          metadata.effectId, metadata.paletteId, metadata.frameIndex);
        }
        Serial.println();
    }
    // ── unknown subcommand ───────────────────────────────────────────
    else {
        Serial.println("Usage: capture <on|off|dump|stream|stop|fps|tap|format|status>");
    }

    return true;
}

}  // namespace lightwaveos::serial
