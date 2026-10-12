/* SPDX-License-Identifier: GPL-3.0-only */
#include "lyrics_lrc.h"

static int space(unsigned char c) { return c == ' ' || c == '\t' || c == '\r'; }
static int digit(unsigned char c) { return c >= '0' && c <= '9'; }
static unsigned char lower(unsigned char c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

static int valid_utf8(const char *text, size_t size)
{
    const unsigned char *p = (const unsigned char *)text;
    size_t i = 0;
    while (i < size) {
        uint32_t code = p[i++], minimum;
        unsigned following;
        if (code < 0x80) {
            if (code < 32 && code != '\n' && code != '\r' && code != '\t') return 0;
            continue;
        }
        if (code >= 0xc2 && code <= 0xdf) { code &= 31; following = 1; minimum = 0x80; }
        else if (code >= 0xe0 && code <= 0xef) { code &= 15; following = 2; minimum = 0x800; }
        else if (code >= 0xf0 && code <= 0xf4) { code &= 7; following = 3; minimum = 0x10000; }
        else return 0;
        if (following > size - i) return 0;
        while (following--) {
            if ((p[i] & 0xc0) != 0x80) return 0;
            code = code << 6 | (p[i++] & 63);
        }
        if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) return 0;
        /* Do not mistake Unicode line separators for text in a timed line.
         * This bounded reader accepts CR/LF records; other separators use the
         * stock text view rather than assigning an untimed line a guessed cue. */
        if (code == 0x85 || code == 0x2028 || code == 0x2029) return 0;
    }
    return 1;
}

/* Length-delimited: failed parses never consume a partial stamp. */
static size_t stamp(const char *p, size_t n, char open, uint32_t *time)
{
    if (!n || p[0] != open) return 0;
    size_t i = 1, start = i;
    uint32_t minutes = 0, seconds = 0, fraction = 0, scale = 100;
    while (i < n && digit(p[i]) && i - start < 4) minutes = minutes * 10 + p[i++] - '0';
    if (i == start || i == n || p[i++] != ':') return 0;
    start = i;
    while (i < n && digit(p[i]) && i - start < 5) seconds = seconds * 10 + p[i++] - '0';
    if (i - start < 2 || (open == '<' && (i - start != 2 || seconds >= 60))) return 0;
    if (i < n && p[i] == '.') {
        start = ++i;
        while (i < n && digit(p[i]) && i - start < 3) {
            fraction += (p[i++] - '0') * scale;
            scale /= 10;
        }
        if (start == i) return 0;
    }
    if (i == n || p[i++] != (open == '[' ? ']' : '>')) return 0;
    *time = minutes * 60000 + seconds * 1000 + fraction;
    if (*time > CFW_LRC_MAX_TIME) return 0;
    return i;
}

static int key(const char *p, size_t n, const char *name)
{
    size_t i = 1;
    if (n < 3 || p[0] != '[' || p[n - 1] != ']') return 0;
    for (size_t j = 1; j + 1 < n; j++) if (p[j] == ']') return 0;
    for (; *name; name++, i++) if (i >= n || lower(p[i]) != *name) return 0;
    return i < n && p[i] == ':' ? (int)i + 1 : 0;
}

static int metadata(const char *p, size_t n)
{
    static const char *const names[] = {"ar", "al", "ti", "au", "by", "re", "ve", "length", "id"};
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        if (key(p, n, names[i])) return 1;
    return 0;
}

static size_t line(const char *text, size_t size, size_t *position, size_t *start)
{
    size_t a = *position, b = a;
    while (b < size && text[b] != '\n' && text[b] != '\r') b++;
    *position = b < size ? b + 1 : b;
    while (a < b && space(text[a])) a++;
    while (b > a && space(text[b - 1])) b--;
    *start = a;
    return b - a;
}

static int after(struct cfw_lrc_cue a, struct cfw_lrc_cue b)
{
    return a.milliseconds > b.milliseconds ||
           (a.milliseconds == b.milliseconds && a.offset > b.offset);
}

static void sift(struct cfw_lrc_cue *cues, unsigned root, unsigned count)
{
    while (root * 2 + 1 < count) {
        unsigned child = root * 2 + 1;
        if (child + 1 < count && after(cues[child + 1], cues[child])) child++;
        if (!after(cues[child], cues[root])) break;
        struct cfw_lrc_cue tmp = cues[root]; cues[root] = cues[child]; cues[child] = tmp;
        root = child;
    }
}

