#!/usr/bin/env python3
# ==============================================================================
# tools/extract_intro_assets.py
# ------------------------------------------------------------------------------
# Extrae los assets del Estado 0 (intro/logo) de baserom.gba y genera:
#     include/intro_assets.h
#     src/intro_assets.c
#
# Los assets se embeben como arrays C const para que ambos targets (GBA y PC)
# funcionen sin necesitar la imagen de ROM en tiempo de ejecucion.
#
# Direcciones verificadas con objdump sobre baserom.gba
# (SHA-256 b1684b7d6a678189011bb607e9f2c23febe5b9062303e8afe56cba43246a1592):
#
#   Paleta OBJ 16 colores   0x080A5934   32 bytes
#   Paleta OBJ 16 colores   0x080A5954   32 bytes
#   Paleta BG  256 colores  0x080A5974  512 bytes  (FUN_08028D60 -> DMA3 a 0x05000000)
#   Paleta OBJ 256 colores  0x080A5B74  512 bytes  (DMA3 a 0x05000200)
#   Stream 4bpp #1          0x080A6174  1204 bytes  (cabecera 4 B + 600 words)
#   Stream 4bpp #2          0x080A6628  1204 bytes
#   Stream 8bpp             0x080A6ADC  1202 bytes  (cabecera 2 B + 600 words)
#
# Pools de tiles (bases tomadas de los literales ARM de los descompresores):
#   4bpp: literal en 0x0854C6B8 -> base 0x0802FB28, tile = 32 bytes
#   8bpp: literal en 0x08000F00 -> base 0x08556B0C, tile = 64 bytes
#
# Los bloques efectivamente usados por los streams se calculan aqui y se
# embeben como rangos; el port resuelve el offset original con
# intro_pool4_ptr() / intro_pool8_ptr().
#
# Uso:  python3 tools/extract_intro_assets.py [baserom.gba]
# ==============================================================================

import hashlib
import os
import struct
import sys

ROM_BASE = 0x08000000
ROM_SHA256 = "b1684b7d6a678189011bb607e9f2c23febe5b9062303e8afe56cba43246a1592"

# --- Assets de datos --------------------------------------------------------
PAL_OBJ_A_ADDR = 0x080A5934
PAL_OBJ_A_SIZE = 32
PAL_OBJ_B_ADDR = 0x080A5954
PAL_OBJ_B_SIZE = 32
PAL_MAIN_ADDR = 0x080A5974      # paleta que FUN_08028D60 encola -> 0x05000000
PAL_MAIN_SIZE = 512
PAL_BG_ADDR = 0x080A5B74        # paleta copiada por el handler -> 0x05000200
PAL_BG_SIZE = 512

STREAM1_ADDR = 0x080A6174
STREAM2_ADDR = 0x080A6628
STREAM3_ADDR = 0x080A6ADC

# --- Pools de tiles ---------------------------------------------------------
POOL4_BASE = 0x0802FB28      # literal ARM en 0x0854C6B8 (descompresor 4bpp)
POOL8_BASE = 0x08556B0C      # literal ARM en 0x08000F00 (descompresor 8bpp)
TILE4_SIZE = 32
TILE8_SIZE = 64

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def die(msg):
    print("error: " + msg, file=sys.stderr)
    sys.exit(1)


class Rom:
    def __init__(self, path):
        with open(path, "rb") as fh:
            self.data = fh.read()
        digest = hashlib.sha256(self.data).hexdigest()
        if digest != ROM_SHA256:
            die("SHA-256 inesperado para %s:\n  esperado %s\n  obtenido %s"
                % (path, ROM_SHA256, digest))
        print("[assets] ROM verificada: %s (%d bytes)" % (path, len(self.data)))

    def read(self, addr, size):
        off = addr - ROM_BASE
        if off < 0 or off + size > len(self.data):
            die("lectura fuera de la ROM: 0x%08X + %d" % (addr, size))
        return self.data[off:off + size]

    def u16(self, addr):
        return struct.unpack_from("<H", self.data, addr - ROM_BASE)[0]

    def u32(self, addr):
        return struct.unpack_from("<I", self.data, addr - ROM_BASE)[0]


def stream_words4(rom, addr):
    """Stream 4bpp: cabecera [cols u8][rows u8][ip u16] + cols*rows words."""
    cols = rom.data[addr - ROM_BASE]
    rows = rom.data[addr - ROM_BASE + 1]
    ip = rom.u16(addr + 2)
    n = cols * rows
    words = [rom.u16(addr + 4 + 2 * i) for i in range(n)]
    return cols, rows, ip, words


def stream_words8(rom, addr):
    """Stream 8bpp: cabecera [cols u8][rows u8] + cols*rows words."""
    cols = rom.data[addr - ROM_BASE]
    rows = rom.data[addr - ROM_BASE + 1]
    n = cols * rows
    words = [rom.u16(addr + 2 + 2 * i) for i in range(n)]
    return cols, rows, words


