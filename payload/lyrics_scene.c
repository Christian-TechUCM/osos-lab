/* SPDX-License-Identifier: GPL-3.0-only */
#include "lyrics_scene.h"
#include "theme.h"

static int next(const struct cfw_lrc *lrc, int cue)
{
    if (cue < 0) return lrc->count ? 0 : -1;
    unsigned i = (unsigned)cue + 1;
    while (i < lrc->count && lrc->cues[i].milliseconds == lrc->cues[cue].milliseconds) i++;
    return i < lrc->count ? (int)i : -1;
}

static int previous(const struct cfw_lrc *lrc, int cue)
{
    if (cue <= 0) return -1;
    int i = cue - 1;
    while (i && lrc->cues[i - 1].milliseconds == lrc->cues[i].milliseconds) i--;
    return i;
}

int cfw_lyrics_scene_layout(struct cfw_lyrics_scene *scene, const struct cfw_lrc *lrc,
                            int cue, int width, int height, uint32_t generation,
                            cfw_lyrics_measure measure, void *context)
{
    /* Adjacent cue windows overlap. Reuse wrapping measurements, but never
     * across a tag, width, or font change (font changes clear valid). */
    int old_cues[CFW_LYRICS_ROWS], old_heights[CFW_LYRICS_ROWS];
    unsigned old_rows = scene->valid && scene->generation == generation &&
                        scene->width == width ? scene->rows : 0;
    if (old_rows > CFW_LYRICS_ROWS) old_rows = 0;
    for (unsigned n = 0; n < old_rows; n++) {
        old_cues[n] = scene->row[n].cue;
        old_heights[n] = scene->row[n].height;
    }
    scene->valid = 0;
    scene->rows = 0;
    if (!lrc || !lrc->count || !measure || width < 48 || width > 320 ||
        height < 32 || height > 240 || cue < -1 || cue >= (int)lrc->count) return 0;
    scene->cue = cue;
    scene->width = width;
    scene->height = height;
    scene->generation = generation;
    scene->active = 0;
    /* Two previous rows keep the outgoing line visible during a transition. */
    int first = cue < 0 ? 0 : cue;
    for (unsigned n = 0; n < 2; n++) {
        int p = previous(lrc, first);
        if (p < 0) break;
        first = p;
        scene->active++;
    }
    for (int i = first; i >= 0 && scene->rows < CFW_LYRICS_ROWS; i = next(lrc, i)) {
        struct cfw_lyrics_row *row = &scene->row[scene->rows++];
        row->cue = i;
        if (!cfw_lrc_copy(lrc, (unsigned)i, row->text, sizeof(row->text))) return 0;
        row->height = 0;
        for (unsigned n = 0; n < old_rows; n++)
            if (old_cues[n] == i) row->height = old_heights[n];
        if (!row->height)
            row->height = row->text[0] ? measure(context, row->text, width - 24) : 24;
        if (row->height < 1 || row->height > 4096) return 0;
    }
    scene->valid = 1;
    return 1;
}

static unsigned ease(unsigned elapsed, unsigned duration)
{
    if (elapsed >= duration) return 256;
    unsigned t = elapsed * 256 / duration;
    return t * t * (768 - 2 * t) / 65536;
}

