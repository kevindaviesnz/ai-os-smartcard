# AI-OS-Smartcard: A Capability-Based Ecosystem for Smartcard Simulation
## Whitepaper v1.1

**Status:** Production Release — POS Terminal Milestone Certified

---

## Abstract

AI-OS-Smartcard is a comprehensive simulation environment built upon a custom, bare-metal AArch64 microkernel (AI-OS). It mathematically simulates the physical constraints and security isolation of a hardware smartcard by enforcing hardware-level memory boundaries, capability-controlled inter-process communication, and strict privilege separation between kernel and user-space modules. The system has been validated end-to-end through a complete ISO 7816 Point-of-Sale transaction simulation, including card insertion handshake and PIN verification, running on a 256KB-constrained smartcard memory profile.

---

## 1. Architectural Philosophy

In a physical smartcard environment, the execution context — the secure element — is entirely isolated from the host terminal. Software vulnerabilities on the host cannot reach the silicon of the card. AI-OS-Smartcard simulates this physical airgap entirely in software by leveraging the ARMv8-A Memory Management Unit.

User-space cartridges — the POS shell, the ISO 7816 middleware, and the virtual vault — are physically barred by hardware-level page tables from reading each other's memory or the kernel's memory. This is not a software convention. The MMU enforces it at the hardware level, and the kernel's trusted computing base is small enough to audit completely.

Three properties define the security model:

**Hardware-enforced isolation.** Each cartridge occupies its own MMU-mapped memory region. Code pages are `AP=11` (read-only at both privilege levels), stack and data pages are `AP=01` (read-write, execute-never). A cartridge cannot read another cartridge's memory regardless of what code it runs.

**Capability-controlled communication.** All inter-module communication is routed through a kernel dispatcher that consults an immutable capability matrix stored in `.rodata`. The matrix is marked read-only by the MMU after boot. No runtime code path can modify it. A module that is not explicitly permitted to reach another module will have its message silently rejected by the dispatcher before any queue is touched.

**Minimal trusted core.** The kernel itself — boot sequence, MMU initialisation, interrupt handling, and dispatcher — comprises a few hundred lines of audited C and assembly. Everything else runs as an isolated EL0 cartridge.

---

## 2. The Autarky Module (.atkm) Format

Standard ELF executables carry dynamic linker dependencies, relocation tables, and assumptions about a hosted runtime environment that are incompatible with bare-metal isolation. The AI-OS-Smartcard ecosystem packages its isolated applications in the proprietary `.atkm` (Autarky Module) format.

### 2.1 Cartridge Header Structure

```
┌─────────────────────────────────────────┐
│  Magic:       0x41544B4D  ('ATKM')      │
│  Module ID:   uint32                    │
│  Code Size:   uint32  (bytes)           │
│  Text Size:   uint32  (executable only) │
│  BSS Size:    uint32  (zero-init data)  │
│  Stack Size:  uint32  (bytes)           │
│  Signature:   uint8[64]                 │
├─────────────────────────────────────────┤
│  Raw Machine Code                       │
│  Entry point is always byte 0           │
└─────────────────────────────────────────┘
```

### 2.2 W^X Enforcement

The loader implements a two-stage memory lifecycle for each cartridge:

**Stage 1 — Load:** Pages are mapped `AP_RW_EL1` (kernel write, no EL0 access). The loader copies the cartridge binary via `memcpy`, flushes the data cache, and invalidates the instruction cache.

**Stage 2 — Lock:** `mmu_lock_module_code()` flips code pages to `AP_RO_BOTH` (read-only at both privilege levels) with `PXN` set (kernel cannot execute module code). A full TLB invalidation barrier sequence (`dsb ishst` → `tlbi vmalle1` → `dsb ish` → `isb`) commits the permission change before execution begins.

Data and BSS pages are separately mapped `AP_RW_BOTH` with `XN_ALL` (execute-never). A cartridge's data is writable. A cartridge's code is not.

### 2.3 BSS Handling

The `mkcartridge.py` tool extracts the `.bss` section size from the ELF using `aarch64-elf-readelf` and embeds it in the cartridge header. The loader allocates the correct total footprint (`code_size + bss_size`) and zeroes the BSS region with `memset` before mapping it. Modules have standard C zero-initialisation semantics without any hosted runtime.

---

## 3. Run-to-Completion IPC

Physical smartcards communicate over a single electrical contact using Application Protocol Data Units. To simulate this constraint, AI-OS-Smartcard implements a run-to-completion Inter-Process Communication bus.

### 3.1 Message Structure

```c
typedef struct {
    uint32_t sender_id;              // Injected by dispatcher — never trusted from caller
    uint32_t target_id;
    uint32_t type;
    uint32_t length;                 // Validated: must be <= IPC_PAYLOAD_MAX_SIZE
    uint8_t  payload[16];
} os_message_t;
```

