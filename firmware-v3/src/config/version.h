// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file version.h
 * @brief Firmware version constants for LightwaveOS v2
 *
 * Single source of truth for firmware version information.
 * Used by OTA validation, device info endpoints, and telemetry.
 *
 * Version comparison uses FIRMWARE_VERSION_NUMBER which encodes
 * MAJOR*10000 + MINOR*100 + PATCH as a single uint32_t for simple
 * numeric comparison (e.g., 2.1.3 -> 20103).
 */

#pragma once

#include <stdint.h>

// ============================================================================
// Firmware Version Components
// ============================================================================

#define FIRMWARE_VERSION_MAJOR  2
#define FIRMWARE_VERSION_MINOR  0
#define FIRMWARE_VERSION_PATCH  0

// ============================================================================
// Derived Version Identifiers
// ============================================================================

/**
 * @brief Human-readable version string (e.g., "2.0.0")
 *
 * Overridable via build flag: -D FIRMWARE_VERSION_STRING=\"2.0.1-beta\"
 */
#ifndef FIRMWARE_VERSION_STRING
#define FIRMWARE_VERSION_STRING "2.0.0"
#endif

/**
 * @brief Numeric version for comparison (MAJOR*10000 + MINOR*100 + PATCH)
 *
 * Examples:
 *   2.0.0  -> 20000
 *   2.1.0  -> 20100
 *   2.1.3  -> 20103
 *   3.0.0  -> 30000
 *
 * Use: if (incomingVersion < FIRMWARE_VERSION_NUMBER) { ... downgrade ... }
 */
#define FIRMWARE_VERSION_NUMBER \
    ((uint32_t)(FIRMWARE_VERSION_MAJOR * 10000 + FIRMWARE_VERSION_MINOR * 100 + FIRMWARE_VERSION_PATCH))

/**
 * @brief Parse a "MAJOR.MINOR.PATCH" string into a numeric version number.
 *
 * Returns 0 on parse failure. Designed for use at runtime when comparing
 * an incoming version string against FIRMWARE_VERSION_NUMBER.
 *
 * @param versionStr  Null-terminated version string (e.g., "2.1.3")
 * @return uint32_t   Numeric version (MAJOR*10000 + MINOR*100 + PATCH), or 0 on failure
 */
inline uint32_t parseVersionNumber(const char* versionStr) {
    if (!versionStr || versionStr[0] == '\0') return 0;

    uint32_t major = 0, minor = 0, patch = 0;
    int fieldsRead = 0;

    // Simple manual parse to avoid sscanf (not always available on all platforms)
    const char* p = versionStr;

    // Skip leading 'v' or 'V' if present
    if (*p == 'v' || *p == 'V') p++;

    // Parse major
    while (*p >= '0' && *p <= '9') {
        major = major * 10 + (*p - '0');
        p++;
    }
    fieldsRead++;

    if (*p == '.') {
        p++;
        // Parse minor
        while (*p >= '0' && *p <= '9') {
            minor = minor * 10 + (*p - '0');
            p++;
        }
        fieldsRead++;

        if (*p == '.') {
            p++;
            // Parse patch
            while (*p >= '0' && *p <= '9') {
                patch = patch * 10 + (*p - '0');
                p++;
            }
            fieldsRead++;
        }
    }

    // Require at least major.minor
    if (fieldsRead < 2) return 0;

    // Sanity bounds
    if (major > 99 || minor > 99 || patch > 99) return 0;

    return major * 10000 + minor * 100 + patch;
}
