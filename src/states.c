// ==============================================================================
// src/states.c - Maquina de estados del juego (Game State Machine)
// ------------------------------------------------------------------------------
// Subsistema: LOGICA / CONTROL DE FLUJO (maquina de estados).
//
// Portado del dispatcher del cartucho verificado con REA + objdump sobre
// baserom.gba (SHA-256 b1684b7d6a678189011bb607e9f2c23febe5b9062303e8afe56cba43246a1592).
//
// Mecanica original (ARM, region de arranque del crt0):
//
//   0x080002E8  ldr   r3, [pc, #244]   @ 0x080003E4   ; r3 = 0x030027AC
//   0x080002EC  add   r2, pc, #4                      ; r2 = 0x080002F8 (tabla)
//   0x080002F0  ldr   r1, [r3]                       ; r1 = *0x030027AC (indice)
//   0x080002F4  add   pc, r2, r1, lsl #2              ; salta a la ranura
//
// La tabla 0x080002F8..0x080003A4 NO contiene punteros: contiene 44
// instrucciones ARM 'b' (0xEA......) que reenvian al handler de cada estado.
// Equivale a:  goto handler_de_la_tabla[indice].
//
// El modelo es COOPERATIVO: cada handler escribe el nuevo indice en
// 0x030027AC y vuelve al dispatcher con 'b 0x080002E8' (nunca retorna). En el
// original cada handler es un bucle propio con espera de frame que decide el
// cambio de estado en funcion de la entrada del usuario.
//
// Registro auxiliar verificado: 0x03004A5C es la "solicitud pendiente de
// cambio de estado". Los handlers cuya transicion no es constante copian
// *0x03004A5C -> 0x030027AC antes de volver al dispatcher.
//
// ---------------------------------------------------------------------------
// Diseno portable
// ---------------------------------------------------------------------------
//   - La tabla de estados vive en la macro GAME_STATE_LIST (include/states.h),
//     que es la unica fuente de verdad: de ella se derivan el enum de ids, la
//     tabla de descriptores y las funciones handler (una por estado).
//   - Portar una pantalla = rellenar el cuerpo de su funcion state_<NN>_<sym>.
//   - Toda la E/S (RAM real en GBA, RAM virtual en PC) pasa por hw_*.
//   - Se conservan las 44 palabras ARM 'b' de la tabla original para poder
//     verificar que el dispatcher C reproduce exactamente el del cartucho.
// ==============================================================================

#include "global.h"
#include "states.h"
#include "memory.h"
#include "intro.h"
#include "fade.h"

#include <stddef.h>   // NULL

#ifndef TARGET_GBA
#include <stdio.h>
#endif

// ==============================================================================
// Tabla de estados (generada desde GAME_STATE_LIST)
// ==============================================================================

#define GAME_STATE_DESC(id, sym, handler, mod, name, cls, flags, n1, n2) \
    { id, handler, mod, name, cls, flags, n1, n2 },

const GameStateDesc g_game_states[GAME_STATE_COUNT] = {
    GAME_STATE_LIST(GAME_STATE_DESC)
};

#undef GAME_STATE_DESC

