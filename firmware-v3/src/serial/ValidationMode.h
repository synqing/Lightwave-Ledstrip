/**
 * @file ValidationMode.h
 * @brief Serial command handler for 2AFC validation stimulus delivery
 *
 * Parses VAL:* commands from serial to render test stimuli directly to the
 * LED buffer. Used by the Python stimulus_generator.py to run perceptual
 * validation studies without going through the normal effect pipeline.
 *
 * Protocol: All commands are newline-terminated ASCII strings.
 * Parameters are key=value pairs separated by spaces.
 *
 * Thread safety: poll() must be called from the render task (Core 1).
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <cmath>

#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <FastLED.h>
#else
#include "../../test/test_native/mocks/fastled_mock.h"
#endif

namespace lightwaveos {
namespace serial {

// ============================================================================
// Command Types
// ============================================================================

enum class ValCommand : uint8_t {
    NONE = 0,
    PULSE,
    CHASE,
    GRADIENT,
    SPARKLE,
    WAVE,
    FADE,
    NOISE,
    CLEAR,
    ACK,
    DIM
};

enum class WaveShape : uint8_t {
    SINE = 0,
    TRI,
    SAW,
    SQUARE
};

enum class PulseOrigin : uint8_t {
    ALL = 0,
    CENTER,
    LEFT,
    RIGHT
};

// ============================================================================
// Parameter Structs (no dynamic allocation)
// ============================================================================

struct PulseParams {
    uint16_t attackMs  = 10;
    uint16_t decayMs   = 300;
    uint8_t  intensity = 255;
    PulseOrigin origin = PulseOrigin::ALL;
};

struct ChaseParams {
    uint16_t speed = 100;   // leds/sec
    uint8_t  width = 3;
    uint8_t  trail = 200;
    uint8_t  count = 1;
};

struct GradientParams {
    uint8_t palette = 0;
    float   scroll  = 0.0f;
    float   scale   = 1.0f;
};

struct SparkleParams {
    uint16_t density = 40;  // per sec
    uint16_t fadeMs  = 200;
};

struct WaveParams {
    WaveShape waveform = WaveShape::SINE;
    float     freq     = 1.0f;
    float     speed    = 0.5f;
    uint8_t   amp      = 200;
};

struct FadeParams {
    uint8_t  startH = 0, startS = 0, startV = 0;
    uint8_t  endH   = 0, endS   = 0, endV   = 0;
    uint16_t durationMs = 1000;
};

struct NoiseParams {
    float   speed   = 0.5f;
    float   scale   = 0.3f;
    uint8_t palette = 0;
};

struct DimParams {
    float weight   = 0.0f;
    float time     = 0.0f;
    float space    = 0.0f;
    float flow     = 0.0f;
    float fluidity = 0.0f;
    float impulse  = 0.0f;
};

// ============================================================================
// ValidationMode
// ============================================================================

class ValidationMode {
public:
    /**
     * @brief Construct validation mode handler
     * @param leds     Pointer to CRGB LED buffer
     * @param ledCount Number of LEDs in the buffer
     */
    ValidationMode(CRGB* leds, uint16_t ledCount)
        : leds_(leds)
        , ledCount_(ledCount)
    {}

    /**
     * @brief Poll serial for incoming commands, render active animation
     *
     * Call once per frame from the render loop.
     */
    void poll() {
#ifndef NATIVE_BUILD
        // Read serial input
        while (Serial.available()) {
            char c = Serial.read();
            if (c == '\n' || c == '\r') {
                if (bufPos_ > 0) {
                    buf_[bufPos_] = '\0';
                    parseCommand(buf_);
                    bufPos_ = 0;
                }
            } else if (bufPos_ < kMaxBuf - 1) {
                buf_[bufPos_++] = c;
            }
        }
#endif
        // Render current animation
        if (active_) {
            renderFrame();
        }
    }

    /** @brief Returns true if validation mode is actively rendering */
    bool isActive() const { return active_; }

    /** @brief Get current dimension params (for external inspection) */
    const DimParams& getDimParams() const { return dim_; }

