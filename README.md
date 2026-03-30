# AI-OS-Smartcard 

The **AI-OS-Smartcard** project is a complete software simulation of a physically isolated hardware smartcard. It is powered by **AI-OS**—a custom, capability-based AArch64 bare-metal microkernel designed from scratch. This ecosystem provides a deterministic, hardware-enforced secure enclave, making it an ideal foundation for autonomous financial matching engines and isolated state machines.

## Architecture

The AI-OS microkernel abandons standard monolithic design in favor of strict, hardware-enforced isolation to perfectly simulate a physical smartcard environment:
* **W^X Memory Protection:** The kernel leverages the AArch64 MMU to enforce Write XOR Execute memory boundaries. Data cannot be executed, and code cannot be overwritten.
* **The `.atkm` Cartridge Format:** User-space applications do not run in kernel space. They are compiled into proprietary `.atkm` (Autarky Module) format, ensuring deterministic memory footprints, and are packed into a secure `.atkb` bundle.
* **Run-to-Completion IPC:** To simulate a smartcard's single electrical I/O pin, modules are physically barred from accessing each other's memory. All communication happens via a strictly routed, capability-based Inter-Process Communication (IPC) bus.

## Core Modules

The ecosystem is divided into specific, isolated capability cartridges:
* `uart.atkm`: The hardware driver. Catches physical IRQ 33 interrupts from the host's PL011 UART and bridges them to the IPC bus.
* `shell.atkm`: An interactive EL0 user-space shell that parses host terminal commands and routes APDUs.
* `fs.atkm`: An isolated filesystem module.
* `iso7816.atkm`: Middleware that simulates a smartcard reader, translating host shell inputs into ISO-compliant signals.
* `cardsim.atkm`: The isolated smartcard simulation environment (the secure enclave / matching engine).

## Building the OS

You will need an `aarch64-elf` cross-compiler and QEMU installed.

To compile the microkernel, pack the modules into `.atkm` cartridges, generate the `.atkb` bundle, and launch the QEMU emulator, run:

```bash
make clean && make run_smartcard
Usage
Because the simulated smartcard relies on a highly precise IPC message queue, standard line-buffered host terminals can flood the mailbox. You must connect to the OS using a raw terminal to send characters exactly as they are typed.

While QEMU is running, open a second terminal window and connect using nc (netcat):

Bash
stty -icanon && nc 127.0.0.1 4444
(Note: typing stty icanon or closing the window will restore your macOS terminal to normal).

Shell Commands

Once connected, you will be greeted by the ai-os-smartcard > prompt.

help: Displays available commands.

info: Displays kernel and build information.

clear: Clears the terminal screen.

apdu <hex>: Packages a hexadecimal Application Protocol Data Unit and routes it securely across the kernel boundary to the iso7816 smartcard reader module. Example: apdu 00 A4 04 00 (SELECT FILE).
```

Note: This project was completed using AI - Gemini Pro and Claude Sonnet 4.6.