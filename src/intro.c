// ==============================================================================
// src/intro.c - Estado 0 (intro / logo de arranque)
// ------------------------------------------------------------------------------
// Portado del handler 0x080A52E4 del cartucho. Clasificado como SUBSISTEMA
// mixto: GRAFICOS (VRAM, paletas, sprites) + AUDIO (BGM) + LOGICA DE ESTADO.
//
// Ver include/intro.h para el volcado del flujo original y las direcciones
// ROM de cada asset.
//
// Los assets (paletas, streams y pools de tiles) estan embebidos en
// src/intro_assets.c, generado por tools/extract_intro_assets.py a partir de
// baserom.gba. En el target GBA viven en la ROM del cartucho, por lo que las
// copias por DMA3 siguen siendo transferencias ROM -> PAL RAM / VRAM.
// ==============================================================================

#include "global.h"
#include "intro.h"
#include "intro_assets.h"
#include "graphics.h"
#include "audio.h"
#include "memory.h"
#include "states.h"

#include <stddef.h>   // NULL

// ------------------------------------------------------------------------------
// Resolucion de los pools de tiles
// ------------------------------------------------------------------------------
// El pool original es un bloque contiguo de la ROM; aqui solo se embeben los
// rangos que los streams referencian, asi que se resuelve el desplazamiento
// original dentro de la tabla de rangos.

const u8 *intro_pool4_ptr(u32 offset)
{
    for (u32 i = 0; i < INTRO_POOL4_RANGE_COUNT; i++) {
        const IntroPoolRange *r = &g_intro_pool4_ranges[i];
        if (offset >= r->offset && offset < (r->offset + r->length)) {
            return r->data + (offset - r->offset);
        }
    }
    return NULL;
}

const u8 *intro_pool8_ptr(u32 offset)
{
    for (u32 i = 0; i < INTRO_POOL8_RANGE_COUNT; i++) {
        const IntroPoolRange *r = &g_intro_pool8_ranges[i];
        if (offset >= r->offset && offset < (r->offset + r->length)) {
            return r->data + (offset - r->offset);
        }
    }
    return NULL;
}

// ------------------------------------------------------------------------------
// Inicializacion comun (FUN_080A5188)
// ------------------------------------------------------------------------------
//
//   0x080A518C  bl 0x0800048C
//   0x080A5190  bl 0x08028D7C
//   0x080A5194  bl 0x08026074
//   0x080A5198  bl 0x08000FE8
//   0x080A519C  bl 0x0802F2DC
//   0x080A51A0  bl 0x08028DA4
//
// Todas son de efectos observables y se portan de forma exacta.
void intro_common_init(void)
{
    // FUN_0800048C: dos palabras a cero.
    //   *(0x030027B0) = 0 ; *(0x030044F0) = 0
    hw_mem_write32(0x030027B0u, 0u);
    hw_mem_write32(0x030044F0u, 0u);

    // FUN_08028D7C: BLDY = 16, BLDCNT = 0x00FF y PALRAM[0] = 0.
    hw_gfx_reg_write16(REG_BLDY, 16u);
    hw_gfx_reg_write16(REG_BLDCNT, 0x00FFu);
    hw_mem_write16(GBA_PALRAM_BASE, 0u);

    // FUN_08026074: limpia la VRAM completa (0x1800 bloques de 16 bytes).
    hw_memzero_blocks(BOOT_CLEAR_VRAM_BASE, BOOT_CLEAR_VRAM_WORDS);

    // FUN_08000FE8: *(0x030027D8) = 0.
    hw_mem_write32(0x030027D8u, 0u);

    // FUN_0802F2DC: limpia 19 slots de 0x60 bytes desde 0x03002080 (la tabla
    // de callbacks de OBJ) y varios campos sueltos.
    for (u32 i = 0; i < 19u; i++) {
        hw_mem_write32(0x03002080u + (i * 0x60u), 0u);
    }
    hw_mem_write32(0x03004508u, 0u);
    hw_mem_write32(0x0300695Cu, 0u);
    hw_mem_write32(0x030044F8u, 0u);
    hw_mem_write32(0x030044C4u, 0u);
    hw_mem_write32(0x030044C8u, 0u);
    hw_mem_write32(0x030044B4u, 0u);
    hw_mem_write32(0x030044B8u, 0u);
    hw_mem_write32(0x030044BCu, 0u);
    hw_mem_write32(0x030044C0u, 0u);

    // FUN_08028DA4: BLDY = 16, BLDCNT = 0x00FF y bytes del descriptor de
    // objeto en 0x030044D8 (+0x0C = 1, +0x0D = 2, +0x0E = 2, +0x0F = 16).
    hw_gfx_reg_write16(REG_BLDY, 16u);
    hw_mem_write8(OBJECT_DESC_ADDR + 0x0Fu, 16u);
    hw_gfx_reg_write16(REG_BLDCNT, 0x00FFu);
    hw_mem_write8(OBJECT_DESC_ADDR + 0x0Cu, 1u);
    hw_mem_write8(OBJECT_DESC_ADDR + 0x0Du, 2u);
    hw_mem_write8(OBJECT_DESC_ADDR + 0x0Eu, 2u);
}

