/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#define _Static_assert(...)
#include "../../../payload/lyrics.h"
#undef _Static_assert
#define CFW_PATCH_H
#define PATCH_ARM
static unsigned char controller[0xdc];
static int awake, starts, forwarded;
int cfw_lyrics_view_keep_awake(void) { return awake; }
static void start(void *timer) { assert(timer == controller + 0x6c); starts++; }
static int original(void *p, const uint32_t *e) { assert(p == controller && e); forwarded++; return 7; }
#undef OSOS_DISPLAY_POWER_EVENT
#undef OSOS_TIMER_START
#define OSOS_DISPLAY_POWER_EVENT original
#define OSOS_TIMER_START start
#include "../../../payload/lyrics_power.c"
int main(void) {
    uint32_t event[] = {0, 15, (uint32_t)(uintptr_t)(controller + 0x6c)};
    controller[0x68] = 1;
    assert(cfw_lyrics_power_event(controller, event) == 7); /* default stock */
    awake = 1;
    assert(cfw_lyrics_power_event(controller, event) == 1 && starts == 1 && forwarded == 1);
    controller[0x68] = 0;
    assert(cfw_lyrics_power_event(controller, event) == 7); /* never wake a sleeping display */
    controller[0x68] = 1;
    event[2] = (uint32_t)(uintptr_t)(controller + 0x98);
    assert(cfw_lyrics_power_event(controller, event) == 7); /* later power transition */
    event[1] = 0x50006;
    assert(cfw_lyrics_power_event(controller, event) == 7); /* other power event */
    event[1] = 13;
    assert(cfw_lyrics_power_event(controller, event) == 7); /* user input */
    event[1] = 15; event[2] = 0;
    assert(cfw_lyrics_power_event(controller, event) == 7); /* unrelated timer */
    awake = 0; event[2] = (uint32_t)(uintptr_t)(controller + 0x6c);
    assert(cfw_lyrics_power_event(controller, event) == 7); /* leaving lyrics */
    assert(starts == 1 && forwarded == 7);
    puts("Lyric wake scope: only lit-display inactivity; other power events forwarded");
}
