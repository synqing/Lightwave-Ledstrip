// ============================================================================
// M5GFX_Mock - SDL2 Implementation
// ============================================================================

#ifdef SIMULATOR_BUILD

#include "M5GFX_Mock.h"
#include <SDL2/SDL.h>
#include <cstring>
#include <cstdio>
#include <cmath>

// Font identifiers (simple placeholders)
namespace fonts {
    const void* Font2 = reinterpret_cast<const void*>(0x1001);
    const void* Font7 = reinterpret_cast<const void*>(0x1002);
    const void* FreeSans9pt7b = reinterpret_cast<const void*>(0x2001);
    const void* FreeSans12pt7b = reinterpret_cast<const void*>(0x2002);
    const void* FreeSansBold9pt7b = reinterpret_cast<const void*>(0x2003);
    const void* FreeSansBold18pt7b = reinterpret_cast<const void*>(0x2004);
}

// Simple 8x8 bitmap font (ASCII 32-126)
// Each character is 8 bytes (8 rows, 1 byte per row)
static const uint8_t bitmap_font_8x8[][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // space (32)
    {0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x00}, // !
    {0x6C, 0x6C, 0x6C, 0x00, 0x00, 0x00, 0x00, 0x00}, // "
    {0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00}, // #
    {0x0C, 0x3F, 0x68, 0x3E, 0x0B, 0x7E, 0x18, 0x00}, // $
    {0x60, 0x66, 0x0C, 0x18, 0x30, 0x66, 0x06, 0x00}, // %
    {0x38, 0x6C, 0x38, 0x6E, 0xDC, 0xCC, 0x76, 0x00}, // &
    {0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00}, // '
    {0x0C, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0C, 0x00}, // (
    {0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x18, 0x30, 0x00}, // )
    {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00}, // *
    {0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00}, // +
    {0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30, 0x00}, // ,
    {0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00}, // -
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00}, // .
    {0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0x00}, // /
    {0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00}, // 0 (48)
    {0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00}, // 1
    {0x3E, 0x63, 0x07, 0x1E, 0x3C, 0x70, 0x7F, 0x00}, // 2
    {0x3E, 0x63, 0x03, 0x1E, 0x03, 0x63, 0x3E, 0x00}, // 3
    {0x06, 0x0E, 0x1E, 0x36, 0x66, 0x7F, 0x06, 0x00}, // 4
    {0x7F, 0x60, 0x7E, 0x03, 0x03, 0x63, 0x3E, 0x00}, // 5
    {0x1E, 0x30, 0x60, 0x7E, 0x63, 0x63, 0x3E, 0x00}, // 6
    {0x7F, 0x63, 0x06, 0x0C, 0x18, 0x18, 0x18, 0x00}, // 7
    {0x3E, 0x63, 0x63, 0x3E, 0x63, 0x63, 0x3E, 0x00}, // 8
    {0x3E, 0x63, 0x63, 0x3F, 0x03, 0x06, 0x3C, 0x00}, // 9
    {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00}, // :
    {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x30, 0x00}, // ;
    {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00}, // <
    {0x00, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00, 0x00}, // =
    {0x60, 0x30, 0x18, 0x0C, 0x18, 0x30, 0x60, 0x00}, // >
    {0x3E, 0x63, 0x03, 0x0E, 0x18, 0x00, 0x18, 0x00}, // ?
    {0x3E, 0x63, 0x6F, 0x6F, 0x6E, 0x60, 0x3E, 0x00}, // @
    {0x1C, 0x36, 0x63, 0x63, 0x7F, 0x63, 0x63, 0x00}, // A (65)
    {0x7E, 0x63, 0x63, 0x7E, 0x63, 0x63, 0x7E, 0x00}, // B
    {0x1E, 0x33, 0x60, 0x60, 0x60, 0x33, 0x1E, 0x00}, // C
    {0x7C, 0x66, 0x63, 0x63, 0x63, 0x66, 0x7C, 0x00}, // D
    {0x7F, 0x60, 0x60, 0x7E, 0x60, 0x60, 0x7F, 0x00}, // E
    {0x7F, 0x60, 0x60, 0x7E, 0x60, 0x60, 0x60, 0x00}, // F
    {0x1E, 0x33, 0x60, 0x67, 0x63, 0x33, 0x1E, 0x00}, // G
    {0x63, 0x63, 0x63, 0x7F, 0x63, 0x63, 0x63, 0x00}, // H
    {0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00}, // I
    {0x0F, 0x06, 0x06, 0x06, 0x66, 0x66, 0x3C, 0x00}, // J
    {0x63, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0x63, 0x00}, // K
    {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7F, 0x00}, // L
    {0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00}, // M
    {0x63, 0x73, 0x7B, 0x6F, 0x67, 0x63, 0x63, 0x00}, // N
    {0x3E, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3E, 0x00}, // O
    {0x7E, 0x63, 0x63, 0x7E, 0x60, 0x60, 0x60, 0x00}, // P
    {0x3E, 0x63, 0x63, 0x63, 0x6B, 0x66, 0x3D, 0x00}, // Q
    {0x7E, 0x63, 0x63, 0x7E, 0x6C, 0x66, 0x63, 0x00}, // R
    {0x3E, 0x63, 0x60, 0x3E, 0x03, 0x63, 0x3E, 0x00}, // S
    {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00}, // T
    {0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3E, 0x00}, // U
    {0x63, 0x63, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00}, // V
    {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00}, // W
    {0x63, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x63, 0x00}, // X
    {0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x00}, // Y
    {0x7F, 0x07, 0x0E, 0x1C, 0x38, 0x70, 0x7F, 0x00}, // Z
};

