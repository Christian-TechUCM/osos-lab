/* SPDX-License-Identifier: GPL-3.0-only */
#include "theme_pixels.h"
#include "theme.h"

static unsigned read16(const unsigned char *p) { return p[0] | (unsigned)p[1] << 8; }
static unsigned read32(const unsigned char *p) { return read16(p) | read16(p + 2) << 16; }
static unsigned distance(unsigned a, unsigned b) { return a > b ? a - b : b - a; }
static unsigned map(unsigned rgb, unsigned role, struct cfw_palette p)
{
    if (role == 1) return p.background;
    if (role == 2) return p.sidebar;
    unsigned r = rgb >> 16, g = (rgb >> 8) & 255, b = rgb & 255;
    /* Recover the neutral ramp in the immutable, compiled navy template.
     * Tolerance covers RGB565 quantization, not arbitrary user artwork. */
    unsigned level = r <= 32 ? 0 : r >= 242 ? 255 : (r - 32) * 255 / 210;
    if (distance(g, 48 + 199 * level / 255) <= 8 &&
        distance(b, 73 + 182 * level / 255) <= 8) {
        unsigned result = 0;
        for (unsigned shift = 0; shift <= 16; shift += 8) {
            unsigned low = (p.background >> shift) & 255, high = (p.ink >> shift) & 255;
            result |= ((low * (255 - level) + high * level) / 255) << shift;
        }
        return result;
    }
    /* The template's cyan accent is a scaled (57,208,255) ramp. */
    if (b >= 24 && distance(r, 57 * b / 255) <= 8 && distance(g, 208 * b / 255) <= 8)
        return (((p.accent >> 16) * b / 255) << 16) |
               ((((p.accent >> 8) & 255) * b / 255) << 8) | ((p.accent & 255) * b / 255);
    return rgb; /* red/green semantic indicators stay unchanged */
}
void cfw_theme_bitmap(unsigned char *out, const unsigned char *source,
                      unsigned size, unsigned role, struct cfw_palette palette)
{
    if (!out || !source || size < 28) return;
    for (unsigned n = 0; n < size; n++) out[n] = source[n];
    unsigned depth = read16(source + 6), stride = read16(source + 4);
    unsigned height = read32(source + 16), width = read32(source + 20);
    if ((depth != 16 && depth != 32) || !width || !height || width > 320 || height > 240 ||
        stride < width * (depth / 8) || stride > (size - 28) / height) return;
    for (unsigned y = 0; y < height; y++) for (unsigned x = 0; x < width; x++) {
        unsigned offset = 28 + y * stride + x * (depth / 8);
        unsigned rgb;
        if (depth == 16) {
            unsigned pixel = read16(source + offset);
            rgb = (((pixel >> 11) * 255 / 31) << 16) |
                  ((((pixel >> 5) & 63) * 255 / 63) << 8) | ((pixel & 31) * 255 / 31);
        } else {
            if (!source[offset + 3]) continue;
            rgb = read32(source + offset) & 0xffffff;
        }
        unsigned mapped = map(rgb, role, palette);
        if (depth == 16) {
            if (mapped == rgb) continue; /* preserve exact semantic pixels */
            unsigned pixel = CFW_THEME_RGB565(mapped);
            out[offset] = pixel; out[offset + 1] = pixel >> 8;
        } else {
            out[offset] = mapped; out[offset + 1] = mapped >> 8; out[offset + 2] = mapped >> 16;
        }
    }
}
