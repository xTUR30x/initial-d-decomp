// ==============================================================================
// src/pc_main.c - Port nativo (PC/SDL2) de "Initial D: Another Stage"
// ------------------------------------------------------------------------------
// Sustituye el buffer de prueba ("HOLA") por un renderizador de video que
// reproduce la salida del GBA a partir del estado emulado del cartucho:
//
//   * REG_DISPCNT (0x04000000) -> modo, habilitacion de BG0/OBJ, forced blank
//   * REG_BG0CNT  (0x04000008) -> base de tiles, base del tilemap, 4bpp/8bpp,
//                                  tamano del mapa y flags de color
//   * VRAM  (0x06000000)       -> tilemap (entradas de 16 bits) y tiles
//                                  (32 bytes en 4bpp, 64 bytes en 8bpp)
//   * PAL RAM (0x05000000)     -> paletas BGR555 de la capa
//   * REG_BLDCNT/BLDALPHA/BLDY -> modulacion de brillo y mezcla alpha
//
// El resultado indexado se convierte a ARGB8888 en tiempo real y se presenta
// como textura escalada (nearest neighbour) en la ventana de SDL2.
//
// Teclas:
//   P -> cambia la region de paleta de la capa 8bpp (0x05000200 <-> 0x05000000)
//   M -> fuerza la interpretacion 4bpp del BG0 (para comparar con el 8bpp)
//   B -> override de depuracion del BLD (normal / oscurecer / aclarar / alpha / nada)
//   S -> guarda una captura BMP del framebuffer
//   ESC -> salir
//
// Opciones de linea de comandos:
//   --screenshot <archivo.bmp>  espera al fundido de entrada y guarda una captura
//   --hold                      no deja que el modo demostracion avance de estado
//   --palette-obj               usa la paleta 0x05000200 (region OBJ) en la capa 8bpp
//   --4bpp                      fuerza la interpretacion 4bpp del fondo 0
//   --bld <none|brighten|darken|alpha|auto>
//                               override de BLDCNT/BLDALPHA/BLDY (auto = los del juego)
// ==============================================================================

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "global.h"
#include "init.h"
#include "states.h"
#include "graphics.h"
#include "memory.h"
#include "fade.h"

#define GBA_SCREEN_WIDTH  240
#define GBA_SCREEN_HEIGHT 160
#define SCREEN_SCALE      3
#define STATUS_BAR_HEIGHT 20

// ------------------------------------------------------------------------------
// Paletas de PAL RAM.
//
// El hardware GBA divide la PAL RAM en dos regiones de 256 colores:
//   0x05000000 -> paleta de fondos (16 bancos de 16 colores)
//   0x05000200 -> paleta de OBJ
//
// Un BG del GBA SIEMPRE lee su paleta en 0x05000000: es la unica region que
// el hardware consulta para un fondo. Por eso el renderizador lee ahi.
//
// En el Estado 0, la ROM deja en cada region:
//   0x05000000 -> 0x080A5974 (256 colores), copiada por la cola de paletas
//                 del motor de VBlank: 0x080A53D8 llama a FUN_08028D60 con
//                 r3 = 0x080A5974 y FUN_08028AC4 hace el DMA3 en el VBlank.
//   0x05000200 -> 0x080A5B74 (256 colores), DMA3 directo en 0x080A53E4.
//
// La region 0x05000200 queda disponible para comparar (tecla P).
// ------------------------------------------------------------------------------
#define PALRAM_BG_BASE   0x05000000u
#define PALRAM_OBJ_BASE  0x05000200u

// ------------------------------------------------------------------------------
// Estado del renderizador
// ------------------------------------------------------------------------------
static u32  s_framebuffer[GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT];
static u32  s_palette_base = PALRAM_BG_BASE;   // region de paleta de la capa
static bool s_force_4bpp    = false;            // override de la interpretacion
static int  s_bld_override  = 0;                // 0 = usar los registros del juego
                                                 // 1 = sin efecto, 2 = aclarar,
                                                 // 3 = oscurecer, 4 = alpha
static u16  s_last_dispcnt  = 0xFFFFu;
static u16  s_last_bg0cnt   = 0xFFFFu;
static u16  s_last_bldcnt   = 0xFFFFu;
static u16  s_last_bldalpha = 0xFFFFu;
static u16  s_last_bldy     = 0xFFFFu;

