# Project Context: Initial D - Another Stage (GBA) Decompilation & PC Port

## Project Goal
Decompile the Game Boy Advance ROM "Initial D: Another Stage" and build a native PC executable (`.exe`/ELF) using C, SDL2, and GCC, while maintaining full source-level GBA compatibility.

## Tech Stack & Toolchain
- **Target Hardware Architecture**: ARMv4T (Game Boy Advance).
- **Toolchain (GBA Build)**: `arm-none-eabi-gcc`, `arm-none-eabi-objcopy`.
- **Toolchain (PC Native Build)**: `gcc`, SDL2 (`libsdl2-dev`).
- **Reverse Engineering**: Ghidra 12.x + Ghidra-REA / MCP.
- **Environment**: Arch Linux.

### REA / Ghidra environment (required)
- REA's Ghidra provider needs a **64-bit full JDK 21+**. Set `JAVA_HOME=/usr/lib/jvm/java-21-openjdk` for the `rea` MCP server.
- Analysis of the full ROM overflows the default 2 GB heap in the ARM analyzer. Disable it via:
  `REA_GHIDRA_DISABLED_ANALYZERS="ARM Constant Reference Analyzer"` (exact analyzer name for Ghidra 12.1.2).
- Both are set in the `rea` MCP server `environment` block in `~/.config/opencode/opencode.jsonc`.

## Project Structure & Architecture
- `baserom.gba`: Clean Japanese ROM dump loaded at base address `0x08000000`.
- `Makefile`: Dual-target compilation (`make gba` -> `.gba`, `make pc` -> `initial_d_decomp_native`).
- `src/pc_main.c`: Native PC entry point: SDL2 window/event loop **and the VRAM/PALRAM renderer** (replaces the old "HOLA" test buffer).
- `src/gba_main.c`: GBA-specific hardware initialization.
- `src/init.c`: Ported cartridge boot sequence (crt0 + `AgbMain`). Portable logic plus per-target `hw_*` hardware shims (`#ifdef TARGET_GBA` / `#else` PC/SDL2).
- `include/init.h`: Boot sequence declarations, hardware/RAM constants, and the DMA copy descriptor table.
- `include/global.h`: Common type definitions and standard types (`u8`, `u16`, `u32`, `s8`, `s16`, `s32`).

## Ghidra Memory Mapping Rules
- **ROM Base Address**: `0x08000000` (cartridge space).
- **IWRAM**: `0x03000000` (Fast internal RAM, stack, hardware vectors).
- **EWRAM**: `0x02000000` (256KB board RAM).
- **IO Registers**: `0x04000000` (Display, DMA, Timers, Interrupts).
- **VRAM / PAL / OAM**: `0x06000000` / `0x05000000` / `0x07000000`.

## Memory & Hardware Map Verified
Verified by disassembling the real ROM bytes with REA/Ghidra (not inferred from decompiler output).
Beware: Ghidra pseudocode of the boot entry is misleading and was corrected below.

### Boot / entry
- **Entry point**: `0x08000000` = `b 0x080000C0`. `0x080000C0` is **`AgbMain`**.
- **`AgbMain` flow** (`0x080000C0`..`0x080002F4`):
  1. `*(0x080003A8 ptr) = REG_DISPSTAT` — save display status (Ghidra called this register VCOUNT; it is DISPSTAT `0x04000006`).
  2. `REG_WAITCNT` (`0x04000204`) = `0xC5B4`.
  3. **Stack setup (NOT an interrupt vector)** — the `_DAT_03007ffc = 0x3000000` line in Ghidra is actually CPSR/mode setup:
     - IRQ stack top: **`0x03007FA0`** (`mov sp,#0x03000000; add #0x7F00; add #0xA0`).
     - Then switches to System/User mode, USER stack top: **`0x03007F00`**.
  4. **IntrMain vector**: `*(u32*)0x03007FFC = 0x03000000` → **IRQ handler lives at `0x03000000`** (IWRAM).
  5. **12 DMA3 ROM→IWRAM copies** (dest, src, size); `r8 = 0x8400` (DMA3CNT_H: enable | 32-bit | immediate):
     | dst | src | size |
     |---|---|---|
     | `0x03000000` | `0x08000ADC` | `0x040` | (IRQ handler)
     | `0x03000100` | `0x0802625C` | `0x140` |
     | `0x03000600` | `0x0800133C` | `0x0C0` |
     | `0x03000900` | `0x08000FFC` | `0x080` |
     | `0x03000B00` | `0x08000F10` | `0x080` |
     | `0x03000D00` | `0x08001C08` | `0x080` |
     | `0x03000F00` | `0x0802F588` | `0x080` |
     | `0x03001100` | `0x080266BC` | `0x1A0` |
     | `0x03001780` | `0x0800157C` | `0x180` |
     | `0x03001D80` | `0x08001D4C` | `0x040` |
     | `0x03001E80` | `0x08026D34` | `0x040` |
     | `0x03001F80` | `0x08000E88` | `0x040` |
     Descriptor table lives at `0x080003AC`..`0x080003D8`. Note: `_DAT_040000d8 = &LAB_03001f80` in Ghidra is this copy's destination, **not** a DMA source pointer.
  6. Early engine init calls (from `0x08000280`, in order): `FUN_0802604C` (clears IWRAM high `0x03002080` + full VRAM `0x06000000`), `FUN_0801B27C` (SRAM save init), then `bl 0x0863DA1C` (thunk `ldr r12,[0x0863DA24=0x0863BFD5]; bx r12` → **`FUN_0863BFD4`**, Thumb entry `0x0863BFD5`, the **sound system init**).
  7. `*(u32*)(0x03004098 + 4) = 0x80` — boot flag (from `0x0800028C`).
  8. `bl 0x08026E0C` (OAM hide), then `ldr r2,[0x080003E0=0x08629EBD]; bx r2` → **`FUN_08629EBC`** (Thumb entry `0x08629EBD`, the **engine runtime init callback**).
  9. Boot-ready wait loop: writes 0 to `REG_IF` (`0x04000128`) and `0x04000134` (sound reg) while `REG_DISPSTAT == 0`.
  10. `REG_DISPCNT` (`0x04000004`) = `8` (forced blank); `REG_IE` (`0x04000200`) = `0x2001`; `REG_IME` (`0x04000208`) = `1`.
  11. **State jump-table**: `pc = 0x080002F8 + state * 4`, where `state = *(u32*)0x030027AC`.

