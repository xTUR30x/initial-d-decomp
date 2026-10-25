/*
# Módulo: Inicialización y Control de la Escena de Historia (Acto 1)

- Dirección ROM: 0x0844E800 (Padre / Setup: 0x0844E61C)
- Nombre sugerido: `state_21_story_scene_init` / `state_21_story_scene_update`
- Tipo: State Life-Cycle & Text Scene Engine Setup

Lógica principal:
1. Navega por el árbol de punteros del evento desde la ROM (`param_4 + 0x10 -> 0x1F -> 0x74`) para extraer los offsets del diálogo y activos gráficos del Acto 1.
2. Calcula y aplica coordenadas en punto fijo (Fixed Point `<< 0xf`, `<< 0x15`, `* 0x40`) para posicionar la caja de texto y los retratos de los personajes.
3. Extrae la referencia al buffer de diálogo desde el offset `0x34` (`*(int *)(iVar1 + 0x34)`) e inicializa el mapa de caracteres y la velocidad de paso de texto (`0x2C`).
4. Entra en el bucle principal de control de la escena (en GBA bloquea por interrupción VBlank / en C gestiona eventos por frame).

Uso en el Port Native C:
- Implementar las funciones de ciclo de vida en `src/story.c` y exponer en `src/story.h`.
- Registrar `state_21_story_scene_init` y `state_21_story_scene_update` en la Jump Table de estados (`src/states.c`).
- Reemplazar el bucle infinito por la gestión asíncrona de eventos de SDL2 (lectura de KEY_A / KEY_START para avanzar diálogos o pasar a la carrera STATE 5 / 28).
- Redirigir el manejador de entrada de la Intro (STATE 0 / 36) directamente hacia STATE 21 en lugar de STATE 1 (Menú Principal) para habilitar la Vertical Slice.
*/

void FUN_0844e800(undefined4 param_1,undefined4 param_2,int param_3,int param_4)

{
  int iVar1;
  int unaff_r4;
  int iVar2;
  
  iVar1 = *(int *)(*(byte *)(*(byte *)(param_4 + 0x10) + 0x1f) + 0x74);
  *(short *)(param_3 * 2) = (short)unaff_r4;
  *(short *)(iVar1 * 2) = (short)iVar1;
  iVar2 = param_3 * 0x40;
  *(int *)(iVar1 + 0x30) = unaff_r4 << 0xf;
  *(int *)(iVar1 + 0x30) = param_3 << 0x15;
  *(char *)(param_3 + 8) = (char)iVar2;
  *(int *)(param_3 + 0x18) = param_3 << 0x15;
  *(int *)(iVar2 + 8) = iVar2;
  iVar2 = *(int *)(iVar1 + 0x34);
  *(uint *)(iVar2 + 0x2c) = (uint)*(byte *)(iVar1 + 0x1f);
  *(short *)(iVar2 + 8) = (short)iVar2;
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}

