/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#define _Static_assert(...)
#include "../../../payload/lyrics.h"
#undef _Static_assert
#define CFW_PATCH_H
#define PATCH_ARM

static unsigned char view[0x240], parent[0x180], art[0xb8], unrelated[0x240];
static int enabled = 1, artwork_enabled = 1, oom, allocs, frees, samples, starts, stops;
static int stock_draws, stock_events, stock_measures, stock_shown, destroyed, invalidations;
static int text_draws, blits, fills, loaded;
static unsigned interval;
static void *timer_owner;
static unsigned setting_font = 1, setting_color, setting_art = 1, setting_awake = 1;
static int dark = 1, font_sets, measurements;
unsigned cfw_lyrics_font(void) { return setting_font; }
unsigned cfw_lyrics_color(void) { return setting_color; }
unsigned cfw_lyrics_album_background(void) { return setting_art; }
unsigned cfw_lyrics_keep_awake(void) { return setting_awake; }
int cfw_dark_mode_enabled(void) { return dark; }
static void set_font(void *p, void *service, uint32_t font, int invalid) {
    /* 64-bit host fields overlap native four-byte slots; service is opaque. */
    (void)service;
    assert(p == view && !invalid);
    assert(font == 0x0dad0013 || font == 0x0dad0012 || font == 0x0dad001e); font_sets++;
}
static struct cfw_lyrics_snapshot snap;
int cfw_lyrics_snapshot(struct cfw_lyrics_snapshot *out) { *out = snap; return enabled; }
void *cfw_lyrics_artwork(void) { return artwork_enabled ? art : 0; }
static void native_load(void *p) { assert(p == view || p == unrelated); loaded++; }
static void native_draw(void *p, void *a, void *c) { assert(p && a == (void *)7 && c == (void *)9); stock_draws++; }
static int native_event(void *p, const void *e) { assert(p && e); stock_events++; return 23; }
static void native_measure(void *p) { assert(p); stock_measures++; }
static void native_visibility(void *p, unsigned shown) { assert(p); stock_shown += shown ? 1 : 2; }
void *cfw_lyrics_view_destroy_original(void *p) { assert(p); destroyed++; return p; }
static void stop(void *p) { assert(p == timer_owner); stops++; }
static void start(void *p) { assert(p == timer_owner); starts++; }
static void set_interval(void *p, uint32_t n) { assert(p == timer_owner && (n == 33 || n == 100)); interval = n; }
static void *allocate(uint32_t n) { allocs++; assert(n <= 153628); return oom ? 0 : calloc(1, n); }
static void deallocate(void *p) { assert(p); frees++; free(p); }
static void set_height(void *p, int h) { assert(p == view && h <= 140); ((int32_t *)(view + 0x80))[2] = 100 + h; }
static void content_rect(void *p, void *out) { assert(p == view); int32_t r[] = {0, 0, 140, 304}; memcpy(out, r, sizeof(r)); }
static void draw_context(void *p, void *c) { assert(p == view && c == view + 0xa4); }
static int wrapped_height(void *c, const char *s, int w, int spacing) { assert(c == view + 0xa4 && s && w == 280 && !spacing); measurements++; return 24; }
static void read_pixel(void *surface, int x, int y, void *out) {
    assert(surface == art + 4 && x >= 0 && x < 100 && y >= 0 && y < 100);
    unsigned char c[] = {(unsigned char)(x * 2), (unsigned char)(y * 2), 80, 255};
    memcpy(out, c, sizeof(c)); samples++;
}
static void mode(void *c, unsigned m) { assert(c == view + 0xa4 && (m == 0 || m == 0x21)); }
static void set_color(void *c, const unsigned char *rgba) { assert(c == view + 0xa4 && rgba[3] == 255); }
static void fill(void *c, const void *rect) { assert(c == view + 0xa4); const int32_t *r = rect; assert(r[0] == 0 && r[1] == 0 && r[2] == 140 && r[3] == 304); fills++; }
static void construct(void *p) { assert(p); }
static void dispose(void *p) { assert(p); }
static void bitmap_load(void *p, const void *data) { assert(p && data); }
static void attach(void *c, void *b) { assert(c && b); }
static void blit(void *c, void *s, const void *sr, const void *dr, unsigned a, unsigned unused) {
    assert(c == view + 0xa4 && s && a == 255 && !unused && !memcmp(sr, dr, 16)); blits++;
}
static void text(void *c, const char *s, const void *rect, unsigned align, int spacing, int x, int y) {
    const int32_t *r = rect;
    assert(c == view + 0xa4 && s && r[1] == 12 && r[3] == 292 && !align && !spacing && !x && !y);
    text_draws++;
}
static void invalidate(void *p, const void *rect) { assert(p == view && rect == view + 0x80); invalidations++; }