// Draw a single character using bitmap font
static void drawBitmapChar(uint32_t* pixels, int32_t width, int32_t height, 
                           char c, int32_t x, int32_t y, uint32_t color, int scale) {
    if (c < 32 || c > 90) return;  // Only handle ASCII 32-90 (space to Z)
    
    const uint8_t* fontData = bitmap_font_8x8[c - 32];
    
    for (int row = 0; row < 8; row++) {
        uint8_t rowData = fontData[row];
        for (int col = 0; col < 8; col++) {
            if (rowData & (0x80 >> col)) {
                // Draw pixel (scaled)
                for (int sy = 0; sy < scale; sy++) {
                    for (int sx = 0; sx < scale; sx++) {
                        int32_t px = x + (col * scale) + sx;
                        int32_t py = y + (row * scale) + sy;
                        if (px >= 0 && px < width && py >= 0 && py < height) {
                            pixels[py * width + px] = color;
                        }
                    }
                }
            }
        }
    }
}

// Helper: Convert RGB565 to RGBA8888
static void rgb565ToRGBA(uint16_t rgb565, uint8_t& r, uint8_t& g, uint8_t& b) {
    r = ((rgb565 >> 11) & 0x1F) << 3;
    g = ((rgb565 >> 5) & 0x3F) << 2;
    b = (rgb565 & 0x1F) << 3;
    // Expand to full range
    r |= r >> 5;
    g |= g >> 6;
    b |= b >> 5;
}

// Helper: Convert RGB565 to SDL color (RGBA8888 format)
// RGBA8888 format: 0xRRGGBBAA (big-endian)
static uint32_t rgb565ToSDLColor(uint16_t rgb565) {
    uint8_t r, g, b;
    rgb565ToRGBA(rgb565, r, g, b);
    // Manually construct RGBA8888: R in high byte, A in low byte
    return (static_cast<uint32_t>(r) << 24) | 
           (static_cast<uint32_t>(g) << 16) | 
           (static_cast<uint32_t>(b) << 8) | 
           0xFF;  // Alpha = 255 (fully opaque)
}

