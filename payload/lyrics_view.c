/* SPDX-License-Identifier: GPL-3.0-only */
#include "lyrics.h"
#include "lyrics_scene.h"
#include "lyrics_settings.h"
#include "dark_mode.h"
#include "theme.h"
#include "patch.h"
#include "lyrics_ui.h"

/* Only the lyric TextView and its private backdrop are replaced. Apple's
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
    unsigned armed_interval, objects_ready;
    uint32_t bitmap[0xb8 / 4], source[0x44 / 4];
    unsigned font, dark, color, artwork;
} visual;
static void *reading_view;
/* Power dispatch must never dereference a UI object during its destruction. */
static volatile unsigned reading_awake;

static int selected(void *view)
{
    return WORD(view, 0x40) == OSOS_PROPERTY_STRING &&
           WORD(view, 0x44) == OSOS_NOW_PLAYING_LYRICS;
}

static int visible(void *view)
{
    return (WORD(view, 0x48) & 0x1800) == 0x800 && !BYTE(view, 0xa0);
}

int cfw_lyrics_view_keep_awake(void)
{
    return reading_awake && cfw_lyrics_keep_awake();
}

static void free_background(void)
{
    if (visual.objects_ready) {
        CALL0(OSOS_GRAPHICS_DESTROY, visual.source);
        CALL0(OSOS_BITMAP_DESTROY, visual.bitmap);
        visual.objects_ready = 0;
    }
    if (visual.background) CALL0(OSOS_IMPORT_FREE, visual.background);
    visual.background = 0;
}

static void release(void *view)
{
    if (visual.owner != view) return;
    /* Stop removes both the pending timer and its queued UI event. */
    CALL0(OSOS_TIMER_STOP, TIMER(view));
    free_background();
    visual.owner = visual.background = 0;
    visual.scene.valid = visual.armed = visual.art_attempted = 0;
}

static void arm(void *view, unsigned interval)
{
    if (visual.armed && visual.armed_interval == interval) return;
    if (visual.armed) CALL0(OSOS_TIMER_STOP, TIMER(view));
    ((void (*)(void *, uint32_t))OSOS_TIMER_SET_INTERVAL)(TIMER(view), interval);
    CALL0(OSOS_TIMER_START, TIMER(view));
    visual.armed = 1;
    visual.armed_interval = interval;
}

static int panel_height(void *view)
{
    /* This child determines its auto-sized parent's height. Reading the
     * parent's previous bounds creates a one-line/zero-height feedback loop
     * on first entry. Reserve the reading panel; native clipping still applies. */
    return PTR(view, 0x34) ? 140 : 0;
}

static void size_panel(void *view, int height)
{
    if ((int64_t)RECT(view, 0x80)[2] - RECT(view, 0x80)[0] != height)
        ((void (*)(void *, int))OSOS_VIEW_SET_HEIGHT)(view, height);
}

