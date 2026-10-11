/* SPDX-License-Identifier: GPL-3.0-only */
#include "timeline.h"
#include <assert.h>
#include <string.h>

static const uint8_t valid[] = {
    'L','R','C','S',1,0,0,0, 1,0,0,0, 3,0,0,0,
    0xe8,3,0,0, 0,0,0,0, 3,0,0,0, 'O','n','e'
};

int main(void)
{
    struct lyrics_timeline timeline;
    struct lyrics_line line;
    assert(lyrics_open(&timeline, valid, sizeof(valid)));
    assert(lyrics_current(&timeline, 999) == -1);
    assert(lyrics_current(&timeline, 1000) == 0);
    assert(lyrics_current_adjusted(&timeline, 1000, 100) == -1);
    assert(lyrics_current_adjusted(&timeline, 1100, 100) == 0);
    assert(lyrics_current_adjusted(&timeline, 900, -100) == 0);
    assert(lyrics_current_adjusted(&timeline, 0, 5000) == -1);
    assert(lyrics_current_adjusted(&timeline, UINT32_MAX, INT32_MIN) == 0);
    assert(lyrics_current_adjusted(&timeline, 6000, INT32_MAX) == 0);
    assert(lyrics_line_at(&timeline, 0, &line) && line.length == 3);
    assert(!memcmp(line.text, "One", 3));
    uint8_t unaligned[sizeof(valid) + 1];
    memcpy(unaligned + 1, valid, sizeof(valid));
    assert(lyrics_open(&timeline, unaligned + 1, sizeof(valid)));
    assert(!lyrics_open(&timeline, 0, 0));
    assert(!lyrics_open(0, valid, sizeof(valid)));
    assert(!lyrics_line_at(&timeline, 0, &line));
    for (unsigned length = 0; length < sizeof(valid); length++)
        assert(!lyrics_open(&timeline, valid, length));
    for (unsigned i = 0; i < sizeof(valid); i++) {
        for (unsigned byte = 0; byte < 256; byte++) {
            uint8_t mutation[sizeof(valid)];
            memcpy(mutation, valid, sizeof(valid));
            mutation[i] = byte;
            if (lyrics_open(&timeline, mutation, sizeof(mutation))) {
                for (uint32_t j = 0; j < timeline.count; j++)
                    assert(lyrics_line_at(&timeline, j, &line));
                (void)lyrics_current(&timeline, UINT32_MAX);
            }
        }
    }
}
