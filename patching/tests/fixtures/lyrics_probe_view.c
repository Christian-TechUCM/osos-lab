/* SPDX-License-Identifier: GPL-3.0-only */
#define CFW_LYRICS_INPUT_DIAGNOSTICS 1
#define main regular_view_contracts
#include "lyrics_view.c"
#undef main

int main(void)
{
    struct cfw_lrc lrc;
    const char *tag = "[00:01]First\n[00:03]Second\n[00:05]Third";
    assert(cfw_lrc_parse(&lrc, tag, strlen(tag)));
    snap.lrc = &lrc; snap.generation = 1; snap.position = 1500;
    WORD(view, 0x40) = OSOS_PROPERTY_STRING; WORD(view, 0x44) = OSOS_NOW_PLAYING_LYRICS;
    WORD(view, 0x48) = 0x800; PTR(view, 0x34) = parent;
    RECT(view, 0x80)[0] = 100;
    timer_owner = TIMER(view);
    setting_art = 0;
    draw();
    assert(probe_texts == 1 && interval == 250 && cfw_lyrics_view_visible());
    assert(visual.font == 0 && font_sets == 1 && setting_font == 1);
    tick(); draw(); /* unchanged position still updates the diagnostic */
    assert(probe_texts == 2 && interval == 250 && screen_invalidations == 1);
    assert(font_sets == 1 && setting_font == 1); /* no per-frame font swaps */
    tick(); assert(invalidations == 1); draw(); assert(probe_texts == 3);
    uint32_t wheel[] = {0, 0x15, 0, 0};
    int before = probe_events;
    assert(cfw_lyrics_view_event(view, wheel) == 23);
    assert(probe_events == before + 1 && probe_last_event == 0x15 && stock_events == 1);
    assert(cfw_lyrics_view_event(unrelated, wheel) == 23);
    assert(probe_events == before + 1); /* no unrelated-view sampling */
    cfw_lyrics_view_visibility(view, 0);
    assert(!cfw_lyrics_view_visible() && !visual.owner && !visual.background);
    int started = starts;
    tick(); assert(starts == started); /* no stale timer resurrection */
    puts("Wheel probe: visible refresh, native forwarding and lifetime passed");
}
