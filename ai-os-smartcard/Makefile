CC = aarch64-elf-gcc
LD = aarch64-elf-ld
OBJCOPY = aarch64-elf-objcopy

KERNEL_DIR = ../ai-os
CFLAGS = -Wall -Wextra -ffreestanding -mgeneral-regs-only -nostdlib -fno-builtin -fno-merge-constants -fPIE -I$(KERNEL_DIR)/include

# Hardcoded — intentionally not in a file.
# Update this value only after explicit ABI validation.
EXPECTED_KERNEL_COMMIT := 9afa8874bf58608fc8c89bc13f504186df506459
KERNEL_COMMIT := $(shell cd $(KERNEL_DIR) && git rev-parse HEAD 2>/dev/null || echo "UNKNOWN")

.PHONY: all clean check_kernel_abi

all: check_kernel_abi iso7816.atkm cardsim.atkm

check_kernel_abi:
	@if [ "$(KERNEL_COMMIT)" != "$(EXPECTED_KERNEL_COMMIT)" ]; then \
		echo "ERROR: Kernel ABI mismatch."; \
		echo "  Expected: $(EXPECTED_KERNEL_COMMIT)"; \
		echo "  Found:    $(KERNEL_COMMIT)"; \
		echo "  Re-validate ABI before updating EXPECTED_KERNEL_COMMIT."; \
		exit 1; \
	fi

# --- ISO7816 Cartridge ---
src/iso7816_main.o: src/iso7816_main.c
	$(CC) $(CFLAGS) -c $< -o $@

iso7816.bin: src/iso7816_main.o $(KERNEL_DIR)/module.ld
	$(LD) -T $(KERNEL_DIR)/module.ld -nostdlib --build-id=none $< -o iso7816.elf
	$(OBJCOPY) -O binary -j .text -j .rodata -j .data iso7816.elf iso7816.bin

iso7816.atkm: iso7816.bin
	python3 $(KERNEL_DIR)/tools/mkcartridge.py iso7816.bin iso7816.elf iso7816.atkm 4 8192

# --- CARDSIM Cartridge ---
src/cardsim_main.o: src/cardsim_main.c
	$(CC) $(CFLAGS) -c $< -o $@

cardsim.bin: src/cardsim_main.o $(KERNEL_DIR)/module.ld
	$(LD) -T $(KERNEL_DIR)/module.ld -nostdlib --build-id=none $< -o cardsim.elf
	$(OBJCOPY) -O binary -j .text -j .rodata -j .data cardsim.elf cardsim.bin

cardsim.atkm: cardsim.bin
	python3 $(KERNEL_DIR)/tools/mkcartridge.py cardsim.bin cardsim.elf cardsim.atkm 5 8192

clean:
	rm -f src/*.o *.elf *.bin *.atkm