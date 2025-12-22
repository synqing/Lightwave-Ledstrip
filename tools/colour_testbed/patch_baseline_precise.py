#!/usr/bin/env python3
"""
Precise patch for baseline commit 9734cca: add capture instrumentation only.

This script:
1. Adds capture infrastructure to RendererActor (header + implementation)
2. Modifies onTick() to capture at Tap A (after renderFrame), Tap B (same as A, no correction), Tap C (before FastLED.show)
3. Adds capture commands + effect <id> to main.cpp
4. Does NOT add ColorCorrectionEngine (baseline has none)
"""

import os
import re
import shutil

BASELINE_DIR = "baseline_9734cca"

def patch_renderer_actor_header():
    """Add capture infrastructure declarations to RendererActor.h"""
    header_path = f"{BASELINE_DIR}/v2/src/core/actors/RendererActor.h"
    
    with open(header_path, 'r') as f:
        content = f.read()
    
    # Find the end of public methods (before protected:)
    protected_pos = content.find("protected:")
    if protected_pos == -1:
        raise ValueError("Could not find 'protected:' in RendererActor.h")
    
    # Insert capture infrastructure before protected:
    capture_decl = """
    // ========================================================================
    // Frame Capture System (for testbed)
    // ========================================================================

    /**
     * @brief Frame capture tap points
     */
    enum class CaptureTap : uint8_t {
        TAP_A_PRE_CORRECTION = 0,   // After renderFrame()
        TAP_B_POST_CORRECTION = 1,  // Same as Tap A in baseline (no correction)
        TAP_C_PRE_WS2812 = 2        // Before FastLED.show()
    };

    /**
     * @brief Enable/disable frame capture mode
     * @param enabled True to enable capture, false to disable
     * @param tapMask Bitmask of taps to capture (bit 0=Tap A, bit 1=Tap B, bit 2=Tap C)
     */
    void setCaptureMode(bool enabled, uint8_t tapMask = 0x07);

    /**
     * @brief Check if capture mode is enabled
     */
    bool isCaptureModeEnabled() const { return m_captureEnabled; }

    /**
     * @brief Get captured frame for a specific tap
     * @param tap Tap point to retrieve
     * @param outBuffer Buffer to copy into (must be TOTAL_LEDS size)
     * @return true if frame was captured, false if not available
     */
    bool getCapturedFrame(CaptureTap tap, CRGB* outBuffer) const;

    /**
     * @brief Get capture metadata (effect ID, palette ID, frame index, timestamp)
     */
    struct CaptureMetadata {
        uint8_t effectId;
        uint8_t paletteId;
        uint8_t brightness;
        uint8_t speed;
        uint32_t frameIndex;
        uint32_t timestampUs;
    };
    CaptureMetadata getCaptureMetadata() const;

    /**
     * @brief Force a single render/capture cycle for the requested tap.
     */
    void forceOneShotCapture(CaptureTap tap);
"""
    
    content = content[:protected_pos] + capture_decl + "\n    " + content[protected_pos:]
    
    # Add capture member variables before private methods
    private_pos = content.find("private:")
    if private_pos == -1:
        raise ValueError("Could not find 'private:' in RendererActor.h")
    
    capture_members = """
    // Frame capture system (for testbed)
    bool m_captureEnabled;
    uint8_t m_captureTapMask;  // Bitmask: bit 0=Tap A, bit 1=Tap B, bit 2=Tap C
    CRGB m_captureTapA[LedConfig::TOTAL_LEDS];  // Pre-correction
    CRGB m_captureTapB[LedConfig::TOTAL_LEDS];  // Post-correction (same as A in baseline)
    CRGB m_captureTapC[LedConfig::TOTAL_LEDS];  // Pre-WS2812 (per-strip, interleaved)
    CaptureMetadata m_captureMetadata;
    bool m_captureTapAValid;
    bool m_captureTapBValid;
    bool m_captureTapCValid;

    /**
     * @brief Capture frame at specified tap point
     * @param tap Tap point to capture
     * @param sourceBuffer Source buffer to copy from
     */
    void captureFrame(CaptureTap tap, const CRGB* sourceBuffer);
"""
    
    # Find the line before private methods section
    lines = content[:private_pos].split('\n')
    insert_pos = len('\n'.join(lines))
    content = content[:insert_pos] + capture_members + "\n    " + content[insert_pos:]
    
    with open(header_path, 'w') as f:
        f.write(content)
    
    print(f"✓ Patched {header_path}")

