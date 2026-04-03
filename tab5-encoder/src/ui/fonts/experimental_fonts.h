// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
#pragma once

/**
 * Experimental Font Collection - LVGL Format
 *
 * Additional font weights and families for UI experimentation
 */

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Bebas Neue Bold (3 sizes)
// ============================================================================
LV_FONT_DECLARE(bebas_neue_bold_24px);
LV_FONT_DECLARE(bebas_neue_bold_32px);
LV_FONT_DECLARE(bebas_neue_bold_40px);

// ============================================================================
// Rajdhani Medium (2 sizes)
// ============================================================================
LV_FONT_DECLARE(rajdhani_medium_18px);
LV_FONT_DECLARE(rajdhani_medium_24px);

// ============================================================================
// Rajdhani Bold (2 sizes)
// ============================================================================
LV_FONT_DECLARE(rajdhani_bold_24px);
LV_FONT_DECLARE(rajdhani_bold_32px);

// ============================================================================
// JetBrains Mono Regular (2 sizes)
// ============================================================================
LV_FONT_DECLARE(jetbrains_mono_regular_24px);
LV_FONT_DECLARE(jetbrains_mono_regular_32px);

// ============================================================================
// JetBrains Mono Bold (2 sizes)
// ============================================================================
LV_FONT_DECLARE(jetbrains_mono_bold_24px);
LV_FONT_DECLARE(jetbrains_mono_bold_32px);

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

// Rajdhani Medium
#define RAJDHANI_MED_18  &rajdhani_medium_18px
#define RAJDHANI_MED_24  &rajdhani_medium_24px

// Rajdhani Bold
#define RAJDHANI_BOLD_24 &rajdhani_bold_24px
#define RAJDHANI_BOLD_32 &rajdhani_bold_32px

// JetBrains Mono Regular
#define JETBRAINS_MONO_REG_24 &jetbrains_mono_regular_24px
#define JETBRAINS_MONO_REG_32 &jetbrains_mono_regular_32px

// JetBrains Mono Bold
#define JETBRAINS_MONO_BOLD_24 &jetbrains_mono_bold_24px
#define JETBRAINS_MONO_BOLD_32 &jetbrains_mono_bold_32px
