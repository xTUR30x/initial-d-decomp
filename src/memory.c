// ==============================================================================
// src/memory.c - Utilidades de memoria, buffers de hardware y ventanas
// ------------------------------------------------------------------------------
// Portado de FUN_0802604c. Clasificado como SUBSISTEMA DE MEMORIA.
//
// Flujo original verificado:
//   1. Bucle 1: escribe 4 dwords (16 bytes) a la vez desde 0x03002080,
//      0x5E7 iteraciones -> limpia 24176 bytes de IWRAM (zona alta).
//   2. Bucle 2: escribe 4 dwords (16 bytes) a la vez desde 0x06000000,
//      0x1800 iteraciones -> limpia 98304 bytes (VRAM completa, 96 KB).
//
// NO toca OAM (0x07000000) ni la paleta (0x05000000).
//
// Ademas implementa la capa de acceso a memoria mapeada (IWRAM, VRAM, PAL RAM)
// que comparten los modulos de graficos, audio e intro. En GBA son punteros
// directos al hardware; en PC son buffers virtuales equivalentes.
// ==============================================================================

#include "global.h"
#include "memory.h"

#include <stddef.h>   // NULL

// ==============================================================================
// API de memoria
// ==============================================================================

void memory_clear_boot_buffers(void)
{
    // 1) Limpia la zona alta de IWRAM.
    hw_memzero_blocks(BOOT_CLEAR_IWRAM_BASE, BOOT_CLEAR_IWRAM_WORDS);

    // 2) Limpia la VRAM completa (96 KB).
    hw_memzero_blocks(BOOT_CLEAR_VRAM_BASE, BOOT_CLEAR_VRAM_WORDS);
}

// ==============================================================================
// Implementacion de la capa de plataforma
// ==============================================================================
#ifdef TARGET_GBA

void hw_memzero_dwords(u32 addr, u32 dwords)
{
    vu32 *p = (vu32 *)addr;
    for (u32 i = 0; i < dwords; i++) {
        p[i] = 0;
    }
}

void hw_memzero_blocks(u32 addr, u32 blocks)
{
    // Equivale a `stmia r1!, {r0,r0,r0,r0}`: 4 dwords por iteracion.
    vu32 *p = (vu32 *)addr;
    for (u32 i = 0; i < blocks; i++) {
        p[0] = 0;
        p[1] = 0;
        p[2] = 0;
        p[3] = 0;
        p += 4;
    }
}

u8 *hw_iwram_high_region(void) { return (u8 *)BOOT_CLEAR_IWRAM_BASE; }
u8 *hw_vram_region(void)       { return (u8 *)BOOT_CLEAR_VRAM_BASE; }
u32 hw_iwram_high_size(void)   { return BOOT_CLEAR_IWRAM_WORDS * 16u; }
u32 hw_vram_size(void)         { return BOOT_CLEAR_VRAM_WORDS * 16u; }

// ------------------------------------------------------------------------------
// Memoria mapeada del GBA: punteros directos.
// ------------------------------------------------------------------------------

u8 *hw_mem_region_ptr(u32 addr)
{
    if (addr >= GBA_IWRAM_BASE && addr < (GBA_IWRAM_BASE + GBA_IWRAM_SIZE)) {
        return (u8 *)addr;
    }
    if (addr >= GBA_VRAM_BASE && addr < (GBA_VRAM_BASE + GBA_VRAM_SIZE)) {
        return (u8 *)addr;
    }
    if (addr >= GBA_PALRAM_BASE && addr < (GBA_PALRAM_BASE + GBA_PALRAM_SIZE)) {
        return (u8 *)addr;
    }
    return NULL;
}

void hw_mem_write8(u32 addr, u8 value)
{
    *(vu8 *)addr = value;
}

void hw_mem_write16(u32 addr, u16 value)
{
    *(vu16 *)addr = value;
}

void hw_mem_write32(u32 addr, u32 value)
{
    *(vu32 *)addr = value;
}

void hw_mem_write_block(u32 dst, const void *src, u32 bytes)
{
    const u8 *s = (const u8 *)src;
    vu8 *d = (vu8 *)dst;
    for (u32 i = 0; i < bytes; i++) {
        d[i] = s[i];
    }
}

