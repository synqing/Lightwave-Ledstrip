// SPDX-License-Identifier: MIT
// BitmapFont.h — 5x7 and 7x10 bitmap font data for M5GFX 128x128 display
// Data-only header; rendering logic lives elsewhere.
#pragma once
#include <cstdint>

namespace bmfont {

// ---------------------------------------------------------------------------
// 5x7 small font — labels, values, small text
// Each glyph is 7 rows of uint8_t. Bits [4..0] hold the 5-pixel pattern
// (bit 4 = leftmost pixel). Bits [7..5] are zero.
// ---------------------------------------------------------------------------
constexpr uint8_t CHAR_W  = 5;
constexpr uint8_t CHAR_H  = 7;
constexpr uint8_t SPACING = 6;  // 5px character + 1px gap

// Digit glyphs (indexed 0-9)
constexpr uint8_t GLYPH_0[CHAR_H] = {0b01110,0b10001,0b10011,0b10101,0b11001,0b10001,0b01110};
constexpr uint8_t GLYPH_1[CHAR_H] = {0b00100,0b01100,0b00100,0b00100,0b00100,0b00100,0b01110};
constexpr uint8_t GLYPH_2[CHAR_H] = {0b01110,0b10001,0b00001,0b00110,0b01000,0b10000,0b11111};
constexpr uint8_t GLYPH_3[CHAR_H] = {0b01110,0b10001,0b00001,0b00110,0b00001,0b10001,0b01110};
constexpr uint8_t GLYPH_4[CHAR_H] = {0b00010,0b00110,0b01010,0b10010,0b11111,0b00010,0b00010};
constexpr uint8_t GLYPH_5[CHAR_H] = {0b11111,0b10000,0b11110,0b00001,0b00001,0b10001,0b01110};
constexpr uint8_t GLYPH_6[CHAR_H] = {0b00110,0b01000,0b10000,0b11110,0b10001,0b10001,0b01110};
constexpr uint8_t GLYPH_7[CHAR_H] = {0b11111,0b00001,0b00010,0b00100,0b01000,0b01000,0b01000};
constexpr uint8_t GLYPH_8[CHAR_H] = {0b01110,0b10001,0b10001,0b01110,0b10001,0b10001,0b01110};
constexpr uint8_t GLYPH_9[CHAR_H] = {0b01110,0b10001,0b10001,0b01111,0b00001,0b00010,0b01100};

// Uppercase letter glyphs (A-Z)
constexpr uint8_t GLYPH_A[CHAR_H] = {0b01110,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001};
constexpr uint8_t GLYPH_B[CHAR_H] = {0b11110,0b10001,0b10001,0b11110,0b10001,0b10001,0b11110};
constexpr uint8_t GLYPH_C[CHAR_H] = {0b01110,0b10001,0b10000,0b10000,0b10000,0b10001,0b01110};
constexpr uint8_t GLYPH_D[CHAR_H] = {0b11110,0b10001,0b10001,0b10001,0b10001,0b10001,0b11110};
constexpr uint8_t GLYPH_E[CHAR_H] = {0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b11111};
constexpr uint8_t GLYPH_F[CHAR_H] = {0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b10000};
constexpr uint8_t GLYPH_G[CHAR_H] = {0b01110,0b10001,0b10000,0b10111,0b10001,0b10001,0b01110};
constexpr uint8_t GLYPH_H[CHAR_H] = {0b10001,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001};
constexpr uint8_t GLYPH_I[CHAR_H] = {0b01110,0b00100,0b00100,0b00100,0b00100,0b00100,0b01110};
constexpr uint8_t GLYPH_J[CHAR_H] = {0b00111,0b00010,0b00010,0b00010,0b00010,0b10010,0b01100};
constexpr uint8_t GLYPH_K[CHAR_H] = {0b10001,0b10010,0b10100,0b11000,0b10100,0b10010,0b10001};
constexpr uint8_t GLYPH_L[CHAR_H] = {0b10000,0b10000,0b10000,0b10000,0b10000,0b10000,0b11111};
constexpr uint8_t GLYPH_M[CHAR_H] = {0b10001,0b11011,0b10101,0b10101,0b10001,0b10001,0b10001};
constexpr uint8_t GLYPH_N[CHAR_H] = {0b10001,0b11001,0b10101,0b10011,0b10001,0b10001,0b10001};
constexpr uint8_t GLYPH_O[CHAR_H] = {0b01110,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110};
constexpr uint8_t GLYPH_P[CHAR_H] = {0b11110,0b10001,0b10001,0b11110,0b10000,0b10000,0b10000};
constexpr uint8_t GLYPH_Q[CHAR_H] = {0b01110,0b10001,0b10001,0b10001,0b10101,0b10010,0b01101};
constexpr uint8_t GLYPH_R[CHAR_H] = {0b11110,0b10001,0b10001,0b11110,0b10100,0b10010,0b10001};
constexpr uint8_t GLYPH_S[CHAR_H] = {0b01110,0b10001,0b10000,0b01110,0b00001,0b10001,0b01110};
constexpr uint8_t GLYPH_T[CHAR_H] = {0b11111,0b00100,0b00100,0b00100,0b00100,0b00100,0b00100};
constexpr uint8_t GLYPH_U[CHAR_H] = {0b10001,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110};
constexpr uint8_t GLYPH_V[CHAR_H] = {0b10001,0b10001,0b10001,0b10001,0b01010,0b01010,0b00100};
constexpr uint8_t GLYPH_W[CHAR_H] = {0b10001,0b10001,0b10001,0b10101,0b10101,0b11011,0b10001};
constexpr uint8_t GLYPH_X[CHAR_H] = {0b10001,0b10001,0b01010,0b00100,0b01010,0b10001,0b10001};
constexpr uint8_t GLYPH_Y[CHAR_H] = {0b10001,0b10001,0b01010,0b00100,0b00100,0b00100,0b00100};
constexpr uint8_t GLYPH_Z[CHAR_H] = {0b11111,0b00001,0b00010,0b00100,0b01000,0b10000,0b11111};

// Symbol glyphs
constexpr uint8_t GLYPH_SPACE  [CHAR_H] = {0b00000,0b00000,0b00000,0b00000,0b00000,0b00000,0b00000};
constexpr uint8_t GLYPH_DOT    [CHAR_H] = {0b00000,0b00000,0b00000,0b00000,0b00000,0b00000,0b00100};
constexpr uint8_t GLYPH_COLON  [CHAR_H] = {0b00000,0b00100,0b00000,0b00000,0b00000,0b00100,0b00000};
constexpr uint8_t GLYPH_DASH   [CHAR_H] = {0b00000,0b00000,0b00000,0b11111,0b00000,0b00000,0b00000};
constexpr uint8_t GLYPH_SLASH  [CHAR_H] = {0b00001,0b00010,0b00010,0b00100,0b01000,0b01000,0b10000};
constexpr uint8_t GLYPH_PERCENT[CHAR_H] = {0b11001,0b11010,0b00010,0b00100,0b01000,0b01011,0b10011};
constexpr uint8_t GLYPH_PLUS   [CHAR_H] = {0b00000,0b00100,0b00100,0b11111,0b00100,0b00100,0b00000};

/// Return a pointer to the 7-byte glyph for the given character,
/// or nullptr if the character is not in the font.
inline const uint8_t* getGlyph(char ch) {
    // Uppercase letters
    if (ch >= 'A' && ch <= 'Z') {
        static const uint8_t* const letters[26] = {
            GLYPH_A, GLYPH_B, GLYPH_C, GLYPH_D, GLYPH_E, GLYPH_F,
            GLYPH_G, GLYPH_H, GLYPH_I, GLYPH_J, GLYPH_K, GLYPH_L,
            GLYPH_M, GLYPH_N, GLYPH_O, GLYPH_P, GLYPH_Q, GLYPH_R,
            GLYPH_S, GLYPH_T, GLYPH_U, GLYPH_V, GLYPH_W, GLYPH_X,
            GLYPH_Y, GLYPH_Z
        };
        return letters[ch - 'A'];
    }
    // Also accept lowercase — map to uppercase glyphs
    if (ch >= 'a' && ch <= 'z') {
        return getGlyph(static_cast<char>(ch - 32));
    }
    // Digits
    if (ch >= '0' && ch <= '9') {
        static const uint8_t* const digits[10] = {
            GLYPH_0, GLYPH_1, GLYPH_2, GLYPH_3, GLYPH_4,
            GLYPH_5, GLYPH_6, GLYPH_7, GLYPH_8, GLYPH_9
        };
        return digits[ch - '0'];
    }
    // Symbols
    switch (ch) {
        case ' ': return GLYPH_SPACE;
        case '.': return GLYPH_DOT;
        case ':': return GLYPH_COLON;
        case '-': return GLYPH_DASH;
        case '/': return GLYPH_SLASH;
        case '%': return GLYPH_PERCENT;
        case '+': return GLYPH_PLUS;
        default:  return nullptr;
    }
}

// ---------------------------------------------------------------------------
// 7x10 large numeral font — overlay value display (digits 0-9 only)
// Each glyph is 10 rows of uint8_t. Bits [6..0] hold the 7-pixel pattern
// (bit 6 = leftmost pixel). Bit 7 is unused/zero.
// ---------------------------------------------------------------------------
constexpr uint8_t LARGE_W       = 7;
constexpr uint8_t LARGE_H       = 10;
constexpr uint8_t LARGE_SPACING = 9;  // 7px character + 2px gap

constexpr uint8_t LARGE_0[LARGE_H] = {0b0111110,0b1100011,0b1100011,0b1100111,0b1101011,0b1110011,0b1100011,0b1100011,0b1100011,0b0111110};
constexpr uint8_t LARGE_1[LARGE_H] = {0b0001100,0b0011100,0b0111100,0b0001100,0b0001100,0b0001100,0b0001100,0b0001100,0b0001100,0b0111111};
constexpr uint8_t LARGE_2[LARGE_H] = {0b0111110,0b1100011,0b0000011,0b0000110,0b0001100,0b0011000,0b0110000,0b1100000,0b1100011,0b1111111};
constexpr uint8_t LARGE_3[LARGE_H] = {0b0111110,0b1100011,0b0000011,0b0000011,0b0011110,0b0000011,0b0000011,0b0000011,0b1100011,0b0111110};
constexpr uint8_t LARGE_4[LARGE_H] = {0b0000110,0b0001110,0b0011110,0b0110110,0b1100110,0b1111111,0b0000110,0b0000110,0b0000110,0b0000110};
constexpr uint8_t LARGE_5[LARGE_H] = {0b1111111,0b1100000,0b1100000,0b1111110,0b1100011,0b0000011,0b0000011,0b0000011,0b1100011,0b0111110};
constexpr uint8_t LARGE_6[LARGE_H] = {0b0011110,0b0110000,0b1100000,0b1100000,0b1111110,0b1100011,0b1100011,0b1100011,0b1100011,0b0111110};
constexpr uint8_t LARGE_7[LARGE_H] = {0b1111111,0b1100011,0b0000011,0b0000110,0b0001100,0b0011000,0b0011000,0b0011000,0b0011000,0b0011000};
constexpr uint8_t LARGE_8[LARGE_H] = {0b0111110,0b1100011,0b1100011,0b1100011,0b0111110,0b1100011,0b1100011,0b1100011,0b1100011,0b0111110};
constexpr uint8_t LARGE_9[LARGE_H] = {0b0111110,0b1100011,0b1100011,0b1100011,0b0111111,0b0000011,0b0000011,0b0000011,0b0000110,0b0111100};

/// Return a pointer to the 10-byte large glyph for a digit character,
/// or nullptr if the character is not '0'-'9'.
inline const uint8_t* getLargeGlyph(char ch) {
    if (ch < '0' || ch > '9') return nullptr;
    static const uint8_t* const digits[10] = {
        LARGE_0, LARGE_1, LARGE_2, LARGE_3, LARGE_4,
        LARGE_5, LARGE_6, LARGE_7, LARGE_8, LARGE_9
    };
    return digits[ch - '0'];
}

}  // namespace bmfont
