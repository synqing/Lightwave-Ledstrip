/**
 * @file sr_lifecycle.h
 * @brief Gate 3A/3B — ESP-SR lifecycle churn tests
 *
 * Gate 3A: "sr cycle N"   — AFE + MultiNet allocation churn (no I2S)
 * Gate 3B: "sr handoff N" — full I2S teardown/reinit + AFE/MN churn
 *
 * Gated by ESP_SR_ENABLED build flag.
 */
#pragma once

#ifdef ESP_SR_ENABLED

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <esp_log.h>

#include "model_path.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "esp_afe_config.h"

static const char *SRL_TAG = "SR_LIFE";

// ============================================================================
// Gate 3B callbacks — set by main.cpp before use
// ============================================================================
struct SrLifecycleCallbacks {
    bool (*stopFeed)(int timeoutMs);     // Stop wake feed task
    bool (*startFeed)();                 // Restart wake feed task
    void (*teardownMic)();               // i2s_channel_disable + del
    bool (*initMic)();                   // Full I2S reinit
    volatile bool *feedRunning;          // Pointer to s_feedRunning
    volatile int  *feedsPerSec;          // Pointer to s_feedsPerSec
};

static SrLifecycleCallbacks s_srCallbacks = {};

inline void sr_lifecycle_setCallbacks(const SrLifecycleCallbacks &cb) {
    s_srCallbacks = cb;
}

// ============================================================================
// Shared: create AFE + MultiNet, return timings
// ============================================================================
struct SrCreateResult {
    bool ok;
    esp_afe_sr_data_t *afe_data;
    model_iface_data_t *mn_data;
    const esp_afe_sr_iface_t *afe_handle;
    esp_mn_iface_t *multinet;
    srmodel_list_t *models;
    int64_t afe_us;
    int64_t mn_us;
};

inline SrCreateResult sr_create_all() {
    SrCreateResult r = {};

    r.models = esp_srmodel_init("model");
    if (!r.models) { ESP_LOGE(SRL_TAG, "esp_srmodel_init FAILED"); return r; }

    char *mn_name = esp_srmodel_filter(r.models, ESP_MN_PREFIX, ESP_MN_ENGLISH);
    if (!mn_name) {
        ESP_LOGE(SRL_TAG, "No MultiNet English model");
        esp_srmodel_deinit(r.models); r.models = NULL; return r;
    }

    r.multinet = esp_mn_handle_from_name(mn_name);
    if (!r.multinet) {
        ESP_LOGE(SRL_TAG, "esp_mn_handle_from_name NULL");
        esp_srmodel_deinit(r.models); r.models = NULL; return r;
    }

    // Create AFE
    int64_t t1 = esp_timer_get_time();
    afe_config_t cfg = AFE_CONFIG_DEFAULT();
    cfg.wakenet_init = false;
    cfg.aec_init = false;
    cfg.pcm_config.total_ch_num = 2;
    cfg.pcm_config.mic_num = 1;
    cfg.pcm_config.ref_num = 1;
    cfg.pcm_config.sample_rate = 16000;
    cfg.memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
    r.afe_handle = &ESP_AFE_SR_HANDLE;
    r.afe_data = r.afe_handle->create_from_config(&cfg);
    int64_t t2 = esp_timer_get_time();
    r.afe_us = t2 - t1;

    if (!r.afe_data) {
        ESP_LOGE(SRL_TAG, "AFE create FAILED");
        esp_srmodel_deinit(r.models); r.models = NULL; return r;
    }

    // Create MultiNet
    int64_t t3 = esp_timer_get_time();
    r.mn_data = r.multinet->create(mn_name, 5760);
    int64_t t4 = esp_timer_get_time();
    r.mn_us = t4 - t3;

    if (!r.mn_data) {
        ESP_LOGE(SRL_TAG, "MultiNet create FAILED");
        r.afe_handle->destroy(r.afe_data); r.afe_data = NULL;
        esp_srmodel_deinit(r.models); r.models = NULL; return r;
    }

    r.ok = true;
    return r;
}

inline void sr_destroy_all(SrCreateResult &r) {
    if (r.mn_data && r.multinet) r.multinet->destroy(r.mn_data);
    if (r.afe_data && r.afe_handle) r.afe_handle->destroy(r.afe_data);
    if (r.models) esp_srmodel_deinit(r.models);
    r.mn_data = NULL;
    r.afe_data = NULL;
    r.models = NULL;
}

