#include "EncoderService.h"

EncoderService::EncoderService(Rotate8Transport& transport, EncoderProcessing& processing)
    : _transport(transport)
    , _processing(processing)
    , _last_poll_ms(0)
    , _last_active_channel(255)
{
    for (uint8_t i = 0; i < 8; i++) {
        _last_channel_change_time[i] = 0;
    }
}

bool EncoderService::begin() {
    if (!_transport.begin()) {
        Serial.println("[ENCODER SERVICE] Transport begin() failed");
        return false;
    }
    
    _processing.begin();
    _last_poll_ms = 0;
    _last_active_channel = 255;
    
    Serial.println("[ENCODER SERVICE] Service initialised");
    return true;
}

void EncoderService::update() {
    // Rate-limit encoder polling to reduce I2C load
    uint32_t now_ms = millis();
    if (now_ms - _last_poll_ms < POLL_INTERVAL_MS) {
        return;
    }
    _last_poll_ms = now_ms;

    // Check if hardware is connected
    if (!_transport.isConnected()) {
        return;
    }

    // Poll all encoders
    for (uint8_t channel = 0; channel < 8; channel++) {
        // Lockout to avoid rapid cross-channel reads
        if (_last_active_channel != 255 &&
            _last_active_channel != channel &&
            (now_ms - _last_channel_change_time[_last_active_channel] < CHANNEL_LOCKOUT_MS)) {
            continue;
        }

        // Read delta
        int32_t delta = _transport.readDelta(channel);
        if (delta != 0) {
            // Process delta (callback is emitted internally if throttling allows)
            _processing.processDelta(channel, delta, now_ms);
            _last_channel_change_time[channel] = now_ms;
            _last_active_channel = channel;
        }

        // Read button
        bool button_pressed = _transport.readButton(channel);
        if (_processing.processButton(channel, button_pressed, now_ms)) {
            // Button press detected (reset event already emitted by processing)
            _last_channel_change_time[channel] = now_ms;
            _last_active_channel = channel;
        }
    }
}

void EncoderService::setCallback(EncoderEventCallback callback) {
    _processing.setCallback(callback);
}

bool EncoderService::isConnected() {
    return _transport.isConnected();
}

