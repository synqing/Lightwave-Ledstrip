// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
#pragma once

#include <M5Unified.h>
#include <lvgl.h>

namespace LVGLBridge {
bool init();
void update();
lv_display_t* getDisplay();
lv_indev_t* getTouchDevice();
}

