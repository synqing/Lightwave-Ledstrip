/**
 * @file sr_probe.h
 * @brief ESP-SR proof-of-build probe
 *
 * Minimal integration that proves ESP-SR compiles, links, and can
 * enumerate models from flash. No audio processing or model instantiation.
 * Gated by ESP_SR_ENABLED build flag.
 */
#pragma once

#ifdef ESP_SR_ENABLED

#include <esp_log.h>
#include "model_path.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_mn_speech_commands.h"
#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"

static const char *SR_TAG = "SR_PROBE";

/**
 * @brief Probe ESP-SR model partition and log available models.
 *
 * Call once at boot after Serial.begin(). Does NOT create any model
 * instances or allocate significant memory. Just reads the model
 * partition header and logs what's available.
 *
 * @return true if model partition was readable and models were found
 */
inline bool sr_probe_models() {
    ESP_LOGI(SR_TAG, "=== ESP-SR Proof-of-Build Probe ===");

    // Init model list from the "model" flash partition
    srmodel_list_t *models = esp_srmodel_init("model");
    if (!models) {
        ESP_LOGE(SR_TAG, "esp_srmodel_init(\"model\") FAILED — check partition table and srmodels.bin");
        return false;
    }

    ESP_LOGI(SR_TAG, "Model partition readable. Found %d model(s):", models->num);
    for (int i = 0; i < models->num; i++) {
        ESP_LOGI(SR_TAG, "  [%d] %s", i, models->model_name[i]);
        if (models->model_info && models->model_info[i]) {
            ESP_LOGI(SR_TAG, "       info: %s", models->model_info[i]);
        }
    }

    // Check for MultiNet English model
    char *mn_name = esp_srmodel_filter(models, ESP_MN_PREFIX, ESP_MN_ENGLISH);
    if (mn_name) {
        ESP_LOGI(SR_TAG, "MultiNet English model: %s", mn_name);
    } else {
        ESP_LOGW(SR_TAG, "No MultiNet English model found in partition");
    }

    // Check for WakeNet model (informational — we use microWakeWord instead)
    char *wn_name = esp_srmodel_filter(models, "wn", NULL);
    if (wn_name) {
        ESP_LOGI(SR_TAG, "WakeNet model: %s (NOT used — microWakeWord is our wake path)", wn_name);
    }

    // Get MultiNet handle (just to verify linkage — don't create instance)
    if (mn_name) {
        esp_mn_iface_t *mn_iface = esp_mn_handle_from_name(mn_name);
        if (mn_iface) {
            ESP_LOGI(SR_TAG, "MultiNet interface handle: OK (create/detect/destroy vtable linked)");
        } else {
            ESP_LOGE(SR_TAG, "esp_mn_handle_from_name() returned NULL");
        }
    }

    ESP_LOGI(SR_TAG, "=== ESP-SR probe complete — all symbols resolved ===");

    // Clean up — free model list (doesn't unload models from flash)
    esp_srmodel_deinit(models);
    return true;
}

#endif // ESP_SR_ENABLED
