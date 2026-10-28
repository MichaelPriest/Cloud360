#include "icons.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "xsdk/text.h"

/* Six bits per row. Original minimalist pixel icons, no external artwork. */
static const uint8_t bitmaps[CLOUD360_ICON_COUNT][6] = {
    { 0x0C, 0x1E, 0x3F, 0x3F, 0x1E, 0x00 }, /* Cloud */
    { 0x00, 0x3F, 0x3F, 0x36, 0x29, 0x00 }, /* Controller */
    { 0x1C, 0x22, 0x22, 0x1C, 0x06, 0x03 }, /* Search */
    { 0x36, 0x2D, 0x2D, 0x2D, 0x36, 0x00 }, /* Library */
    { 0x12, 0x3F, 0x2D, 0x2D, 0x3F, 0x12 }, /* Gear */
    { 0x0C, 0x3F, 0x1E, 0x3F, 0x21, 0x00 }, /* Star */
    { 0x0C, 0x1E, 0x3F, 0x33, 0x33, 0x3F }, /* Home */
    { 0x1E, 0x21, 0x25, 0x21, 0x25, 0x1E }, /* Info */
    { 0x3F, 0x21, 0x2D, 0x21, 0x3F, 0x0C }, /* Monitor */
    { 0x04, 0x15, 0x0E, 0x3F, 0x0E, 0x15 }  /* Spark */
};

bool cloud360_icon_encode(Cloud360Icon icon, char *output, size_t capacity)
{
    /* Each icon is 6 columns x 3 character rows, at most 56 UTF-8 bytes. */
    if (!output || capacity < 57 || (unsigned)icon >= CLOUD360_ICON_COUNT)
        return false;

    char *out = output;
    for (uint32_t row = 0; row < 3; ++row) {
        for (uint32_t col = 0; col < 6; ++col) {
            const uint32_t bit = 1u << (5 - col);
            const bool top = (bitmaps[icon][row * 2] & bit) != 0;
            const bool bottom = (bitmaps[icon][row * 2 + 1] & bit) != 0;

            if (!top && !bottom) {
                *out++ = ' ';
            } else {
                /* U+2580 top, U+2584 bottom, U+2588 full block. */
                *out++ = (char)0xE2;
                *out++ = (char)0x96;
                *out++ = (char)(top && bottom ? 0x88 : top ? 0x80 : 0x84);
            }
        }
        if (row != 2)
            *out++ = '\n';
    }
    *out = '\0';
    return true;
}

void cloud360_icon_draw(int32_t x, int32_t y, Cloud360Icon icon, uint32_t argb)
{
    char glyphs[64];
    if (cloud360_icon_encode(icon, glyphs, sizeof glyphs))
        xsdk_text_draw(x, y, 1, argb, glyphs);
}
