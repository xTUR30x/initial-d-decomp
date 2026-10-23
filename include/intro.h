#ifndef GUARD_INTRO_H
#define GUARD_INTRO_H

#include "global.h"

// ==============================================================================
// include/intro.h - Estado 0 (intro / logo de arranque)
// ------------------------------------------------------------------------------
// Portado del handler 0x080A52E4 del cartucho (verificado con objdump sobre
// baserom.gba, SHA-256 b1684b7d6a678189011bb607e9f2c23febe5b9062303e8afe56cba43246a1592).
//
// Clasificacion: GRAFICOS + AUDIO + LOGICA DE ESTADO.
//
// Secuencia original (ARM):
//
//   0x080A52E4  *0x030027AC = 0                     ; el handler se reindexa
//   0x080A52F0  DISPCNT = 0x0040                    ; modo 0, OBJ 1D mapping
//   0x080A52FC  if (*0x030047AC != 0) -> 0x080A53BC ; flag "ya inicializado"
//   0x080A530C  *0x030047AC = 1
//   0x080A5314  bl 0x080A5188                       ; init comun
//   0x080A5318  bl 0x080005D4                       ; BGM stop
//   0x080A5324  DISPCNT = 0x0140
//   0x080A532C  BG0CNT  = 0x1F00
//   0x080A5340  bl 0x0854C5B8 (stream 4bpp #1, r1=0x080A6174, r2=0x0600F800)
//   0x080A534C  DMA3 0x080A5934 -> 0x05000000 (8 palabras)  ; paleta OBJ A
//   0x080A536C  bl 0x080AF768 (r4=0x080A5580)       ; callback OBJ #1
//   0x080A5370  bl 0x08540B7C                       ; actualizacion comun
//   0x080A5374  bl 0x080A5188
//   0x080A5388  bl 0x0854C5B8 (stream 4bpp #2, r1=0x080A6628)
//   0x080A5394  DMA3 0x080A5954 -> 0x05000000 (8 palabras)  ; paleta OBJ B
//   0x080A53B4  bl 0x080AF768 (r4=0x080A55EC)       ; callback OBJ #2
//   0x080A53B8  bl 0x08540B7C
//   0x080A53BC  bl 0x080A5188                       ; BLOQUE COMUN (cada vuelta)
//   0x080A53D0  bl 0x08000E74 (stream 8bpp, r1=0x080A6ADC)
//   0x080A53D8  bl 0x08028D60 (r3=0x080A5974)       ; peticion de DMA de paleta
//                                                     (el VBlank la copia a
//                                                      0x05000000)
//   0x080A53E4  DMA3 0x080A5B74 -> 0x05000200 (128 palabras) ; paleta OBJ
//   0x080A5408  DISPCNT = 0x1140
//   0x080A5410  BG0CNT  = 0x1F80
//   0x080A5418  bl 0x080AF768 (r4=0x080A547C)       ; callback OBJ #3
//   0x080A5420  bl 0x0800055C (r4=1)                ; BGM pista 1
//   0x080A5424  bl 0x08540B7C
//   0x080A5428  r1 = *0x03004A5C ; if (r1 == 0) -> 0x080A5314 (repetir)
//   0x080A543C  *0x030027AC = r1 ; b 0x080002E8    ; transicion dinamica
// ==============================================================================

// ------------------------------------------------------------------------------
// Direcciones ROM verificadas de los assets del estado 0
// ------------------------------------------------------------------------------
#define INTRO_ROM_PAL_OBJ_A   0x080A5934u  // paleta OBJ, 16 colores (32 bytes)
#define INTRO_ROM_PAL_OBJ_B   0x080A5954u  // paleta OBJ, 16 colores (32 bytes)
#define INTRO_ROM_PAL_MAIN    0x080A5974u  // paleta principal, 256 colores (512 B)
                                           // FUN_08028D60 -> DMA3 -> 0x05000000
#define INTRO_ROM_PAL_BG      0x080A5B74u  // paleta de la region OBJ (512 B)
                                           // DMA3 -> 0x05000200
#define INTRO_ROM_STREAM1     0x080A6174u  // stream 4bpp (cabecera 4 B + 600)
#define INTRO_ROM_STREAM2     0x080A6628u  // stream 4bpp (cabecera 4 B + 600)
#define INTRO_ROM_STREAM3     0x080A6ADCu  // stream 8bpp (cabecera 2 B + 600)
#define INTRO_ROM_CB_OBJ1     0x080A5580u  // callback de dibujado OBJ #1
#define INTRO_ROM_CB_OBJ2     0x080A55ECu  // callback de dibujado OBJ #2
#define INTRO_ROM_CB_OBJ3     0x080A547Cu  // callback de dibujado OBJ #3

// Bases de los pools de tiles (literales ARM de los descompresores).
#define INTRO_POOL4_BASE      0x0802FB28u  // literal en 0x0854C6B8 (4bpp)
#define INTRO_POOL8_BASE      0x08556B0Cu  // literal en 0x08000F00 (8bpp)

// Mapa de tiles donde se dibuja la pantalla (0x0600F800).
#define INTRO_TILEMAP_ADDR    0x0600F800u

// Flag "ya inicializado" del estado 0 (0x030047AC).
#define INTRO_INIT_FLAG_ADDR  0x030047ACu

// ------------------------------------------------------------------------------
// API del modulo (implementada en src/intro.c)
// ------------------------------------------------------------------------------

// Cuerpo completo del estado 0 (un equivalente del handler 0x080A52E4).
// Se ejecuta en cada frame mientras el estado siga activo; la transicion la
// aplica la maquina de estados leyendo la solicitud pendiente (0x03004A5C).
void intro_state_body(void);

// Bloque comun que el estado 0 ejecuta en cada iteracion.
void intro_common_block(void);

// Portado de FUN_080A5188: inicializacion comun del motor de pantallas.
void intro_common_init(void);

// Portado de FUN_08540B7C: espera de frame del estado 0 (ARM; 0x08540B80 es un
// bucle de sincronizacion, no Thumb). El ritmo de frames lo marca la plataforma.
void intro_common_update(void);

// Resolucion de un desplazamiento dentro de los pools de tiles originales.
const u8 *intro_pool4_ptr(u32 offset);
const u8 *intro_pool8_ptr(u32 offset);

#endif // GUARD_INTRO_H
