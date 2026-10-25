/*
# Módulo: Transferencia de Gráficos / Unpack DMA (VRAM)

- Dirección ROM: 0x080260A0
- Nombre sugerido: `vram_unpack_tiles_dma3`
- Tipo: DMA Graphic Unpacker / Software Palette Mixer
- Registros Hardware involucrados:
  * 0x040000D4 (REG_DMA3SAD - Origen DMA)
  * 0x040000D8 (REG_DMA3DAD - Destino DMA)
  * 0x040000DE (REG_DMA3CNT_H - Control DMA, valor 0x8400)

Lógica principal:
1. Multiplica el ID del tile por 0x40 (64 bytes / 4bpp tile size).
2. Procesa la transparencia y máscara de color mediante desplazamientos de bits (0xFF).
3. Dispara transferencia rápida por hardware usando el canal DMA3.

Uso en el Port Native C:
- Implementar en `src/gfx.c`.
- Abstraer la llamada DMA3 a un bucle de copia a memoria simulada (`vram_buffer`).

*/

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_080260a0(ushort *param_1,int param_2,int param_3)

{
  ushort *puVar1;
  int iVar2;
  int iVar3;
  int unaff_r4;
  uint *unaff_r7;
  uint *puVar4;
  int unaff_r8;
  uint uVar5;
  uint *unaff_r10;
  uint *puVar6;
  
  do {
    iVar2 = param_2 + -1;
    puVar1 = param_1;
    iVar3 = param_3;
    puVar6 = unaff_r7;
    if (iVar2 == 0 || param_2 < 1) {
      while( true ) {
        unaff_r7 = puVar6;
        param_3 = iVar3 + -1;
        if (param_3 == 0 || iVar3 < 1) {
          return;
        }
        param_1 = puVar1 + 1;
        unaff_r10 = (uint *)((uint)*puVar1 * 0x40 + unaff_r4);
        if (*puVar1 != 0) break;
        _DAT_040000de = 0x8400;
        puVar1 = param_1;
        iVar3 = param_3;
        puVar6 = unaff_r7 + 0x10;
        _DAT_040000d4 = unaff_r10;
        _DAT_040000d8 = unaff_r7;
      }
      iVar2 = 2;
    }
    uVar5 = *unaff_r10;
    if ((uVar5 & 0xff000000) != 0) {
      uVar5 = uVar5 | unaff_r8 << 0x1d;
    }
    if ((uVar5 & 0xff0000) != 0) {
      uVar5 = uVar5 | unaff_r8 << 0x15;
    }
    if ((uVar5 & 0xff00) != 0) {
      uVar5 = uVar5 | unaff_r8 << 0xd;
    }
    if ((uVar5 & 0xff) != 0) {
      uVar5 = uVar5 | unaff_r8 << 5;
    }
    *unaff_r7 = uVar5;
    uVar5 = unaff_r10[1];
    if ((uVar5 & 0xff000000) != 0) {
      uVar5 = uVar5 | unaff_r8 << 0x1d;
    }
    if ((uVar5 & 0xff0000) != 0) {
      uVar5 = uVar5 | unaff_r8 << 0x15;
    }
    if ((uVar5 & 0xff00) != 0) {
      uVar5 = uVar5 | unaff_r8 << 0xd;
    }
    if ((uVar5 & 0xff) != 0) {
      uVar5 = uVar5 | unaff_r8 << 5;
    }
    unaff_r7[1] = uVar5;
    uVar5 = unaff_r10[2];
    if ((uVar5 & 0xff000000) != 0) {
      uVar5 = uVar5 | unaff_r8 << 0x1d;
    }
    if ((uVar5 & 0xff0000) != 0) {
      uVar5 = uVar5 | unaff_r8 << 0x15;
    }
    if ((uVar5 & 0xff00) != 0) {
      uVar5 = uVar5 | unaff_r8 << 0xd;
    }
    if ((uVar5 & 0xff) != 0) {
      uVar5 = uVar5 | unaff_r8 << 5;
    }
    unaff_r7[2] = uVar5;
    uVar5 = unaff_r10[3];
    if ((uVar5 & 0xff000000) != 0) {
      uVar5 = uVar5 | unaff_r8 << 0x1d;
    }
    if ((uVar5 & 0xff0000) != 0) {
      uVar5 = uVar5 | unaff_r8 << 0x15;
    }
    if ((uVar5 & 0xff00) != 0) {
      uVar5 = uVar5 | unaff_r8 << 0xd;
    }
    if ((uVar5 & 0xff) != 0) {
      uVar5 = uVar5 | unaff_r8 << 5;
    }
    unaff_r7[3] = uVar5;
    uVar5 = unaff_r10[4];
    if ((uVar5 & 0xff000000) != 0) {
      uVar5 = uVar5 | unaff_r8 << 0x1d;
    }
    if ((uVar5 & 0xff0000) != 0) {
      uVar5 = uVar5 | unaff_r8 << 0x15;
    }
    if ((uVar5 & 0xff00) != 0) {
      uVar5 = uVar5 | unaff_r8 << 0xd;
    }
    if ((uVar5 & 0xff) != 0) {
      uVar5 = uVar5 | unaff_r8 << 5;
    }
    unaff_r7[4] = uVar5;
    uVar5 = unaff_r10[5];
    if ((uVar5 & 0xff000000) != 0) {
      uVar5 = uVar5 | unaff_r8 << 0x1d;
    }
    if ((uVar5 & 0xff0000) != 0) {
      uVar5 = uVar5 | unaff_r8 << 0x15;
    }
    if ((uVar5 & 0xff00) != 0) {
      uVar5 = uVar5 | unaff_r8 << 0xd;
    }
    if ((uVar5 & 0xff) != 0) {
      uVar5 = uVar5 | unaff_r8 << 5;
    }
    unaff_r7[5] = uVar5;
    puVar6 = unaff_r10 + 7;
    uVar5 = unaff_r10[6];
    if ((uVar5 & 0xff000000) != 0) {
      uVar5 = uVar5 | unaff_r8 << 0x1d;
    }
    if ((uVar5 & 0xff0000) != 0) {
      uVar5 = uVar5 | unaff_r8 << 0x15;
    }
    if ((uVar5 & 0xff00) != 0) {
      uVar5 = uVar5 | unaff_r8 << 0xd;
    }
    if ((uVar5 & 0xff) != 0) {
      uVar5 = uVar5 | unaff_r8 << 5;
    }
    puVar4 = unaff_r7 + 7;
    unaff_r7[6] = uVar5;
    unaff_r10 = unaff_r10 + 8;
    uVar5 = *puVar6;
    if ((uVar5 & 0xff000000) != 0) {
      uVar5 = uVar5 | unaff_r8 << 0x1d;
    }
    if ((uVar5 & 0xff0000) != 0) {
      uVar5 = uVar5 | unaff_r8 << 0x15;
    }
    if ((uVar5 & 0xff00) != 0) {
      uVar5 = uVar5 | unaff_r8 << 0xd;
    }
    if ((uVar5 & 0xff) != 0) {
      uVar5 = uVar5 | unaff_r8 << 5;
    }
    unaff_r7 = unaff_r7 + 8;
    *puVar4 = uVar5;
    param_2 = iVar2;
  } while( true );
}

