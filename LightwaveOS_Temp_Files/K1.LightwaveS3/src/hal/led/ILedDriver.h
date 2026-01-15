/**
 * @file ILedDriver.h
 * @brief Hardware Abstraction Layer interface for LED drivers
 *
 * LightwaveOS v2 HAL - LED Driver Interface
 *
 * This interface abstracts LED hardware so effects can be written once
 * and work with any LED type (WS2812, SK6812, APA102, etc.).
 *
 * Design principles:
 * - No global variables - all state in class members
 * - Thread-safe for FreeRTOS multi-core operation
 * - Memory efficient (~1KB RAM max for driver overhead)
 * - CENTER ORIGIN aware - provides center point information
 *
 * @copyright 2024 LightwaveOS Project
 */

#ifndef LIGHTWAVEOS_HAL_ILED_DRIVER_H
#define LIGHTWAVEOS_HAL_ILED_DRIVER_H

#include <cstdint>
#include <cstddef>

namespace lightwaveos {
namespace hal {

/**
 * @brief RGB color structure
 *
 * Simple 24-bit RGB color representation.
 * Memory layout matches most LED drivers (3 bytes per LED).
 */
struct RGB {
    uint8_t r;  ///< Red component (0-255)
    uint8_t g;  ///< Green component (0-255)
    uint8_t b;  ///< Blue component (0-255)

    /**
     * @brief Default constructor - black
     */
    constexpr RGB() : r(0), g(0), b(0) {}

    /**
     * @brief Construct from RGB values
     * @param r Red component
     * @param g Green component
     * @param b Blue component
     */
    constexpr RGB(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}

    /**
     * @brief Construct from packed 24-bit value (0xRRGGBB)
     * @param packed Packed RGB value
     */
    constexpr explicit RGB(uint32_t packed)
        : r((packed >> 16) & 0xFF)
        , g((packed >> 8) & 0xFF)
        , b(packed & 0xFF) {}

    /**
     * @brief Convert to packed 24-bit value
     * @return Packed RGB value (0xRRGGBB)
     */
    constexpr uint32_t toPacked() const {
        return (static_cast<uint32_t>(r) << 16) |
               (static_cast<uint32_t>(g) << 8) |
               static_cast<uint32_t>(b);
    }

    /**
     * @brief Equality comparison
     */
    constexpr bool operator==(const RGB& other) const {
        return r == other.r && g == other.g && b == other.b;
    }

    /**
     * @brief Inequality comparison
     */
    constexpr bool operator!=(const RGB& other) const {
        return !(*this == other);
    }

    /**
     * @brief Scale color by 8-bit factor (0-255)
     * @param scale Scale factor (0 = black, 255 = full)
     * @return Scaled color
     */
    RGB scaled(uint8_t scale) const {
        return RGB(
            static_cast<uint8_t>((static_cast<uint16_t>(r) * scale) >> 8),
            static_cast<uint8_t>((static_cast<uint16_t>(g) * scale) >> 8),
            static_cast<uint8_t>((static_cast<uint16_t>(b) * scale) >> 8)
        );
    }

    // Common color constants
    static constexpr RGB Black()   { return RGB(0, 0, 0); }
    static constexpr RGB White()   { return RGB(255, 255, 255); }
    static constexpr RGB Red()     { return RGB(255, 0, 0); }
    static constexpr RGB Green()   { return RGB(0, 255, 0); }
    static constexpr RGB Blue()    { return RGB(0, 0, 255); }
    static constexpr RGB Yellow()  { return RGB(255, 255, 0); }
    static constexpr RGB Cyan()    { return RGB(0, 255, 255); }
    static constexpr RGB Magenta() { return RGB(255, 0, 255); }
};

/**
 * @brief Strip topology information
 *
 * Provides information about the physical and logical layout
 * of the LED strip system for CENTER ORIGIN effects.
 */
struct StripTopology {
    uint16_t totalLeds;       ///< Total LED count across all strips
    uint16_t ledsPerStrip;    ///< LEDs per physical strip
    uint8_t  stripCount;      ///< Number of physical strips
    uint16_t centerPoint;     ///< Logical center LED index for CENTER ORIGIN
    uint16_t halfLength;      ///< LEDs from center to edge

    /**
     * @brief Check if an index is in the left half (0 to center-1)
     */
    constexpr bool isLeftHalf(uint16_t index) const {
        return index < centerPoint;
    }

    /**
     * @brief Check if an index is in the right half (center to end)
     */
    constexpr bool isRightHalf(uint16_t index) const {
        return index >= centerPoint;
    }

    /**
     * @brief Get distance from center for an LED index
     * @param index LED index
     * @return Distance from center point (0 at center, increases outward)
     */
    constexpr uint16_t distanceFromCenter(uint16_t index) const {
        if (index < centerPoint) {
            return centerPoint - 1 - index;
        }
        return index - centerPoint;
    }
};

/**
 * @brief Abstract interface for LED drivers
 *
 * Implementations must be thread-safe. The show() method may be called
 * from the render task on Core 1 while configuration methods may be
 * called from network handlers on Core 0.
 *
 * Usage pattern:
 * @code
 * ILedDriver* driver = new FastLedDriver(config);
 * if (!driver->init()) {
 *     // Handle error
 * }
 *
 * // In render loop
 * driver->clear();
 * driver->setLed(80, RGB::Red());  // Set center LED
 * driver->show();
 * @endcode
 */
class ILedDriver {
public:
    virtual ~ILedDriver() = default;

