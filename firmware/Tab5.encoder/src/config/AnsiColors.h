#ifndef ANSI_COLORS_H
#define ANSI_COLORS_H

// ANSI color codes for terminal output
// Usage: Serial.printf(ANSI_COLOR_CYAN "[WiFi]" ANSI_RESET " message\n");

#define ANSI_RESET         "\033[0m"

// Basic colors
#define ANSI_BLACK         "\033[30m"
#define ANSI_RED           "\033[31m"
#define ANSI_GREEN         "\033[32m"
#define ANSI_YELLOW        "\033[33m"
#define ANSI_BLUE          "\033[34m"
#define ANSI_MAGENTA       "\033[35m"
#define ANSI_CYAN          "\033[36m"
#define ANSI_WHITE         "\033[37m"

// Bright colors
#define ANSI_BRIGHT_BLACK  "\033[90m"
#define ANSI_BRIGHT_RED    "\033[91m"
#define ANSI_BRIGHT_GREEN  "\033[92m"
#define ANSI_BRIGHT_YELLOW "\033[93m"
#define ANSI_BRIGHT_BLUE   "\033[94m"
#define ANSI_BRIGHT_MAGENTA "\033[95m"
#define ANSI_BRIGHT_CYAN   "\033[96m"
#define ANSI_BRIGHT_WHITE  "\033[97m"

// Background colors
#define ANSI_BG_BLACK      "\033[40m"
#define ANSI_BG_RED        "\033[41m"
#define ANSI_BG_GREEN      "\033[42m"
#define ANSI_BG_YELLOW     "\033[43m"
#define ANSI_BG_BLUE       "\033[44m"
#define ANSI_BG_MAGENTA    "\033[45m"
#define ANSI_BG_CYAN       "\033[46m"
#define ANSI_BG_WHITE      "\033[47m"

// Text styles
#define ANSI_BOLD          "\033[1m"
#define ANSI_DIM           "\033[2m"
#define ANSI_UNDERLINE     "\033[4m"
#define ANSI_BLINK         "\033[5m"
#define ANSI_REVERSE       "\033[7m"

// ============================================================================
// Debug Category Colors (Semantic Mapping)
// ============================================================================

// Critical/System
#define COLOR_WDT          ANSI_BRIGHT_RED        // Watchdog
#define COLOR_INIT         ANSI_BRIGHT_GREEN      // Initialization
#define COLOR_OK           ANSI_BRIGHT_GREEN      // Success markers

// Network
#define COLOR_WIFI         ANSI_CYAN              // WiFi operations
#define COLOR_NETWORK      ANSI_BLUE              // Network layer
#define COLOR_WEBSOCKET    ANSI_BRIGHT_BLUE       // WebSocket

// Storage/State
#define COLOR_NVS          ANSI_MAGENTA           // NVS storage
#define COLOR_PRESET       ANSI_BRIGHT_MAGENTA    // Preset management

// UI/Display
#define COLOR_UI           ANSI_YELLOW            // UI operations
#define COLOR_DISPLAY      ANSI_BRIGHT_YELLOW     // Display rendering
#define COLOR_TOUCH        ANSI_YELLOW            // Touch events

// Input
#define COLOR_BUTTON       ANSI_BLUE              // Button events
#define COLOR_LED          ANSI_GREEN             // LED feedback
#define COLOR_COARSE       ANSI_BRIGHT_CYAN       // Coarse mode

// I2C/Hardware
#define COLOR_I2C          ANSI_MAGENTA           // I2C operations
#define COLOR_I2C_RECOVERY ANSI_BRIGHT_RED        // I2C recovery

// Diagnostics
#define COLOR_DEBUG        ANSI_BRIGHT_BLACK      // Debug traces
#define COLOR_STATUS       ANSI_CYAN              // Status reports
#define COLOR_LOOP_TRACE   ANSI_DIM              // Loop traces (dim)
#define COLOR_CT_TRACE     ANSI_DIM              // Coarse tracking trace
#define COLOR_DISPLAYUI_TRACE ANSI_DIM           // DisplayUI trace
#define COLOR_ZC_TRACE     ANSI_DIM              // ZoneComposer trace

// OTA
#define COLOR_OTA          ANSI_BRIGHT_MAGENTA    // OTA updates

// Zone System
#define COLOR_ZONE         ANSI_YELLOW            // Zone operations

// ============================================================================
// Convenience Macros
// ============================================================================