unsigned cfw_lyrics_scene_step(struct cfw_lyrics_scene *scene, const struct cfw_lrc *lrc,
                               uint32_t position, int delay_ms)
{
    if (!scene->valid || !scene->rows) return 100;
    unsigned a = scene->active, progress = 256;
    int64_t adjusted = (int64_t)position - delay_ms;
    uint32_t elapsed = 0, duration = 0;
    if (scene->cue >= 0) {
        int n = next(lrc, scene->cue);
        duration = n < 0 ? 0 : lrc->cues[n].milliseconds - lrc->cues[scene->cue].milliseconds;
        int64_t since = adjusted - lrc->cues[scene->cue].milliseconds;
        elapsed = since <= 0 ? 0 : since > UINT32_MAX ? UINT32_MAX : (uint32_t)since;
        if (a) progress = ease(elapsed, duration && duration < 500 ? duration : 500);
    }
    int h = scene->row[a].height;
    int top = (scene->height - h) / 2;
    unsigned interval = progress < 256 ? 33 : 100;
    if (h > scene->height - 24) {
        /* Long cues start at the top, then pan across their available time. */
        unsigned available = duration ? duration : 6000;
        unsigned pan = available > 1600 && elapsed > 800 ?
            ease(elapsed - 800, available - 1600) : 0;
        top = 12 - (h - scene->height + 24) * (int)pan / 256;
        if (pan && pan < 256) interval = 33;
    }
    if (a) {
        int previous_height = scene->row[a - 1].height;
        int previous_top = (scene->height - previous_height) / 2;
        int p = scene->row[a - 1].cue;
        unsigned previous_duration = lrc->cues[scene->cue].milliseconds - lrc->cues[p].milliseconds;
        if (previous_height > scene->height - 24)
            previous_top = previous_duration > 1600 ? scene->height - previous_height - 12 : 12;
        int start = previous_top + previous_height + 18;
        top += (start - top) * (int)(256 - progress) / 256;
    } else {
        progress = scene->cue < 0 ? 0 : ease(elapsed, duration && duration < 500 ? duration : 500);
        top += (scene->height / 2 + 18 - top) * (int)(256 - progress) / 256;
        if (scene->cue >= 0 && progress < 256) interval = 33;
    }
    scene->row[a].top = top;
    for (unsigned i = a; i > 0; i--)
        scene->row[i - 1].top = scene->row[i].top - scene->row[i - 1].height - 18;
    for (unsigned i = a + 1; i < scene->rows; i++)
        scene->row[i].top = scene->row[i - 1].top + scene->row[i - 1].height + 18;
    for (unsigned i = 0; i < scene->rows; i++) {
        unsigned strength = scene->cue < 0 ? 0 : i == a ? progress :
                            i + 1 == a ? 256 - progress : 0;
        scene->row[i].brightness = 180 + 70 * strength / 256;
    }
    /* Start sampling faster BEFORE a cue boundary, not up to 100 ms after it.
     * Keep the idle interval between transitions to avoid needless redraws. */
    int following = next(lrc, scene->cue);
    if (following >= 0) {
        int64_t remaining = (int64_t)lrc->cues[following].milliseconds - adjusted;
        if (remaining > 0 && remaining <= 150) interval = 33;
    }
    return interval;
}

void cfw_lyrics_blur_art(uint8_t rgb[CFW_LYRICS_SAMPLES][3], unsigned background,
                         int light, unsigned level)
{
    /* One scanline of scratch, rather than a second thumbnail on the stack. */
    uint8_t temp[CFW_LYRICS_SAMPLES_X][3];
    /* Three separable box passes approximate a Gaussian, with bounded work
     * only on cache creation. Never run this in the scrolling frame loop. */
    int radius = level == 0 ? 1 : level == 1 ? 2 : 4;
    unsigned divisor = (unsigned)(2 * radius + 1);
    for (unsigned pass = 0; pass < 3; pass++) {
        for (int y = 0; y < CFW_LYRICS_SAMPLES_Y; y++) {
            for (int x = 0; x < CFW_LYRICS_SAMPLES_X; x++) {
                unsigned total[3] = {0, 0, 0};
                for (int dx = -radius; dx <= radius; dx++) {
                    int sx = x + dx;
                    if (sx < 0) sx = 0;
                    if (sx >= CFW_LYRICS_SAMPLES_X) sx = CFW_LYRICS_SAMPLES_X - 1;
                    for (unsigned c = 0; c < 3; c++) total[c] += rgb[y * CFW_LYRICS_SAMPLES_X + sx][c];
                }
                for (unsigned c = 0; c < 3; c++) temp[x][c] = total[c] / divisor;
            }
            for (unsigned x = 0; x < CFW_LYRICS_SAMPLES_X; x++)
                for (unsigned c = 0; c < 3; c++) rgb[y * CFW_LYRICS_SAMPLES_X + x][c] = temp[x][c];
        }
        for (int x = 0; x < CFW_LYRICS_SAMPLES_X; x++) {
            for (int y = 0; y < CFW_LYRICS_SAMPLES_Y; y++) {
                unsigned total[3] = {0, 0, 0};
                for (int dy = -radius; dy <= radius; dy++) {
                    int sy = y + dy;
                    if (sy < 0) sy = 0;
                    if (sy >= CFW_LYRICS_SAMPLES_Y) sy = CFW_LYRICS_SAMPLES_Y - 1;
                    for (unsigned c = 0; c < 3; c++) total[c] += rgb[sy * CFW_LYRICS_SAMPLES_X + x][c];
                }
                for (unsigned c = 0; c < 3; c++) temp[y][c] = total[c] / divisor;
            }
            for (unsigned y = 0; y < CFW_LYRICS_SAMPLES_Y; y++)
                for (unsigned c = 0; c < 3; c++) rgb[y * CFW_LYRICS_SAMPLES_X + x][c] = temp[y][c];
        }
    }
    /* Dark enough for muted neighboring lines as well as the highlighted line. */
    const unsigned neutral[3] = {background >> 16, (background >> 8) & 255, background & 255};
    unsigned weight = light ? 15 : 30;
    for (unsigned i = 0; i < CFW_LYRICS_SAMPLES; i++)
        for (unsigned c = 0; c < 3; c++) rgb[i][c] = (rgb[i][c] * weight + neutral[c] * (100 - weight)) / 100;
}

