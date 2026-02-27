// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
// ============================================================================
// LoadingScreen Implementation
// ============================================================================
// Supports both legacy M5GFX direct rendering and new LVGL-based UI.
// ============================================================================

#include "LoadingScreen.h"

#ifdef SIMULATOR_BUILD
    #include "../hal/SimHal.h"
#else
    #include <Arduino.h>
#endif

#include "../hal/EspHal.h"
#include "Theme.h"
#include "SpectraSynqLogo.h"
#include "../config/Config.h"

#ifndef SIMULATOR_BUILD
    #include <M5Unified.h>
    #include <Arduino.h>
    #include <pgmspace.h>
#endif

#include <cstring>

// Common includes end here.

#if defined(TAB5_ENCODER_USE_LVGL) && (TAB5_ENCODER_USE_LVGL) && !defined(SIMULATOR_BUILD)
#include "lvgl_bridge.h"
#include "fonts/experimental_fonts.h"
#endif

namespace LoadingScreen {

#if defined(TAB5_ENCODER_USE_LVGL) && (TAB5_ENCODER_USE_LVGL) && !defined(SIMULATOR_BUILD)

// ============================================================================
// LVGL Implementation
// ============================================================================

static lv_obj_t* s_screen = nullptr;
static lv_obj_t* s_subtitleLabel = nullptr;
static lv_obj_t* s_dotsLabel = nullptr;
static lv_timer_t* s_dotsTimer = nullptr;
static uint8_t s_dotState = 0;
static char s_currentSubtitle[64] = {0};  // Track current subtitle to control dots animation

static const lv_image_dsc_t s_logoDsc = {
    .header = {
        .magic = LV_IMAGE_HEADER_MAGIC,
        .cf = LV_COLOR_FORMAT_RGB565,
        .flags = 0,
        .w = SPECTRASYNQ_LOGO_SMALL_WIDTH,
        .h = SPECTRASYNQ_LOGO_SMALL_HEIGHT,
        .stride = SPECTRASYNQ_LOGO_SMALL_WIDTH * 2,
        .reserved_2 = 0
    },
    .data_size = SPECTRASYNQ_LOGO_SMALL_WIDTH * SPECTRASYNQ_LOGO_SMALL_HEIGHT * 2,
    .data = (const uint8_t*)SpectraSynq_Logo_Small
};

static void dots_timer_cb(lv_timer_t* timer) {
    if (!s_dotsLabel) return;
    
    // Always animate dots for "CONNECTING TO HOST" main label (dots are part of main label, not subtitle)
    // Animate dots one at a time: dot, dot, dot (cycling through 0, 1, 2, 3)
    s_dotState = (s_dotState + 1) % 4;
    switch (s_dotState) {
        case 0: lv_label_set_text(s_dotsLabel, ""); break;
        case 1: lv_label_set_text(s_dotsLabel, "."); break;
        case 2: lv_label_set_text(s_dotsLabel, ".."); break;
        case 3: lv_label_set_text(s_dotsLabel, "..."); break;
    }
    // Re-align dots to maintain position relative to main label (which is centered)
    // Find the main label by getting the parent and finding the first child
    lv_obj_t* parent = lv_obj_get_parent(s_dotsLabel);
    if (parent) {
        lv_obj_t* mainLabel = lv_obj_get_child(parent, 0);
        if (mainLabel) {
            lv_obj_align_to(s_dotsLabel, mainLabel, LV_ALIGN_OUT_RIGHT_MID, 4, 0);
        }
    }
}

// create_badge function removed - encoder badges no longer used

// update_badges function removed - encoder badges no longer displayed

void show(M5GFX& display, const char* message, bool unitA, bool unitB) {
    (void)display; // Unused in LVGL mode
    if (s_screen) {
        update(display, message, unitA, unitB);
        return;
    }

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x0A0A0B), 0); // TAB5_COLOR_BG_PAGE
    lv_obj_set_style_text_font(s_screen, BEBAS_BOLD_24, 0);

    // Layout Container
    lv_obj_t* cont = lv_obj_create(s_screen);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
    lv_obj_center(cont);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(cont, 0, 0);
    lv_obj_set_style_border_width(cont, 0, 0);

    // Logo - no scaling, use native size from SpectraSynqLogo.h
    lv_obj_t* logo = lv_image_create(cont);
    lv_image_set_src(logo, &s_logoDsc);
    // No scaling - logo displays at native resolution (1170x1478px from converted JPG)
    lv_obj_set_style_margin_top(logo, -15, 0); // Move logo up 15px
    
    // Container for main label and dots - use fixed layout to prevent text movement
    lv_obj_t* mainLabelCont = lv_obj_create(cont);
    lv_obj_set_size(mainLabelCont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(mainLabelCont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(mainLabelCont, 0, 0);
    lv_obj_set_style_pad_all(mainLabelCont, 0, 0);
    lv_obj_set_style_margin_top(mainLabelCont, 24, 0); // GAP_LOGO_TO_TEXT from working impl
    lv_obj_clear_flag(mainLabelCont, LV_OBJ_FLAG_SCROLLABLE);
    
    // Main Label - locked position, text never changes, centered (position doesn't move)
    lv_obj_t* mainLabel = lv_label_create(mainLabelCont);
    lv_label_set_text(mainLabel, "CONNECTING TO HOST");
    lv_obj_set_style_text_color(mainLabel, lv_color_hex(0xFFC700), 0); // TAB5_COLOR_BRAND_PRIMARY
    lv_obj_set_style_text_font(mainLabel, BEBAS_BOLD_24, 0);
    lv_obj_set_style_text_letter_space(mainLabel, 2, 0); // Slight letter spacing for better appearance
    lv_obj_align(mainLabel, LV_ALIGN_CENTER, 0, 0); // Center the main label - position is fixed

    // Dots - positioned absolutely relative to main label, doesn't affect main label position
    s_dotsLabel = lv_label_create(mainLabelCont);
    lv_label_set_text(s_dotsLabel, "");
    lv_obj_set_style_text_color(s_dotsLabel, lv_color_hex(0xFFC700), 0);
    lv_obj_set_style_text_font(s_dotsLabel, BEBAS_BOLD_24, 0);
    // Position dots to the right of "CONNECTING TO HOST" text - absolute positioning
    lv_obj_align_to(s_dotsLabel, mainLabel, LV_ALIGN_OUT_RIGHT_MID, 4, 0);

    // Subtitle
    s_subtitleLabel = lv_label_create(cont);
    const char* displayMessage = message ? message : "";
    lv_label_set_text(s_subtitleLabel, displayMessage);
    lv_obj_set_style_text_color(s_subtitleLabel, lv_color_hex(0x848484), 0); // TAB5_COLOR_FG_SECONDARY
    lv_obj_set_style_text_font(s_subtitleLabel, BEBAS_BOLD_24, 0);
    lv_obj_set_style_margin_top(s_subtitleLabel, 10, 0);
    // Initialize subtitle tracking for dots animation control
    strncpy(s_currentSubtitle, displayMessage, sizeof(s_currentSubtitle) - 1);
    s_currentSubtitle[sizeof(s_currentSubtitle) - 1] = '\0';

    // Encoder badges removed - not needed on loading screen per user request

    s_dotsTimer = lv_timer_create(dots_timer_cb, 500, NULL);

    lv_display_t* disp = LVGLBridge::getDisplay();
    if (disp) {
        lv_disp_load_scr(s_screen);
    }
}

void update(M5GFX& display, const char* message, bool unitA, bool unitB) {
    (void)display;
    (void)unitA;  // Encoder status no longer displayed
    (void)unitB;  // Encoder status no longer displayed
    if (!s_screen) return;
    
    if (s_subtitleLabel) {
        const char* displayMessage = message ? message : "";
        lv_label_set_text(s_subtitleLabel, displayMessage);
        // Store current subtitle (for potential future use, but dots always animate for main label)
        strncpy(s_currentSubtitle, displayMessage, sizeof(s_currentSubtitle) - 1);
        s_currentSubtitle[sizeof(s_currentSubtitle) - 1] = '\0';
        // Dots always animate - they're part of the "CONNECTING TO HOST" main label, not the subtitle
    }
}

void hide(M5GFX& display) {
    (void)display;
    if (s_dotsTimer) {
        lv_timer_del(s_dotsTimer);
        s_dotsTimer = nullptr;
    }
    if (s_screen) {
        lv_obj_del(s_screen);
        s_screen = nullptr;
        s_subtitleLabel = nullptr;
        s_dotsLabel = nullptr;
    }
}

// No-ops for M5GFX specific functions in LVGL mode
void setPpaEnabled(bool enabled) { (void)enabled; }
bool isPpaEnabled() { return false; }
uint32_t benchmarkLogo(M5GFX& display, uint16_t iterations, bool usePpa) { 
    (void)display; (void)iterations; (void)usePpa; return 0; 
}

#else

// ============================================================================
// M5GFX Implementation
// ============================================================================

#ifndef SIMULATOR_BUILD
    #if ENABLE_PPA_UI && !defined(SIMULATOR_BUILD)
    #include <LGFX_PPA.hpp>
    #endif
#else
    // PPA not available in simulator - disable it
    #undef ENABLE_PPA_UI
    #define ENABLE_PPA_UI 0
    // pgm_read_word macro for simulator (direct memory access)
    #define pgm_read_word(addr) (*(const uint16_t*)(addr))
#endif

static uint32_t s_lastDotUpdate = 0;
static uint8_t s_dotState = 0;  // 0="", 1=".", 2="..", 3="..."

static constexpr uint16_t TAB5_COLOR_BG_PAGE_RGB565 = 0x0841;       // RGB888 0x0A0A0B
static constexpr uint16_t TAB5_COLOR_BRAND_PRIMARY_RGB565 = 0xFE20; // RGB888 0xFFC700
static constexpr uint16_t TAB5_COLOR_FG_SECONDARY_RGB565 = 0x8410;

static constexpr const char* MAIN_LABEL = "CONNECTING TO HOST";

static constexpr uint32_t DOT_INTERVAL_MS = 500;
static constexpr int DOT_GAP_PX = 10;

static constexpr int LOGO_SCALE = 2;  // Nearest-neighbour scale factor for logo (used in non-LVGL paths)

static_assert(LOGO_SCALE == 2, "LoadingScreen logo scaling currently assumes a fixed 2x scale.");

struct DotLayout {
    int x = 0;
    int y = 0;
    int clearW = 0;
    int clearH = 0;
};

static DotLayout s_dotLayout;
static char s_lastSubtitle[64] = {0};
static bool s_lastUnitA = false;
static bool s_lastUnitB = false;

#if ENABLE_PPA_UI && !defined(SIMULATOR_BUILD)
static lgfx::PPA_Sprite s_logoSprite;
static lgfx::PPASrm* s_ppaSrm = nullptr;
static bool s_ppaLogoReady = false;
static bool s_ppaInitFailed = false;
static bool s_ppaEnabledRuntime = true;
#endif  // ENABLE_PPA_UI && !SIMULATOR_BUILD

#if ENABLE_PPA_UI && !defined(SIMULATOR_BUILD)
static bool initPpaLogo(M5GFX& display) {
    if (s_ppaLogoReady) return true;
    if (s_ppaInitFailed) return false;

    if (!s_logoSprite.createSprite(SPECTRASYNQ_LOGO_SMALL_WIDTH, SPECTRASYNQ_LOGO_SMALL_HEIGHT)) {
        s_ppaInitFailed = true;
        return false;
    }

    static uint16_t lineBuf[SPECTRASYNQ_LOGO_SMALL_WIDTH];
    for (int srcY = 0; srcY < SPECTRASYNQ_LOGO_SMALL_HEIGHT; ++srcY) {
        for (int srcX = 0; srcX < SPECTRASYNQ_LOGO_SMALL_WIDTH; ++srcX) {
            lineBuf[srcX] = pgm_read_word(&SpectraSynq_Logo_Small[srcY * SPECTRASYNQ_LOGO_SMALL_WIDTH + srcX]);
        }
        s_logoSprite.pushImage(0, srcY, SPECTRASYNQ_LOGO_SMALL_WIDTH, 1, lineBuf);
    }

    s_ppaSrm = new lgfx::PPASrm(&display, false);
    s_ppaSrm->setRotation(0);
    s_ppaSrm->setMirror(false, false);

    s_ppaLogoReady = s_ppaSrm->available();
    if (!s_ppaLogoReady) {
        s_ppaInitFailed = true;
    }

    return s_ppaLogoReady;
}
#endif  // ENABLE_PPA_UI && !SIMULATOR_BUILD

const char* getDots() {
    uint32_t now = EspHal::millis();
    // Handle millis() wrap-around: if now < s_lastDotUpdate, subtraction will wrap correctly
    if ((uint32_t)(now - s_lastDotUpdate) >= DOT_INTERVAL_MS) {
        s_lastDotUpdate = now;
        s_dotState = (s_dotState + 1) % 4;
    }
    
    switch (s_dotState) {
        case 0: return "";
        case 1: return ".";
        case 2: return "..";
        case 3: return "...";
        default: return "";
    }
}

static bool strEq(const char* a, const char* b) {
    if (a == b) return true;
    if (!a || !b) return false;
    return std::strcmp(a, b) == 0;
}

static void normaliseSubtitle(const char* subtitle, char* out, size_t outSize) {
    if (!out || outSize == 0) return;

    if (!subtitle || subtitle[0] == '\0' || strEq(subtitle, MAIN_LABEL)) {
        out[0] = '\0';
        return;
    }

    std::strncpy(out, subtitle, outSize - 1);
    out[outSize - 1] = '\0';
}

static void drawLogoScaled(M5GFX& display, int dstX, int dstY) {
    bool usePpa = false;
#if ENABLE_PPA_UI && !defined(SIMULATOR_BUILD)
    usePpa = s_ppaEnabledRuntime;
    if (usePpa && initPpaLogo(display) && s_ppaSrm && s_ppaSrm->available()) {
        s_ppaSrm->pushSRM(&s_logoSprite, dstX, dstY, LOGO_SCALE, LOGO_SCALE);
        return;
    }
#endif
    static uint16_t lineBuf[SPECTRASYNQ_LOGO_SMALL_WIDTH * LOGO_SCALE];

    display.startWrite();
    for (int srcY = 0; srcY < SPECTRASYNQ_LOGO_SMALL_HEIGHT; ++srcY) {
        for (int srcX = 0; srcX < SPECTRASYNQ_LOGO_SMALL_WIDTH; ++srcX) {
            const uint16_t px = pgm_read_word(&SpectraSynq_Logo_Small[srcY * SPECTRASYNQ_LOGO_SMALL_WIDTH + srcX]);
            lineBuf[srcX * 2] = px;
            lineBuf[srcX * 2 + 1] = px;
        }

        const int dstRow = dstY + (srcY * LOGO_SCALE);
        display.pushImage(dstX, dstRow, SPECTRASYNQ_LOGO_SMALL_WIDTH * LOGO_SCALE, 1, lineBuf);
        display.pushImage(dstX, dstRow + 1, SPECTRASYNQ_LOGO_SMALL_WIDTH * LOGO_SCALE, 1, lineBuf);
    }
    display.endWrite();
}

void setPpaEnabled(bool enabled) {
#if ENABLE_PPA_UI && !defined(SIMULATOR_BUILD)
    s_ppaEnabledRuntime = enabled;
#else
    (void)enabled;
#endif
}

bool isPpaEnabled() {
#if ENABLE_PPA_UI && !defined(SIMULATOR_BUILD)
    return s_ppaEnabledRuntime;
#else
    return false;
#endif
}

uint32_t benchmarkLogo(M5GFX& display, uint16_t iterations, bool usePpa) {
    if (iterations == 0) return 0;

    const int dstX = (Theme::SCREEN_W / 2) - (SPECTRASYNQ_LOGO_SMALL_WIDTH * LOGO_SCALE / 2);
    const int dstY = (Theme::SCREEN_H / 2) - (SPECTRASYNQ_LOGO_SMALL_HEIGHT * LOGO_SCALE / 2);

    bool prev = isPpaEnabled();
    setPpaEnabled(usePpa);

    #ifdef SIMULATOR_BUILD
    uint32_t startUs = EspHal::millis() * 1000;  // Approximate micros
    #else
    uint32_t startUs = micros();
    #endif
    for (uint16_t i = 0; i < iterations; ++i) {
        drawLogoScaled(display, dstX, dstY);
    }
    #ifdef SIMULATOR_BUILD
    uint32_t elapsed = (EspHal::millis() * 1000) - startUs;
    #else
    uint32_t elapsed = micros() - startUs;
    #endif

    setPpaEnabled(prev);

    return elapsed / iterations;
}

static void computeDotLayout(M5GFX& display, int centerX, int centerY) {
    display.setFont(&fonts::FreeSansBold18pt7b);
    display.setTextSize(3);

    #ifdef SIMULATOR_BUILD
    // Estimate text width (rough approximation)
    const int mainTextWidth = strlen(MAIN_LABEL) * 12;  // Approximate character width
    #else
    const int mainTextWidth = display.textWidth(MAIN_LABEL);
    #endif
    #ifdef SIMULATOR_BUILD
    const int mainTextHeight = 12 * 3;  // Approximate (textSize 3)
    #else
    #ifdef SIMULATOR_BUILD
    const int mainTextHeight = 12 * 3;  // Approximate (textSize 3)
    #else
    const int mainTextHeight = display.fontHeight();
    #endif
    #endif

    s_dotLayout.x = centerX + (mainTextWidth / 2) + DOT_GAP_PX;
    s_dotLayout.y = centerY;
    #ifdef SIMULATOR_BUILD
    s_dotLayout.clearW = strlen("...") * 12 + 6;  // Approximate
    #else
    s_dotLayout.clearW = display.textWidth("...") + 6;
    #endif
    s_dotLayout.clearH = mainTextHeight + 6;
}

static void drawStatusBadges(M5GFX& display, bool unitA, bool unitB) {
    // Simple, low-ink indicators: small labelled boxes in the bottom area.
    constexpr int boxW = 90;
    constexpr int boxH = 34;
    constexpr int gap = 16;
    constexpr int y = Theme::SCREEN_H - 60;

    const int totalW = (boxW * 2) + gap;
    const int startX = (Theme::SCREEN_W / 2) - (totalW / 2);

    auto drawBox = [&](int x, const char* label, bool ok) {
        const uint16_t bg = ok ? Theme::STATUS_OK : Theme::STATUS_ERR;
        display.fillRoundRect(x, y, boxW, boxH, 6, bg);
        display.drawRoundRect(x, y, boxW, boxH, 6, Theme::BG_PANEL);
        display.setTextDatum(textdatum_t::middle_center);
        display.setFont(&fonts::Font2);
        display.setTextSize(1);
        display.setTextColor(Theme::BG_DARK);
        display.drawString(label, x + (boxW / 2), y + (boxH / 2));
    };

    drawBox(startX, "ENC-A", unitA);
    drawBox(startX + boxW + gap, "ENC-B", unitB);
}

static void drawFull(M5GFX& display, const char* subtitle, bool unitA, bool unitB) {
    normaliseSubtitle(subtitle, s_lastSubtitle, sizeof(s_lastSubtitle));

    display.fillScreen(TAB5_COLOR_BG_PAGE_RGB565);

    const int centerX = Theme::SCREEN_W / 2;
    const int centerY = Theme::SCREEN_H / 2;

    // Main label font (best available approximation to the reference's Bebas Neue 56px).
    display.setFont(&fonts::FreeSansBold18pt7b);
    display.setTextSize(3);
    display.setTextDatum(textdatum_t::middle_center);
    display.setTextColor(TAB5_COLOR_BRAND_PRIMARY_RGB565);

    #ifdef SIMULATOR_BUILD
    const int mainTextH = 12 * 3;  // Approximate
    #else
    const int mainTextH = display.fontHeight();
    #endif

    // Subtitle font.
    int subtitleH = 0;
    if (s_lastSubtitle[0] != '\0') {
        display.setFont(&fonts::FreeSans12pt7b);
        display.setTextSize(1);
        #ifdef SIMULATOR_BUILD
        subtitleH = 12 * 1;  // Approximate
        #else
        subtitleH = display.fontHeight();
        #endif
    }

    const int logoW = SPECTRASYNQ_LOGO_SMALL_WIDTH * LOGO_SCALE;
    const int logoH = SPECTRASYNQ_LOGO_SMALL_HEIGHT * LOGO_SCALE;
    const int gapLogoToMain = 24;
    const int gapMainToSub = (subtitleH > 0) ? 16 : 0;

    const int stackH = logoH + gapLogoToMain + mainTextH + gapMainToSub + subtitleH;
    const int topY = (Theme::SCREEN_H - stackH) / 2;

    // Logo.
    const int logoX = centerX - (logoW / 2);
    const int logoY = topY;
    drawLogoScaled(display, logoX, logoY);

    // Main label + dots.
    display.setFont(&fonts::FreeSansBold18pt7b);
    display.setTextSize(3);
    display.setTextDatum(textdatum_t::middle_center);
    display.setTextColor(TAB5_COLOR_BRAND_PRIMARY_RGB565);

    const int mainY = logoY + logoH + gapLogoToMain + (mainTextH / 2);
    display.drawString(MAIN_LABEL, centerX, mainY);

    computeDotLayout(display, centerX, mainY);

    // Subtitle (optional).
    int subY = 0;
    if (s_lastSubtitle[0] != '\0') {
        display.setFont(&fonts::FreeSans12pt7b);
        display.setTextSize(1);
        display.setTextDatum(textdatum_t::middle_center);
        display.setTextColor(TAB5_COLOR_FG_SECONDARY_RGB565);
        subY = mainY + (mainTextH / 2) + gapMainToSub + (subtitleH / 2);
        display.drawString(s_lastSubtitle, centerX, subY);
    }

    drawStatusBadges(display, unitA, unitB);
}

static void redrawDots(M5GFX& display) {
    const char* dots = getDots();

    // Clear dot area.
    display.fillRect(
        s_dotLayout.x,
        s_dotLayout.y - (s_dotLayout.clearH / 2),
        s_dotLayout.clearW,
        s_dotLayout.clearH,
        TAB5_COLOR_BG_PAGE_RGB565
    );

    if (dots && dots[0] != '\0') {
        display.setFont(&fonts::FreeSansBold18pt7b);
        display.setTextSize(3);
        display.setTextDatum(textdatum_t::middle_left);
        display.setTextColor(TAB5_COLOR_BRAND_PRIMARY_RGB565);
        display.drawString(dots, s_dotLayout.x, s_dotLayout.y);
    }
}

void show(M5GFX& display, const char* message, bool unitA, bool unitB) {
    // Reset dot animation
    s_lastDotUpdate = EspHal::millis();
    s_dotState = 0;

    s_lastUnitA = unitA;
    s_lastUnitB = unitB;
    s_lastSubtitle[0] = '\0';

    drawFull(display, message, unitA, unitB);
    redrawDots(display);
}

void update(M5GFX& display, const char* message, bool unitA, bool unitB) {
    uint32_t now = EspHal::millis();
    const bool dotsNeedUpdate = ((uint32_t)(now - s_lastDotUpdate) >= DOT_INTERVAL_MS);

    // Subtitle/encoder status changes require a full redraw.
    char nextSubtitle[sizeof(s_lastSubtitle)];
    normaliseSubtitle(message, nextSubtitle, sizeof(nextSubtitle));
    const bool subtitleChanged = !strEq(nextSubtitle, s_lastSubtitle);
    const bool statusChanged = (unitA != s_lastUnitA) || (unitB != s_lastUnitB);
    if (subtitleChanged || statusChanged) {
        s_lastUnitA = unitA;
        s_lastUnitB = unitB;
        drawFull(display, message, unitA, unitB);
        redrawDots(display);
        return;
    }

    if (dotsNeedUpdate) {
        redrawDots(display);
    }
}

void hide(M5GFX& display) {
    // Simply clear the screen - UI will draw over it
    display.fillScreen(Theme::BG_DARK);
}

#endif  // TAB5_ENCODER_USE_LVGL && !SIMULATOR_BUILD

}  // namespace LoadingScreen