### Key pointers referenced by the cartridge (ROM → RAM)
- `0x080003A8` → `0x030027C4` (DISPSTAT save).
- `0x080003DC` → `0x03004098` (boot flag base; `+4` = boot flag `0x80`).
- `0x080003E0` → `0x08629EBD` (Thumb boot callback = `FUN_08629EBC`, engine runtime init).
- `0x080003E4` → `0x030027AC` (**game state index** for the jump-table).

### I/O registers identified
- `0x04000000` `REG_DISPCNT`, `0x04000004` `REG_DISPSTAT`, `0x04000006` `REG_VCOUNT` (unused at boot).
- `0x040000D4/D8/DC/DE` `DMA3SAD/DAD/CNT_L/CNT_H` (32-bit blocks, `CNT_H = 0x8400`).
- `0x04000128` `REG_IF`, `0x04000134` sound register (cleared at boot).
- `0x04000200` `REG_IE` (`0x2001`), `0x04000204` `REG_WAITCNT`, `0x04000208` `REG_IME`.

### Ported subsystems (source modules)
| Original | Module | Subsystem | Notes |
|---|---|---|---|
| `FUN_0802604C` | `src/memory.c` (`memory_clear_boot_buffers`) | Memoria | Clears IWRAM high `0x03002080` (`0x5E7` ×16 B) + full VRAM `0x06000000` (`0x1800` ×16 B = 96 KB). No OAM/palette touched. |
| `FUN_0801B27C` | `src/save.c` (`save_init`) | Memoria/SAVE (SRAM) | Header signature `"INITIAL-D Another Stage for GBA"`; SRAM `0x0E000000`; work buffer EWRAM `0x02030000` (`0x2004`). |
| `FUN_0801B4C8` | `src/save.c` (`save_verify_checksum`) | Memoria/SAVE (SRAM) | **NOT a delay** (earlier mis-read). Byte-sum checksum over `0x2000` bytes (4 B × `0x800` iters) of the save work buffer, compared with the dword at `0x02032000` (buffer + `0x2000`). Original returns via CPU flags (`cmp`); port returns `bool`. |
| `FUN_08629EBC` | `src/engine.c` (`engine_runtime_init`) | Lógica/Runtime | Fills the engine vtable (25 slots `0x030066A4`..`0x03006704`) then calls `FUN_0862A028` (runtime state init). Touches **no I/O registers** — all writes are IWRAM. Invoked as the boot callback (`*0x080003E0 = 0x08629EBD`). |
| `FUN_08026E0C` | `src/graphics.c` (`graphics_hide_oam_sprites`) | Gráficos/OAM | Parks unused sprites off-screen: fills OAM entries `[*(0x03004098), 127]` with `attr0|attr1 = 0x000F02B0` (Y=176, X=240) and `attr2 = 0`. OAM base `0x07000000`, 8 B/entry. |
| `FUN_0863BFD4` | `src/audio.c` (`audio_system_init`) | Audio | Sound system init (Thumb, entry `0x0863BFD5`, called via thunk `0x0863DA1C`). SWI `0x0B` `SoundDriverInit`, sound regs (`SOUNDCNT_L/X`, `SOUNDBIAS`, `SOUND4CNT_H`, FIFO), driver work area `0x03007FF0` (magic `"Smsh"` = `0x68736D53`), and 4 music tracks (descriptors `0x08648B08`). |
| `0x080002E8` (dispatcher) | `src/states.c` (`states_dispatch`) | Lógica/Máquina de estados | Game State Machine: 44-entry jump-table at `0x080002F8`, index at `0x030027AC`, pending register at `0x03004A5C`. X-macro `GAME_STATE_LIST(X)` is the single source of truth (enum + descriptor table + per-state handlers). ROM branch words embedded and verified at runtime (0 discrepancies). |
| `FUN_080A52E4` (Estado 0) | `src/intro.c` (`intro_state_body`) | Lógica/Gráficos/Audio | Intro/logo: monta paletas por DMA3, descomprime tiles 4bpp/8bpp, registra callbacks OBJ y arranca el BGM pista 1. Flag de "ya inicializado" en `0x030047AC`. |
| `FUN_0854C5B8` / `FUN_08000E88` | `src/graphics.c` (`graphics_load_tiles4/8`) | Gráficos/VRAM | Descompresores de tiles de la ROM (32 B/tile en 4bpp, 64 B/tile en 8bpp). Cabecera leída una sola vez; celda `word==0` vacía; entrada de mapa = `slot + ((word & 0x8000) >> 5)` (+ `ip << 12` en 4bpp); el cursor de mapa avanza 64 B por fila (30 celdas escritas de 32). |
| `FUN_080AF768` | `src/graphics.c` (`graphics_register_obj_callback`) | Gráficos/OBJ | Tabla de 16 slots de 0x60 B en `0x03002080`; busca el 1º libre, escribe callback en `+0x00`, `+0x10/+0x14 = 0x10000` y pone a 0 el resto **excepto `+0x1C`** (la ROM no lo escribe). |
| `FUN_08028D60` | `src/graphics.c` (`graphics_register_object`) | Gráficos/OBJ | `*(0x030044D8) = 0x100`, `*(0x030044DC) = 0`, `*(0x030044E0) = descriptor`. |
| `FUN_0800055C` / `FUN_080005D4` | `src/audio.c` (`audio_bgm_play/stop`) | Audio | Selector de BGM: tabla de 12 descriptores en ROM `0x080005A4`; "pista actual" en `0x030027B4`; handle del driver `0x03007B60`. `movs r4,r4` solo actualiza flags (**no** trunca a 16 bits): el puntero de canción es completo. |
| `FUN_080A5188` | `src/intro.c` (`intro_common_init`) | Lógica/Gráficos | Init común: `FUN_0800048C` (2 palabras a 0), `FUN_08028D7C` (BLDY=16, BLDCNT=0x00FF, PALRAM[0]=0), `FUN_08026074` (borra VRAM 96 KB), `FUN_08000FE8` (`*(0x030027D8)=0`), `FUN_0802F2DC` (19 slots × 0x60 + 9 palabras a 0), `FUN_08028DA4` (BLDY/BLDCNT + bytes `+0x0C..+0x0F` del descriptor). |
| `FUN_08540B7C` | `src/intro.c` (`intro_common_update`) | Lógica | Update común del módulo de carrera: **stub documentado** (no es código de la pantalla de intro). |
| — (nuevo en PC) | `src/pc_main.c` (`render_bg0`) | Gráficos/Render | Renderizador de video del port PC: reproduce el BG0 del GBA desde `DISPCNT`/`BG0CNT` + VRAM + PAL RAM y convierte indexado → ARGB8888. Ver "Modelo de video medido del fondo 0". |