    // ========== Lifecycle ==========

    /**
     * @brief Initialize the LED driver hardware
     *
     * Must be called before any other methods. Sets up GPIO pins,
     * allocates buffers, and initializes the underlying LED library.
     *
     * @return true if initialization successful, false on error
     */
    virtual bool init() = 0;

    /**
     * @brief Shutdown the LED driver
     *
     * Turns off all LEDs and releases hardware resources.
     * After calling shutdown(), init() must be called again before use.
     */
    virtual void shutdown() = 0;

    /**
     * @brief Check if driver is initialized and ready
     * @return true if driver is ready for use
     */
    virtual bool isReady() const = 0;

    // ========== Configuration ==========

    /**
     * @brief Get total LED count
     * @return Total number of LEDs managed by this driver
     */
    virtual uint16_t getLedCount() const = 0;

    /**
     * @brief Get center point index for CENTER ORIGIN effects
     *
     * For LightwaveOS dual-strip setup:
     * - Strip 1: LEDs 0-159
     * - Strip 2: LEDs 160-319
     * - Center point: LED 80 (where strips conceptually meet)
     *
     * @return Index of the center LED
     */
    virtual uint16_t getCenterPoint() const = 0;

    /**
     * @brief Get strip topology information
     * @return StripTopology structure with layout details
     */
    virtual StripTopology getTopology() const = 0;

    // ========== Buffer Operations ==========

    /**
     * @brief Set a single LED to an RGB color
     * @param index LED index (0 to getLedCount()-1)
     * @param color RGB color value
     *
     * @note Does nothing if index is out of range (fail-safe)
     */
    virtual void setLed(uint16_t index, RGB color) = 0;

    /**
     * @brief Set a single LED to RGB components
     * @param index LED index (0 to getLedCount()-1)
     * @param r Red component (0-255)
     * @param g Green component (0-255)
     * @param b Blue component (0-255)
     */
    virtual void setLed(uint16_t index, uint8_t r, uint8_t g, uint8_t b) = 0;

    /**
     * @brief Get the current color of an LED
     * @param index LED index (0 to getLedCount()-1)
     * @return Current RGB color (black if index out of range)
     */
    virtual RGB getLed(uint16_t index) const = 0;

    /**
     * @brief Fill all LEDs with a single color
     * @param color RGB color to fill with
     */
    virtual void fill(RGB color) = 0;

    /**
     * @brief Fill a range of LEDs with a single color
     * @param startIndex First LED index
     * @param count Number of LEDs to fill
     * @param color RGB color to fill with
     */
    virtual void fillRange(uint16_t startIndex, uint16_t count, RGB color) = 0;

    /**
     * @brief Clear all LEDs to black
     *
     * Equivalent to fill(RGB::Black()) but may be optimized.
     */
    virtual void clear() = 0;

    /**
     * @brief Get pointer to raw LED buffer
     *
     * @warning This bypasses abstraction for performance-critical code.
     * Use with caution - format depends on implementation.
     *
     * @return Pointer to internal RGB buffer, or nullptr if not supported
     */
    virtual RGB* getBuffer() = 0;

    /**
     * @brief Get const pointer to raw LED buffer
     * @return Const pointer to internal RGB buffer, or nullptr if not supported
     */
    virtual const RGB* getBuffer() const = 0;

    // ========== Output Control ==========

    /**
     * @brief Output the LED buffer to physical LEDs
     *
     * Transfers the internal buffer to the LED strip hardware.
     * For WS2812 at 320 LEDs, this takes approximately 9.6ms.
     *
     * This method should be called from the render task (Core 1)
     * after all LED values have been set.
     *
     * @note This is a blocking call - it does not return until
     *       data transmission is complete.
     */
    virtual void show() = 0;

    /**
     * @brief Set global brightness
     * @param brightness Brightness level (0-255, 0=off, 255=max)
     *
     * @note Brightness is applied during show(), not immediately.
     */
    virtual void setBrightness(uint8_t brightness) = 0;

    /**
     * @brief Get current brightness setting
     * @return Current brightness level (0-255)
     */
    virtual uint8_t getBrightness() const = 0;

    /**
     * @brief Set maximum power budget
     * @param volts Supply voltage (typically 5)
     * @param milliamps Maximum current in milliamps
     *
     * The driver will scale brightness to stay within power budget.
     * Not all implementations support this.
     */
    virtual void setMaxPower(uint8_t volts, uint32_t milliamps) = 0;

    // ========== Performance ==========

    /**
     * @brief Get time of last show() call in microseconds
     * @return Duration of last show() in microseconds, or 0 if unknown
     */
    virtual uint32_t getLastShowTime() const = 0;

    /**
     * @brief Get estimated frame rate based on show() timing
     * @return Estimated FPS, or 0 if unknown
     */
    virtual float getEstimatedFPS() const = 0;
};

} // namespace hal
} // namespace lightwaveos

#endif // LIGHTWAVEOS_HAL_ILED_DRIVER_H
