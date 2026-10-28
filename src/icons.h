#ifndef CLOUD360_ICONS_H
#define CLOUD360_ICONS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    CLOUD360_ICON_CLOUD = 0,
    CLOUD360_ICON_GAMEPAD,
    CLOUD360_ICON_SEARCH,
    CLOUD360_ICON_LIBRARY,
    CLOUD360_ICON_SETTINGS,
    CLOUD360_ICON_STAR,
    CLOUD360_ICON_HOME,
    CLOUD360_ICON_INFO,
    CLOUD360_ICON_PC,
    CLOUD360_ICON_SPARK,
    CLOUD360_ICON_COUNT
} Cloud360Icon;

/* 6x6 icon pixels, packed into the XSDK font's top/bottom block glyphs. */
bool cloud360_icon_encode(Cloud360Icon icon, char *output, size_t capacity);
void cloud360_icon_draw(int32_t x, int32_t y, Cloud360Icon icon, uint32_t argb);

#endif
