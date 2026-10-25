/*
# Módulo: Reset de Memoria y VRAM

- Dirección ROM: 0x0802604C
- Nombre sugerido: `vram_and_callbacks_clear`
- Tipo: Memory Wipe / Scene Reset

Lógica principal:
1. Pone a cero la tabla de callbacks de objetos en IWRAM (0x03002080).
2. Limpia los 96 KB enteros de la VRAM (0x06000000) rellenando con 0.

Uso en el Port Native C:
- Implementar en `src/gfx.c`.
- Abstraer a un `memset` de las estructuras simuladas de VRAM y Callbacks.

*/

void FUN_0802604c(void)

{
  undefined4 *puVar1;
  int iVar2;
  
  puVar1 = (undefined4 *)&DAT_03002080;
  iVar2 = DAT_0802609c;
  do {
    *puVar1 = 0;
    puVar1[1] = 0;
    puVar1[2] = 0;
    puVar1[3] = 0;
    puVar1 = puVar1 + 4;
    iVar2 = iVar2 + -1;
  } while (iVar2 != 0);
  puVar1 = (undefined4 *)&DAT_06000000;
  iVar2 = 0x1800;
  do {
    *puVar1 = 0;
    puVar1[1] = 0;
    puVar1[2] = 0;
    puVar1[3] = 0;
    puVar1 = puVar1 + 4;
    iVar2 = iVar2 + -1;
  } while (iVar2 != 0);
  return;
}