// M5GFX Implementation
M5GFX::M5GFX()
    : _window(nullptr)
    , _renderer(nullptr)
    , _texture(nullptr)
    , _pixels(nullptr)
    , _width(1280)
    , _height(720)
    , _rotation(0)
    , _textDatum(textdatum_t::top_left)
    , _font(nullptr)
    , _textColor(0xFFFF)
    , _textBgColor(0x0000)
    , _textSize(1)
    , _inWrite(false)
{
}

M5GFX::~M5GFX() {
    cleanupSDL();
}

void M5GFX::begin() {
    initSDL();
}

void M5GFX::initSDL() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return;
    }
    
    _window = SDL_CreateWindow(
        "Tab5.encoder UI Simulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        _width,
        _height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    
    if (!_window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return;
    }
    
    // Ensure window is shown and raised on macOS
    SDL_ShowWindow(static_cast<SDL_Window*>(_window));
    SDL_RaiseWindow(static_cast<SDL_Window*>(_window));
    
    _renderer = SDL_CreateRenderer(
        static_cast<SDL_Window*>(_window),
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    
    if (!_renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return;
    }
    
    // Set renderer draw color to black (for clear operations)
    SDL_SetRenderDrawColor(static_cast<SDL_Renderer*>(_renderer), 0, 0, 0, 255);
    
    // Create texture for framebuffer
    _texture = SDL_CreateTexture(
        static_cast<SDL_Renderer*>(_renderer),
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        _width,
        _height
    );
    
    if (!_texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        return;
    }
    
    // Allocate pixel buffer
    _pixels = new uint32_t[_width * _height];
    memset(_pixels, 0, _width * _height * sizeof(uint32_t));
}

void M5GFX::cleanupSDL() {
    if (_texture) {
        SDL_DestroyTexture(static_cast<SDL_Texture*>(_texture));
        _texture = nullptr;
    }
    if (_renderer) {
        SDL_DestroyRenderer(static_cast<SDL_Renderer*>(_renderer));
        _renderer = nullptr;
    }
    if (_window) {
        SDL_DestroyWindow(static_cast<SDL_Window*>(_window));
        _window = nullptr;
    }
    if (_pixels) {
        delete[] _pixels;
        _pixels = nullptr;
    }
    SDL_Quit();
}

void M5GFX::setRotation(uint8_t rotation) {
    _rotation = rotation;
    // Rotation not fully implemented - would need coordinate transform
}

void M5GFX::fillScreen(uint16_t color) {
    fillRect(0, 0, _width, _height, color);
}

void M5GFX::fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color) {
    if (!_pixels) return;
    
    uint32_t sdlColor = rgb565ToSDLColor(color);
    
    // Clamp to screen bounds
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > _width) w = _width - x;
    if (y + h > _height) h = _height - y;
    if (w <= 0 || h <= 0) return;
    
    for (int32_t py = y; py < y + h; py++) {
        for (int32_t px = x; px < x + w; px++) {
            _pixels[py * _width + px] = sdlColor;
        }
    }
}

void M5GFX::fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t color) {
    // Simplified: just fill rect (rounded corners not implemented)
    fillRect(x, y, w, h, color);
}

void M5GFX::drawRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t color) {
    // Simplified: just draw rect (rounded corners not implemented)
    drawRect(x, y, w, h, color);
}

void M5GFX::drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color) {
    if (!_pixels) return;
    
    uint32_t sdlColor = rgb565ToSDLColor(color);
    
    // Clamp to screen bounds
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > _width) w = _width - x;
    if (y + h > _height) h = _height - y;
    if (w <= 0 || h <= 0) return;
    
    // Top edge
    for (int32_t px = x; px < x + w; px++) {
        if (px >= 0 && px < _width && y >= 0 && y < _height) {
            _pixels[y * _width + px] = sdlColor;
        }
    }
    // Bottom edge
    if (y + h - 1 >= 0 && y + h - 1 < _height) {
        for (int32_t px = x; px < x + w; px++) {
            if (px >= 0 && px < _width) {
                _pixels[(y + h - 1) * _width + px] = sdlColor;
            }
        }
    }
    // Left edge
    for (int32_t py = y; py < y + h; py++) {
        if (py >= 0 && py < _height && x >= 0 && x < _width) {
            _pixels[py * _width + x] = sdlColor;
        }
    }
    // Right edge
    if (x + w - 1 >= 0 && x + w - 1 < _width) {
        for (int32_t py = y; py < y + h; py++) {
            if (py >= 0 && py < _height) {
                _pixels[py * _width + (x + w - 1)] = sdlColor;
            }
        }
    }
}

