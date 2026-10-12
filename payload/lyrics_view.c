/* SPDX-License-Identifier: GPL-3.0-only */
#include "lyrics.h"
#include "lyrics_scene.h"
#include "patch.h"

/* Only the stock lyric TextView is replaced. No raw framebuffer writes: Apple's
 * context applies the view/window clip and its existing font wraps UTF-8. */
#define WORD(p, n) (*(uint32_t *)((unsigned char *)(p) + (n)))
#define RECT(p, n) ((int32_t *)((unsigned char *)(p) + (n)))
#define PTR(p, n) (*(void **)((unsigned char *)(p) + (n)))
#define BYTE(p, n) (*((unsigned char *)(p) + (n)))
#define TIMER(p) ((unsigned char *)(p) + 0x1f4)
#define CONTEXT(p) ((unsigned char *)(p) + 0xa4)
#define CALL0(address, p) (((void (*)(void *))(address))(p))

static struct {
    void *owner, *background;
    struct cfw_lyrics_scene scene;
    uint32_t position, art_generation;
    int delay_ms, art_width, art_height;
    unsigned interval, armed, art_attempted;
} visual;

static int selected(void *view)
{
    return WORD(view, 0x40) == OSOS_PROPERTY_STRING &&
           WORD(view, 0x44) == OSOS_NOW_PLAYING_LYRICS;
}

static int visible(void *view)
{
    return (WORD(view, 0x48) & 0x1800) == 0x800 && !BYTE(view, 0xa0);
}

static void release(void *view)
{
    if (visual.owner != view) return;
    /* Stop removes both the pending timer and its queued UI event. */
    CALL0(OSOS_TIMER_STOP, TIMER(view));
    if (visual.background) CALL0(OSOS_IMPORT_FREE, visual.background);
    visual.owner = visual.background = 0;
    visual.scene.valid = visual.armed = visual.art_attempted = 0;
}

static void arm(void *view, unsigned interval)
{
    if (visual.armed) return;
    ((void (*)(void *, uint32_t))OSOS_TIMER_SET_INTERVAL)(TIMER(view), interval);
    CALL0(OSOS_TIMER_START, TIMER(view));
    visual.armed = 1;
}

static int panel_height(void *view)
{
    void *parent = PTR(view, 0x34);
    if (!parent) return 0;
    int top = RECT(view, 0x80)[0], bottom = RECT(parent, 0x80)[2];
    int clip_bottom = RECT(parent, 0xd0)[2];
    if (clip_bottom > top && clip_bottom < bottom) bottom = clip_bottom;
    /* The native lyric reading panel begins below the artwork/title header. */
    int64_t height = (int64_t)bottom - top;
    return height < 32 ? 0 : height > 140 ? 140 : (int)height;
}

PATCH_ARM void cfw_lyrics_view_measure(void *view)
{
    struct cfw_lyrics_snapshot snapshot;
    if (selected(view)) {
        CALL0(OSOS_TEXT_VIEW_LOAD, view);
        int height = panel_height(view);
        if (height && cfw_lyrics_snapshot(&snapshot)) {
            ((void (*)(void *, int))OSOS_VIEW_SET_HEIGHT)(view, height);
            return;
        }
    }
    CALL0(OSOS_TEXT_VIEW_MEASURE, view);
}

static int measure(void *context, const char *text, int width)
{
    return ((int (*)(void *, const char *, int, int))OSOS_GRAPHICS_MEASURE_WRAPPED)
        (context, text, width, 0);
}

