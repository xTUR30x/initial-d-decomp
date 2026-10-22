#ifndef GUARD_ENGINE_H
#define GUARD_ENGINE_H

#include "global.h"

// ==============================================================================
// include/engine.h - Inicializacion del runtime del motor (SDK de desarrollo)
// ------------------------------------------------------------------------------
// Portado de FUN_08629ebc (entry Thumb 0x08629EBD) y su cola FUN_0862a028.
//
// Responsabilidad:
//   1. Rellena una tabla de punteros a funciones (vtable) en IWRAM
//      (0x030066A4..0x03006704) con 25 direcciones de rutinas del motor en ROM.
//      Asi el juego llama al motor a traves de punteros en RAM (capa de
//      indireccion tipica de un SDK enlazado en la ROM).
//   2. Llama a FUN_0862a028(), que inicializa el estado/buffers del runtime:
//        - memset/clear de varios buffers de IWRAM
//        - escribe contadores y defaults (0, -1, punteros)
//      No toca registros de E/S (ni DISPCNT, ni sonido, ni timers, ni DMA).
//
// Subsistema: LOGICA / RUNTIME (inicializacion de datos en RAM). No es
// graficos, ni entrada, ni audio: todas las direcciones destino son IWRAM.
// ==============================================================================

// ------------------------------------------------------------------------------
// Tabla de punteros a funciones del motor (vtable) en IWRAM.
// 25 entradas de 4 bytes escritas por engine_init_dispatch_table().
// ------------------------------------------------------------------------------
#define ENGINE_VTABLE_COUNT   25u

// Direccion IWRAM de cada slot de la vtable (verificadas con REA).
extern const u32 g_engine_vtable_slots[ENGINE_VTABLE_COUNT];
// Valor (direccion de funcion en ROM) escrito en cada slot.
extern const u32 g_engine_vtable_values[ENGINE_VTABLE_COUNT];

// ------------------------------------------------------------------------------
// Capa de plataforma (implementado en cada target: GBA y PC/SDL2)
// ------------------------------------------------------------------------------

// Escribe un valor en una direccion de RAM del motor.
// En GBA es una direccion real de IWRAM; en PC se redirige a un buffer virtual.
void hw_engine_store32(u32 addr, u32 value);

// Pone a cero `size` bytes en `addr` (equivalente a FUN_0862d800).
void hw_engine_memset(u32 addr, u8 value, u32 size);

// Direccion del buffer de runtime del motor (FUN_0862dea4 limpia 0x8C bytes en
// 0x03004ea0). En PC devuelve un buffer real.
u32 hw_engine_runtime_buffer(void);

// ------------------------------------------------------------------------------
// API del runtime del motor (implementado en src/engine.c)
// ------------------------------------------------------------------------------

// Portado de FUN_08629ebc: instala la vtable y llama al init del runtime.
void engine_runtime_init(void);

// Portado de FUN_0862a028: inicializa estado/buffers del runtime (sin I/O).
void engine_runtime_state_init(void);

// Portado de FUN_0862d154: defaults del bloque de estado del motor.
void engine_runtime_defaults(void);

// Portado de FUN_0862d524 + FUN_0862d5f8: limpia buffers del motor.
void engine_runtime_clear_buffers(void);

#endif // GUARD_ENGINE_H