def union_ranges(ranges):
    merged = []
    for start, end in sorted(ranges):
        if merged and start <= merged[-1][1]:
            if end > merged[-1][1]:
                merged[-1] = (merged[-1][0], end)
        else:
            merged.append((start, end))
    return merged


def emit_bytes(out, name, data):
    out.append("const u8 %s[%d] = {" % (name, len(data)))
    for i in range(0, len(data), 16):
        chunk = data[i:i + 16]
        out.append("    " + " ".join("0x%02X," % b for b in chunk))
    out.append("};")
    out.append("")


def emit_header(path, n4, n8, sizes):
    with open(path, "w", encoding="utf-8") as fh:
        fh.write("""#ifndef GUARD_INTRO_ASSETS_H
#define GUARD_INTRO_ASSETS_H

#include "global.h"

// ==============================================================================
// include/intro_assets.h - Assets del Estado 0 (intro/logo)
// ------------------------------------------------------------------------------
// GENERADO AUTOMATICAMENTE por tools/extract_intro_assets.py - NO EDITAR.
// Origen: baserom.gba (SHA-256 %s).
//
// Contiene:
//   * las 4 paletas del estado 0 (2 OBJ de 16 colores, la principal de 256
//     colores que el motor copia por DMA3 a 0x05000000 y la de la region OBJ
//     que el handler copia a 0x05000200),
//   * los 3 streams de tiles comprimidos (4bpp, 4bpp, 8bpp),
//   * los bloques de los pools de tiles que esos streams referencian.
//
// En el target GBA estos arrays viven en la ROM del cartucho, por lo que las
// copias por DMA3 siguen siendo transferencias ROM -> PAL RAM, igual que en el
// original.
// ==============================================================================

// Numero de rangos embebidos de cada pool de tiles.
#define INTRO_POOL4_RANGE_COUNT  %du
#define INTRO_POOL8_RANGE_COUNT  %du

// Rango del pool de tiles: 'offset' es el desplazamiento dentro del pool
// original de la ROM, 'length' su tamano en bytes y 'data' los datos.
typedef struct {
    u32        offset;
    u32        length;
    const u8  *data;
} IntroPoolRange;

// Paletas del estado 0.
extern const u8 g_intro_pal_obj_a[%d];
extern const u8 g_intro_pal_obj_b[%d];
extern const u8 g_intro_pal_main[%d];   // 0x080A5974 -> 0x05000000 (BG0 8bpp)
extern const u8 g_intro_pal_bg[%d];     // 0x080A5B74 -> 0x05000200 (region OBJ)

// Streams de tiles comprimidos (cabecera + entradas).
extern const u8 g_intro_stream1[%d];
extern const u8 g_intro_stream2[%d];
extern const u8 g_intro_stream3[%d];

// Rangos embebidos de los pools de tiles.
extern const IntroPoolRange g_intro_pool4_ranges[INTRO_POOL4_RANGE_COUNT];
extern const IntroPoolRange g_intro_pool8_ranges[INTRO_POOL8_RANGE_COUNT];

#endif // GUARD_INTRO_ASSETS_H
""" % (ROM_SHA256, n4, n8,
       sizes["pal_a"], sizes["pal_b"], sizes["pal_main"], sizes["pal_bg"],
       sizes["s1"], sizes["s2"], sizes["s3"]))


