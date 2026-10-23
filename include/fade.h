#ifndef GUARD_FADE_H
#define GUARD_FADE_H

#include "global.h"

// ==============================================================================
// include/fade.h - Motor de fundidos y cola de paletas (BLD + DMA3)
// ------------------------------------------------------------------------------
// Portado del bloque 0x08028AC4..0x08029030 de baserom.gba
// (SHA-256 b1684b7d6a678189011bb607e9f2c23febe5b9062303e8afe56cba43246a1592).
//
// El motor vive en el handler de VBlank: 0x08000BC4 llama a FUN_08028AC4
// antes de la animacion de paletas de OBJ (FUN_0802E0F0). FUN_08028AC4 hace,
// en este orden:
//
//   1. Ejecuta la peticion pendiente de copia de paleta (DMA3) si la hay.
//   2. Llama a FUN_0802E0F0 (animacion de paletas OBJ; NO portada).
//   3. Avanza la maquina de estados de fundido (descriptor 0x030044D8).
//
// Descriptor verificado (0x030044D8):
//   +0x00  u32  count      numero de halfwords pendientes de copiar
//   +0x04  u32  dst_index  indice destino en PAL RAM (en halfwords)
//   +0x08  u32  rom_src    direccion ROM de origen
//   +0x0C  u8   state      estado de la maquina de fundido
//   +0x0D  u8   frames     frames por paso (reload del timer)
//   +0x0E  u8   timer      contador decreciente de frames
//   +0x0F  u8   bldy       nivel de BLDY que va aplicando la maquina
//   +0x10  u32  pending    valor que se publica en 0x030027B0 al acabar
//   +0x18  u32  extra      se pone a 0 al terminar los estados 1/3/5
//
// Estados de la maquina (todos verificados en el desensamblado). La direccion
// real de cada fundido se deduce de BLDY y del modo de BLDCNT aplicando la
// formula de hardware:
//   modo 3 (decrease): nivel 16 = negro, nivel 0 = sin efecto
//   modo 2 (increase): nivel 16 = blanco, nivel 0 = sin efecto
//   1 -> BLDCNT 0x00FF (modo 3, todos), BLDY 16 -> 0: fundido de ENTRADA
//        desde negro (0x08028DA4). Al llegar a 0: estado 0, BLDCNT = 0x0F40
//        (modo alpha sin 1er objetivo = inerte), BLDALPHA = 0x0808, +0x18 = 0.
//   2 -> BLDCNT 0x00FF (modo 3), BLDY 0 -> 16: fundido de SALIDA a negro
//        (0x08028FA4). Al llegar a 16: estado 0 y, si +0x10 != 0, lo publica
//        en 0x030027B0 y limpia +0x10.
//   3 -> BLDCNT 0x00F4 (modo 3, BG2/OBJ/backdrop), BLDY 16 -> 0: entrada
//        desde negro de ese subconjunto (0x08028EA4). Mismo final que 1.
//   4 -> BLDCNT 0x00F4, BLDY 0 -> 16: salida a negro del subconjunto
//        (0x08028FE4). Al llegar a 16: estado 0.
//   5 -> BLDCNT 0x00BF (modo 2, todos), BLDY 16 -> 0: entrada desde BLANCO
//        (0x08028DE4). Mismo final que 1.
//   6 -> BLDCNT 0x00BE (modo 2, todos menos BG0), BLDY 0 -> 16: salida a
//        blanco (0x08028E24). Al llegar a 16: estado 7 (reposo; el switch
//        del original ignora 7).
//   8 -> estado 0, BLDCNT = 0x0442 (alpha BG1 sobre BG2) y BLDALPHA = 0x1010
//        (EVA = EVB = 16).
//
// La semantica de BLD que aplica el renderizador del port PC esta en
// src/pc_main.c: nivel = min(BLDY,16), modo 3 = C*(16-nivel)/16,
// modo 2 = C + (255-C)*nivel/16, modo 1 = EVA/EVB del registro BLDALPHA.
// ==============================================================================

// Direccion del descriptor del motor (misma region que usa FUN_08028D60).
#define FADE_DESC_ADDR            0x030044D8u

// Variable donde el estado 2 publica la peticion pendiente (+0x10).
#define FADE_PENDING_PUBLISH_ADDR 0x030027B0u

// ------------------------------------------------------------------------------
// Cola de paletas (FUN_08028D60, ejecutada por FUN_08028AC4 en VBlank)
// ------------------------------------------------------------------------------

// Portado de FUN_08028D60: pide copiar a PAL RAM 256 halfwords (512 bytes)
// desde `rom_src` al indice 0 (0x05000000). La copia real la hace el VBlank.
void fade_palette_dma_request(u32 rom_src);

// ------------------------------------------------------------------------------
// API de fundidos (0x08028D7C..0x08029030)
// ------------------------------------------------------------------------------

// Portado de FUN_08028D7C: BLDY = 16, BLDCNT = 0x00FF y PALRAM[0] = 0.
// No toca el estado de la maquina.
void fade_reset(void);

// Portado de FUN_08028DA4: estado 1 (oscurecer todos, 0x00FF, BLDY 16).
void fade_out_all(void);

// Portado de FUN_08028DE4: estado 5 (aclarar hacia blanco, 0x00BF, BLDY 16).
void fade_to_white(void);

// Portado de FUN_08028E24: estado 6 (volver del blanco, 0x00BE, BLDY 0).
void fade_from_white(void);

// Portado de FUN_08028EA4: estado 3 (oscurecer subconjunto, 0x00F4).
// Solo actua si la maquina esta parada (estado 0).
void fade_out_subset(void);

// Portado de FUN_08028FA4: estado 2 sin publicar peticion (0x00FF, BLDY 0).
void fade_in_all(void);

// Portado de FUN_08028EF0/0x08028F00: estado 2 con peticion pendiente = 1.
void fade_in_all_request(void);

// Portado de FUN_08028FE4: estado 4 (volver del blanco en un subconjunto).
// Solo actua si la maquina esta parada (estado 0).
void fade_in_subset(void);

// Dispara el estado 8 (alpha BG1 sobre BG2) en el siguiente VBlank.
void fade_request_alpha_bg1_bg2(void);

// ------------------------------------------------------------------------------
// Tick por frame
// ------------------------------------------------------------------------------

// Portado de FUN_08028AC4. Debe llamarse una vez por frame, en el punto en el
// que el cartucho ejecuta el handler de VBlank.
void fade_frame_update(void);

#endif // GUARD_FADE_H
