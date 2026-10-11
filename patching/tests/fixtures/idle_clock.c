/* SPDX-License-Identifier: GPL-3.0-only */
/* Exercise PR #17's state transitions with native timer/backlight calls mocked. */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "../../../payload/native/bindings.h"
#define CFW_PATCH_H
#define PATCH_ARM

static uint64_t service_storage[32];
static int lamp, clock_object, task, level_calls, enable_calls, last_level, last_on;
static int timer_inits, timer_starts, timer_stops, stock_events, create_fails;
static void *timer_object;

static uint8_t *service_get(void) { return (uint8_t *)service_storage; }
static void timer_init(void *timer, int zero, void *owner, void *owner_task)
{
    assert(!zero && owner == service_storage && owner_task == &task);
    timer_object = timer;
    timer_inits++;
}
static void timer_stop(void *timer) { assert(timer == timer_object); timer_stops++; }
static void timer_interval(void *timer, uint32_t ms)
{
    assert(timer == timer_object && ms == 350);
}
static void timer_start(void *timer) { assert(timer == timer_object); timer_starts++; }
static int lamp_level(void *value, int level)
{
    assert(value == &lamp);
    last_level = level;
    level_calls++;
    return 3;
}
static int lamp_enable(void *value, int on)
{
    assert(value == &lamp);
    last_on = on;
    enable_calls++;
    return 4;
}
static int service_event(void *service, void *event)
{
    assert(service == service_storage && event);
    stock_events++;
    return 7;
}
static void *clock_create(void *a, void *b, void *c, void *d)
{
    assert(!a && !b && !c && !d);
    return create_fails ? NULL : &clock_object;
}
static void *clock_destroy(void *clock) { assert(clock == &clock_object); return clock; }
static void clock_delete(void *clock) { (void)clock_destroy(clock); }

#undef OSOS_SLEEP_SERVICE
#undef OSOS_TIMER_INIT
#undef OSOS_TIMER_STOP
#undef OSOS_TIMER_SET_INTERVAL
#undef OSOS_TIMER_START
#undef OSOS_BACKLIGHT_LEVEL
#undef OSOS_BACKLIGHT_ENABLE
#undef OSOS_SLEEP_SERVICE_EVENT
#undef OSOS_IDLE_CLOCK_CREATE
#undef OSOS_IDLE_CLOCK_DESTROY
#undef OSOS_IDLE_CLOCK_DELETE
#define OSOS_SLEEP_SERVICE service_get
#define OSOS_TIMER_INIT timer_init
#define OSOS_TIMER_STOP timer_stop
#define OSOS_TIMER_SET_INTERVAL timer_interval
#define OSOS_TIMER_START timer_start
#define OSOS_BACKLIGHT_LEVEL lamp_level
#define OSOS_BACKLIGHT_ENABLE lamp_enable
#define OSOS_SLEEP_SERVICE_EVENT service_event
#define OSOS_IDLE_CLOCK_CREATE clock_create
#define OSOS_IDLE_CLOCK_DESTROY clock_destroy
#define OSOS_IDLE_CLOCK_DELETE clock_delete
#include "../../../payload/idle_clock.c"

static int event(uint32_t kind, void *timer)
{
    uint64_t storage[2] = {0};
    memcpy((uint8_t *)storage + 4, &kind, sizeof(kind));
    memcpy((uint8_t *)storage + 8, &timer, sizeof(timer));
    return cfw_sleep_service_event(service_storage, (uint8_t *)storage);
}

int main(void)
{
    void *task_pointer = &task;
    memcpy(service_get() + OSOS_SLEEP_SERVICE_TIMER + 0x28,
           &task_pointer, sizeof(task_pointer));
    assert(cfw_backlight_level(&lamp, 5) == 3 && level_calls == 1);
    assert(cfw_backlight_enable(&lamp, 1) == 4 && enable_calls == 1);
    create_fails = 1;
    assert(!cfw_idle_clock_create(NULL, NULL, NULL, NULL) && !idle_clocks);
    create_fails = 0;
    assert(cfw_idle_clock_create(NULL, NULL, NULL, NULL) == &clock_object);
    cfw_backlight_level(&lamp, 7);
    cfw_backlight_enable(&lamp, 1);
    assert(level_calls == 1 && enable_calls == 1 && timer_starts == 1);
    assert(timer_inits == 1 && timer_stops == 1);
    assert(cfw_idle_clock_destroy(&clock_object) == &clock_object);
    assert(!idle_clocks && waking);
    cfw_backlight_level(&lamp, 9); /* Last requested brightness wins. */
    assert(timer_starts == 1 && level_calls == 1);
    assert(event(TIMER_EVENT, &task) == 7 && stock_events == 1);
    assert(event(99, timer_object) == 7 && stock_events == 2);
    assert(event(TIMER_EVENT, timer_object) == 1);
    assert(!waking && level_calls == 2 && enable_calls == 2);
    assert(last_level == 9 && last_on == 1);
    event(TIMER_EVENT, timer_object); /* Duplicate event is harmless. */
    assert(level_calls == 2 && enable_calls == 2);

    cfw_idle_clock_create(NULL, NULL, NULL, NULL);
    cfw_idle_clock_delete(&clock_object);
    assert(!idle_clocks && waking);
    cfw_backlight_enable(&lamp, 1);
    assert(timer_starts == 2 && timer_inits == 1);
    cfw_backlight_enable(&lamp, 0); /* Turning off cancels a pending wake. */
    event(TIMER_EVENT, timer_object);
    assert(last_on == 0 && enable_calls == 3 && !waking);

    cfw_idle_clock_create(NULL, NULL, NULL, NULL);
    cfw_idle_clock_delete(&clock_object);
    cfw_backlight_enable(&lamp, 1);
    event(TIMER_EVENT, timer_object);
    assert(enable_calls == 4 && level_calls == 2 && last_on == 1);
}
