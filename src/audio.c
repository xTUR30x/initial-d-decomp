// ==============================================================================
// src/audio.c - Inicializacion del sistema de sonido
// ------------------------------------------------------------------------------
// Portado de FUN_0863bfd4. Clasificado como SUBSISTEMA DE AUDIO.
//
// Original (verificado con REA en 0x0863BFD4, entry normal ARM):
//   FUN_0863d4c8(magic & ~1, 0x03005B30, 0x04000100);  // SWI 0x0B SoundDriverInit
//   FUN_0863c3d4(0x03006A20);                          // salida/mezcla
//   FUN_0863c290(0x03007A60);                          // canales
//   FUN_0863c570(0x0093D700);                          // config del mixer
//   count = (0x0863C040 & 0xffff) = 4;                 // pistas
//   for (i = 0; i < count; i++) {
//       d = &desc[i];                    // 12 bytes por descriptor
//       FUN_0863c714(d->handle, d->data, (u8)d->param);
//       *(u8 *)(d->handle + 0x0B) = *(u16 *)((u8*)d + 0x0A);
//       *(u32 *)(d->handle + 0x18) = 0x03007C20;
//   }
//
// Registros de sonido tocados (por las sub-inicializaciones):
//   0x04000080 SOUNDCNT_L, 0x04000084 SOUNDCNT_X, 0x040000C6 BIAS,
//   0x04000063 SOUND4CNT_H, 0x040000BC WAVE_RAM, 0x040000A0/A4 FIFO.
//   Area del driver del BIOS en 0x03007FF0 (firma "Smsh" = 0x68736D53).
//
// En PC todo el driver del BIOS y los registros se simulan con stubs; la LOGICA
// de orquestacion (las 4 llamadas + el bucle de pistas) se ejecuta tal cual.
// ==============================================================================

#include "global.h"
#include "audio.h"
#include "memory.h"

// ------------------------------------------------------------------------------
// Tabla de pistas (4 descriptores de 12 bytes, en ROM 0x08648B08).
// Cada descriptor: handle(IWRAM), data, param(dword) + extra(halfword @ +0x0A).
// ------------------------------------------------------------------------------
const AudioTrackDesc g_audio_tracks[AUDIO_TRACK_COUNT] = {
    { 0x03007B60u, 0x03005220u, 0x0000000Au, 0x0000u },
    { 0x03007BA0u, 0x03005540u, 0x00000008u, 0x0000u },
    { 0x03007BE0u, 0x030057C0u, 0x0000000Au, 0x0000u },
    { 0x03007C30u, 0x03005AE0u, 0x00010001u, 0x0100u },
};

// ------------------------------------------------------------------------------
// Tabla de temas BGM (12 descriptores en ROM 0x080005A4).
// Usada por FUN_0800055c: el indice es el numero de pista y el valor es el
// puntero a los datos de la cancion (0 = pista inexistente).
// ------------------------------------------------------------------------------
const u32 g_audio_bgm_table[BGM_TABLE_COUNT] = {
    0x00000000u,  //  0
    0x08754734u,  //  1
    0x00000000u,  //  2
    0x08754F44u,  //  3
    0x08756168u,  //  4
    0x08757194u,  //  5
    0x08757D30u,  //  6
    0x08757FE0u,  //  7
    0x0875855Cu,  //  8
    0x087589B4u,  //  9
    0x08758EA4u,  // 10
    0x087593B0u,  // 11
};

// Destino IWRAM escrito en el campo +0x18 de cada pista.
#define AUDIO_TRACK_TIMEBASE  0x03007C20u

// ==============================================================================
// Sub-inicializaciones portadas
// ==============================================================================

// Portado de FUN_0863c3d4: config de la salida maestro / mezclador.
void audio_init_output(void)
{
    // 0x04000084 SOUNDCNT_X <- 0x8F (master enable, PSG/FIFO a todo volumen).
    hw_sound_reg8(REG_SOUNDCNT_X, 0x8Fu);
    // 0x04000080 SOUNDCNT_L <- 0xA90E (volumen L/R).
    hw_sound_reg16(REG_SOUNDCNT_L, 0xA90Eu);
    // 0x040000C4..C6 SOUNDBIAS <- 0x84400004 (bias del DAC).
    hw_sound_reg16(REG_SOUNDBIAS, 0x4000u);
    // Canal de salida FIFO/DMA y wave RAM.
    hw_sound_reg16(REG_WAVE_RAM0, 0x0400u);
    hw_sound_reg16(REG_FIFO_A, 0x0400u);
}