// ==============================================================================
// Conversion de color
// ==============================================================================

// BGR555 (15 bits) -> ARGB8888 (el GBA guarda 5 bits por canal).
//
// Decodificacion BGR555 (los bits bajos del color son el ROJO):
//   rojo   = (color >> 0)  & 0x1F
//   verde  = (color >> 5)  & 0x1F
//   azul   = (color >> 10) & 0x1F
//
// Empaquetado ARGB8888 (SDL_PIXELFORMAT_ARGB8888 = 0xAARRGGBB):
//   bits 24-31 = alfa, 16-23 = rojo, 8-15 = verde, 0-7 = azul
static u32 bgr555_to_argb(u16 color)
{
    u32 r = (color >> 0) & 31u;
    u32 g = (color >> 5) & 31u;
    u32 b = (color >> 10) & 31u;

    r = (r << 3) | (r >> 2);   // 5 bits -> 8 bits
    g = (g << 3) | (g >> 2);
    b = (b << 3) | (b >> 2);

    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

// ==============================================================================
// Unidad de blend del GBA (BLDCNT / BLDALPHA / BLDY)
// ==============================================================================
//
// REG_BLDCNT (0x04000050):
//   bits 0-5  : primeros objetivos  (0=BG0, 1=BG1, 2=BG2, 3=BG3, 4=OBJ, 5=fondo)
//   bits 6-7  : modo (0 = sin efecto, 1 = alpha, 2 = aclarar, 3 = oscurecer)
//   bits 8-13 : segundos objetivos (8=BG0, 9=BG1, 10=BG2, 11=BG3, 12=OBJ, 13=fondo)
//   bit  14   : sprites alojados en la VRAM de OBJ
// REG_BLDALPHA (0x04000052): EVA en los bits 0-4 y EVB en los bits 8-12 (0..16)
// REG_BLDY     (0x04000054): nivel de brillo (0..16)
//
// Semantica implementada (GBATEK, "4000054h - BLDY - Brightness (Y)
// coefficient"; EVY = nivel/16):
//   modo 2 (increase): I = I + (31-I)*EVY   -> 8 bits: C + (255-C)*nivel/16
//   modo 3 (decrease): I = I - I*EVY        -> 8 bits: C * (16-nivel)/16
//   modo 1 (alpha)   : C_sup*EVA/16 + C_inf*EVB/16 (EVA/EVB con clamp a 16)
//
// Por tanto nivel = 16 es el efecto MAXIMO (negro en modo 3, blanco en modo 2)
// y nivel = 0 es "sin efecto". Esto es lo que usan los estados del motor
// FUN_08028AC4: el estado 1 arranca en BLDY = 16 (negro) y BAJA hasta 0, es
// decir, es un fundido de entrada desde negro (0x08028DA4). El estado 2 sube
// de 0 a 16 y es un fundido de salida a negro (0x08028FA4).
//
// En los modos 2 y 3 solo cuenta el 1er objetivo (el 2o se ignora). En el modo
// 1 el 1er objetivo es la capa superior y el 2o la capa inferior.
//
// Valores reales del Estado 0 (medidos en la ROM):
//   FUN_08028D7C / FUN_08028DA4 -> BLDY = 16, BLDCNT = 0x00FF
//     0x00FF = objetivos 0x3F (todos) + modo 3 (oscurecer) + sin 2o objetivo.
//     Con BLDY = 16 la pantalla queda NEGRA y el motor la va aclarando al
//     bajar BLDY hasta 0 (fundido de entrada, src/fade.c). Al terminar deja
//     BLDCNT = 0x0F40 (modo alpha sin 1er objetivo = inerte) y BLDY = 0.
//   BLDALPHA no se escribe en el Estado 0; el motor lo deja en 0x0808 al
//   terminar un fundido (estados 1/3/5) y en 0x1010 en el estado 8.
// ==============================================================================

#define BLD_TARGET_BG0      0x01u   // bit 0 de cada conjunto de objetivos
#define BLD_TARGET_BACKDROP 0x20u   // bit 5 de cada conjunto de objetivos

// Nivel de BLDY efectivo: el hardware solo usa 0..16.
static u32 bld_level(u16 bldy)
{
    return (bldy > 16u) ? 16u : (u32)bldy;
}

// Canal alpha: capa superior * EVA + capa inferior * EVB, ambos sobre 16.
static u32 bld_alpha_channel(u32 top, u32 bottom, u32 eva, u32 evb)
{
    u32 v = top * eva + bottom * evb;
    v /= 16u;
    return (v > 255u) ? 255u : v;
}

// Aplica el blend a un pixel ARGB8888.
//   top      : color del pixel de la capa que se esta dibujando
//   top_t    : objetivo BLD de esa capa (BG0 o fondo)
//   bottom   : color de la capa inferior (solo se usa en modo alpha)
//   bottom_t : objetivo BLD de la capa inferior
static u32 bld_apply(u32 top, u32 top_t, u32 bottom, u32 bottom_t,
                     u16 bldcnt, u16 bldalpha, u16 bldy)
{
    const u32 mode = (bldcnt >> 6) & 3u;
    if (mode == 0u) {
        return top;   // modo 0 = sin efecto
    }

    const u32 first_mask  = bldcnt & 0x3Fu;
    const u32 second_mask = (bldcnt >> 8) & 0x3Fu;

    u32 r = (top >> 16) & 0xFFu;
    u32 g = (top >> 8) & 0xFFu;
    u32 b = top & 0xFFu;

    if (mode == 2u || mode == 3u) {
        // Brillo: el hardware solo modula el 1er objetivo.
        if ((first_mask & top_t) == 0u) {
            return top;
        }

        const u32 level = bld_level(bldy);

        // nivel 0 = sin efecto en ambos modos; nivel 16 = efecto maximo.
        if (level == 0u) {
            return top;
        }

        if (mode == 3u) {
            // Decrease (GBATEK): I = I - I*EVY, negro con nivel 16.
            const u32 keep = 16u - level;
            r = r * keep / 16u;
            g = g * keep / 16u;
            b = b * keep / 16u;
        } else {
            // Increase (GBATEK): I = I + (31-I)*EVY, blanco con nivel 16.
            r = r + (255u - r) * level / 16u;
            g = g + (255u - g) * level / 16u;
            b = b + (255u - b) * level / 16u;
        }

        return 0xFF000000u | (r << 16) | (g << 8) | b;
    }

    // Modo 1 (alpha): 1er objetivo = capa superior, 2o = capa inferior.
    if ((first_mask & top_t) == 0u || (second_mask & bottom_t) == 0u) {
        return top;
    }

    const u32 eva = (bldalpha & 0x1Fu) > 16u ? 16u : (bldalpha & 0x1Fu);
    const u32 evb = ((bldalpha >> 8) & 0x1Fu) > 16u ? 16u
                                                   : ((bldalpha >> 8) & 0x1Fu);

    r = bld_alpha_channel(r, (bottom >> 16) & 0xFFu, eva, evb);
    g = bld_alpha_channel(g, (bottom >> 8) & 0xFFu, eva, evb);
    b = bld_alpha_channel(b, bottom & 0xFFu, eva, evb);

    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

// ==============================================================================
// Renderizador de BG0
// ==============================================================================
//
// Reproduce las reglas del hardware GBA para el fondo 0:
//
//   DISPCNT bit 7  -> forced blank (pantalla blanca)
//   DISPCNT bit 8  -> BG0 habilitado
//   BG0CNT bits 2-3 -> bloque de base de tiles (16 KB cada uno)
//   BG0CNT bits 6-7 -> 0/1 = 4bpp (16 colores), 2/3 = 8bpp (256 colores)
//   BG0CNT bits 8-12 -> bloque de base del tilemap (2 KB cada uno)
//   BG0CNT bits 14-15 -> tamano del mapa (32x32, 32x64, 64x32, 64x64 tiles)
//
//   Entrada del tilemap (16 bits):
//     bits 0-9   numero de tile
//     bits 10-12 banco de paleta
//     bit  12    volteo horizontal
//     bit  13    volteo vertical
//
//   En 4bpp el color es (banco * 16 + nibble) y el indice 0 es transparente
//   (se ve el color de fondo, la entrada 0 de la paleta BG). En 8bpp el byte
//   del pixel es el indice directo de la paleta de 256 colores: el nibble
//   alto hace de banco (medido sobre los datos del Estado 0 y del Estado 9).
// ==============================================================================

// Devuelve la direccion de la entrada del tilemap para la celda (tx, ty).
static u32 bg0_map_entry_addr(u32 screen_base, u32 tx, u32 ty)
{
    // El layout del GBA reparte el mapa en bloques de 32x32 tiles:
    //   32x32 -> bloque 0
    //   32x64 -> bloques 0 y 2 (vertical)
    //   64x32 -> bloques 0 y 1 (horizontal)
    //   64x64 -> bloques 0, 1, 2 y 3
    const u32 block = (ty >> 5) * 2u + (tx >> 5);

    return screen_base + block * 0x800u + ((ty & 31u) * 64u) + ((tx & 31u) * 2u);
}

// Registros de blend efectivos. El override de depuracion (tecla B) sustituye
// los valores que escribio el juego para poder ver la modulacion en pantalla.
static void bld_effective_regs(u16 *bldcnt, u16 *bldalpha, u16 *bldy)
{
    switch (s_bld_override) {
        case 1:   // sin efecto
            *bldcnt   = 0x0000u;
            *bldalpha = 0x0000u;
            *bldy     = 16u;
            break;
        case 2:   // aclarar (modo 2, 1er objetivo BG0), 50% hacia blanco
            *bldcnt   = 0x0081u;
            *bldalpha = 0x0000u;
            *bldy     = 8u;
            break;
        case 3:   // oscurecer (modo 3, 1er objetivo BG0), 50% hacia negro
            *bldcnt   = 0x00C1u;
            *bldalpha = 0x0000u;
            *bldy     = 8u;
            break;
        case 4:   // alpha: BG0 (superior) sobre el fondo, EVA = EVB = 8
            *bldcnt   = 0x2041u;   // 1er obj BG0, modo 1, 2o obj fondo
            *bldalpha = 0x0808u;
            *bldy     = 16u;
            break;
        default:
            *bldcnt   = hw_gfx_reg_read16(REG_BLDCNT);
            *bldalpha = hw_gfx_reg_read16(REG_BLDALPHA);
            *bldy     = hw_gfx_reg_read16(REG_BLDY);
            break;
    }
}

// Rellena el framebuffer con la imagen del fondo 0.
static void render_bg0(void)
{
    const u16 dispcnt = hw_gfx_reg_read16(REG_DISPCNT);
    const u16 bg0cnt  = hw_gfx_reg_read16(REG_BG0CNT);

    // Modulacion de brillo y alpha (BLDCNT / BLDALPHA / BLDY).
    u16 bldcnt, bldalpha, bldy;
    bld_effective_regs(&bldcnt, &bldalpha, &bldy);

    // Traza por consola cuando cambian los registros de video o de blend.
    if (dispcnt != s_last_dispcnt || bg0cnt != s_last_bg0cnt ||
        bldcnt != s_last_bldcnt || bldalpha != s_last_bldalpha ||
        bldy != s_last_bldy) {
        s_last_dispcnt  = dispcnt;
        s_last_bg0cnt   = bg0cnt;
        s_last_bldcnt   = bldcnt;
        s_last_bldalpha = bldalpha;
        s_last_bldy     = bldy;
        printf("[render] DISPCNT=0x%04X  BG0CNT=0x%04X  (modo %u, %s, %s)\n",
               (unsigned)dispcnt, (unsigned)bg0cnt,
               (unsigned)(dispcnt & 7u),
               ((bg0cnt & 0x00C0u) == 0x0080u) ? "8bpp" : "4bpp",
               ((dispcnt & 0x0080u) != 0u) ? "forced blank"
                                           : ((dispcnt & 0x0100u) != 0u) ? "BG0 on" : "BG0 off");
        printf("[render] BLDCNT=0x%04X (modo %u)  BLDALPHA=0x%04X (EVA=%u EVB=%u)  BLDY=%u\n",
               (unsigned)bldcnt, (unsigned)((bldcnt >> 6) & 3u),
               (unsigned)bldalpha,
               (unsigned)(bldalpha & 0x1Fu), (unsigned)((bldalpha >> 8) & 0x1Fu),
               (unsigned)bldy);
    }

    const u32 pixels = (u32)GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT;

    // Forced blank: la pantalla se pone blanca.
    if ((dispcnt & 0x0080u) != 0u) {
        for (u32 i = 0; i < pixels; i++) {
            s_framebuffer[i] = 0xFFFFFFFFu;
        }
        return;
    }

    // Color de fondo (backdrop): entrada 0 de la paleta de fondos.
    const u32 backdrop = bgr555_to_argb(hw_mem_read16(PALRAM_BG_BASE));

    // Sin BG0 (o sin OBJ) solo se ve el fondo.
    if ((dispcnt & 0x0100u) == 0u) {
        const u32 bg = bld_apply(backdrop, BLD_TARGET_BACKDROP,
                                 backdrop, BLD_TARGET_BACKDROP,
                                 bldcnt, bldalpha, bldy);
        for (u32 i = 0; i < pixels; i++) {
            s_framebuffer[i] = bg;
        }
        return;
    }

    const u32 char_base  = GBA_VRAM_BASE + (((bg0cnt >> 2) & 3u) * 0x4000u);
    const u32 screen_base = GBA_VRAM_BASE + (((bg0cnt >> 8) & 0x1Fu) * 0x800u);

    // "256 colores" (bits 6-7 == 2) selecciona tiles de 64 bytes (8bpp).
    const bool bpp8 = (((bg0cnt & 0x00C0u) == 0x0080u) && !s_force_4bpp);
    const u32  tile_size = bpp8 ? 64u : 32u;

    // La paleta de 16 colores (4bpp) siempre vive en la region BG. La de 256
    // colores (8bpp) se lee tambien en la region BG por defecto, que es lo
    // que haria el hardware; con la tecla P se puede cambiar a la region OBJ
    // (0x05000200) para comparar con la paleta 0x080A5B74.
    const u32 pal_base = bpp8 ? s_palette_base : PALRAM_BG_BASE;

    for (u32 py = 0; py < GBA_SCREEN_HEIGHT; py++) {
        const u32 ty = py >> 3;

        for (u32 px = 0; px < GBA_SCREEN_WIDTH; px++) {
            const u32 tx = px >> 3;

            const u16 entry = hw_mem_read16(bg0_map_entry_addr(screen_base, tx, ty));
            const u32 tile  = entry & 0x03FFu;
            const u32 bank  = (entry >> 10) & (bpp8 ? 0x07u : 0x03u);

            u32 ix = px & 7u;
            u32 iy = py & 7u;
            if (((entry >> 12) & 1u) != 0u) { ix = 7u - ix; }   // volteo H
            if (((entry >> 13) & 1u) != 0u) { iy = 7u - iy; }   // volteo V

            const u32 tile_addr = char_base + tile * tile_size;

            u32 color_index;
            if (bpp8) {
                // 8bpp: el byte del pixel es el indice de la paleta.
                color_index = hw_mem_read8(tile_addr + iy * 8u + ix);
            } else {
                // 4bpp: dos pixeles por byte, indice = banco * 16 + nibble.
                const u8 byte = hw_mem_read8(tile_addr + iy * 4u + (ix >> 1));
                const u32 nibble = ((ix & 1u) != 0u) ? (u32)(byte & 0x0Fu)
                                                     : (u32)(byte >> 4);
                color_index = bank * 16u + nibble;
            }

            u32 argb;
            if (!bpp8 && color_index == 0u) {
                // 4bpp: el indice 0 es transparente, se ve el fondo (backdrop).
                argb = bld_apply(backdrop, BLD_TARGET_BACKDROP,
                                 backdrop, BLD_TARGET_BACKDROP,
                                 bldcnt, bldalpha, bldy);
            } else {
                const u32 layer = bgr555_to_argb(hw_mem_read16(pal_base + color_index * 2u));
                argb = bld_apply(layer, BLD_TARGET_BG0,
                                 backdrop, BLD_TARGET_BACKDROP,
                                 bldcnt, bldalpha, bldy);
            }

            s_framebuffer[py * GBA_SCREEN_WIDTH + px] = argb;
        }
    }
}

// ==============================================================================
// Captura BMP (24 bits, filas de abajo arriba) del framebuffer
// ==============================================================================

static void put_u16_le(u8 *p, u32 value)
{
    p[0] = (u8)(value & 0xFFu);
    p[1] = (u8)((value >> 8) & 0xFFu);
}

static void put_u32_le(u8 *p, u32 value)
{
    p[0] = (u8)(value & 0xFFu);
    p[1] = (u8)((value >> 8) & 0xFFu);
    p[2] = (u8)((value >> 16) & 0xFFu);
    p[3] = (u8)((value >> 24) & 0xFFu);
}

static bool save_bmp(const char *path)
{
    const u32 width  = GBA_SCREEN_WIDTH;
    const u32 height = GBA_SCREEN_HEIGHT;
    const u32 row    = ((width * 3u) + 3u) & ~3u;   // filas alineadas a 4 bytes
    const u32 imgsz  = row * height;
    const u32 fsz    = 54u + imgsz;

    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        printf("[render] no se pudo abrir '%s' para la captura\n", path);
        return false;
    }

    u8 header[54];
    memset(header, 0, sizeof(header));
    header[0] = 'B';
    header[1] = 'M';
    put_u32_le(&header[2],  fsz);
    put_u32_le(&header[10], 54u);
    put_u32_le(&header[14], 40u);
    put_u32_le(&header[18], width);
    put_u32_le(&header[22], height);
    put_u16_le(&header[26], 1u);
    put_u16_le(&header[28], 24u);
    put_u32_le(&header[34], imgsz);
    fwrite(header, 1u, sizeof(header), f);

    u8 line[((GBA_SCREEN_WIDTH * 3u) + 3u) & ~3u];
    for (u32 y = 0; y < height; y++) {
        const u32 src = (height - 1u - y) * width;   // BMP: filas de abajo arriba
        for (u32 x = 0; x < width; x++) {
            const u32 argb = s_framebuffer[src + x];
            line[x * 3u + 0u] = (u8)(argb & 0xFFu);         // B
            line[x * 3u + 1u] = (u8)((argb >> 8) & 0xFFu);  // G
            line[x * 3u + 2u] = (u8)((argb >> 16) & 0xFFu); // R
        }
        for (u32 i = width * 3u; i < row; i++) {
            line[i] = 0u;
        }
        fwrite(line, 1u, row, f);
    }

    fclose(f);
    printf("[render] captura guardada en '%s' (%ux%u)\n", path,
           (unsigned)width, (unsigned)height);
    return true;
}

// ==============================================================================
// Barra de estado (fuente 5x7 minima para digitos hexadecimales y etiquetas)
// ==============================================================================

// Cada glifo son 7 filas de 5 bits (bit 4 = columna izquierda).
typedef struct {
    char c;
    u8   rows[7];
} Glyph;

static const Glyph k_font[] = {
    { '0', { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E } },
    { '1', { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E } },
    { '2', { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F } },
    { '3', { 0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E } },
    { '4', { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 } },
    { '5', { 0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E } },
    { '6', { 0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E } },
    { '7', { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 } },
    { '8', { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E } },
    { '9', { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C } },
    { 'A', { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 } },
    { 'B', { 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E } },
    { 'C', { 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E } },
    { 'D', { 0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C } },
    { 'E', { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F } },
    { 'F', { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10 } },
    { 'G', { 0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F } },
    { 'L', { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F } },
    { 'P', { 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10 } },
    { 'S', { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E } },
    { 'T', { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 } },
    { ' ', { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
};

static const u8 *font_glyph(char c)
{
    for (u32 i = 0; i < sizeof(k_font) / sizeof(k_font[0]); i++) {
        if (k_font[i].c == c) {
            return k_font[i].rows;
        }
    }
    return NULL;
}

// Dibuja una linea de texto (5x7) con el color indicado.
static void draw_text(SDL_Renderer *renderer, int x, int y, int scale,
                      const char *text, u8 r, u8 g, u8 b)
{
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);

    int cx = x;
    for (const char *p = text; *p != '\0'; p++) {
        const u8 *rows = font_glyph(*p);
        if (rows != NULL) {
            for (int row = 0; row < 7; row++) {
                for (int col = 0; col < 5; col++) {
                    if ((rows[row] & (0x10u >> col)) != 0u) {
                        SDL_Rect px = { cx + col * scale, y + row * scale, scale, scale };
                        SDL_RenderFillRect(renderer, &px);
                    }
                }
            }
        }
        cx += 6 * scale;
    }
}

// Barra de estado bajo la pantalla del GBA.
static void draw_status_bar(SDL_Renderer *renderer)
{
    const int bar_y = GBA_SCREEN_HEIGHT * SCREEN_SCALE;

    SDL_SetRenderDrawColor(renderer, 18, 18, 26, 255);
    SDL_Rect bar = { 0, bar_y, GBA_SCREEN_WIDTH * SCREEN_SCALE, STATUS_BAR_HEIGHT };
    SDL_RenderFillRect(renderer, &bar);

    const u16 dispcnt = hw_gfx_reg_read16(REG_DISPCNT);
    const u16 bg0cnt  = hw_gfx_reg_read16(REG_BG0CNT);
    const bool bpp8 = (((bg0cnt & 0x00C0u) == 0x0080u) && !s_force_4bpp);

    u16 bldcnt, bldalpha, bldy;
    bld_effective_regs(&bldcnt, &bldalpha, &bldy);

    char text[96];
    snprintf(text, sizeof(text), "STATE %02u  %04X  %04X  %s  %06X  BLD %u %02X %04X",
             (unsigned)states_get_current(),
             (unsigned)dispcnt, (unsigned)bg0cnt,
             bpp8 ? "8BPP" : "4BPP",
             (unsigned)(bpp8 ? s_palette_base : PALRAM_BG_BASE),
             (unsigned)((bldcnt >> 6) & 3u), (unsigned)bldy, (unsigned)bldalpha);

    draw_text(renderer, 6, bar_y + 3, 2, text, 200, 210, 230);
}

// ==============================================================================
// Presentacion
// ==============================================================================

static void present(SDL_Renderer *renderer, SDL_Texture *texture)
{
    SDL_UpdateTexture(texture, NULL, s_framebuffer,
                      (int)(GBA_SCREEN_WIDTH * sizeof(u32)));

    SDL_SetRenderDrawColor(renderer, 15, 25, 45, 255);
    SDL_RenderClear(renderer);

    SDL_RenderCopy(renderer, texture, NULL, NULL);

    // Marco estilo retro.
    SDL_SetRenderDrawColor(renderer, 230, 70, 40, 255);
    SDL_Rect border = { 0, 0, GBA_SCREEN_WIDTH * SCREEN_SCALE,
                             GBA_SCREEN_HEIGHT * SCREEN_SCALE };
    SDL_RenderDrawRect(renderer, &border);

    draw_status_bar(renderer);

    SDL_RenderPresent(renderer);
}

// ==============================================================================
// main
// ==============================================================================

int main(int argc, char *argv[])
{
    const char *screenshot_path = NULL;
    bool hold_state = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--screenshot") == 0 && (i + 1) < argc) {
            screenshot_path = argv[++i];
        } else if (strcmp(argv[i], "--hold") == 0) {
            hold_state = true;
        } else if (strcmp(argv[i], "--palette-obj") == 0) {
            // Fuerza la paleta de la region OBJ (0x05000200) en la capa 8bpp.
            s_palette_base = PALRAM_OBJ_BASE;
        } else if (strcmp(argv[i], "--4bpp") == 0) {
            // Fuerza la interpretacion 4bpp del fondo 0.
            s_force_4bpp = true;
        } else if (strcmp(argv[i], "--bld") == 0 && (i + 1) < argc) {
            // Override de depuracion de la unidad de blend (BLDCNT/BLDY).
            const char *mode = argv[++i];
            if (strcmp(mode, "none") == 0) {
                s_bld_override = 1;
            } else if (strcmp(mode, "brighten") == 0) {
                s_bld_override = 2;
            } else if (strcmp(mode, "darken") == 0) {
                s_bld_override = 3;
            } else if (strcmp(mode, "alpha") == 0) {
                s_bld_override = 4;
            } else if (strcmp(mode, "auto") == 0) {
                s_bld_override = 0;
            } else {
                printf("modo de BLD desconocido: %s (none|brighten|darken|alpha|auto)\n", mode);
                return 1;
            }
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("uso: %s [--screenshot <archivo.bmp>] [--hold] [--palette-obj] [--4bpp]"
                   " [--bld none|brighten|darken|alpha|auto]\n", argv[0]);
            printf("  --screenshot  renderiza unos frames y guarda una captura BMP\n");
            printf("  --hold        mantiene el estado actual (no barre los 44 estados)\n");
            printf("  --palette-obj usa la paleta 0x05000200 (region OBJ) en la capa 8bpp\n");
            printf("  --4bpp        fuerza la interpretacion 4bpp del fondo 0\n");
            printf("  --bld <modo>  override de BLDCNT/BLDALPHA/BLDY (auto = los del juego)\n");
            return 0;
        } else {
            printf("opcion desconocida: %s (use --help)\n", argv[i]);
            return 1;
        }
    }

    printf("\n==================================================\n");
    printf(" Initial D: Another Stage - port nativo (SDL2)\n");
    printf("==================================================\n\n");

    // Ejecuta la secuencia de arranque del cartucho (implementacion PC/SDL2).
    game_start();

    if (hold_state) {
        // Sin barrido automatico: la pantalla se queda en el estado actual.
        states_set_demo_advance(false);
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("Error al inicializar SDL2: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Initial D - Native PC Port",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        GBA_SCREEN_WIDTH * SCREEN_SCALE,
        GBA_SCREEN_HEIGHT * SCREEN_SCALE + STATUS_BAR_HEIGHT,
        SDL_WINDOW_SHOWN
    );

    if (window == NULL) {
        printf("Error al crear la ventana: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
                                                SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (renderer == NULL) {
        printf("Error al crear el renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             GBA_SCREEN_WIDTH, GBA_SCREEN_HEIGHT);
    if (texture != NULL) {
        // Escala sin filtrado (pixeles nitidos, como el hardware).
        SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);
    }

    printf("[render] ventana lista: P=paleta  M=4bpp  B=BLD  S=captura  ESC=salir\n");

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        running = false;
                        break;
                    case SDLK_p:
                        s_palette_base = (s_palette_base == PALRAM_OBJ_BASE)
                                             ? PALRAM_BG_BASE
                                             : PALRAM_OBJ_BASE;
                        printf("[render] paleta de la capa 8bpp -> 0x%08X\n",
                               (unsigned)s_palette_base);
                        break;
                    case SDLK_m:
                        s_force_4bpp = !s_force_4bpp;
                        printf("[render] interpretacion forzada: %s\n",
                               s_force_4bpp ? "4bpp" : "8bpp");
                        break;
                    case SDLK_b:
                        // Override de depuracion del BLD: normal -> oscurecer
                        // -> aclarar -> alpha -> sin efecto -> normal.
                        s_bld_override = (s_bld_override == 0) ? 3
                                       : (s_bld_override == 3) ? 2
                                       : (s_bld_override == 2) ? 4
                                       : (s_bld_override == 4) ? 1 : 0;
                        printf("[render] BLD: %s\n",
                               (s_bld_override == 0) ? "registros del juego"
                               : (s_bld_override == 1) ? "sin efecto"
                               : (s_bld_override == 2) ? "aclarar (BLDY=8)"
                               : (s_bld_override == 3) ? "oscurecer (BLDY=8)"
                                                       : "alpha (EVA=EVB=8)");
                        break;
                    case SDLK_s:
                        save_bmp("captura.bmp");
                        break;
                    default:
                        break;
                }
            }
        }

        // 1) VBlank: motor de fundidos y cola de paletas (FUN_08028AC4).
        fade_frame_update();

        // 2) Maquina de estados: un paso por frame (equivalente a un frame
        //    del bucle cooperativo del cartucho).
        states_tick();

        // 3) Render del estado emulado del video.
        render_bg0();

        if (texture != NULL) {
            present(renderer, texture);
        }

        if (screenshot_path != NULL) {
            // Modo captura: espera a que termine el fundido de entrada del
            // Estado 0. El motor baja BLDY de 16 a 0 con frames = 2, o sea
            // 32 frames; a partir de ahi el logo esta a plena intensidad y la
            // paleta principal ya se copio a 0x05000000 (frame 2).
            static u32 frames = 0;
            frames++;
            if (frames >= 40u) {
                save_bmp(screenshot_path);
                running = false;
            }
        }
    }

    if (texture != NULL) {
        SDL_DestroyTexture(texture);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
