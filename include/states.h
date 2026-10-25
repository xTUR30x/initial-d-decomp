#ifndef GUARD_STATES_H
#define GUARD_STATES_H

#include "global.h"

// ==============================================================================
// include/states.h - Maquina de estados del juego (Game State Machine)
// ------------------------------------------------------------------------------
// Portado del dispatcher de estados del cartucho (verificado con REA/objdump
// sobre baserom.gba, SHA-256 b1684b7d6a678189011bb607e9f2c23febe5b9062303e8afe56cba43246a1592).
//
// Mecanica original (ARM, region de arranque):
//
//   0x080002E8  ldr   r3, [pc, #244]      @ 0x080003E4 ; r3 = 0x030027AC
//   0x080002EC  add   r2, pc, #4                  ; r2 = 0x080002F8 (tabla)
//   0x080002F0  ldr   r1, [r3]                   ; r1 = *0x030027AC (indice)
//   0x080002F4  add   pc, r2, r1, lsl #2          ; salta a la ranura
//
// La "jump-table" en 0x080002F8..0x080003A4 no contiene punteros: contiene 44
// instrucciones ARM 'b' (0xEA......) que reenvian al handler de cada estado.
// Por eso el dispatcher equivale a:  goto handler_de_la_tabla[indice].
//
// El modelo es COOPERATIVO: cada handler escribe el nuevo indice en
// 0x030027AC y vuelve al dispatcher con 'b 0x080002E8' (no retorna). El
// handler decide el cambio de estado en funcion de la entrada del usuario,
// por lo que en el original cada handler es un bucle con espera de frame.
//
// Registro auxiliar descubierto: 0x03004A5C es el "solicitud de cambio de
// estado" global. Los handlers cuya transicion no es una constante la leen
// de ahi (copian *0x03004A5C -> 0x030027AC) antes de volver al dispatcher.
//
// Este modulo reproduce la tabla y el dispatcher de forma portable:
//   - GBA: lee el indice de 0x030027AC real y salta al handler C portado,
//     conservando la misma aritmetica (ranura = 0x080002F8 + indice*4).
//   - PC : el indice vive en un bloque de RAM virtual y el dispatcher llama
//     a un handler C por estado, de forma modular (una funcion por estado).
// ==============================================================================

// ------------------------------------------------------------------------------
// Direcciones verificadas en la ROM
// ------------------------------------------------------------------------------

// IWRAM: indice del estado actual (puntero en ROM: 0x080003E4).
#define GBA_GAME_STATE_ADDR   0x030027ACu
// IWRAM: solicitud pendiente de cambio de estado.
#define GBA_NEXT_STATE_ADDR   0x03004A5Cu
// ROM: jump-table de estados (44 entradas 'b'), 0x080002F8..0x080003A4.
#define GBA_STATE_TABLE_ADDR  0x080002F8u
// Numero de estados registrados en la tabla.
#define GAME_STATE_COUNT      44u
// Valor centinela: "sin transicion conocida".
#define GAME_STATE_INVALID    0xFFu

// ------------------------------------------------------------------------------
// Clasificacion de pantallas (evidencia estructural, no texto)
// ------------------------------------------------------------------------------
// El juego no almacena nombres de pantalla en texto plano (la tipografia usa
// una codificacion propia), por lo que la clasificacion se apoya en:
//   * la familia de direcciones ROM del handler (modulo del juego),
//   * las subrutinas que invoca (widgets de menu 0x080987xx, guardado
//     0x0801B528, selector de BGM 0x0800055C, paleta por DMA, ...),
//   * los registros de RAM que toca (0x0203xxxx = datos guardados),
//   * el grafo de transiciones verificado.
typedef enum {
    GAME_SCREEN_UNKNOWN = 0,  // sin clasificar
    GAME_SCREEN_INTRO,        // splash / logo de arranque
    GAME_SCREEN_TITLE,        // titulo / vuelta al titulo
    GAME_SCREEN_MENU,         // menu de seleccion
    GAME_SCREEN_SAVE,         // guardado / carga / records
    GAME_SCREEN_SCENE,        // escena o cutscene de historia
    GAME_SCREEN_RACE,         // modulo de carrera (0x0853xxx..0x0855xxx)
    GAME_SCREEN_RESULT,       // resultados / ranking / tabla de tiempos
    GAME_SCREEN_MUSIC,        // pseudo-estato: selector de BGM del cursor
    GAME_SCREEN_CLASS_COUNT
} GameScreenClass;