#define LOG_WDT(fmt, ...)         Serial.printf(COLOR_WDT "[WDT]" ANSI_RESET " " fmt "\n", ##__VA_ARGS__)
#define LOG_INIT(fmt, ...)        Serial.printf(COLOR_INIT "[INIT]" ANSI_RESET " " fmt "\n", ##__VA_ARGS__)
#define LOG_OK(fmt, ...)          Serial.printf(COLOR_OK "[OK]" ANSI_RESET " " fmt "\n", ##__VA_ARGS__)
#define LOG_WIFI(fmt, ...)        Serial.printf(COLOR_WIFI "[WiFi]" ANSI_RESET " " fmt "\n", ##__VA_ARGS__)
#define LOG_NETWORK(fmt, ...)     Serial.printf(COLOR_NETWORK "[NETWORK]" ANSI_RESET " " fmt "\n", ##__VA_ARGS__)
#define LOG_WEBSOCKET(fmt, ...)   Serial.printf(COLOR_WEBSOCKET "[WS]" ANSI_RESET " " fmt "\n", ##__VA_ARGS__)
#define LOG_NVS(fmt, ...)         Serial.printf(COLOR_NVS "[NVS]" ANSI_RESET " " fmt "\n", ##__VA_ARGS__)
#define LOG_PRESET(fmt, ...)      Serial.printf(COLOR_PRESET "[PRESET]" ANSI_RESET " " fmt "\n", ##__VA_ARGS__)
#define LOG_UI(fmt, ...)          Serial.printf(COLOR_UI "[UI]" ANSI_RESET " " fmt "\n", ##__VA_ARGS__)
#define LOG_DISPLAY(fmt, ...)     Serial.printf(COLOR_DISPLAY "[Display]" ANSI_RESET " " fmt "\n", ##__VA_ARGS__)
#define LOG_TOUCH(fmt, ...)       Serial.printf(COLOR_TOUCH "[TOUCH]" ANSI_RESET " " fmt "\n", ##__VA_ARGS__)
#define LOG_BUTTON(fmt, ...)      Serial.printf(COLOR_BUTTON "[Button]" ANSI_RESET " " fmt "\n", ##__VA_ARGS__)
#define LOG_LED(fmt, ...)         Serial.printf(COLOR_LED "[LED]" ANSI_RESET " " fmt "\n", ##__VA_ARGS__)
#define LOG_COARSE(fmt, ...)      Serial.printf(COLOR_COARSE "[CoarseMode]" ANSI_RESET " " fmt "\n", ##__VA_ARGS__)
#define LOG_I2C(fmt, ...)         Serial.printf(COLOR_I2C "[I2C]" ANSI_RESET " " fmt "\n", ##__VA_ARGS__)
#define LOG_I2C_RECOVERY(fmt, ...) Serial.printf(COLOR_I2C_RECOVERY "[I2C_RECOVERY]" ANSI_RESET " " fmt "\n", ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...)       Serial.printf(COLOR_DEBUG "[DEBUG]" ANSI_RESET " " fmt "\n", ##__VA_ARGS__)
#define LOG_STATUS(fmt, ...)      Serial.printf(COLOR_STATUS "[STATUS]" ANSI_RESET " " fmt "\n", ##__VA_ARGS__)
#define LOG_LOOP_TRACE(fmt, ...)  Serial.printf(COLOR_LOOP_TRACE "[LOOP_TRACE]" ANSI_RESET " " fmt "\n", ##__VA_ARGS__)
#define LOG_CT_TRACE(fmt, ...)    Serial.printf(COLOR_CT_TRACE "[CT_TRACE]" ANSI_RESET " " fmt "\n", ##__VA_ARGS__)
#define LOG_DISPLAYUI_TRACE(fmt, ...) Serial.printf(COLOR_DISPLAYUI_TRACE "[DisplayUI_TRACE]" ANSI_RESET " " fmt "\n", ##__VA_ARGS__)
#define LOG_ZC_TRACE(fmt, ...)    Serial.printf(COLOR_ZC_TRACE "[ZC_TRACE]" ANSI_RESET " " fmt "\n", ##__VA_ARGS__)
#define LOG_OTA(fmt, ...)         Serial.printf(COLOR_OTA "[OTA]" ANSI_RESET " " fmt "\n", ##__VA_ARGS__)
#define LOG_ZONE(fmt, ...)        Serial.printf(COLOR_ZONE "[ZoneComposer]" ANSI_RESET " " fmt "\n", ##__VA_ARGS__)

#endif // ANSI_COLORS_H
