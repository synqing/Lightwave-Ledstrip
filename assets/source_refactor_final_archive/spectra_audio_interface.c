#include "spectra_audio_interface.h"
#include "esp_log.h" // For logging

// Static tag for logging
static const char* TAG_AUDIO_INTERFACE = "SpectraAudioIF";

// Definition of the queue handle
QueueHandle_t g_audio_features_queue = NULL;

esp_err_t spectra_audio_interface_init(uint32_t queue_length, uint32_t item_size) {
    if (item_size == 0 || queue_length == 0) {
        ESP_LOGE(TAG_AUDIO_INTERFACE, "Queue length and item size must be non-zero.");
        return ESP_ERR_INVALID_ARG;
    }
    g_audio_features_queue = xQueueCreate(queue_length, item_size);
    if (g_audio_features_queue == NULL) {
        ESP_LOGE(TAG_AUDIO_INTERFACE, "Failed to create audio features queue.");
        return ESP_FAIL; // Or a more specific error like ESP_ERR_NO_MEM
    }
    ESP_LOGI(TAG_AUDIO_INTERFACE, "Audio features queue created. Length: %lu, ItemSize: %lu",
             (unsigned long)queue_length, (unsigned long)item_size);
    return ESP_OK;
}

BaseType_t spectra_audio_interface_send(const audio_features_s3_t* features, TickType_t timeout) {
    if (g_audio_features_queue == NULL) {
        ESP_LOGE(TAG_AUDIO_INTERFACE, "Audio features queue not initialized for send.");
        return pdFAIL;
    }
    if (features == NULL) {
        ESP_LOGE(TAG_AUDIO_INTERFACE, "Cannot send NULL features.");
        return pdFAIL;
    }
    return xQueueSend(g_audio_features_queue, features, timeout);
}

BaseType_t spectra_audio_interface_receive(audio_features_s3_t* features, TickType_t timeout) {
    if (g_audio_features_queue == NULL) {
        ESP_LOGE(TAG_AUDIO_INTERFACE, "Audio features queue not initialized for receive.");
        return pdFAIL;
    }
    if (features == NULL) {
        ESP_LOGE(TAG_AUDIO_INTERFACE, "Cannot receive into NULL features buffer.");
        return pdFAIL;
    }
    return xQueueReceive(g_audio_features_queue, features, timeout);
}

void spectra_audio_interface_deinit(void) {
    if (g_audio_features_queue != NULL) {
        vQueueDelete(g_audio_features_queue);
        g_audio_features_queue = NULL;
        ESP_LOGI(TAG_AUDIO_INTERFACE, "Audio features queue deinitialized.");
    }
} 