#undef OSOS_TEXT_VIEW_LOAD
#undef OSOS_TEXT_VIEW_SET_FONT
#undef OSOS_TEXT_VIEW_DRAW
#undef OSOS_TEXT_VIEW_EVENT
#undef OSOS_TEXT_VIEW_VISIBILITY
#undef OSOS_TEXT_VIEW_MEASURE
#undef OSOS_TIMER_STOP
#undef OSOS_TIMER_START
#undef OSOS_TIMER_SET_INTERVAL
#undef OSOS_IMPORT_FREE
#undef OSOS_IMPORT_ALLOC
#undef OSOS_VIEW_SET_HEIGHT
#undef OSOS_VIEW_CONTENT_RECT
#undef OSOS_VIEW_DRAW_CONTEXT
#undef OSOS_VIEW_INVALIDATE_RECT
#undef OSOS_GRAPHICS_MEASURE_WRAPPED
#undef OSOS_GRAPHICS_DRAW_WRAPPED
#undef OSOS_SURFACE_READ_PIXEL
#undef OSOS_GRAPHICS_SET_MODE
#undef OSOS_GRAPHICS_FILL
#undef OSOS_GRAPHICS_BLIT
#undef OSOS_GRAPHICS_CONSTRUCT
#undef OSOS_GRAPHICS_DESTROY
#undef OSOS_BITMAP_CONSTRUCT
#undef OSOS_BITMAP_DESTROY
#define OSOS_TEXT_VIEW_LOAD native_load
#define OSOS_TEXT_VIEW_SET_FONT set_font
#define OSOS_TEXT_VIEW_DRAW native_draw
#define OSOS_TEXT_VIEW_EVENT native_event
#define OSOS_TEXT_VIEW_VISIBILITY native_visibility
#define OSOS_TEXT_VIEW_MEASURE native_measure
#define OSOS_TIMER_STOP stop
#define OSOS_TIMER_START start
#define OSOS_TIMER_SET_INTERVAL set_interval
#define OSOS_IMPORT_FREE deallocate
#define OSOS_IMPORT_ALLOC allocate
#define OSOS_VIEW_SET_HEIGHT set_height
#define OSOS_VIEW_CONTENT_RECT content_rect
#define OSOS_VIEW_DRAW_CONTEXT draw_context
#define OSOS_VIEW_INVALIDATE_RECT invalidate
#define OSOS_GRAPHICS_MEASURE_WRAPPED wrapped_height
#define OSOS_GRAPHICS_DRAW_WRAPPED text
#define OSOS_SURFACE_READ_PIXEL read_pixel
#define OSOS_GRAPHICS_SET_MODE mode
#define OSOS_GRAPHICS_FILL fill
#define OSOS_GRAPHICS_BLIT blit
#define OSOS_GRAPHICS_CONSTRUCT construct
#define OSOS_GRAPHICS_DESTROY dispose
#define OSOS_BITMAP_CONSTRUCT construct
#define OSOS_BITMAP_DESTROY dispose
#define osos_graphics_set_color set_color
#define osos_bitmap_load_resource bitmap_load
#define osos_graphics_attach_bitmap attach
#include "../../../payload/lyrics_lrc.c"
#include "../../../payload/lyrics_scene.c"
#include "../../../payload/lyrics_view.c"

