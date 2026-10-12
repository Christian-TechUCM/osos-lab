/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef CFW_LYRICS_H
#define CFW_LYRICS_H
#include "osos.h"
#include "lyrics_lrc.h"
struct cfw_lyrics_snapshot {
    const struct cfw_lrc *lrc;
    uint32_t position, generation;
    int delay_ms;
};
/* UI-thread-only borrowed snapshot. Never keep it across native event calls. */
int cfw_lyrics_snapshot(struct cfw_lyrics_snapshot *out);
void *cfw_lyrics_artwork(void);
uint32_t cfw_lyrics_generation(void);
int cfw_lyrics_view_keep_awake(void);
void cfw_lyrics_menu(struct osos_context_menu *menu);
int cfw_lyrics_action(void *controller, const char *action, uint32_t argument);
#endif