u8 hw_mem_read8(u32 addr)  { return *(const vu8 *)addr; }
u16 hw_mem_read16(u32 addr) { return *(const vu16 *)addr; }
u32 hw_mem_read32(u32 addr) { return *(const vu32 *)addr; }

#else // ===================== PC / SDL2 =====================

#include <string.h>

// Buffers reales que simulan las regiones de hardware en PC.
static u8 g_iwram_high[BOOT_CLEAR_IWRAM_WORDS * 16u];
static u8 g_vram[BOOT_CLEAR_VRAM_WORDS * 16u];
static u8 g_palram[GBA_PALRAM_SIZE];

void hw_memzero_dwords(u32 addr, u32 dwords)
{
    (void)addr; (void)dwords;
    // No se usa directamente en la logica portable; delegado a los buffers.
}

void hw_memzero_blocks(u32 addr, u32 blocks)
{
    (void)addr;
    // En PC la limpieza se aplica a los buffers virtuales segun la region.
    if (addr == BOOT_CLEAR_IWRAM_BASE) {
        u32 bytes = blocks * 16u;
        if (bytes > sizeof(g_iwram_high)) bytes = sizeof(g_iwram_high);
        memset(g_iwram_high, 0, bytes);
    } else if (addr == BOOT_CLEAR_VRAM_BASE) {
        u32 bytes = blocks * 16u;
        if (bytes > sizeof(g_vram)) bytes = sizeof(g_vram);
        memset(g_vram, 0, bytes);
    }
}

u8 *hw_iwram_high_region(void) { return g_iwram_high; }
u8 *hw_vram_region(void)       { return g_vram; }
u32 hw_iwram_high_size(void)   { return sizeof(g_iwram_high); }
u32 hw_vram_size(void)         { return sizeof(g_vram); }

// ------------------------------------------------------------------------------
// Memoria mapeada emulada en PC.
//
// Nota: la IWRAM solo esta emulada a partir de la zona alta (0x03002080), que
// es la que limpia FUN_0802604c y la que usan el motor, el estado 0 y la
// maquina de estados. Las direcciones por debajo de ese limite no tienen
// imagen en PC (en GBA si, porque son la RAM real).
// ------------------------------------------------------------------------------

u8 *hw_mem_region_ptr(u32 addr)
{
    if (addr >= BOOT_CLEAR_IWRAM_BASE &&
        addr < (BOOT_CLEAR_IWRAM_BASE + sizeof(g_iwram_high))) {
        return g_iwram_high + (addr - BOOT_CLEAR_IWRAM_BASE);
    }
    if (addr >= GBA_VRAM_BASE && addr < (GBA_VRAM_BASE + sizeof(g_vram))) {
        return g_vram + (addr - GBA_VRAM_BASE);
    }
    if (addr >= GBA_PALRAM_BASE && addr < (GBA_PALRAM_BASE + sizeof(g_palram))) {
        return g_palram + (addr - GBA_PALRAM_BASE);
    }
    return NULL;
}

void hw_mem_write8(u32 addr, u8 value)
{
    u8 *p = hw_mem_region_ptr(addr);
    if (p != NULL) {
        *p = value;
    }
}

void hw_mem_write16(u32 addr, u16 value)
{
    u8 *p = hw_mem_region_ptr(addr);
    if (p != NULL) {
        *(u16 *)p = value;
    }
}

void hw_mem_write32(u32 addr, u32 value)
{
    u8 *p = hw_mem_region_ptr(addr);
    if (p != NULL) {
        *(u32 *)p = value;
    }
}

void hw_mem_write_block(u32 dst, const void *src, u32 bytes)
{
    u8 *p = hw_mem_region_ptr(dst);
    if (p != NULL) {
        memcpy(p, src, bytes);
    }
}

u8 hw_mem_read8(u32 addr)
{
    const u8 *p = hw_mem_region_ptr(addr);
    return (p != NULL) ? *p : 0u;
}

u16 hw_mem_read16(u32 addr)
{
    const u8 *p = hw_mem_region_ptr(addr);
    return (p != NULL) ? *(const u16 *)p : 0u;
}

u32 hw_mem_read32(u32 addr)
{
    const u8 *p = hw_mem_region_ptr(addr);
    return (p != NULL) ? *(const u32 *)p : 0u;
}

#endif // TARGET_GBA