// Portado de FUN_0863c290: config de canales PSG.
void audio_init_channels(void)
{
    hw_sound_reg8(REG_SOUNDCNT_X, 0x8Fu);
    // 0x04000080..: barrido/volumen de los 4 canales PSG.
    hw_sound_reg16(REG_SOUNDCNT_L, 0x0000u);
    hw_sound_reg8(REG_SOUND4CNT_H, 0x08u);
    hw_sound_reg8(REG_SOUNDCNT_X, 0x77u);
}

// Portado de FUN_0863c570: config del mezclador.
void audio_init_mixer(u32 cfg)
{
    if ((cfg & 0xFFu) != 0u) {
        hw_sound_reg8(REG_SOUNDCNT_X, (u8)(cfg & 0x7Fu));
    }
    if ((cfg & 0xF00u) != 0u) {
        hw_sound_reg8(REG_SOUNDCNT_X, (u8)((cfg & 0xF00u) >> 8));
    }
    if ((cfg & 0xF000u) != 0u) {
        hw_sound_reg8(REG_SOUNDBIAS, (u8)((cfg & 0xF000u) >> 12));
    }
}

// Portado de FUN_0863c714: config de una pista del driver.
void audio_init_track(u32 handle, u32 data, u8 param)
{
    if (param == 0u) {
        return;
    }
    if (param > 0x10u) {
        param = 0x10u;
    }
    hw_audio_track_setup(handle, data, param);
}

// ==============================================================================
// API principal
// ==============================================================================

// Portado de FUN_0863bfd4.
void audio_system_init(void)
{
    // 1) Init del driver de sonido del BIOS (SWI 0x0B).
    //    handler = (magic & ~1); work area = 0x03005B30; arg = 0x04000100.
    hw_sound_driver_init(SOUND_DRIVER_MAGIC & ~1u, 0x03005B30u, 0x04000100u);

    // 2) Sub-inicializaciones (salida, canales, mezclador).
    audio_init_output();
    audio_init_channels();
    audio_init_mixer(AUDIO_MIXER_CONFIG);

    // 3) Arranca las 4 pistas de musica/efectos.
    for (u32 i = 0; i < AUDIO_TRACK_COUNT; i++) {
        const AudioTrackDesc *d = &g_audio_tracks[i];
        audio_init_track(d->handle, d->data, (u8)(d->param & 0xFFu));

        // *(u8 *)(handle + 0x0B) = halfword extra del descriptor.
        hw_audio_store8(d->handle + 0x0Bu, (u8)(d->extra & 0xFFu));
        // *(u32 *)(handle + 0x18) = 0x03007C20 (timebase).
        hw_audio_store32(d->handle + 0x18u, AUDIO_TRACK_TIMEBASE);
    }
}

// ==============================================================================
// Selector de BGM (FUN_0800055c / FUN_080005d4)
// ==============================================================================
//
// Original (verificado con objdump):
//
//   0x0800055c  r5 = 0x080005A4 + pista*4
//   0x08000564  r4 = tabla[pista]
//   0x08000568  movs r4, r4            ; actualiza los flags
//   0x0800056c  if (r4 == 0) return    ; pista inexistente
//   0x08000570  r5 = 0x030027B4        ; "pista actual"
//   0x08000574  r6 = *r5
//   0x08000578  if (r4 == r6) return   ; ya suena
//   0x08000580  *r5 = r4               ; "pista actual" = nueva
//   0x0800058c  r1 = r4 ; r0 = 0x03007B60
//   0x08000594  bl 0x0863da28          ; driver: reproducir
//
//   0x080005d4  r0 = 0x03007B60
//   0x080005e4  bl 0x0863da34          ; driver: detener
//   0x080005f0  *(0x030027B4) = -1     ; "pista actual" = ninguna
//
// El driver de sonido (0x0863c78c / 0x0863c871) vive en la ROM del
// cartucho original y no forma parte de este port: la gestion de la pista
// actual si se reproduce de forma exacta.

// Portado de FUN_0800055c.
void audio_bgm_play(u32 track)
{
    if (track >= BGM_TABLE_COUNT) {
        return;
    }

    const u32 song = g_audio_bgm_table[track];

    if (song == 0u) {
        return;   // pista inexistente
    }

    const u32 current = hw_mem_read32(BGM_CURRENT_SONG_ADDR);

    if (song == current) {
        return;   // ya es la pista que suena
    }

    hw_mem_write32(BGM_CURRENT_SONG_ADDR, song);
    hw_audio_bgm_play(BGM_DRIVER_HANDLE, song);
}

// Portado de FUN_080005d4.
void audio_bgm_stop(void)
{
    hw_audio_bgm_stop(BGM_DRIVER_HANDLE);
    hw_mem_write32(BGM_CURRENT_SONG_ADDR, 0xFFFFFFFFu);
}

// ==============================================================================
// Implementacion de la capa de plataforma
// ==============================================================================
#ifdef TARGET_GBA