// ------------------------------------------------------------------------------
// Flags de estado (evidencia observable en el handler)
// ------------------------------------------------------------------------------
#define GAME_STATE_FLAG_NONE        0x00u
#define GAME_STATE_FLAG_SAVE_DATA   0x01u  // toca 0x0203xxxx (SRAM / EWRAM de save)
#define GAME_STATE_FLAG_MUSIC       0x02u  // invoca el selector de BGM 0x0800055C
#define GAME_STATE_FLAG_PALETTE_DMA 0x04u  // copia la paleta por DMA3
#define GAME_STATE_FLAG_SHARED      0x08u  // comparte handler con otro estado
#define GAME_STATE_FLAG_DYNAMIC     0x10u  // su transicion se lee de 0x03004A5C
#define GAME_STATE_FLAG_CUSTOM      0x20u  // tiene cuerpo propio en otro modulo

// ------------------------------------------------------------------------------
// Descriptor de un estado
// ------------------------------------------------------------------------------
typedef struct {
    u32          index;        // indice en la jump-table (0..43)
    u32          rom_handler;  // direccion ROM del handler (destino del 'b')
    u32          rom_module;   // base de la familia de codigo que lo contiene
    const char  *name;         // etiqueta descriptiva
    u8           screen_class; // GameScreenClass
    u8           flags;        // GAME_STATE_FLAG_*
    u8           next_state;   // transicion verificada (GAME_STATE_INVALID si no)
    u8           next_state2;  // transicion alternativa verificada
} GameStateDesc;

