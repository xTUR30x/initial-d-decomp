// ==============================================================================
// src/engine.c - Inicializacion del runtime del motor (SDK de desarrollo)
// ------------------------------------------------------------------------------
// Portado de FUN_08629ebc (entry Thumb 0x08629EBD) y su cola FUN_0862a028.
// Clasificado como SUBSISTEMA DE LOGICA / RUNTIME.
//
// Original (verificado con REA):
//   FUN_08629ebc:
//     - 25 pares [destino],[valor]: *destino = valor
//       -> rellena la vtable del motor en IWRAM (0x030066A4..0x03006704)
//         con 25 direcciones de funciones del motor en ROM.
//     - llama a FUN_0862a028() (init del estado del runtime).
//     - retorna (bx r0).
//
//   FUN_0862a028 (estado del runtime):
//     - FUN_0862d154(): defaults del bloque de estado.
//     - FUN_0862d524(): limpia buffers (FUN_0862d5f8).
//     - FUN_0862d120(_DAT_0862a114 = 0x0845A91C): escribe puntero.
//     - serie de *destino = valor (0 / -1) en IWRAM.
//     - FUN_0862d840(0x03004838, 0, 0x0845A918 << 2): memset.
//     - FUN_0862dea4(): memset(0x03004ea0, 0, 0x8C).
//     - retorna.
//
// No se toca NINGUN registro de E/S: todo son escrituras a IWRAM. Por eso la
// logica se ejecuta igual en PC, redirigiendo las direcciones fijas a buffers
// virtuales mediante los wrappers hw_engine_*.
// ==============================================================================

#include "global.h"
#include "engine.h"

// ------------------------------------------------------------------------------
// Vtable del motor: 25 slots en IWRAM y sus valores (funciones en ROM).
// Extraidos del par (destino, valor) del original, en orden de ejecucion.
// ------------------------------------------------------------------------------
const u32 g_engine_vtable_slots[ENGINE_VTABLE_COUNT] = {
    0x030066E0u, 0x030066CCu, 0x030066E4u, 0x030066B8u, 0x030066C0u,
    0x03006700u, 0x030066D4u, 0x030066D8u, 0x030066C8u, 0x030066E8u,
    0x030066BCu, 0x030066D0u, 0x030066DCu, 0x030066B0u, 0x030066ECu,
    0x03006704u, 0x030066F8u, 0x030066A8u, 0x030066B4u, 0x030066C4u,
    0x030066F4u, 0x030066F0u, 0x030066ACu, 0x030066A4u, 0x030066FCu,
};

const u32 g_engine_vtable_values[ENGINE_VTABLE_COUNT] = {
    0x080AF618u, 0x080004BCu, 0x08001B10u, 0x08001B78u, 0x0802E05Cu,
    0x08029030u, 0x08000654u, 0x0809D5BCu, 0x0800CD38u, 0x0800CE1Cu,
    0x08028E64u, 0x0800CE44u, 0x080004E0u, 0x08028EA4u, 0x08028FE4u,
    0x0800CF30u, 0x0800D2CCu, 0x080012C0u, 0x080AF348u, 0x0802F2B8u,
    0x080AF7F8u, 0x08001250u, 0x0802E080u, 0x0802E0CCu, 0x0802CD04u,
};

// Direcciones del bloque de estado del runtime (IWRAM).
#define ENGINE_STATE_A          0x030067D0u  // slot de FUN_0862d154 / FUN_0862d120
#define ENGINE_STATE_B          0x030068ACu  // <- 0x030068A0 (por defecto)
#define ENGINE_STATE_C          0x030068A8u  // <- 0
#define ENGINE_STATE_D          0x030068B4u  // <- 0
#define ENGINE_STATE_E          0x030067C8u  // <- 0
#define ENGINE_STATE_F          0x030068B8u  // <- 2
#define ENGINE_STATE_G          0x030068A4u  // <- 0

