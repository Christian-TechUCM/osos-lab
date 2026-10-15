/* SPDX-License-Identifier: GPL-3.0-only */
#include "lyrics.h"
#include "patch.h"
#include "lyrics_probe.h"

#if CFW_LYRICS_INPUT_DIAGNOSTICS
/* UI-thread only, no allocation, storage writes, extra native calls, or
 * retained UI/event pointers. Counters count handler visits, not detents:
 * Apple's bubbling can visit several recipients for a single event. */
static struct {
    uint32_t calls, wheel, visible, layout_ok, manager, controller, type, parent;
    uint32_t sent, returned, gate, layout, vtable, view_wheel, view_other;
} probe;
#define PROBE(statement) do { statement; } while (0)

void cfw_lyrics_probe_view_event(uint32_t type)
{
    if (type == OSOS_WHEEL_EVENT) probe.view_wheel++;
    else if (type != 15 && type != 19) probe.view_other = type;
}

/* All values are fixed-width hex (six digits for bounded display counters).
 * The fixed template is <384 bytes even at maximum counter values. */
static char *label(char *out, const char *text)
{
    while (*text) *out++ = *text++;
    return out;
}
static char *hex(char *out, uint32_t value, unsigned digits)
{
    while (digits) *out++ = "0123456789abcdef"[(value >> (--digits * 4)) & 15];
    return out;
}
void cfw_lyrics_probe_text(char out[CFW_LYRICS_PROBE_TEXT_BYTES])
{
    char *p = label(out, "WHEEL PROBE 1 (hex)\nC:");
    p = hex(p, probe.calls, 6); p = label(p, " W:"); p = hex(p, probe.wheel, 6);
    p = label(p, "\nV:"); p = hex(p, probe.visible, 6);
    p = label(p, " L:"); p = hex(p, probe.layout_ok, 6);
    p = label(p, "\nM:"); p = hex(p, probe.manager, 6);
    p = label(p, " N:"); p = hex(p, probe.type, 6);
    p = label(p, "\nP:"); p = hex(p, probe.parent, 6);
    p = label(p, " S:"); p = hex(p, probe.sent, 6);
    p = label(p, "\nR:"); p = hex(p, probe.returned, 6);
    p = label(p, " G:"); p = hex(p, probe.gate, 1);
    p = label(p, " E:"); p = hex(p, probe.view_other, 2);
    p = label(p, "\nLayout:"); p = hex(p, probe.layout, 8);
    p = label(p, "\nVT:"); p = hex(p, probe.vtable, 8);
    p = label(p, " VW:"); p = hex(p, probe.view_wheel, 4);
    *p = 0;
}
#else
#define PROBE(statement) do { } while (0)
#endif

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
    PROBE(probe.calls++);
    if (event && event[1] == OSOS_WHEEL_EVENT) {
        PROBE(probe.wheel++; probe.gate = 1; probe.layout = 0; probe.vtable = 0);
        if (!cfw_lyrics_view_visible()) goto forward;
        PROBE(probe.visible++; probe.gate = 2);
        const unsigned char *state = ((void *(*)(void))OSOS_UI_LAYOUT_STATE)();
        PROBE(if (state) probe.layout = *(const uint32_t *)(state + 0x28));
        if (state && *(const uint32_t *)(state + 0x28) == 0x0dad0974) {
            PROBE(probe.layout_ok++; probe.gate = 3);
            void *manager = ((void *(*)(void))OSOS_CONTROLLER_MANAGER)();
            PROBE(if (manager) { probe.manager++; probe.gate = 4; });
            void *controller = manager
                ? ((void *(*)(void *))OSOS_CONTROLLER_CURRENT)(manager) : 0;
            PROBE(if (controller) { probe.controller++; probe.gate = 5;
                probe.vtable = (uint32_t)*(uintptr_t *)controller; });
            if (controller && *(uintptr_t *)controller == OSOS_NOW_PLAYING_VTABLE) {
                PROBE(probe.type++; probe.gate = 6);
                if (!belongs_to(focused, controller)) goto forward;
                PROBE(probe.parent++; probe.gate = 7; probe.sent++);
                /* Original native wheel object: retain decoding, volume
                 * limits and acknowledgement. No synthetic direction/delta. */
                int result = cfw_lyrics_action(controller, "CFW_LyricsVolume",
                                              (uint32_t)(uintptr_t)event);
                PROBE(probe.returned++; probe.gate = 8);
                return result;
            }
        }
    }
forward:;
    typedef int (*event_fn)(void *, const uint32_t *);
    return ((event_fn)method(focused, 0x30))(focused, event);
}
