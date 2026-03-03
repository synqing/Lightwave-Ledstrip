// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
#pragma once
// ============================================================================
// SimEventQueue - MPSC Ring Buffer for Encoder Simulation Events
// ============================================================================
// Thread-safe multi-producer single-consumer queue for injecting encoder
// events from Serial commands and REST API handlers into the main loop.
//
// Producers (any thread):  enqueue() / enqueueBatch()  — portMUX guarded
// Consumer (main loop):    dequeue()                    — no lock needed
//
// Queue overflow: enqueue returns false. Callers must handle rejection.
// ============================================================================

#include <cstdint>
#include <freertos/portmacro.h>

enum class SimEventType : uint8_t {
    SET,    // Set encoder to absolute value
    DELTA,  // Apply delta to current value
    PRESS   // Simulate button press (reset)
};

struct SimEvent {
    SimEventType type;
    uint8_t index;       // encoder 0-15
    int16_t value;       // absolute value (SET) or delta (DELTA), unused for PRESS
    bool persist;        // true = allow NVS save, false = suppress
    uint32_t batchId;    // batch tracking (0 = no batch)
};

class SimEventQueue {
public:
    static constexpr size_t CAPACITY = 32;

    /**
     * Thread-safe enqueue (portMUX guarded).
     * @return false if queue is full
     */
    bool enqueue(const SimEvent& event) {
        portENTER_CRITICAL(&_mux);
        uint8_t nextHead = (_head + 1) % CAPACITY;
        if (nextHead == _tail) {
            portEXIT_CRITICAL(&_mux);
            return false;  // Full
        }
        _buffer[_head] = event;
        _head = nextHead;
        portEXIT_CRITICAL(&_mux);
        return true;
    }

    /**
     * Atomic batch enqueue: all-or-nothing.
     * Pre-checks capacity, enqueues all N events under a single lock,
     * or rejects the entire batch.
     * @param events Array of events to enqueue
     * @param count Number of events
     * @return false if insufficient capacity (nothing enqueued)
     */
    bool enqueueBatch(const SimEvent* events, uint8_t count) {
        if (!events || count == 0) return true;
        if (count >= CAPACITY) return false;  // Can never fit

        portENTER_CRITICAL(&_mux);

        // Check available space
        uint8_t used = (_head >= _tail) ? (_head - _tail) : (CAPACITY - _tail + _head);
        uint8_t free = CAPACITY - 1 - used;  // -1 because head==tail means empty
        if (count > free) {
            portEXIT_CRITICAL(&_mux);
            return false;  // Reject entire batch
        }

        // Enqueue all events
        for (uint8_t i = 0; i < count; i++) {
            _buffer[_head] = events[i];
            _head = (_head + 1) % CAPACITY;
        }

        portEXIT_CRITICAL(&_mux);
        return true;
    }

    /**
     * Main-loop only dequeue (single consumer, no lock needed).
     * @param event Output event
     * @return false if queue is empty
     */
    bool dequeue(SimEvent& event) {
        if (_head == _tail) return false;  // Empty
        event = _buffer[_tail];
        _tail = (_tail + 1) % CAPACITY;
        return true;
    }

    bool isEmpty() const {
        return _head == _tail;
    }

    uint8_t available() const {
        return (_head >= _tail) ? (_head - _tail) : (CAPACITY - _tail + _head);
    }

private:
    SimEvent _buffer[CAPACITY];
    volatile uint8_t _head = 0;  // Write position (producers)
    volatile uint8_t _tail = 0;  // Read position (consumer)
    portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
};
