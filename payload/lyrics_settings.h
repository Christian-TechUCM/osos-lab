/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
void cfw_lyrics_settings_load(void);
int cfw_lyrics_settings_action(const char *action, uint32_t argument);
unsigned cfw_lyrics_font(void);
unsigned cfw_lyrics_color(void);
unsigned cfw_lyrics_album_background(void);
unsigned cfw_lyrics_keep_awake(void);
unsigned cfw_lyrics_blur_level(void);