// Punteros a las entradas del BIOS para invocar SWI 0x0B.
void hw_sound_driver_init(u32 handler, u32 work_area, u32 arg)
{
    (void)handler; (void)work_area; (void)arg;
    // SWI 0x0B = SoundDriverInit. Los argumentos reales van por el area de
    // trabajo apuntada por r0/r1/r2; aqui se invoca el vector del BIOS.
    register u32 r0_arg asm("r0") = handler;
    register u32 r1_arg asm("r1") = work_area;
    register u32 r2_arg asm("r2") = arg;
    (void)r0_arg; (void)r1_arg; (void)r2_arg;
    __asm__ volatile("svc 0x0b\n" ::: "r0", "r1", "r2", "memory");
}

void hw_sound_reg16(u32 reg, u16 value) { *(vu16 *)reg = value; }
void hw_sound_reg8(u32 reg, u8 value)   { *(vu8 *)reg = value; }

void hw_audio_track_setup(u32 handle, u32 data, u8 param)
{
    // Modelo simplificado: deja el estado inicial de la pista.
    *(vu8 *)(handle + 0x08u) = param;
    *(vu32 *)(handle + 0x2Cu) = data;
    *(vu32 *)(handle + 0x04u) = 0x80000000u;
}

void hw_audio_store8(u32 addr, u8 value)  { *(vu8 *)addr = value; }
void hw_audio_store32(u32 addr, u32 value) { *(vu32 *)addr = value; }

// El driver de sonido (0x0863c78c / 0x0863c871) forma parte de la ROM del
// cartucho original, que no se incluye en este port: la pista queda
// registrada en el estado del juego pero no se reproduce audio real.
void hw_audio_bgm_play(u32 handle, u32 song)
{
    (void)handle; (void)song;
}

void hw_audio_bgm_stop(u32 handle)
{
    (void)handle;
}

#else // ===================== PC / SDL2 =====================

#include <stdio.h>
#include <string.h>

// Registros de sonido simulados (solo para trazabilidad en PC).
static u8  g_sound_regs[0x100];
// Area del driver de sonido (simula 0x03007FF0 y las estructuras de pista).
static u8  g_sound_work[0x400];

void hw_sound_driver_init(u32 handler, u32 work_area, u32 arg)
{
    (void)handler; (void)work_area; (void)arg;
    // Marca la firma del driver como si el BIOS lo hubiera inicializado.
    *(u32 *)g_sound_work = SOUND_DRIVER_MAGIC;
    printf("[audio] SWI 0x0B SoundDriverInit (stub PC), magic=\"Smsh\"\n");
}

void hw_sound_reg16(u32 reg, u16 value)
{
    // En PC solo registramos los registros conocidos de sonido.
    if (reg >= 0x04000060u && reg < 0x04000100u) {
        *(u16 *)(g_sound_regs + (reg - 0x04000060u)) = value;
    }
}

void hw_sound_reg8(u32 reg, u8 value)
{
    if (reg >= 0x04000060u && reg < 0x04000100u) {
        *(u8 *)(g_sound_regs + (reg - 0x04000060u)) = value;
    }
}

void hw_audio_track_setup(u32 handle, u32 data, u8 param)
{
    (void)handle; (void)data; (void)param;
    // En PC no hay estado real de pista; se registra la llamada.
}

void hw_audio_store8(u32 addr, u8 value)   { (void)addr; (void)value; }
void hw_audio_store32(u32 addr, u32 value)  { (void)addr; (void)value; }

// El driver de sonido no existe en PC: solo se registra la peticion.
void hw_audio_bgm_play(u32 handle, u32 song)
{
    printf("[audio] BGM play: handle=0x%08X song=0x%08X (stub PC)\n",
           (unsigned)handle, (unsigned)song);
}

void hw_audio_bgm_stop(u32 handle)
{
    printf("[audio] BGM stop: handle=0x%08X (stub PC)\n", (unsigned)handle);
}

// Volcado de depuracion (solo PC).
void hw_audio_debug_dump(void)
{
    printf("[audio] SOUNDCNT_L=0x%04X SOUNDCNT_X=0x%02X BIAS=0x%04X XOR=0x%02X\n",
           *(const u16 *)(g_sound_regs + (0x04000080u - 0x04000060u)),
           *(const u8  *)(g_sound_regs + (0x04000084u - 0x04000060u)),
           *(const u16 *)(g_sound_regs + (0x04000088u - 0x04000060u)),
           *(const u8  *)(g_sound_regs + (0x04000089u - 0x04000060u)));
    printf("[audio] %u pistas inicializadas\n", (unsigned)AUDIO_TRACK_COUNT);
}

#endif // TARGET_GBA
