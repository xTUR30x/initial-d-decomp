#ifndef GUARD_AUDIO_H
#define GUARD_AUDIO_H

#include "global.h"

// ==============================================================================
// include/audio.h - Inicializacion del sistema de sonido
// ------------------------------------------------------------------------------
// Portado de FUN_0863bfd4. Configura el driver de sonido del BIOS (GBA SWI 0x0B
// SoundDriverInit), los registros de sonido (SOUNDCNT_L/H, SOUND4CNT, BIAS,
// FIFO) y arranca 4 pistas de musica/efectos.
//
// Subsistema: AUDIO. Es el callback de arranque del motor (*PTR_BOOT_CALLBACK),
// invocado desde hw_boot_start_engine().
// ==============================================================================

// ------------------------------------------------------------------------------
// Registros de sonido del GBA (solo se usan en la implementacion GBA)
// ------------------------------------------------------------------------------
#define REG_SOUNDCNT_L      0x04000080u  // volumen maestro L/R
#define REG_SOUNDCNT_H      0x04000082u  // volumen / ruteo (byte alto en 0x84)
#define REG_SOUNDCNT_X      0x04000084u  // master enable
#define REG_SOUNDBIAS       0x04000088u  // bias del DAC
#define REG_SOUND4CNT_H     0x04000063u  // canal 4 (ruido)
#define REG_WAVE_RAM0       0x04000090u  // wave RAM / FIFO
#define REG_FIFO_A          0x040000A0u
#define REG_FIFO_B          0x040000A4u

// Area de trabajo del driver de sonido del BIOS en la parte alta de IWRAM.
#define SOUND_DRIVER_WORK   0x03007FF0u
// Firma del driver ("Smsh" = 0x68736D53).
#define SOUND_DRIVER_MAGIC  0x68736D53u

// Numero de pistas inicializadas en el bucle de descriptores.
#define AUDIO_TRACK_COUNT   4u
// Valor de configuracion pasado a FUN_0863c570.
#define AUDIO_MIXER_CONFIG  0x0093D700u

// ------------------------------------------------------------------------------
// Descriptor de pista: handle (IWRAM) + datos + longitud de config.
// ------------------------------------------------------------------------------
typedef struct {
    u32 handle;   // puntero al estado de la pista en IWRAM
    u32 data;     // puntero a los datos de la pista
    u32 param;    // parametro / longitud
    u16 extra;    // halfword extra (leyenda en el handler)
} AudioTrackDesc;

extern const AudioTrackDesc g_audio_tracks[AUDIO_TRACK_COUNT];

// ------------------------------------------------------------------------------
// Selector de BGM (FUN_0800055c / FUN_080005d4)
// ------------------------------------------------------------------------------
// Tabla de 12 descriptores de pista en ROM 0x080005A4. El indice 0 y el 2
// estan vacios (0x00000000) y el resto apuntan a los datos de cada tema.
#define BGM_TABLE_COUNT        12u
// Variable "pista actual" en IWRAM (0x030027B4).
#define BGM_CURRENT_SONG_ADDR  0x030027B4u
// Handle del driver de sonido usado para el BGM (0x03007B60).
#define BGM_DRIVER_HANDLE       0x03007B60u

extern const u32 g_audio_bgm_table[BGM_TABLE_COUNT];

// ------------------------------------------------------------------------------
// Capa de plataforma (implementado en cada target: GBA y PC/SDL2)
// ------------------------------------------------------------------------------

// Inicializa el driver de sonido del BIOS (GBA: SWI 0x0B SoundDriverInit).
// En PC es un stub.
void hw_sound_driver_init(u32 handler, u32 work_area, u32 arg);

// Escribe un halfword en un registro de sonido.
void hw_sound_reg16(u32 reg, u16 value);

// Escribe un byte en un registro de sonido.
void hw_sound_reg8(u32 reg, u8 value);

// Configura una pista del mixer (equivalente a FUN_0863c3d4/c290/c570/c714).
void hw_audio_track_setup(u32 handle, u32 data, u8 param);

// Escrituras de estado de pista (helper de la logica portable).
void hw_audio_store8(u32 addr, u8 value);
void hw_audio_store32(u32 addr, u32 value);

// Invoca al driver de sonido para reproducir una pista (FUN_0800055c ->
// thunk 0x0863da28 -> driver Thumb 0x0863c78c).
//   handle: estado de la pista en IWRAM (0x03007B60)
//   song  : puntero a los datos de la cancion
void hw_audio_bgm_play(u32 handle, u32 song);

// Invoca al driver de sonido para detener el BGM (FUN_080005d4 ->
// thunk 0x0863da34 -> driver Thumb 0x0863c871).
void hw_audio_bgm_stop(u32 handle);

#ifndef TARGET_GBA
// Solo PC: volcado de depuracion de los registros de sonido simulados.
void hw_audio_debug_dump(void);
#endif

// ------------------------------------------------------------------------------
// API de audio (implementado en src/audio.c)
// ------------------------------------------------------------------------------

// Portado de FUN_0863bfd4. Configura el driver, los registros y las pistas.
void audio_system_init(void);

// Sub-inicializaciones portadas.
void audio_init_output(void);      // FUN_0863c3d4
void audio_init_channels(void);    // FUN_0863c290
void audio_init_mixer(u32 cfg);    // FUN_0863c570
void audio_init_track(u32 handle, u32 data, u8 param);  // FUN_0863c714

// Portado de FUN_0800055c: reproduce la pista BGM indicada (1..11).
// No hace nada si la pista no existe (entrada 0 de la tabla) o si ya suena.
void audio_bgm_play(u32 track);

// Portado de FUN_080005d4: detiene el BGM y marca "sin pista" (-1).
void audio_bgm_stop(void);

#endif // GUARD_AUDIO_H
