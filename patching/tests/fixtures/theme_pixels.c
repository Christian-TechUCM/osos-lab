/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <string.h>
#include "../../../payload/theme_pixels.c"
int main(void) {
    struct cfw_palette p = {0x11121a, 0x0a0b13, 0xf1efff, 0x8b7cf6, 0x43386f, 0x38334f};
    assert(map(CFW_THEME_BACKGROUND, 0, p) == p.background);
    assert(map(CFW_THEME_INK, 0, p) == p.ink);
    assert(map(CFW_THEME_ACCENT, 0, p) == p.accent);
    assert(map(0xff0000, 0, p) == 0xff0000);
    assert(map(0x00ff00, 0, p) == 0x00ff00);
    unsigned char source[44] = {0x88, 0x18, 0, 0, 16, 0, 32, 0};
    source[16] = 1; source[20] = 4;
    unsigned char pixels[] = {73,48,32,255, 255,247,242,255, 255,208,57,123, 7,8,9,0};
    memcpy(source + 28, pixels, 16);
    unsigned char out[46]; memset(out, 0xa5, sizeof(out));
    cfw_theme_bitmap(out + 1, source, sizeof(source), 0, p);
    assert(out[0] == 0xa5 && out[45] == 0xa5);
    assert(read32(out + 29) == (0xff000000 | p.background));
    assert(read32(out + 33) == (0xff000000 | p.ink));
    assert(read32(out + 37) == (0x7b000000 | p.accent));
    assert(!memcmp(out + 41, source + 40, 4)); /* transparent pixels untouched */
    source[20] = 255; /* invalid bounds fail to the intact original */
    cfw_theme_bitmap(out + 1, source, sizeof(source), 0, p);
    assert(!memcmp(out + 1, source, sizeof(source)));
    source[20] = 1; source[6] = 16; source[4] = 4;
    source[28] = 0; source[29] = 0;
    cfw_theme_bitmap(out + 1, source, 32, 1, p);
    assert(read16(out + 29) == CFW_THEME_RGB565(p.background));
    assert(!memcmp(source + 28, "\0\0", 2)); /* never recolor the immutable template */
}
