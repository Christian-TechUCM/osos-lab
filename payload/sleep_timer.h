/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "osos.h"

void cfw_sleep_timer_menu(struct osos_context_menu *menu);
int cfw_sleep_timer_action(void *controller, const char *action, uint32_t argument);