// ============================================================================
// Gate 3A — allocation churn only (no I2S)
// ============================================================================
inline bool sr_lifecycle_run(int numCycles) {
    ESP_LOGI(SRL_TAG, "=== Gate 3A: Lifecycle churn — %d cycle(s) ===", numCycles);

    const size_t sramBase = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const size_t psramBase = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    ESP_LOGI(SRL_TAG, "BASELINE: SRAM %u | PSRAM %u", sramBase, psramBase);

    int maxSramDelta = 0;
    int maxPsramDelta = 0;

    for (int i = 0; i < numCycles; i++) {
        size_t sramBefore = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        size_t psramBefore = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

        SrCreateResult r = sr_create_all();
        if (!r.ok) {
            ESP_LOGE(SRL_TAG, "[%d] Create FAILED — aborting", i);
            return false;
        }

        vTaskDelay(pdMS_TO_TICKS(250));

        int64_t t5 = esp_timer_get_time();
        sr_destroy_all(r);
        int64_t t6 = esp_timer_get_time();

        size_t sramAfter = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        size_t psramAfter = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

        int sramDelta = (int)sramAfter - (int)sramBefore;
        int psramDelta = (int)psramAfter - (int)psramBefore;
        if (abs(sramDelta) > abs(maxSramDelta)) maxSramDelta = sramDelta;
        if (abs(psramDelta) > abs(maxPsramDelta)) maxPsramDelta = psramDelta;

        ESP_LOGI(SRL_TAG, "[%d/%d] AFE=%lldus MN=%lldus destroy=%lldus | "
                 "SRAM %u→%u (%+d) | PSRAM %u→%u (%+d)",
                 i + 1, numCycles,
                 (long long)r.afe_us, (long long)r.mn_us,
                 (long long)(t6 - t5),
                 sramBefore, sramAfter, sramDelta,
                 psramBefore, psramAfter, psramDelta);

        vTaskDelay(pdMS_TO_TICKS(50));
    }

    size_t sramFinal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t psramFinal = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    int sramFinalDelta = (int)sramFinal - (int)sramBase;
    int psramFinalDelta = (int)psramFinal - (int)psramBase;

    ESP_LOGI(SRL_TAG, "=== SUMMARY ===");
    ESP_LOGI(SRL_TAG, "Cycles: %d | Max SRAM delta: %+d | Max PSRAM delta: %+d",
             numCycles, maxSramDelta, maxPsramDelta);
    ESP_LOGI(SRL_TAG, "Final vs baseline: SRAM %+d (%u→%u) | PSRAM %+d (%u→%u)",
             sramFinalDelta, sramBase, sramFinal,
             psramFinalDelta, psramBase, psramFinal);

    bool leak = (sramFinalDelta < -1024) || (psramFinalDelta < -4096);
    if (leak) {
        ESP_LOGW(SRL_TAG, "POSSIBLE LEAK: final heap significantly below baseline");
    } else {
        ESP_LOGI(SRL_TAG, "PASS — no significant heap ratchet detected");
    }
    ESP_LOGI(SRL_TAG, "=== Gate 3A complete ===");
    return true;
}

