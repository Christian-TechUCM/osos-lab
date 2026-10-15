/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <string.h>
#ifndef CFW_LYRICS_INPUT_DIAGNOSTICS
#define CFW_LYRICS_INPUT_DIAGNOSTICS 1
#endif
#define _Static_assert(...)
#include "../../../payload/osos.h"
#undef _Static_assert
#define CFW_PATCH_H
#define PATCH_ARM
/* Native slots are four bytes apart; uintptr_t makes mock function pointers
 * usable by the 64-bit host while preserving the native slot indices. */
struct node { uintptr_t *vtable; struct node *parent; };
static uintptr_t screen_methods[16], derived_methods[16], child_methods[16];
static struct node controller = {screen_methods, 0};
static struct node child = {child_methods, &controller};
static struct node grandchild = {child_methods, &child};
static struct node unrelated = {child_methods, 0};
static uint32_t state[16], event[7] = {0, 0x15};
static int manager, actions, fallbacks, parent_calls, visible_queries;
static int reading = 1, have_state = 1, have_manager = 1;
static void *active = &controller;
static void *last_recipient;
static const uint32_t *last_event;
static int fallback_result = 23;
static void *layout(void) { return have_state ? state : 0; }
static void *get_manager(void) { return have_manager ? &manager : 0; }
static void *get_current(void *m) { assert(m == &manager); return active; }
static void *get_parent(void *n) { parent_calls++; return ((struct node *)n)->parent; }
static int fallback(void *n, const uint32_t *e) {
    last_recipient = n; last_event = e; fallbacks++; return fallback_result;
}
int cfw_lyrics_view_visible(void) { visible_queries++; return reading; }
int cfw_lyrics_action(void *c, const char *a, uint32_t argument) {
    assert(c == &controller && !strcmp(a, "CFW_LyricsVolume"));
    assert(argument == (uint32_t)(uintptr_t)&event); actions++; return 1;
}
#undef OSOS_UI_LAYOUT_STATE
#undef OSOS_CONTROLLER_MANAGER
#undef OSOS_CONTROLLER_CURRENT
#undef OSOS_NOW_PLAYING_VTABLE
#undef OSOS_NOW_PLAYING_DERIVED_VTABLE
#define OSOS_UI_LAYOUT_STATE layout
#define OSOS_CONTROLLER_MANAGER get_manager
#define OSOS_CONTROLLER_CURRENT get_current
#define OSOS_NOW_PLAYING_VTABLE ((uintptr_t)screen_methods)
#define OSOS_NOW_PLAYING_DERIVED_VTABLE ((uintptr_t)derived_methods)
#include "../../../payload/lyrics_input.c"

static void forwarded(void *recipient, const uint32_t *e) {
    int before = actions, calls = fallbacks;
    assert(cfw_lyrics_focused_event(recipient, e) == fallback_result);
    assert(actions == before && fallbacks == calls + 1);
    assert(last_recipient == recipient && last_event == e);
}

