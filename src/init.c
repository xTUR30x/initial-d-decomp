// ==============================================================================
// src/init.c - Arranque del juego (crt0 + AgbMain), portable GBA / PC
// ------------------------------------------------------------------------------
// Reproduce la secuencia real verificada en la ROM:
//
//   0x08000000  b AgbMain
//   0x080000C0  AgbMain:
//       1. *pDispStatSave = REG_DISPSTAT      (guarda estado de display)
//       2. REG_WAITCNT = 0xC5B4
//       3. Setup de stacks (IRQ 0x03007FA0 / User 0x03007F00)  [solo GBA]
//       4. *(0x03007FFC) = IntrMain (0x03000000)
//       5. 12 copias DMA3 ROM -> IWRAM (rutinas + handler IRQ)
//       6. FUN_0802604c() / FUN_0801b27c() / FUN_08629ebc()
//       7. *(0x03004098 + 4) = 0x80
//       8. FUN_08026e0c(); (*fn)()        (arranque del motor)
//       9. bucle IF/IE hasta DISPSTAT != 0
//      10. REG_DISPCNT = 8; REG_IE = 0x2001; REG_IME = 1
//      11. jump-table  pc = 0x080002F8 + state*4
//
// La E/S de hardware vive tras hw_*(), implementadas de forma distinta por
// target. El flujo de alto nivel es idéntico en GBA y en PC.
// ==============================================================================

#include "global.h"
#include "init.h"
#include "memory.h"
#include "save.h"
#include "engine.h"
#include "graphics.h"
#include "audio.h"
#include "states.h"

// ------------------------------------------------------------------------------
// Registros de E/S (solo se usan en la implementación GBA)
// ------------------------------------------------------------------------------
// Las direcciones de los registros de video y de DMA3 estan definidas en
// include/graphics.h (fuente unica de verdad); aqui solo se declaran los
// que usa el arranque y que no pertenecen a ese modulo.
// ------------------------------------------------------------------------------
#ifdef TARGET_GBA
#define REG_WAITCNT   (*(vu16 *)0x04000204)
#define REG_IE        (*(vu16 *)0x04000200)
#define REG_IME       (*(vu16 *)0x04000208)
#define REG_IF        (*(vu16 *)0x04000128)
#define REG_SND_CLEAR (*(vu16 *)0x04000134)

// Punteros a RAM/ROM referenciados por el crt0 (en la ROM original).
// El destino real en IWRAM de la guardado de DISPSTAT es 0x030027C4
// (puntero almacenado en la palabra de ROM 0x080003A8).
#define GBA_DISPSTAT_SAVE_ADDR 0x030027C4u
#define PTR_BOOT_FLAG       (*(vu32 *)0x080003DC)  // -> 0x03004098 (IWRAM)
#define PTR_BOOT_CALLBACK   (*(vu32 *)0x080003E0)  // -> 0x08629EBD (Thumb)
#define PTR_GAME_STATE      (*(vu32 *)0x080003E4)  // -> 0x030027AC (IWRAM)
#endif // TARGET_GBA

// ------------------------------------------------------------------------------
// Tabla de copias DMA3 ROM -> IWRAM (12 entradas, verificado con REA)
// ------------------------------------------------------------------------------
const DmaCopyDesc g_dma_copy_table[] = {
    { 0x03000000u, 0x08000ADCu, 0x040u },  // handler de IRQ
    { 0x03000100u, 0x0802625Cu, 0x140u },
    { 0x03000600u, 0x0800133Cu, 0x0C0u },
    { 0x03000900u, 0x08000FFCu, 0x080u },
    { 0x03000B00u, 0x08000F10u, 0x080u },
    { 0x03000D00u, 0x08001C08u, 0x080u },
    { 0x03000F00u, 0x0802F588u, 0x080u },
    { 0x03001100u, 0x080266BCu, 0x1A0u },
    { 0x03001780u, 0x0800157Cu, 0x180u },
    { 0x03001D80u, 0x08001D4Cu, 0x040u },
    { 0x03001E80u, 0x08026D34u, 0x040u },
    { 0x03001F80u, 0x08000E88u, 0x040u },
};
const u32 g_dma_copy_count = sizeof(g_dma_copy_table) / sizeof(g_dma_copy_table[0]);