void M5GFX::drawFastHLine(int32_t x, int32_t y, int32_t w, uint16_t color) {
    if (!_pixels || y < 0 || y >= _height) return;
    
    uint32_t sdlColor = rgb565ToSDLColor(color);
    
    if (x < 0) { w += x; x = 0; }
    if (x + w > _width) w = _width - x;
    if (w <= 0) return;
    
    for (int32_t px = x; px < x + w; px++) {
        _pixels[y * _width + px] = sdlColor;
    }
}

void M5GFX::drawFastVLine(int32_t x, int32_t y, int32_t h, uint16_t color) {
    if (!_pixels || x < 0 || x >= _width) return;
    
    uint32_t sdlColor = rgb565ToSDLColor(color);
    
    if (y < 0) { h += y; y = 0; }
    if (y + h > _height) h = _height - y;
    if (h <= 0) return;
    
    for (int32_t py = y; py < y + h; py++) {
        _pixels[py * _width + x] = sdlColor;
    }
}

void M5GFX::pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t* data) {
    if (!_pixels || !data) return;
    
    // Clamp to screen bounds
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > _width) w = _width - x;
    if (y + h > _height) h = _height - y;
    if (w <= 0 || h <= 0) return;
    
    // Copy RGB565 data to pixel buffer
    for (int32_t py = 0; py < h; py++) {
        for (int32_t px = 0; px < w; px++) {
            uint16_t rgb565 = data[py * w + px];
            uint32_t sdlColor = rgb565ToSDLColor(rgb565);
            _pixels[(y + py) * _width + (x + px)] = sdlColor;
        }
    }
}

void M5GFX::setTextDatum(textdatum_t datum) {
    _textDatum = datum;
}

void M5GFX::setFont(const void* font) {
    _font = font;
}

void M5GFX::setTextColor(uint16_t color) {
    _textColor = color;
    _textBgColor = 0x0000;  // Default to transparent
}

void M5GFX::setTextColor(uint16_t fg, uint16_t bg) {
    _textColor = fg;
    _textBgColor = bg;
}

void M5GFX::setTextSize(uint8_t size) {
    _textSize = size;
}

int32_t M5GFX::textWidth(const char* str) {
    if (!str) return 0;
    // Bitmap font: 8 pixels per character * text size
    return strlen(str) * 8 * _textSize;
}

int32_t M5GFX::fontHeight() {
    // Bitmap font: 8 pixels * text size
    return 8 * _textSize;
}

void M5GFX::drawString(const char* str, int32_t x, int32_t y) {
    if (!_pixels || !str) return;
    
    // Use bitmap font rendering
    int charWidth = 8 * _textSize;
    int charHeight = 8 * _textSize;
    int textWidth = strlen(str) * charWidth;
    int textHeight = charHeight;
    
    // Adjust position based on text datum
    int32_t drawX = x;
    int32_t drawY = y;
    
    switch (_textDatum) {
        case textdatum_t::top_left:
            break;
        case textdatum_t::top_center:
            drawX -= textWidth / 2;
            break;
        case textdatum_t::top_right:
            drawX -= textWidth;
            break;
        case textdatum_t::middle_left:
            drawY -= textHeight / 2;
            break;
        case textdatum_t::middle_center:
            drawX -= textWidth / 2;
            drawY -= textHeight / 2;
            break;
        case textdatum_t::middle_right:
            drawX -= textWidth;
            drawY -= textHeight / 2;
            break;
        case textdatum_t::bottom_left:
            drawY -= textHeight;
            break;
        case textdatum_t::bottom_center:
            drawX -= textWidth / 2;
            drawY -= textHeight;
            break;
        case textdatum_t::bottom_right:
            drawX -= textWidth;
            drawY -= textHeight;
            break;
    }
    
    // Draw each character
    uint32_t fgColor = rgb565ToSDLColor(_textColor);
    const char* p = str;
    int32_t charX = drawX;
    
    while (*p) {
        char c = *p;
        // Convert to uppercase for bitmap font (only has A-Z)
        if (c >= 'a' && c <= 'z') {
            c = c - 'a' + 'A';
        }
        drawBitmapChar(_pixels, _width, _height, c, charX, drawY, fgColor, _textSize);
        charX += charWidth;
        p++;
    }
}

