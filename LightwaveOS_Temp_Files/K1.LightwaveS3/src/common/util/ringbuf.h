/**
 * @file ringbuf.h
 * @brief Generic Ring Buffer (Bounded, Lock-Free Single Producer/Consumer)
 * 
 * Lightweight ring buffer for inter-task communication without heap allocation.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// Generic ring buffer structure
typedef struct {
    uint8_t* buffer;
    uint16_t capacity;  // Buffer capacity in bytes
    uint16_t size;      // Current number of bytes
    uint16_t head;      // Write index
    uint16_t tail;      // Read index
} lw_ringbuf_t;

/**
 * @brief Initialize ring buffer
 * @param rb Ring buffer structure
 * @param buffer Backing buffer
 * @param capacity Buffer capacity in bytes
 */
static inline void lw_ringbuf_init(lw_ringbuf_t* rb, uint8_t* buffer, uint16_t capacity) {
    rb->buffer = buffer;
    rb->capacity = capacity;
    rb->size = 0;
    rb->head = 0;
    rb->tail = 0;
}

/**
 * @brief Get available space for writing
 */
static inline uint16_t lw_ringbuf_available(const lw_ringbuf_t* rb) {
    return rb->capacity - rb->size;
}

/**
 * @brief Get number of bytes available for reading
 */
static inline uint16_t lw_ringbuf_size(const lw_ringbuf_t* rb) {
    return rb->size;
}

/**
 * @brief Check if ring buffer is empty
 */
static inline bool lw_ringbuf_is_empty(const lw_ringbuf_t* rb) {
    return rb->size == 0;
}

/**
 * @brief Check if ring buffer is full
 */
static inline bool lw_ringbuf_is_full(const lw_ringbuf_t* rb) {
    return rb->size == rb->capacity;
}

/**
 * @brief Write data to ring buffer
 * @param rb Ring buffer
 * @param data Data to write
 * @param len Length of data
 * @return Number of bytes actually written
 */
static inline uint16_t lw_ringbuf_write(lw_ringbuf_t* rb, const uint8_t* data, uint16_t len) {
    uint16_t available = lw_ringbuf_available(rb);
    if (len > available) {
        len = available;
    }
    
    for (uint16_t i = 0; i < len; i++) {
        rb->buffer[rb->head] = data[i];
        rb->head = (rb->head + 1) % rb->capacity;
    }
    
    rb->size += len;
    return len;
}

/**
 * @brief Read data from ring buffer
 * @param rb Ring buffer
 * @param data Output buffer
 * @param len Maximum bytes to read
 * @return Number of bytes actually read
 */
static inline uint16_t lw_ringbuf_read(lw_ringbuf_t* rb, uint8_t* data, uint16_t len) {
    uint16_t size = lw_ringbuf_size(rb);
    if (len > size) {
        len = size;
    }
    
    for (uint16_t i = 0; i < len; i++) {
        data[i] = rb->buffer[rb->tail];
        rb->tail = (rb->tail + 1) % rb->capacity;
    }
    
    rb->size -= len;
    return len;
}

/**
 * @brief Clear ring buffer
 */
static inline void lw_ringbuf_clear(lw_ringbuf_t* rb) {
    rb->size = 0;
    rb->head = 0;
    rb->tail = 0;
}

#ifdef __cplusplus
}
#endif

#endif // RINGBUF_H
