// ==============================================================================
// src/graphics.c - Rutinas de graficos (OAM / sprites / VRAM / paletas)
// ------------------------------------------------------------------------------
// Portado de FUN_08026e0c. Clasificado como SUBSISTEMA DE GRAFICOS (OAM).
//
// Original (verificado con REA):
//   idx = *(u32 *)0x03004098;                 // indice inicial (0..127)
//   puVar = (u32 *)(idx * 8 + 0x07000000);    // entrada OAM de ese indice
//   count = 0x80 - idx;                       // sprites restantes hasta 127
//   do {
//       *puVar = 0x000F02B0;                  // attr0|attr1 (fuera de pantalla)
//       puVar++;                              // avanza 4 bytes
//       *(u16 *)puVar = 0;                    // attr2
//       puVar++;                              // avanza 4 bytes (total 8/entrada)
//   } while (--count);
//
// Escribe en la OAM (0x07000000), NO en registros de E/S ni en RAM del juego.
// El efecto es "aparcar" los sprites no usados fuera de pantalla (X=240, Y=176).
//
// Ademas: acceso a registros de E/S, transferencias DMA3, descompresores de
// tiles y registro de objetos (usados por el Estado 0 / intro).
// ==============================================================================

#include "global.h"
#include "graphics.h"
#include "memory.h"

#include <stddef.h>   // NULL

// ------------------------------------------------------------------------------
// API de graficos
// ------------------------------------------------------------------------------

void graphics_hide_oam_sprites(void)
{
    u32 start = hw_oam_start_index();
    if (start >= OAM_ENTRY_COUNT) {
        return;  // nada que ocultar
    }

    for (u32 i = start; i < OAM_ENTRY_COUNT; i++) {
        // Escribe attr0|attr1 en una palabra y attr2 en la media palabra siguiente.
        hw_oam_write_entry(i, OAM_HIDDEN_WORD, 0);
    }
}

// ------------------------------------------------------------------------------
// Descompresores de tiles
// ------------------------------------------------------------------------------
// Los dos descompresores de la ROM comparten estructura:
//
//   cabecera:  4bpp -> [cols u8][rows u8][ip u16]
//              8bpp -> [cols u8][rows u8]
//   entradas:  cols*rows media palabras (una por celda del mapa)
//   word == 0  -> celda vacia: se escribe 0 en el mapa y NO se copia tile
//   word != 0  -> offset = (word << 17) >> shift  (shift = 12 en 4bpp, 11 en 8bpp)
//                  se copia el tile a VRAM y la entrada del mapa es
//                      slot + ((word & 0x8000) >> 5) [+ (ip << 12) en 4bpp]
//   al terminar una fila el cursor del mapa avanza 64 bytes (32 celdas).
//
// La cabecera se lee UNA SOLA VEZ antes de la primera fila (verificado en el
// desensamblado: el salto de fila vuelve a 'mov r4,r7', no a la lectura de la
// cabecera).

static u16 gfx_read_le16(const u8 *p)
{
    return (u16)((u16)p[0] | ((u16)p[1] << 8));
}

// Portado de FUN_0854c5b8 (4bpp, tiles de 32 bytes).
void graphics_load_tiles4(const u8 *stream, u32 dst_map, u32 first_slot,
                          GfxTilePoolFn pool)
{
    u32 slot = first_slot;                  // sl
    u32 vram = GBA_VRAM_BASE + (slot << 5); // r9
    u32 map = dst_map;                      // r2 (cursor de escritura)
    u32 row_base = dst_map;                 // r8

    const u32 cols = stream[0];
    const u32 rows = stream[1];
    const u16 ip = gfx_read_le16(stream + 2);
    const u8 *src = stream + 4;

    for (u32 row = 0; row < rows; row++) {
        for (u32 col = 0; col < cols; col++) {
            const u16 word = gfx_read_le16(src);
            src += 2;

            u16 entry;

            if (word == 0u) {
                // Celda vacia: entrada 0, no se copia tile ni avanza el slot.
                entry = 0u;
            } else {
                const u32 offset = (u32)(((u32)word << 17) >> 12);
                const u8 *tile = pool(offset);

                if (tile != NULL) {
                    // DMA3 de 32 bytes a 0x06000000 + slot*32 (CNT_L = 8).
                    hw_gfx_dma3_copy(tile, vram, 8u);
                }
                vram += 32u;

                entry = (u16)(slot + ((word & 0x8000u) >> 5));
                entry = (u16)(entry | (u16)((u32)ip << 12));
                slot++;
            }

            hw_gfx_vram_write16(map, entry);
            map += 2u;
        }

        row_base += 64u;
        map = row_base;
    }
}