// ------------------------------------------------------------------------------
// Tabla de estados (44 entradas). Es la unica fuente de verdad del modulo:
// de aqui se derivan el enum de identificadores, la tabla de descriptores y
// las funciones handler (una por estado, generadas con la misma macro).
//
// Campos por estado:
//   X(indice, simbolo, handler_rom, modulo_rom, nombre, clase, flags,
//     transicion_1, transicion_2)
// ------------------------------------------------------------------------------
#define GAME_STATE_LIST(X)                                                                     \
    X( 0, INTRO_LOGO,   0x080A52E4u, 0x080A5000u, "Intro/Logo",                GAME_SCREEN_INTRO,   GAME_STATE_FLAG_CUSTOM | GAME_STATE_FLAG_DYNAMIC, GAME_STATE_INVALID, GAME_STATE_INVALID) \
    X( 1, TITLE_MENU,   0x080A3EB0u, 0x080A3000u, "Menu principal",            GAME_SCREEN_MENU,    GAME_STATE_FLAG_MUSIC | GAME_STATE_FLAG_CUSTOM,       23u,  GAME_STATE_INVALID) \
    X( 2, MODE_SELECT,  0x0809D34Cu, 0x0809D000u, "Seleccion de modo",         GAME_SCREEN_MENU,    GAME_STATE_FLAG_DYNAMIC,      9u,  GAME_STATE_INVALID) \
    X( 3, SCENE_LOAD,   0x0845A814u, 0x0845A000u, "Carga de escena",           GAME_SCREEN_SCENE,   GAME_STATE_FLAG_DYNAMIC,      2u,  GAME_STATE_INVALID) \
    X( 4, MENU_BGM,     0x080004D4u, 0x08000400u, "BGM del cursor de menu",    GAME_SCREEN_MUSIC,   GAME_STATE_FLAG_MUSIC,       GAME_STATE_INVALID, GAME_STATE_INVALID) \
    X( 5, RACE_LOAD,    0x08542700u, 0x08542000u, "Carga de carrera",          GAME_SCREEN_RACE,    GAME_STATE_FLAG_DYNAMIC,     GAME_STATE_INVALID, GAME_STATE_INVALID) \
    X( 6, SAVE_SLOT,    0x0801F55Cu, 0x0801F500u, "Guardado - pantalla",       GAME_SCREEN_SAVE,    GAME_STATE_FLAG_SAVE_DATA,   36u,  35u) \
    X( 7, TRACK_LOAD,   0x08550444u, 0x08550000u, "Carga de pista",            GAME_SCREEN_RACE,    GAME_STATE_FLAG_NONE,         2u,  GAME_STATE_INVALID) \
    X( 8, RACE_VIEW,    0x08552F68u, 0x08552000u, "Vista de carrera",          GAME_SCREEN_RACE,    GAME_STATE_FLAG_MUSIC | GAME_STATE_FLAG_DYNAMIC, GAME_STATE_INVALID, GAME_STATE_INVALID) \
    X( 9, MODE_SUBMENU, 0x080A5670u, 0x080A5000u, "Sub-menu de modo",          GAME_SCREEN_MENU,    GAME_STATE_FLAG_MUSIC,       40u,  2u) \
    X(10, SAVE_DATA,    0x0801BBE4u, 0x0801B000u, "Datos guardados",           GAME_SCREEN_SAVE,    GAME_STATE_FLAG_SAVE_DATA,   36u,  GAME_STATE_INVALID) \
    X(11, OPTION_MENU,  0x08096D48u, 0x08096000u, "Menu de opciones",          GAME_SCREEN_MENU,    GAME_STATE_FLAG_PALETTE_DMA | GAME_STATE_FLAG_DYNAMIC, GAME_STATE_INVALID, GAME_STATE_INVALID) \
    X(12, RECORD_SCR,   0x080AF898u, 0x080AF000u, "Records y datos",           GAME_SCREEN_SAVE,    GAME_STATE_FLAG_SAVE_DATA | GAME_STATE_FLAG_MUSIC | GAME_STATE_FLAG_DYNAMIC, GAME_STATE_INVALID, GAME_STATE_INVALID) \
    X(13, RANKING,      0x08026ED8u, 0x08026000u, "Ranking",                   GAME_SCREEN_RESULT,  GAME_STATE_FLAG_MUSIC | GAME_STATE_FLAG_DYNAMIC, GAME_STATE_INVALID, GAME_STATE_INVALID) \
    X(14, SAVE_CONFIRM, 0x0801F618u, 0x0801F600u, "Confirmacion de guardado",  GAME_SCREEN_SAVE,    GAME_STATE_FLAG_SAVE_DATA,   13u,  GAME_STATE_INVALID) \
    X(15, MODE_LIST,    0x080979F4u, 0x08097000u, "Lista de modos",            GAME_SCREEN_MENU,    GAME_STATE_FLAG_DYNAMIC,      29u,  GAME_STATE_INVALID) \
    X(16, SLOT_LIST,    0x08098DE4u, 0x08098D00u, "Lista de slots",            GAME_SCREEN_MENU,    GAME_STATE_FLAG_DYNAMIC,      6u,  GAME_STATE_INVALID) \
    X(17, CHAR_SELECT,  0x0809C094u, 0x0809C000u, "Seleccion de personaje",    GAME_SCREEN_MENU,    GAME_STATE_FLAG_DYNAMIC,      36u,  GAME_STATE_INVALID) \
    X(18, OPTION_SUB,   0x080AAB68u, 0x080AA000u, "Sub-menu de opciones",      GAME_SCREEN_MENU,    GAME_STATE_FLAG_MUSIC | GAME_STATE_FLAG_DYNAMIC, GAME_STATE_INVALID, GAME_STATE_INVALID) \
    X(19, RACE_PREP,    0x0854D40Cu, 0x0854D000u, "Carrera - preparacion",     GAME_SCREEN_RACE,    GAME_STATE_FLAG_SAVE_DATA | GAME_STATE_FLAG_MUSIC | GAME_STATE_FLAG_DYNAMIC, GAME_STATE_INVALID, GAME_STATE_INVALID) \
    X(20, RACE_QUEUE,   0x0854DCC4u, 0x0854D000u, "Carrera - lista de espera", GAME_SCREEN_RACE,    GAME_STATE_FLAG_MUSIC | GAME_STATE_FLAG_DYNAMIC, 8u,  GAME_STATE_INVALID) \
    X(21, STORY_SCENE,  0x0844E61Cu, 0x0844E000u, "Escena de historia",        GAME_SCREEN_SCENE,   GAME_STATE_FLAG_MUSIC | GAME_STATE_FLAG_DYNAMIC, GAME_STATE_INVALID, GAME_STATE_INVALID) \
    X(22, SCENE_SETUP,  0x084501A8u, 0x08450000u, "Setup de escena",           GAME_SCREEN_SCENE,   GAME_STATE_FLAG_DYNAMIC,     GAME_STATE_INVALID, GAME_STATE_INVALID) \
    X(23, TIMETABLE,    0x0853BC24u, 0x0853B000u, "Tabla de tiempos",          GAME_SCREEN_RESULT,  GAME_STATE_FLAG_SAVE_DATA,    2u,  GAME_STATE_INVALID) \
    X(24, MENU_A,       0x080A4190u, 0x080A4000u, "Menu A",                    GAME_SCREEN_MENU,    GAME_STATE_FLAG_NONE,        28u,  33u) \
    X(25, MENU_B,       0x080A4210u, 0x080A4000u, "Menu B",                    GAME_SCREEN_MENU,    GAME_STATE_FLAG_NONE,        33u,  GAME_STATE_INVALID) \
    X(26, MENU_C,       0x080AD5ACu, 0x080AD000u, "Menu C",                    GAME_SCREEN_MENU,    GAME_STATE_FLAG_MUSIC | GAME_STATE_FLAG_DYNAMIC, GAME_STATE_INVALID, GAME_STATE_INVALID) \
    X(27, SCENE_SETUP2, 0x084501A8u, 0x08450000u, "Setup de escena (2)",       GAME_SCREEN_SCENE,   GAME_STATE_FLAG_SHARED | GAME_STATE_FLAG_DYNAMIC, GAME_STATE_INVALID, GAME_STATE_INVALID) \
    X(28, RACE_START,   0x08540940u, 0x08540000u, "Carrera - salida",          GAME_SCREEN_RACE,    GAME_STATE_FLAG_SAVE_DATA | GAME_STATE_FLAG_MUSIC, 8u,  GAME_STATE_INVALID) \
    X(29, RACE_COUNT,   0x08540D28u, 0x08540D00u, "Carrera - cuenta atras",    GAME_SCREEN_RACE,    GAME_STATE_FLAG_DYNAMIC,     GAME_STATE_INVALID, GAME_STATE_INVALID) \
    X(30, RACE_RESULT,  0x0854FAF0u, 0x0854FA00u, "Carrera - resultado",       GAME_SCREEN_RESULT,  GAME_STATE_FLAG_NONE,        43u,  GAME_STATE_INVALID) \
    X(31, COURSE_SEL,   0x084542C4u, 0x08454000u, "Seleccion de escenario",    GAME_SCREEN_MENU,    GAME_STATE_FLAG_NONE,         2u,  GAME_STATE_INVALID) \
    X(32, ROOT_MENU,    0x080A3600u, 0x080A3000u, "Menu raiz",                 GAME_SCREEN_MENU,    GAME_STATE_FLAG_DYNAMIC,     GAME_STATE_INVALID, GAME_STATE_INVALID) \
    X(33, RACE_SAVE,    0x08540074u, 0x08540000u, "Carrera - guardar partida", GAME_SCREEN_RACE,    GAME_STATE_FLAG_SAVE_DATA | GAME_STATE_FLAG_DYNAMIC, GAME_STATE_INVALID, GAME_STATE_INVALID) \
    X(34, ATTRACT,      0x0844F73Cu, 0x0844F000u, "Intro de escena",           GAME_SCREEN_SCENE,   GAME_STATE_FLAG_PALETTE_DMA,  8u,  GAME_STATE_INVALID) \
    X(35, SAVE_DATA2,   0x0801BBE4u, 0x0801B000u, "Datos guardados (2)",        GAME_SCREEN_SAVE,    GAME_STATE_FLAG_SAVE_DATA | GAME_STATE_FLAG_SHARED, 36u, GAME_STATE_INVALID) \
    X(36, RACE_SIDE,    0x0854E5C0u, 0x0854E000u, "Carrera - vista lateral",   GAME_SCREEN_RACE,    GAME_STATE_FLAG_MUSIC | GAME_STATE_FLAG_DYNAMIC, GAME_STATE_INVALID, GAME_STATE_INVALID) \
    X(37, MENU_CONFIRM, 0x080A365Cu, 0x080A3000u, "Menu - confirmacion",       GAME_SCREEN_MENU,    GAME_STATE_FLAG_DYNAMIC,     GAME_STATE_INVALID, GAME_STATE_INVALID) \
    X(38, RECORD_SCR2,  0x080AF898u, 0x080AF000u, "Records y datos (2)",        GAME_SCREEN_SAVE,    GAME_STATE_FLAG_SAVE_DATA | GAME_STATE_FLAG_MUSIC | GAME_STATE_FLAG_SHARED | GAME_STATE_FLAG_DYNAMIC, GAME_STATE_INVALID, GAME_STATE_INVALID) \
    X(39, RECORD_SCR3,  0x080AF898u, 0x080AF000u, "Records y datos (3)",        GAME_SCREEN_SAVE,    GAME_STATE_FLAG_SAVE_DATA | GAME_STATE_FLAG_MUSIC | GAME_STATE_FLAG_SHARED | GAME_STATE_FLAG_DYNAMIC, GAME_STATE_INVALID, GAME_STATE_INVALID) \
    X(40, RETURN_TITLE, 0x080A7E28u, 0x080A7000u, "Volver al titulo",          GAME_SCREEN_TITLE,   GAME_STATE_FLAG_MUSIC,        0u,  GAME_STATE_INVALID) \
    X(41, RACE_VARIANT, 0x08540D6Cu, 0x08540D00u, "Carrera - variante",        GAME_SCREEN_RACE,    GAME_STATE_FLAG_NONE,         6u,  GAME_STATE_INVALID) \
    X(42, RACE_COUNT2,  0x08540D28u, 0x08540D00u, "Carrera - cuenta atras (2)", GAME_SCREEN_RACE,    GAME_STATE_FLAG_SHARED | GAME_STATE_FLAG_DYNAMIC, GAME_STATE_INVALID, GAME_STATE_INVALID) \
    X(43, MODE_LIST2,   0x080979F4u, 0x08097000u, "Lista de modos (2)",         GAME_SCREEN_MENU,    GAME_STATE_FLAG_SHARED | GAME_STATE_FLAG_DYNAMIC, 29u, GAME_STATE_INVALID)

