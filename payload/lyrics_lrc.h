/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef CFW_LYRICS_LRC_H
#define CFW_LYRICS_LRC_H
#include <stddef.h>
#include <stdint.h>

/* Caller owns the immutable UTF-8 tag. No allocations, file I/O or player calls. */
#define CFW_LRC_MAX_TEXT 32768u
#define CFW_LRC_MAX_CUES 1024u
#define CFW_LRC_MAX_LINE 2048u
#define CFW_LRC_MAX_TIME 86400000u
struct cfw_lrc_cue {
    uint32_t milliseconds;
    uint16_t offset, length;
};
struct cfw_lrc {
    const char *text;
    uint32_t count;
    struct cfw_lrc_cue cues[CFW_LRC_MAX_CUES];
};

/* Failure leaves count zero: callers must preserve the ordinary static view. */
int cfw_lrc_parse(struct cfw_lrc *out, const char *text, size_t size);
/* Index of the FIRST entry at the current timestamp, or -1 before the first cue. */
int cfw_lrc_current(const struct cfw_lrc *lrc, uint32_t milliseconds, int delay_ms);
/* Copies all simultaneous lines, stripping enhanced-LRC word markers. */
int cfw_lrc_copy(const struct cfw_lrc *lrc, unsigned index, char *out, size_t capacity);
#endif
