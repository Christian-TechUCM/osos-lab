/* SPDX-License-Identifier: GPL-3.0-only */
/* Stock-screen shortcut adapted from kayboll's upstream PR #15,
 * commit 0efdcab988d07102c5006ea40e9a5780d8c560ea.
 * Deliberately excludes Night Shift, private timer-state offsets, and the
 * audiobook shuffle-selector hooks. Apple owns timer state and expiration.
 */
#include "sleep_timer.h"
#include "sleep_timer_ui.h"
#include "cfw.h"

static uint32_t resource_id(const char *text)
{
    struct osos_string name;
    osos_string_init(&name, text);
    const uint32_t *slot = osos_resource_name_slot(&name);
    uint32_t id = slot ? *slot : 0;
    osos_string_destroy(&name);
    return id;
}

static void show(void *controller)
{
    if (!controller)
        return;
    uint32_t screen = resource_id("Alarms_Sleep_Timer_Screen");
    uint32_t layout = resource_id("Alarms_Sleep_Timer_Screen_Default");
    if (!screen || !layout)
        return;
    /* Use the same factory and foreground-push contract as Song Info. */
    typedef void *(*registry_fn)(void);
    typedef void *(*create_fn)(void *, uint32_t, uint32_t, uint32_t);
    typedef void (*push_fn)(void *, void *);
    void *registry = ((registry_fn)OSOS_CONTROLLER_REGISTRY)();
    push_fn push = (push_fn)osos_method(controller, 0xd0);
    if (!registry || !push)
        return;
    void *menu = ((create_fn)OSOS_CREATE_SCREEN_CONTROLLER)(registry, screen, layout, 2);
    if (menu)
        push(controller, menu);
}

void cfw_sleep_timer_menu(struct osos_context_menu *menu)
{
    osos_context_menu_append(menu, CFW_SLEEP_TIMER_LABEL, "CFW_SleepTimer", 1);
}

int cfw_sleep_timer_action(void *controller, const char *action, uint32_t argument)
{
    if (!cfw_string_equal(action, "CFW_SleepTimer"))
        return 0;
    if (argument != OSOS_ACTION_SUPPORT_QUERY)
        show(controller);
    return 1;
}