// Bloque de estado de FUN_0862a028 (IWRAM).
#define ENGINE_CTRL_0           0x03006718u  // <- -1
#define ENGINE_CTRL_1           0x03004A10u  // <- -1
#define ENGINE_CTRL_2           0x0300679Cu  // <- 0
#define ENGINE_CTRL_3           0x030067B0u  // <- -1
#define ENGINE_CTRL_4           0x03006758u  // <- 0
#define ENGINE_CTRL_5           0x0300671Cu  // <- 0
#define ENGINE_CTRL_6           0x03006754u  // <- 0
#define ENGINE_CTRL_7           0x030067A8u  // <- 0
#define ENGINE_CTRL_8           0x030067A0u  // <- 0
#define ENGINE_CTRL_9           0x03006790u  // <- 0
#define ENGINE_CTRL_10          0x03006798u  // <- 0
#define ENGINE_CTRL_11          0x03006750u  // <- 0
#define ENGINE_CTRL_12          0x030067A4u  // <- 0
#define ENGINE_CTRL_13          0x03006794u  // <- 0
#define ENGINE_CTRL_14          0x03006710u  // <- -1
#define ENGINE_CTRL_15          0x0300675Cu  // <- 0
#define ENGINE_CTRL_16          0x030067ACu  // <- 0
#define ENGINE_CTRL_17          0x03006760u  // <- 0
#define ENGINE_CTRL_18          0x03006764u  // <- 0
#define ENGINE_CTRL_19          0x030067B4u  // <- 0x0852AD50
#define ENGINE_CTRL_20          0x03006714u  // <- 0
#define ENGINE_CTRL_21          0x03006720u  // <- 0
#define ENGINE_CTRL_22          0x03006770u  // 7 dwords <- 0x0852AD50

// Vectores/punteros constantes del motor.
#define ENGINE_PTR_A            0x0845A91Cu  // escrito en 0x030067D0
#define ENGINE_CONST_52AD50     0x0852AD50u  // valor repetido
#define ENGINE_CLEAR_BUF_ADDR   0x03004838u  // memset(addr, 0, 0x0845A918 << 2)
#define ENGINE_CLEAR_BUF_COUNT  0x0845A918u
#define ENGINE_RUNTIME_BUF_ADDR 0x03004EA0u  // memset(addr, 0, 0x8C)
#define ENGINE_RUNTIME_BUF_SIZE 0x8Cu

// Buffers limpiados por FUN_0862d524 / FUN_0862d5f8.
#define ENGINE_DYN_A            0x03004E88u  // [0] y [1] <- 0; luego 3 buffers de 0x60
#define ENGINE_BUF_X            0x03004DC8u  // memset 0x60
#define ENGINE_BUF_Y            0x03004E28u  // memset 0x60
#define ENGINE_BUF_Z            0x03004E94u  // <- 0

// ==============================================================================
// Logica portable
// ==============================================================================

// Portado de FUN_0862d154: defaults del bloque de estado del motor.
void engine_runtime_defaults(void)
{
    hw_engine_store32(ENGINE_STATE_B, ENGINE_STATE_A);  // 0x030068AC <- 0x030068A0
    hw_engine_store32(ENGINE_STATE_A, 0);
    hw_engine_store32(ENGINE_STATE_C, 0);
    hw_engine_store32(ENGINE_STATE_D, 0);
    hw_engine_store32(ENGINE_STATE_E, 0);
    hw_engine_store32(ENGINE_STATE_F, 2);
    hw_engine_store32(ENGINE_STATE_G, 0);
}

// Portado de FUN_0862d5f8: limpieza de buffers dinamicos del motor.
static void engine_runtime_clear_dynamic(void)
{
    hw_engine_store32(ENGINE_BUF_Z, 0);

    // El original comprueba si los punteros de los slots [0] y [1] de
    // ENGINE_DYN_A son no nulos y, en ese caso, hace memset(p, 0, 0x60).
    // Como el buffer de trabajo ya fue limpiado a 0 justo antes, modelamos
    // la limpieza sobre los buffers conocidos del motor.
    hw_engine_memset(ENGINE_DYN_A, 0, 0x60u);
    hw_engine_memset(ENGINE_DYN_A + 4u, 0, 0x60u);
    hw_engine_memset(ENGINE_BUF_X, 0, 0x60u);
    hw_engine_memset(ENGINE_BUF_Y, 0, 0x60u);
}

// Portado de FUN_0862d524.
void engine_runtime_clear_buffers(void)
{
    hw_engine_store32(ENGINE_DYN_A, 0);
    hw_engine_store32(ENGINE_DYN_A + 4u, 0);
    engine_runtime_clear_dynamic();
}