static void run_variant(uintptr_t *methods)
{
    controller.vtable = methods;
    child.parent = &controller;
    grandchild.parent = &child;
    active = &controller;
    actions = fallbacks = parent_calls = visible_queries = 0;
    reading = have_state = have_manager = 1;
    fallback_result = 23;
    memset(state, 0, sizeof(state));
    memset(event, 0, sizeof(event));
    event[1] = OSOS_WHEEL_EVENT;
#if CFW_LYRICS_INPUT_DIAGNOSTICS
    memset(&probe, 0, sizeof(probe));
#endif
    methods[0x1c / 4] = child_methods[0x1c / 4] = (uintptr_t)get_parent;
    methods[0x30 / 4] = child_methods[0x30 / 4] = (uintptr_t)fallback;
    forwarded(&grandchild, event); /* not the ready lyrics layout */
    state[10] = 0x0dad0974;
    /* Regression: the focused child reports handled before Now Playing ever
     * sees the event. The old late hook cannot run in this routing scenario. */
    fallback_result = 1;
    assert(fallback(&grandchild, event) == 1 && !actions);
    int calls = fallbacks;
    assert(cfw_lyrics_focused_event(&grandchild, event) == 1);
    assert(actions == 1 && fallbacks == calls && parent_calls == 2);
    assert(cfw_lyrics_focused_event(&controller, event) == 1 && actions == 2);
    /* Both directions preserve the original object (the native decoder owns
     * delta/limit logic). Never read or manufacture a volume delta here. */
    event[3] = (uint32_t)-3;
    assert(cfw_lyrics_focused_event(&child, event) == 1 && actions == 3);
    event[3] = 4;
    assert(cfw_lyrics_focused_event(&child, event) == 1 && actions == 4);
    fallback_result = 23;
    /* Buttons, notifications, timers, and null events never query UI state. */
    int queries = visible_queries;
    const uint32_t other_types[] = {0, 5, 15, 19, 20, 22, 0x7f25, 0x7f29};
    for (unsigned i = 0; i < sizeof(other_types) / sizeof(other_types[0]); i++) {
        event[1] = other_types[i]; forwarded(&grandchild, event);
    }
    forwarded(&grandchild, 0);
    assert(visible_queries == queries);
    event[1] = OSOS_WHEEL_EVENT;
    reading = 0; forwarded(&grandchild, event); reading = 1;
    have_state = 0; forwarded(&grandchild, event); have_state = 1;
    have_manager = 0; forwarded(&grandchild, event); have_manager = 1;
    active = 0; forwarded(&grandchild, event);
    active = &unrelated; forwarded(&grandchild, event); /* another screen/modal */
    active = &controller;
    forwarded(&unrelated, event); /* focus outside Now Playing */
    const uint32_t layouts[] = {0, 0x0dad0975, 0x0dad0976, 0x0dad0977};
    for (unsigned i = 0; i < sizeof(layouts) / sizeof(layouts[0]); i++) {
        state[10] = layouts[i]; forwarded(&grandchild, event);
    }
    state[10] = 0x0dad0974;
    child.parent = &child; forwarded(&grandchild, event);
    child.parent = &grandchild; /* cyclic parent graph is bounded */
    calls = parent_calls; forwarded(&grandchild, event);
    assert(parent_calls - calls == 16);
    child.parent = &controller;
    child_methods[0x1c / 4] = 0; forwarded(&grandchild, event);
    assert(actions == 4);
#if CFW_LYRICS_INPUT_DIAGNOSTICS
    assert(probe.sent == 4 && probe.returned == 4 && probe.parent == 4);
    assert(probe.wheel > probe.visible && probe.visible > probe.layout_ok);
    assert(probe.layout_ok > probe.manager && probe.manager > probe.controller);
    assert(probe.controller > probe.type && probe.type > probe.parent);
    assert(probe.gate == 6); /* missing parent accessor */
    child_methods[0x1c / 4] = (uintptr_t)get_parent;
    assert(cfw_lyrics_focused_event(&child, event) == 1 && probe.gate == 8);
    reading = 0; forwarded(&child, event);
    assert(probe.gate == 1 && !probe.layout && !probe.vtable);
    reading = 1; state[10] = 0x0dad0975; forwarded(&child, event);
    assert(probe.gate == 2 && probe.layout == 0x0dad0975 && !probe.vtable);
    state[10] = 0x0dad0974; have_manager = 0; forwarded(&child, event);
    assert(probe.gate == 3); have_manager = 1;
    active = 0; forwarded(&child, event); assert(probe.gate == 4);
    active = &unrelated; forwarded(&child, event); assert(probe.gate == 5);
    assert(probe.vtable == (uint32_t)(uintptr_t)child_methods);
    cfw_lyrics_probe_view_event(0x15); cfw_lyrics_probe_view_event(22);
    cfw_lyrics_probe_view_event(15); cfw_lyrics_probe_view_event(19);
    assert(probe.view_wheel == 1 && probe.view_other == 22);
    struct { char text[CFW_LYRICS_PROBE_TEXT_BYTES]; char guard[8]; } output;
    memset(&output, 0x5a, sizeof(output));
    cfw_lyrics_probe_text(output.text);
    assert(strstr(output.text, "WHEEL PROBE 1 (hex)\n"));
    assert(strstr(output.text, " G:5 E:16\nLayout:0dad0974"));
    assert(strstr(output.text, " VW:0001"));
    for (unsigned i = 0; i < sizeof(output.guard); i++) assert(output.guard[i] == 0x5a);
    memset(&probe, 0xff, sizeof(probe));
    cfw_lyrics_probe_text(output.text);
    assert(strlen(output.text) < sizeof(output.text));
    assert(strstr(output.text, "C:ffffff W:ffffff"));
    assert(strstr(output.text, "Layout:ffffffff"));
    for (unsigned i = 0; i < sizeof(output.guard); i++) assert(output.guard[i] == 0x5a);
#endif
}

int main(void)
{
    /* Execute the full routing and fail-closed matrix for BOTH known types.
     * The derived case reproduces the device's former G:5 / N:0 failure. */
    run_variant(screen_methods);
    run_variant(derived_methods);
    assert(!now_playing_controller(0));
    assert(!now_playing_controller(&unrelated));
    controller.vtable = derived_methods + 1;
    assert(!now_playing_controller(&controller)); /* no range-based matching */
    return 0;
}
