
## Estado 1 - TITLE_MENU (0x080A39F4 / 0x080A3EB0)

### Punto de entrada
- Dispatcher: slot 1 en `0x080002FC` → `0x080A3EB0` (ARM)
- Flags: `GAME_STATE_FLAG_MUSIC | GAME_STATE_FLAG_CUSTOM`
- Siguiente estado típico: `23` (TIMETABLE) o flujo hacia `21` (Historia)

### Trampolín ARM 0x080A3EB0
Literal pool `0x080A3F44+`:
- `0x03006758` → copiado a `0x030047B8`
- `0x030067B0` → leído, testado; valor guardado en `0x030046F0+0x10` y `0x03004A24`
- `0x030046F0` → base para OAM/animación (`+0x4`, `+0x10`)
- `0x03006760` → copiado a `0x030046F0+0x4`
- `0x08622E05` (Thumb) → llamado via `bx`
- `0x030027AC` (índice de estado)
- `0x080A3F90` = `0x7FFF03E0` (paleta menú)

### Llamadas por frame
1. `bl 0x080A51A8` - Setup inicial
2. `bl 0x080A4A08` - Setup OAM/tabla OBJ
3. `bl 0x080A5278` - Setup buffers
4. `bl 0x080003E8` - Frame/sync
5. `bl 0x080A5030` - **Lectura de input (KEYINPUT)** 
6. `ldr r4,=0x08622E05; bx r4` - **Lógica principal Thumb**
7. `bl 0x0802F354` - Procesa callbacks tabla OBJ `0x03002080`
8. `bl 0x08098720` - Widget menú
9. `bl 0x080988D0` - Widget menú  
10. `bl 0x08098A84` - Widget menú
11. `bl 0x08098D00` - Widget menú
12. `bl 0x080260A0` - Update OAM
13. `bl 0x0844E2CC` - Auxiliar
14. `bl 0x080004AC` - Frame advance
15. Loop: vuelve a `0x080A3EFC` mientras condición

### IWRAM usado
`0x030046F0`, `0x030047B8`, `0x03004A24`, `0x03006758`, `0x03006760`, `0x030067B0`

### Implementación en port
- Init CUSTOM en `src/states.c` (STATE 1): limpia VRAM, carga paleta `0x080A3F90` via DMA3, configura BG0/DISPCNT, arranca BGM, `fade_in_all()`
- Cuerpo CUSTOM: usa abstracciones (`fade_update()`, `input_is_pressed()`)
- `src/menu.c` creado con esqueleto basado en análisis