// ============================================================================
// Gate 3B — full I2S handoff churn
// ============================================================================
inline bool sr_handoff_run(int numCycles) {
    if (!s_srCallbacks.stopFeed || !s_srCallbacks.startFeed ||
        !s_srCallbacks.teardownMic || !s_srCallbacks.initMic ||
        !s_srCallbacks.feedRunning || !s_srCallbacks.feedsPerSec) {
        ESP_LOGE(SRL_TAG, "Gate 3B: callbacks not set — call sr_lifecycle_setCallbacks first");
        return false;
    }

    ESP_LOGI(SRL_TAG, "=== Gate 3B: I2S handoff churn — %d cycle(s) ===", numCycles);

    const size_t sramBase = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const size_t psramBase = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    ESP_LOGI(SRL_TAG, "BASELINE: SRAM %u | PSRAM %u", sramBase, psramBase);

    int maxSramDelta = 0;
    int maxPsramDelta = 0;
    int feedFailCount = 0;

    for (int i = 0; i < numCycles; i++) {
        size_t sramBefore = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        size_t psramBefore = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        int64_t cycleStart = esp_timer_get_time();

        // 1. Stop wake detector feed task
        if (!s_srCallbacks.stopFeed(3000)) {
            ESP_LOGE(SRL_TAG, "[%d] Feed task stop TIMEOUT — aborting", i);
            return false;
        }

        // 2. Tear down I2S
        int64_t tI2sDown = esp_timer_get_time();
        s_srCallbacks.teardownMic();
        int64_t tI2sDownDone = esp_timer_get_time();

        // 3. Create AFE + MultiNet
        SrCreateResult r = sr_create_all();
        if (!r.ok) {
            ESP_LOGE(SRL_TAG, "[%d] SR create FAILED — reiniting mic and aborting", i);
            s_srCallbacks.initMic();
            s_srCallbacks.startFeed();
            return false;
        }

        // 4. Dwell 500ms (simulates command listening window)
        vTaskDelay(pdMS_TO_TICKS(500));

        // 5. Destroy MultiNet + AFE
        int64_t tDestroy = esp_timer_get_time();
        sr_destroy_all(r);
        int64_t tDestroyDone = esp_timer_get_time();

        // 6. Reinit I2S
        int64_t tI2sUp = esp_timer_get_time();
        if (!s_srCallbacks.initMic()) {
            ESP_LOGE(SRL_TAG, "[%d] I2S reinit FAILED — aborting", i);
            return false;
        }
        int64_t tI2sUpDone = esp_timer_get_time();

        // 7. Resume wake detector feed task
        if (!s_srCallbacks.startFeed()) {
            ESP_LOGE(SRL_TAG, "[%d] Feed task restart FAILED — aborting", i);
            return false;
        }

        // 8. Verify feeds/s resumes (wait up to 3s for non-zero)
        bool feedRecovered = false;
        uint32_t verifyStart = millis();
        while (millis() - verifyStart < 3000) {
            vTaskDelay(pdMS_TO_TICKS(100));
            if (*s_srCallbacks.feedsPerSec > 0) {
                feedRecovered = true;
                break;
            }
        }
        if (!feedRecovered) {
            feedFailCount++;
            ESP_LOGW(SRL_TAG, "[%d] Feed rate did not recover in 3s (fail #%d)",
                     i, feedFailCount);
        }

        int64_t cycleEnd = esp_timer_get_time();

        // Record post-cycle heap
        size_t sramAfter = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        size_t psramAfter = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        int sramDelta = (int)sramAfter - (int)sramBefore;
        int psramDelta = (int)psramAfter - (int)psramBefore;
        if (abs(sramDelta) > abs(maxSramDelta)) maxSramDelta = sramDelta;
        if (abs(psramDelta) > abs(maxPsramDelta)) maxPsramDelta = psramDelta;

        ESP_LOGI(SRL_TAG, "[%d/%d] i2s_down=%lldus SR=%lldus+%lldus destroy=%lldus i2s_up=%lldus total=%lldms | "
                 "SRAM %u→%u (%+d) | PSRAM %u→%u (%+d) | feed=%s",
                 i + 1, numCycles,
                 (long long)(tI2sDownDone - tI2sDown),
                 (long long)r.afe_us, (long long)r.mn_us,
                 (long long)(tDestroyDone - tDestroy),
                 (long long)(tI2sUpDone - tI2sUp),
                 (long long)((cycleEnd - cycleStart) / 1000),
                 sramBefore, sramAfter, sramDelta,
                 psramBefore, psramAfter, psramDelta,
                 feedRecovered ? "OK" : "FAIL");

        // Brief settle between cycles
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Final summary
    size_t sramFinal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t psramFinal = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    int sramFinalDelta = (int)sramFinal - (int)sramBase;
    int psramFinalDelta = (int)psramFinal - (int)psramBase;

    ESP_LOGI(SRL_TAG, "=== SUMMARY ===");
    ESP_LOGI(SRL_TAG, "Cycles: %d | Max SRAM delta: %+d | Max PSRAM delta: %+d | Feed fails: %d",
             numCycles, maxSramDelta, maxPsramDelta, feedFailCount);
    ESP_LOGI(SRL_TAG, "Final vs baseline: SRAM %+d (%u→%u) | PSRAM %+d (%u→%u)",
             sramFinalDelta, sramBase, sramFinal,
             psramFinalDelta, psramBase, psramFinal);

    bool leak = (sramFinalDelta < -1024) || (psramFinalDelta < -4096);
    bool feedOk = (feedFailCount == 0);
    if (leak) {
        ESP_LOGW(SRL_TAG, "POSSIBLE LEAK: final heap significantly below baseline");
    }
    if (!feedOk) {
        ESP_LOGW(SRL_TAG, "FEED RECOVERY FAILURES: %d/%d cycles", feedFailCount, numCycles);
    }
    if (!leak && feedOk) {
        ESP_LOGI(SRL_TAG, "PASS — no heap ratchet, all feed recoveries OK");
    }
    ESP_LOGI(SRL_TAG, "=== Gate 3B complete ===");
    return !leak && feedOk;
}

// ============================================================================
// Serial command dispatcher
// ============================================================================
inline bool sr_lifecycle_handleCommand(const String &cmd) {
    if (cmd.startsWith("sr cycle ")) {
        int n = cmd.substring(9).toInt();
        if (n < 1 || n > 1000) {
            Serial.println("[SR_LIFE] Usage: sr cycle N (1-1000)");
            return true;
        }
        sr_lifecycle_run(n);
        return true;
    }
    if (cmd.startsWith("sr handoff ")) {
        int n = cmd.substring(11).toInt();
        if (n < 1 || n > 1000) {
            Serial.println("[SR_LIFE] Usage: sr handoff N (1-1000)");
            return true;
        }
        sr_handoff_run(n);
        return true;
    }
    return false;
}

#endif // ESP_SR_ENABLED
