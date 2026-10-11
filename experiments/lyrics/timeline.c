/* SPDX-License-Identifier: GPL-3.0-only */
#include "timeline.h"

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static int utf8(const uint8_t *p, uint32_t length)
{
    uint32_t i = 0;
    while (i < length) {
        uint32_t code = p[i++], minimum;
        unsigned trailing;
        if (code < 0x80) {
            if (code < 0x20 && code != '\n' && code != '\t')
                return 0;
            continue;
        }
        if (code >= 0xc2 && code <= 0xdf) {
            code &= 0x1f; trailing = 1; minimum = 0x80;
        } else if (code >= 0xe0 && code <= 0xef) {
            code &= 0xf; trailing = 2; minimum = 0x800;
        } else if (code >= 0xf0 && code <= 0xf4) {
            code &= 7; trailing = 3; minimum = 0x10000;
        } else {
            return 0;
        }
        if (trailing > length - i)
            return 0;
        while (trailing--) {
            if ((p[i] & 0xc0) != 0x80)
                return 0;
            code = (code << 6) | (p[i++] & 0x3f);
        }
        if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff))
            return 0;
    }
    return 1;
}

int lyrics_open(struct lyrics_timeline *out, const void *data, size_t size)
{
    if (!out)
        return 0;
    out->data = 0; out->size = 0; out->count = 0;
    if (!data || size < 16 || size > LYRICS_MAX_BYTES)
        return 0;
    const uint8_t *bytes = data;
    if (bytes[0] != 'L' || bytes[1] != 'R' || bytes[2] != 'C' || bytes[3] != 'S' ||
        bytes[4] != 1 || bytes[5] || bytes[6] || bytes[7])
        return 0;
    uint32_t count = le32(bytes + 8), text_size = le32(bytes + 12);
    if (!count || count > LYRICS_MAX_CUES || text_size > LYRICS_MAX_BYTES)
        return 0;
    uint32_t text_start = 16 + count * 12;
    if (text_start > size || text_size != size - text_start)
        return 0;
    uint32_t previous = 0, expected_offset = 0;
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t *record = bytes + 16 + i * 12;
        uint32_t time = le32(record), offset = le32(record + 4), length = le32(record + 8);
        if ((i && time <= previous) || time > 86400000 || offset != expected_offset ||
            offset > text_size || length > text_size - offset || length > LYRICS_MAX_LINE)
            return 0;
        if (!utf8(bytes + text_start + offset, length))
            return 0;
        expected_offset += length;
        previous = time;
    }
    if (expected_offset != text_size)
        return 0;
    out->data = bytes; out->size = size; out->count = count;
    return 1;
}

int lyrics_line_at(const struct lyrics_timeline *timeline, uint32_t index,
                   struct lyrics_line *line)
{
    if (!timeline || !timeline->data || !line || index >= timeline->count)
        return 0;
    const uint8_t *record = timeline->data + 16 + index * 12;
    line->start_ms = le32(record);
    line->text = (const char *)timeline->data + 16 + timeline->count * 12 + le32(record + 4);
    line->length = le32(record + 8);
    return 1;
}

int lyrics_current(const struct lyrics_timeline *timeline, uint32_t position_ms)
{
    if (!timeline || !timeline->data)
        return -1;
    uint32_t low = 0, high = timeline->count;
    while (low < high) {
        uint32_t mid = low + (high - low) / 2;
        if (le32(timeline->data + 16 + mid * 12) <= position_ms)
            low = mid + 1;
        else
            high = mid;
    }
    return (int)low - 1;
}

int lyrics_current_adjusted(const struct lyrics_timeline *timeline,
                            uint32_t position_ms, int32_t delay_ms)
{
    if (delay_ms > 5000)
        delay_ms = 5000;
    if (delay_ms < -5000)
        delay_ms = -5000;
    int64_t adjusted = (int64_t)position_ms - delay_ms;
    if (adjusted < 0)
        return -1;
    return lyrics_current(timeline, adjusted > UINT32_MAX ? UINT32_MAX : (uint32_t)adjusted);
}