### Memoria mapeada (nueva capa)

`src/memory.c` expone una capa de acceso a IWRAM / VRAM / PAL RAM idéntica en ambos targets:
- GBA: puntero directo a la dirección real.
- PC: buffer virtual (IWRAM alta `0x03002080`..`0x03007EF0`, VRAM 96 KB, PAL RAM 1 KB).

API: `hw_mem_region_ptr`, `hw_mem_write8/16/32`, `hw_mem_write_block`, `hw_mem_read8/16/32`.
**Fuente única de verdad**: el índice de estado (`0x030027AC`) y la solicitud pendiente (`0x03004A5C`) ya no tienen buffers propios en `states.c`; se acceden con `hw_mem_read32`/`hw_mem_write32`.

### Estado 0 — intro/logo (`0x080A52E4`)

Flujo verificado (objdump):
```
0x080A52E4  *0x030027AC = 0
0x080A52F0  DISPCNT = 0x0040
0x080A52FC  if (*0x030047AC != 0) -> 0x080A53BC     ; flag "ya inicializado"
0x080A530C  *0x030047AC = 1
0x080A5314  bl 0x080A5188                           ; init comun
0x080A5318  bl 0x080005D4                           ; BGM stop
0x080A5324  DISPCNT = 0x0140
0x080A532C  BG0CNT  = 0x1F00
0x080A5340  bl 0x0854C5B8 (r1=0x080A6174, r2=0x0600F800, sl=1)   ; stream 4bpp #1
0x080A534C  DMA3 0x080A5934 -> 0x05000000, CNT_L=8, CNT_H=0x8400  ; paleta OBJ A
0x080A536C  bl 0x080AF768 (r4=0x080A5580)           ; callback OBJ #1
0x080A5370  bl 0x08540B7C
0x080A5374  bl 0x080A5188                           ; init comun (¡borra los slots!)
0x080A5388  bl 0x0854C5B8 (r1=0x080A6628, sl=1)     ; stream 4bpp #2
0x080A5394  DMA3 0x080A5954 -> 0x05000000, CNT_L=8, CNT_H=0x8400  ; paleta OBJ B
0x080A53B4  bl 0x080AF768 (r4=0x080A55EC)           ; callback OBJ #2
0x080A53B8  bl 0x08540B7C
0x080A53BC  BLOQUE COMUN (cada iteracion):
0x080A53BC  bl 0x080A5188
0x080A53D0  bl 0x08000E74 (r1=0x080A6ADC, sl=1)     ; stream 8bpp
0x080A53D8  bl 0x08028D60 (r3=0x080A5974)           ; objeto de fondo
0x080A53E4  DMA3 0x080A5B74 -> 0x05000200, CNT_L=0x80, CNT_H=0x8400 ; paleta 256 col. (region OBJ)
0x080A5408  DISPCNT = 0x1140
0x080A5410  BG0CNT  = 0x1F80
0x080A5418  bl 0x080AF768 (r4=0x080A547C)           ; callback OBJ #3
0x080A5420  bl 0x0800055C (r4=1)                    ; BGM pista 1
0x080A5424  bl 0x08540B7C
0x080A5428  r2 = *0x03004A5C ; if (r2 == 0) -> 0x080A5314 (repetir)
0x080A543C  *0x030027AC = pending ; b 0x080002E8
```

