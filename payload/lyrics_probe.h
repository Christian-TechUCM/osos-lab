/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef CFW_LYRICS_PROBE_H
#define CFW_LYRICS_PROBE_H
#include <stdint.h>
/* Opt-in diagnostics only; normal firmware has no probe or refresh overhead. */
#ifndef CFW_LYRICS_INPUT_DIAGNOSTICS
#define CFW_LYRICS_INPUT_DIAGNOSTICS 0
#endif
#if CFW_LYRICS_INPUT_DIAGNOSTICS
#define CFW_LYRICS_PROBE_TEXT_BYTES 384
void cfw_lyrics_probe_text(char out[CFW_LYRICS_PROBE_TEXT_BYTES]);
void cfw_lyrics_probe_view_event(uint32_t type);
#endif
#endif