// Portado de FUN_0862a028: estado/buffers del runtime (sin I/O).
void engine_runtime_state_init(void)
{
    engine_runtime_defaults();
    engine_runtime_clear_buffers();

    // Puntero constante del motor.
    hw_engine_store32(ENGINE_STATE_A, ENGINE_PTR_A);

    // Bloque de control: -1 donde toca, 0 en el resto.
    hw_engine_store32(ENGINE_CTRL_0, 0xFFFFFFFFu);
    hw_engine_store32(ENGINE_CTRL_0, 0xFFFFFFFFu);  // el original lo repite
    hw_engine_store32(ENGINE_CTRL_1, 0xFFFFFFFFu);
    hw_engine_store32(ENGINE_CTRL_2, 0);
    hw_engine_store32(ENGINE_CTRL_3, 0xFFFFFFFFu);
    hw_engine_store32(ENGINE_CTRL_4, 0);
    hw_engine_store32(ENGINE_CTRL_5, 0);
    hw_engine_store32(ENGINE_CTRL_6, 0);
    hw_engine_store32(ENGINE_CTRL_7, 0);
    hw_engine_store32(ENGINE_CTRL_8, 0);
    hw_engine_store32(ENGINE_CTRL_9, 0);
    hw_engine_store32(ENGINE_CTRL_10, 0);
    hw_engine_store32(ENGINE_CTRL_11, 0);
    hw_engine_store32(ENGINE_CTRL_12, 0);
    hw_engine_store32(ENGINE_CTRL_13, 0);
    hw_engine_store32(ENGINE_CTRL_14, 0xFFFFFFFFu);
    hw_engine_store32(ENGINE_CTRL_15, 0);
    hw_engine_store32(ENGINE_CTRL_16, 0);
    hw_engine_store32(ENGINE_CTRL_17, 0);
    hw_engine_store32(ENGINE_CTRL_18, 0);
    hw_engine_store32(ENGINE_CTRL_19, ENGINE_CONST_52AD50);
    hw_engine_store32(ENGINE_CTRL_20, 0);
    hw_engine_store32(ENGINE_CTRL_21, 0);

    // 7 dwords consecutivos <- 0x0852AD50 (desde 0x03006770).
    for (u32 i = 0; i < 7u; i++) {
        hw_engine_store32(ENGINE_CTRL_22 + i * 4u, ENGINE_CONST_52AD50);
    }

    // memset(0x03004838, 0, 0x0845A918 << 2).
    hw_engine_memset(ENGINE_CLEAR_BUF_ADDR, 0,
                     (ENGINE_CLEAR_BUF_COUNT << 2));

    // memset del buffer de runtime (0x8C bytes).
    hw_engine_memset(hw_engine_runtime_buffer(), 0, ENGINE_RUNTIME_BUF_SIZE);
}

// Portado de FUN_08629ebc: instala la vtable y arranca el estado del runtime.
void engine_runtime_init(void)
{
    // 1) Vtable: *slot = valor (25 pares).
    for (u32 i = 0; i < ENGINE_VTABLE_COUNT; i++) {
        hw_engine_store32(g_engine_vtable_slots[i], g_engine_vtable_values[i]);
    }

    // 2) Init del estado del runtime.
    engine_runtime_state_init();
}

// ==============================================================================
// Implementacion de la capa de plataforma
// ==============================================================================
#ifdef TARGET_GBA

void hw_engine_store32(u32 addr, u32 value)
{
    *(vu32 *)addr = value;
}

void hw_engine_memset(u32 addr, u8 value, u32 size)
{
    volatile u8 *p = (volatile u8 *)addr;
    for (u32 i = 0; i < size; i++) {
        p[i] = value;
    }
}

u32 hw_engine_runtime_buffer(void)
{
    return ENGINE_RUNTIME_BUF_ADDR;
}

#else // ===================== PC / SDL2 =====================

#include <stdio.h>
#include <string.h>

// Buffer virtual que simula el bloque de RAM del motor en IWRAM.
// Rango cubierto: 0x03004838 .. 0x030068B8. Base = 0x03004000, tamano 0x3000.
#define ENGINE_RAM_BASE  0x03004000u
#define ENGINE_RAM_SIZE  0x3000u

static u8 g_engine_ram[ENGINE_RAM_SIZE];

static int engine_addr_valid(u32 addr, u32 size)
{
    return (addr >= ENGINE_RAM_BASE) &&
           ((addr + size) <= (ENGINE_RAM_BASE + ENGINE_RAM_SIZE));
}

void hw_engine_store32(u32 addr, u32 value)
{
    if (!engine_addr_valid(addr, 4u)) {
        // Fuera del bloque modelado: se ignora (punteros a ROM / otras RAM).
        return;
    }
    *(u32 *)(g_engine_ram + (addr - ENGINE_RAM_BASE)) = value;
}

void hw_engine_memset(u32 addr, u8 value, u32 size)
{
    if (!engine_addr_valid(addr, size)) {
        return;
    }
    memset(g_engine_ram + (addr - ENGINE_RAM_BASE), value, size);
}

u32 hw_engine_runtime_buffer(void)
{
    // En PC el buffer de runtime del motor tambien vive en el bloque virtual.
    return ENGINE_RUNTIME_BUF_ADDR;
}

#endif // TARGET_GBA
