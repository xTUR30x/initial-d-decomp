/*
# Módulo: Selección de Modo (MODE_SELECT)

- Dirección ROM: 0x0809D34C
- Estado X-Macro: State 2 (`GAME_SCREEN_MENU`)
- Nombre sugerido: `state_02_mode_select_loop`

Lógica principal:
1. Controla el índice de la opción activa mediante el offset `0x1f` del bloque de estado del menú.
2. Actualiza la posición de las paletas y buffers de visualización para destacar la opción seleccionada (`0x8000`, `0x1000`).
3. Mantiene la actualización de OAM / Tilemap asociada a `PTR_DAT_080a11dc` y las variables globales de RAM (`0x080A1094`, `0x080A121c`).
*/


void FUN_0809d34c(void)

{
  byte bVar1;
  undefined *puVar2;
  uint uVar3;
  int iVar4;
  int iVar5;
  int iVar6;
  int iVar7;
  int iVar8;
  int in_stack_0000034c;
  
  iVar8 = (in_stack_0000034c + 0x112) * 0x1000 + -0xce >> 0x1f;
  do {
    iVar8 = iVar8 >> 0x1f;
    *(int *)(iVar8 + 0x30) = iVar8 << 0xf;
    puVar2 = PTR_DAT_080a11dc;
    *(uint *)(iVar8 + 0x38) = (uint)*(byte *)(iVar8 + 0x1f);
    iVar5 = *(int *)(iVar8 + 1 + (uint)*(byte *)(iVar8 + 0x1f));
    *(short *)((int)puVar2 * 2) = (short)iVar5;
    iVar4 = *(int *)(iVar8 + 0x74);
    iVar6 = (iVar8 + 0xf6) * 4;
    *(short *)(iVar4 + iVar6) = (short)iVar4;
    iVar7 = iRam080a1094;
    uVar3 = (uint)*(byte *)(iVar6 + iVar5 * 0x8000 + iVar8);
    *(uint *)(iVar4 * 0x1000 + 0x30) = uVar3 << 0x17;
    iVar5 = iRam080a121c;
    bVar1 = *(byte *)(iVar8 + 0x1f);
    *(uint *)(iVar4 * 0x1000 + 0x38) = (uint)bVar1;
    uVar3 = *(uint *)(uVar3 * 0x100 + 1 + (uint)bVar1);
    *(short *)(iVar5 * 2) = (short)uVar3;
    iVar8 = *(int *)(uVar3 * 2 + 0x74);
    iVar7 = (iVar7 + 0xf6) * 4;
    *(short *)(iVar8 + iVar7) = (short)iVar8;
    uVar3 = (uVar3 & 0x1ffff) >> 1;
    iVar8 = *(char *)((uint)*(byte *)(uVar3 + 0xf) + *(int *)(uVar3 + 0x34)) + 0xad;
    *(char *)(uVar3 + 0xe) = (char)iVar7;
  } while( true );
}