All messages are fixed-size. No dynamic allocation. No variable-length payloads. The dispatcher validates `length`, `sender_id`, and `target_id` bounds before touching any mailbox.

### 3.2 Capability Matrix

```
         KERNEL  UART  SHELL  FS   ISO7816  CARDSIM
KERNEL  [  1      1     1     1      1        1   ]
UART    [  1      0     1     0      0        0   ]
SHELL   [  0      1     0     1      1        0   ]
FS      [  0      0     1     0      0        0   ]
ISO7816 [  0      0     1     0      0        1   ]
CARDSIM [  0      0     0     0      1        0   ]
```

Stored in `.rodata`. Mapped read-only by the MMU. Every `1` entry has a documented policy justification. `SHELL → KERNEL = 0` is a permanent invariant — user space cannot issue kernel commands directly.

### 3.3 Dispatcher Execution Model

Modules spend idle time in `wfi` (Wait For Interrupt). When an interrupt fires or a message is queued, the dispatcher evaluates all module mailboxes. The run-to-completion lock prevents dispatch while any module is executing — a module that sends a message during its handler will not cause another module to be dispatched until the current handler completes via `SYS_HANDLER_DONE`. This eliminates re-entrancy hazards without requiring a scheduler.

---

## 4. Smartcard Middleware (ISO 7816)

The `iso7816` middleware cartridge bridges the POS shell and the virtual card vault. It implements the following transaction model:

**Card insertion:** The shell sends `IPC_TYPE_APDU_COMMAND` containing `00 A4 04 00` (SELECT FILE). The middleware forwards it, receives `90 00`, and routes the response back to the shell via `IPC_TYPE_APDU_RESPONSE`.

**PIN verification:** The shell encodes PIN digits as ASCII hex bytes and constructs a `VERIFY` APDU (`00 20 00 00 05` + PIN bytes). The middleware routes it to the vault and returns the status word.

The middleware never holds card secrets. It is a routing and protocol layer only. The vault module — which holds the PIN — cannot be reached by the shell directly. `capability_matrix[SHELL][VAULT] = 0`. The only path from the shell to the vault is through the ISO 7816 middleware, which enforces the protocol.

---

## 5. Hardware Profile

The smartcard profile targets a 256KB RAM constraint, matching real smartcard silicon specifications:

| Resource | Limit | Actual Usage |
|---|---|---|
| Total RAM | 256KB | ~120KB |
| Kernel + MMU tables | — | ~56KB |
| Cartridge stacks (×5) | — | ~40KB |
| Kernel stack | 16KB | 16KB |
| EL0 stack | 16KB | 16KB |

The constraint is enforced at link time by `linker_smartcard.ld`. If the kernel binary exceeds 256KB, the build fails before producing a binary.

---

## 6. AI-Driven Development and Orchestration

The development lifecycle of this project relied entirely on multi-agent AI orchestration across more than 100 review cycles spanning 10 engineering phases.

**Gemini Pro** functioned as the Lead Developer, generating architecture proposals and implementation code based on requirements and prior audit findings.

**Claude Sonnet 4.6** operated as the Senior QA Architect, issuing structured audit reports with explicit blocker/advisory/deferred classifications. Every file was reviewed before compilation. No build was authorised until all blockers were resolved.

**Kevin Davies** acted as the Hardware Interface — executing approved builds in a Debian ARM64 sandbox on Apple Silicon via OrbStack, providing terminal output and screenshots as the primary feedback signal.

Zero manual code modifications were made without explicit AI instruction. The pipeline validated that multi-agent AI orchestration can produce auditable, security-critical bare-metal systems code with a rigorous review discipline that matches professional embedded systems practice.

The full audit trail — 53 named blockers, 23 advisories, 10 deferred items, and their resolutions — is preserved in the project's QA report.

---

## 7. Verified Capabilities

The following were verified by live execution on QEMU `virt` (Cortex-A53):

- EL2 → EL1 privilege transition at boot
- MMU with hardware-enforced per-section permissions
- GICv2 interrupt routing for UART (SPI 33) and timer (PPI 30)
- ARM Generic Timer at 10ms preemption interval (drift-free via CVAL)
- EL0 user-space execution with SVC gate and pointer validation
- Five-module cartridge loader with W^X enforcement and BSS zeroing
- Capability-controlled IPC with sender identity injection
- Full ISO 7816 APDU transaction: SELECT FILE + VERIFY PIN
- 256KB smartcard memory profile enforced at link time

---

*AI-OS-Smartcard is an original work. The `.atkm` cartridge format, the capability matrix IPC model, and the run-to-completion dispatcher are purpose-built for this project.*

