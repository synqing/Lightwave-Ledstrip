/**
 * @file sr_command_window.h
 * @brief Gate 4A — post-wake command recognition window
 *
 * Two commands: LIGHTS OFF (id=0), BRIGHTER (id=1)
 * 3-second single-command window, exclusive with wake detection.
 *
 * Flow: wake detected → stop wake feed → create AFE+MN+commands →
 *       start SR feed+detect tasks → wait for command/timeout →
 *       destroy SR → resume wake feed
 *
 * Serial: "sr listen" to trigger manually (bypasses wake word)
 *
 * Gated by ESP_SR_ENABLED build flag.
 */
#pragma once

#ifdef ESP_SR_ENABLED

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <driver/i2s_std.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "model_path.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_mn_speech_commands.h"
#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "esp_afe_config.h"

static const char *CMD_TAG = "SR_CMD";

// ============================================================================
// Command definitions
// ============================================================================
enum SrCommandId : int {
    CMD_LIGHTS_OFF = 0,
    CMD_BRIGHTER   = 1,
};

static const char *kCommandNames[] = {
    "lights off",
    "brighter",
};
static const int kNumCommands = 2;

// ============================================================================
// Visual state callback
// ============================================================================
enum CmdVisualState {
    CMD_VIS_WAKE_ACK,     // Immediate wake acknowledgement
    CMD_VIS_LISTENING,    // SR ready, listening for command
    CMD_VIS_SUCCESS,      // Command recognised
    CMD_VIS_TIMEOUT,      // No command detected
    CMD_VIS_ERROR,        // Something went wrong
    CMD_VIS_IDLE,         // Back to normal wake listening
};

// ============================================================================
// Command window configuration (set by main.cpp)
// ============================================================================
struct CmdWindowConfig {
    bool (*stopFeed)(int timeoutMs);
    bool (*startFeed)();
    i2s_chan_handle_t *rxHandle;
    volatile bool *feedRunning;
    volatile int *feedsPerSec;
    void (*showVisual)(CmdVisualState state, const char *detail);
    void (*onCommand)(int commandId);    // action callback
};

static CmdWindowConfig s_cmdConfig = {};

inline void sr_command_setConfig(const CmdWindowConfig &cfg) {
    s_cmdConfig = cfg;
}

// ============================================================================
// Internal shared state for SR tasks
// ============================================================================
struct CmdWindowState {
    const esp_afe_sr_iface_t *afe_handle;
    esp_afe_sr_data_t *afe_data;
    esp_mn_iface_t *multinet;
    model_iface_data_t *mn_data;
    i2s_chan_handle_t rxHandle;

    volatile bool running;          // Tasks should keep running
    volatile bool commandDetected;
    volatile bool timedOut;
    volatile bool error;
    volatile bool feedDone;         // Feed task exited cleanly
    volatile bool detectDone;       // Detect task exited cleanly

    int commandId;
    float confidence;

    // Timestamps (microseconds)
    int64_t wakeTs;
    int64_t readyTs;
    int64_t commandTs;
    int64_t teardownTs;
    int64_t resumeTs;
};

static CmdWindowState s_cmdState = {};