// ==============================================================================
// Flujo de arranque portable
// ==============================================================================

void game_boot(void)
{
    // 1) Estado de CPU + stacks + DISPSTAT + waitstates.
    hw_boot_cpu_setup();

    // 2) Copias del runtime a IWRAM (incluye handler de IRQ).
    hw_boot_dma_copies();

    // 3) Inicialización temprana del motor.
    hw_boot_engine_init();

    // 4) Flag de arranque del juego.
    hw_boot_set_flag(GBA_BOOT_FLAG_ADDR, GBA_BOOT_FLAG_VALUE);

    // 5) Arranque del motor y callback inicial.
    hw_boot_start_engine();

    // 6) Espera a que el sistema esté listo.
    hw_wait_for_boot_ready();

    // 7) Configuración final de interrupciones y pantalla.
    hw_setup_interrupts_and_display();

    // 8) Prepara la maquina de estados (indice en 0x030027AC, tabla de saltos).
    states_init();

    // 9) Salto al estado inicial del juego (jump-table). No retorna en GBA.
    hw_jump_to_game_state(0);
}

void game_start(void)
{
    game_boot();
}

// ==============================================================================
// Implementación GBA (bare-metal)
// ==============================================================================
#ifdef TARGET_GBA

void hw_boot_cpu_setup(void)
{
    // El crt0 original fija modo IRQ (sp=0x03007FA0) y modo User (sp=0x03007F00)
    // manipulando CPSR. En C bare-metal confiamos en el vector de arranque del
    // linker/CRT; aquí solo replicamos la parte de E/S observable.
    // El crt0 original lee el puntero 0x080003A8 (que vale 0x030027C4) y
    // guarda ahi el DISPSTAT; en nuestra ROM el destino es la direccion real.
    *(vu32 *)GBA_DISPSTAT_SAVE_ADDR = *(vu16 *)REG_DISPSTAT;
    REG_WAITCNT = 0xC5B4u;
    *(vu32 *)GBA_INTR_VECTOR_ADDR = GBA_IRQ_HANDLER_ADDR;
}

void hw_boot_dma_copies(void)
{
    for (u32 i = 0; i < g_dma_copy_count; i++) {
        // DMA3: origen, destino, numero de palabras y control (32 bits, inmediato).
        hw_gfx_dma3_copy((const void *)g_dma_copy_table[i].src,
                         g_dma_copy_table[i].dst,
                         (u32)(g_dma_copy_table[i].size / 4u));
    }
}

void hw_boot_engine_init(void)
{
    // FUN_0802604c: limpia IWRAM alto + VRAM -> memory_clear_boot_buffers().
    // FUN_0801b27c: init del sistema de guardado (SRAM) -> save_init().
    // FUN_0863bfd4: init del sistema de sonido -> audio_system_init().
    //   (invocada via thunk 0x0863da1c; entry Thumb 0x0863bfd5)
    memory_clear_boot_buffers();
    save_init();
    audio_system_init();
}

void hw_boot_set_flag(u32 addr, u32 value)
{
    *(vu32 *)(addr + 4u) = value;
}

void hw_boot_start_engine(void)
{
    // FUN_08026e0c: aparca los sprites no usados fuera de pantalla (OAM).
    graphics_hide_oam_sprites();

    // Callback de arranque: *PTR_BOOT_CALLBACK = 0x08629EBD (Thumb) = FUN_08629ebc,
    // que es el init del runtime del motor -> engine_runtime_init().
    engine_runtime_init();
}

