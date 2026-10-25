// ------------------------------------------------------------------------------
// src/menu.c - TITLE_MENU (Estado 1)
// Reconstructado a partir del trampolín ARM en 0x080A3EB0 y flujo verificado
// ------------------------------------------------------------------------------

#include "global.h"
#include "states.h"
#include "memory.h"
#include "graphics.h"
#include "audio.h"
#include "fade.h"
#include "intro.h"

#include <stddef.h>

void menu_state_body(void)
{
    fade_frame_update();
}