static void make_background(uint32_t generation, int width, int height)
{
    if (visual.art_attempted && visual.art_generation == generation &&
        visual.art_width == width && visual.art_height == height) return;
    if (visual.background) CALL0(OSOS_IMPORT_FREE, visual.background);
    visual.background = 0;
    visual.art_attempted = 1;
    visual.art_generation = generation;
    visual.art_width = width;
    visual.art_height = height;
    unsigned char *bitmap = cfw_lyrics_artwork();
    if (!bitmap || !PTR(bitmap, 4)) return;
    const int32_t *bounds = RECT(bitmap, 0x98);
    int w = bounds[3], h = bounds[2];
    unsigned depth = WORD(bitmap, 0xc), stride = WORD(bitmap, 8);
    /* Only bounded, unpacked native RGB surfaces; otherwise use neutral fill.
     * Never retain the borrowed artwork or assume the track's codec/format. */
    if (bounds[0] || bounds[1] || w < 1 || w > 1024 || h < 1 || h > 1024 ||
        (depth != 16 && depth != 32) || stride < (unsigned)w * (depth / 8) ||
        stride > 4096) return;
    for (unsigned n = 0; n < 4; n++)
        if (RECT(bitmap, 0x18)[n] != bounds[n]) return;
    uint8_t rgb[CFW_LYRICS_SAMPLES][3];
    for (unsigned y = 0; y < CFW_LYRICS_SAMPLES_Y; y++) {
        for (unsigned x = 0; x < CFW_LYRICS_SAMPLES_X; x++) {
            uint8_t rgba[4] = {0, 0, 0, 255};
            ((void (*)(void *, int, int, void *))OSOS_SURFACE_READ_PIXEL)
                (bitmap + 4, (2 * x + 1) * w / (2 * CFW_LYRICS_SAMPLES_X),
                 (2 * y + 1) * h / (2 * CFW_LYRICS_SAMPLES_Y), rgba);
            for (unsigned c = 0; c < 3; c++) rgb[y * CFW_LYRICS_SAMPLES_X + x][c] = rgba[c];
        }
    }
    cfw_lyrics_blur(rgb);
    unsigned bytes = 28 + (((unsigned)width * 2 + 3) & ~3u) * (unsigned)height;
    void *data = ((void *(*)(uint32_t))OSOS_IMPORT_ALLOC)(bytes);
    if (!data) return; /* one attempt per track/size, no allocation retry loop */
    if (!cfw_lyrics_background(data, bytes, width, height, rgb)) {
        CALL0(OSOS_IMPORT_FREE, data);
        return;
    }
    visual.background = data;
}

static void draw_background(void *context, int32_t rect[4])
{
    ((void (*)(void *, unsigned))OSOS_GRAPHICS_SET_MODE)(context, 0);
    static const unsigned char neutral[4] = {13, 17, 23, 255};
    osos_graphics_set_color(context, neutral);
    ((void (*)(void *, const void *))OSOS_GRAPHICS_FILL)(context, rect);
    if (!visual.background) return;
    /* Stack objects are aligned and destroyed before their backing BMap can
     * be freed. Bitmap_LoadResource borrows pixels, it does not own them. */
    uint32_t bitmap[0xb8 / 4], source[0x44 / 4];
    CALL0(OSOS_BITMAP_CONSTRUCT, bitmap);
    CALL0(OSOS_GRAPHICS_CONSTRUCT, source);
    osos_bitmap_load_resource(bitmap, visual.background);
    osos_graphics_attach_bitmap(source, bitmap);
    int32_t src[4] = {0, 0, visual.scene.height, visual.scene.width};
    ((void (*)(void *, void *, const void *, const void *, unsigned, unsigned))OSOS_GRAPHICS_BLIT)
        (context, source, src, rect, 255, 0);
    CALL0(OSOS_GRAPHICS_DESTROY, source);
    CALL0(OSOS_BITMAP_DESTROY, bitmap);
}

