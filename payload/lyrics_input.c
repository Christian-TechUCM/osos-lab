/* SPDX-License-Identifier: GPL-3.0-only */
#include "lyrics.h"
#include "cfw.h"
#include "patch.h"

/* Ready Extra Info owns a wheel binding, but hardware testing shows that the
 * resource action route alone is insufficient. Intercept its named wheel at
 * the Now Playing input slot; pass the ORIGINAL event to Apple's decoder.
 * Every other layout/event follows the original dispatch unchanged. */
PATCH_ARM int cfw_lyrics_controller_input(void *controller, void *event)
{
    const unsigned char *state = ((void *(*)(void))OSOS_UI_LAYOUT_STATE)();
    if (state && *(const uint32_t *)(state + 0x28) == 0x0dad0974 && event) {
        struct osos_string name;
        ((void (*)(struct osos_string *, void *, void *))OSOS_CONTROLLER_EVENT_NAME)
            (&name, controller, event);
        int wheel = cfw_string_equal(name.data, "wheel");
        osos_string_destroy(&name);
        if (wheel)
            return cfw_lyrics_action(controller, "CFW_LyricsVolume", (uint32_t)(uintptr_t)event);
    }
    return ((int (*)(void *, void *))OSOS_CONTROLLER_INPUT_EVENT)(controller, event);
}
