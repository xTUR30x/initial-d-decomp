#ifndef GUARD_SAVE_H
#define GUARD_SAVE_H

#include "global.h"

// ==============================================================================
// include/save.h - Sistema de guardado (SRAM) del cartucho
// ------------------------------------------------------------------------------
// Maneja el save de partida almacenado en SRAM (0x0E000000). El bloque de
// trabajo se lee a EWRAM y se valida contra una firma. Portado de FUN_0801b27c.
// ==============================================================================

// SRAM del cartucho (batería). 32 KB, mapeada en 0x0E000000.
#define SRAM_BASE_ADDR        0x0E000000u

// Buffer de trabajo del save en EWRAM (0x2004 bytes, verificado con REA).
#define SAVE_WORK_BUFFER_ADDR 0x02030000u
#define SAVE_WORK_BUFFER_SIZE 0x2004u

// Firma de cabecera del save: "INITIAL-D Another Stage for GBA" (8 dwords = 32 bytes).
extern const char g_save_signature[];
#define SAVE_SIGNATURE_DWORDS 8u

// Campos de estado de progreso dentro del buffer (bits de bandera).
#define SAVE_FLAG_A_OFFSET    0x0800u  // +0x800: bit 1 (0x2)
#define SAVE_FLAG_B_OFFSET    0x1000u  // +0x1000: bit 2 (0x4)
#define SAVE_FLAG_C_OFFSET    0x1800u  // +0x1800: bit 3 (0x8)

// Checksum del save (FUN_0801b4c8): suma de SAVE_CHECKSUM_BYTES bytes desde el
// inicio del buffer, comparada contra el dword almacenado justo a continuacion.
#define SAVE_CHECKSUM_BYTES   0x2000u  // 8192 bytes (0x800 iteraciones x 4)
#define SAVE_CHECKSUM_ADDR    0x02032000u  // buffer + 0x2000 (dword almacenado)

// Destinos en IWRAM del espejo de cabecera.
#define SAVE_MIRROR_NAME_ADDR 0x030044CCu  // 3 bytes (nombre/versión)
#define SAVE_MIRROR_A_ADDR    0x03004050u  // 2 dwords
#define SAVE_MIRROR_B_ADDR    0x03004058u  // 1 dword
#define SAVE_MIRROR_C_ADDR    0x030047D8u  // 1 dword

// Contador global limpiado al inicio (EWRAM).
#define SAVE_GLOBAL_COUNTER   0x02032004u

// ------------------------------------------------------------------------------
// Capa de plataforma (SRAM + temporización)
// ------------------------------------------------------------------------------

// Lee `size` bytes desde SRAM a `dst` (en EWRAM). Habilita SRAM vía WAITCNT.
void hw_sram_read(u32 src, void *dst, u32 size);

// Escribe `size` bytes a SRAM desde `src`. Verifica leyendo de vuelta.
// Devuelve 0 si tuvo éxito, distinto de 0 si falló.
int hw_sram_write(const void *src, u32 dst, u32 size);

// PC/SDL2: devuelve el buffer de trabajo real (simula EWRAM 0x02030000).
u8 *hw_save_work_buffer(void);

// PC/SDL2: espeja la cabecera del save (simula destinos IWRAM).
void hw_save_mirror_header(const u8 *work);

// ------------------------------------------------------------------------------
// API del sistema de guardado (implementado en src/save.c)
// ------------------------------------------------------------------------------

// Inicializa/carga el save. Portado de FUN_0801b27c.
// Devuelve 1 si el save es válido y se cargó, 0 si se reinicializó/falló.
int save_init(void);

// Comprueba si las cabeceras del buffer de trabajo coinciden con la firma.
// Devuelve true si las 8 dwords coinciden.
bool save_check_signature(const void *buffer);

// Portado de FUN_0801b4c8: verifica el checksum del save.
// Suma SAVE_CHECKSUM_BYTES bytes del buffer y los compara con el dword
// almacenado en el offset SAVE_CHECKSUM_BYTES. Devuelve true si coincide.
bool save_verify_checksum(const void *buffer);

// Inicializa el buffer de trabajo con valores por defecto y la firma.
void save_reset_defaults(void *buffer);

#endif // GUARD_SAVE_H