// ============================================================================
// SR Feed Task — reads I2S, converts, feeds AFE
// ============================================================================
static void srFeedTask(void *arg) {
    CmdWindowState *st = (CmdWindowState *)arg;

    int feedChunk = st->afe_handle->get_feed_chunksize(st->afe_data);
    int totalCh = 2;  // 1 mic + 1 ref (our AFE config)

    ESP_LOGI(CMD_TAG, "[feed] chunksize=%d samples, channels=%d", feedChunk, totalCh);

    // Allocate buffers
    int32_t *rawBuf = (int32_t *)heap_caps_malloc(feedChunk * sizeof(int32_t), MALLOC_CAP_INTERNAL);
    int16_t *interBuf = (int16_t *)heap_caps_malloc(feedChunk * totalCh * sizeof(int16_t), MALLOC_CAP_INTERNAL);

    if (!rawBuf || !interBuf) {
        ESP_LOGE(CMD_TAG, "[feed] malloc FAIL");
        st->error = true;
        if (rawBuf) heap_caps_free(rawBuf);
        if (interBuf) heap_caps_free(interBuf);
        vTaskDelete(NULL);
        return;
    }

    // Drain stale I2S data (DMA buffers may have old audio)
    for (int d = 0; d < 6; d++) {
        size_t discard = 0;
        i2s_channel_read(st->rxHandle, rawBuf, feedChunk * sizeof(int32_t),
                         &discard, pdMS_TO_TICKS(50));
    }

    while (st->running) {
        // Read mono I2S (32-bit samples, right slot)
        size_t bytesRead = 0;
        esp_err_t err = i2s_channel_read(st->rxHandle, rawBuf,
                                          feedChunk * sizeof(int32_t),
                                          &bytesRead, pdMS_TO_TICKS(200));
        if (err != ESP_OK || bytesRead == 0) {
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }

        int samplesRead = bytesRead / sizeof(int32_t);

        // Convert int32 → int16 and interleave: [mic, ref=0]
        for (int i = 0; i < samplesRead && i < feedChunk; i++) {
            interBuf[i * totalCh + 0] = (int16_t)(rawBuf[i] >> 14);  // mic
            interBuf[i * totalCh + 1] = 0;                            // ref (no AEC)
        }
        // Zero-fill if short read
        for (int i = samplesRead; i < feedChunk; i++) {
            interBuf[i * totalCh + 0] = 0;
            interBuf[i * totalCh + 1] = 0;
        }

        // Feed AFE
        st->afe_handle->feed(st->afe_data, interBuf);
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    heap_caps_free(rawBuf);
    heap_caps_free(interBuf);
    UBaseType_t feedHWM = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI(CMD_TAG, "[feed] task exiting (stack HWM: %u bytes free)", (unsigned)(feedHWM * sizeof(StackType_t)));
    st->feedDone = true;
    vTaskDelete(NULL);
}

// ============================================================================
// SR Detect Task — fetches from AFE, runs MultiNet
// ============================================================================
static void srDetectTask(void *arg) {
    CmdWindowState *st = (CmdWindowState *)arg;

    ESP_LOGI(CMD_TAG, "[detect] task started");

    while (st->running) {
        afe_fetch_result_t *res = st->afe_handle->fetch(st->afe_data);
        if (!res || res->ret_value == ESP_FAIL || !res->data) {
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }

        // Run MultiNet detection
        esp_mn_state_t mn_state = st->multinet->detect(st->mn_data, res->data);

        if (mn_state == ESP_MN_STATE_DETECTED) {
            esp_mn_results_t *mn_result = st->multinet->get_results(st->mn_data);
            if (mn_result && mn_result->num > 0) {
                st->commandId = mn_result->command_id[0];
                st->confidence = mn_result->prob[0];
                st->commandTs = esp_timer_get_time();
                st->commandDetected = true;
                st->running = false;

                ESP_LOGI(CMD_TAG, "[detect] COMMAND: id=%d conf=%.2f name='%s'",
                         st->commandId, st->confidence,
                         (st->commandId < kNumCommands) ? kCommandNames[st->commandId] : "?");
            }
        } else if (mn_state == ESP_MN_STATE_TIMEOUT) {
            st->commandTs = esp_timer_get_time();
            st->timedOut = true;
            st->running = false;
            ESP_LOGI(CMD_TAG, "[detect] TIMEOUT — no command recognised");
        }
        // ESP_MN_STATE_DETECTING — still accumulating audio, continue
    }

    UBaseType_t detectHWM = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI(CMD_TAG, "[detect] task exiting (stack HWM: %u bytes free)", (unsigned)(detectHWM * sizeof(StackType_t)));
    st->detectDone = true;
    vTaskDelete(NULL);
}

// ============================================================================
// Command window orchestrator
// ============================================================================
inline void runCommandWindow() {
    if (!s_cmdConfig.stopFeed || !s_cmdConfig.startFeed ||
        !s_cmdConfig.rxHandle || !s_cmdConfig.showVisual) {
        ESP_LOGE(CMD_TAG, "Config not set — call sr_command_setConfig first");
        return;
    }

    // ---- Timestamps ----
    s_cmdState = {};
    s_cmdState.wakeTs = esp_timer_get_time();

    ESP_LOGI(CMD_TAG, "=== Command Window Opening ===");

    // 1. Immediate visual acknowledgement
    s_cmdConfig.showVisual(CMD_VIS_WAKE_ACK, NULL);

    // 2. Stop wake feed task
    if (!s_cmdConfig.stopFeed(3000)) {
        ESP_LOGE(CMD_TAG, "Feed task stop FAILED");
        s_cmdConfig.showVisual(CMD_VIS_ERROR, "Feed stop failed");
        vTaskDelay(pdMS_TO_TICKS(1000));
        s_cmdConfig.showVisual(CMD_VIS_IDLE, NULL);
        return;
    }

    // 3. Create AFE + MultiNet
    srmodel_list_t *models = esp_srmodel_init("model");
    if (!models) {
        ESP_LOGE(CMD_TAG, "esp_srmodel_init FAILED");
        s_cmdConfig.startFeed();
        s_cmdConfig.showVisual(CMD_VIS_ERROR, "Model init failed");
        vTaskDelay(pdMS_TO_TICKS(1000));
        s_cmdConfig.showVisual(CMD_VIS_IDLE, NULL);
        return;
    }

    char *mn_name = esp_srmodel_filter(models, ESP_MN_PREFIX, ESP_MN_ENGLISH);
    esp_mn_iface_t *multinet = esp_mn_handle_from_name(mn_name);

    afe_config_t cfg = AFE_CONFIG_DEFAULT();
    cfg.wakenet_init = false;
    cfg.aec_init = false;
    cfg.pcm_config.total_ch_num = 2;
    cfg.pcm_config.mic_num = 1;
    cfg.pcm_config.ref_num = 1;
    cfg.pcm_config.sample_rate = 16000;
    cfg.memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;

    const esp_afe_sr_iface_t *afe_handle = &ESP_AFE_SR_HANDLE;
    esp_afe_sr_data_t *afe_data = afe_handle->create_from_config(&cfg);
    if (!afe_data) {
        ESP_LOGE(CMD_TAG, "AFE create FAILED");
        esp_srmodel_deinit(models);
        s_cmdConfig.startFeed();
        s_cmdConfig.showVisual(CMD_VIS_ERROR, "AFE create failed");
        vTaskDelay(pdMS_TO_TICKS(1000));
        s_cmdConfig.showVisual(CMD_VIS_IDLE, NULL);
        return;
    }

    // 3-second command window
    model_iface_data_t *mn_data = multinet->create(mn_name, 3000);
    if (!mn_data) {
        ESP_LOGE(CMD_TAG, "MultiNet create FAILED");
        afe_handle->destroy(afe_data);
        esp_srmodel_deinit(models);
        s_cmdConfig.startFeed();
        s_cmdConfig.showVisual(CMD_VIS_ERROR, "MN create failed");
        vTaskDelay(pdMS_TO_TICKS(1000));
        s_cmdConfig.showVisual(CMD_VIS_IDLE, NULL);
        return;
    }

    // 4. Register commands (auto-G2P)
    esp_err_t alloc_err = esp_mn_commands_alloc(multinet, mn_data);
    if (alloc_err != ESP_OK) {
        ESP_LOGE(CMD_TAG, "esp_mn_commands_alloc FAILED: %s", esp_err_to_name(alloc_err));
        multinet->destroy(mn_data);
        afe_handle->destroy(afe_data);
        esp_srmodel_deinit(models);
        s_cmdConfig.startFeed();
        s_cmdConfig.showVisual(CMD_VIS_ERROR, "Cmd alloc failed");
        vTaskDelay(pdMS_TO_TICKS(1000));
        s_cmdConfig.showVisual(CMD_VIS_IDLE, NULL);
        return;
    }
    for (int i = 0; i < kNumCommands; i++) {
        esp_err_t err = esp_mn_commands_add(i, kCommandNames[i]);
        ESP_LOGI(CMD_TAG, "Command %d '%s': %s", i, kCommandNames[i],
                 (err == ESP_OK) ? "OK" : "FAIL");
    }
    esp_mn_error_t *mn_err = esp_mn_commands_update();
    if (mn_err) {
        ESP_LOGW(CMD_TAG, "Some commands failed to parse — check phonemes");
    }

    // 5. Set up shared state
    s_cmdState.afe_handle = afe_handle;
    s_cmdState.afe_data = afe_data;
    s_cmdState.multinet = multinet;
    s_cmdState.mn_data = mn_data;
    s_cmdState.rxHandle = *s_cmdConfig.rxHandle;
    s_cmdState.running = true;
    s_cmdState.commandDetected = false;
    s_cmdState.timedOut = false;
    s_cmdState.error = false;

    s_cmdState.readyTs = esp_timer_get_time();
    int64_t entryLatencyUs = s_cmdState.readyTs - s_cmdState.wakeTs;
    ESP_LOGI(CMD_TAG, "SR ready in %lld ms — listening...", (long long)(entryLatencyUs / 1000));

    // 6. Show listening visual
    s_cmdConfig.showVisual(CMD_VIS_LISTENING, NULL);

    // 7. Start SR tasks
    TaskHandle_t feedHandle = NULL;
    TaskHandle_t detectHandle = NULL;
    xTaskCreatePinnedToCore(srFeedTask, "sr_feed", 8192, &s_cmdState, 5, &feedHandle, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    xTaskCreatePinnedToCore(srDetectTask, "sr_detect", 12288, &s_cmdState, 5, &detectHandle, 1);

    // 8. Wait for result (command, timeout, or safety timeout)
    uint32_t safetyTimeout = millis() + 5000;  // Hard safety cap
    while (s_cmdState.running && !s_cmdState.error && millis() < safetyTimeout) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    // 9. Stop SR tasks
    s_cmdState.running = false;
    vTaskDelay(pdMS_TO_TICKS(300));  // Let tasks self-delete

    // Only force-delete if task did NOT exit cleanly (stuck in blocking call)
    if (!s_cmdState.feedDone && feedHandle) {
        ESP_LOGW(CMD_TAG, "Feed task did not exit — force deleting");
        vTaskDelete(feedHandle);
    }
    if (!s_cmdState.detectDone && detectHandle) {
        ESP_LOGW(CMD_TAG, "Detect task did not exit — force deleting");
        vTaskDelete(detectHandle);
    }
    vTaskDelay(pdMS_TO_TICKS(50));

    // 10. Process result
    if (s_cmdState.commandDetected) {
        int cmdId = s_cmdState.commandId;
        const char *name = (cmdId < kNumCommands) ? kCommandNames[cmdId] : "unknown";
        int64_t cmdLatencyUs = s_cmdState.commandTs - s_cmdState.readyTs;

        ESP_LOGI(CMD_TAG, "=== COMMAND: '%s' (id=%d, conf=%.2f, latency=%lld ms) ===",
                 name, cmdId, s_cmdState.confidence, (long long)(cmdLatencyUs / 1000));

        s_cmdConfig.showVisual(CMD_VIS_SUCCESS, name);

        // Execute command action
        if (s_cmdConfig.onCommand) {
            s_cmdConfig.onCommand(cmdId);
        }

        vTaskDelay(pdMS_TO_TICKS(1500));  // Hold success visual
    } else if (s_cmdState.timedOut) {
        int64_t windowUs = s_cmdState.commandTs - s_cmdState.readyTs;
        ESP_LOGI(CMD_TAG, "=== TIMEOUT after %lld ms ===", (long long)(windowUs / 1000));

        s_cmdConfig.showVisual(CMD_VIS_TIMEOUT, NULL);
        vTaskDelay(pdMS_TO_TICKS(1000));
    } else {
        ESP_LOGW(CMD_TAG, "=== Command window ended (error or safety timeout) ===");
        s_cmdConfig.showVisual(CMD_VIS_ERROR, "Window error");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // 11. Destroy SR
    s_cmdState.teardownTs = esp_timer_get_time();
    esp_mn_commands_free();
    multinet->destroy(mn_data);
    afe_handle->destroy(afe_data);
    esp_srmodel_deinit(models);

    // 12. Resume wake feed task
    if (!s_cmdConfig.startFeed()) {
        ESP_LOGE(CMD_TAG, "Feed task restart FAILED");
    }

    s_cmdState.resumeTs = esp_timer_get_time();

    // 13. Log full session timing
    ESP_LOGI(CMD_TAG, "=== Session Timing ===");
    ESP_LOGI(CMD_TAG, "  wake → ready:    %lld ms",
             (long long)((s_cmdState.readyTs - s_cmdState.wakeTs) / 1000));
    if (s_cmdState.commandDetected || s_cmdState.timedOut) {
        ESP_LOGI(CMD_TAG, "  ready → result:  %lld ms",
                 (long long)((s_cmdState.commandTs - s_cmdState.readyTs) / 1000));
    }
    ESP_LOGI(CMD_TAG, "  teardown → resume: %lld ms",
             (long long)((s_cmdState.resumeTs - s_cmdState.teardownTs) / 1000));
    ESP_LOGI(CMD_TAG, "  total session:   %lld ms",
             (long long)((s_cmdState.resumeTs - s_cmdState.wakeTs) / 1000));

    // Heap check
    size_t sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    ESP_LOGI(CMD_TAG, "  heap: SRAM %u | PSRAM %u", sram, psram);

    // 14. Back to idle
    s_cmdConfig.showVisual(CMD_VIS_IDLE, NULL);
    ESP_LOGI(CMD_TAG, "=== Command Window Closed ===");
}

// ============================================================================
// Serial command handler
// ============================================================================
inline bool sr_command_handleCommand(const String &cmd) {
    if (cmd == "sr listen") {
        runCommandWindow();
        return true;
    }
    return false;
}

#endif // ESP_SR_ENABLED
