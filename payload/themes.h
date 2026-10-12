/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
struct cfw_palette { uint32_t background, sidebar, ink, accent, selection, border; };
void cfw_themes_load(void);
int cfw_themes_action(const char *, uint32_t);
struct cfw_palette cfw_theme_palette(void);
unsigned cfw_theme_key(void);
int cfw_theme_custom_light(void);
