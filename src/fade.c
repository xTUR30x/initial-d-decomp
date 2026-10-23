// ==============================================================================
// src/fade.c - Motor de fundidos y cola de paletas (BLD + DMA3)
// ------------------------------------------------------------------------------
// Portado del bloque 0x08028AC4..0x08029030 del cartucho.
//
// Todo lo de este modulo escribe registros reales de E/S y el descriptor de
// IWRAM (0x030044D8), igual que el original, asi que el mismo codigo vale para
// los dos targets: en GBA los registros y la PAL RAM son hardware; en PC son
// los buffers y registros simulados de src/graphics.c y src/memory.c.
//
// La unica diferencia esta en el origen de la copia de paleta: el original
// lee de la ROM mapeada en 0x08000000, y el port PC resuelve esa direccion a
// un array embebido con hw_rom_ptr().
// ==============================================================================

#include "global.h"
#include "fade.h"
#include "graphics.h"
#include "memory.h"

#include <stddef.h>   // NULL

// ------------------------------------------------------------------------------
// Helpers del descriptor
// ------------------------------------------------------------------------------

static u32 fade_desc_read32(u32 off)
{
    return hw_mem_read32(FADE_DESC_ADDR + off);
}

static void fade_desc_write32(u32 off, u32 value)
{
    hw_mem_write32(FADE_DESC_ADDR + off, value);
}

static u8 fade_desc_read8(u32 off)
{
    return hw_mem_read8(FADE_DESC_ADDR + off);
}

static void fade_desc_write8(u32 off, u8 value)
{
    hw_mem_write8(FADE_DESC_ADDR + off, value);
}

// Escribe los campos comunes de arranque de un estado (BLDCNT/BLDY y
// descriptor). Equivale a la secuencia de stores de cada funcion init.
static void fade_begin(u16 bldcnt, u16 bldy, u8 state, u8 frames)
{
    hw_gfx_reg_write16(REG_BLDCNT, bldcnt);
    hw_gfx_reg_write16(REG_BLDY, bldy);
    fade_desc_write8(0x0Fu, (u8)bldy);
    fade_desc_write8(0x0Cu, state);
    fade_desc_write8(0x0Du, frames);
    fade_desc_write8(0x0Eu, frames);
}

// ------------------------------------------------------------------------------
// Cola de paletas
// ------------------------------------------------------------------------------

void fade_palette_dma_request(u32 rom_src)
{
    // 0x08028D60: count = 0x100 halfwords (512 bytes), destino 0x05000000.
    fade_desc_write32(0x00u, 0x100u);
    fade_desc_write32(0x04u, 0u);
    fade_desc_write32(0x08u, rom_src);
}

// ------------------------------------------------------------------------------
// Inicializadores de la maquina de estados
// ------------------------------------------------------------------------------

void fade_reset(void)
{
    // 0x08028D7C: BLDY = 16, BLDCNT = 0x00FF, PALRAM[0] = 0.
    hw_gfx_reg_write16(REG_BLDY, 16u);
    hw_gfx_reg_write16(REG_BLDCNT, 0x00FFu);
    hw_mem_write16(GBA_PALRAM_BASE, 0u);
}

void fade_out_all(void)
{
    // 0x08028DA4: estado 1, 0x00FF, BLDY 16, frames/timer 2.
    fade_begin(0x00FFu, 16u, 1u, 2u);
}

void fade_to_white(void)
{
    // 0x08028DE4: estado 5, 0x00BF, BLDY 16, frames/timer 2.
    fade_begin(0x00BFu, 16u, 5u, 2u);
}

void fade_from_white(void)
{
    // 0x08028E24: estado 6, 0x00BE, BLDY 0, frames/timer 2.
    fade_begin(0x00BEu, 0u, 6u, 2u);
}

void fade_out_subset(void)
{
    // 0x08028EA4: solo si la maquina esta parada (estado 0).
    if (fade_desc_read8(0x0Cu) != 0u) {
        return;
    }
    fade_begin(0x00F4u, 16u, 3u, 2u);
}

void fade_in_all(void)
{
    // 0x08028FA4: estado 2 sin peticion pendiente, 0x00FF, BLDY 0.
    fade_begin(0x00FFu, 0u, 2u, 2u);
}

void fade_in_all_request(void)
{
    // 0x08028EF0 / 0x08028F00: peticion pendiente = 1 y estado 2.
    fade_desc_write32(0x10u, 1u);
    fade_begin(0x00FFu, 0u, 2u, 2u);
}

void fade_in_subset(void)
{
    // 0x08028FE4: solo si la maquina esta parada (estado 0).
    if (fade_desc_read8(0x0Cu) != 0u) {
        return;
    }
    fade_begin(0x00F4u, 0u, 4u, 2u);
}

