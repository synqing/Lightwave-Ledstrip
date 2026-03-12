/**
 * @file RM690B0Driver.cpp
 * @brief RM690B0 AMOLED QSPI driver implementation
 *
 * Init sequence and QSPI protocol derived from LilyGo-AMOLED-Series.
 * Uses ESP-IDF SPI master driver for QSPI (quad I/O) transfers.
 */

#include "RM690B0Driver.h"
#include "../../config/chip_config.h"
#include "../../config/features.h"

#if !defined(NATIVE_BUILD) && FEATURE_AMOLED_DISPLAY

#include <Arduino.h>
#include <driver/gpio.h>
#include <cstring>

namespace lightwaveos {
namespace display {

// ============================================================================
// RM690B0 Panel Init Sequence
// ============================================================================
// Format: { command, {data...}, data_length | delay_flags }
// Bit 7 of length: 0x80 = 120ms delay after, 0x20 = 10ms delay after

// Init command entry: cmd, data[], data_length, delay_ms
// Derived from working voice-feasibility harness (SH8601 / Waveshare reference)
struct PanelCmd {
    uint8_t cmd;
    uint8_t data[4];
    uint8_t len;
    uint8_t delayMs;
};

static const PanelCmd s_initSequence[] = {
    {0xFE, {0x20},                   1, 0},    // Enter CMD page 0x20
    {0x26, {0x0A},                   1, 0},    // Undocumented panel timing
    {0x24, {0x80},                   1, 0},    // Undocumented panel timing
    {0xFE, {0x00},                   1, 0},    // Return to CMD page 0x00
    {0x3A, {0x55},                   1, 0},    // Pixel format: RGB565
    {0xC2, {0x00},                   1, 10},   // 10ms delay
    {0x35, {0x00},                   0, 0},    // Tearing effect line on
    {0x51, {0x00},                   1, 10},   // Brightness = 0
    {0x11, {0x00},                   0, 80},   // Sleep out + 80ms
    {0x2A, {0x00, 0x10, 0x01, 0xD1}, 4, 0},   // CASET: col 16..465 (450 px native)
    {0x2B, {0x00, 0x00, 0x02, 0x57}, 4, 0},   // RASET: row 0..599  (600 px native)
    {0x29, {0x00},                   0, 10},   // Display on + 10ms
    {0x36, {0x30},                   1, 0},    // MADCTL: MV+ML (portrait→landscape 600×450)
    {0x51, {0xFF},                   1, 0},    // Brightness = max
};

static constexpr uint8_t INIT_SEQ_COUNT = sizeof(s_initSequence) / sizeof(s_initSequence[0]);

// ============================================================================
// Constructor / Destructor
// ============================================================================

RM690B0Driver::~RM690B0Driver() {
    if (m_spi) {
        spi_bus_remove_device(m_spi);
        spi_bus_free(SPI2_HOST);
        m_spi = nullptr;
    }
}

// ============================================================================
// Initialization
// ============================================================================

bool RM690B0Driver::init() {
    if (m_initialized) return true;

    // Configure CS as GPIO output (manual control for QSPI transactions)
    gpio_set_direction(static_cast<gpio_num_t>(chip::gpio::AMOLED_CS), GPIO_MODE_OUTPUT);
    gpio_set_level(static_cast<gpio_num_t>(chip::gpio::AMOLED_CS), 1);  // Deassert

    // Hardware reset via AMOLED_RST (GPIO 21)
    hardwareReset();

    // Initialize QSPI bus
    spi_bus_config_t busConfig = {};
    busConfig.sclk_io_num   = chip::gpio::AMOLED_CLK;
    busConfig.data0_io_num  = chip::gpio::AMOLED_D0;
    busConfig.data1_io_num  = chip::gpio::AMOLED_D1;
    busConfig.data2_io_num  = chip::gpio::AMOLED_D2;
    busConfig.data3_io_num  = chip::gpio::AMOLED_D3;
    busConfig.max_transfer_sz = MAX_TRANSFER_BYTES;
    busConfig.flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_QUAD;

    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &busConfig, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE("RM690B0", "SPI bus init failed: %s", esp_err_to_name(ret));
        return false;
    }

    // Attach SPI device (CS managed manually)
    spi_device_interface_config_t devConfig = {};
    devConfig.clock_speed_hz = chip::display::SPI_FREQ_HZ;
    devConfig.mode = 0;
    devConfig.spics_io_num = -1;  // Manual CS
    devConfig.queue_size = 1;
    devConfig.flags = SPI_DEVICE_HALFDUPLEX;
    // Command: 8 bits, Address: 24 bits (SH8601 QSPI protocol)
    devConfig.command_bits = 8;
    devConfig.address_bits = 24;

    ret = spi_bus_add_device(SPI2_HOST, &devConfig, &m_spi);
    if (ret != ESP_OK) {
        ESP_LOGE("RM690B0", "SPI device add failed: %s", esp_err_to_name(ret));
        spi_bus_free(SPI2_HOST);
        return false;
    }

    // Run panel init sequence
    runInitSequence();