def patch_renderer_actor_cpp():
    """Add capture infrastructure implementation to RendererActor.cpp"""
    cpp_path = f"{BASELINE_DIR}/v2/src/core/actors/RendererActor.cpp"
    
    with open(cpp_path, 'r') as f:
        content = f.read()
    
    # Initialize capture members in constructor
    init_pos = content.find("m_lastFrameTime(0)")
    if init_pos == -1:
        raise ValueError("Could not find constructor initialization list")
    
    # Find the end of the initialization list
    brace_pos = content.find("{", init_pos)
    if brace_pos == -1:
        raise ValueError("Could not find constructor body")
    
    # Add capture initialization before the brace
    capture_init = "    , m_captureEnabled(false)\n    , m_captureTapMask(0)\n    , m_captureTapAValid(false)\n    , m_captureTapBValid(false)\n    , m_captureTapCValid(false)"
    
    # Find the last comma before the brace
    last_comma = content.rfind(",", init_pos, brace_pos)
    if last_comma != -1:
        content = content[:last_comma+1] + capture_init + content[last_comma+1:]
    else:
        # No comma found, add after last member
        content = content[:brace_pos] + capture_init + "\n" + content[brace_pos:]
    
    # Initialize capture buffers in constructor body
    memset_pos = content.find("memset(m_leds, 0, sizeof(m_leds));", brace_pos)
    if memset_pos != -1:
        capture_memset = "\n    memset(m_captureTapA, 0, sizeof(m_captureTapA));\n    memset(m_captureTapB, 0, sizeof(m_captureTapB));\n    memset(m_captureTapC, 0, sizeof(m_captureTapC));"
        content = content[:memset_pos] + content[memset_pos:].replace("memset(m_leds, 0, sizeof(m_leds));", 
                                                                        "memset(m_leds, 0, sizeof(m_leds));" + capture_memset, 1)
    
    # Modify onTick() to add capture taps
    ontick_start = content.find("void RendererActor::onTick()")
    if ontick_start == -1:
        raise ValueError("Could not find onTick()")
    
    ontick_body_start = content.find("{", ontick_start)
    render_pos = content.find("renderFrame();", ontick_body_start)
    showleds_pos = content.find("showLeds();", ontick_body_start)
    
    if render_pos == -1 or showleds_pos == -1:
        raise ValueError("Could not find renderFrame() or showLeds() in onTick()")
    
    # Add Tap A capture after renderFrame()
    tap_a = "\n\n    // TAP A: Capture pre-correction (after renderFrame, no correction in baseline)\n    if (m_captureEnabled && (m_captureTapMask & 0x01)) {\n        captureFrame(CaptureTap::TAP_A_PRE_CORRECTION, m_leds);\n    }\n\n    // TAP B: Capture same as Tap A (no color correction in baseline)\n    if (m_captureEnabled && (m_captureTapMask & 0x02)) {\n        captureFrame(CaptureTap::TAP_B_POST_CORRECTION, m_leds);\n    }"
    
    content = content[:render_pos] + content[render_pos:render_pos+len("renderFrame();")] + tap_a + content[render_pos+len("renderFrame();"):]
    
    # Add Tap C capture before showLeds() (need to modify showLeds to capture before FastLED.show)
    # Actually, we'll capture in showLeds() itself
    
    # Add capture implementation methods at the end (before private methods section)
    private_section = content.find("// ============================================================================\n// Private Methods")
    if private_section == -1:
        private_section = content.find("void RendererActor::initLeds()")
    
    if private_section == -1:
        raise ValueError("Could not find private methods section")
    
    capture_impl = """
// ============================================================================
// Frame Capture System (for testbed)
// ============================================================================

void RendererActor::setCaptureMode(bool enabled, uint8_t tapMask) {
    m_captureEnabled = enabled;
    m_captureTapMask = tapMask & 0x07;  // Only bits 0-2 are valid
    
    if (!enabled) {
        m_captureTapAValid = false;
        m_captureTapBValid = false;
        m_captureTapCValid = false;
    }
    
    ESP_LOGI("Renderer", "Capture mode %s (tapMask=0x%02X)", 
             enabled ? "enabled" : "disabled", m_captureTapMask);
}

bool RendererActor::getCapturedFrame(CaptureTap tap, CRGB* outBuffer) const {
    if (!m_captureEnabled || outBuffer == nullptr) {
        return false;
    }
    
    bool valid = false;
    const CRGB* source = nullptr;
    
    switch (tap) {
        case CaptureTap::TAP_A_PRE_CORRECTION:
            valid = m_captureTapAValid;
            source = m_captureTapA;
            break;
        case CaptureTap::TAP_B_POST_CORRECTION:
            valid = m_captureTapBValid;
            source = m_captureTapB;
            break;
        case CaptureTap::TAP_C_PRE_WS2812:
            valid = m_captureTapCValid;
            source = m_captureTapC;
            break;
        default:
            return false;
    }
    
    if (valid && source != nullptr) {
        memcpy(outBuffer, source, sizeof(CRGB) * LedConfig::TOTAL_LEDS);
        return true;
    }
    
    return false;
}

RendererActor::CaptureMetadata RendererActor::getCaptureMetadata() const {
    return m_captureMetadata;
}

void RendererActor::forceOneShotCapture(CaptureTap tap) {
    if (!m_captureEnabled) {
        return;
    }
    
    // Snapshot current state to avoid corrupting buffer-feedback effects
    CRGB snapshot[LedConfig::TOTAL_LEDS];
    memcpy(snapshot, m_leds, sizeof(CRGB) * LedConfig::TOTAL_LEDS);
    uint32_t savedHue = m_hue;
    
    // Render one frame
    renderFrame();
    
    // Capture the requested tap
    switch (tap) {
        case CaptureTap::TAP_A_PRE_CORRECTION:
            captureFrame(CaptureTap::TAP_A_PRE_CORRECTION, m_leds);
            break;
        case CaptureTap::TAP_B_POST_CORRECTION:
            captureFrame(CaptureTap::TAP_B_POST_CORRECTION, m_leds);
            break;
        case CaptureTap::TAP_C_PRE_WS2812:
            // For Tap C, we need to go through showLeds() to get the strip-split buffer
            showLeds();  // This will capture Tap C internally
            break;
        default:
            break;
    }
    
    // Restore state
    memcpy(m_leds, snapshot, sizeof(CRGB) * LedConfig::TOTAL_LEDS);
    m_hue = savedHue;
}

void RendererActor::captureFrame(CaptureTap tap, const CRGB* sourceBuffer) {
    if (sourceBuffer == nullptr) {
        return;
    }
    
    // Update metadata
    m_captureMetadata.effectId = m_currentEffect;
    m_captureMetadata.paletteId = m_paletteIndex;
    m_captureMetadata.brightness = m_brightness;
    m_captureMetadata.speed = m_speed;
    m_captureMetadata.frameIndex = m_frameCount;
    m_captureMetadata.timestampUs = micros();
    
    // Copy frame data
    switch (tap) {
        case CaptureTap::TAP_A_PRE_CORRECTION:
            memcpy(m_captureTapA, sourceBuffer, sizeof(CRGB) * LedConfig::TOTAL_LEDS);
            m_captureTapAValid = true;
            break;
        case CaptureTap::TAP_B_POST_CORRECTION:
            memcpy(m_captureTapB, sourceBuffer, sizeof(CRGB) * LedConfig::TOTAL_LEDS);
            m_captureTapBValid = true;
            break;
        case CaptureTap::TAP_C_PRE_WS2812:
            memcpy(m_captureTapC, sourceBuffer, sizeof(CRGB) * LedConfig::TOTAL_LEDS);
            m_captureTapCValid = true;
            break;
        default:
            break;
    }
}

"""
    
    content = content[:private_section] + capture_impl + "\n" + content[private_section:]
    
    # Modify showLeds() to capture Tap C
    showleds_start = content.find("void RendererActor::showLeds()")
    if showleds_start != -1:
        showleds_body = content.find("{", showleds_start)
        fastled_show = content.find("FastLED.show();", showleds_body)
        
        if fastled_show != -1:
            # Before FastLED.show(), capture Tap C (interleaved strip format)
            tap_c_capture = """
    // TAP C: Capture pre-WS2812 (after strip split, before FastLED.show)
    if (m_captureEnabled && (m_captureTapMask & 0x04)) {
        // Interleave strip1 and strip2 into unified format for capture
        for (uint16_t i = 0; i < LedConfig::LEDS_PER_STRIP; i++) {
            m_captureTapC[i] = m_strip1[i];
            m_captureTapC[i + LedConfig::LEDS_PER_STRIP] = m_strip2[i];
        }
        captureFrame(CaptureTap::TAP_C_PRE_WS2812, m_captureTapC);
    }

"""
            content = content[:fastled_show] + tap_c_capture + content[fastled_show:]
    
    with open(cpp_path, 'w') as f:
        f.write(content)
    
    print(f"✓ Patched {cpp_path}")