// ------------------------------------------------------------------------------
// Tabla de saltos original: las 44 palabras ARM 'b' de 0x080002F8..0x080003A4.
// Se conservan como dato para verificar la simetria con el binario ARM:
//   destino = ranura + 8 + imm24 * 4   (imm24 con signo de 24 bits)
// ------------------------------------------------------------------------------
const u32 g_rom_state_branch[GAME_STATE_COUNT] = {
    0xEA0293F9u, /*  0 -> 0x080A52E4 */
    0xEA028EEBu, /*  1 -> 0x080A3EB0 */
    0xEA027411u, /*  2 -> 0x0809D34C */
    0xEA116942u, /*  3 -> 0x0845A814 */
    0xEA000071u, /*  4 -> 0x080004D4 */
    0xEA1508FBu, /*  5 -> 0x08542700 */
    0xEA007C91u, /*  6 -> 0x0801F55C */
    0xEA15404Au, /*  7 -> 0x08550444 */
    0xEA154B12u, /*  8 -> 0x08552F68 */
    0xEA0294D3u, /*  9 -> 0x080A5670 */
    0xEA006E2Fu, /* 10 -> 0x0801BBE4 */
    0xEA025A87u, /* 11 -> 0x08096D48 */
    0xEA02BD5Au, /* 12 -> 0x080AF898 */
    0xEA009AE9u, /* 13 -> 0x08026ED8 */
    0xEA007CB8u, /* 14 -> 0x0801F618 */
    0xEA025DAEu, /* 15 -> 0x080979F4 */
    0xEA0262A9u, /* 16 -> 0x08098DE4 */
    0xEA026F54u, /* 17 -> 0x0809C094 */
    0xEA02AA08u, /* 18 -> 0x080AAB68 */
    0xEA153430u, /* 19 -> 0x0854D40C */
    0xEA15365Du, /* 20 -> 0x0854DCC4 */
    0xEA1138B2u, /* 21 -> 0x0844E61C */
    0xEA113F94u, /* 22 -> 0x084501A8 */
    0xEA14EE32u, /* 23 -> 0x0853BC24 */
    0xEA028F8Cu, /* 24 -> 0x080A4190 */
    0xEA028FABu, /* 25 -> 0x080A4210 */
    0xEA02B491u, /* 26 -> 0x080AD5AC */
    0xEA113F8Fu, /* 27 -> 0x084501A8 */
    0xEA150174u, /* 28 -> 0x08540940 */
    0xEA15026Du, /* 29 -> 0x08540D28 */
    0xEA153DDEu, /* 30 -> 0x0854FAF0 */
    0xEA114FD2u, /* 31 -> 0x084542C4 */
    0xEA028CA0u, /* 32 -> 0x080A3600 */
    0xEA14FF3Cu, /* 33 -> 0x08540074 */
    0xEA113CEDu, /* 34 -> 0x0844F73C */
    0xEA006E16u, /* 35 -> 0x0801BBE4 */
    0xEA15388Cu, /* 36 -> 0x0854E5C0 */
    0xEA028CB2u, /* 37 -> 0x080A365C */
    0xEA02BD40u, /* 38 -> 0x080AF898 */
    0xEA02BD3Fu, /* 39 -> 0x080AF898 */
    0xEA029EA2u, /* 40 -> 0x080A7E28 */
    0xEA150272u, /* 41 -> 0x08540D6C */
    0xEA150260u, /* 42 -> 0x08540D28 */
    0xEA025D92u, /* 43 -> 0x080979F4 */
};

// ==============================================================================
// Handlers por estado
// ==============================================================================
// Cada estado tiene su propia funcion. Portar una pantalla consiste en
// sustituir el cuerpo de su funcion por el codigo real; el dispatcher no se
// toca. Todas delegan hoy en states_enter(), que aplica la transicion
// verificada en la ROM.

static void states_enter(u32 index);

// Nota: la macro se expande en una sola linea logica, asi que cada definicion
// termina en ';' (declaracion vacia) y no en ',' (que solo valeria dentro de
// una lista de inicializadores).
#define GAME_STATE_HANDLER(id, sym, handler, mod, name, cls, flags, n1, n2) \
    static void state_##id##_##sym(void) { states_enter(GAME_STATE_##id##_##sym); };

GAME_STATE_LIST(GAME_STATE_HANDLER)

#undef GAME_STATE_HANDLER

// Tabla de handlers (indice = indice del estado).
#define GAME_STATE_FN(id, sym, handler, mod, name, cls, flags, n1, n2) \
    state_##id##_##sym,

static const StateHandlerFn g_state_handlers[GAME_STATE_COUNT] = {
    GAME_STATE_LIST(GAME_STATE_FN)
};

#undef GAME_STATE_FN

// ==============================================================================
// Estado interno
// ==============================================================================

// Modo demostracion: un estado sin transicion verificada avanza al siguiente
// indice de la tabla para que la build PC recorra la tabla completa.
// Activo por defecto solo en PC (ver states_init).
static bool s_demo_advance = false;

// Frames retenidos para los estados con cuerpo propio antes de avanzar en el
// modo demostracion. Sin esto el montaje de una pantalla portada (paleta,
// VRAM, sprites) seria visible un solo frame. En GBA el modo demostracion
// esta desactivado, asi que el valor no tiene efecto.
#define CUSTOM_STATE_HOLD_FRAMES  120u

// Frames que lleva el estado actual en pantalla y ultimo indice atendido.
static u32 s_state_frames = 0;
static u32 s_last_index = GAME_STATE_INVALID;