int cfw_lrc_parse(struct cfw_lrc *out, const char *text, size_t size)
{
    if (!out) return 0;
    out->count = 0; out->text = 0;
    if (!text || !size || size > CFW_LRC_MAX_TEXT || !valid_utf8(text, size)) return 0;
    size_t first = size >= 3 && (unsigned char)text[0] == 0xef &&
                   (unsigned char)text[1] == 0xbb && (unsigned char)text[2] == 0xbf ? 3 : 0;
    int32_t offset = 0;
    int have_offset = 0;
    size_t pos = first, start;
    while (pos < size) {
        size_t n = line(text, size, &pos, &start);
        int begin = key(text + start, n, "offset");
        if (!begin) continue;
        if (have_offset++) return 0;
        size_t i = (size_t)begin;
        int sign = 1;
        if (i < n && (text[start + i] == '+' || text[start + i] == '-'))
            sign = text[start + i++] == '-' ? -1 : 1;
        size_t digits = i;
        uint32_t value = 0;
        while (i + 1 < n && digit(text[start + i])) {
            if (value > CFW_LRC_MAX_TIME / 10) return 0;
            value = value * 10 + text[start + i++] - '0';
        }
        if (i == digits || i != n - 1 || value > CFW_LRC_MAX_TIME) return 0;
        offset = sign * (int32_t)value;
    }
    unsigned count = 0;
    int untimed = 0;
    pos = first;
    while (pos < size) {
        size_t n = line(text, size, &pos, &start);
        const char *p = text + start;
        if (!n || metadata(p, n) || key(p, n, "offset")) continue;
        unsigned group = count;
        size_t consumed = 0, bytes;
        uint32_t time;
        while ((bytes = stamp(p + consumed, n - consumed, '[', &time))) {
            if (count == CFW_LRC_MAX_CUES) return 0;
            int32_t adjusted = (int32_t)time + offset;
            out->cues[count++].milliseconds = adjusted < 0 ? 0 :
                (uint32_t)adjusted > CFW_LRC_MAX_TIME ? CFW_LRC_MAX_TIME : (uint32_t)adjusted;
            consumed += bytes;
            while (consumed < n && space(p[consumed])) consumed++;
        }
        if (group == count) {
            if (n > 1 && p[0] == '[' && digit(p[1])) return 0;
            untimed = 1;
            continue;
        }
        if (n - consumed > CFW_LRC_MAX_LINE) return 0;
        for (unsigned i = group; i < count; i++) {
            out->cues[i].offset = (uint16_t)(start + consumed);
            out->cues[i].length = (uint16_t)(n - consumed);
        }
    }
    if (!count || untimed) return 0;
    for (unsigned i = count / 2; i; i--) sift(out->cues, i - 1, count);
    for (unsigned i = count; i > 1; i--) {
        struct cfw_lrc_cue tmp = out->cues[0]; out->cues[0] = out->cues[i - 1]; out->cues[i - 1] = tmp;
        sift(out->cues, 0, i - 1);
    }
    out->text = text; out->count = count;
    return 1;
}

int cfw_lrc_current(const struct cfw_lrc *lrc, uint32_t milliseconds, int delay_ms)
{
    if (!lrc || !lrc->text || !lrc->count) return -1;
    if (delay_ms > 5000) delay_ms = 5000;
    if (delay_ms < -5000) delay_ms = -5000;
    int64_t adjusted = (int64_t)milliseconds - delay_ms;
    if (adjusted < 0) return -1;
    unsigned low = 0, high = lrc->count;
    while (low < high) {
        unsigned mid = low + (high - low) / 2;
        if (lrc->cues[mid].milliseconds <= (uint64_t)adjusted) low = mid + 1;
        else high = mid;
    }
    if (!low) return -1;
    unsigned i = low - 1;
    while (i && lrc->cues[i - 1].milliseconds == lrc->cues[i].milliseconds) i--;
    return (int)i;
}

int cfw_lrc_copy(const struct cfw_lrc *lrc, unsigned index, char *out, size_t capacity)
{
    if (!out || !capacity) return 0;
    out[0] = 0;
    if (!lrc || !lrc->text || index >= lrc->count) return 0;
    size_t used = 0;
    for (unsigned i = index; i < lrc->count &&
         lrc->cues[i].milliseconds == lrc->cues[index].milliseconds; i++) {
        const struct cfw_lrc_cue *cue = &lrc->cues[i];
        if (i > index && cue->offset == lrc->cues[i - 1].offset) continue;
        if (i > index) {
            if (used + 1 >= capacity) goto fail;
            out[used++] = '\n';
        }
        size_t line_start = used;
        for (size_t j = 0; j < cue->length;) {
            uint32_t ignored;
            size_t skip = stamp(lrc->text + cue->offset + j, cue->length - j, '<', &ignored);
            if (skip) { j += skip; continue; }
            char c = lrc->text[cue->offset + j++];
            if (used == line_start && space(c)) continue;
            if (used + 1 >= capacity) goto fail;
            out[used++] = c;
        }
        while (used > line_start && space(out[used - 1])) used--;
    }
    out[used] = 0;
    return 1;
fail:
    out[0] = 0;
    return 0;
}