- **Cada `bl 0x080A5188` borra la tabla de slots OBJ**, así que los 3 callbacks terminan registrados uno tras otro en el slot 0: el estado final solo conserva el callback #3 (`0x080A547C`). El port reproduce ese efecto (cada `intro_common_init()` limpia los 19 slots).
- **`PALRAM[0]` queda a 0**: `FUN_08028D7C` lo pone a 0 en cada init común, y la última llamada ocurre *después* de la copia de la paleta OBJ B.
- Los callbacks de dibujado (`0x080A5580`, `0x080A55EC`, `0x080A547C`) son **código ARM de la ROM original**: solo se registra su dirección en la tabla de slots; sus cuerpos no forman parte de este port.

**Assets del Estado 0** (direcciones ROM):
| Asset | ROM addr | Tamaño |
|---|---|---|
| Paleta OBJ A (16 col.) | `0x080A5934` | 32 B |
| Paleta OBJ B (16 col.) | `0x080A5954` | 32 B |
| Paleta BG (256 col.) | `0x080A5B74` | 512 B (16 colores reales + 240 de relleno verde) |
| Stream 4bpp #1 | `0x080A6174` | 1204 B (cabecera 4 B + 600 words) |
| Stream 4bpp #2 | `0x080A6628` | 1204 B |
| Stream 8bpp | `0x080A6ADC` | 1202 B (cabecera 2 B + 600 words) |
| Descriptor objeto fondo | `0x080A5974` | 8 B |

**Pools de tiles** (bases distintas, verificadas con los literales ARM de cada descompresor):
- 4bpp: literal en `0x0854C6B8` → base **`0x0802FB28`**, tile de 32 B. Rangos usados: 4 (9504 B): `0x004780+32`, `0x00D420+32`, `0x013020..0x0145A0`, `0x014880..0x0157E0`.
- 8bpp: literal en `0x08000F00` → base **`0x08556B0C`**, tile de 64 B. Rangos usados: 2 (36 160 B): `0x01CE40+64`, `0x037300..0x040000`.

> ⚠️ El pool 4bpp **no** está en `0x08556B0C`: usar la base equivocada produce tiles planos (constantes `0x66/0x33/0x55/0xFF`).

Los assets se embeben como arrays C (`src/intro_assets.c`, 49 850 B) generados por `tools/extract_intro_assets.py`, que verifica el SHA-256 de `baserom.gba`. Así ambos targets funcionan sin la imagen de ROM en runtime, y en GBA las copias por DMA3 siguen siendo transferencias ROM → PAL RAM / VRAM.


### Modelo de video medido del fondo 0 (renderizador PC)

El renderizador de `src/pc_main.c` reproduce el fondo 0 del GBA leyendo los registros y la memoria emulados. Todo lo de abajo está **medido sobre los bytes de la ROM**, no inferido.

**Reglas del hardware implementadas**
| Fuente | Bits | Significado |
|---|---|---|
| `REG_DISPCNT` (`0x04000000`) | 0-2 | modo de video |
| | 7 | forced blank (pantalla blanca) |
| | 8 | BG0 habilitado |
| | 12 | OBJ habilitado |
| `REG_BG0CNT` (`0x04000008`) | 2-3 | bloque de base de tiles (16 KB) |
| | 6-7 | `0b10` = 256 colores (8bpp, tile de 64 B); `0b00/0b01` = 16 colores (4bpp, tile de 32 B) |
| | 8-12 | bloque de base del tilemap (2 KB) |
| | 14-15 | tamaño del mapa (32x32 / 32x64 / 64x32 / 64x64 tiles) |
| Entrada del tilemap (16 bits) | 0-9 | número de tile |
| | 10-12 | banco de paleta |
| | 12 / 13 | volteo H / V |

