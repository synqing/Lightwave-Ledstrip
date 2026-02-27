#pragma once
// ============================================================================
// M5GFX_Mock - M5GFX-compatible interface using SDL2
// ============================================================================
// Provides drop-in replacement for M5GFX in simulator builds
// ============================================================================

#ifdef SIMULATOR_BUILD

#include <cstdint>
#include <cstddef>

// Forward declaration
class M5Canvas;

// Text datum enum (matching M5GFX)
enum class textdatum_t : uint8_t {
    top_left = 0,
    top_center = 1,
    top_right = 2,
    middle_left = 3,
    middle_center = 4,
    middle_right = 5,
    bottom_left = 6,
    bottom_center = 7,
    bottom_right = 8
};

// Fonts namespace (matching M5GFX structure)
namespace fonts {
    // Font pointers - we'll use simple font identifiers
    extern const void* Font2;
    extern const void* Font7;
    extern const void* FreeSans9pt7b;
    extern const void* FreeSans12pt7b;
    extern const void* FreeSansBold9pt7b;
    extern const void* FreeSansBold18pt7b;
}

// M5GFX class mock
class M5GFX {
public:
    M5GFX();
    ~M5GFX();

    // Display initialization
    void begin();
    void setRotation(uint8_t rotation);
    
    // Drawing primitives
    void fillScreen(uint16_t color);
    void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color);
    void drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color);
    void fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t color);
    void drawRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t color);
    void drawFastHLine(int32_t x, int32_t y, int32_t w, uint16_t color);
    void drawFastVLine(int32_t x, int32_t y, int32_t h, uint16_t color);
    void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t* data);
    
    // Text rendering
    void setTextDatum(textdatum_t datum);
    void setFont(const void* font);
    void setTextColor(uint16_t color);
    void setTextColor(uint16_t fg, uint16_t bg);
    void setTextSize(uint8_t size);
    void drawString(const char* str, int32_t x, int32_t y);
    int32_t textWidth(const char* str);
    int32_t fontHeight();
    
    // Write control (for optimization)
    void startWrite();
    void endWrite();
    
    // Display dimensions
    int32_t width() const { return _width; }
    int32_t height() const { return _height; }
    
    // Canvas creation
    M5Canvas* createCanvas(int32_t w, int32_t h);
    
    // SDL2-specific: Get renderer for canvas operations
    void* getSDLRenderer() const { return _renderer; }
    void* getSDLWindow() const { return _window; }
    uint32_t* getPixelBuffer() const { return _pixels; }
    
    // Update display (call in main loop)
    void update();

private:
    void* _window;      // SDL_Window*
    void* _renderer;     // SDL_Renderer*
    void* _texture;      // SDL_Texture* (main framebuffer)
    uint32_t* _pixels;   // Pixel buffer (RGB565 format)
    
    int32_t _width;
    int32_t _height;
    uint8_t _rotation;
    
    // Text state
    textdatum_t _textDatum;
    const void* _font;
    uint16_t _textColor;
    uint16_t _textBgColor;
    uint8_t _textSize;
    
    bool _inWrite;
    
    void initSDL();
    void cleanupSDL();
    void convertRGB565ToRGBA(uint32_t* dest, uint16_t color, size_t count);
    void present();
};

// M5Canvas class mock
class M5Canvas {
public:
    M5Canvas(M5GFX* display);
    ~M5Canvas();

    // Canvas creation
    bool createSprite(int32_t w, int32_t h);
    void deleteSprite();
    
    // Canvas properties
    void setColorDepth(uint8_t depth);
    void setPsram(bool usePsram);
    
    // Drawing primitives (same as M5GFX)
    void fillSprite(uint16_t color);
    void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color);
    void drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color);
    void drawFastHLine(int32_t x, int32_t y, int32_t w, uint16_t color);
    void drawFastVLine(int32_t x, int32_t y, int32_t h, uint16_t color);
    void drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t color);
    void drawCircle(int32_t x, int32_t y, int32_t r, uint16_t color);
    void fillCircle(int32_t x, int32_t y, int32_t r, uint16_t color);
    
    // Text rendering (same as M5GFX)
    void setTextDatum(textdatum_t datum);
    void setFont(const void* font);
    void setTextColor(uint16_t color);
    void setTextColor(uint16_t fg, uint16_t bg);
    void setTextSize(uint8_t size);
    void drawString(const char* str, int32_t x, int32_t y);
    
    // Write control
    void startWrite();
    void endWrite();
    
    // Push sprite to display
    void pushSprite(int32_t x, int32_t y);
    void pushSprite(M5GFX* display, int32_t x, int32_t y);
    
    // Canvas dimensions
    int32_t width() const { return _width; }
    int32_t height() const { return _height; }
    
    bool isValid() const { return _valid; }

private:
    M5GFX* _display;
    void* _texture;      // SDL_Texture* for sprite
    uint32_t* _pixels;   // Pixel buffer
    int32_t _width;
    int32_t _height;
    bool _valid;
    
    // Text state
    textdatum_t _textDatum;
    const void* _font;
    uint16_t _textColor;
    uint16_t _textBgColor;
    uint8_t _textSize;
    
    bool _inWrite;
    
    void updateTexture();
};

#endif  // SIMULATOR_BUILD

