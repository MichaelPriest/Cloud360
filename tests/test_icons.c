#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../src/icons.h"

static unsigned draw_calls = 0;

/* Test-only replacement for the XSDK font function. */
void xsdk_text_draw(int32_t x, int32_t y, uint32_t scale,
                    uint32_t argb, const char *text)
{
    (void)x;
    (void)y;
    (void)argb;
    assert(scale == 1);
    assert(text && text[0] != 0);
    ++draw_calls;
}

static void test_glyphs(void)
{
    for (int icon = 0; icon < CLOUD360_ICON_COUNT; ++icon) {
        char buffer[64] = { 0 };
        assert(cloud360_icon_encode((Cloud360Icon)icon, buffer, sizeof buffer));
        const size_t n = strlen(buffer);
        assert(n > 0 && n <= 56);
        unsigned lines = 0;
        unsigned pixels = 0;
        for (size_t i = 0; i < n; ++i) {
            lines += buffer[i] == '\n';
            pixels += (unsigned char)buffer[i] == 0xE2;
        }
        assert(lines == 2);
        assert(pixels > 0);
        cloud360_icon_draw(100, 120, (Cloud360Icon)icon, 0xFFFFFFFFu);
    }
    assert(draw_calls == CLOUD360_ICON_COUNT);
    char too_small[5] = { 0 };
    assert(!cloud360_icon_encode(CLOUD360_ICON_CLOUD, too_small, sizeof too_small));
    assert(!cloud360_icon_encode((Cloud360Icon)CLOUD360_ICON_COUNT,
                                 too_small, sizeof too_small));
}

int main(void)
{
    test_glyphs();
    puts("Cloud360 original pixel icons: PASS");
    return 0;
}