void hw_wait_for_boot_ready(void)
{
    do {
        REG_IF = 0;
        REG_SND_CLEAR = 0;
    } while (*(vu16 *)REG_DISPSTAT == 0);
}

void hw_setup_interrupts_and_display(void)
{
    *(vu16 *)REG_DISPCNT = 8;      // forced blank
    REG_IE = 0x2001u;
    REG_IME = 1;
}

void hw_jump_to_game_state(u32 state)
{
    // Equivale al dispatcher original del cartucho (0x080002E8):
    //   el indice vive en 0x030027AC y la ranura de la tabla se calcula
    //   como 0x080002F8 + indice*4 (misma aritmetica, ver states.c).
    // En GBA el despacho es un bucle cooperativo: no retorna, igual que el
    // 'b 0x080002E8' original.
    states_set_current(state);
    states_dispatch();
}

#else // ===================== PC / SDL2 =====================

#include <stdio.h>

void hw_boot_cpu_setup(void)
{
    printf("[boot] cpu setup: DISPSTAT save, WAITCNT=0xC5B4, IntrMain=0x%08X\n",
           GBA_IRQ_HANDLER_ADDR);
}

void hw_boot_dma_copies(void)
{
    printf("[boot] %u bloques DMA3 ROM->IWRAM a copiar (stub PC)\n",
           (unsigned)g_dma_copy_count);
}

void hw_boot_engine_init(void)
{
    printf("[boot] engine init (memory_clear / save_init / audio_system_init)\n");
    // FUN_0802604c -> memory_clear_boot_buffers() es logica portable.
    memory_clear_boot_buffers();
    printf("[boot]   memory_clear_boot_buffers() -> IWRAM alto + VRAM a cero\n");
    // FUN_0801b27c -> save_init() es logica portable.
    int has_save = save_init();
    printf("[boot]   save_init() -> save %s\n", has_save ? "valido" : "reinicializado");
    // FUN_0863bfd4 -> audio_system_init() es logica portable (registros stub).
    audio_system_init();
    hw_audio_debug_dump();
}

void hw_boot_set_flag(u32 addr, u32 value)
{
    printf("[boot] *0x%08X = 0x%08X\n",
           (unsigned)(addr + 4u), (unsigned)value);
}

void hw_boot_start_engine(void)
{
    printf("[boot] arranque del motor (FUN_08026e0c: OAM) + callback inicial\n");
    // FUN_08026e0c -> graphics_hide_oam_sprites() es logica portable.
    graphics_hide_oam_sprites();
    hw_oam_debug_dump();
    // Callback = Funcion 0x08629EBD (Thumb) -> engine_runtime_init().
    engine_runtime_init();
    printf("[boot]   callback inicial -> engine_runtime_init() (vtable + estado)\n");
}

void hw_wait_for_boot_ready(void)
{
    printf("[boot] espera de boot ready (stub PC)\n");
}

void hw_setup_interrupts_and_display(void)
{
    printf("[boot] DISPCNT=forced blank, IE=0x2001, IME=1 (stub PC)\n");
}

void hw_jump_to_game_state(u32 state)
{
    const GameStateDesc *desc = states_get_desc(state);
    u32 mismatches = states_verify_table();

    if (mismatches == 0u) {
        printf("[boot] tabla de estados verificada: %u entradas, 0 discrepancias\n",
               (unsigned)states_count());
    } else {
        printf("[boot] tabla de estados: %u discrepancias!\n", (unsigned)mismatches);
    }

    if (desc != NULL) {
        printf("[boot] estado inicial: #%02u %s (rom=0x%08X, clase=%s)\n",
               (unsigned)desc->index,
               desc->name,
               (unsigned)desc->rom_handler,
               states_class_name(desc->screen_class));
    }

    // Tabla completa de estados (44) con su clasificacion y transiciones.
    states_dump_table();

    // En PC el dispatcher no bloquea: el bucle SDL llama a states_tick().
    states_set_current(state);
}

#endif // TARGET_GBA