PATCH_ARM void cfw_lyrics_view_draw(void *view, void *argument, void *clip)
{
    struct cfw_lyrics_snapshot snapshot;
    if (!selected(view) || !visible(view) || (WORD(view, 0xf0) & 0x40)) goto stock;
    CALL0(OSOS_TEXT_VIEW_LOAD, view);
    if (!cfw_lyrics_snapshot(&snapshot)) goto stock;
    cfw_lyrics_view_measure(view);
    int32_t rect[4];
    ((void (*)(void *, void *))OSOS_VIEW_CONTENT_RECT)(view, rect);
    int width = rect[3] - rect[1], height = rect[2] - rect[0];
    if (width < 48 || width > 320 || height < 32 || height > 240) goto stock;
    if (visual.owner != view) {
        if (visual.owner) release(visual.owner);
        visual.owner = view;
    }
    void *context = CONTEXT(view);
    ((void (*)(void *, void *))OSOS_VIEW_DRAW_CONTEXT)(view, context);
    int cue = cfw_lrc_current(snapshot.lrc, snapshot.position, snapshot.delay_ms);
    struct cfw_lyrics_scene *scene = &visual.scene;
    if (!scene->valid || scene->generation != snapshot.generation || scene->cue != cue ||
        scene->width != width || scene->height != height) {
        if (!cfw_lyrics_scene_layout(scene, snapshot.lrc, cue, width, height,
                                     snapshot.generation, measure, context)) goto stock;
    }
    visual.interval = cfw_lyrics_scene_step(scene, snapshot.lrc, snapshot.position, snapshot.delay_ms);
    visual.position = snapshot.position;
    visual.delay_ms = snapshot.delay_ms;
    make_background(snapshot.generation, width, height);
    draw_background(context, rect);
    ((void (*)(void *, unsigned))OSOS_GRAPHICS_SET_MODE)(context, 0x21);
    for (unsigned i = 0; i < scene->rows; i++) {
        struct cfw_lyrics_row *row = &scene->row[i];
        if (!row->text[0] || row->top >= height || row->top + row->height <= 0) continue;
        int32_t line[4] = {rect[0] + row->top, rect[1] + 12,
                          rect[0] + row->top + row->height, rect[3] - 12};
        unsigned char color[4] = {row->brightness, row->brightness, row->brightness, 255};
        osos_graphics_set_color(context, color);
        ((void (*)(void *, const char *, const void *, unsigned, int, int, int))OSOS_GRAPHICS_DRAW_WRAPPED)
            (context, row->text, line, BYTE(view, 0xe9), 0, 0, 0);
    }
    BYTE(view, 0x1f0) = 0;
    arm(view, visual.interval);
    return;
stock:
    release(view);
    ((void (*)(void *, void *, void *))OSOS_TEXT_VIEW_DRAW)(view, argument, clip);
}

PATCH_ARM int cfw_lyrics_view_event(void *view, const uint32_t *event)
{
    if (selected(view) && !(WORD(view, 0xf0) & 0x40) && event[1] == 15 &&
        event[2] == (uint32_t)(uintptr_t)TIMER(view)) {
        if (visual.owner != view) return 1; /* stale event: do not start stock blinking */
        visual.armed = 0;
        if (!visible(view)) { release(view); return 1; }
        struct cfw_lyrics_snapshot snapshot;
        unsigned interval = 100;
        int changed = 1;
        if (cfw_lyrics_snapshot(&snapshot) && visual.scene.generation == snapshot.generation &&
            visual.scene.cue == cfw_lrc_current(snapshot.lrc, snapshot.position, snapshot.delay_ms)) {
            int top[CFW_LYRICS_ROWS];
            unsigned color[CFW_LYRICS_ROWS];
            for (unsigned i = 0; i < visual.scene.rows; i++) {
                top[i] = visual.scene.row[i].top;
                color[i] = visual.scene.row[i].brightness;
            }
            interval = cfw_lyrics_scene_step(&visual.scene, snapshot.lrc, snapshot.position, snapshot.delay_ms);
            changed = 0;
            for (unsigned i = 0; i < visual.scene.rows; i++)
                if (top[i] != visual.scene.row[i].top || color[i] != visual.scene.row[i].brightness) changed = 1;
            if (snapshot.position == visual.position && snapshot.delay_ms == visual.delay_ms) interval = 100;
        }
        if (changed)
            ((void (*)(void *, const void *))OSOS_VIEW_INVALIDATE_RECT)(view, RECT(view, 0x80));
        arm(view, interval);
        return 1;
    }
    return ((int (*)(void *, const void *))OSOS_TEXT_VIEW_EVENT)(view, event);
}

PATCH_ARM void cfw_lyrics_view_visibility(void *view, unsigned shown)
{
    release(view);
    ((void (*)(void *, unsigned))OSOS_TEXT_VIEW_VISIBILITY)(view, shown);
}

void *cfw_lyrics_view_destroy_original(void *view);
PATCH_ARM void *cfw_lyrics_view_destroy(void *view)
{
    release(view);
    return cfw_lyrics_view_destroy_original(view);
}
