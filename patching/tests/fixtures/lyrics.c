/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#define _Static_assert(...)
#include "../../../payload/osos.h"
#undef _Static_assert
#define CFW_PATCH_H
#define PATCH_ARM

static int model, other_model, controller, player;
static uint32_t now;
static int available = 1, stock_result = 1, events, notifications, prepares, shows, appends;
static const char *tag;
static int original_property(void *m, uint32_t type, uint32_t property, void **value)
{
    assert(m == &model || m == &other_model);
    (void)type; (void)property;
    if (value) *value = (void *)tag;
    return stock_result;
}
static int original_event(void *m, const void *event)
{
    assert((m == &model || m == &other_model) && event);
    events++;
    return 42;
}
static struct osos_player *player_get(void) { return (struct osos_player *)&player; }
static uint32_t player_position(void *p) { assert(p == &player); return now; }
static void notify(void *m, uint32_t type, uint32_t property)
{
    assert(m == &model && type == OSOS_PROPERTY_STRING && property == OSOS_NOW_PLAYING_LYRICS);
    notifications++;
}
static void *get_model(void) { return &model; }
static int has_info(void *m) { assert(m == &model); return available; }
static void prepare(void *m) { assert(m == &model); prepares++; }
static void show(void *c) { assert(c == &controller); shows++; }
static void append(struct osos_context_menu *menu, uint32_t label, const char *action, uint32_t flags)
{
    assert(menu && label && action && flags == 1);
    appends++;
}
#define osos_player_get player_get
#define osos_model_notify notify
#define osos_context_menu_append append
#undef OSOS_PLAYER_POSITION_MS
#undef OSOS_NOW_PLAYING_PROPERTY
#undef OSOS_NOW_PLAYING_MODEL_EVENT
#undef OSOS_NOW_PLAYING_MODEL_GET
#undef OSOS_NOW_PLAYING_HAS_EXTRA_INFO
#undef OSOS_NOW_PLAYING_PREPARE_EXTRA_INFO
#undef OSOS_NOW_PLAYING_SHOW_EXTRA_INFO
#define OSOS_PLAYER_POSITION_MS player_position
#define OSOS_NOW_PLAYING_PROPERTY original_property
#define OSOS_NOW_PLAYING_MODEL_EVENT original_event
#define OSOS_NOW_PLAYING_MODEL_GET get_model
#define OSOS_NOW_PLAYING_HAS_EXTRA_INFO has_info
#define OSOS_NOW_PLAYING_PREPARE_EXTRA_INFO prepare
#define OSOS_NOW_PLAYING_SHOW_EXTRA_INFO show
#include "../../../payload/lyrics_lrc.c"
#include "../../../payload/lyrics.c"

static const char *read_line(void)
{
    void *value = 0;
    assert(cfw_lyrics_property(&model, OSOS_PROPERTY_STRING, OSOS_NOW_PLAYING_LYRICS, &value) == stock_result);
    return value;
}
static void event(void *m, uint32_t kind)
{
    uint32_t words[] = {0, kind, 0, 0};
    assert(cfw_lyrics_model_event(m, words) == 42);
}
static void action(const char *name) { assert(cfw_lyrics_action(&controller, name, 0)); }

