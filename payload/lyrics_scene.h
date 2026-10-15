/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef CFW_LYRICS_SCENE_H
#define CFW_LYRICS_SCENE_H
#include "lyrics_lrc.h"

#define CFW_LYRICS_ROWS 5
#define CFW_LYRICS_SAMPLES_X 160
#define CFW_LYRICS_SAMPLES_Y 120
#define CFW_LYRICS_SAMPLES (CFW_LYRICS_SAMPLES_X * CFW_LYRICS_SAMPLES_Y)
struct cfw_lyrics_row {
    int cue, height, top;
    unsigned brightness;
    char text[CFW_LRC_MAX_LINE + 1];
};
struct cfw_lyrics_scene {
    int cue, width, height;
    uint32_t generation;
    unsigned rows, active, valid;
    struct cfw_lyrics_row row[CFW_LYRICS_ROWS];
};
typedef int (*cfw_lyrics_measure)(void *, const char *, int);
/* Zero-initialize scene before first use. No native calls/allocations.
 * A failed layout leaves valid zero; clearing valid also resets measurements. */
int cfw_lyrics_scene_layout(struct cfw_lyrics_scene *, const struct cfw_lrc *,
                           int cue, int width, int height, uint32_t generation,
                           cfw_lyrics_measure, void *);
/* Returns the next polling interval. Coordinates are driven by the song clock. */
unsigned cfw_lyrics_scene_step(struct cfw_lyrics_scene *, const struct cfw_lrc *,
                               uint32_t position, int delay_ms);
/* RGB samples in spatial order, blurred with clamped edges and darkened. */
void cfw_lyrics_blur(uint8_t rgb[CFW_LYRICS_SAMPLES][3]);
void cfw_lyrics_blur_palette(uint8_t rgb[CFW_LYRICS_SAMPLES][3], unsigned background, int light);
void cfw_lyrics_blur_art(uint8_t rgb[CFW_LYRICS_SAMPLES][3], unsigned background,
                         int light, unsigned level);
/* A native little-endian RGB565 BMap, bounded to 320 x 240. */
int cfw_lyrics_background(void *out, size_t capacity, unsigned width, unsigned height,
                          const uint8_t rgb[CFW_LYRICS_SAMPLES][3]);
#endif