    m_initialized = true;
    return true;
}

void RM690B0Driver::hardwareReset() {
    auto rst = static_cast<gpio_num_t>(chip::gpio::AMOLED_RST);
    gpio_set_direction(rst, GPIO_MODE_OUTPUT);
    gpio_set_level(rst, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(rst, 0);    // Assert reset (active LOW)
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(rst, 1);    // Release reset
    vTaskDelay(pdMS_TO_TICKS(150));  // Wait for RM690B0 to stabilise (harness uses 150ms)
    ESP_LOGI("RM690B0", "Hardware reset complete (GPIO %d)", chip::gpio::AMOLED_RST);
}

void RM690B0Driver::runInitSequence() {
    for (uint8_t i = 0; i < INIT_SEQ_COUNT; i++) {
        const auto& cmd = s_initSequence[i];
        writeCommand(cmd.cmd, cmd.data, cmd.len);
        if (cmd.delayMs > 0) {
            vTaskDelay(pdMS_TO_TICKS(cmd.delayMs));
        }
    }
}

// ============================================================================
// Drawing Operations
// ============================================================================

void RM690B0Driver::setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    // Column address set (0x2A)
    uint8_t colData[4] = {
        static_cast<uint8_t>(x0 >> 8), static_cast<uint8_t>(x0 & 0xFF),
        static_cast<uint8_t>(x1 >> 8), static_cast<uint8_t>(x1 & 0xFF)
    };
    writeCommand(0x2A, colData, 4);

    // Row address set (0x2B)
    uint8_t rowData[4] = {
        static_cast<uint8_t>(y0 >> 8), static_cast<uint8_t>(y0 & 0xFF),
        static_cast<uint8_t>(y1 >> 8), static_cast<uint8_t>(y1 & 0xFF)
    };
    writeCommand(0x2B, rowData, 4);
}

void RM690B0Driver::pushPixels(const uint16_t* data, uint32_t count) {
    if (!m_spi || count == 0) return;

    const uint16_t* p = data;
    uint32_t remaining = count;
    bool firstChunk = true;

    setCS();

    while (remaining > 0) {
        uint32_t chunkPixels = remaining;
        uint32_t maxPixels = MAX_TRANSFER_BYTES / 2;  // 2 bytes per pixel
        if (chunkPixels > maxPixels) chunkPixels = maxPixels;

        spi_transaction_ext_t t = {};

        if (firstChunk) {
            // First chunk: send RAMWR command (0x32 for quad write, address 0x002C00)
            t.base.flags = SPI_TRANS_MODE_QIO;
            t.base.cmd = 0x32;          // Quad write command
            t.base.addr = 0x002C00;     // RAMWR register (middle byte of 24-bit address, wire: 32 00 2C 00)
            firstChunk = false;
        } else {
            // Continuation chunks: no command/address, just data
            t.base.flags = SPI_TRANS_MODE_QIO |
                          SPI_TRANS_VARIABLE_CMD |
                          SPI_TRANS_VARIABLE_ADDR |
                          SPI_TRANS_VARIABLE_DUMMY;
            t.command_bits = 0;
            t.address_bits = 0;
            t.dummy_bits = 0;
        }

        t.base.tx_buffer = p;
        t.base.length = chunkPixels * 16;  // Length in bits (16 bits per pixel)

        spi_device_polling_transmit(m_spi, reinterpret_cast<spi_transaction_t*>(&t));

        p += chunkPixels;
        remaining -= chunkPixels;
    }

    clrCS();
}

// ============================================================================
// Display Control
// ============================================================================

void RM690B0Driver::setBrightness(uint8_t level) {
    writeCommand(0x51, &level, 1);  // WRDISBV
}

void RM690B0Driver::setDisplayOn(bool on) {
    writeCommand(on ? 0x29 : 0x28);  // DISPON / DISPOFF
}

// ============================================================================
// Low-Level SPI
// ============================================================================

void RM690B0Driver::writeCommand(uint8_t cmd, const uint8_t* data, uint32_t len) {
    if (!m_spi) return;

    setCS();

    spi_transaction_t t = {};
    // No MULTILINE flags — opcode 0x02 and address must be single-line.
    // Only pixel data (opcode 0x32 in pushPixels) uses quad I/O.
    t.flags = 0;
    t.cmd = 0x02;                   // Single-line write command prefix
    t.addr = static_cast<uint32_t>(cmd) << 8;   // Register byte in middle of 24-bit address (wire: 02 00 XX 00)

    if (len > 0 && data != nullptr) {
        if (len <= 4) {
            // Small payloads: use inline buffer (avoids DMA from flash-resident static const data)
            t.flags |= SPI_TRANS_USE_TXDATA;
            memcpy(t.tx_data, data, len);
        } else {
            t.tx_buffer = data;
        }
        t.length = 8 * len;        // Length in bits
    }

    spi_device_polling_transmit(m_spi, &t);

    clrCS();
}

void RM690B0Driver::setCS() {
    gpio_set_level(static_cast<gpio_num_t>(chip::gpio::AMOLED_CS), 0);
}

void RM690B0Driver::clrCS() {
    gpio_set_level(static_cast<gpio_num_t>(chip::gpio::AMOLED_CS), 1);
}

} // namespace display
} // namespace lightwaveos

#endif // !NATIVE_BUILD && FEATURE_AMOLED_DISPLAY
