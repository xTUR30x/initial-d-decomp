// ==============================================================================
// src/save.c - Sistema de guardado (SRAM) del cartucho
// ------------------------------------------------------------------------------
// Portado de FUN_0801b27c (+ FUN_0801b4c8). SUBSISTEMA DE MEMORIA/SAVE (SRAM).
//
// Flujo original (verificado con REA):
//   1. *0x02032004 = 0                         (limpia contador global)
//   2. lee 0x2004 bytes de SRAM (0x0E000000) -> buffer EWRAM (0x02030000)
//   3. compara 8 dwords con "INITIAL-D Another Stage for GBA"
//   4a. coincide    -> FUN_0801b4c8() = verifica el CHECKSUM del save
//                      (si el checksum coincide -> save cargado)
//   4b. no coincide -> borra buffer, graba firma+defaults, reescribe SRAM (3 intentos)
//   5. calcula flags de progreso segun 0x02030800/0x02031000/0x02031800
//   6. copia cabecera del buffer a espejos en IWRAM
//
// CORRECCION: FUN_0801b4c8 NO es un retardo. Suma 0x2000 bytes del buffer de
// trabajo (4 bytes x 0x800 iteraciones) y compara la suma contra el dword
// almacenado en 0x02032000. Devuelve el resultado por flags de CPU (cmp).
// ==============================================================================

#include "global.h"
#include "save.h"

// Firma de cabecera del save (32 bytes exactos, verificada en ROM 0x0801B25C).
const char g_save_signature[] = "INITIAL-D Another Stage for GBA";

// Contador global limpiado al inicio. En GBA es EWRAM; en PC, memoria real.
#ifdef TARGET_GBA
#define REG_SAVE_COUNTER (*(vu32 *)SAVE_GLOBAL_COUNTER)
#else
extern u32 g_save_counter_pc;
#define REG_SAVE_COUNTER (g_save_counter_pc)
#endif

// ==============================================================================
// Helpers portables
// ==============================================================================

bool save_check_signature(const void *buffer)
{
    const u32 *buf = (const u32 *)buffer;
    const u32 *sig = (const u32 *)g_save_signature;
    for (u32 i = 0; i < SAVE_SIGNATURE_DWORDS; i++) {
        if (buf[i] != sig[i]) {
            return false;
        }
    }
    return true;
}

// Portado de FUN_0801b4c8: verifica el checksum del save.
// Suma los bytes del bloque [0, SAVE_CHECKSUM_BYTES) del buffer de trabajo y lo
// compara con el dword almacenado en SAVE_CHECKSUM_ADDR (buffer + 0x2000).
bool save_verify_checksum(const void *buffer)
{
    const u8 *buf = (const u8 *)buffer;

    // Suma de comprobacion byte a byte (el original lo hace de 4 en 4).
    u32 sum = 0;
    for (u32 i = 0; i < SAVE_CHECKSUM_BYTES; i++) {
        sum += buf[i];
    }

    // El checksum almacenado vive al final del bloque sumado (offset 0x2000).
    u32 stored = *(const u32 *)(buf + SAVE_CHECKSUM_BYTES);

    return sum == stored;
}

void save_reset_defaults(void *buffer)
{
    u8 *buf = (u8 *)buffer;

    // Copia la firma de cabecera (8 dwords).
    const u32 *sig = (const u32 *)g_save_signature;
    for (u32 i = 0; i < SAVE_SIGNATURE_DWORDS; i++) {
        ((u32 *)buf)[i] = sig[i];
    }

    // Defaults de estado inicial (bytes): 1, 0, 1, 0, y cuatro dwords a 0.
    buf[0x00] = 1;
    buf[0x01] = 0;
    buf[0x02] = 1;
    buf[0x03] = 0;
    *(u32 *)(buf + 0x04) = 0;
    *(u32 *)(buf + 0x08) = 0;
    *(u32 *)(buf + 0x0C) = 0;
    *(u32 *)(buf + 0x10) = 0;
}

// ------------------------------------------------------------------------------
// Acceso al buffer de trabajo del save.
// En GBA es una direccion fija de EWRAM. En PC es memoria real para poder
// ejecutar la logica portable sin direcciones de hardware.
// ------------------------------------------------------------------------------
#ifdef TARGET_GBA
#define SAVE_BUF() ((u8 *)SAVE_WORK_BUFFER_ADDR)
#else
u8 *hw_save_work_buffer(void);
#define SAVE_BUF() (hw_save_work_buffer())
#endif

// ==============================================================================
// API principal
// ==============================================================================