// ==============================================================================
// API portable
// ==============================================================================

u32 states_count(void)
{
    return GAME_STATE_COUNT;
}

const GameStateDesc *states_get_desc(u32 index)
{
    if (index >= GAME_STATE_COUNT) {
        return NULL;
    }
    return &g_game_states[index];
}

const char *states_class_name(u8 screen_class)
{
    switch (screen_class) {
        case GAME_SCREEN_INTRO:  return "INTRO";
        case GAME_SCREEN_TITLE:  return "TITLE";
        case GAME_SCREEN_MENU:   return "MENU";
        case GAME_SCREEN_SAVE:   return "SAVE";
        case GAME_SCREEN_SCENE:  return "SCENE";
        case GAME_SCREEN_RACE:   return "RACE";
        case GAME_SCREEN_RESULT: return "RESULT";
        case GAME_SCREEN_MUSIC:  return "MUSIC";
        default:                 return "UNKNOWN";
    }
}

void states_set_demo_advance(bool enabled)
{
    s_demo_advance = enabled;
}

bool states_get_demo_advance(void)
{
    return s_demo_advance;
}

u32 states_get_current(void)
{
    return hw_state_index_read();
}

void states_set_current(u32 index)
{
    if (index >= GAME_STATE_COUNT) {
        return;
    }
    hw_state_index_write(index);
}

u32 states_get_pending(void)
{
    return hw_state_pending_read();
}

void states_set_pending(u32 index)
{
    if (index >= GAME_STATE_COUNT) {
        return;
    }
    hw_state_pending_write(index);
}

void states_request(u32 index)
{
    if (index >= GAME_STATE_COUNT) {
        return;
    }
    // Equivale al original: *0x030027AC = nuevo_indice; b 0x080002E8.
    states_set_current(index);
    hw_state_return_to_dispatcher();
}

void states_init(void)
{
    states_set_current(0);
    states_set_pending(GAME_STATE_INVALID);

#ifdef TARGET_GBA
    s_demo_advance = false;
#else
    s_demo_advance = true;
#endif
}

// ------------------------------------------------------------------------------
// Cuerpo generico de un estado.
// ------------------------------------------------------------------------------
// En la ROM cada handler:
//   1. monta la pantalla (DISPCNT, paleta, VRAM, sprites),
//   2. entra en su bucle propio esperando frame e input,
//   3. decide la transicion: escribe el indice y hace 'b 0x080002E8'.
//
// Por eso, si el estado tiene cuerpo propio (GAME_STATE_FLAG_CUSTOM), ese
// cuerpo se ejecuta ANTES de la espera y de la transicion: es el equivalente
// al paso 1 del handler original.
// ------------------------------------------------------------------------------
static void states_enter(u32 index)
{
    const GameStateDesc *desc = states_get_desc(index);

    if (desc == NULL) {
        return;
    }

    // Contador de frames del estado actual (usado por el modo demostracion).
    if (index != s_last_index) {
        s_last_index = index;
        s_state_frames = 0;
    }
    s_state_frames++;

    hw_state_log(index);

    // Montaje propio de la pantalla (Estado 0 -> src/intro.c).
    states_run_custom_body(index);

    hw_state_frame_delay();

    // Modo demostracion (solo PC por defecto): barrido lineal de la tabla
    // completa, para poder ver los 44 estados y su clasificacion. Se ignora
    // la transicion verificada (que se sigue mostrando en la traza).
    if (s_demo_advance) {
        // Los estados con cuerpo propio se retienen unos frames para que su
        // montaje sea visible antes de continuar el barrido.
        if ((desc->flags & GAME_STATE_FLAG_CUSTOM) != 0u &&
            s_state_frames < CUSTOM_STATE_HOLD_FRAMES) {
            return;
        }
        states_set_current((index + 1u) % GAME_STATE_COUNT);
        s_state_frames = 0;
        return;
    }

    // Transicion constante verificada en la ROM.
    if (desc->next_state != GAME_STATE_INVALID) {
        states_set_current(desc->next_state);
        return;
    }
    if (desc->next_state2 != GAME_STATE_INVALID) {
        states_set_current(desc->next_state2);
        return;
    }

    // Transicion dinamica: el handler original copia *0x03004A5C al indice.
    // El valor 0 significa "sin solicitud pendiente" (cmp r1,#0 en la ROM).
    if ((desc->flags & GAME_STATE_FLAG_DYNAMIC) != 0u) {
        u32 pending = states_get_pending();
        if (pending != 0u && pending != GAME_STATE_INVALID && pending < GAME_STATE_COUNT) {
            states_set_current(pending);
            return;
        }
    }
}

