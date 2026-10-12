/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <string.h>
#define _Static_assert(...)
#include "../../../payload/osos.h"
#undef _Static_assert
#define CFW_PATCH_H
#define PATCH_ARM
/* Native slots are four bytes apart; uintptr_t makes mock function pointers
 * usable by the 64-bit host while preserving the native slot indices. */
struct node { uintptr_t *vtable; struct node *parent; };
static uintptr_t screen_methods[16], child_methods[16];
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
#define OSOS_UI_LAYOUT_STATE layout
#define OSOS_CONTROLLER_MANAGER get_manager
#define OSOS_CONTROLLER_CURRENT get_current
#define OSOS_NOW_PLAYING_VTABLE ((uintptr_t)screen_methods)
#include "../../../payload/lyrics_input.c"

static void forwarded(void *recipient, const uint32_t *e) {
    int before = actions, calls = fallbacks;
    assert(cfw_lyrics_focused_event(recipient, e) == fallback_result);
    assert(actions == before && fallbacks == calls + 1);
    assert(last_recipient == recipient && last_event == e);
}

int main(void)
{
    screen_methods[0x1c / 4] = child_methods[0x1c / 4] = (uintptr_t)get_parent;
    screen_methods[0x30 / 4] = child_methods[0x30 / 4] = (uintptr_t)fallback;
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
}
