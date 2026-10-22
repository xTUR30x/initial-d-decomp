#include "global.h"

// Registros de control del hardware de GBA
#define REG_DISPCNT  *(vu16*)(0x04000000)
#define REG_IME      *(vu16*)(0x04000208)

int main(void) {
    // Desactivar interrupciones de hardware durante la inicialización
    REG_IME = 0;

    // Bucle principal de la GBA (donde llamaremos a las funciones descompiladas)
    while (1) {
        // Main Loop
    }

    return 0;
}