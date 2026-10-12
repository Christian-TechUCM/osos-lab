/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "themes.h"
/* Transform only our compiled UI assets, never user album art. */
void cfw_theme_bitmap(unsigned char *out, const unsigned char *source,
                      unsigned size, unsigned role, struct cfw_palette palette);
