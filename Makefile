# ==============================================================================
# Makefile Multi-Target: Game Boy Advance + PC Nativo (SDL2)
# ==============================================================================

ROM_TITLE := initial_d_decomp
BUILD_DIR := build

# ------------------------------------------------------------------------------
# Configuración PC Nativo (Linux/Windows)
# ------------------------------------------------------------------------------
HOST_CC     := gcc
HOST_CFLAGS := -O2 -Wall -Wextra -Iinclude $(shell sdl2-config --cflags)
HOST_LIBS   := $(shell sdl2-config --libs)
PC_TARGET   := $(BUILD_DIR)/$(ROM_TITLE)_native

# ------------------------------------------------------------------------------
# Configuración GBA (Bare-metal ARM)
# ------------------------------------------------------------------------------
ARM_PREFIX  := arm-none-eabi-
ARM_CC      := $(ARM_PREFIX)gcc
ARM_OBJCOPY := $(ARM_PREFIX)objcopy
ARM_CFLAGS  := -mthumb -mthumb-interwork -mcpu=arm7tdmi -O2 -Wall -Iinclude --specs=nosys.specs -DTARGET_GBA
GBA_TARGET  := $(BUILD_DIR)/$(ROM_TITLE).gba

# Fuentes comunes filtrando los puntos de entrada específicos
COMMON_SRCS := $(filter-out src/pc_main.c src/gba_main.c, $(wildcard src/*.c))

.PHONY: all pc gba clean

all: pc

# Regla para compilar ejecutable NATIVO de PC
pc: $(COMMON_SRCS) src/pc_main.c | $(BUILD_DIR)
	@echo "  HOST_CC  $@"
	@$(HOST_CC) $(HOST_CFLAGS) $^ -o $(PC_TARGET) $(HOST_LIBS)
	@echo "=================================================="
	@echo " Ejecutable nativo PC generado: $(PC_TARGET)"
	@echo "=================================================="

# Regla para compilar ROM de GBA
gba: $(COMMON_SRCS) src/gba_main.c | $(BUILD_DIR)
	@echo "  ARM_CC   $@"
	@$(ARM_CC) $(ARM_CFLAGS) $^ -o $(BUILD_DIR)/$(ROM_TITLE).elf -lm
	@$(ARM_OBJCOPY) -O binary $(BUILD_DIR)/$(ROM_TITLE).elf $(GBA_TARGET)
	@echo "=================================================="
	@echo " ROM GBA generada: $(GBA_TARGET)"
	@echo "=================================================="

$(BUILD_DIR):
	@mkdir -p $@

clean:
	@rm -rf $(BUILD_DIR)