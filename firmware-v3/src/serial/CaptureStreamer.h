#pragma once

/**
 * CaptureStreamer — serial binary frame streaming for LED capture diagnostics.
 *
 * Extracted from main.cpp (Phase 2 decomposition).  Owns all capture state,
 * the async FreeRTOS producer task, the esp_timer pacing, and the CLI
 * subcommand dispatch for `capture *` commands.
 *
 * The streamer pushes binary frames over Serial (USB CDC) at a configurable
 * FPS.  Two execution paths exist:
 *   - Async (preferred): dedicated FreeRTOS task on Core 0, paced by esp_timer.
 *   - Sync (fallback):   called from loop() when the async task fails to start.
 *
 * Frame format versions:
 *   v1 — 977 bytes  (17-byte header + 960 RGB)
 *   v2 — 1009 bytes (17-byte header + 960 RGB + 32-byte metrics trailer)
 *   v3 — 49 bytes   (17-byte header + 32-byte metrics, no RGB)
 *   v4 — 529 bytes  (17-byte header + 480 half-res RGB + 32-byte metrics)
 */

#include <Arduino.h>

#ifndef NATIVE_BUILD
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

#include <FastLED.h>  // CRGB

// Forward declarations — avoid pulling heavy headers into every translation unit.
namespace lightwaveos::actors { class RendererActor; }

namespace lightwaveos::serial {

class CaptureStreamer {
public:
    CaptureStreamer() = default;

    /// Allocate PSRAM scratch buffers.  Call once from setup().
    void init();

    /// Pass RendererActor* after the actor system starts.
    void setRenderer(lightwaveos::actors::RendererActor* ren) { m_renderer = ren; }

    /// Sync-fallback tick — call from loop() every iteration.
    /// @param nowUs  Current value of micros() (computed once in the caller).
    void tick(uint32_t nowUs);

    /// Process a `capture ...` serial command.  Returns true if the input
    /// was recognised as a capture command and consumed.
    bool handleCommand(const String& input);

    /// True when the streamer is actively pushing frames.
    bool isActive() const { return m_streamActive; }

    /// Pointer to the dump-frame scratch buffer (shared with main.cpp's
    /// one-shot `capture dump` and loop-level validation).
    CRGB* getDumpFrameScratch() const { return m_dumpFrameScratch; }

    /// Pointer to the pre-assembled frame buffer (PSRAM when available).
    uint8_t* getFrameBuf() const { return m_frameBuf; }

private:
    // ── Frame assembly (shared by async + sync paths) ────────────────
    /// Build a complete binary capture frame into m_frameBuf.
    /// Returns the number of bytes written (0 on failure).
    size_t assembleFrame(CRGB* frame,
                         lightwaveos::actors::RendererActor* ren);

    // ── FreeRTOS task + esp_timer ────────────────────────────────────
#ifndef NATIVE_BUILD
    static void captureTimerCallbackISR(void* arg);
    static void captureProducerTaskFn(void* param);
#endif

    // ── Renderer reference ───────────────────────────────────────────
    lightwaveos::actors::RendererActor* m_renderer = nullptr;

    // ── Streaming state ──────────────────────────────────────────────
    bool     m_streamActive      = false;
    uint8_t  m_streamTapMask     = 0x02;  // default: tap B
    uint8_t  m_streamVersion     = 2;     // 1/2/3/4
    uint32_t m_streamIntervalUs  = 66667; // ~15 FPS (microseconds)
    uint32_t m_streamLastPushUs  = 0;
    uint32_t m_streamLastFrameIdx = UINT32_MAX;
    uint32_t m_streamDropped     = 0;

    // ── Write instrumentation ────────────────────────────────────────
    uint32_t m_writeMaxUs        = 0;
    uint32_t m_writeTotalUs      = 0;
    uint32_t m_writeCount        = 0;
    uint16_t m_availMin          = UINT16_MAX;
    uint32_t m_backpressureSkips = 0;
    uint32_t m_assembleMaxUs     = 0;
    uint32_t m_assembleTotalUs   = 0;

    // ── Capture tap (typed enum from RendererActor) ──────────────────
    // Stored as uint8_t to avoid pulling RendererActor.h into the header.
    uint8_t  m_streamTapRaw      = 2;  // CaptureTap::TAP_B_POST_CORRECTION

    // ── FreeRTOS task state ──────────────────────────────────────────
#ifndef NATIVE_BUILD
    TaskHandle_t       m_taskHandle  = nullptr;
    volatile bool      m_taskRunning = false;
    esp_timer_handle_t m_timer       = nullptr;
#endif

    // ── Buffers ──────────────────────────────────────────────────────
    // Pre-assembled frame buffer — allocated in PSRAM when available.
    static constexpr size_t kFrameBufSize = 1024;
    uint8_t  m_frameBufFallback[kFrameBufSize] = {};
    uint8_t* m_frameBuf = m_frameBufFallback;

    // Dump-frame scratch (320 LEDs) — shared with the `capture dump` CLI.
    static constexpr uint16_t kLedCount = 320;
    CRGB     m_dumpFrameFallback[kLedCount] = {};
    CRGB*    m_dumpFrameScratch = m_dumpFrameFallback;

    // Dedicated CRGB buffer for the async capture task (avoids racing
    // with the dump command which uses m_dumpFrameScratch).
    CRGB     m_taskFrameBuf[kLedCount] = {};

    bool     m_initialised = false;
};

}  // namespace lightwaveos::serial
