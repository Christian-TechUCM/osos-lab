/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#define _Static_assert(...)
#include "../../../payload/osos.h"
#undef _Static_assert
#include "../../../payload/ui.h"
#define CFW_PATCH_H
#define PATCH_COPY(...)
#define PATCH_BYTES(...)
#define PATCH_WORD(...)

static unsigned char disk[28];
static int missing = 1, short_read, fail_write, writes, redraws, marks, status;
static int file_open(const struct osos_path *path, int mode, void **out) {
    assert(!strcmp(path->text, "iPod_Control\\Device\\CFWLyrics"));
    if (mode == 1 && missing) return -1;
    *out = disk; return 0;
}
static int file_read(void *file, void *out, uint32_t size, uint32_t *count) {
    assert(file == disk && size == sizeof(disk));
    memcpy(out, disk, size); *count = size - short_read; return 0;
}
static int file_write(void *file, const void *in, uint32_t size, uint32_t *count) {
    assert(file == disk && size == sizeof(disk)); writes++;
    *count = fail_write ? 0 : size;
    if (!fail_write) { memcpy(disk, in, size); missing = 0; }
    return 0;
}
static void file_close(void *file) { assert(file == disk); }
static void changed(uint32_t item) { assert(item); marks++; }
static void invalidate(void) { redraws++; }
void cfw_ui_refresh(const struct cfw_ui_field *field, unsigned value) { assert(value < field->count); }
void cfw_ui_status(const struct cfw_ui_action *action, unsigned value) {
    assert(value < action->count); status = value;
}
#define osos_file_open file_open
#define osos_file_read file_read
#define osos_file_write file_write
#define osos_file_close file_close
#define osos_menu_item_changed changed
#define osos_ui_invalidate invalidate
#include "../../../payload/lyrics_settings.c"
#include "ui_resources.c"
/* Compile the real action parser; unused refresh paths are kept separate. */
#define cfw_ui_refresh native_refresh_unused
#define cfw_ui_status native_status_unused
#include "../../../payload/ui.c"

static void defaults(void) {
    assert(cfw_lyrics_font() == 1 && cfw_lyrics_color() == 0);
    assert(cfw_lyrics_album_background() == 1 && cfw_lyrics_keep_awake() == 1);
}
int main(void) {
    cfw_lyrics_settings_load(); defaults();
    assert(cfw_lyrics_settings_action("CFW_LyricsSettings_Set_00_02", OSOS_ACTION_SUPPORT_QUERY));
    defaults(); assert(!writes && !redraws && !marks);
    assert(!cfw_lyrics_settings_action("CFW_LyricsSettings_Set_00_09", 0));
    assert(!cfw_lyrics_settings_action("CFW_LyricsSettings_Set_04_00", 0));
    assert(!cfw_lyrics_settings_action(NULL, 0));
    assert(cfw_lyrics_settings_action("CFW_LyricsSettings_Set_00_02", 0));
    assert(cfw_lyrics_font() == 2 && writes == 1 && redraws == 1 && marks == 2 && !status);
    cfw_lyrics_settings_load(); assert(cfw_lyrics_font() == 2);
    assert(cfw_lyrics_settings_action("CFW_LyricsSettings_Set_01_03", 0));
    assert(cfw_lyrics_settings_action("CFW_LyricsSettings_Set_02_00", 0));
    assert(cfw_lyrics_settings_action("CFW_LyricsSettings_Set_03_00", 0));
    cfw_lyrics_settings_load();
    assert(cfw_lyrics_color() == 3 && !cfw_lyrics_album_background() && !cfw_lyrics_keep_awake());
    fail_write = 1;
    assert(cfw_lyrics_settings_action("CFW_LyricsSettings_Set_00_00", 0));
    assert(cfw_lyrics_font() == 0 && status == 1);
    fail_write = 0;
    assert(cfw_lyrics_settings_action("CFW_LyricsSettings_Save", 0));
    assert(!status); cfw_lyrics_settings_load(); assert(cfw_lyrics_font() == 0);
    disk[10] ^= 1; cfw_lyrics_settings_load(); defaults(); disk[10] ^= 1;
    short_read = 1; cfw_lyrics_settings_load(); defaults(); short_read = 0;
    struct record corrupt; memcpy(&corrupt, disk, sizeof(corrupt));
    corrupt.values[3] = 99; corrupt.checksum = checksum(&corrupt);
    memcpy(disk, &corrupt, sizeof(corrupt)); cfw_lyrics_settings_load(); defaults();
    assert(cfw_lyrics_settings_action("CFW_LyricsSettings_Open", 0));
    puts("Lyric settings: defaults, selection, persistence, corruption and retry passed");
}
