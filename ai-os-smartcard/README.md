# AI-OS Smartcard Driver

Bare-metal ISO7816 smartcard integration for the AI-OS AArch64 microkernel. Enables raw APDU communication, secure IPC messaging, and hardware-level cryptographic isolation.

## Architecture & ABI Pinning
This repository is an out-of-tree Application Target Kernel Module (`.atkm`) designed to run on top of AI-OS. 
To ensure strict ABI compliance, the `Makefile` cross-references the `ai-os` kernel headers and explicitly pins the build to a verified kernel Git commit. Any modifications to the kernel headers will require an explicit ABI re-validation and commit hash update in the Makefile.

## BSS Handling
The ai-os loader correctly zeros cartridge `.bss` sections using the `bss_size` field embedded in the `.atkm` header by `mkcartridge.py`. Uninitialized static variables are safe to use in this module.

## Build Instructions
1. Ensure the `ai-os` repository is located in the sibling directory (`../ai-os`).
2. Run `make`.
3. If the ABI pin matches, the build will produce `iso7816.atkm`.
4. Copy `iso7816.atkm` into your `ai-os` root and append it to your `mkbundle.py` build script.