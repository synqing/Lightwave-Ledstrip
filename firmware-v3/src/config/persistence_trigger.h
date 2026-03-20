#pragma once

#include <atomic>

/// Global flag for triggering debounced NVS save from network handlers.
/// Set by REST/WS handlers (Core 0), cleared by main loop (Core 1).
/// Defined in main.cpp.
extern std::atomic<bool> g_externalNvsSaveRequest;
