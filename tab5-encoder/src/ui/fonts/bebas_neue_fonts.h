// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
#pragma once

/**
 * Bebas Neue Font Family - LVGL Format
 *
 * Brand typography for K1 Tab5 Deck UI
 * Available sizes: 24, 40 px
 */

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

LV_FONT_DECLARE(bebas_neue_24px);
LV_FONT_DECLARE(bebas_neue_40px);

#ifdef __cplusplus
}
#endif

// Helper macros for common UI text sizes
#define BEBAS_LABEL_SMALL   &bebas_neue_24px
#define BEBAS_LABEL_LARGE   &bebas_neue_40px
