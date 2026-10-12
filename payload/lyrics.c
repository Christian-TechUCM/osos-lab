/* SPDX-License-Identifier: GPL-3.0-only */
#include "lyrics.h"
#include "lyrics_lrc.h"
#include "lyrics_ui.h"
#include "cfw.h"
#include "patch.h"

/* See docs/lyrics-native.md for the pinned 2.0.5 ABI evidence. All state is
 * UI-owned. Copy the tag: neither track objects nor Apple's temporary strings
 * are retained. The separate view adapter owns its UI timer; no file writes or
 * audio callbacks are used. */
static struct {
    void *model;
    struct cfw_lrc lrc;
    char text[CFW_LRC_MAX_TEXT + 1];
    char display[CFW_LRC_MAX_TEXT + 1];
    size_t size;
    int delay_ms;
    uint32_t generation;
} lyrics;

static void reset(void)
{
    lyrics.generation++;
    lyrics.model = 0;
    lyrics.lrc.count = 0;
    lyrics.lrc.text = 0;
    lyrics.size = 0;
    lyrics.delay_ms = 0;
}

static uint32_t position(void)
{
    return ((uint32_t (*)(void *))OSOS_PLAYER_POSITION_MS)(osos_player_get());
}

static int cached(void *model, const char *text, size_t size)
{
    if (model != lyrics.model || size != lyrics.size) return 0;
    for (size_t i = 0; i < size; i++) if (text[i] != lyrics.text[i]) return 0;
    return 1;
}

static int plain_lyrics(void)
{
    size_t used = 0;
    for (unsigned i = 0; i < lyrics.lrc.count;) {
        if (!cfw_lrc_copy(&lyrics.lrc, i, lyrics.display + used,
                          sizeof(lyrics.display) - used)) return 0;
        while (lyrics.display[used]) used++;
        unsigned stamp = lyrics.lrc.cues[i].milliseconds;
        do { i++; } while (i < lyrics.lrc.count && lyrics.lrc.cues[i].milliseconds == stamp);
        if (i < lyrics.lrc.count) {
            if (used + 1 >= sizeof(lyrics.display)) return 0;
            lyrics.display[used++] = '\n';
        }
    }
    lyrics.display[used] = 0;
    return 1;
}

PATCH_ARM int cfw_lyrics_property(void *model, uint32_t type, uint32_t property, void **value)
{
    typedef int (*original_fn)(void *, uint32_t, uint32_t, void **);
    int result = ((original_fn)OSOS_NOW_PLAYING_PROPERTY)(model, type, property, value);
    if (type != OSOS_PROPERTY_STRING || property != OSOS_NOW_PLAYING_LYRICS) return result;
    if (!result || !value || !*value) {
        if (model == lyrics.model) reset();
        return result;
    }
    const char *text = *value;
    size_t size = 0;
    while (size <= CFW_LRC_MAX_TEXT && text[size]) size++;
    if (size > CFW_LRC_MAX_TEXT) { reset(); return result; }
    if (!cached(model, text, size)) {
        reset();
        lyrics.model = model;
        lyrics.size = size;
        for (size_t i = 0; i <= size; i++) lyrics.text[i] = text[i];
        cfw_lrc_parse(&lyrics.lrc, lyrics.text, size);
        /* Keep a complete readable fallback, not just the current cue. The
         * custom view follows the playback snapshot independently. */
        if (lyrics.lrc.count && !plain_lyrics()) lyrics.lrc.count = 0;
    }
    if (!lyrics.lrc.count) return result;
    *value = lyrics.display;
    return result;
}

PATCH_ARM int cfw_lyrics_model_event(void *model, const uint32_t *event)
{
    typedef int (*original_fn)(void *, const void *);
    uint32_t code = event[1];
    /* These are the two stock current-track replacement notifications. */
    if (code == 0x80009 || code == 0x8000a) reset();
    int result = ((original_fn)OSOS_NOW_PLAYING_MODEL_EVENT)(model, event);
    /* The view's scoped timer drives cue changes. Avoid full native text
     * re-layout/property notifications at every lyric boundary. */
    return result;
}

static void *model_get(void)
{
    return ((void *(*)(void))OSOS_NOW_PLAYING_MODEL_GET)();
}

int cfw_lyrics_snapshot(struct cfw_lyrics_snapshot *out)
{
    if (!lyrics.lrc.count || lyrics.model != model_get()) return 0;
    out->lrc = &lyrics.lrc;
    out->position = position();
    out->generation = lyrics.generation;
    out->delay_ms = lyrics.delay_ms;
    return 1;
}

void *cfw_lyrics_artwork(void)
{
    /* The backdrop paints before the text child first loads its lyric tag. */
    void *model = model_get();
    if (!model) return 0;
    void *bitmap = 0;
    typedef int (*fn)(void *, uint32_t, uint32_t, void **);
    if (!((fn)OSOS_NOW_PLAYING_PROPERTY)(model, OSOS_RESOURCE_DRAW,
                                        OSOS_NOW_PLAYING_ARTWORK, &bitmap)) return 0;
    return bitmap;
}

uint32_t cfw_lyrics_generation(void) { return lyrics.generation; }

void cfw_lyrics_menu(struct osos_context_menu *menu)
{
    void *model = model_get();
    if (!model || !((int (*)(void *))OSOS_NOW_PLAYING_HAS_EXTRA_INFO)(model)) return;
    osos_context_menu_append(menu, CFW_LYRICS_LABEL, "CFW_Lyrics", 1);
    if (lyrics.model == model && lyrics.lrc.count) {
        osos_context_menu_append(menu, CFW_LYRICS_EARLIER, "CFW_LyricsEarlier", 1);
        osos_context_menu_append(menu, CFW_LYRICS_LATER, "CFW_LyricsLater", 1);
        osos_context_menu_append(menu, CFW_LYRICS_RESET, "CFW_LyricsReset", 1);
    }
}

int cfw_lyrics_action(void *controller, const char *action, uint32_t argument)
{
    int want_show = cfw_string_equal(action, "CFW_Lyrics");
    int earlier = cfw_string_equal(action, "CFW_LyricsEarlier");
    int later = cfw_string_equal(action, "CFW_LyricsLater");
    int clear = cfw_string_equal(action, "CFW_LyricsReset");
    if (!want_show && !earlier && !later && !clear) return 0;
    if (argument == OSOS_ACTION_SUPPORT_QUERY) return 1;
    void *model = model_get();
    if (!model || !controller) return 1;
    if (want_show) {
        if (((int (*)(void *))OSOS_NOW_PLAYING_HAS_EXTRA_INFO)(model)) {
            ((void (*)(void *))OSOS_NOW_PLAYING_PREPARE_EXTRA_INFO)(model);
            ((void (*)(void *))OSOS_NOW_PLAYING_SHOW_EXTRA_INFO)(controller);
        }
    } else if (lyrics.model == model && lyrics.lrc.count) {
        if (clear) lyrics.delay_ms = 0;
        else if (earlier && lyrics.delay_ms > -5000) lyrics.delay_ms -= 100;
        else if (later && lyrics.delay_ms < 5000) lyrics.delay_ms += 100;
        osos_model_notify(model, OSOS_PROPERTY_STRING, OSOS_NOW_PLAYING_LYRICS);
    }
    return 1;
}
