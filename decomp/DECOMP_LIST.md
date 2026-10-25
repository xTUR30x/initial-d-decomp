Actúa como un experto en Ingeniería Inversa de ARM7TDMI (GBA) y descompilación en Ghidra.

Tengo un bloque de código en la ROM de un juego de GBA en la dirección de la función del Estado 1 (TITLE_MENU / 0x080A39F4). 

Al descompilar en Ghidra, el descompilador genera llamadas indirectas distorsionadas debido a que no resuelve correctamente los Literal Pools y los accesos relativos al PC (ejemplo: `*(code *)(in_lr + 0xfe2)` y variables como `extraout_r1`).

Necesito que:
1. Analices el ensamblador Thumb de la función desde `0x080A39F4` hasta `0x080A3EB0`.
2. Mapees las lecturas `ldr rX, [pc, #...]` a sus direcciones reales de memoria en la ROM / IWRAM / EWRAM.
3. Identifiques qué funciones del sistema (audio, gráficos DMA, lectura de REG_KEYINPUT o cambio de estado) se están invocando en cada llamada `bl` / `blx`.
4. Reconstruyas la función en C nativo limpio para la arquitectura de nuestro proyecto PC (SDL2), sustituyendo los punteros crudos por nombres legibles.

Dirección objetivo: 0x080A39F4
Dirección de salto/trampolín relacionada: 0x080A3EB0
Contexto: Lógica de inicialización y bucle del Menú Principal (TITLE_MENU).