void M5GFX::startWrite() {
    _inWrite = true;
}

void M5GFX::endWrite() {
    _inWrite = false;
    present();
}

void M5GFX::update() {
    present();
}

void M5GFX::present() {
    if (!_texture || !_renderer || !_pixels) return;
    
    // Update texture with pixel data
    void* pixels;
    int pitch;
    SDL_LockTexture(static_cast<SDL_Texture*>(_texture), nullptr, &pixels, &pitch);
    memcpy(pixels, _pixels, _width * _height * sizeof(uint32_t));
    SDL_UnlockTexture(static_cast<SDL_Texture*>(_texture));
    
    // Render texture to screen
    // Set clear color to black before clearing (prevents red background)
    SDL_SetRenderDrawColor(static_cast<SDL_Renderer*>(_renderer), 0, 0, 0, 255);
    SDL_RenderClear(static_cast<SDL_Renderer*>(_renderer));
    SDL_RenderCopy(static_cast<SDL_Renderer*>(_renderer), static_cast<SDL_Texture*>(_texture), nullptr, nullptr);
    SDL_RenderPresent(static_cast<SDL_Renderer*>(_renderer));
}

M5Canvas* M5GFX::createCanvas(int32_t w, int32_t h) {
    // Not implemented - M5Canvas is created directly
    return nullptr;
}

// M5Canvas Implementation
M5Canvas::M5Canvas(M5GFX* display)
    : _display(display)
    , _texture(nullptr)
    , _pixels(nullptr)
    , _width(0)
    , _height(0)
    , _valid(false)
    , _textDatum(textdatum_t::top_left)
    , _font(nullptr)
    , _textColor(0xFFFF)
    , _textBgColor(0x0000)
    , _textSize(1)
    , _inWrite(false)
{
}

M5Canvas::~M5Canvas() {
    deleteSprite();
}

bool M5Canvas::createSprite(int32_t w, int32_t h) {
    if (_valid) {
        deleteSprite();
    }
    
    _width = w;
    _height = h;
    
    if (!_display || !_display->getSDLRenderer()) {
        return false;
    }
    
    SDL_Renderer* renderer = static_cast<SDL_Renderer*>(_display->getSDLRenderer());
    
    _texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        w,
        h
    );
    
    if (!_texture) {
        return false;
    }
    
    _pixels = new uint32_t[w * h];
    memset(_pixels, 0, w * h * sizeof(uint32_t));
    
    _valid = true;
    return true;
}

void M5Canvas::deleteSprite() {
    if (_texture) {
        SDL_DestroyTexture(static_cast<SDL_Texture*>(_texture));
        _texture = nullptr;
    }
    if (_pixels) {
        delete[] _pixels;
        _pixels = nullptr;
    }
    _valid = false;
    _width = 0;
    _height = 0;
}

void M5Canvas::setColorDepth(uint8_t depth) {
    // Ignored in simulator (always 16-bit RGB565)
    (void)depth;
}

void M5Canvas::setPsram(bool usePsram) {
    // Ignored in simulator
    (void)usePsram;
}