def main():
    rom_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(REPO_ROOT, "baserom.gba")
    if not os.path.isfile(rom_path):
        die("no existe la ROM: %s" % rom_path)

    rom = Rom(rom_path)

    # --- Paletas -------------------------------------------------------------
    pal_a = rom.read(PAL_OBJ_A_ADDR, PAL_OBJ_A_SIZE)
    pal_b = rom.read(PAL_OBJ_B_ADDR, PAL_OBJ_B_SIZE)
    pal_main = rom.read(PAL_MAIN_ADDR, PAL_MAIN_SIZE)
    pal_bg = rom.read(PAL_BG_ADDR, PAL_BG_SIZE)

    # --- Streams -------------------------------------------------------------
    c1, r1, ip1, w1 = stream_words4(rom, STREAM1_ADDR)
    c2, r2, ip2, w2 = stream_words4(rom, STREAM2_ADDR)
    c3, r3, w3 = stream_words8(rom, STREAM3_ADDR)
    print("[assets] stream 4bpp #1: %ux%u ip=0x%04X (%u entradas)" % (c1, r1, ip1, len(w1)))
    print("[assets] stream 4bpp #2: %ux%u ip=0x%04X (%u entradas)" % (c2, r2, ip2, len(w2)))
    print("[assets] stream 8bpp   : %ux%u (%u entradas)" % (c3, r3, len(w3)))

    stream1 = rom.read(STREAM1_ADDR, 4 + 2 * len(w1))
    stream2 = rom.read(STREAM2_ADDR, 4 + 2 * len(w2))
    stream3 = rom.read(STREAM3_ADDR, 2 + 2 * len(w3))

    # --- Rangos de los pools -------------------------------------------------
    # 4bpp: offset = (word << 17) >> 12   (tile de 32 bytes)
    # 8bpp: offset = (word << 17) >> 11   (tile de 64 bytes)
    blocks4 = []
    for word in w1 + w2:
        off = (word << 17) >> 12
        if off:
            blocks4.append((off, off + TILE4_SIZE))
    blocks8 = []
    for word in w3:
        off = (word << 17) >> 11
        if off:
            blocks8.append((off, off + TILE8_SIZE))

    ranges4 = union_ranges(blocks4)
    ranges8 = union_ranges(blocks8)

    total4 = sum(b - a for a, b in ranges4)
    total8 = sum(b - a for a, b in ranges8)
    print("[assets] pool 4bpp (base 0x%08X): %d rangos, %d bytes"
          % (POOL4_BASE, len(ranges4), total4))
    for a, b in ranges4:
        print("            off 0x%06X..0x%06X  ROM 0x%08X..0x%08X"
              % (a, b, POOL4_BASE + a, POOL4_BASE + b))
    print("[assets] pool 8bpp (base 0x%08X): %d rangos, %d bytes"
          % (POOL8_BASE, len(ranges8), total8))
    for a, b in ranges8:
        print("            off 0x%06X..0x%06X  ROM 0x%08X..0x%08X"
              % (a, b, POOL8_BASE + a, POOL8_BASE + b))

    # --- Generacion ----------------------------------------------------------
    out = []
    out.append("// ==============================================================================")
    out.append("// src/intro_assets.c - Assets del Estado 0 (intro/logo)")
    out.append("// ------------------------------------------------------------------------------")
    out.append("// GENERADO AUTOMATICAMENTE por tools/extract_intro_assets.py - NO EDITAR.")
    out.append("// Origen: baserom.gba (SHA-256 %s)" % ROM_SHA256)
    out.append("// ==============================================================================")
    out.append("")
    out.append('#include "intro_assets.h"')
    out.append("")

    emit_bytes(out, "g_intro_pal_obj_a", pal_a)
    emit_bytes(out, "g_intro_pal_obj_b", pal_b)
    emit_bytes(out, "g_intro_pal_main", pal_main)
    emit_bytes(out, "g_intro_pal_bg", pal_bg)
    emit_bytes(out, "g_intro_stream1", stream1)
    emit_bytes(out, "g_intro_stream2", stream2)
    emit_bytes(out, "g_intro_stream3", stream3)

    for idx, (a, b) in enumerate(ranges4):
        emit_bytes(out, "g_intro_pool4_r%d" % idx, rom.read(POOL4_BASE + a, b - a))
    for idx, (a, b) in enumerate(ranges8):
        emit_bytes(out, "g_intro_pool8_r%d" % idx, rom.read(POOL8_BASE + a, b - a))

    out.append("const IntroPoolRange g_intro_pool4_ranges[INTRO_POOL4_RANGE_COUNT] = {")
    for idx, (a, b) in enumerate(ranges4):
        out.append("    { 0x%06Xu, %du, g_intro_pool4_r%d }," % (a, b - a, idx))
    out.append("};")
    out.append("")
    out.append("const IntroPoolRange g_intro_pool8_ranges[INTRO_POOL8_RANGE_COUNT] = {")
    for idx, (a, b) in enumerate(ranges8):
        out.append("    { 0x%06Xu, %du, g_intro_pool8_r%d }," % (a, b - a, idx))
    out.append("};")
    out.append("")

    sizes = {
        "pal_a": len(pal_a), "pal_b": len(pal_b), "pal_main": len(pal_main),
        "pal_bg": len(pal_bg),
        "s1": len(stream1), "s2": len(stream2), "s3": len(stream3),
    }

    c_path = os.path.join(REPO_ROOT, "src", "intro_assets.c")
    h_path = os.path.join(REPO_ROOT, "include", "intro_assets.h")
    with open(c_path, "w", encoding="utf-8") as fh:
        fh.write("\n".join(out))
    emit_header(h_path, len(ranges4), len(ranges8), sizes)

    total = (len(pal_a) + len(pal_b) + len(pal_main) + len(pal_bg)
             + len(stream1) + len(stream2) + len(stream3)
             + total4 + total8)
    print("[assets] generado %s (%d bytes de datos)" % (c_path, total))
    print("[assets] generado %s" % h_path)


if __name__ == "__main__":
    main()
