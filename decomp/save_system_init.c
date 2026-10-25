/*
# Módulo: Sistema de Guardado SRAM / EEPROM (Save Init)

- Dirección ROM: 0x0801B27C
- Nombre sugerido: `save_system_init`
- Tipo: Save Data Verification & System Settings Loader

Lógica principal:
1. Lee los bytes de la memoria SRAM (0x0E000000) hacia el buffer de trabajo en EWRAM (0x02030000).
2. Valida la firma "INITIAL-D Another Stage for GBA".
3. Si la firma falla, formatea el buffer a cero, asigna valores por defecto y calcula el Checksum con `FUN_0801b4c8`.
4. Si la firma es correcta, extrae los ajustes guardados de usuario (audio, récords, progreso).

Uso en el Port Native C:
- Implementar en `src/save.c`.
- Abstraer el acceso a 0x0E000000 mediante un archivo de guardado local (ej. `save.dat` o `save.bin`).
*/

 void FUN_0801b27c(void)


{

  bool bVar1;

  undefined1 *puVar2;

  int iVar3;

  int *piVar4;

  uint uVar5;

  undefined1 *puVar6;

  int *piVar7;

  undefined4 *puVar8;

  undefined4 *puVar9;

  int iVar10;

  bool bVar11;

  

  *DAT_0801b4b0 = 0;

  thunk_FUN_0863d4f8(0xe000000,&DAT_02030000,0x2004);

  piVar4 = (int *)&DAT_02030000;

  iVar3 = 8;

  piVar7 = (int *)s_INITIAL-D_Another_Stage_for_GBA_0801b25c;

  do {

    if (*piVar7 != *piVar4) goto LAB_0801b2d4;

    bVar11 = iVar3 + -1 == 0;

    bVar1 = 0 < iVar3;

    piVar4 = piVar4 + 1;

    iVar3 = iVar3 + -1;

    piVar7 = piVar7 + 1;

  } while (!bVar11 && bVar1);

  FUN_0801b4c8();

  if (!bVar11) {

LAB_0801b2d4:

    puVar8 = (undefined4 *)&DAT_02030000;

    iVar3 = 0x100;

    do {

      *puVar8 = 0;

      puVar8[1] = 0;

      puVar8[2] = 0;

      puVar8[3] = 0;

      puVar8[4] = 0;

      puVar8[5] = 0;

      puVar9 = puVar8 + 7;

      puVar8[6] = 0;

      puVar8 = puVar8 + 8;

      *puVar9 = 0;

      iVar10 = iVar3 + -1;

      bVar1 = 0 < iVar3;

      iVar3 = iVar10;

    } while (iVar10 != 0 && bVar1);

    FUN_0801b50c();

    DAT_02030000 = 1;

    DAT_02030001 = 0;

    DAT_02030002 = 1;

    DAT_02030003 = 0;

    _DAT_02030004 = 0;

    _DAT_02030008 = 0;

    _DAT_0203000c = 0;

    _DAT_02030010 = 0;

    puVar8 = DAT_0801b4b4;

    iVar3 = 8;

    do {

      *puVar8 = 0;

      puVar8[1] = 0;

      puVar8[2] = 0;

      puVar8[3] = 0;

      puVar8[4] = 0;

      puVar8[5] = 0;

      puVar8[6] = 0;

      puVar8[7] = 0;

      puVar8[8] = 0;

      puVar8[9] = 0;

      puVar8[10] = 0;

      puVar8[0xb] = 0;

      puVar8[0xc] = 0;

      puVar8[0xd] = 0;

      puVar8[0xe] = 0;

      puVar8[0xf] = 0;

      puVar8[0x10] = 0;

      puVar8[0x11] = 0;

      puVar8[0x12] = 0;

      puVar8[0x13] = 0;

      puVar8[0x14] = 0;

      puVar8[0x15] = 0;

      puVar8[0x16] = 0;

      puVar8[0x17] = 0;

      puVar8[0x18] = 0;

      puVar8[0x19] = 0;

      puVar8[0x1a] = 0;

      puVar8[0x1b] = 0;

      puVar8[0x1c] = 0;

      puVar8[0x1d] = 0;

      puVar8[0x1e] = 0;

      puVar8[0x1f] = 0;

      puVar8[0x20] = 0;

      puVar8[0x21] = 0;

      puVar9 = puVar8 + 0x23;

      puVar8[0x22] = 0;

      puVar8 = puVar8 + 0x24;

      *puVar9 = 0;

      iVar10 = iVar3 + -1;

      bVar1 = 0 < iVar3;

      iVar3 = iVar10;

    } while (iVar10 != 0 && bVar1);

    iVar3 = FUN_0801b4c8();

    _DAT_02032000 = puVar8;

    iVar3 = thunk_FUN_0863d630(iVar3 + 0x30000,0xe000000,0x2004);

    if (iVar3 != 0) {

      return;

    }

  }

  uVar5 = 1;

  if (_DAT_02030800 != 0) {

    uVar5 = 3;

  }

  if (_DAT_02031000 != 0) {

    uVar5 = uVar5 | 4;

  }

  if (_DAT_02031800 != 0) {

    uVar5 = uVar5 | 8;

  }

  *DAT_0801b4b0 = uVar5;

  puVar2 = DAT_0801b4b8;

  puVar6 = DAT_0801b4b8 + 1;

  *DAT_0801b4b8 = DAT_02030020;

  *puVar6 = DAT_02030021;

  puVar2[2] = DAT_02030022;

  puVar8 = DAT_0801b4bc;

  *DAT_0801b4bc = _DAT_02030024;

  puVar8[1] = _DAT_02030028;

  *DAT_0801b4c0 = _DAT_0203002c;

  *DAT_0801b4c4 = _DAT_02030030;

  return;

} 