int save_init(void)
{
    u8 *work = SAVE_BUF();

    // 1) Limpia contador global.
    REG_SAVE_COUNTER = 0;

    // 2) Lee el save de SRAM al buffer de trabajo en EWRAM.
    hw_sram_read(SRAM_BASE_ADDR, work, SAVE_WORK_BUFFER_SIZE);

    // 3) Valida la firma de cabecera.
    bool valid = save_check_signature(work);

    if (valid) {
        // 4a) FIRMA correcta -> FUN_0801b4c8: verifica el CHECKSUM del save.
        //     Si el checksum tambien coincide, el save es valido.
        valid = save_verify_checksum(work);
    }

    if (!valid) {
        // 4b) Save invalido (firma o checksum incorrectos): reinicializa.

        // Borra 0x100 dwords agrupadas de a 8 (4 KB) del buffer de trabajo.
        u32 *buf = (u32 *)work;
        for (u32 i = 0; i < 0x100u; i++) {
            for (u32 j = 0; j < 8u; j++) {
                buf[i * 8u + j] = 0;
            }
        }

        // Graba firma + defaults y reescribe en SRAM con verificacion.
        save_reset_defaults(work);
        int write_ok = hw_sram_write(work, SRAM_BASE_ADDR, SAVE_WORK_BUFFER_SIZE);
        if (write_ok != 0) {
            return 0;  // Fallo de escritura: el juego no puede continuar.
        }
    }

    // 5) Calcula las banderas de progreso (bits 1, 2, 4, 8).
    u32 flags = 0x1u;
    if (*(const u32 *)(work + SAVE_FLAG_A_OFFSET) != 0) {
        flags |= 0x2u;
    }
    if (*(const u32 *)(work + SAVE_FLAG_B_OFFSET) != 0) {
        flags |= 0x4u;
    }
    if (*(const u32 *)(work + SAVE_FLAG_C_OFFSET) != 0) {
        flags |= 0x8u;
    }
    REG_SAVE_COUNTER = flags;

    // 6) Espejo de cabecera a IWRAM.
#ifdef TARGET_GBA
    u8 *mirror_name = (u8 *)SAVE_MIRROR_NAME_ADDR;
    mirror_name[0] = work[0x20];
    mirror_name[1] = work[0x21];
    mirror_name[2] = work[0x22];

    u32 *mirror_a = (u32 *)SAVE_MIRROR_A_ADDR;
    mirror_a[0] = *(const u32 *)(work + 0x24);
    mirror_a[1] = *(const u32 *)(work + 0x28);

    *(u32 *)SAVE_MIRROR_B_ADDR = *(const u32 *)(work + 0x2C);
    *(u32 *)SAVE_MIRROR_C_ADDR = *(const u32 *)(work + 0x30);
#else
    // En PC el espejo se guarda en memoria normal (ver hw_save_mirror_*).
    hw_save_mirror_header(work);
#endif

    return valid ? 1 : 0;
}

// ==============================================================================
// Implementacion de la capa de plataforma
// ==============================================================================
#ifdef TARGET_GBA

#define REG_WAITCNT_SAVE (*(vu16 *)0x04000204)

void hw_sram_read(u32 src, void *dst, u32 size)
{
    // El runtime GBA habilita SRAM ajustando WAITCNT y usa una copia de
    // 16 bits desde el espacio de cartucho (0x0E000000).
    u16 saved_waitcnt = REG_WAITCNT_SAVE;
    REG_WAITCNT_SAVE = (u16)((saved_waitcnt & 0xFFFCu) | 0x3u);

    volatile u16 *s = (volatile u16 *)src;
    u16 *d = (u16 *)dst;
    for (u32 i = 0; i < size / 2u; i++) {
        d[i] = s[i];
    }

    REG_WAITCNT_SAVE = saved_waitcnt;
}

int hw_sram_write(const void *src, u32 dst, u32 size)
{
    u16 saved_waitcnt = REG_WAITCNT_SAVE;
    REG_WAITCNT_SAVE = (u16)((saved_waitcnt & 0xFFFCu) | 0x3u);

    const u16 *s = (const u16 *)src;
    volatile u16 *d = (volatile u16 *)dst;
    for (u32 i = 0; i < size / 2u; i++) {
        d[i] = s[i];
    }

    // Verificacion de lectura (el runtime reintenta hasta 3 veces).
    int ok = 1;
    for (u32 i = 0; i < size / 2u; i++) {
        if (d[i] != s[i]) {
            ok = 0;
            break;
        }
    }

    REG_WAITCNT_SAVE = saved_waitcnt;
    return ok ? 0 : 1;
}

#else // ===================== PC / SDL2 =====================

#include <stdio.h>
#include <string.h>

// Buffer que simula SRAM en PC para poder probar la logica de save.
static u8 g_sram_sim[SAVE_WORK_BUFFER_SIZE];

// Buffer de trabajo del save en memoria real del PC (simula EWRAM 0x02030000).
static u8 g_save_work[SAVE_WORK_BUFFER_SIZE];

// Contador global del save (simula EWRAM 0x02032004).
u32 g_save_counter_pc = 0;

u8 *hw_save_work_buffer(void)
{
    return g_save_work;
}

void hw_sram_read(u32 src, void *dst, u32 size)
{
    (void)src;
    if (size > sizeof(g_sram_sim)) {
        size = sizeof(g_sram_sim);
    }
    memcpy(dst, g_sram_sim, size);
    printf("[save] SRAM read %u bytes (simulado)\n", (unsigned)size);
}

int hw_sram_write(const void *src, u32 dst, u32 size)
{
    (void)dst;
    if (size > sizeof(g_sram_sim)) {
        size = sizeof(g_sram_sim);
    }
    memcpy(g_sram_sim, src, size);
    printf("[save] SRAM write %u bytes (simulado)\n", (unsigned)size);
    return 0;
}

// Espejo de cabecera a "IWRAM" (memoria real en PC).
static u8  g_mirror_name[3];
static u32 g_mirror_a[2];
static u32 g_mirror_b;
static u32 g_mirror_c;

void hw_save_mirror_header(const u8 *work)
{
    g_mirror_name[0] = work[0x20];
    g_mirror_name[1] = work[0x21];
    g_mirror_name[2] = work[0x22];
    g_mirror_a[0] = *(const u32 *)(work + 0x24);
    g_mirror_a[1] = *(const u32 *)(work + 0x28);
    g_mirror_b = *(const u32 *)(work + 0x2C);
    g_mirror_c = *(const u32 *)(work + 0x30);
    printf("[save] espejo cabecera: name=%02X%02X%02X\n",
           g_mirror_name[0], g_mirror_name[1], g_mirror_name[2]);
}

#endif // TARGET_GBA