// ------------------------------------------------------------------------------
// Cuerpos propios de los estados
// ------------------------------------------------------------------------------
// Un estado marcado con GAME_STATE_FLAG_CUSTOM delega su montaje en otro
// modulo, para que la logica de pantalla no se mezcle con la maquina de
// estados. Por ahora solo el Estado 0 (intro/logo) esta portado: su cuerpo
// vive en src/intro.c (intro_state_body) y reproduce el handler 0x080A52E4
// del cartucho.
//
// La transicion no se decide aqui: la aplica states_enter() leyendo la
// solicitud pendiente (0x03004A5C), igual que el handler original.
// ------------------------------------------------------------------------------
void states_run_custom_body(u32 index)
{
    const GameStateDesc *desc = states_get_desc(index);

    if (desc == NULL) {
        return;
    }

    if ((desc->flags & GAME_STATE_FLAG_CUSTOM) == 0u) {
        return;
    }

    switch (index) {
        case GAME_STATE_0_INTRO_LOGO:
            // Estado 0: intro/logo (handler ROM 0x080A52E4).
            intro_state_body();
            break;

        default:
            break;
    }
}

// ------------------------------------------------------------------------------
// Despacho
// ------------------------------------------------------------------------------

// Un paso: ejecuta el handler del estado actual.
void states_tick(void)
{
    u32 index = states_get_current();

    if (index >= GAME_STATE_COUNT) {
        index = 0;
        states_set_current(index);
    }

    g_state_handlers[index]();
}

// Despacho. En GBA es el equivalente del 'b 0x080002E8' original: el handler
// decide el cambio de estado escribiendo el indice y el bucle vuelve a
// despachar (nunca retorna, igual que el original).
void states_dispatch(void)
{
#ifdef TARGET_GBA
    for (;;) {
        u32 index = states_get_current();

        if (index >= GAME_STATE_COUNT) {
            index = 0;
            states_set_current(index);
        }

        g_state_handlers[index]();
    }
#else
    // En PC el bucle SDL llama a states_tick() en cada frame; aqui solo se
    // procesa un estado para no bloquear el render.
    states_tick();
#endif
}

// ==============================================================================
// Tabla de saltos original (simetria con el binario ARM)
// ==============================================================================

u32 states_rom_slot_address(u32 index)
{
    if (index >= GAME_STATE_COUNT) {
        return 0;
    }
    // Misma aritmetica que el dispatcher original:
    //   ranura = 0x080002F8 + indice * 4
    return GBA_STATE_TABLE_ADDR + (index << 2);
}

u32 states_rom_branch_target(u32 index)
{
    u32 slot;
    u32 word;
    s32 imm;

    if (index >= GAME_STATE_COUNT) {
        return 0;
    }

    slot = states_rom_slot_address(index);
    word = g_rom_state_branch[index];
    imm = (s32)(word & 0x00FFFFFFu);
    if ((imm & 0x00800000) != 0) {
        imm -= 0x01000000;   // signo de 24 bits
    }

    return (u32)((slot + 8u + (u32)((s32)imm * 4)) & 0xFFFFFFFFu);
}

// Comprueba que la tabla de saltos embebida y la tabla de descriptores
// coinciden con la ROM original. Devuelve el numero de discrepancias.
u32 states_verify_table(void)
{
    u32 mismatches = 0;
    u32 i;

    for (i = 0; i < GAME_STATE_COUNT; i++) {
        u32 target = states_rom_branch_target(i);
        if (target != g_game_states[i].rom_handler) {
            mismatches++;
        }
    }

    return mismatches;
}

#ifndef TARGET_GBA
void states_dump_table(void)
{
    u32 i;

    for (i = 0; i < GAME_STATE_COUNT; i++) {
        const GameStateDesc *d = &g_game_states[i];
        const char *n1 = (d->next_state  != GAME_STATE_INVALID) ? states_get_desc(d->next_state)->name  : "--";
        const char *n2 = (d->next_state2 != GAME_STATE_INVALID) ? states_get_desc(d->next_state2)->name : "--";

        printf("[states] #%02u %-30s rom=0x%08X  clase=%-7s  flags=0x%02X  next=%s / %s\n",
               (unsigned)d->index,
               d->name,
               (unsigned)d->rom_handler,
               states_class_name(d->screen_class),
               (unsigned)d->flags,
               n1,
               n2);
    }
}
#endif // !TARGET_GBA

