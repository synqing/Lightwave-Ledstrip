#ifndef SPECTRA_AUDIO_INTERFACE_H
#define SPECTRA_AUDIO_INTERFACE_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
// Assuming L_common_audio_defs.h is accessible in the include path.
// If not, the path might need adjustment (e.g., "../Lightwave_AudioPipeline/L_common_audio_defs.h")
// For now, let's assume it can be found directly.
#include "L_common_audio_defs.h" // For audio_features_s3_t

// Define the queue handle globally or provide a getter
extern QueueHandle_t g_audio_features_queue;

/**
 * @brief Initializes the audio interface, primarily creating the FreeRTOS queue.
 *
 * @param queue_length The maximum number of items the queue can hold.
 * @param item_size The size of each item in the queue (should be sizeof(audio_features_s3_t)).
 * @return esp_err_t ESP_OK on success, or an error code if queue creation fails.
 */
esp_err_t spectra_audio_interface_init(uint32_t queue_length, uint32_t item_size);

/**
 * @brief Sends audio features from the producing core (Core 0) to the queue.
 *
 * @param features Pointer to the audio_features_s3_t structure to send.
 * @param timeout Ticks to wait if the queue is full.
 * @return pdPASS if successful, errQUEUE_FULL if the queue is full.
 */
BaseType_t spectra_audio_interface_send(const audio_features_s3_t* features, TickType_t timeout);

/**
 * @brief Receives audio features on the consuming core (Core 1) from the queue.
 *
 * @param features Pointer to an audio_features_s3_t structure to fill with received data.
 * @param timeout Ticks to wait if the queue is empty.
 * @return pdPASS if successful, errQUEUE_EMPTY if the queue is empty.
 */
BaseType_t spectra_audio_interface_receive(audio_features_s3_t* features, TickType_t timeout);

/**
 * @brief Deinitializes the audio interface, primarily deleting the FreeRTOS queue.
 */
void spectra_audio_interface_deinit(void);

#endif // SPECTRA_AUDIO_INTERFACE_H 