int main(void)
{
    tag = "[00:01.00]First line\n[00:02.50]Second line\n[00:03.00]";
    now = 0; assert(!strcmp(read_line(), "First line\nSecond line\n"));
    now = 1000; assert(!strcmp(read_line(), "First line\nSecond line\n"));
    struct cfw_lyrics_snapshot snapshot;
    assert(cfw_lyrics_snapshot(&snapshot));
    assert(snapshot.position == now && snapshot.lrc == &lyrics.lrc && !snapshot.delay_ms);
    uint32_t first_generation = snapshot.generation;
    event(&model, 0x80008); assert(notifications == 0); /* pause */
    now = 2500; event(&model, 0x80008); assert(notifications == 0);
    assert(!strcmp(read_line(), "First line\nSecond line\n"));
    now = 1000; event(&model, 0x80008); assert(notifications == 0); /* view owns timing */
    assert(!strcmp(read_line(), "First line\nSecond line\n"));
    event(&other_model, 0x80008); event(&model, 123); assert(notifications == 0);
    now = 3000; assert(!strcmp(read_line(), "First line\nSecond line\n"));
    assert(!cfw_lyrics_action(&controller, "Other", 0));
    assert(!cfw_lyrics_action(&controller, 0, 0));
    assert(cfw_lyrics_action(&controller, "CFW_Lyrics", OSOS_ACTION_SUPPORT_QUERY));
    assert(prepares == 0 && shows == 0);
    action("CFW_Lyrics"); assert(prepares == 1 && shows == 1);
    action("CFW_LyricsLater"); assert(lyrics.delay_ms == 100);
    now = 1000; assert(!strcmp(read_line(), "First line\nSecond line\n"));
    now = 1100; assert(!strcmp(read_line(), "First line\nSecond line\n"));
    for (unsigned i = 0; i < 100; i++) action("CFW_LyricsLater");
    assert(lyrics.delay_ms == 5000);
    for (unsigned i = 0; i < 110; i++) action("CFW_LyricsEarlier");
    assert(lyrics.delay_ms == -5000);
    action("CFW_LyricsReset"); assert(lyrics.delay_ms == 0);
    cfw_lyrics_menu((struct osos_context_menu *)&controller); assert(appends == 4);
    event(&model, 0x80009); assert(!lyrics.model && !lyrics.lrc.count && !lyrics.delay_ms);
    assert(!cfw_lyrics_snapshot(&snapshot));
    tag = "Untimed lyrics stay intact"; assert(read_line() == tag);
    tag = "[00:01]Timed\nUntimed"; assert(read_line() == tag);
    tag = "[00:01.1234]Malformed"; assert(read_line() == tag);
    tag = "[00:01]Timed\xe2\x80\xa8Untimed"; assert(read_line() == tag);
    tag = "[00:01]Replacement"; now = 1000; assert(!strcmp(read_line(), "Replacement"));
    assert(cfw_lyrics_snapshot(&snapshot) && snapshot.generation != first_generation);
    /* A changed tag at the same owner must replace the previous cached text. */
    tag = "[00:01]New text!!!"; assert(!strcmp(read_line(), "New text!!!"));
    void *unrelated = 0;
    assert(cfw_lyrics_property(&model, OSOS_PROPERTY_STRING, 0x1234, &unrelated));
    assert(unrelated == tag); /* Other string properties are never transformed. */
    assert(cfw_lyrics_property(&model, 0, OSOS_NOW_PLAYING_LYRICS, &unrelated));
    assert(unrelated == tag);
    action("CFW_LyricsLater");
    event(&model, 0x8000a); assert(!lyrics.model && !lyrics.delay_ms);
    tag = "[00:01]New text!!!"; now = 0; assert(!strcmp(read_line(), "New text!!!"));
    now = 1000;
    tag = 0; assert(read_line() == 0 && !lyrics.lrc.count);
    tag = "[00:01]Again"; assert(!strcmp(read_line(), "Again"));
    stock_result = 0; read_line(); assert(!lyrics.lrc.count); stock_result = 1;
    static char oversized[CFW_LRC_MAX_TEXT + 2];
    memset(oversized, 'x', sizeof(oversized) - 1); tag = oversized;
    assert(read_line() == tag && !lyrics.lrc.count);
    /* Repeated timestamps can expand a small tag beyond the plain-fallback
     * buffer. Preserve the source instead of truncating or overflowing. */
    static char expanded[2048];
    size_t used = 0;
    for (unsigned i = 0; i < 40; i++)
        used += (size_t)snprintf(expanded + used, sizeof(expanded) - used, "[00:%02u]", i);
    memset(expanded + used, 'x', 1000); expanded[used + 1000] = 0;
    tag = expanded; assert(read_line() == tag && !lyrics.lrc.count);
    available = 0; action("CFW_Lyrics"); assert(prepares == 1 && shows == 1);
    cfw_lyrics_menu((struct osos_context_menu *)&controller); assert(appends == 4);
    /* Deterministic malformed byte/length cases exercise the actual parser. */
    struct cfw_lrc lrc;
    const char *valid = "[00:01.2]Hola, coraz\xc3\xb3n\n[00:02]<00:02.00>Next";
    char buffer[256], output[CFW_LRC_MAX_LINE + 1];
    for (unsigned i = 0; i < 4096; i++) {
        size_t size = strlen(valid);
        memcpy(buffer, valid, size);
        buffer[i % size] = (char)(i / size);
        if (cfw_lrc_parse(&lrc, buffer, i % (size + 1))) {
            int cue = cfw_lrc_current(&lrc, UINT32_MAX, -5000);
            assert(cue >= 0);
            cfw_lrc_copy(&lrc, (unsigned)cue, output, sizeof(output));
        } else assert(!lrc.count && !lrc.text);
    }
    puts("Native lyrics: playback, fallback, timing, ownership and malformed inputs passed");
}