**Decisiones medidas (no supuestas)**
- **La capa del Estado 0 es 8bpp, no 4bpp.** `BG0CNT = 0x1F80` → bits 6-7 = `0b10` (256 colores) y los tiles miden 64 B. Confirmado empíricamente: interpretando el mismo tilemap como 4bpp la imagen da **95.3 % de bordes** (ruido); como 8bpp da **15.5 %** (imagen estructurada).
- **Regla A de indexado de paleta (8bpp): `color = pal[valor]`.** El byte del píxel es el índice directo de la paleta de 256 entradas y **el nibble alto hace de banco** (16 bancos de 16). Descartada la Regla B (`pal[valor & 15]`) porque con el Estado 9 esa variante colapsa la imagen a 9 colores frente a 54 de la Regla A (y solo 107 de los 256 valores se usan, con bancos 0-7 y 12 activos).
- **Región de paleta: `0x05000000` por defecto, también en 8bpp.** Un BG del GBA solo puede leer la paleta en `0x05000000` (la región BG); `0x05000200` es la región de OBJ y **ningún fondo del hardware la consulta**. Por eso el renderizador usa `0x05000000` para cualquier profundidad. Con la tecla `P` (o `--palette-obj`) se cambia a `0x05000200` para comparar.
- **Espejo de la paleta de 256 colores (solo target PC, `src/intro.c`).** La capa de 8bpp del Estado 0 la dibuja la ROM **por software** (el "objeto de fondo" de `FUN_08028D60`, descriptor `0x080A5974`) usando la paleta de 256 colores que la ROM copia a `0x05000200`; el BG0 del hardware solo puede leer `0x05000000`, donde quedó la paleta B de 16 colores de las capas 4bpp. Como el port PC no ejecuta los callbacks OBJ (`0x080A5580` / `0x080A55EC` / `0x080A547C`, código ARM de la ROM), **el port copia los 512 bytes de `0x05000200` a `0x05000000`** justo después del DMA3 de `0x080A53E4`, para que el fondo 0 muestre los colores reales del logo. En el **target GBA no se hace** (`#ifndef TARGET_GBA`): el cartucho no espeja nada y el hardware leería la paleta B de 16 colores. Efecto secundario medido: `PALRAM[0]` pasa de `0x0000` a `0x03E3` (verde), pero en 8bpp no hay píxeles transparentes, así que el fondo no se ve.
- **Conversión BGR555 → ARGB8888** (`bgr555_to_argb`): decodificación `rojo = (c >> 0) & 0x1F`, `verde = (c >> 5) & 0x1F`, `azul = (c >> 10) & 0x1F`, expandidos a 8 bits con `(v << 3) | (v >> 2)`; empaquetado `0xFF000000 | (rojo << 16) | (verde << 8) | azul` (ARGB8888 = `0xAARRGGBB`: bits 24-31 alfa, 16-23 rojo, 8-15 verde, 0-7 azul). **Ojo: invertir rojo y azul en el empaquetado no se detecta en colores simétricos** (magenta, blanco, grises) y produce el "verde dominante" característico: `0x03E3` se veía como `#00FF18` en vez de `#18FF00`, y el cian `0x7FE0` como `#FFFF00` en vez de `#00FFFF`. Todos los colores de la captura deben mapear 1:1 a una entrada de la paleta de la ROM (verificado).

**Unidad de blend (BLDCNT / BLDY) — implementada y medida**
- Direcciones reales: **`REG_BLDCNT = 0x04000050`**, **`REG_BLDY = 0x04000054`** (no `0x40`/`0x42`; verificado en el objdump de `FUN_08028D7C`: `strh r5,[r6,#84]` y `strh r5,[r6,#80]` con `r6 = 0x04000000`).
- Estructura de `BLDCNT`: bits 0-5 = 1.er objetivo (0=BG0, 1=BG1, 2=BG2, 3=BG3, 4=OBJ, 5=fondo); **bits 6-7 = modo** (0=sin efecto, 1=alpha, 2=aclarar, 3=oscurecer); bits 8-13 = 2.º objetivo; bit 14 = sprites en VRAM de OBJ.
- Semántica aplicada por `bld_apply()` (idéntica al hardware): modo 3 → `color * BLDY/16` (solo si `BLDY < 16`); modo 2 → mezcla con blanco `(BLDY-16)/15` (solo si `BLDY > 16`); modo 1 → mezcla 1.er objetivo contra 2.º (o contra el fondo) según `BLDY`.
- **Valores reales del Estado 0** (`FUN_08028D7C` y `FUN_08028DA4`, verificados): `BLDY = 16`, `BLDCNT = 0x00FF`. Decodificado, `0x00FF` = objetivos `0x3F` (todos) + **modo 3 (oscurecer)** + 2.º objetivo BG0 (bit 8). **Con `BLDY = 16` el modo "oscurecer" no atenúa** (`16/16 = 1.0`), así que la imagen del Estado 0 **no cambia** — verificado: la captura con BLD es byte a byte idéntica a la captura sin BLD.
- El fundido real lo produce el motor **bajando `BLDY` frame a frame** desde 16 hasta 0; ese código por frame (`FUN_08540B7C` y el runtime del motor) **todavía no está portado**, por eso la intro no se ve atenuarse.
- Se aplica a los píxeles de BG0 y al fondo (backdrop = `PALRAM[0]`); en 4bpp el índice 0 es transparente, así que ese píxel se modula como fondo, no como capa BG0.
- Override de depuración: tecla `B` (ciclo normal → oscurecer → aclarar → sin efecto) y `--bld none|brighten|darken|auto`. Comprobación numérica: con `BLDY=8` cada canal se divide por 2 (`#E7E7E7` → `#737373`); con `BLDY=24` se mezcla con blanco al 8/15 (`#737373` → `#BDBDBD`). Los valores de BLDCNT/BLDY se muestran en la barra de estado (`BLD <modo> <BLDY>`) y en la traza por consola.