PATCH_ARM void cfw_lyrics_view_measure(void *view)
{
    struct cfw_lyrics_snapshot snapshot;
    if (selected(view)) {
        CALL0(OSOS_TEXT_VIEW_LOAD, view);
        int height = panel_height(view);
        if (height && cfw_lyrics_snapshot(&snapshot)) {
            size_panel(view, height);
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
    free_background();
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
    CALL0(OSOS_BITMAP_CONSTRUCT, visual.bitmap);
    CALL0(OSOS_GRAPHICS_CONSTRUCT, visual.source);
    osos_bitmap_load_resource(visual.bitmap, data);
    osos_graphics_attach_bitmap(visual.source, visual.bitmap);
    visual.objects_ready = 1;
}

static void draw_background(void *view, void *context, int32_t rect[4])
{
    ((void (*)(void *, unsigned))OSOS_GRAPHICS_SET_MODE)(context, 0);
    if (!visual.dark || !visual.artwork || !visual.background) {
        unsigned rgb = visual.dark ? CFW_THEME_BACKGROUND : 0xffffffu;
        unsigned char neutral[4] = {rgb >> 16, rgb >> 8, rgb, 255};
        osos_graphics_set_color(context, neutral);
        ((void (*)(void *, const void *))OSOS_GRAPHICS_FILL)(context, rect);
        return;
    }
    /* Native +0x80 bounds are screen-space; content_rect is local. Crop every
     * child from the SAME screen-sized image so header/margins/panel meet.
     * The native destination context continues to enforce clipping. */
    int32_t src[4];
    for (unsigned i = 0; i < 4; i++)
        src[i] = rect[i] + RECT(view, 0x80)[i & 1];
    if (src[0] < 0 || src[1] < 0 || src[2] > 240 || src[3] > 320) {
        unsigned rgb = CFW_THEME_BACKGROUND;
        unsigned char neutral[4] = {rgb >> 16, rgb >> 8, rgb, 255};
        osos_graphics_set_color(context, neutral);
        ((void (*)(void *, const void *))OSOS_GRAPHICS_FILL)(context, rect);
        return;
    }
    ((void (*)(void *, void *, const void *, const void *, unsigned, unsigned))OSOS_GRAPHICS_BLIT)
        (context, visual.source, src, rect, 255, 0);
}

PATCH_ARM void cfw_lyrics_backdrop_draw(void *view, void *argument, void *clip)
{
    /* The stock color-only BitmapView uses +0x44 for its background COLR
     * regardless of the image-source type at +0x40 (which may be empty). */
    if (WORD(view, 0x44) != CFW_LYRICS_BACKDROP || !cfw_dark_mode_enabled()) {
        ((void (*)(void *, void *, void *))OSOS_BITMAP_VIEW_DRAW)(view, argument, clip);
        return;
    }
    unsigned flags = WORD(view, 0x48);
    /* Match the two stock BitmapView draw-suppression guards exactly. */
    if (((flags & 0x10000000) && !(flags & 8)) ||
        ((flags & 0x20000000) && (flags & 8))) return;
    int32_t rect[4];
    ((void (*)(void *, void *))OSOS_VIEW_CONTENT_RECT)(view, rect);
    if (rect[2] <= rect[0] || rect[3] <= rect[1]) return;
    visual.dark = 1;
    visual.artwork = cfw_lyrics_album_background();
    if (visual.artwork) make_background(cfw_lyrics_generation(), 320, 240);
    uint32_t context[0x44 / 4];
    CALL0(OSOS_GRAPHICS_CONSTRUCT, context);
    ((void (*)(void *, void *))OSOS_VIEW_DRAW_CONTEXT)(view, context);
    draw_background(view, context, rect);
    CALL0(OSOS_GRAPHICS_DESTROY, context);
}

PATCH_ARM void cfw_lyrics_view_draw(void *view, void *argument, void *clip)
{
    struct cfw_lyrics_snapshot snapshot;
    if (selected(view)) {
        reading_view = visible(view) ? view : 0;
        reading_awake = reading_view != 0;
    }
    if (!selected(view) || !visible(view) || (WORD(view, 0xf0) & 0x40)) goto stock;
    /* Native events invalidate the source; animation frames reuse its copied
     * LRC instead of repeatedly scanning/loading the entire metadata tag. */
    if (visual.owner != view || !cfw_lyrics_snapshot(&snapshot))
        CALL0(OSOS_TEXT_VIEW_LOAD, view);
    if (!cfw_lyrics_snapshot(&snapshot)) goto stock;
    int panel = panel_height(view);
    if (!panel) goto stock;
    size_panel(view, panel);
    int32_t rect[4];
    ((void (*)(void *, void *))OSOS_VIEW_CONTENT_RECT)(view, rect);
    int width = rect[3] - rect[1], height = rect[2] - rect[0];
    if (width < 48 || width > 320 || height < 32 || height > 240) goto stock;
    if (visual.owner != view) {
        if (visual.owner) release(visual.owner);
        visual.owner = view;
    }
    void *context = CONTEXT(view);
    unsigned font = cfw_lyrics_font();
    if (!visual.scene.valid || visual.font != font || visual.scene.generation != snapshot.generation) {
        static const uint32_t fonts[] = {0x0dad0013, 0x0dad0012, 0x0dad001e};
        ((void (*)(void *, void *, uint32_t, int))OSOS_TEXT_VIEW_SET_FONT)
            (view, PTR(view, 0x38), fonts[font < 3 ? font : 1], 0);
        visual.font = font;
        visual.scene.valid = 0;
    }
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
    visual.dark = cfw_dark_mode_enabled();
    visual.color = cfw_lyrics_color();
    visual.artwork = cfw_lyrics_album_background();
    if (visual.dark && visual.artwork) make_background(snapshot.generation, 320, 240);
    draw_background(view, context, rect);
    ((void (*)(void *, unsigned))OSOS_GRAPHICS_SET_MODE)(context, 0x21);
    for (unsigned i = 0; i < scene->rows; i++) {
        struct cfw_lyrics_row *row = &scene->row[i];
        if (!row->text[0] || row->top >= height || row->top + row->height <= 0) continue;
        int32_t line[4] = {rect[0] + row->top, rect[1] + 12,
                          rect[0] + row->top + row->height, rect[3] - 12};
        static const unsigned light[] = {0x0969da, 0x0969da, 0x006d80, 0x7540ad, 0x805500};
        static const unsigned dark[] = {0xf2f7ff, 0x8ac2ff, 0x80e5ff, 0xd8b7ff, 0xffdf91};
        unsigned choice = visual.color < 5 ? visual.color : 0;
        unsigned rgb = visual.dark ? dark[choice] : light[choice];
        unsigned char color[4] = {rgb >> 16, rgb >> 8, rgb, 255};
        for (unsigned c = 0; c < 3; c++) {
            if (visual.dark) color[c] = color[c] * row->brightness / 250;
            else color[c] += (255 - color[c]) * (250 - row->brightness) / 180;
        }
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
        if (!visible(view)) {
            if (reading_view == view) { reading_awake = 0; reading_view = 0; }
            release(view);
            return 1;
        }
        struct cfw_lyrics_snapshot snapshot;
        unsigned interval = 33; /* a new cue must not inherit the idle 100 ms poll */
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
    int result = ((int (*)(void *, const void *))OSOS_TEXT_VIEW_EVENT)(view, event);
    /* Let the native property handler refresh before the next draw. */
    if (visual.owner == view && event[1] != 15) CALL0(OSOS_TEXT_VIEW_LOAD, view);
    return result;
}

PATCH_ARM void cfw_lyrics_view_visibility(void *view, unsigned shown)
{
    if (reading_view == view) { reading_awake = 0; reading_view = 0; }
    release(view);
    ((void (*)(void *, unsigned))OSOS_TEXT_VIEW_VISIBILITY)(view, shown);
}

void *cfw_lyrics_view_destroy_original(void *view);
PATCH_ARM void *cfw_lyrics_view_destroy(void *view)
{
    if (reading_view == view) { reading_awake = 0; reading_view = 0; }
    release(view);
    return cfw_lyrics_view_destroy_original(view);
}
