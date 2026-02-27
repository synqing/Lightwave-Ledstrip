// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
#pragma once

/**
 * Experimental Font Collection - LVGL Format
 *
 * Additional font weights and families for UI experimentation
 * Available sizes: 24, 32, 40, 48 px
 */

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Bebas Neue Bold (4 sizes)
// ============================================================================
LV_FONT_DECLARE(bebas_neue_bold_24px);
LV_FONT_DECLARE(bebas_neue_bold_32px);
LV_FONT_DECLARE(bebas_neue_bold_40px);
LV_FONT_DECLARE(bebas_neue_bold_48px);

// ============================================================================
// Bebas Neue Light (4 sizes)
// ============================================================================
LV_FONT_DECLARE(bebas_neue_light_24px);
LV_FONT_DECLARE(bebas_neue_light_32px);
LV_FONT_DECLARE(bebas_neue_light_40px);
LV_FONT_DECLARE(bebas_neue_light_48px);

// ============================================================================
// Rajdhani Regular (4 sizes)
// ============================================================================
LV_FONT_DECLARE(rajdhani_regular_24px);
LV_FONT_DECLARE(rajdhani_regular_32px);
LV_FONT_DECLARE(rajdhani_regular_40px);
LV_FONT_DECLARE(rajdhani_regular_48px);

// ============================================================================
// Rajdhani Medium (4 sizes)
// ============================================================================
LV_FONT_DECLARE(rajdhani_medium_24px);
LV_FONT_DECLARE(rajdhani_medium_32px);
LV_FONT_DECLARE(rajdhani_medium_40px);
LV_FONT_DECLARE(rajdhani_medium_48px);

// ============================================================================
// Rajdhani Bold (4 sizes)
// ============================================================================
LV_FONT_DECLARE(rajdhani_bold_24px);
LV_FONT_DECLARE(rajdhani_bold_32px);
LV_FONT_DECLARE(rajdhani_bold_40px);
LV_FONT_DECLARE(rajdhani_bold_48px);

// ============================================================================
// JetBrains Mono Regular (4 sizes)
// ============================================================================
LV_FONT_DECLARE(jetbrains_mono_regular_24px);
LV_FONT_DECLARE(jetbrains_mono_regular_32px);
LV_FONT_DECLARE(jetbrains_mono_regular_40px);
LV_FONT_DECLARE(jetbrains_mono_regular_48px);

// ============================================================================
// JetBrains Mono Bold (4 sizes)
// ============================================================================
LV_FONT_DECLARE(jetbrains_mono_bold_24px);
LV_FONT_DECLARE(jetbrains_mono_bold_32px);
LV_FONT_DECLARE(jetbrains_mono_bold_40px);
LV_FONT_DECLARE(jetbrains_mono_bold_48px);

#ifdef __cplusplus
}
#endif

// ============================================================================
// Helper Macros for Quick Access
// ============================================================================

// Bebas Neue Bold
#define BEBAS_BOLD_24  &bebas_neue_bold_24px
#define BEBAS_BOLD_32  &bebas_neue_bold_32px
#define BEBAS_BOLD_40  &bebas_neue_bold_40px
#define BEBAS_BOLD_48  &bebas_neue_bold_48px

// Bebas Neue Light
#define BEBAS_LIGHT_24 &bebas_neue_light_24px
#define BEBAS_LIGHT_32 &bebas_neue_light_32px
#define BEBAS_LIGHT_40 &bebas_neue_light_40px
#define BEBAS_LIGHT_48 &bebas_neue_light_48px

// Rajdhani Regular
#define RAJDHANI_REG_24  &rajdhani_regular_24px
#define RAJDHANI_REG_32  &rajdhani_regular_32px
#define RAJDHANI_REG_40  &rajdhani_regular_40px
#define RAJDHANI_REG_48  &rajdhani_regular_48px

// Rajdhani Medium
#define RAJDHANI_MED_24  &rajdhani_medium_24px
#define RAJDHANI_MED_32  &rajdhani_medium_32px
#define RAJDHANI_MED_40  &rajdhani_medium_40px
#define RAJDHANI_MED_48  &rajdhani_medium_48px

// Rajdhani Bold
#define RAJDHANI_BOLD_24 &rajdhani_bold_24px
#define RAJDHANI_BOLD_32 &rajdhani_bold_32px
#define RAJDHANI_BOLD_40 &rajdhani_bold_40px
#define RAJDHANI_BOLD_48 &rajdhani_bold_48px

// JetBrains Mono Regular
#define JETBRAINS_MONO_REG_24 &jetbrains_mono_regular_24px
#define JETBRAINS_MONO_REG_32 &jetbrains_mono_regular_32px
#define JETBRAINS_MONO_REG_40 &jetbrains_mono_regular_40px
#define JETBRAINS_MONO_REG_48 &jetbrains_mono_regular_48px

// JetBrains Mono Bold
#define JETBRAINS_MONO_BOLD_24 &jetbrains_mono_bold_24px
#define JETBRAINS_MONO_BOLD_32 &jetbrains_mono_bold_32px
#define JETBRAINS_MONO_BOLD_40 &jetbrains_mono_bold_40px
#define JETBRAINS_MONO_BOLD_48 &jetbrains_mono_bold_48px

