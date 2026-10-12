/* SPDX-License-Identifier: GPL-3.0-only */
#include "lyrics.h"
#include "patch.h"

/* Apple's UI dispatcher runs its global and recipient input filters BEFORE
 * this virtual +0x30 call. A focused child may consume the wheel before the
 * Now Playing resource/input handlers run. Give volume priority only inside
 * the active, ready lyrics screen; never bypass native modal/input filters.
 * Event ownership stays with the dispatcher, including its single destructor. */
static uintptr_t method(void *object, unsigned slot)
{
    return (*(const uintptr_t **)object)[slot / 4];
}

static int belongs_to(void *focused, void *controller)
{
    /* Use the same virtual parent accessor as Apple's dispatch loop. Bound
     * traversal so an unexpected/cyclic hierarchy fails closed. */
    for (unsigned depth = 0; focused && depth < 16; depth++) {
        if (focused == controller) return 1;
        typedef void *(*parent_fn)(void *);
        parent_fn parent = (parent_fn)method(focused, 0x1c);
        if (!parent) return 0;
        void *next = parent(focused);
        if (next == focused) return 0;
        focused = next;
    }
    return 0;
}

PATCH_ARM int cfw_lyrics_focused_event(void *focused, const uint32_t *event)
{
    if (event && event[1] == OSOS_WHEEL_EVENT && cfw_lyrics_view_visible()) {
        const unsigned char *state = ((void *(*)(void))OSOS_UI_LAYOUT_STATE)();
        if (state && *(const uint32_t *)(state + 0x28) == 0x0dad0974) {
            void *manager = ((void *(*)(void))OSOS_CONTROLLER_MANAGER)();
            void *controller = manager
                ? ((void *(*)(void *))OSOS_CONTROLLER_CURRENT)(manager) : 0;
            if (controller && *(uintptr_t *)controller == OSOS_NOW_PLAYING_VTABLE &&
                belongs_to(focused, controller)) {
                /* Original native wheel object: retain decoding, volume
                 * limits and acknowledgement. No synthetic direction/delta. */
                return cfw_lyrics_action(controller, "CFW_LyricsVolume",
                                         (uint32_t)(uintptr_t)event);
            }
        }
    }
    typedef int (*event_fn)(void *, const uint32_t *);
    return ((event_fn)method(focused, 0x30))(focused, event);
}