**Discrepancia real de la ROM (documentada, no es un error del port)**
- El **Estado 0** copia su paleta de 256 colores a `0x05000200` (región OBJ) — verificado en `0x080A53E8`: `mov r5,#0x05000000` / `add r5,#0x200` → `DMA3DAD = 0x05000200`.
- El **Estado 9** copia la suya a `0x05000000` (región BG) — verificado en `0x080A56D0`: `mov r5,#0x05000000` → `DMA3DAD = 0x05000000`.
- Un BG real del GBA solo puede leer `0x05000000`, así que **en hardware la capa 8bpp del Estado 0 se vería con la paleta de 16 colores que quedó en `0x05000000`** (negro + logo en cian/gris/blanco), no con la de 256 colores. La capa de 8bpp del Estado 0 es, por tanto, una capa **renderizada por software** (el "objeto de fondo" que registra `FUN_08028D60` con el descriptor de `0x080A5974`), no el BG0 del hardware.
- Además `DISPCNT = 0x1140` declara **modo 0** (BG de 16 colores) mientras `BG0CNT` pide 256 colores: el juego usa `BG0CNT` como selector de profundidad real. No existe en toda la ROM ningún `mov rN,#0x1144` (modo 4 + BG0); los únicos candidatos a modo 4 son `0x3FC`, `0x12C`, `0x364` y `0x384`.
- **Resolución en el port PC**: el espejo de paleta descrito arriba deja el fondo 0 con los colores reales de la intro (verde/magenta/amarillo) sin depender de teclas de depuración. El target GBA conserva el comportamiento del cartucho (sin espejo).
- El renderizador ofrece las teclas `P` (cambiar de región de paleta), `M` (forzar 4bpp) y `B` (override de BLD) para comparar las variantes en vivo.

**Estado real de la PAL RAM emulada del Estado 0** (volcado del binario, sin el espejo)
| Índice | `0x05000000` (BG) | `0x05000200` (OBJ) |
|---|---|---|
| 0 | `0x0000` (**forzado a 0** por `FUN_08028D7C`, que corre después de la copia de la paleta B) | `0x03E3` verde |
| 1 | `0x739C` gris claro | `0x53FF` amarillo pálido |
| 2 | `0x5EF7` gris | `0x031F` naranja |
| 3 | `0x39CE` gris oscuro | `0x001F` rojo |
| 4 | `0x0000` | `0x0000` negro |
| 5 | `0x7FFF` blanco | `0x7C1F` magenta |
| 6-15 | `0x7FE0` cian | `0x7C1F` magenta |
| 16-255 | `0x0000` (nunca escrito) | `0x03E3` verde (relleno) |

> Con el espejo del port PC, `0x05000000` pasa a contener exactamente los mismos 256 valores que `0x05000200` (16 colores reales + 240 de relleno verde).

**Renderizador (`src/pc_main.c`)**
- `render_bg0()` → `bgr555_to_argb()` → framebuffer ARGB8888 → textura `SDL_PIXELFORMAT_ARGB8888` escalada con `SDL_ScaleModeNearest`.
- Layout del mapa por bloques de 32x32 tiles: `32x32` → bloque 0; `32x64` → bloques 0 y 2; `64x32` → bloques 0 y 1; `64x64` → bloques 0, 1, 2 y 3.
- En 4bpp el índice es `banco * 16 + nibble` y el índice 0 es **transparente** (se ve el backdrop = `PALRAM[0]`).
- Barra de estado con fuente 5x7 propia (`STATE nn / DISPCNT / BG0CNT / 8BPP|4BPP / dirección de paleta / BLD modo BLDY`).
- Captura BMP de 24 bits (`save_bmp`), atajos `P`/`M`/`B`/`S`/`ESC` y opciones `--screenshot <f.bmp>`, `--hold`, `--palette-obj`, `--4bpp`, `--bld none|brighten|darken|auto`.
- **Los OBJ no se renderizan**: los callbacks `0x080A5580` / `0x080A55EC` / `0x080A547C` son código ARM de la ROM y solo se registra su dirección en la tabla de slots.

**Capturas de referencia verificadas** (`SDL_VIDEODRIVER=dummy ... --screenshot`)
| Variante | Colores | Colores y pesos | Bordes | Lectura |
|---|---|---|---|---|
| 8bpp, paleta `0x05000000` (**por defecto, con espejo**) | 6 | `#18FF00` 79.4 %, `#FF00FF` 10.0 %, `#FFFFA5` 5.4 %, `#000000` 2.5 %, `#FFC600` 2.3 %, `#FF0000` 0.5 % | 15.5 % | **colores reales de la intro**: fondo verde + logo en magenta/amarillo/naranja/rojo |
| 8bpp, paleta `0x05000200` (`--palette-obj`) | 6 | idéntica a la fila anterior | 15.5 % | con el espejo ambas regiones contienen la misma paleta |
| 8bpp **sin espejo** (referencia del hardware GBA) | 6 | `#000000` 81.8 %, `#00FFFF` 9.3 %, `#E7E7E7` 5.4 %, `#BDBDBD` 2.3 %, `#FFFFFF` 0.8 %, `#737373` 0.5 % | 15.7 % | lo que vería el BG0 del hardware: paleta B de 16 colores |
| 4bpp forzado (`--4bpp`) | 6 | — | **95.3 %** | ruido → confirma que la capa es 8bpp |
| 8bpp + BLD del juego (`BLDCNT=0x00FF`, `BLDY=16`) | 6 | idéntica a la primera fila | 15.5 % | modo 3 sin atenuación (`16/16 = 1.0`) |
| 8bpp + `--bld darken` (`BLDY=8`) | 6 | `#0C7F00` 79.4 %, `#7F007F` 10.0 %, `#7F7F52` 5.4 %, `#000000` 2.5 %, `#7F6300` 2.3 %, `#7F0000` 0.5 % | 15.5 % | cada canal × 8/16 (p. ej. `#18FF00` → `#0C7F00`) |
| 8bpp + `--bld brighten` (`BLDY=24`) | 6 | `#93FF88` 79.4 %, `#FF88FF` 10.0 %, `#FFFFD5` 5.4 %, `#888888` 2.5 %, `#FFE488` 2.3 %, `#FF8888` 0.5 % | 15.5 % | mezcla con blanco al 8/15 (p. ej. `#18FF00` → `#93FF88`) |

