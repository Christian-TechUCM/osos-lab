/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <string.h>
#define _Static_assert(...)
#include "../../../payload/osos.h"
#undef _Static_assert
#include "../../../payload/ui.h"
#define CFW_PATCH_H
#define PATCH_COPY(...)
static unsigned char disk[24];
static int missing = 1, short_read, fail_write, dark, writes, redraws, marks, status;
int cfw_dark_mode_enabled(void) { return dark; }
static int file_open(const struct osos_path *p, int mode, void **out) {
    assert(!strcmp(p->text, "iPod_Control\\Device\\CFWThemes"));
    if (mode == 1 && missing) return -1;
    *out = disk; return 0;
}
static int file_read(void *f, void *out, uint32_t n, uint32_t *count) {
    assert(f == disk && n == sizeof(disk)); memcpy(out, disk, n); *count = n - short_read; return 0;
}
static int file_write(void *f, const void *in, uint32_t n, uint32_t *count) {
    assert(f == disk && n == sizeof(disk)); writes++; *count = fail_write ? 0 : n;
    if (!fail_write) { memcpy(disk, in, n); missing = 0; } return 0;
}
static void close_file(void *f) { assert(f == disk); }
static void changed(uint32_t i) { assert(i); marks++; }
static void invalidate(void) { redraws++; }
static void *get_model(void) { return 0; }
void cfw_ui_refresh(const struct cfw_ui_field *f, unsigned n) { assert(n < f->count); }
void cfw_ui_status(const struct cfw_ui_action *a, unsigned n) { assert(n < a->count); status = n; }
#define osos_file_open file_open
#define osos_file_read file_read
#define osos_file_write file_write
#define osos_file_close close_file
#define osos_menu_item_changed changed
#define osos_ui_invalidate invalidate
#define osos_now_playing_model get_model
#include "../../../payload/themes.c"
#include "ui_resources.c"
#define cfw_ui_refresh unused_refresh
#define cfw_ui_status unused_status
#include "../../../payload/ui.c"
int main(void) {
    cfw_themes_load();
    assert(cfw_theme_palette().background == 0xffffff && cfw_theme_key() == 1);
    dark = 1; assert(cfw_theme_palette().background == CFW_THEME_BACKGROUND && cfw_theme_key() == 129);
    assert(cfw_themes_action("CFW_Themes_Set_00_03", OSOS_ACTION_SUPPORT_QUERY));
    assert(!writes && !redraws && !marks);
    assert(cfw_themes_action("CFW_Themes_Set_00_03", 0));
    assert(cfw_theme_palette().accent == 0x8b7cf6 && writes == 1 && redraws == 1 && marks == 2);
    cfw_themes_load(); assert(cfw_theme_palette().background == 0x11121a);
    dark = 0; assert(!cfw_theme_custom_light() && cfw_theme_palette().background == 0xffffff);
    assert(cfw_themes_action("CFW_Themes_Set_01_01", 0));
    assert(cfw_theme_custom_light() && cfw_theme_palette().background == 0xecfeff);
    assert(cfw_themes_action("CFW_Themes_Set_01_02", 0));
    assert(cfw_theme_palette().accent == 0x38bdf8);
    assert(cfw_themes_action("CFW_Themes_Set_02_03", 0));
    assert(cfw_theme_palette().accent == 0xe53950);
    for (unsigned mode = 0; mode < 2; mode++) {
        dark = mode;
        for (unsigned p = 0; p < (mode ? 4u : 3u); p++) {
            choices[mode ? 0 : 1] = p;
            for (unsigned a = 0; a < 7; a++) {
                choices[2] = a;
                struct cfw_palette palette = cfw_theme_palette();
                assert(palette.background <= 0xffffff && palette.ink <= 0xffffff);
                assert(palette.accent <= 0xffffff && palette.selection <= 0xffffff);
            }
        }
    }
    assert(!cfw_themes_action("CFW_Themes_Set_00_04", 0));
    assert(!cfw_themes_action("CFW_Themes_Set_01_03", 0));
    assert(!cfw_themes_action(NULL, 0));
    fail_write = 1; assert(cfw_themes_action("CFW_Themes_Set_00_00", 0) && status == 1);
    fail_write = 0; assert(cfw_themes_action("CFW_Themes_Save", 0) && !status);
    disk[8] ^= 1; cfw_themes_load(); assert(choices[0] == 0 && choices[1] == 0 && choices[2] == 0);
    disk[8] ^= 1; short_read = 1; cfw_themes_load(); assert(!choices[2]); short_read = 0;
    struct theme_record r; memcpy(&r, disk, sizeof(r)); r.choices[1] = 99; r.checksum = checksum(&r);
    memcpy(disk, &r, sizeof(r)); cfw_themes_load(); assert(!choices[1]);
    assert(cfw_themes_action("CFW_Themes_Open", 0));
}