// Identificadores enumerados de cada estado.
#define GAME_STATE_ENUM(id, sym, handler, mod, name, cls, flags, n1, n2) \
    GAME_STATE_##id##_##sym = id,
typedef enum {
    GAME_STATE_LIST(GAME_STATE_ENUM)
    GAME_STATE_TOTAL = GAME_STATE_COUNT
} GameStateId;

// Tabla de descriptores (44 entradas).
extern const GameStateDesc g_game_states[GAME_STATE_COUNT];

// ------------------------------------------------------------------------------
// API portable (implementada en src/states.c)
// ------------------------------------------------------------------------------

// Prepara la maquina de estados: indice a 0 y verifica la tabla de saltos.
void states_init(void);

// Indice del estado actual (leido de 0x030027AC en GBA).
u32  states_get_current(void);

// Escribe el indice del estado actual (0x030027AC en GBA).
void states_set_current(u32 index);

// Solicitud pendiente de cambio de estado (0x03004A5C en GBA).
u32  states_get_pending(void);
void states_set_pending(u32 index);

// Despacha el estado actual.
//   GBA: bucle cooperativo, equivalente al 'b 0x080002E8' original; no retorna.
//   PC : procesa un unico estado y retorna (el bucle SDL llama a states_tick).
void states_dispatch(void);

