/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "../../../payload/lyrics_lrc.c"
#include "../../../payload/lyrics_scene.c"

static int measure(void *context, const char *text, int width)
{
    (void)context;
    assert(width > 0);
    return text[0] == 'L' ? 260 : 24;
}
static unsigned u16(const unsigned char *p) { return p[0] | p[1] << 8; }
int main(void)
{
    struct cfw_lrc lrc;
    struct cfw_lyrics_scene scene = {0};
    const char *tag = "[00:01]One\n[00:03]Two\n[00:06]Long\n[00:10]Four\n[00:12]Five\n[00:14]Six";
    assert(cfw_lrc_parse(&lrc, tag, strlen(tag)));
    assert(cfw_lyrics_scene_layout(&scene, &lrc, -1, 304, 140, 1, measure, 0));
    assert(cfw_lyrics_scene_step(&scene, &lrc, 0, 0) == 100);
    assert(scene.row[0].brightness == 180 && scene.row[0].top == 88);
    assert(cfw_lyrics_scene_layout(&scene, &lrc, 0, 304, 140, 1, measure, 0));
    cfw_lyrics_scene_step(&scene, &lrc, 1000, 0);
    assert(scene.row[0].top == 88 && scene.row[0].brightness == 180);
    cfw_lyrics_scene_step(&scene, &lrc, 1500, 0);
    assert(scene.row[0].top == 58 && scene.row[0].brightness == 250);
    int incoming = scene.row[1].top;
    assert(cfw_lyrics_scene_layout(&scene, &lrc, 1, 304, 140, 1, measure, 0));
    cfw_lyrics_scene_step(&scene, &lrc, 3000, 0);
    assert(scene.row[1].top == incoming); /* no jump at cue boundary */
    int last = incoming;
    unsigned last_color = 180;
    for (unsigned t = 3000; t <= 3500; t += 10) {
        unsigned interval = cfw_lyrics_scene_step(&scene, &lrc, t, 0);
        assert(interval == (t < 3500 ? 33 : 100));
        assert(scene.row[1].top <= last && scene.row[1].brightness >= last_color);
        last = scene.row[1].top; last_color = scene.row[1].brightness;
        cfw_lyrics_scene_step(&scene, &lrc, t, 0); /* paused clock: identical */
        assert(scene.row[1].top == last && scene.row[1].brightness == last_color);
    }
    assert(last == 58 && last_color == 250);
    incoming = scene.row[2].top;
    assert(cfw_lyrics_scene_layout(&scene, &lrc, 2, 304, 140, 1, measure, 0));
    cfw_lyrics_scene_step(&scene, &lrc, 6000, 0);
    assert(scene.row[2].top == incoming);
    cfw_lyrics_scene_step(&scene, &lrc, 6500, 0);
    assert(scene.row[2].top == 12);
    cfw_lyrics_scene_step(&scene, &lrc, 9500, 0);
    assert(scene.row[2].top == -132); /* all of a tall cue can be read */
    incoming = scene.row[3].top;
    assert(cfw_lyrics_scene_layout(&scene, &lrc, 3, 304, 140, 1, measure, 0));
    cfw_lyrics_scene_step(&scene, &lrc, 10000, 0);
    assert(scene.row[scene.active].top == incoming);
    cfw_lyrics_scene_step(&scene, &lrc, 10400, 0);
    int seek_top = scene.row[scene.active].top;
    cfw_lyrics_scene_step(&scene, &lrc, 15400, 5000);
    assert(scene.row[scene.active].top == seek_top); /* delay uses song time */
    assert(!cfw_lyrics_scene_layout(&scene, &lrc, 999, 304, 140, 1, measure, 0));
    assert(!scene.valid);
    assert(!cfw_lyrics_scene_layout(&scene, &lrc, 0, 321, 140, 1, measure, 0));
    assert(!cfw_lyrics_scene_layout(&scene, &lrc, 0, 304, 241, 1, measure, 0));
    const char *fast = "[00:00]A\n[00:00.1]B\n[00:00.2]C";
    assert(cfw_lrc_parse(&lrc, fast, strlen(fast)));
    assert(cfw_lyrics_scene_layout(&scene, &lrc, 1, 304, 140, 2, measure, 0));
    cfw_lyrics_scene_step(&scene, &lrc, 200, 0);
    assert(scene.row[scene.active].brightness == 250);
    const char *last_long = "[00:00]Long final cue";
    assert(cfw_lrc_parse(&lrc, last_long, strlen(last_long)));
    assert(cfw_lyrics_scene_layout(&scene, &lrc, 0, 304, 140, 3, measure, 0));
    cfw_lyrics_scene_step(&scene, &lrc, 5500, 0);
    assert(scene.row[0].top == -132);

    uint8_t rgb[CFW_LYRICS_SAMPLES][3];
    for (unsigned y = 0; y < CFW_LYRICS_SAMPLES_Y; y++)
        for (unsigned x = 0; x < CFW_LYRICS_SAMPLES_X; x++) {
            unsigned i = y * CFW_LYRICS_SAMPLES_X + x;
            rgb[i][0] = x < 8 ? 255 : 0;
            rgb[i][1] = y < 6 ? 120 : 0;
            rgb[i][2] = x < 8 ? 0 : 255;
        }
    cfw_lyrics_blur(rgb);
    assert(rgb[0][0] > rgb[15][0] && rgb[0][2] < rgb[15][2]);
    assert(rgb[7][0] != rgb[0][0] && rgb[7][2] != rgb[15][2]); /* mixed blur */
    unsigned char pixels[28 + 640 * 240 + 2];
    memset(pixels, 0xa5, sizeof(pixels));
    assert(!cfw_lyrics_background(pixels + 1, 20, 320, 240, rgb));
    assert(pixels[1] == 0xa5);
    assert(cfw_lyrics_background(pixels + 1, sizeof(pixels) - 2, 320, 240, rgb));
    assert(pixels[0] == 0xa5 && pixels[sizeof(pixels) - 1] == 0xa5);
    assert(u16(pixels + 1) == 0x565 && u16(pixels + 5) == 640);
    /* Top row fades into the theme; album colors remain below the fade. */
    assert(u16(pixels + 29) == u16(pixels + 29 + 316 * 2));
    assert(u16(pixels + 29 + 640 * 40) != u16(pixels + 29 + 640 * 40 + 316 * 2));
    assert(cfw_lyrics_background(pixels + 1, sizeof(pixels) - 2, 1, 1, rgb));
    assert(u16(pixels + 5) == 4 && pixels[31] == 0 && pixels[32] == 0);
    for (unsigned w = 1; w <= 320; w += 7)
        for (unsigned h = 1; h <= 240; h += 11)
            assert(cfw_lyrics_background(pixels + 1, sizeof(pixels) - 2, w, h, rgb));
    assert(!cfw_lyrics_background(pixels, sizeof(pixels), 0, 1, rgb));
    assert(!cfw_lyrics_background(pixels, sizeof(pixels), 321, 240, rgb));
    puts("Native lyric scene: easing, pause/seek, timing, long cues, blur and bounds passed");
}