void M5Canvas::fillSprite(uint16_t color) {
    fillRect(0, 0, _width, _height, color);
}

void M5Canvas::fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color) {
    if (!_pixels || !_valid) return;
    
    uint32_t sdlColor = rgb565ToSDLColor(color);
    
    // Clamp to canvas bounds
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > _width) w = _width - x;
    if (y + h > _height) h = _height - y;
    if (w <= 0 || h <= 0) return;
    
    for (int32_t py = y; py < y + h; py++) {
        for (int32_t px = x; px < x + w; px++) {
            _pixels[py * _width + px] = sdlColor;
        }
    }
}

void M5Canvas::drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color) {
    if (!_pixels || !_valid) return;
    
    uint32_t sdlColor = rgb565ToSDLColor(color);
    
    // Clamp to canvas bounds
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > _width) w = _width - x;
    if (y + h > _height) h = _height - y;
    if (w <= 0 || h <= 0) return;
    
    // Top edge
    for (int32_t px = x; px < x + w; px++) {
        if (px >= 0 && px < _width && y >= 0 && y < _height) {
            _pixels[y * _width + px] = sdlColor;
        }
    }
    // Bottom edge
    if (y + h - 1 >= 0 && y + h - 1 < _height) {
        for (int32_t px = x; px < x + w; px++) {
            if (px >= 0 && px < _width) {
                _pixels[(y + h - 1) * _width + px] = sdlColor;
            }
        }
    }
    // Left edge
    for (int32_t py = y; py < y + h; py++) {
        if (py >= 0 && py < _height && x >= 0 && x < _width) {
            _pixels[py * _width + x] = sdlColor;
        }
    }
    // Right edge
    if (x + w - 1 >= 0 && x + w - 1 < _width) {
        for (int32_t py = y; py < y + h; py++) {
            if (py >= 0 && py < _height) {
                _pixels[py * _width + (x + w - 1)] = sdlColor;
            }
        }
    }
}

void M5Canvas::drawFastHLine(int32_t x, int32_t y, int32_t w, uint16_t color) {
    if (!_pixels || !_valid || y < 0 || y >= _height) return;
    
    uint32_t sdlColor = rgb565ToSDLColor(color);
    
    if (x < 0) { w += x; x = 0; }
    if (x + w > _width) w = _width - x;
    if (w <= 0) return;
    
    for (int32_t px = x; px < x + w; px++) {
        _pixels[y * _width + px] = sdlColor;
    }
}

void M5Canvas::drawFastVLine(int32_t x, int32_t y, int32_t h, uint16_t color) {
    if (!_pixels || !_valid || x < 0 || x >= _width) return;
    
    uint32_t sdlColor = rgb565ToSDLColor(color);
    
    if (y < 0) { h += y; y = 0; }
    if (y + h > _height) h = _height - y;
    if (h <= 0) return;
    
    for (int32_t py = y; py < y + h; py++) {
        _pixels[py * _width + x] = sdlColor;
    }
}