// ------------------------------------------------------------------------------
// Actualizacion comun (FUN_08540B7C)
// ------------------------------------------------------------------------------
// Rutina del modulo de carrera (0x0854xxxx) que actualiza los objetos de
// fondo ya registrados. No es codigo de la pantalla de intro sino del motor
// compartido, asi que en este port queda como un punto de extension
// documentado: el estado 0 solo necesita el montaje estatico de la pantalla.
void intro_common_update(void)
{
    // TODO: portar FUN_08540b7c (actualizacion de los objetos de fondo).
}

// ------------------------------------------------------------------------------
// Bloque comun del estado 0 (0x080A53BC..0x080A5424)
// ------------------------------------------------------------------------------
// Se ejecuta en cada iteracion del bucle del handler original.
void intro_common_block(void)
{
    // 0x080A53BC: init comun.
    intro_common_init();

    // 0x080A53D0: stream 8bpp -> mapa 0x0600F800, tiles de 64 bytes.
    graphics_load_tiles8(g_intro_stream3, INTRO_TILEMAP_ADDR, 1u, intro_pool8_ptr);

    // 0x080A53D8: registra el descriptor del objeto de fondo.
    graphics_register_object(INTRO_ROM_OBJ_DESC);

    // 0x080A53E4: DMA3 de la paleta BG (256 colores) a 0x05000200.
    hw_gfx_dma3_copy(g_intro_pal_bg, GBA_PALRAM_BASE + 0x200u, 128u);

#ifndef TARGET_GBA
    // ---------------------------------------------------------------------
    // Espejo de la paleta de 256 colores (solo en el port PC).
    //
    // La capa de 8bpp del Estado 0 la dibuja la ROM por software: el "objeto
    // de fondo" que registra FUN_08028D60 (descriptor 0x080A5974) usa la
    // paleta de 256 colores que la ROM copia a 0x05000200 (region OBJ). Un BG
    // del hardware GBA, en cambio, solo puede leer su paleta en 0x05000000
    // (region de fondos), donde la ROM dejo la paleta B de 16 colores de las
    // capas 4bpp (0x080A5394).
    //
    // El port PC no ejecuta los callbacks OBJ (0x080A5580 / 0x080A55EC /
    // 0x080A547C: codigo ARM de la ROM), asi que la unica capa que se puede
    // dibujar es el fondo 0. Para que muestre los colores reales del logo se
    // copian los 512 bytes de la paleta de 256 colores a la region de fondos,
    // y el renderizador lee 0x05000000 por defecto en 8bpp.
    //
    // En el target GBA NO se hace: el cartucho no espeja nada y el hardware
    // leeria la paleta B de 16 colores (fondo negro + logo en cian/gris/blanco).
    // ---------------------------------------------------------------------
    {
        const u8 *src = hw_mem_region_ptr(GBA_PALRAM_BASE + 0x200u);
        if (src != NULL) {
            hw_mem_write_block(GBA_PALRAM_BASE, src, 512u);
        }
    }
#endif

    // 0x080A5408: DISPCNT = 0x1140 (modo 0, OBJ 1D mapping, BG0 activa).
    hw_gfx_reg_write16(REG_DISPCNT, 0x1140u);

    // 0x080A5410: BG0CNT = 0x1F80 (tamano 512x512, 256 colores, mapa 0x0600F800).
    hw_gfx_reg_write16(REG_BG0CNT, 0x1F80u);

    // 0x080A5418: registra el callback de dibujado OBJ #3.
    graphics_register_obj_callback(INTRO_ROM_CB_OBJ3);

    // 0x080A5420: BGM pista 1.
    audio_bgm_play(1u);

    // 0x080A5424: actualizacion comun.
    intro_common_update();
}

