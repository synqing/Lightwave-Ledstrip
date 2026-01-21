#pragma once

#include <M5Unified.h>
#include <lvgl.h>

namespace LVGLBridge {
bool init();
void update();
lv_display_t* getDisplay();
lv_indev_t* getTouchDevice();
}

