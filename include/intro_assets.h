#ifndef GUARD_INTRO_ASSETS_H
#define GUARD_INTRO_ASSETS_H

#include "global.h"

// ==============================================================================
// include/intro_assets.h - Assets del Estado 0 (intro/logo)
// ------------------------------------------------------------------------------
// GENERADO AUTOMATICAMENTE por tools/extract_intro_assets.py - NO EDITAR.
// Origen: baserom.gba (SHA-256 b1684b7d6a678189011bb607e9f2c23febe5b9062303e8afe56cba43246a1592).
//
// Contiene:
//   * las 4 paletas del estado 0 (2 OBJ de 16 colores, la principal de 256
//     colores que el motor copia por DMA3 a 0x05000000 y la de la region OBJ
//     que el handler copia a 0x05000200),
//   * los 3 streams de tiles comprimidos (4bpp, 4bpp, 8bpp),
//   * los bloques de los pools de tiles que esos streams referencian.
//
// En el target GBA estos arrays viven en la ROM del cartucho, por lo que las
// copias por DMA3 siguen siendo transferencias ROM -> PAL RAM, igual que en el
// original.
// ==============================================================================

// Numero de rangos embebidos de cada pool de tiles.
#define INTRO_POOL4_RANGE_COUNT  4u
#define INTRO_POOL8_RANGE_COUNT  2u

// Rango del pool de tiles: 'offset' es el desplazamiento dentro del pool
// original de la ROM, 'length' su tamano en bytes y 'data' los datos.
typedef struct {
    u32        offset;
    u32        length;
    const u8  *data;
} IntroPoolRange;

// Paletas del estado 0.
extern const u8 g_intro_pal_obj_a[32];
extern const u8 g_intro_pal_obj_b[32];
extern const u8 g_intro_pal_main[512];   // 0x080A5974 -> 0x05000000 (BG0 8bpp)
extern const u8 g_intro_pal_bg[512];     // 0x080A5B74 -> 0x05000200 (region OBJ)

// Streams de tiles comprimidos (cabecera + entradas).
extern const u8 g_intro_stream1[1204];
extern const u8 g_intro_stream2[1204];
extern const u8 g_intro_stream3[1202];

// Rangos embebidos de los pools de tiles.
extern const IntroPoolRange g_intro_pool4_ranges[INTRO_POOL4_RANGE_COUNT];
extern const IntroPoolRange g_intro_pool8_ranges[INTRO_POOL8_RANGE_COUNT];

#endif // GUARD_INTRO_ASSETS_H