void M5Canvas::drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t color) {
    if (!_pixels || !_valid) return;
    
    uint32_t sdlColor = rgb565ToSDLColor(color);
    
    // Simple line drawing (Bresenham's algorithm)
    int32_t dx = abs(x1 - x0);
    int32_t dy = abs(y1 - y0);
    int32_t sx = (x0 < x1) ? 1 : -1;
    int32_t sy = (y0 < y1) ? 1 : -1;
    int32_t err = dx - dy;
    
    int32_t x = x0;
    int32_t y = y0;
    
    while (true) {
        if (x >= 0 && x < _width && y >= 0 && y < _height) {
            _pixels[y * _width + x] = sdlColor;
        }
        
        if (x == x1 && y == y1) break;
        
        int32_t e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
}

void M5Canvas::drawCircle(int32_t x, int32_t y, int32_t r, uint16_t color) {
    if (!_pixels || !_valid || r <= 0) return;
    
    uint32_t sdlColor = rgb565ToSDLColor(color);
    
    // Simple circle drawing (midpoint algorithm)
    int32_t px = 0;
    int32_t py = r;
    int32_t d = 1 - r;
    
    auto plotCircle = [&](int32_t cx, int32_t cy) {
        if (cx >= 0 && cx < _width && cy >= 0 && cy < _height) {
            _pixels[cy * _width + cx] = sdlColor;
        }
    };
    
    plotCircle(x, y + r);
    plotCircle(x, y - r);
    plotCircle(x + r, y);
    plotCircle(x - r, y);
    
    while (px < py) {
        if (d < 0) {
            d += 2 * px + 3;
        } else {
            d += 2 * (px - py) + 5;
            py--;
        }
        px++;
        
        plotCircle(x + px, y + py);
        plotCircle(x - px, y + py);
        plotCircle(x + px, y - py);
        plotCircle(x - px, y - py);
        plotCircle(x + py, y + px);
        plotCircle(x - py, y + px);
        plotCircle(x + py, y - px);
        plotCircle(x - py, y - px);
    }
}

void M5Canvas::fillCircle(int32_t x, int32_t y, int32_t r, uint16_t color) {
    if (!_pixels || !_valid || r <= 0) return;
    
    uint32_t sdlColor = rgb565ToSDLColor(color);
    
    // Fill circle by drawing horizontal lines
    for (int32_t py = -r; py <= r; py++) {
        int32_t dx = (int32_t)sqrt(r * r - py * py);
        int32_t x0 = x - dx;
        int32_t x1 = x + dx;
        
        if (x0 < 0) x0 = 0;
        if (x1 >= _width) x1 = _width - 1;
        if (y + py >= 0 && y + py < _height && x0 < _width && x1 >= 0) {
            for (int32_t px = x0; px <= x1; px++) {
                _pixels[(y + py) * _width + px] = sdlColor;
            }
        }
    }
}

void M5Canvas::setTextDatum(textdatum_t datum) {
    _textDatum = datum;
}

void M5Canvas::setFont(const void* font) {
    _font = font;
}

void M5Canvas::setTextColor(uint16_t color) {
    _textColor = color;
    _textBgColor = 0x0000;
}

void M5Canvas::setTextColor(uint16_t fg, uint16_t bg) {
    _textColor = fg;
    _textBgColor = bg;
}

void M5Canvas::setTextSize(uint8_t size) {
    _textSize = size;
}

void M5Canvas::drawString(const char* str, int32_t x, int32_t y) {
    if (!_pixels || !_valid || !str) return;
    
    // Use bitmap font rendering (same as M5GFX)
    int charWidth = 8 * _textSize;
    int charHeight = 8 * _textSize;
    int textWidth = strlen(str) * charWidth;
    int textHeight = charHeight;
    
    // Adjust position based on text datum
    int32_t drawX = x;
    int32_t drawY = y;
    
    switch (_textDatum) {
        case textdatum_t::top_left:
            break;
        case textdatum_t::top_center:
            drawX -= textWidth / 2;
            break;
        case textdatum_t::top_right:
            drawX -= textWidth;
            break;
        case textdatum_t::middle_left:
            drawY -= textHeight / 2;
            break;
        case textdatum_t::middle_center:
            drawX -= textWidth / 2;
            drawY -= textHeight / 2;
            break;
        case textdatum_t::middle_right:
            drawX -= textWidth;
            drawY -= textHeight / 2;
            break;
        case textdatum_t::bottom_left:
            drawY -= textHeight;
            break;
        case textdatum_t::bottom_center:
            drawX -= textWidth / 2;
            drawY -= textHeight;
            break;
        case textdatum_t::bottom_right:
            drawX -= textWidth;
            drawY -= textHeight;
            break;
    }
    
    // Draw each character
    uint32_t fgColor = rgb565ToSDLColor(_textColor);
    const char* p = str;
    int32_t charX = drawX;
    
    while (*p) {
        char c = *p;
        // Convert to uppercase for bitmap font (only has A-Z)
        if (c >= 'a' && c <= 'z') {
            c = c - 'a' + 'A';
        }
        drawBitmapChar(_pixels, _width, _height, c, charX, drawY, fgColor, _textSize);
        charX += charWidth;
        p++;
    }
}

void M5Canvas::startWrite() {
    _inWrite = true;
}

void M5Canvas::endWrite() {
    _inWrite = false;
    updateTexture();
}

void M5Canvas::pushSprite(int32_t x, int32_t y) {
    if (!_display || !_valid) return;
    pushSprite(_display, x, y);
}

void M5Canvas::pushSprite(M5GFX* display, int32_t x, int32_t y) {
    if (!display || !_valid || !_pixels) {
        printf("[ERROR] pushSprite: invalid state display=%p valid=%d pixels=%p\n", display, _valid, _pixels);
        return;
    }
    
    // Update texture first
    updateTexture();
    
    // Copy sprite pixels directly to display's pixel buffer
    // This ensures the pixel buffer is updated for present()
    uint32_t* displayPixels = display->getPixelBuffer();
    if (!displayPixels) {
        printf("[ERROR] pushSprite: display pixel buffer is NULL\n");
        return;
    }
    
    int32_t displayW = display->width();
    int32_t displayH = display->height();
    
    // Clamp to display bounds
    int32_t srcX = 0;
    int32_t srcY = 0;
    int32_t copyW = _width;
    int32_t copyH = _height;
    int32_t dstX = x;
    int32_t dstY = y;
    
    if (dstX < 0) { srcX = -dstX; copyW += dstX; dstX = 0; }
    if (dstY < 0) { srcY = -dstY; copyH += dstY; dstY = 0; }
    if (dstX + copyW > displayW) {
        copyW = displayW - dstX;
        if (copyW < 0) copyW = 0;
    }
    if (dstY + copyH > displayH) {
        copyH = displayH - dstY;
        if (copyH < 0) copyH = 0;
    }
    // Debug: log first few pushes
    static int pushCount = 0;
    if (pushCount < 20) {
        printf("[DEBUG] pushSprite #%d: x=%d y=%d w=%d h=%d -> dstX=%d dstY=%d copyW=%d copyH=%d\n", 
               pushCount, x, y, _width, _height, dstX, dstY, copyW, copyH);
        pushCount++;
    }
    
    if (copyW <= 0 || copyH <= 0) {
        if (pushCount < 30) {
            printf("[WARN] pushSprite: clipped out x=%d y=%d w=%d h=%d displayW=%d displayH=%d -> copyW=%d copyH=%d\n", 
                   x, y, _width, _height, displayW, displayH, copyW, copyH);
        }
        return;
    }
    
    // Copy pixels from sprite to display buffer
    int pixelsCopied = 0;
    for (int32_t py = 0; py < copyH; py++) {
        for (int32_t px = 0; px < copyW; px++) {
            uint32_t spritePixel = _pixels[(srcY + py) * _width + (srcX + px)];
            int32_t finalY = dstY + py;
            int32_t finalX = dstX + px;
            if (finalY >= 0 && finalY < displayH && finalX >= 0 && finalX < displayW) {
                displayPixels[finalY * displayW + finalX] = spritePixel;
                pixelsCopied++;
            }
        }
    }
    
    if (pushCount <= 20) {
        printf("[DEBUG] pushSprite: copied %d pixels\n", pixelsCopied);
    }
}

void M5Canvas::updateTexture() {
    if (!_texture || !_pixels) return;
    
    void* pixels;
    int pitch;
    SDL_LockTexture(static_cast<SDL_Texture*>(_texture), nullptr, &pixels, &pitch);
    memcpy(pixels, _pixels, _width * _height * sizeof(uint32_t));
    SDL_UnlockTexture(static_cast<SDL_Texture*>(_texture));
}

#endif  // SIMULATOR_BUILD