// Portado de FUN_08000e88 (8bpp, tiles de 64 bytes). En la ROM esta rutina
// se copia a IWRAM (0x03001f80) y se ejecuta desde alli; el stub 0x08000e74
// calcula r9 = 0x06000000 + sl*64 y salta a la copia.
void graphics_load_tiles8(const u8 *stream, u32 dst_map, u32 first_slot,
                          GfxTilePoolFn pool)
{
    u32 slot = first_slot;                  // sl
    u32 vram = GBA_VRAM_BASE + (slot << 6); // r9
    u32 map = dst_map;                      // r2 (cursor de escritura)
    u32 row_base = dst_map;                 // r8

    const u32 cols = stream[0];
    const u32 rows = stream[1];
    const u8 *src = stream + 2;

    for (u32 row = 0; row < rows; row++) {
        for (u32 col = 0; col < cols; col++) {
            const u16 word = gfx_read_le16(src);
            src += 2;

            u16 entry;

            if (word == 0u) {
                // Celda vacia: entrada 0 (el original escribe ip = word<<17 = 0).
                entry = 0u;
            } else {
                const u32 offset = (u32)(((u32)word << 17) >> 11);
                const u8 *tile = pool(offset);

                if (tile != NULL) {
                    // DMA3 de 64 bytes a 0x06000000 + slot*64 (CNT_L = 16).
                    hw_gfx_dma3_copy(tile, vram, 16u);
                }
                vram += 64u;

                entry = (u16)(slot + ((word & 0x8000u) >> 5));
                slot++;
            }

            hw_gfx_vram_write16(map, entry);
            map += 2u;
        }

        row_base += 64u;
        map = row_base;
    }
}

// ------------------------------------------------------------------------------
// Objetos
// ------------------------------------------------------------------------------

// Portado de FUN_080af768: busca el primer slot libre de la tabla de
// 0x03002080 (16 slots de 0x60 bytes) e inicializa sus campos.
u8 *graphics_register_obj_callback(u32 callback)
{
    u8 *base = hw_mem_region_ptr(OBJ_SLOT_TABLE_ADDR);

    if (base == NULL) {
        return NULL;
    }

    for (u32 i = 0; i < OBJ_SLOT_COUNT; i++) {
        u8 *slot = base + i * OBJ_SLOT_SIZE;
        u32 *w = (u32 *)slot;

        if (w[0] != 0u) {
            continue;   // slot ocupado
        }

        // Inicializacion exacta del original (0x080af78c..0x080af7ec).
        // Se omiten los offsets que la ROM no escribe (+0x1C).
        w[0]  = callback;   // +0x00
        w[1]  = 0u;         // +0x04
        w[2]  = 0u;         // +0x08
        w[3]  = 0u;         // +0x0C
        w[4]  = 0x10000u;   // +0x10
        w[5]  = 0x10000u;   // +0x14
        w[6]  = 0u;         // +0x18
        w[8]  = 0u;         // +0x20
        w[9]  = 0u;         // +0x24
        w[10] = 0u;         // +0x28
        w[11] = 0u;         // +0x2C
        w[12] = 0u;         // +0x30
        w[13] = 0u;         // +0x34
        w[14] = 0u;         // +0x38
        w[15] = 0u;         // +0x3C
        w[16] = 0u;         // +0x40
        w[17] = 0u;         // +0x44
        w[18] = 0u;         // +0x48
        w[19] = 0u;         // +0x4C
        w[20] = 0u;         // +0x50
        w[21] = 0u;         // +0x54
        w[22] = 0u;         // +0x58
        w[23] = 0u;         // +0x5C

        return slot;
    }

    return NULL;   // tabla llena: el original tambien retorna sin hacer nada
}

// Portado de FUN_08028d60 se movio al motor de fundidos (src/fade.c).

// ==============================================================================
// Implementacion de la capa de plataforma
// ==============================================================================
#ifdef TARGET_GBA