// Un paso de la maquina de estados (PC): ejecuta el estado actual una vez.
void states_tick(void);

// Pide un cambio de estado: escribe el indice y vuelve al dispatcher.
// Equivale al original: *0x030027AC = nuevo; b 0x080002E8.
void states_request(u32 index);

// Ejecuta el cuerpo propio del estado (GAME_STATE_FLAG_CUSTOM), si lo tiene.
// El Estado 0 (intro/logo) delega aqui su montaje de pantalla, implementado
// en src/intro.c a partir del handler 0x080A52E4 del cartucho.
void states_run_custom_body(u32 index);

// Descriptor de un estado (NULL si el indice es invalido).
const GameStateDesc *states_get_desc(u32 index);

// Numero de estados registrados.
u32  states_count(void);

// Nombre legible de una clase de pantalla.
const char *states_class_name(u8 screen_class);

// Vuelca la tabla de estados por consola (depuracion; solo PC).
#ifndef TARGET_GBA
void states_dump_table(void);
#endif

// ------------------------------------------------------------------------------
// Tabla de saltos original (simetria con el binario ARM)
// ------------------------------------------------------------------------------

// Las 44 palabras ARM 'b' de la tabla original (0x080002F8..0x080003A4).
// Se conservan para poder verificar que la tabla C reproduce exactamente el
// dispatcher del cartucho.
extern const u32 g_rom_state_branch[GAME_STATE_COUNT];

