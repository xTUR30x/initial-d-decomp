/*
# Módulo: Limpieza y Ocultación de Sprites OAM

- Dirección ROM: 0x08026E0C
- Nombre sugerido: `graphics_hide_oam_sprites`
- Tipo: OAM Attribute Reset / Sprite Parking

Lógica principal:
1. Lee la cantidad de sprites activos desde IWRAM (*DAT_08026e50).
2. Recorre la tabla OAM (0x07000000) desde el slot activo hasta el último (127).
3. Asigna Y = 240 (0xF0) a Attr0 para posicionar los sprites no utilizados fuera de pantalla.

Uso en el Port Native C:
- Implementar en `src/graphics.c`.
- Mantener la lógica para inicializar correctamente el buffer estático de sprites de SDL2.
*/



void FUN_08026e0c(void)

{
  bool bVar1;
  undefined4 *puVar2;
  undefined4 *puVar3;
  int iVar4;
  int iVar5;
  
  puVar2 = (undefined4 *)(*DAT_08026e50 * 8 + 0x7000000);
  iVar4 = 0x80 - *DAT_08026e50;
  do {
    puVar3 = puVar2 + 1;
    *puVar2 = 0xf002b0;
    puVar2 = puVar2 + 2;
    *(undefined2 *)puVar3 = 0;
    iVar5 = iVar4 + -1;
    bVar1 = 0 < iVar4;
    iVar4 = iVar5;
  } while (iVar5 != 0 && bVar1);
  return;
}

