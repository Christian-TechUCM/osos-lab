/* SPDX-License-Identifier: GPL-3.0-only */
#include "lyrics.h"
#include "patch.h"

/* The display power controller is distinct from SleepService. Its +0x6c
 * timer is the configured backlight inactivity timeout; +0x98 is a later
 * state-transition timer and MUST NOT be swallowed. See lyrics-native.md. */
PATCH_ARM int cfw_lyrics_power_event(void *controller, const uint32_t *event)
{
    void *timer = (unsigned char *)controller + 0x6c;
    if (event[1] == 15 && event[2] == (uint32_t)(uintptr_t)timer &&
        *((unsigned char *)controller + 0x68) && cfw_lyrics_view_keep_awake()) {
        /* Keep the current configured interval. No global preferences changed;
         * normal timeout behavior resumes after the reading view disappears. */
        ((void (*)(void *))OSOS_TIMER_START)(timer);
        return 1;
    }
    return ((int (*)(void *, const uint32_t *))OSOS_DISPLAY_POWER_EVENT)(controller, event);
}