// ==============================================================================
// Acceso al indice de estado y a la solicitud pendiente
// ==============================================================================
// El indice de estado vive en 0x030027AC y la solicitud pendiente en
// 0x03004A5C, ambos en la zona alta de IWRAM.
//
//   GBA: acceso directo a la RAM real.
//   PC : acceso al buffer virtual de IWRAM que mantiene src/memory.c, de modo
//        que la maquina de estados, el motor, los graficos y el modulo de
//        intro comparten una sola imagen de la memoria del juego.
// ------------------------------------------------------------------------------

u32 hw_state_index_read(void)
{
    return hw_mem_read32(GBA_GAME_STATE_ADDR);
}

void hw_state_index_write(u32 value)
{
    hw_mem_write32(GBA_GAME_STATE_ADDR, value);
}

u32 hw_state_pending_read(void)
{
    return hw_mem_read32(GBA_NEXT_STATE_ADDR);
}

void hw_state_pending_write(u32 value)
{
    hw_mem_write32(GBA_NEXT_STATE_ADDR, value);
}

// ==============================================================================
// Implementacion GBA (bare-metal)
// ==============================================================================
#ifdef TARGET_GBA

// Registros de E/S usados por la capa de plataforma.
#define REG_VCOUNT   (*(vu16 *)0x04000006)

// Retardo calibrado de ~1 frame (1/60 s). El arranque deja la pantalla en
// forced blank, asi que VCOUNT no avanza todavia: cuando se porte IntrMain
// (IRQ de VBlank) esta funcion pasara a sincronizarse con el VBlank real.
#define GBA_FRAME_DELAY_LOOPS  12000u

static void gba_delay_frame(void)
{
    volatile u32 loops = GBA_FRAME_DELAY_LOOPS;
    while (loops--) {
        __asm__ volatile("" ::: "memory");
    }
}

void hw_state_frame_wait(void)
{
    gba_delay_frame();

    // Equivalente al VBlank del cartucho (hasta que se porte IntrMain): el
    // motor de fundidos y la cola de paletas avanzan una vez por frame.
    fade_frame_update();
}

void hw_state_frame_delay(void)
{
    u32 i;
    for (i = 0; i < 30u; i++) {     // ~0.5 s de permanencia en el estado
        gba_delay_frame();
    }
}

void hw_state_log(u32 index)
{
    (void)index;   // sin traza en hardware
}

void hw_state_return_to_dispatcher(void)
{
    // Equivale al 'b 0x080002E8' original (no retorna en GBA).
    states_dispatch();
}

// ==============================================================================
// Implementacion PC / SDL2
// ==============================================================================
#else

#include <stdio.h>

void hw_state_frame_wait(void)
{
    // El bucle SDL de src/pc_main.c ya marca el ritmo de frames.
}

void hw_state_frame_delay(void)
{
    // En PC la permanencencia en el estado la controla el ritmo del bucle SDL.
}

void hw_state_log(u32 index)
{
    const GameStateDesc *d = states_get_desc(index);
    const char *nxt;

    if (d == NULL) {
        return;
    }

    if (d->next_state != GAME_STATE_INVALID) {
        nxt = states_get_desc(d->next_state)->name;
    } else if (d->next_state2 != GAME_STATE_INVALID) {
        nxt = states_get_desc(d->next_state2)->name;
    } else if ((d->flags & GAME_STATE_FLAG_DYNAMIC) != 0u) {
        nxt = "(dinamica: 0x03004A5C)";
    } else {
        nxt = "(sin transicion)";
    }

    printf("[states] #%02u %-30s rom=0x%08X clase=%-7s next=%s\n",
           (unsigned)d->index,
           d->name,
           (unsigned)d->rom_handler,
           states_class_name(d->screen_class),
           nxt);
}

void hw_state_return_to_dispatcher(void)
{
    // En PC el cambio de estado lo recoge el bucle SDL en el frame siguiente.
}

#endif // TARGET_GBA