void cfw_lyrics_blur_palette(uint8_t rgb[CFW_LYRICS_SAMPLES][3], unsigned background, int light)
{
    cfw_lyrics_blur_art(rgb, background, light, 2);
}

void cfw_lyrics_blur(uint8_t rgb[CFW_LYRICS_SAMPLES][3])
{
    cfw_lyrics_blur_palette(rgb, CFW_THEME_BACKGROUND, 0);
}

static void word(uint8_t *out, unsigned offset, uint32_t value, unsigned size)
{
    for (unsigned n = 0; n < size; n++) out[offset + n] = (uint8_t)(value >> (n * 8));
}

int cfw_lyrics_background(void *output, size_t capacity, unsigned width, unsigned height,
                          const uint8_t rgb[CFW_LYRICS_SAMPLES][3])
{
    if (!output || !rgb || !width || width > 320 || !height || height > 240) return 0;
    unsigned stride = (width * 2 + 3) & ~3u;
    unsigned size = 28 + stride * height;
    if (capacity < size) return 0;
    uint8_t *out = output;
    for (unsigned n = 0; n < size; n++) out[n] = 0;
    word(out, 0, 0x565, 2); word(out, 4, stride, 2); word(out, 6, 16, 2);
    word(out, 16, height, 4); word(out, 20, width, 4); word(out, 24, stride * height, 4);
    for (unsigned y = 0; y < height; y++) {
        unsigned fy = height > 1 ? y * (CFW_LYRICS_SAMPLES_Y - 1) * 256 / (height - 1) : 0;
        unsigned ay = fy / 256, by = ay + (ay + 1 < CFW_LYRICS_SAMPLES_Y);
        fy &= 255;
        for (unsigned x = 0; x < width; x++) {
            unsigned fx = width > 1 ? x * (CFW_LYRICS_SAMPLES_X - 1) * 256 / (width - 1) : 0;
            unsigned ax = fx / 256, bx = ax + (ax + 1 < CFW_LYRICS_SAMPLES_X), color[3];
            fx &= 255;
            for (unsigned c = 0; c < 3; c++) {
                unsigned top = rgb[ay * CFW_LYRICS_SAMPLES_X + ax][c] * (256 - fx) + rgb[ay * CFW_LYRICS_SAMPLES_X + bx][c] * fx;
                unsigned bottom = rgb[by * CFW_LYRICS_SAMPLES_X + ax][c] * (256 - fx) + rgb[by * CFW_LYRICS_SAMPLES_X + bx][c] * fx;
                color[c] = (top * (256 - fy) + bottom * fy) / 65536;
            }
            /* RGB565 has few dark tones. Ordered dithering trades visible
             * gradient bands for a bounded, one-quantization-step pattern. */
            static const uint8_t bayer[16] = {0, 8, 2, 10, 12, 4, 14, 6,
                                             3, 11, 1, 9, 15, 7, 13, 5};
            unsigned noise = bayer[(y & 3) * 4 + (x & 3)];
            unsigned r = (color[0] + noise / 2) >> 3;
            unsigned g = (color[1] + noise / 4) >> 2;
            unsigned b = (color[2] + noise / 2) >> 3;
            if (r > 31) r = 31;
            if (g > 63) g = 63;
            if (b > 31) b = 31;
            unsigned pixel = r << 11 | g << 5 | b;
            word(out, 28 + y * stride + x * 2, pixel, 2);
        }
    }
    return 1;
}
