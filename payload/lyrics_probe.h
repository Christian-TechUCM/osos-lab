/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef CFW_LYRICS_PROBE_H
#define CFW_LYRICS_PROBE_H
#include <stdint.h>
/* Temporary diagnostic branch only. Set to zero for the regular lyric UI. */
#ifndef CFW_LYRICS_INPUT_DIAGNOSTICS
#define CFW_LYRICS_INPUT_DIAGNOSTICS 1
#endif
#if CFW_LYRICS_INPUT_DIAGNOSTICS
#define CFW_LYRICS_PROBE_TEXT_BYTES 384
void cfw_lyrics_probe_text(char out[CFW_LYRICS_PROBE_TEXT_BYTES]);
void cfw_lyrics_probe_view_event(uint32_t type);
#endif
#endif
