/* SPDX-License-Identifier: GPL-3.0-only */
#include "../../../payload/lyrics_probe.h"
#if CFW_LYRICS_INPUT_DIAGNOSTICS
#error "Normal firmware must default to diagnostics disabled"
#endif
#include "lyrics_input.c"
