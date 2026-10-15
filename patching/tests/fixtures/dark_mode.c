/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <string.h>
/* This fixture mocks native calls, not their 32-bit object layouts. */
#define _Static_assert(...)
#include "../../../payload/osos.h"
#include "../../../payload/dark_mode.h"
#include "../../../payload/themes.h"
#include "../../../payload/theme.h"
#undef _Static_assert
#define CFW_PATCH_H
#define PATCH_ARM

static uint32_t disk_record, disk_size = 4;
static int missing = 1, fail_write, writes, closes, redraws, notifications;
static int controller, model, surface;
static int now_playing, now_playing_available = 1, icon_notifications;
static unsigned icon_invalidations;
static unsigned char drawn[4];
static uint32_t query_view[0x60 / 4], other_view[0x60 / 4];
static unsigned query_ink, nested_ink;
void cfw_dark_mode_text_color(void *, const unsigned char *);
static void stock_text_draw(void *view, void *argument, void *clip)
{
    assert(argument == &model && clip == &controller);
    const unsigned char black[4] = {0, 0, 0, 255};
    if (view == query_view) {
        cfw_dark_mode_stock_text_draw(other_view, argument, clip);
        nested_ink = drawn[0];
    }
    cfw_dark_mode_text_color(&surface, black);
    if (view == query_view) query_ink = drawn[0];
}
int cfw_theme_custom_light(void) { return 0; }
struct cfw_palette cfw_theme_palette(void) {
    return (struct cfw_palette){CFW_THEME_BACKGROUND, CFW_THEME_SIDEBAR, CFW_THEME_INK,
                               CFW_THEME_ACCENT, CFW_THEME_SELECTION, CFW_THEME_BORDER};
}
int cfw_themes_action(const char *action, uint32_t argument) { (void)action; (void)argument; return 0; }

static void string_init(struct osos_string *value, const char *text)
{
    (void)value;
    assert(!strcmp(text, "NowPlaying_Idle_Background"));
}

static void string_destroy(struct osos_string *value) { (void)value; }

static uint32_t *resource_name_slot(const struct osos_string *value)
{
    static uint32_t id = 456;
    (void)value;
    return &id;
}

static int file_open(const struct osos_path *path, int mode, void **handle)
{
    assert(!strcmp(path->text, "iPod_Control\\Device\\CFWDark"));
    if (mode == 1 && missing)
        return -1;
    *handle = &disk_record;
    return 0;
}

static int file_read(void *handle, void *out, uint32_t size, uint32_t *count)
{
    assert(handle == &disk_record && size == 4);
    memcpy(out, &disk_record, 4);
    *count = disk_size;
    return 0;
}

static int file_write(void *handle, const void *record, uint32_t size, uint32_t *count)
{
    assert(handle == &disk_record && size == 4);
    writes++;
    *count = fail_write ? 0 : size;
    if (!fail_write) {
        memcpy(&disk_record, record, size);
        missing = 0;
    }
    return 0;
}

static void file_close(void *handle)
{
    assert(handle == &disk_record);
    closes++;
}

static void *settings_model(void *value)
{
    assert(value == &controller);
    return &model;
}

static uintptr_t on_off(int value) { return value ? 2 : 1; }
static void *now_playing_model(void)
{
    return now_playing_available ? &now_playing : NULL;
}

static void invalidate(void)
{
    assert(icon_invalidations == (now_playing_available ? 3u : 0u));
    icon_invalidations = 0;
    redraws++;
}

static void notify(void *value, uint32_t type, uint32_t property)
{
    if (value == &now_playing) {
        assert(type == OSOS_RESOURCE_DRAW);
        assert(property == 0x7f0c || property == 0x7f0d);
        assert(cfw_dark_mode_enabled() == (disk_record == 0x314b5244));
        unsigned bit = 1u << (property - 0x7f0c);
        assert(!(icon_invalidations & bit));
        icon_invalidations |= bit;
        icon_notifications++;
        return;
    }
    assert(value == &model && type == OSOS_NOTIFY_ALL);
    (void)property;
    notifications++;
}

static void fill(void *target, const void *rect, const unsigned char *color,
                 uint32_t mode, void *clip)
{
    assert(target == &surface && rect == &model && mode == 7 && clip == &controller);
    memcpy(drawn, color, 4);
}

