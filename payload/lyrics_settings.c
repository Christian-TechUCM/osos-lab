/* SPDX-License-Identifier: GPL-3.0-only */
#include "lyrics_settings.h"
#include "lyrics_settings_ui.h"
#include "preferences.h"
#include "cfw.h"

static uint32_t values[CFW_LYRICSSETTINGS_FIELDS] = {1, 0, 0, 1, 1};
static unsigned save_status;
static const struct osos_path path = {0, "iPod_Control\\Device\\CFWLyrics"};
struct record { uint32_t magic, version, values[CFW_LYRICSSETTINGS_FIELDS], checksum; };
struct record_v1 { uint32_t magic, version, values[4], checksum; };

static uint32_t record_checksum(const void *r, unsigned size)
{
    const unsigned char *bytes = (const unsigned char *)r;
    uint32_t sum = 2166136261u;
    for (unsigned n = 0; n < size - 4; n++) sum = (sum ^ bytes[n]) * 16777619u;
    return sum;
}
static uint32_t checksum(const struct record *r) { return record_checksum(r, sizeof(*r)); }

void cfw_lyrics_settings_load(void)
{
    struct record r;
    for (unsigned n = 0; n < CFW_LYRICSSETTINGS_FIELDS; n++)
        values[n] = cfw_lyricssettings_fields[n].default_value;
    unsigned fields = CFW_LYRICSSETTINGS_FIELDS;
    if (!cfw_preferences_read(&path, &r, sizeof(r)) || r.magic != 0x3152594c ||
        (r.version != 2 && r.version != 3) || r.checksum != checksum(&r)) {
        /* Preserve all four existing preferences when upgrading. No disk
         * write at boot; persist v3 only on the next explicit settings save. */
        struct record_v1 old;
        if (!cfw_preferences_read(&path, &old, sizeof(old)) || old.magic != 0x3152594c ||
            old.version != 1 || old.checksum != record_checksum(&old, sizeof(old))) return;
        fields = 4;
        for (unsigned n = 0; n < fields; n++) r.values[n] = old.values[n];
    } else if (r.version == 2) {
        /* Low/Medium/High become Subtle/Medium/Heavy; Off is a new choice. */
        if (r.values[CFW_LYRICSSETTINGS_BLUR] >= 3) return;
        r.values[CFW_LYRICSSETTINGS_BLUR]++;
    }
    for (unsigned n = 0; n < fields; n++)
        if (r.values[n] >= cfw_lyricssettings_fields[n].count) return;
    for (unsigned n = 0; n < fields; n++) values[n] = r.values[n];
}

static void save(void)
{
    struct record r = {0};
    r.magic = 0x3152594c; r.version = 3;
    for (unsigned n = 0; n < CFW_LYRICSSETTINGS_FIELDS; n++) r.values[n] = values[n];
    r.checksum = checksum(&r);
    save_status = !cfw_preferences_write(&path, &r, sizeof(r));
}

unsigned cfw_lyrics_font(void) { return values[CFW_LYRICSSETTINGS_FONT]; }
unsigned cfw_lyrics_color(void) { return values[CFW_LYRICSSETTINGS_COLOR]; }
unsigned cfw_lyrics_album_background(void) { return values[CFW_LYRICSSETTINGS_ARTWORK]; }
unsigned cfw_lyrics_keep_awake(void) { return values[CFW_LYRICSSETTINGS_AWAKE]; }
unsigned cfw_lyrics_blur_level(void) { return values[CFW_LYRICSSETTINGS_BLUR]; }

int cfw_lyrics_settings_action(const char *action, uint32_t argument)
{
    unsigned field, choice;
    int open = cfw_string_equal(action, "CFW_LyricsSettings_Open");
    int retry = cfw_string_equal(action, "CFW_LyricsSettings_Save");
    int chosen = cfw_ui_selection("CFW_LyricsSettings_Set_", action,
        cfw_lyricssettings_fields, CFW_LYRICSSETTINGS_FIELDS, &field, &choice);
    if (!open && !retry && !chosen) return 0;
    if (argument == OSOS_ACTION_SUPPORT_QUERY) return 1;
    if (chosen) {
        unsigned old = values[field];
        values[field] = choice;
        save();
        cfw_ui_refresh(&cfw_lyricssettings_fields[field], choice);
        osos_menu_item_changed(cfw_lyricssettings_fields[field].items[old]);
        osos_menu_item_changed(cfw_lyricssettings_fields[field].items[choice]);
        osos_ui_invalidate();
    }
    if (retry) save();
    if (open)
        for (unsigned n = 0; n < CFW_LYRICSSETTINGS_FIELDS; n++)
            cfw_ui_refresh(&cfw_lyricssettings_fields[n], values[n]);
    cfw_ui_status(&cfw_lyricssettings_save, save_status);
    return 1;
}
