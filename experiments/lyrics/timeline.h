/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef CFW_LYRICS_TIMELINE_H
#define CFW_LYRICS_TIMELINE_H
#include <stddef.h>
#include <stdint.h>

/* Portable core only. No firmware addresses, allocations, threads, or I/O. */
enum { LYRICS_MAX_BYTES = 131072, LYRICS_MAX_CUES = 1024, LYRICS_MAX_LINE = 2048 };
struct lyrics_timeline {
    const uint8_t *data; /* Caller owns immutable storage for the view's lifetime. */
    size_t size;
    uint32_t count;
};
struct lyrics_line { uint32_t start_ms; const char *text; uint32_t length; };

/* Invalid input resets out; never render a partially validated file. */
int lyrics_open(struct lyrics_timeline *out, const void *data, size_t size);
int lyrics_line_at(const struct lyrics_timeline *timeline, uint32_t index,
                   struct lyrics_line *line);
/* -1 before the first cue or for an empty view. Seek-safe O(log n) lookup. */
int lyrics_current(const struct lyrics_timeline *timeline, uint32_t position_ms);
/* Positive delay shows lyrics later. Clamp to +/-5000 ms, without wraparound.
 * Runtime display adjustment only: never changes the sidecar or audio clock. */
int lyrics_current_adjusted(const struct lyrics_timeline *timeline,
                            uint32_t position_ms, int32_t delay_ms);
#endif
