#ifndef GUARD_GRAPHICS_H
#define GUARD_GRAPHICS_H

#include "global.h"

// ==============================================================================
// include/graphics.h - Rutinas de graficos (OAM / sprites / VRAM / paletas)
// ------------------------------------------------------------------------------
// Portado de FUN_08026e0c. Inicializa la OAM: aparca (oculta) los sprites
// desde un indice inicial hasta el sprite 127, escribiendo el patron
// "fuera de pantalla" en cada entrada de 8 bytes.
//
// Ademas incorpora las piezas de video que necesita el Estado 0 (intro/logo):
//   * escritura de registros de E/S (DISPCNT, BGxCNT, BLDCNT/BLDY),
//   * transferencias DMA3 (paletas y tiles) con el valor literal 0x8400,
//   * los dos descompresores de tiles de la ROM (4bpp y 8bpp),
//   * el registro de callbacks de OBJ (FUN_080af768) y de objeto de fondo
//     (FUN_08028d60).
// ==============================================================================

// OAM del GBA: 128 entradas de 8 bytes (attr0/attr1/attr2/padding).
#define OAM_BASE_ADDR     0x07000000u
#define OAM_ENTRY_COUNT   128u
#define OAM_ENTRY_SIZE    8u

// Patron "sprite oculto" (fuera de pantalla, verificado con REA):
//   attr0 = 0x02B0 -> Y = 176, shape = 0 (cuadrado), 4bpp
//   attr1 = 0x00F0 -> X = 240
//   attr2 = 0
// El word completo escrito en attr0|attr1 es 0x000F02B0.
#define OAM_HIDDEN_ATTR0  0x02B0u  // Y = 176 (offscreen)
#define OAM_HIDDEN_ATTR1  0x00F0u  // X = 240 (offscreen)
#define OAM_HIDDEN_WORD   0x000F02B0u

// Variable de estado (IWRAM) que indica el primer sprite a aparcar.
// Cadena de arranque: puntero ROM 0x080003DC -> 0x03004098.
#define OAM_START_INDEX_ADDR  0x03004098u

// ------------------------------------------------------------------------------
// Registros de E/S de video
// ------------------------------------------------------------------------------
#define REG_DISPCNT      0x04000000u  // control de display
#define REG_DISPSTAT     0x04000004u  // estado de display
#define REG_BG0CNT       0x04000008u  // control del fondo 0
#define REG_BG1CNT       0x0400000Au
#define REG_BG2CNT       0x0400000Cu
#define REG_BG3CNT       0x0400000Eu
#define REG_BLDCNT       0x04000050u  // control de blend
#define REG_BLDY         0x04000054u  // intensidad de blend
#define REG_DMA3SAD      0x040000D4u  // origen del DMA3
#define REG_DMA3DAD      0x040000D8u  // destino del DMA3
#define REG_DMA3CNT_L    0x040000DCu  // numero de unidades
#define REG_DMA3CNT_H    0x040000DEu  // control del DMA3

// Valor literal de DMA3CNT_H usado por la ROM (32 bits, inmediato, inc.).
#define DMA3_CTRL_32BIT_IMMEDIATE  0x8400u

// ------------------------------------------------------------------------------
// Tabla de callbacks de OBJ (FUN_080af768)
// ------------------------------------------------------------------------------
// 16 slots de 0x60 bytesstarting en 0x03002080 (literal en 0x080af890).
#define OBJ_SLOT_TABLE_ADDR  0x03002080u
#define OBJ_SLOT_COUNT       16u
#define OBJ_SLOT_SIZE        0x60u

// Descriptores del objeto de fondo (FUN_08028d60).
#define OBJECT_DESC_ADDR     0x030044D8u

// ------------------------------------------------------------------------------
// Capa de plataforma (implementado en cada target: GBA y PC/SDL2)
// ------------------------------------------------------------------------------

// Lee el indice inicial de sprites a ocultar (0..127).
u32 hw_oam_start_index(void);

// Escribe una entrada OAM (8 bytes) en el indice dado.
// En GBA -> OAM real (0x07000000); en PC -> buffer virtual.
void hw_oam_write_entry(u32 index, u32 attr01_word, u16 attr2);

// Escribe un halfword / dword en un registro de E/S.
void hw_gfx_reg_write16(u32 reg, u16 value);
void hw_gfx_reg_write32(u32 reg, u32 value);
u16  hw_gfx_reg_read16(u32 reg);

// Transferencia DMA3: copia `words` palabras de 32 bits desde `src` a `dst`.
// En GBA programa DMA3SAD/DAD/CNT_L/CNT_H (0x8400); en PC copia al buffer
// virtual equivalente (VRAM / PAL RAM / IWRAM).
void hw_gfx_dma3_copy(const void *src, u32 dst, u32 words);

// Escribe un halfword en VRAM (mapa de tiles) o PAL RAM.
void hw_gfx_vram_write16(u32 addr, u16 value);

// ------------------------------------------------------------------------------
// API de graficos (implementado en src/graphics.c)
// ------------------------------------------------------------------------------

// Portado de FUN_08026e0c: oculta los sprites [start, 127] de la OAM.
void graphics_hide_oam_sprites(void);

// ------------------------------------------------------------------------------
// Descompresores de tiles
// ------------------------------------------------------------------------------
// Resuelve el desplazamiento de un bloque dentro del pool de tiles original.
// Devuelve NULL si el bloque no esta embebido.
typedef const u8 *(*GfxTilePoolFn)(u32 offset);

// Portado de FUN_0854c5b8 (4bpp): cabecera de 4 bytes (cols, rows, ip) y
// tiles de 32 bytes. Escribe el mapa en `dst_map` y copia los tiles a VRAM
// (0x06000000 + slot*32).
void graphics_load_tiles4(const u8 *stream, u32 dst_map, u32 first_slot,
                          GfxTilePoolFn pool);

// Portado de FUN_08000e88 (8bpp, ejecutado desde la copia en IWRAM
// 0x03001f80): cabecera de 2 bytes (cols, rows) y tiles de 64 bytes.
void graphics_load_tiles8(const u8 *stream, u32 dst_map, u32 first_slot,
                          GfxTilePoolFn pool);

// ------------------------------------------------------------------------------
// Objetos
// ------------------------------------------------------------------------------

// Portado de FUN_080af768: registra un callback de OBJ en el primer slot
// libre de la tabla de 0x03002080. Devuelve el slot o NULL si esta llena.
u8 *graphics_register_obj_callback(u32 callback);

// Portado de FUN_08028d60: registra el descriptor del objeto de fondo.
void graphics_register_object(u32 desc);

#ifndef TARGET_GBA
// Solo PC: volcado de depuracion del estado simulado de la OAM.
void hw_oam_debug_dump(void);
#endif

#endif // GUARD_GRAPHICS_H
