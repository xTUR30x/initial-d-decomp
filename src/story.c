// ------------------------------------------------------------------------------
// src/story.c - STORY_SCENE (Estado 21)
// ------------------------------------------------------------------------------
#include "global.h"
#include "states.h"
#include "memory.h"
#include "graphics.h"
#include "audio.h"
#include "fade.h"

#include <stddef.h>

void state_21_story_scene_init(void)
{
    states_set_current(21u);
    hw_gfx_reg_write16(0x04000000u, 0x0040u); // forced blank
    fade_reset();
    hw_memzero_blocks(0x06000000u, 0x1800u);
    
    // Intentar cargar paleta desde literal pool cercano a 0x0844E800
    const void *pal = hw_rom_ptr(0x0844E800u);
    if (pal != NULL) {
        hw_gfx_dma3_copy(pal, 0x05000000u, 128u);
    }
    
    // Configurar BG0
    hw_gfx_reg_write16(0x04000008u, 0x1F00u);
    hw_gfx_reg_write16(0x04000000u, 0x0140u);
    audio_bgm_play(1u);
    fade_in_all();
}

void state_21_story_scene_update(void)
{
    fade_frame_update();
}

#include "story_assets.h"

void story_register_assets_pc(void)
{
#ifndef TARGET_GBA
    hw_rom_register(0x0844E800u, g_story_pal);
#endif
}