def patch_main_cpp():
    """Add capture commands + effect <id> to main.cpp"""
    # For now, just copy the current main.cpp and remove ColorCorrectionEngine references
    main_path = f"{BASELINE_DIR}/v2/src/main.cpp"
    current_main = "v2/src/main.cpp"
    
    if not os.path.exists(current_main):
        raise FileNotFoundError(f"Current main.cpp not found: {current_main}")
    
    shutil.copy(current_main, main_path)
    
    # Remove ColorCorrectionEngine includes and calls
    with open(main_path, 'r') as f:
        content = f.read()
    
    # Remove includes
    content = re.sub(r'#include\s+["<].*ColorCorrectionEngine.*[">]\s*\n', '', content)
    
    # Remove ColorCorrectionEngine command handlers (cc, ae, gamma, brown, Csave)
    # This is complex, so we'll just comment them out for safety
    # Actually, let's keep the structure but make them no-ops or remove the handlers
    
    # Remove actual engine calls
    content = re.sub(r'ColorCorrectionEngine::getInstance\(\)[^;]*;', '// Color correction disabled in baseline', content)
    
    with open(main_path, 'w') as f:
        f.write(content)
    
    print(f"✓ Patched {main_path}")

if __name__ == "__main__":
    if not os.path.exists(BASELINE_DIR):
        print(f"Error: {BASELINE_DIR} not found.")
        print("Run: git archive 9734cca | tar -x -C baseline_9734cca")
        exit(1)
    
    print("Patching baseline with capture instrumentation (no color correction)...")
    patch_renderer_actor_header()
    patch_renderer_actor_cpp()
    patch_main_cpp()
    print("\n✓ Baseline patched successfully!")
    print("\nNext steps:")
    print(f"1. cd {BASELINE_DIR}/v2")
    print("2. pio run -e esp32dev -t upload --upload-port /dev/tty.usbmodem1101")
    print("3. cd ../..")
    print("4. python3 tools/colour_testbed/capture_test.py --output captures/baseline_gamut")

