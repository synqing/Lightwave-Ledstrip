// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
#pragma once

/**
 * Bebas Neue Font Family - LVGL Format
 *
 * Brand typography for K1 Tab5 Deck UI
 * Available sizes: 24, 32, 40, 48, 56, 64, 72 px
 */

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Declare all Bebas Neue font sizes
LV_FONT_DECLARE(bebas_neue_24px);
LV_FONT_DECLARE(bebas_neue_32px);
LV_FONT_DECLARE(bebas_neue_40px);
LV_FONT_DECLARE(bebas_neue_48px);
LV_FONT_DECLARE(bebas_neue_56px);
LV_FONT_DECLARE(bebas_neue_64px);
LV_FONT_DECLARE(bebas_neue_72px);

#ifdef __cplusplus
}
#endif

// Helper macros for common UI text sizes
#define BEBAS_LABEL_SMALL   &bebas_neue_24px
#define BEBAS_LABEL_MEDIUM  &bebas_neue_32px
#define BEBAS_LABEL_LARGE   &bebas_neue_40px
#define BEBAS_VALUE_SMALL   &bebas_neue_48px
#define BEBAS_VALUE_MEDIUM  &bebas_neue_56px
#define BEBAS_VALUE_LARGE   &bebas_neue_64px
#define BEBAS_HERO          &bebas_neue_72px
