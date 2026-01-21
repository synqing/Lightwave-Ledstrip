#!/usr/bin/env python3
"""
Demo script showing what the colorized debug output will look like.
Run this to preview the color scheme in your terminal.
"""

# ANSI color codes
ANSI_RESET = "\033[0m"
ANSI_RED = "\033[31m"
ANSI_GREEN = "\033[32m"
ANSI_YELLOW = "\033[33m"
ANSI_BLUE = "\033[34m"
ANSI_MAGENTA = "\033[35m"
ANSI_CYAN = "\033[36m"
ANSI_BRIGHT_RED = "\033[91m"
ANSI_BRIGHT_GREEN = "\033[92m"
ANSI_BRIGHT_YELLOW = "\033[93m"
ANSI_BRIGHT_BLUE = "\033[94m"
ANSI_BRIGHT_MAGENTA = "\033[95m"
ANSI_BRIGHT_CYAN = "\033[96m"
ANSI_BRIGHT_BLACK = "\033[90m"
ANSI_DIM = "\033[2m"

print("\n" + "=" * 80)
print("Tab5.encoder Color Scheme Preview")
print("=" * 80 + "\n")

categories = [
    (ANSI_BRIGHT_RED, "[WDT]", "Watchdog initialized (5s timeout)"),
    (ANSI_BRIGHT_GREEN, "[INIT]", "M5Stack Tab5 initialized"),
    (ANSI_BRIGHT_GREEN, "[OK]", "Both units detected - 16 encoders available!"),
    (ANSI_CYAN, "[WiFi]", "Starting connection..."),
    (ANSI_BLUE, "[NETWORK]", "WiFi connected! IP: 192.168.1.100"),
    (ANSI_BRIGHT_BLUE, "[WS]", "Connecting to ws://192.168.1.50:8080/ws..."),
    (ANSI_MAGENTA, "[NVS]", "Restored 12 parameters from flash"),
    (ANSI_BRIGHT_MAGENTA, "[PRESET]", "Initialized with 8 stored presets"),
    (ANSI_YELLOW, "[UI]", "Display initialized (800x480)"),
    (ANSI_YELLOW, "[TOUCH]", "Touch handler initialized"),
    (ANSI_BLUE, "[Button]", "Button handler initialized"),
    (ANSI_GREEN, "[LED]", "Connection status LED feedback initialized"),
    (ANSI_BRIGHT_CYAN, "[CoarseMode]", "Coarse mode manager initialized"),
    (ANSI_BRIGHT_RED, "[I2C_RECOVERY]", "Recovery module initialized"),
    (ANSI_BRIGHT_MAGENTA, "[OTA]", "Update started, expecting 2MB"),
    (ANSI_YELLOW, "[ZoneComposer]", "Zone 0: Full Strip (0-319)"),
    (ANSI_BRIGHT_BLACK, "[DEBUG]", "After WiFi.begin - Heap: free=245520"),
    (ANSI_CYAN, "[STATUS]", "A:OK B:OK WiFi:connected WS:connected"),
    (ANSI_DIM, "[LOOP_TRACE]", "loop() running @ 12345 ms"),
]

for color, tag, message in categories:
    print(f"{color}{tag}{ANSI_RESET} {message}")

print("\n" + "=" * 80)
print("Critical/Error categories use BRIGHT colors for high visibility")
print("Trace categories use DIM to reduce visual noise")
print("=" * 80 + "\n")
