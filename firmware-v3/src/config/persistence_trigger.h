#pragma once

/**
 * @file persistence_trigger.h
 * @brief Re-exports the g_externalNvsSaveRequest flag from runtime_state.h.
 *
 * Existing code includes this header to access the NVS save trigger.
 * The actual extern declaration now lives in runtime_state.h; this file
 * forwards it for backward compatibility.
 */

#include "runtime_state.h"
