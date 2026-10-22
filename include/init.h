#ifndef GUARD_INIT_H
#define GUARD_INIT_H

#include "global.h"
#include "states.h"

// ==============================================================================
// src/init.c - Arranque del juego (crt0 + AgbMain)
// ------------------------------------------------------------------------------
// Este módulo reproduce, en C portable, la secuencia de arranque del cartucho:
//
//   0x08000000  b AgbMain              (entry point de la cabecera GBA)
//   0x080000C0  AgbMain                (setup de CPU, stacks, DMA, IRQ, estado)
//
// Toda la E/S de hardware (registros 0x0400xxxx, DMA3, copias a IWRAM) pasa por
// la capa de plataforma (hw_*) para poder compilar en GBA (bare-metal) y en PC
// (SDL2) con el mismo código de alto nivel.
// ==============================================================================

// ------------------------------------------------------------------------------
// Estado de arranque del cartucho (descubierto y verificado con REA)
// ------------------------------------------------------------------------------

// Vector IntrMain: la CPU copia aquí el handler de IRQ (0x03007FFC en IWRAM).
#define GBA_INTR_VECTOR_ADDR   0x03007FFCu
// Destino IWRAM de la rutina de IRQ copiada al arranque.
#define GBA_IRQ_HANDLER_ADDR   0x03000000u
// Stacks configurados por el crt0.
#define GBA_IRQ_STACK_TOP      0x03007FA0u
#define GBA_USER_STACK_TOP     0x03007F00u

// La maquina de estados (indice en 0x030027AC, tabla de saltos en 0x080002F8)
// vive en include/states.h, que es su unica fuente de verdad.
// Flag de arranque escrito como 0x80 en DAT_03004098+4.
#define GBA_BOOT_FLAG_ADDR     0x03004098u
#define GBA_BOOT_FLAG_VALUE    0x80u

// Copias DMA3 ROM -> IWRAM del crt0 (tabla de descriptores en 0x080003AC).
typedef struct {
    u32 dst;    // destino IWRAM
    u32 src;    // origen ROM
    u32 size;   // tamaño en bytes
} DmaCopyDesc;

extern const DmaCopyDesc g_dma_copy_table[];
extern const u32          g_dma_copy_count;

// ------------------------------------------------------------------------------
// Capa de plataforma (implementada en cada target: GBA y PC/SDL2)
// ------------------------------------------------------------------------------

// Recopila el estado de sistema del arranque real: instala los stacks, guarda
// DISPSTAT y configura los waitstates de la ROM.
void hw_boot_cpu_setup(void);

// Copia los bloques de código de ROM a IWRAM (rutinas de runtime + handler IRQ).
void hw_boot_dma_copies(void);

// Inicialización temprana del motor (FUN_0802604c, FUN_0801b27c, FUN_08629ebc).
void hw_boot_engine_init(void);

// Escribe el flag de arranque del juego (*(addr + 4) = value).
void hw_boot_set_flag(u32 addr, u32 value);

// Arranque del motor: FUN_08026e0c() + callback inicial del cartucho.
void hw_boot_start_engine(void);

// Habilita/deshabilita registro de interrupciones y pide blanqueo de pantalla.
void hw_setup_interrupts_and_display(void);

// Espera activa hasta que el sistema esté listo para arrancar el juego.
void hw_wait_for_boot_ready(void);

// Salta al estado inicial del juego (jump-table en ROM). No retorna en GBA.
void hw_jump_to_game_state(u32 state);

// ------------------------------------------------------------------------------
// Arranque portable (implementado en src/init.c)
// ------------------------------------------------------------------------------

// Secuencia completa del AgbMain. En GBA no retorna: cede el control al estado
// inicial del juego. En PC retorna y el bucle SDL toma el control.
void game_boot(void);

// Punto de entrada único compartido por ambos targets. Llama a game_boot().
// Los puntos de entrada específicos (src/pc_main.c, src/gba_main.c) lo invocan.
void game_start(void);

#endif // GUARD_INIT_H