static void draw(void) { cfw_lyrics_view_draw(view, (void *)7, (void *)9); }
static void tick(void) {
    uint32_t e[] = {0, 15, (uint32_t)(uintptr_t)TIMER(view), 0};
    assert(cfw_lyrics_view_event(view, e) == 1);
}
int main(void)
{
    struct cfw_lrc lrc;
    const char *tag = "[00:01]First\n[00:03]Second\n[00:05]Third\n[00:08]Last";
    assert(cfw_lrc_parse(&lrc, tag, strlen(tag)));
    snap.lrc = &lrc; snap.generation = 1; snap.position = 1400;
    WORD(view, 0x40) = OSOS_PROPERTY_STRING; WORD(view, 0x44) = OSOS_NOW_PLAYING_LYRICS;
    WORD(view, 0x48) = 0x800; PTR(view, 0x34) = parent;
    RECT(view, 0x80)[0] = 100; RECT(parent, 0x80)[2] = RECT(parent, 0xd0)[2] = 240;
    PTR(art, 4) = art; WORD(art, 8) = 200; WORD(art, 0xc) = 16;
    RECT(art, 0x18)[2] = RECT(art, 0x98)[2] = 100;
    RECT(art, 0x18)[3] = RECT(art, 0x98)[3] = 100;
    /* A host pointer at bitmap+4 overlaps stride+8: only non-null is tested. */
    timer_owner = TIMER(view);
    draw();
    assert(cfw_lyrics_view_keep_awake());
    setting_awake = 0; assert(!cfw_lyrics_view_keep_awake()); setting_awake = 1;
    assert(visual.owner == view && visual.background && allocs == 1 && samples == 192);
    assert(blits == 1 && fills == 0 && text_draws && starts == 1 && interval == 100);
    int first_loads = loaded, first_measures = measurements;
    draw(); assert(allocs == 1 && samples == 192 && starts == 1); /* cached, timer not postponed */
    assert(loaded == first_loads && measurements == first_measures && font_sets == 1);
    tick(); assert(!invalidations && interval == 100); /* pause: no redraw */
    snap.position = 2400; tick(); assert(!invalidations); /* steady cue: no redraw */
    snap.position = 3000; tick(); assert(invalidations == 1); draw();
    assert(measurements == first_measures); /* overlapping rows reused */
    snap.position = 3100; tick(); assert(invalidations == 2 && interval == 33); draw();
    tick(); assert(invalidations == 2 && interval == 100); /* pause mid-transition */
    snap.position = 1000; tick(); assert(invalidations == 3); draw(); /* backward seek */
    snap.delay_ms = 500; tick(); assert(invalidations == 4); draw();
    snap.generation++; tick(); draw(); assert(allocs == 2 && frees == 1 && samples == 384);
    int stopped = stops;
    cfw_lyrics_view_visibility(view, 0);
    assert(!cfw_lyrics_view_keep_awake());
    assert(!visual.owner && !visual.background && frees == 2 && stops == stopped + 1);
    int started = starts; tick(); assert(starts == started); /* stale event consumed */
    cfw_lyrics_view_visibility(view, 1); draw(); assert(visual.owner == view);
    assert(cfw_lyrics_view_destroy(view) == view && !visual.owner && !visual.background);
    assert(!cfw_lyrics_view_keep_awake());
    started = starts; tick(); assert(starts == started);
    oom = 1; draw(); assert(visual.owner == view && !visual.background);
    int attempts = allocs; draw(); assert(allocs == attempts); /* no OOM retry storm */
    cfw_lyrics_view_visibility(view, 0); oom = 0; artwork_enabled = 0;
    draw(); assert(!visual.background && allocs == attempts);
    artwork_enabled = 1; snap.generation++; draw(); assert(visual.background);
    attempts = allocs;
    WORD(art, 0xc) = 8; snap.generation++; draw();
    assert(!visual.background && allocs == attempts); /* unsupported surface */
    WORD(art, 0xc) = 16; RECT(art, 0x98)[3] = 1025; snap.generation++; draw();
    assert(!visual.background && allocs == attempts); /* corrupt bounds */
    RECT(art, 0x98)[3] = 100;
    /* Appearance changes do not sample artwork in light mode. */
    dark = 0; snap.generation++; attempts = samples; int old_blits = blits;
    draw(); assert(samples == attempts && blits == old_blits && fills);
    setting_color = 3; setting_font = 2; draw(); assert(visual.font == 2 && visual.color == 3);
    setting_art = 0; dark = 1; draw(); assert(samples == attempts && blits == old_blits);
    setting_art = 1; draw(); assert(samples > attempts && blits > old_blits);
    /* First layout may have an empty or one-line auto-sized parent. */
    RECT(parent, 0x80)[2] = RECT(parent, 0xd0)[2] = 0;
    cfw_lyrics_view_measure(view); assert(RECT(view, 0x80)[2] == 240 && !stock_measures);
    RECT(parent, 0x80)[2] = RECT(parent, 0xd0)[2] = 124;
    cfw_lyrics_view_measure(view); assert(RECT(view, 0x80)[2] == 240 && !stock_measures);
    enabled = 0; draw(); assert(!visual.owner && stock_draws == 1); /* static fallback */
    assert(cfw_lyrics_view_keep_awake()); /* ordinary untimed lyrics also stay awake */
    enabled = 1; BYTE(view, 0xa0) = 1; draw(); assert(!visual.owner && stock_draws == 2);
    assert(!cfw_lyrics_view_keep_awake());
    BYTE(view, 0xa0) = 0; draw(); BYTE(view, 0xa0) = 1; tick(); assert(!visual.owner);
    BYTE(view, 0xa0) = 0;
    cfw_lyrics_view_draw(unrelated, (void *)7, (void *)9); assert(stock_draws == 3);
    uint32_t other_event[] = {0, 19, 0, 0};
    assert(cfw_lyrics_view_event(view, other_event) == 23 && stock_events == 1);
    other_event[1] = 15;
    assert(cfw_lyrics_view_event(view, other_event) == 23); /* another timer */
    assert(cfw_lyrics_view_event(unrelated, other_event) == 23);
    cfw_lyrics_view_measure(unrelated); assert(stock_measures == 1);
    cfw_lyrics_view_destroy(unrelated); assert(destroyed == 2);
    assert(frees == allocs - 1); /* one allocation deliberately failed */
    puts("Native lyric view: scoped draw, cache, timer, lifecycle, fallback and OOM passed");
}
