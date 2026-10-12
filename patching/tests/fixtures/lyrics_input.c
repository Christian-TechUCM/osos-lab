/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <string.h>
#define _Static_assert(...)
#include "../../../payload/osos.h"
#undef _Static_assert
#define CFW_PATCH_H
#define PATCH_ARM
static uint32_t state[16];
static int controller, event, names, destroys, actions, fallbacks;
static const char *event_name = "wheel";
static void *layout(void) { return state; }
static void get_name(struct osos_string *s, void *c, void *e) {
    assert(c == &controller && e == &event); names++; s->data = (char *)event_name;
}
static void destroy(struct osos_string *s) { assert(s->data == event_name); destroys++; }
static int fallback(void *c, void *e) { assert(c == &controller && (e == &event || !e)); fallbacks++; return 23; }
int cfw_lyrics_action(void *c, const char *a, uint32_t argument) {
    assert(c == &controller && !strcmp(a, "CFW_LyricsVolume"));
    assert(argument == (uint32_t)(uintptr_t)&event); actions++; return 1;
}
#undef OSOS_CONTROLLER_INPUT_EVENT
#undef OSOS_CONTROLLER_EVENT_NAME
#undef OSOS_UI_LAYOUT_STATE
#define OSOS_CONTROLLER_INPUT_EVENT fallback
#define OSOS_CONTROLLER_EVENT_NAME get_name
#define OSOS_UI_LAYOUT_STATE layout
#define osos_string_destroy destroy
#include "../../../payload/lyrics_input.c"
int main(void) {
    assert(cfw_lyrics_controller_input(&controller, &event) == 23 && !names);
    state[10] = 0x0dad0974;
    assert(cfw_lyrics_controller_input(&controller, &event) == 1 && actions == 1 && ! (names - destroys));
    event_name = "playPause";
    assert(cfw_lyrics_controller_input(&controller, &event) == 23 && actions == 1);
    assert(cfw_lyrics_controller_input(&controller, NULL) == 23 && names == 2);
    state[10] = 0x0dad0975; event_name = "wheel";
    assert(cfw_lyrics_controller_input(&controller, &event) == 23 && names == 2);
    assert(fallbacks == 4 && destroys == 2);
}