static void text_color(void *target, const unsigned char *color)
{
    assert(target == &surface);
    memcpy(drawn, color, 4);
}

static int clear_rgb[3], clear_alpha;
static void clear_color(int red, int green, int blue, int alpha)
{
    clear_rgb[0] = red;
    clear_rgb[1] = green;
    clear_rgb[2] = blue;
    clear_alpha = alpha;
}

static uint32_t blend_background;
static uint32_t blend(uint32_t pixel, uint32_t background, int weight)
{
    assert(pixel == 0xabcd && weight == 0x8000);
    blend_background = background;
    return 0x1234;
}

#define osos_file_open file_open
#define osos_file_read file_read
#define osos_file_write file_write
#define osos_file_close file_close
#define osos_settings_model settings_model
#define osos_settings_on_off on_off
#define osos_model_notify notify
#define osos_now_playing_model now_playing_model
#define osos_ui_invalidate invalidate
#define osos_surface_fill_rect fill
#define osos_graphics_set_color text_color
#define osos_gles_clear_color clear_color
#define osos_rgb565_blend blend
#define osos_string_init string_init
#define osos_string_destroy string_destroy
#define osos_resource_name_slot resource_name_slot
#undef OSOS_TEXT_VIEW_DRAW
#define OSOS_TEXT_VIEW_DRAW stock_text_draw
#include "../../../payload/dark_mode.c"

static int stock_actions, stock_properties, adjustments;
static int settings_action(void *value, const char *action, uint32_t argument)
{
    assert(value == &controller && !strcmp(action, "stock") && argument == 0);
    stock_actions++;
    return 7;
}

static int settings_property(void *value, uint32_t type, uint32_t property, uintptr_t *out)
{
    assert(value == &model);
    (void)type;
    stock_properties++;
    *out = 5;
    return property == 123;
}

int cfw_eq_action(const char *action, uint32_t argument)
{
    (void)argument;
    return !strcmp(action, "CFW_EQ_Open");
}

int cfw_lyrics_settings_action(const char *action, uint32_t argument)
{
    (void)argument;
    return !strcmp(action, "CFW_LyricsSettings_Open");
}

void cfw_album_artists_menu_adjust_property(uint32_t type, uint32_t property, uintptr_t *out)
{
    assert(type == OSOS_PROPERTY_COUNT && property == 123 && *out == 5);
    adjustments++;
    ++*out;
}

#define osos_settings_action settings_action
#define osos_settings_property settings_property
#include "../../../payload/settings.c"

