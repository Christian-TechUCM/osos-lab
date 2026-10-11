/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <string.h>
#define _Static_assert(...)
#include "../../../payload/osos.h"
#undef _Static_assert
#include "sleep_timer_ui.h"

static int controller, registry_object, screen_object, menu_object;
static int initialized, destroyed, created, pushed, appended;
static int no_slot, no_registry, no_push, no_screen;
static uint32_t screen_id = 41, layout_id = 42;

static void string_init(struct osos_string *name, const char *text)
{
    name->data = (char *)text;
    initialized++;
}
static void string_destroy(struct osos_string *name)
{
    name->data = 0;
    destroyed++;
}
static uint32_t *name_slot(const struct osos_string *name)
{
    if (no_slot)
        return 0;
    if (!strcmp(name->data, "Alarms_Sleep_Timer_Screen"))
        return &screen_id;
    assert(!strcmp(name->data, "Alarms_Sleep_Timer_Screen_Default"));
    return &layout_id;
}
static void *mock_registry(void) { return no_registry ? 0 : &registry_object; }
static void *create(void *owner, uint32_t screen, uint32_t layout, uint32_t mode)
{
    assert(owner == &registry_object && screen == 41 && layout == 42 && mode == 2);
    created++;
    return no_screen ? 0 : &screen_object;
}
static void push(void *owner, void *screen)
{
    assert(owner == &controller && screen == &screen_object);
    pushed++;
}
static uintptr_t method(void *owner, unsigned offset)
{
    assert(owner == &controller && offset == 0xd0);
    return no_push ? 0 : (uintptr_t)push;
}
static void append(struct osos_context_menu *menu, uint32_t label,
                   const char *action, uint32_t flags)
{
    assert(menu == (struct osos_context_menu *)&menu_object);
    assert(label == CFW_SLEEP_TIMER_LABEL && flags == 1);
    assert(!strcmp(action, "CFW_SleepTimer"));
    appended++;
}

#define osos_string_init string_init
#define osos_string_destroy string_destroy
#define osos_resource_name_slot name_slot
#define osos_method method
#define osos_context_menu_append append
#undef OSOS_CONTROLLER_REGISTRY
#define OSOS_CONTROLLER_REGISTRY mock_registry
#undef OSOS_CREATE_SCREEN_CONTROLLER
#define OSOS_CREATE_SCREEN_CONTROLLER create
#include "../../../payload/sleep_timer.c"

int main(void)
{
    cfw_sleep_timer_menu((struct osos_context_menu *)&menu_object);
    assert(appended == 1);
    assert(!cfw_sleep_timer_action(&controller, 0, 0));
    assert(!cfw_sleep_timer_action(&controller, "Unrelated", 0));
    assert(cfw_sleep_timer_action(&controller, "CFW_SleepTimer", OSOS_ACTION_SUPPORT_QUERY));
    assert(initialized == 0 && created == 0 && pushed == 0);
    assert(cfw_sleep_timer_action(&controller, "CFW_SleepTimer", 0));
    assert(created == 1 && pushed == 1 && initialized == 2 && destroyed == 2);
    /* Missing native resources/services/factory results must not be pushed. */
    cfw_sleep_timer_action(0, "CFW_SleepTimer", 0);
    no_slot = 1; cfw_sleep_timer_action(&controller, "CFW_SleepTimer", 0); no_slot = 0;
    screen_id = 0; cfw_sleep_timer_action(&controller, "CFW_SleepTimer", 0); screen_id = 41;
    layout_id = 0; cfw_sleep_timer_action(&controller, "CFW_SleepTimer", 0); layout_id = 42;
    no_registry = 1; cfw_sleep_timer_action(&controller, "CFW_SleepTimer", 0); no_registry = 0;
    no_push = 1; cfw_sleep_timer_action(&controller, "CFW_SleepTimer", 0); no_push = 0;
    assert(created == 1 && pushed == 1);
    no_screen = 1; cfw_sleep_timer_action(&controller, "CFW_SleepTimer", 0);
    assert(created == 2 && pushed == 1 && initialized == destroyed);
}