private:
    // ========================================================================
    // Constants
    // ========================================================================

    static constexpr uint16_t kMaxBuf    = 256;
    static constexpr uint8_t  kMaxTokens = 16;
    static constexpr uint8_t  kMaxKVLen  = 32;

    // ========================================================================
    // State
    // ========================================================================

    CRGB*    leds_;
    uint16_t ledCount_;
    bool     active_   = false;
    char     buf_[kMaxBuf];
    uint16_t bufPos_   = 0;

    ValCommand currentCmd_ = ValCommand::NONE;
    uint32_t   cmdStartMs_ = 0;
    float      phase_      = 0.0f;  // running phase for animations

    // Parameter storage (union would save RAM but clarity wins here)
    PulseParams    pulse_;
    ChaseParams    chase_;
    GradientParams gradient_;
    SparkleParams  sparkle_;
    WaveParams     wave_;
    FadeParams     fade_;
    NoiseParams    noise_;
    DimParams      dim_;

    // ========================================================================
    // Command Parsing
    // ========================================================================

    void parseCommand(const char* line) {
        // Must start with "VAL:"
        if (strncmp(line, "VAL:", 4) != 0) return;
        const char* cmd = line + 4;

        if (strncmp(cmd, "ACK", 3) == 0) {
#ifndef NATIVE_BUILD
            Serial.println("VAL:READY");
#endif
            return;
        }

        if (strncmp(cmd, "CLEAR", 5) == 0) {
            fill_solid(leds_, ledCount_, CRGB::Black);
            active_     = false;
            currentCmd_ = ValCommand::CLEAR;
            return;
        }

        // Find first space to separate command name from params
        const char* space = strchr(cmd, ' ');
        char cmdName[16];
        if (space) {
            uint8_t len = (uint8_t)(space - cmd);
            if (len >= sizeof(cmdName)) len = sizeof(cmdName) - 1;
            memcpy(cmdName, cmd, len);
            cmdName[len] = '\0';
        } else {
            strncpy(cmdName, cmd, sizeof(cmdName) - 1);
            cmdName[sizeof(cmdName) - 1] = '\0';
        }

        // Parse key=value pairs from remaining string
        const char* params = space ? space + 1 : "";

        if (strcmp(cmdName, "PULSE") == 0) {
            pulse_ = PulseParams{};
            pulse_.attackMs  = (uint16_t)getIntParam(params, "attack", 10);
            pulse_.decayMs   = (uint16_t)getIntParam(params, "decay", 300);
            pulse_.intensity = (uint8_t)getIntParam(params, "intensity", 255);
            const char* orig = getStrParam(params, "origin");
            if      (strcmp(orig, "center") == 0) pulse_.origin = PulseOrigin::CENTER;
            else if (strcmp(orig, "left") == 0)   pulse_.origin = PulseOrigin::LEFT;
            else if (strcmp(orig, "right") == 0)  pulse_.origin = PulseOrigin::RIGHT;
            else                                  pulse_.origin = PulseOrigin::ALL;
            currentCmd_ = ValCommand::PULSE;
            cmdStartMs_ = millis();
            active_ = true;

        } else if (strcmp(cmdName, "CHASE") == 0) {
            chase_ = ChaseParams{};
            chase_.speed = (uint16_t)getIntParam(params, "speed", 100);
            chase_.width = (uint8_t)getIntParam(params, "width", 3);
            chase_.trail = (uint8_t)getIntParam(params, "trail", 200);
            chase_.count = (uint8_t)getIntParam(params, "count", 1);
            currentCmd_ = ValCommand::CHASE;
            cmdStartMs_ = millis();
            phase_ = 0.0f;
            active_ = true;

        } else if (strcmp(cmdName, "GRADIENT") == 0) {
            gradient_ = GradientParams{};
            gradient_.palette = (uint8_t)getIntParam(params, "palette", 0);
            gradient_.scroll  = getFloatParam(params, "scroll", 0.0f);
            gradient_.scale   = getFloatParam(params, "scale", 1.0f);
            currentCmd_ = ValCommand::GRADIENT;
            cmdStartMs_ = millis();
            phase_ = 0.0f;
            active_ = true;

        } else if (strcmp(cmdName, "SPARKLE") == 0) {
            sparkle_ = SparkleParams{};
            sparkle_.density = (uint16_t)getIntParam(params, "density", 40);
            sparkle_.fadeMs  = (uint16_t)getIntParam(params, "fade", 200);
            currentCmd_ = ValCommand::SPARKLE;
            cmdStartMs_ = millis();
            active_ = true;

        } else if (strcmp(cmdName, "WAVE") == 0) {
            wave_ = WaveParams{};
            const char* wf = getStrParam(params, "waveform");
            if      (strcmp(wf, "tri") == 0)    wave_.waveform = WaveShape::TRI;
            else if (strcmp(wf, "saw") == 0)    wave_.waveform = WaveShape::SAW;
            else if (strcmp(wf, "square") == 0) wave_.waveform = WaveShape::SQUARE;
            else                                wave_.waveform = WaveShape::SINE;
            wave_.freq  = getFloatParam(params, "freq", 1.0f);
            wave_.speed = getFloatParam(params, "speed", 0.5f);
            wave_.amp   = (uint8_t)getIntParam(params, "amp", 200);
            currentCmd_ = ValCommand::WAVE;
            cmdStartMs_ = millis();
            phase_ = 0.0f;
            active_ = true;

        } else if (strcmp(cmdName, "FADE") == 0) {
            fade_ = FadeParams{};
            fade_.startH     = (uint8_t)getIntParam(params, "start_h", 0);
            fade_.startS     = (uint8_t)getIntParam(params, "start_s", 0);
            fade_.startV     = (uint8_t)getIntParam(params, "start_v", 0);
            fade_.endH       = (uint8_t)getIntParam(params, "end_h", 0);
            fade_.endS       = (uint8_t)getIntParam(params, "end_s", 0);
            fade_.endV       = (uint8_t)getIntParam(params, "end_v", 0);
            fade_.durationMs = (uint16_t)getIntParam(params, "duration", 1000);
            currentCmd_ = ValCommand::FADE;
            cmdStartMs_ = millis();
            active_ = true;

        } else if (strcmp(cmdName, "NOISE") == 0) {
            noise_ = NoiseParams{};
            noise_.speed   = getFloatParam(params, "speed", 0.5f);
            noise_.scale   = getFloatParam(params, "scale", 0.3f);
            noise_.palette = (uint8_t)getIntParam(params, "palette", 0);
            currentCmd_ = ValCommand::NOISE;
            cmdStartMs_ = millis();
            phase_ = 0.0f;
            active_ = true;

        } else if (strcmp(cmdName, "DIM") == 0) {
            dim_ = DimParams{};
            dim_.weight   = getFloatParam(params, "weight", 0.0f);
            dim_.time     = getFloatParam(params, "time", 0.0f);
            dim_.space    = getFloatParam(params, "space", 0.0f);
            dim_.flow     = getFloatParam(params, "flow", 0.0f);
            dim_.fluidity = getFloatParam(params, "fluidity", 0.0f);
            dim_.impulse  = getFloatParam(params, "impulse", 0.0f);
            currentCmd_ = ValCommand::DIM;
            active_ = true;
        }
    }

    // ========================================================================
    // Key=Value Parser (no dynamic allocation)
    // ========================================================================

    /** @brief Extract integer parameter value by key name */
    int32_t getIntParam(const char* params, const char* key, int32_t def) const {
        char val[kMaxKVLen];
        if (extractValue(params, key, val, sizeof(val))) {
            return atol(val);
        }
        return def;
    }

    /** @brief Extract float parameter value by key name */
    float getFloatParam(const char* params, const char* key, float def) const {
        char val[kMaxKVLen];
        if (extractValue(params, key, val, sizeof(val))) {
            return (float)atof(val);
        }
        return def;
    }

    /** @brief Extract string parameter value by key name (returns static buffer) */
    const char* getStrParam(const char* params, const char* key) const {
        static char val[kMaxKVLen];
        if (extractValue(params, key, val, sizeof(val))) {
            return val;
        }
        val[0] = '\0';
        return val;
    }

    /**
     * @brief Find key=value in space-delimited param string
     * @return true if key found, value written to out
     */
    bool extractValue(const char* params, const char* key, char* out, uint8_t outLen) const {
        uint8_t keyLen = (uint8_t)strlen(key);
        const char* p = params;
        while (*p) {
            // Skip leading spaces
            while (*p == ' ') p++;
            if (*p == '\0') break;

            // Check if this token starts with key=
            if (strncmp(p, key, keyLen) == 0 && p[keyLen] == '=') {
                const char* valStart = p + keyLen + 1;
                const char* valEnd = valStart;
                while (*valEnd && *valEnd != ' ') valEnd++;
                uint8_t len = (uint8_t)(valEnd - valStart);
                if (len >= outLen) len = outLen - 1;
                memcpy(out, valStart, len);
                out[len] = '\0';
                return true;
            }

            // Skip to next space
            while (*p && *p != ' ') p++;
        }
        return false;
    }

    // ========================================================================
    // Rendering
    // ========================================================================

    void renderFrame() {
        uint32_t elapsed = millis() - cmdStartMs_;

        switch (currentCmd_) {
            case ValCommand::PULSE:   renderPulse(elapsed); break;
            case ValCommand::CHASE:   renderChase(elapsed); break;
            case ValCommand::GRADIENT:renderGradient(elapsed); break;
            case ValCommand::SPARKLE: renderSparkle(elapsed); break;
            case ValCommand::WAVE:    renderWave(elapsed); break;
            case ValCommand::FADE:    renderFade(elapsed); break;
            case ValCommand::NOISE:   renderNoise(elapsed); break;
            case ValCommand::DIM:     /* DIM sets params only, no render */ break;
            default: break;
        }
    }

    void renderPulse(uint32_t elapsedMs) {
        float brightness;
        if (elapsedMs < pulse_.attackMs) {
            brightness = (float)elapsedMs / (float)pulse_.attackMs;
        } else {
            uint32_t decayElapsed = elapsedMs - pulse_.attackMs;
            if (decayElapsed >= pulse_.decayMs) {
                brightness = 0.0f;
            } else {
                brightness = 1.0f - (float)decayElapsed / (float)pulse_.decayMs;
            }
        }
        uint8_t val = (uint8_t)(brightness * pulse_.intensity);
        uint16_t center = ledCount_ / 2;

        for (uint16_t i = 0; i < ledCount_; i++) {
            uint8_t v = val;
            switch (pulse_.origin) {
                case PulseOrigin::CENTER: {
                    float dist = (float)abs((int)i - (int)center) / (float)center;
                    v = (uint8_t)(val * (1.0f - dist * 0.5f));
                    break;
                }
                case PulseOrigin::LEFT: {
                    float dist = (float)i / (float)ledCount_;
                    v = (uint8_t)(val * (1.0f - dist * 0.7f));
                    break;
                }
                case PulseOrigin::RIGHT: {
                    float dist = 1.0f - (float)i / (float)ledCount_;
                    v = (uint8_t)(val * (1.0f - dist * 0.7f));
                    break;
                }
                case PulseOrigin::ALL:
                default:
                    break;
            }
            leds_[i] = CHSV(0, 0, v);
        }
    }

    void renderChase(uint32_t elapsedMs) {
        fadeToBlackBy(leds_, ledCount_, chase_.trail);
        float pos = (float)elapsedMs * 0.001f * chase_.speed;
        for (uint8_t c = 0; c < chase_.count; c++) {
            float offset = pos + (float)c * ((float)ledCount_ / chase_.count);
            int16_t head = (int16_t)fmod(offset, (float)ledCount_);
            if (head < 0) head += ledCount_;
            for (uint8_t w = 0; w < chase_.width; w++) {
                int16_t idx = (head - w + ledCount_) % ledCount_;
                uint8_t bright = 255 - (uint8_t)((float)w / chase_.width * 128);
                leds_[idx] = CHSV((uint8_t)(c * 64), 255, bright);
            }
        }
    }

    void renderGradient(uint32_t elapsedMs) {
        float scrollOffset = (float)elapsedMs * 0.001f * gradient_.scroll;
        for (uint16_t i = 0; i < ledCount_; i++) {
            float pos = ((float)i / ledCount_) * gradient_.scale + scrollOffset;
            uint8_t hue = gradient_.palette + (uint8_t)(pos * 255.0f);
            leds_[i] = CHSV(hue, 240, 200);
        }
    }

    void renderSparkle(uint32_t elapsedMs) {
        // Fade existing
        uint8_t fadeAmount = sparkle_.fadeMs > 0
            ? (uint8_t)(255 * 8 / sparkle_.fadeMs)  // approx for ~120fps
            : 255;
        fadeToBlackBy(leds_, ledCount_, fadeAmount);

        // Spawn new sparkles based on density (per second, at ~120fps)
        float spawnChance = (float)sparkle_.density / 120.0f;
        for (uint16_t i = 0; i < ledCount_; i++) {
            if ((float)random(10000) / 10000.0f < spawnChance / ledCount_) {
                leds_[i] = CHSV((uint8_t)random(256), 200, 255);
            }
        }
        (void)elapsedMs;
    }

    void renderWave(uint32_t elapsedMs) {
        float t = (float)elapsedMs * 0.001f;
        phase_ = t * wave_.speed;
        for (uint16_t i = 0; i < ledCount_; i++) {
            float x = (float)i / (float)ledCount_;
            float angle = (x * wave_.freq + phase_) * 2.0f * 3.14159265f;
            float sample;
            switch (wave_.waveform) {
                case WaveShape::SINE:
                    sample = sinf(angle) * 0.5f + 0.5f;
                    break;
                case WaveShape::TRI:
                    sample = fabsf(fmodf(angle / (2.0f * 3.14159265f), 1.0f) * 2.0f - 1.0f);
                    break;
                case WaveShape::SAW:
                    sample = fmodf(angle / (2.0f * 3.14159265f), 1.0f);
                    if (sample < 0.0f) sample += 1.0f;
                    break;
                case WaveShape::SQUARE:
                    sample = sinf(angle) > 0.0f ? 1.0f : 0.0f;
                    break;
                default:
                    sample = 0.0f;
                    break;
            }
            uint8_t val = (uint8_t)(sample * wave_.amp);
            leds_[i] = CHSV(160, 200, val);
        }
    }

    void renderFade(uint32_t elapsedMs) {
        float t = (fade_.durationMs > 0)
            ? fminf((float)elapsedMs / (float)fade_.durationMs, 1.0f)
            : 1.0f;
        uint8_t h = fade_.startH + (uint8_t)((float)(fade_.endH - fade_.startH) * t);
        uint8_t s = fade_.startS + (uint8_t)((float)(fade_.endS - fade_.startS) * t);
        uint8_t v = fade_.startV + (uint8_t)((float)(fade_.endV - fade_.startV) * t);
        fill_solid(leds_, ledCount_, CHSV(h, s, v));
    }

    void renderNoise(uint32_t elapsedMs) {
        float t = (float)elapsedMs * 0.001f * noise_.speed;
        for (uint16_t i = 0; i < ledCount_; i++) {
            // Use FastLED's inoise8 for deterministic Perlin noise
            uint16_t nx = (uint16_t)((float)i * noise_.scale * 100.0f);
            uint16_t ny = (uint16_t)(t * 100.0f);
            uint8_t n = inoise8(nx, ny);
            uint8_t hue = noise_.palette + (n >> 1);
            leds_[i] = CHSV(hue, 220, n);
        }
    }
};

} // namespace serial
} // namespace lightwaveos