void fade_request_alpha_bg1_bg2(void)
{
    // El original no tiene funcion de arranque para el estado 8: un llamador
    // escribe 8 en el byte de estado y el VBlank aplica BLDCNT/BLDALPHA.
    fade_desc_write8(0x0Cu, 8u);
}

// ------------------------------------------------------------------------------
// Tick de la maquina de estados
// ------------------------------------------------------------------------------

// Avanza el timer de frames. Devuelve true cuando toca dar un paso de BLDY
// (equivale a `subs r1,r1,#1; strb; movgt pc,lr` + recarga del timer).
static bool fade_tick_due(void)
{
    u8 timer = fade_desc_read8(0x0Eu);
    timer = (u8)(timer - 1u);
    fade_desc_write8(0x0Eu, timer);

    if (timer > 0u) {
        return false;
    }

    fade_desc_write8(0x0Eu, fade_desc_read8(0x0Du));   // timer = frames
    return true;
}

// Estados 1, 3 y 5: bajan BLDY hasta 0 y despues aplican el "reset" de blend.
static void fade_step_darken(void)
{
    if (!fade_tick_due()) {
        return;
    }

    u8 bldy = fade_desc_read8(0x0Fu);
    bldy = (u8)(bldy - 1u);
    fade_desc_write8(0x0Fu, bldy);
    hw_gfx_reg_write16(REG_BLDY, bldy);

    if (bldy != 0u) {
        return;
    }

    // `strh r1,[ip,#12]` con r1 = 0: limpia estado y frames.
    fade_desc_write8(0x0Cu, 0u);
    fade_desc_write8(0x0Du, 0u);

    hw_gfx_reg_write16(REG_BLDCNT, 0x0F40u);
    hw_gfx_reg_write16(REG_BLDALPHA, 0x0808u);
    fade_desc_write32(0x18u, 0u);
}

// Estados 2, 4 y 6: suben BLDY hasta 16 y despues cierran el estado.
//   state 2 -> puede publicar la peticion pendiente en 0x030027B0
//   state 4 -> estado 0
//   state 6 -> estado 7 (reposo; el switch del original ignora 7)
static void fade_step_brighten(u8 state)
{
    if (!fade_tick_due()) {
        return;
    }

    u8 bldy = fade_desc_read8(0x0Fu);
    bldy = (u8)(bldy + 1u);
    fade_desc_write8(0x0Fu, bldy);
    hw_gfx_reg_write16(REG_BLDY, bldy);

    if (bldy < 16u) {
        return;
    }

    fade_desc_write8(0x0Cu, 0u);
    fade_desc_write8(0x0Du, 0u);

    if (state == 2u) {
        const u32 pending = fade_desc_read32(0x10u);
        if (pending == 0u) {
            return;
        }
        fade_desc_write32(0x10u, 0u);
        hw_mem_write32(FADE_PENDING_PUBLISH_ADDR, pending);
    } else if (state == 6u) {
        fade_desc_write8(0x0Cu, 7u);   // `mov r1,#7; strh r1,[ip,#12]`
    }
}

// ------------------------------------------------------------------------------
// Tick por frame (FUN_08028AC4)
// ------------------------------------------------------------------------------

void fade_frame_update(void)
{
    // 1) Cola de paletas: DMA3 desde la ROM al indice de PAL RAM indicado.
    const u32 count = fade_desc_read32(0x00u);
    const u32 words = count >> 1;

    if (words != 0u) {
        const u32 dst = (fade_desc_read32(0x04u) << 1) + GBA_PALRAM_BASE;
        const u32 src = fade_desc_read32(0x08u);
        const void *host_src = hw_rom_ptr(src);

        if (host_src != NULL) {
            hw_gfx_dma3_copy(host_src, dst, words);
        }

        fade_desc_write32(0x00u, 0u);
    }

    // 2) Animacion de paletas de OBJ (FUN_0802E0F0): no portada.
    //    La ROM la ejecuta aqui sobre las listas de 0x030044F8 y la region
    //    0x05000200; el Estado 0 no depende de ella para el fondo 0.

    // 3) Maquina de estados de fundido.
    const u8 state = fade_desc_read8(0x0Cu);

    switch (state) {
        case 1u:
        case 3u:
        case 5u:
            fade_step_darken();
            break;

        case 2u:
        case 4u:
        case 6u:
            fade_step_brighten(state);
            break;

        case 8u:
            // 0x08028D30: estado 0, BLDCNT = 0x0442, BLDALPHA = 0x1010.
            fade_desc_write8(0x0Cu, 0u);
            fade_desc_write8(0x0Du, 0u);
            hw_gfx_reg_write16(REG_BLDCNT, 0x0442u);
            hw_gfx_reg_write16(REG_BLDALPHA, 0x1010u);
            break;

        case 0u:
        case 7u:
        default:
            break;   // parado o marcador de reposo
    }
}
