/* SPDX-License-Identifier: GPL-3.0-only */

#include "dark_mode.h"
#include "dark_mode_ui.h"
#include "osos.h"
#include "cfw.h"
#include "preferences.h"
#include "patch.h"
#include "theme.h"
#include "themes.h"

enum {
    DARK_MODE_OFF_RECORD = 0x304b5244, /* DRK0 */
    DARK_MODE_ON_RECORD = 0x314b5244,  /* DRK1 */
    BACKGROUND_LIGHT_MIN = 0xb0,
    TEXT_DARK_MAX = 0x80,
};

static unsigned int enabled;
static const struct osos_path settings_path = {0, "iPod_Control\\Device\\CFWDark"};

int cfw_dark_mode_enabled(void)
{
    return enabled != 0;
}

void cfw_dark_mode_load(void)
{
    uint32_t record;
    enabled = cfw_preferences_read(&settings_path, &record, sizeof(record)) &&
              record == DARK_MODE_ON_RECORD;
}

static int save(unsigned int value)
{
    uint32_t record = value ? DARK_MODE_ON_RECORD : DARK_MODE_OFF_RECORD;
    return cfw_preferences_write(&settings_path, &record, sizeof(record));
}

int cfw_dark_mode_property(uint32_t type, uint32_t property, uintptr_t *out)
{
    if (type != OSOS_PROPERTY_STRING || property != CFW_DarkMode_Value)
        return 0;
    *out = osos_settings_on_off(enabled);
    return 1;
}

int cfw_dark_mode_action(void *controller, const char *action, uint32_t argument)
{
    if (!cfw_string_equal(action, "CFW_DarkMode_Toggle"))
        return 0;
    if (argument == OSOS_ACTION_SUPPORT_QUERY)
        return 1;

    if (save(!enabled)) {
        enabled = !enabled;
        osos_model_notify(osos_settings_model(controller), OSOS_NOTIFY_ALL,
                          CFW_DarkMode_Value);
        void *now_playing = osos_now_playing_model();
        if (now_playing) {
            /* Match Apple's playback-state notifications to reload cached icons. */
            osos_model_notify(now_playing, OSOS_RESOURCE_DRAW, 0x7f0c);
            osos_model_notify(now_playing, OSOS_RESOURCE_DRAW, 0x7f0d);
        }
        osos_ui_invalidate();
    }
    return 1;
}

/*
 * The Now Playing idle clock (shown while playing on battery) draws its digits and
 * icons as light bitmaps, so it keeps its light background. COLR data is one
 * 0x00RRGGBB word; the fill only darkens neutral grays, so a one-step tint is
 * enough to leave this color alone.
 */
const void *cfw_dark_mode_color(uint32_t id, const void *color)
{
    static uint32_t idle_background;
    static uint32_t light;
    if (!enabled || !color)
        return color;
    if (!idle_background) {
        struct osos_string name;
        osos_string_init(&name, "NowPlaying_Idle_Background");
        idle_background = *osos_resource_name_slot(&name);
        osos_string_destroy(&name);
    }
    uint32_t rgb = *(const uint32_t *)color;
    uint32_t blue = rgb & 0xff;
    if (id != idle_background || ((rgb >> 16) & 0xff) != blue ||
        ((rgb >> 8) & 0xff) != blue || blue < BACKGROUND_LIGHT_MIN)
        return color;
    light = rgb - 1;
    return &light;
}

PATCH_ARM void cfw_dark_mode_fill(void *surface, const void *rect,
                                const unsigned char *color, uint32_t mode,
                                void *clip)
{
    unsigned char mapped[4];
    struct cfw_palette palette = cfw_theme_palette();
    int themed = enabled || cfw_theme_custom_light();
    uint32_t rgb = palette.background;
    int shade = -1;
    if (themed && color[0] == color[1] && color[1] == color[2] &&
        color[0] >= BACKGROUND_LIGHT_MIN)
        shade = enabled ? (255 - color[0]) / 4 : 0;
    /* CoverFlow_Backside_Border_Color (COLR 0x0dad05d8). */
    else if (themed && color[0] == 0x7e && color[1] == 0x81 && color[2] == 0x83) {
        rgb = palette.border;
        shade = 0;
    } else if (themed && color[0] == 35 && color[1] == 108 && color[2] == 196) {
        rgb = palette.selection;
        shade = 0;
    }
    if (shade >= 0) {
        mapped[0] = CFW_THEME_RED(rgb) + shade;
        mapped[1] = CFW_THEME_GREEN(rgb) + shade;
        mapped[2] = CFW_THEME_BLUE(rgb) + shade;
        mapped[3] = color[3];
        color = mapped;
    }
    osos_surface_fill_rect(surface, rect, color, mode, clip);
}

PATCH_ARM void cfw_dark_mode_clear_color(int red, int green, int blue, int alpha)
{
    if (enabled || cfw_theme_custom_light()) {
        unsigned rgb = cfw_theme_palette().background;
        red = CFW_THEME_RED(rgb) * 0x10000 / 255;
        green = CFW_THEME_GREEN(rgb) * 0x10000 / 255;
        blue = CFW_THEME_BLUE(rgb) * 0x10000 / 255;
    }
    osos_gles_clear_color(red, green, blue, alpha);
}

PATCH_ARM uint32_t cfw_dark_mode_reflection_blend(uint32_t pixel, uint32_t background,
                                                int weight)
{
    if (enabled || cfw_theme_custom_light())
        background = CFW_THEME_RGB565(cfw_theme_palette().background);
    return osos_rgb565_blend(pixel, background, weight);
}

PATCH_ARM void cfw_dark_mode_text_color(void *context, const unsigned char *color)
{
    unsigned char mapped[4];
    struct cfw_palette palette = cfw_theme_palette();
    if (enabled && color[0] == color[1] && color[1] == color[2] &&
        color[0] <= TEXT_DARK_MAX) {
        mapped[0] = CFW_THEME_RED(palette.ink) - color[0] / 2;
        mapped[1] = CFW_THEME_GREEN(palette.ink) - color[0] / 2;
        mapped[2] = CFW_THEME_BLUE(palette.ink) - color[0] / 2;
        mapped[3] = color[3];
        color = mapped;
    } else if (cfw_theme_custom_light() && color[0] == color[1] && color[1] == color[2] &&
               color[0] <= TEXT_DARK_MAX) {
        mapped[0] = CFW_THEME_RED(palette.ink) + color[0] / 2;
        mapped[1] = CFW_THEME_GREEN(palette.ink) + color[0] / 2;
        mapped[2] = CFW_THEME_BLUE(palette.ink) + color[0] / 2;
        mapped[3] = color[3];
        color = mapped;
    } else if ((enabled || cfw_theme_custom_light()) && color[0] == 35 && color[1] == 108 && color[2] == 196) {
        /* Bright accents are selection/icon colors; light-mode text uses its
         * darker selection counterpart for readable contrast. */
        unsigned rgb = enabled ? palette.accent : palette.selection;
        mapped[0] = CFW_THEME_RED(rgb);
        mapped[1] = CFW_THEME_GREEN(rgb);
        mapped[2] = CFW_THEME_BLUE(rgb);
        mapped[3] = color[3];
        color = mapped;
    }
    osos_graphics_set_color(context, color);
}
