/* SPDX-License-Identifier: GPL-3.0-only */
#include "native/bindings.h"
.syntax unified
.cpu arm926ej-s
.arm
.text
.balign 4
.global cfw_lyrics_view_destroy_original
.type cfw_lyrics_view_destroy_original,%function
cfw_lyrics_view_destroy_original:
    push {r4, r5, r6, lr}
    mov r4, r0
    ldr pc, =OSOS_TEXT_VIEW_DESTROY + 8
.ltorg