u32 hw_oam_start_index(void)
{
    return *(const vu32 *)OAM_START_INDEX_ADDR;
}

void hw_oam_write_entry(u32 index, u32 attr01_word, u16 attr2)
{
    vu32 *entry = (vu32 *)(OAM_BASE_ADDR + index * OAM_ENTRY_SIZE);
    entry[0] = attr01_word;             // attr0 + attr1
    *(vu16 *)((u8 *)entry + 4u) = attr2; // attr2
}

void hw_gfx_reg_write16(u32 reg, u16 value) { *(vu16 *)reg = value; }
void hw_gfx_reg_write32(u32 reg, u32 value) { *(vu32 *)reg = value; }
u16  hw_gfx_reg_read16(u32 reg)             { return *(vu16 *)reg; }

void hw_gfx_dma3_copy(const void *src, u32 dst, u32 words)
{
    *(vu32 *)REG_DMA3SAD   = (u32)src;
    *(vu32 *)REG_DMA3DAD   = dst;
    *(vu16 *)REG_DMA3CNT_L = (u16)words;
    *(vu16 *)REG_DMA3CNT_H = DMA3_CTRL_32BIT_IMMEDIATE;
}

void hw_gfx_vram_write16(u32 addr, u16 value)
{
    *(vu16 *)addr = value;
}

#else // ===================== PC / SDL2 =====================

#include <stdio.h>
#include <string.h>

// Buffer virtual que simula la OAM (128 entradas * 8 bytes = 1 KB).
static u8 g_oam[OAM_ENTRY_COUNT * OAM_ENTRY_SIZE];
static u32 g_oam_start_index = 0;  // simula *(0x03004098)

// Registros de E/S simulados (0x04000000..0x040001FF).
static u16 g_gfx_regs[0x200];

u32 hw_oam_start_index(void)
{
    return g_oam_start_index;
}

void hw_oam_write_entry(u32 index, u32 attr01_word, u16 attr2)
{
    if (index >= OAM_ENTRY_COUNT) {
        return;
    }
    u8 *entry = g_oam + index * OAM_ENTRY_SIZE;
    *(u32 *)entry = attr01_word;
    *(u16 *)(entry + 4u) = attr2;
}

void hw_gfx_reg_write16(u32 reg, u16 value)
{
    if (reg >= 0x04000000u && reg < 0x04000200u) {
        g_gfx_regs[(reg - 0x04000000u) >> 1] = value;
    }
}

void hw_gfx_reg_write32(u32 reg, u32 value)
{
    hw_gfx_reg_write16(reg, (u16)(value & 0xFFFFu));
    hw_gfx_reg_write16(reg + 2u, (u16)((value >> 16) & 0xFFFFu));
}

u16 hw_gfx_reg_read16(u32 reg)
{
    if (reg >= 0x04000000u && reg < 0x04000200u) {
        return g_gfx_regs[(reg - 0x04000000u) >> 1];
    }
    return 0u;
}

void hw_gfx_dma3_copy(const void *src, u32 dst, u32 words)
{
    // Se registra la transferencia como en la ROM y se copia al buffer virtual.
    hw_gfx_reg_write32(REG_DMA3SAD, (u32)(uintptr_t)src);
    hw_gfx_reg_write32(REG_DMA3DAD, dst);
    hw_gfx_reg_write16(REG_DMA3CNT_L, (u16)words);
    hw_gfx_reg_write16(REG_DMA3CNT_H, DMA3_CTRL_32BIT_IMMEDIATE);

    hw_mem_write_block(dst, src, words * 4u);
}

void hw_gfx_vram_write16(u32 addr, u16 value)
{
    hw_mem_write16(addr, value);
}

// Acceso de depuracion desde el arranque en PC.
void hw_oam_debug_dump(void)
{
    // Cuenta cuantas entradas quedaron "ocultas" (attr0 = 0x02B0).
    u32 hidden = 0;
    for (u32 i = 0; i < OAM_ENTRY_COUNT; i++) {
        if (*(const u16 *)(g_oam + i * OAM_ENTRY_SIZE) == OAM_HIDDEN_ATTR0) {
            hidden++;
        }
    }
    printf("[gfx] OAM: %u/%u sprites aparcados fuera de pantalla (simulado)\n",
           (unsigned)hidden, (unsigned)OAM_ENTRY_COUNT);
}

#endif // TARGET_GBA