> Los colores de las capturas de 8bpp se comprobaron uno a uno contra las entradas de la paleta de 256 colores de la ROM (`0x080A5B74`): **todos** mapean 1:1 (ver `tools`-style check en `/tmp/opencode/palette_check.py`).


### Engine runtime layout (verified with REA)
- **vtable**: 25 dwords at `0x030066A4`..`0x03006704`, each `*slot = <ROM function address>`.
- **`FUN_0862A028`** runtime state: writes at `0x03004838`, `0x03004A10`, `0x03004DC8`, `0x03004E28`, `0x03004E88`, `0x03004E94`, `0x03004EA0`, `0x030066A8`..`0x030068B8`; `memset(0x03004838, 0, 0x0845A918 << 2)` and `memset(0x03004EA0, 0, 0x8C)`.
- Helpers: `FUN_0862D800` = `memset(dst, val, size)`; `FUN_0862D840` = word-fill; `FUN_0862D154` = state defaults (`0x030068AC <- 0x030068A0`, `0x030068B8 <- 2`, rest 0).

## Game State Machine (verified with objdump on `baserom.gba`)

### Dispatcher (`0x080002E8`, reached from `AgbMain` step 11)
```
0x080002E8  ldr   r3, [pc, #244]   @ 0x080003E4   ; r3 = 0x030027AC
0x080002EC  add   r2, pc, #4                      ; r2 = 0x080002F8 (tabla)
0x080002F0  ldr   r1, [r3]                       ; r1 = *0x030027AC (indice)
0x080002F4  add   pc, r2, r1, lsl #2              ; salta a la ranura
```
- **Index register**: `0x030027AC` (4 bytes, little endian).
- **Pending register**: `0x03004A5C` — "solicitud pendiente de cambio de estado". Verified from state 0's literal pool (`0x080A5474` = `0x03004A5C`, `0x080A5478` = `0x030027AC`). Handlers with dynamic transitions do `*0x030027AC = *0x03004A5C` before returning.
- **Table**: `0x080002F8`..`0x080003A4` — **44 slots, and they are ARM `b` instructions (`0xEA......`), not raw pointers**. Decode: `target = slot + 8 + imm24*4` (imm24 sign-extended from 24 bits).
- **Cooperative model**: each handler writes the new index into `0x030027AC` and branches back to `0x080002E8` (never returns). Handlers are per-screen loops that wait for frames and poll input.

### Slot → handler map (44 entries, exact ARM branch words)
| idx | word | target | idx | word | target |
|---|---|---|---|---|---|
| 0 | `0xEA0293F9` | `0x080A52E4` | 22 | `0xEA113F94` | `0x084501A8` |
| 1 | `0xEA028EEB` | `0x080A3EB0` | 23 | `0xEA14EE32` | `0x0853BC24` |
| 2 | `0xEA027411` | `0x0809D34C` | 24 | `0xEA028F8C` | `0x080A4190` |
| 3 | `0xEA116942` | `0x0845A814` | 25 | `0xEA028FAB` | `0x080A4210` |
| 4 | `0xEA000071` | `0x080004D4` | 26 | `0xEA02B491` | `0x080AD5AC` |
| 5 | `0xEA1508FB` | `0x08542700` | 27 | `0xEA113F8F` | `0x084501A8` |
| 6 | `0xEA007C91` | `0x0801F55C` | 28 | `0xEA150174` | `0x08540940` |
| 7 | `0xEA15404A` | `0x08550444` | 29 | `0xEA15026D` | `0x08540D28` |
| 8 | `0xEA154B12` | `0x08552F68` | 30 | `0xEA153DDE` | `0x0854FAF0` |
| 9 | `0xEA0294D3` | `0x080A5670` | 31 | `0xEA114FD2` | `0x084542C4` |
| 10 | `0xEA006E2F` | `0x0801BBE4` | 32 | `0xEA028CA0` | `0x080A3600` |
| 11 | `0xEA025A87` | `0x08096D48` | 33 | `0xEA14FF3C` | `0x08540074` |
| 12 | `0xEA02BD5A` | `0x080AF898` | 34 | `0xEA113CED` | `0x0844F73C` |
| 13 | `0xEA009AE9` | `0x08026ED8` | 35 | `0xEA006E16` | `0x0801BBE4` |
| 14 | `0xEA007CB8` | `0x0801F618` | 36 | `0xEA15388C` | `0x0854E5C0` |
| 15 | `0xEA025DAE` | `0x080979F4` | 37 | `0xEA028CB2` | `0x080A365C` |
| 16 | `0xEA0262A9` | `0x08098DE4` | 38 | `0xEA02BD40` | `0x080AF898` |
| 17 | `0xEA026F54` | `0x0809C094` | 39 | `0xEA02BD3F` | `0x080AF898` |
| 18 | `0xEA02AA08` | `0x080AAB68` | 40 | `0xEA029EA2` | `0x080A7E28` |
| 19 | `0xEA153430` | `0x0854D40C` | 41 | `0xEA150272` | `0x08540D6C` |
| 20 | `0xEA15365D` | `0x0854DCC4` | 42 | `0xEA150260` | `0x08540D28` |
| 21 | `0xEA1138B2` | `0x0844E61C` | 43 | `0xEA025D92` | `0x080979F4` |