// Destino ROM de la ranura 'index' de la tabla original.
u32 states_rom_branch_target(u32 index);

// Direccion de la ranura 'index' en la tabla original (0x080002F8 + index*4).
u32 states_rom_slot_address(u32 index);

// Comprueba que la tabla de saltos embebida coincide con g_game_states.
// Devuelve el numero de discrepancias (0 = correcto).
u32 states_verify_table(void);

// ------------------------------------------------------------------------------
// Capa de plataforma (implementada en cada target: GBA y PC/SDL2)
// ------------------------------------------------------------------------------

// Lectura / escritura del indice de estado.
u32  hw_state_index_read(void);
void hw_state_index_write(u32 value);

// Lectura / escritura de la solicitud pendiente de cambio de estado.
u32  hw_state_pending_read(void);
void hw_state_pending_write(u32 value);

// Espera de frame: en GBA sincroniza con VBlank, en PC no hace nada
// (el bucle SDL ya marca el ritmo).
void hw_state_frame_wait(void);

// Espera previa a la transicion de estado (~0.5 s en GBA, instantanea en PC).
void hw_state_frame_delay(void);

// Traza del estado actual (solo informativo; no-op en GBA).
void hw_state_log(u32 index);

// Vuelta al dispatcher: en GBA equivale al 'b 0x080002E8' original (no
// retorna); en PC no hace nada (el bucle SDL recoge el cambio).
void hw_state_return_to_dispatcher(void);

// Modo demostracion: si esta activo, un estado sin transicion verificada
// avanza al siguiente indice para que la build PC recorra la tabla completa.
// Se activa por defecto solo en PC.
void states_set_demo_advance(bool enabled);
bool states_get_demo_advance(void);

// ------------------------------------------------------------------------------
// Handlers por estado
// ------------------------------------------------------------------------------
// Portar una pantalla consiste en rellenar el cuerpo de su funcion handler.
// Cada estado tiene una funcion propia (state_<NN>_<simbolo>) generada desde
// GAME_STATE_LIST; todas delegan hoy en el cuerpo generico states_enter(),
// que sera sustituido por el codigo real de cada pantalla.
typedef void (*StateHandlerFn)(void);

// ------------------------------------------------------------------------------
// Alias del handler del Estado 0
// ------------------------------------------------------------------------------
// El Estado 0 se genera como 'state_0_INTRO_LOGO' a partir de GAME_STATE_LIST.
// Su cuerpo real vive en src/intro.c (intro_state_body), asi que el alias solo
// es valido dentro de src/states.c.
#define state_00_intro_logo state_0_INTRO_LOGO

#endif // GUARD_STATES_H