// ------------------------------------------------------------------------------
// Cuerpo del estado 0 (handler 0x080A52E4)
// ------------------------------------------------------------------------------
void intro_state_body(void)
{
    // 0x080A52E4: *0x030027AC = 0 (el handler se reindexa a si mismo).
    states_set_current(0);

    // 0x080A52F0: DISPCNT = 0x0040 (modo 0, OBJ 1D mapping).
    hw_gfx_reg_write16(REG_DISPCNT, 0x0040u);

    // 0x080A52FC: si el flag "ya inicializado" esta puesto, se salta al
    // bloque comun (el montaje completo solo se hace la primera vez).
    if (hw_mem_read32(INTRO_INIT_FLAG_ADDR) != 0u) {
        intro_common_block();
        return;
    }

    // 0x080A530C: marca "ya inicializado".
    hw_mem_write32(INTRO_INIT_FLAG_ADDR, 1u);

    // ---- Bloque de la primera vez ------------------------------------------

    // 0x080A5314: init comun.
    intro_common_init();

    // 0x080A5318: detiene el BGM (la pantalla monta su propia musica).
    audio_bgm_stop();

    // 0x080A5324: DISPCNT = 0x0140.
    hw_gfx_reg_write16(REG_DISPCNT, 0x0140u);

    // 0x080A532C: BG0CNT = 0x1F00.
    hw_gfx_reg_write16(REG_BG0CNT, 0x1F00u);

    // 0x080A5340: stream 4bpp #1 -> tiles de 32 bytes.
    graphics_load_tiles4(g_intro_stream1, INTRO_TILEMAP_ADDR, 1u, intro_pool4_ptr);

    // 0x080A534C: DMA3 de la paleta OBJ A (16 colores) a 0x05000000.
    hw_gfx_dma3_copy(g_intro_pal_obj_a, GBA_PALRAM_BASE, 8u);

    // 0x080A536C: callback de dibujado OBJ #1.
    graphics_register_obj_callback(INTRO_ROM_CB_OBJ1);

    // 0x080A5370: actualizacion comun.
    intro_common_update();

    // ---- Segunda capa (misma estructura, stream y paleta propios) ----------

    // 0x080A5374: init comun.
    intro_common_init();

    // 0x080A5388: stream 4bpp #2 -> tiles de 32 bytes.
    graphics_load_tiles4(g_intro_stream2, INTRO_TILEMAP_ADDR, 1u, intro_pool4_ptr);

    // 0x080A5394: DMA3 de la paleta OBJ B (16 colores) a 0x05000000.
    hw_gfx_dma3_copy(g_intro_pal_obj_b, GBA_PALRAM_BASE, 8u);

    // 0x080A53B4: callback de dibujado OBJ #2.
    graphics_register_obj_callback(INTRO_ROM_CB_OBJ2);

    // 0x080A53B8: actualizacion comun.
    intro_common_update();

    // ---- Bloque comun -------------------------------------------------------
    intro_common_block();
}
