// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
#pragma once

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Bebas Neue fonts in various sizes
extern lv_font_t bebas_neue_lvgl_24;
extern lv_font_t bebas_neue_lvgl_36;
extern lv_font_t bebas_neue_lvgl_48;
extern lv_font_t bebas_neue_lvgl_64;

#ifdef __cplusplus
}
#endif
