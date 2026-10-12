/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef CFW_LYRICS_H
#define CFW_LYRICS_H
#include "osos.h"
void cfw_lyrics_menu(struct osos_context_menu *menu);
int cfw_lyrics_action(void *controller, const char *action, uint32_t argument);
#endif