int main(void)
{
    uintptr_t value = 0;
    cfw_dark_mode_load();
    assert(!cfw_dark_mode_enabled() && !closes);
    assert(cfw_settings_property(&model, OSOS_PROPERTY_STRING, CFW_DarkMode_Value, &value));
    assert(value == 1 && !stock_properties);
    assert(cfw_settings_property(&model, OSOS_PROPERTY_COUNT, 123, &value));
    assert(value == 6 && adjustments == 1);
    assert(!cfw_settings_property(&model, OSOS_PROPERTY_COUNT, 124, &value));
    assert(adjustments == 1);
    assert(cfw_settings_action(&controller, "stock", 0) == 7);
    assert(cfw_settings_action(&controller, "CFW_EQ_Open", 0));
    assert(cfw_settings_action(&controller, "CFW_LyricsSettings_Open", 0));
    assert(cfw_settings_action(&controller, "CFW_DarkMode_Toggle", OSOS_ACTION_SUPPORT_QUERY));
    assert(stock_actions == 1 && !writes && !redraws && !icon_notifications);

    const unsigned char fills[][4] = {
        {255, 255, 255, 173}, {0x7e, 0x81, 0x83, 127}, {35, 108, 196, 91},
    };
    const unsigned char texts[][4] = {{0, 0, 0, 123}, {255, 255, 255, 255}};
    for (int cycle = 0; cycle < 3; cycle++) {
        if (cycle)
            assert(cfw_settings_action(&controller, "CFW_DarkMode_Toggle", 0));
        int dark = cycle == 1;
        assert(cfw_dark_mode_enabled() == dark);
        const uint32_t white = 0xffffff, colored = 0xabcdef, gray = 0x808080;
        const uint32_t *idle_color = cfw_dark_mode_color(456, &white);
        assert(*idle_color == (dark ? 0xfffffeu : white));
        assert(white == 0xffffff); /* The shared resource must stay unchanged. */
        assert(cfw_dark_mode_color(999, &white) == &white);
        assert(cfw_dark_mode_color(456, &colored) == &colored);
        assert(cfw_dark_mode_color(456, &gray) == &gray);
        assert(!cfw_dark_mode_color(456, NULL));
        cfw_dark_mode_fill(&surface, &model, (const unsigned char *)idle_color,
                           7, &controller);
        assert(!memcmp(drawn, idle_color, 4));
        for (unsigned i = 0; i < 3; i++) {
            cfw_dark_mode_fill(&surface, &model, fills[i], 7, &controller);
            if (dark) {
                uint32_t rgb = i == 0 ? CFW_THEME_BACKGROUND :
                               i == 1 ? CFW_THEME_BORDER : CFW_THEME_SELECTION;
                assert(drawn[0] == CFW_THEME_RED(rgb));
                assert(drawn[1] == CFW_THEME_GREEN(rgb));
                assert(drawn[2] == CFW_THEME_BLUE(rgb));
                assert(drawn[3] == fills[i][3]);
            } else {
                assert(!memcmp(drawn, fills[i], 4));
            }
        }
        for (unsigned i = 0; i < 2; i++) {
            cfw_dark_mode_text_color(&surface, texts[i]);
            assert(drawn[0] == (dark && !i ? 0xf2 : texts[i][0]));
            assert(drawn[1] == (dark && !i ? 0xf7 : texts[i][1]));
            assert(drawn[2] == (dark && !i ? 0xff : texts[i][2]));
            assert(drawn[3] == texts[i][3]);
        }
        cfw_dark_mode_clear_color(0x10000, 0x10000, 0x10000, 0x4321);
        assert(clear_rgb[0] == (dark ? 0x20 * 0x10000 / 255 : 0x10000));
        assert(clear_rgb[1] == (dark ? 0x30 * 0x10000 / 255 : 0x10000));
        assert(clear_rgb[2] == (dark ? 0x49 * 0x10000 / 255 : 0x10000));
        assert(clear_alpha == 0x4321);
        assert(cfw_dark_mode_reflection_blend(0xabcd, 0xffff, 0x8000) == 0x1234);
        assert(blend_background == (dark ? CFW_THEME_RGB565(CFW_THEME_BACKGROUND) : 0xffffu));
        cfw_dark_mode_text_color(&surface, fills[2]);
        assert(drawn[0] == (dark ? 0x39 : 35));
        assert(drawn[1] == (dark ? 0xd0 : 108));
        assert(drawn[2] == (dark ? 0xff : 196));
        assert(drawn[3] == 91);
        query_view[0x4c / 4] = 0x0dad0ddc;
        cfw_dark_mode_stock_text_draw(query_view, &model, &controller);
        assert(query_ink == 0 && nested_ink == (dark ? 0xf2u : 0u));
        cfw_dark_mode_text_color(&surface, texts[0]);
        assert(drawn[0] == (dark ? 0xf2 : 0)); /* exemption never leaks */
        const unsigned char warning[] = {248, 81, 73, 64};
        cfw_dark_mode_fill(&surface, &model, warning, 7, &controller);
        assert(!memcmp(drawn, warning, 4));
        cfw_dark_mode_load();
        assert(cfw_dark_mode_enabled() == dark);
    }
    assert(writes == 2 && redraws == 2 && notifications == 2);
    assert(icon_notifications == 4);
    fail_write = 1;
    assert(cfw_dark_mode_action(&controller, "CFW_DarkMode_Toggle", 0));
    assert(!cfw_dark_mode_enabled() && redraws == 2 && notifications == 2);
    assert(icon_notifications == 4 && !icon_invalidations);
    assert(closes == 5);
    disk_record = DARK_MODE_ON_RECORD;
    disk_size = 2;
    cfw_dark_mode_load();
    assert(!cfw_dark_mode_enabled());
    disk_size = 4;
    disk_record = 0;
    cfw_dark_mode_load();
    assert(!cfw_dark_mode_enabled());
    fail_write = 0;
    now_playing_available = 0;
    assert(cfw_dark_mode_action(&controller, "CFW_DarkMode_Toggle", 0));
    assert(cfw_dark_mode_enabled() && redraws == 3 && notifications == 3);
    assert(icon_notifications == 4);
}
