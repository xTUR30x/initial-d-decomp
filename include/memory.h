#ifndef GUARD_MEMORY_H
#define GUARD_MEMORY_H

#include "global.h"

// ==============================================================================
// include/memory.h - Utilidades de memoria, buffers de hardware y ventanas
// ------------------------------------------------------------------------------
// Portado de FUN_0802604c. Limpia la zona alta de IWRAM y toda la VRAM.
//
// Ademas expone una capa de acceso a la memoria mapeada del GBA (IWRAM, VRAM
// y PAL RAM) para que los modulos portables (graficos, audio, intro) puedan
// escribir en el hardware con el mismo codigo en los dos targets:
//   * GBA: puntero directo a la direccion real.
//   * PC : buffer virtual equivalente.
// ==============================================================================

// Regiones limpiadas por memory_clear_boot_buffers().
#define BOOT_CLEAR_IWRAM_BASE  0x03002080u  // zona alta de IWRAM
#define BOOT_CLEAR_IWRAM_WORDS 0x5E7u       // iteraciones de 16 bytes (24176 bytes)
#define BOOT_CLEAR_VRAM_BASE   0x06000000u  // VRAM completa (96 KB)
#define BOOT_CLEAR_VRAM_WORDS  0x1800u      // iteraciones de 16 bytes (98304 bytes)

// ------------------------------------------------------------------------------
// Regiones de hardware mapeadas
// ------------------------------------------------------------------------------
#define GBA_IWRAM_BASE    0x03000000u  // IWRAM: 32 KB (0x03000000..0x03007FFF)
#define GBA_IWRAM_SIZE    0x00008000u
#define GBA_VRAM_BASE     0x06000000u  // VRAM: 96 KB (0x06000000..0x06017FFF)
#define GBA_VRAM_SIZE     0x00018000u
#define GBA_PALRAM_BASE   0x05000000u  // PAL RAM: 1 KB (0x05000000..0x050003FF)
#define GBA_PALRAM_SIZE   0x00000400u

// ------------------------------------------------------------------------------
// Capa de plataforma (acceso a memoria del sistema)
// ------------------------------------------------------------------------------

// Pone a cero `dwords` palabras de 32 bits en `addr`.
void hw_memzero_dwords(u32 addr, u32 dwords);

// Pone a cero `bytes` bytes de manera contigua (bloques de 16 bytes).
void hw_memzero_blocks(u32 addr, u32 blocks);

// RAM del sistema para la simulacion en PC (devuelve punteros reales).
// En GBA devuelven direcciones de hardware; en PC, buffers reales.
u8 *hw_iwram_high_region(void);
u8 *hw_vram_region(void);

// Tamano de las regiones anteriores en bytes.
u32 hw_iwram_high_size(void);
u32 hw_vram_size(void);

// Puntero escribible a la region mapeada que contiene `addr`.
// Devuelve NULL si la direccion no pertenece a ninguna region emulada.
// En GBA devuelve la direccion real (IWRAM / VRAM / PAL RAM).
u8 *hw_mem_region_ptr(u32 addr);

// Escrituras en memoria mapeada (IWRAM / VRAM / PAL RAM).
void hw_mem_write8(u32 addr, u8 value);
void hw_mem_write16(u32 addr, u16 value);
void hw_mem_write32(u32 addr, u32 value);
void hw_mem_write_block(u32 dst, const void *src, u32 bytes);

// Lecturas de memoria mapeada (IWRAM / VRAM / PAL RAM).
u8  hw_mem_read8(u32 addr);
u16 hw_mem_read16(u32 addr);
u32 hw_mem_read32(u32 addr);

// ------------------------------------------------------------------------------
// API de memoria (implementado en src/memory.c)
// ------------------------------------------------------------------------------

// Limpia los buffers de arranque: IWRAM alto + VRAM. Portado de FUN_0802604c.
void memory_clear_boot_buffers(void);

#endif // GUARD_MEMORY_H