### Verified transitions (constant writes to `0x030027AC`)
`1→23, 2→9, 3→2, 6→{36,35}, 7→2, 9→{40,2}, 10→36, 14→13, 15→29, 16→{36,6}, 17→36, 20→8, 23→2, 24→{28,33}, 25→33, 28→8, 30→43, 31→2, 34→8, 35→36, 40→0, 41→6, 43→29`.
All remaining states transition dynamically through `0x03004A5C` (state 4 writes no state index at all — it is a pure BGM helper).

### Module families (by ROM address range)
- `0x0801xxxx` — save/data UI (works on `0x02032004`, `0x02030040`).
- `0x08026xxx` — ranking / records (`0x08026ED8`).
- `0x08096xxx`..`0x0809Dxxx` — menu module (shared widgets `0x08098720`, `0x080988D0`, `0x08098A84`, `0x08098D00`).
- `0x080A3xxx`..`0x080ADxxx` — title/menu family.
- `0x080AFxxx` — records/data (`0x080AF898`).
- `0x0844xxxx`/`0x0845xxxx` — scene/story.
- `0x0853xxxx`..`0x0855xxxx` — race module.
- Shared helpers: `0x080A5188` (init), `0x080AF768` (VRAM/OBJ), `0x08540B7C` (common update), `0x0800055C`/`0x080005D4` (BGM select/stop), `0x080003E8`/`0x0800048C`/`0x080004AC` (boot/flag helpers).

### Naming caveat
The ROM's text uses a **custom font encoding**, so state names are **not readable from strings**. Names in `include/states.h` are **inferred** from structural evidence (RAM/ROM literals, callees, DISPCNT constants, transition graph) — they are labels for the developer, not extracted strings.

### Portable implementation (`src/states.c`, `include/states.h`)
- `GAME_STATE_LIST(X)` X-macro = single source of truth → enum `GameStateId`, descriptor table `g_game_states[44]`, and one handler function per state (`state_<NN>_<sym>`).
- **Trap**: the X-macro expands to a **single logical line**, so generated *function definitions* must end with `;` (not `,` — `,` is only valid inside initializer lists).
- **Trap**: state indices are plain decimal (`0`..`43`); never use `08`/`09` (C octal literal trap).
- `states_enter()` applies the verified transition in priority order: constant → alternative constant → dynamic pending (`0x03004A5C`, value `0` = "no request") → demo advance (PC only).
- `states_verify_table()` decodes the embedded original ARM branch words and compares them against the descriptor table (0 discrepancies at runtime).
- ARM symmetry verified in the GBA build: `states_rom_slot_address` compiles to `0x080002F8 + (idx << 2)`; `hw_state_index_read` loads literal `0x030027AC`; `hw_state_pending_read` loads `0x03004A5C`; `states_dispatch` does index load → `lsls #2` → table lookup → indirect `bl` (mirrors `add pc, r2, r1, lsl #2`).
- PC-only "demo advance" mode (`states_set_demo_advance`, default ON for PC, OFF for GBA) sweeps all 44 states linearly so the whole table can be inspected.

## Build notes / gotchas
- **`TARGET_GBA` must be defined for the GBA build** (`-DTARGET_GBA` in `ARM_CFLAGS`). It was missing historically, so `make gba` silently compiled the **PC/SDL2 branches** of every module. After the fix the ROM dropped from 53 KB → 18 KB and the ELF contains no `printf`/stdio. Current size with the embedded intro assets: `.text` 64 276 B, `.data` 1 372 B, `.bss` 848 B, `.gba` 69 752 B.
- `src/pc_main.c` is **excluded from the GBA build** (`COMMON_SRCS := $(filter-out src/pc_main.c src/gba_main.c, ...)`): it is the SDL2 renderer and solo compila para PC.
- `include/init.h` includes `states.h`; the state addresses live only in `states.h` (do not duplicate them).
- Hex literals for ROM addresses need **exactly 8 digits** (`0x080005A4`, not `0x08005A4`).
- The generated `.gba` still uses the default newlib crt0: **no Nintendo logo header and the entry point is not at `0x08000000` branching to `AgbMain`**, so it is not bootable on real hardware yet.
- GBA `hw_state_frame_wait()` uses a calibrated busy loop (not VBlank sync) until `IntrMain`/IRQ is ported.
- `make gba` emits **newlib "not implemented" warnings** (`_close`, `_lseek`, `_read`, `_write`) al enlazar `libc.a` con `--specs=nosys.specs`: son avisos del enlazador de newlib, **no errores del port**. El target PC no los produce.

## Decompilation Rules for the Agent
1. **Clean Architecture**: When extracting functions from Ghidra, separate hardware-dependent logic (registers `0x0400xxxx`) from pure game logic (physics, state machine, menus).
2. **Type Mapping**: Convert Ghidra types (`undefined4`, `uint`, etc.) to explicit GBA fixed-width types (`u32`, `u16`, `u8`, `s32`, `s16`, `s8`).
3. **Function Isolation**: Keep function signatures identical to their original decompilation where possible, isolating GBA I/O into wrapper functions so they can be stubbed or emulated in `src/pc_main.c` via SDL2.