/* SPDX-License-Identifier: GPL-3.0-only */
#include "themes.h"
#include "theme.h"
#include "themes_ui.h"
#include "dark_mode.h"
#include "preferences.h"
#include "cfw.h"

/* DexThemes surface/sidebar/accent values, with supporting text and selection
 * roles adapted for the iPod. Original navy remains byte-for-byte unchanged. */
static const struct cfw_palette dark_palettes[] = {
    {CFW_THEME_BACKGROUND, CFW_THEME_SIDEBAR, CFW_THEME_INK,
     CFW_THEME_ACCENT, CFW_THEME_SELECTION, CFW_THEME_BORDER},
    {0x111111, 0x0a0a0a, 0xf4f4f5, 0xdc2626, 0x661c1c, 0x414148},
    {0x130c10, 0x0d080b, 0xfff1f4, 0xe51a3c, 0x68172b, 0x4b2836},
    {0x11121a, 0x0a0b13, 0xf1efff, 0x8b7cf6, 0x43386f, 0x38334f},
};
static const struct cfw_palette light_palettes[] = {
    {0xffffff, 0xf2f2f2, 0x202020, 0x236cc4, 0x236cc4, 0x7e8183},
    {0xecfeff, 0xe2f4f5, 0x123c42, 0x14b8a6, 0x0f766e, 0xa2c5c7},
    {0xf2fcff, 0xe8f2f5, 0x173549, 0x38bdf8, 0x036b99, 0xaabfc9},
};
static uint32_t choices[CFW_THEMES_FIELDS];
static unsigned save_status;
static const struct osos_path path = {0, "iPod_Control\\Device\\CFWThemes"};
struct theme_record { uint32_t magic, version, choices[CFW_THEMES_FIELDS], checksum; };
static uint32_t checksum(const struct theme_record *r)
{
    const unsigned char *p = (const unsigned char *)r;
    uint32_t sum = 2166136261u;
    for (unsigned i = 0; i < sizeof(*r) - 4; i++) sum = (sum ^ p[i]) * 16777619u;
    return sum;
}
void cfw_themes_load(void)
{
    struct theme_record r;
    for (unsigned i = 0; i < CFW_THEMES_FIELDS; i++) choices[i] = 0;
    if (!cfw_preferences_read(&path, &r, sizeof(r)) || r.magic != 0x314d4854 ||
        r.version != 1 || r.checksum != checksum(&r)) return;
    for (unsigned i = 0; i < CFW_THEMES_FIELDS; i++)
        if (r.choices[i] >= cfw_themes_fields[i].count) return;
    for (unsigned i = 0; i < CFW_THEMES_FIELDS; i++) choices[i] = r.choices[i];
}
static void save(void)
{
    struct theme_record r = {0};
    r.magic = 0x314d4854; r.version = 1;
    for (unsigned i = 0; i < CFW_THEMES_FIELDS; i++) r.choices[i] = choices[i];
    r.checksum = checksum(&r);
    save_status = !cfw_preferences_write(&path, &r, sizeof(r));
}
struct cfw_palette cfw_theme_palette(void)
{
    int dark = cfw_dark_mode_enabled();
    struct cfw_palette p = dark ? dark_palettes[choices[CFW_THEMES_DARK]] :
                                light_palettes[choices[CFW_THEMES_LIGHT]];
    static const uint32_t accents[] = {0, 0x438eff, 0x39d0ff, 0xe53950, 0xa28aff, 0x14b8a6, 0xe5b94f};
    unsigned a = choices[CFW_THEMES_ACCENT];
    if (a) {
        p.accent = accents[a];
        /* White selection text needs a darker surface than the raw accent. */
        unsigned r = p.accent >> 16, g = (p.accent >> 8) & 255, b = p.accent & 255;
        unsigned scale = dark ? 40 : 55;
        p.selection = (r * scale / 100) << 16 | (g * scale / 100) << 8 | b * scale / 100;
    }
    return p;
}
unsigned cfw_theme_key(void)
{
    return 1 + (cfw_dark_mode_enabled() ? 128 + choices[CFW_THEMES_DARK] : choices[CFW_THEMES_LIGHT]) +
           choices[CFW_THEMES_ACCENT] * 8;
}
int cfw_theme_custom_light(void)
{
    return !cfw_dark_mode_enabled() && (choices[CFW_THEMES_LIGHT] || choices[CFW_THEMES_ACCENT]);
}
int cfw_themes_action(const char *action, uint32_t argument)
{
    unsigned field, choice;
    int open = cfw_string_equal(action, "CFW_Themes_Open");
    int retry = cfw_string_equal(action, "CFW_Themes_Save");
    int chosen = cfw_ui_selection("CFW_Themes_Set_", action, cfw_themes_fields,
                                  CFW_THEMES_FIELDS, &field, &choice);
    if (!open && !retry && !chosen) return 0;
    if (argument == OSOS_ACTION_SUPPORT_QUERY) return 1;
    if (chosen) {
        unsigned old = choices[field];
        choices[field] = choice;
        save();
        cfw_ui_refresh(&cfw_themes_fields[field], choice);
        osos_menu_item_changed(cfw_themes_fields[field].items[old]);
        osos_menu_item_changed(cfw_themes_fields[field].items[choice]);
        void *model = osos_now_playing_model();
        if (model) {
            osos_model_notify(model, OSOS_RESOURCE_DRAW, 0x7f0c);
            osos_model_notify(model, OSOS_RESOURCE_DRAW, 0x7f0d);
        }
        osos_ui_invalidate();
    }
    if (open)
        for (unsigned i = 0; i < CFW_THEMES_FIELDS; i++) cfw_ui_refresh(&cfw_themes_fields[i], choices[i]);
    if (retry) save();
    cfw_ui_status(&cfw_themes_save, save_status);
    return 1;
}
