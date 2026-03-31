# AI-OS-Smartcard: Engineering QA Report
## Milestone 1.0 — Interactive POS Terminal & ISO 7816 Transaction Simulation
**Auditor:** Claude Sonnet 4.6 (Senior QA Architect)
**Lead Developer:** Gemini Pro
**Status:** ✅ CERTIFIED — All blockers resolved, POS terminal milestone signed off

---

## 1. Executive Summary

This report covers the complete development and audit lifecycle of the AI-OS-Smartcard ecosystem, from bare-metal bootloader through to a fully interactive Point-of-Sale terminal simulation running ISO 7816 APDU transactions over a capability-controlled IPC bus.

The system boots a custom AArch64 microkernel on QEMU's `virt` machine (Cortex-A53), loads five isolated EL0 cartridges from a signed bundle, enforces hardware memory isolation via the MMU, and routes all inter-module communication through an immutable capability matrix. The final verification demonstrated a complete card insertion and PIN verification sequence with correct `90 00` status word responses.

**53 named blockers were raised and resolved across 10 engineering phases. 10 items were intentionally deferred with documented trigger conditions. Zero open blockers remain.**

---

## 2. Methodology

**Architecture:** Capability-based microkernel. All application logic runs in unprivileged EL0 cartridges. The kernel is a minimal EL1 dispatcher with no knowledge of application semantics.

**Hardware target:** QEMU `virt` machine, Cortex-A53, 256KB smartcard memory profile enforced at link time.

**Toolchain:** `aarch64-elf-gcc`, `aarch64-elf-ld`, `aarch64-elf-objcopy`, `aarch64-elf-readelf`. Native compilation on Apple Silicon via OrbStack Debian ARM64 sandbox.

**Review protocol:** Every file reviewed before compilation. No build authorised with open blockers. Architecture proposals required before implementation on all security-critical components.

**Audit classification:**
- 🔴 **Blocker** — Build not authorised until resolved
- 🟠 **Warning** — Required before merge
- 🟡 **Advisory** — Required before next phase
- 🔵 **Deferred** — Documented with explicit trigger condition

---

## 3. Phase Summary

### Phase 0 — Toolchain Validation
Established native-as-cross compilation strategy on Debian ARM64. Identified and resolved: undefined `sp` at reset (BLOCKER-01), native `ld` default memory map overriding boot address (BLOCKER-02), ELF64 compatibility with QEMU `-kernel` (BLOCKER-03).

### Phase 1 — Bootloader & Memory Map
Implemented `boot.S` with core locking, stack initialisation, and `.bss` zero loop. Established `linker.ld` with `KEEP(*(.text.boot))` invariant. Confirmed `e_entry = 0x40000000` via `readelf`.

### Phase 2 — UART & C Kernel
First successful boot: `[KERNEL] Microkernel Phase 2 Booted Successfully.` UART confirmed operational at `0x09000000`.

### Phase 3 — IPC Dispatcher & Capability Matrix
Implemented fixed-size `os_message_t`, ring-buffer mailboxes, and the 3×3 capability matrix. Key security findings: `sender_id` forgery vector (SECURITY-01), `length` overread (SECURITY-02), `static const` ODR violation (SECURITY-03), `ipc_receive` ownership gate (SECURITY-04). All resolved. Test suite confirmed: `SHELL → KERNEL` correctly blocked.

### Phase 4A — MMU & Page Tables
Implemented 5-table page walk (L1→L2→L3, 4KB granule, 39-bit VA). Key findings: AP bits inverted exposing EL0 to kernel data (BLOCKER-05), TLB invalidation absent (BLOCKER-06), `TCR_EL1` unspecified (BLOCKER-07), insufficient table count (BLOCKER-08), array indices missing throughout `mmu.c` (BLOCKER-09), Access Flag absent from all descriptors (BLOCKER-10). Verification: `SCTLR_EL1.M = 1` confirmed via UART readback.

### Phase 4B — GICv2 & Exception Vectors
Implemented 16-slot vector table, IRQ handler with 272-byte register save frame including `ELR_EL1` and `SPSR_EL1`. Key findings: system registers not saved (BLOCKER-11), frame 16 bytes too small (BLOCKER-12), array index missing in GIC MMU update (BLOCKER-13), `msg.payload = c` array assign error (BLOCKER-14), `daifclr` never set (BLOCKER-15), spurious interrupt unguarded (WARNING-03). Verification: keystrokes echoed through full interrupt pipeline.

### Phase 5 — ARM Generic Timer
Implemented PPI 30 (CNTPNSIRQ) at 10ms interval. Key finding: `CNTHCTL_EL2` physical timer trap on EL2 boot (BLOCKER-16) — resolved via EL2 detection and `CNTHCTL_EL2.EL1PCEN` configuration. DEFERRED-03 (CVAL drift-free timing) resolved in Phase 9. Verification: heartbeat dots appeared at 1-second intervals.

### Phase 6 — EL0 Transition & SVC Gate
Implemented `eret` to EL0 with guard page, pointer validation with overflow guard, SVC whitelist, and `ESR_EL1` decode. Key findings: `regs[]` indices absent in `syscall.c` (BLOCKER-17), EL0 `.text` mapped RW enabling self-modification (BLOCKER-18), `sender_id` from EL0 register — identity forgery (BLOCKER-19), `ptr + size` overflow bypasses range check (BLOCKER-20), kernel register state leaked into EL0 on `eret` (WARNING-06). Verification: `>........jjjjjjjj..` confirmed keyboard-to-screen pipeline.

### Phase 7 — Cartridge Loader
Implemented ATKB bundle format, ATKM cartridge format, bounded bump allocator, per-cartridge MMU isolation, registration handshake, and run-to-completion dispatcher. Key findings across multiple review cycles: 20+ blockers including duplicate function definitions, block descriptor misuse in `ensure_l3_table` (causing kernel to write page descriptors into read-only bundle memory), `module_context_t` missing array dimensions, `eret` called outside exception context. UART and shell extracted from kernel binary. Extraction test: silent kernel with empty bundle confirmed.

### Phase 8 — FS Cartridge & Autonomous IPC
Implemented FS cartridge with RAM buffer, autonomous APDU test sequence. Resolved: `.bss` stripping by `objcopy` (DEFERRED-04 resolved), PL011 FIFO stranding via RTIM and timer-driven poll fallback.

### Phase 9 — Run-to-Completion Dispatcher
Resolved context destruction bug: `try_dispatch_next` called from `SYS_IPC_SEND` while caller still active overwrote return frame. Implemented run-to-completion lock with load-bearing architectural comment. DEFERRED-03 closed via CVAL absolute timing.

### Phase 10 — Smart Card Profile
Established `ai-os-smartcard` repository with ABI pin, bridge Makefile, and ISO 7816 + CardSim cartridges. Key findings across extensive debug cycle: W^X two-stage loader required for write-then-lock semantics, `ensure_l3_table` block/table descriptor discrimination, linker boundary collapse from empty `.data` section (resolved via `QUAD(0)` + `ALIGN(4096)`), GOT stripping by `objcopy` (resolved via explicit `.got*` capture in `module.ld`), BSS offset desync between ELF and loader (resolved via page-aligned `.data` padding), `mkcartridge.py` silent subprocess failure (resolved via `shutil.which` + hard fail), boot chain stall at FS module (resolved via stub `SYS_INIT_DONE`), capability matrix missing `SHELL → ISO7816` route.

---

## 4. Key Findings by Category

### Security Findings Resolved

| ID | Finding | Resolution |
|---|---|---|
| SECURITY-01 | `sender_id` forgery — caller can impersonate any module | Dispatcher injects `caller_id`, never trusts message field |
| SECURITY-02 | `length` field overread — unbounded payload copy | `IPC_MSG_VALID` macro enforces `length <= IPC_PAYLOAD_MAX_SIZE` |
| SECURITY-03 | `static const` capability matrix in header — ODR violation, N copies | `extern` declaration, single definition in `dispatcher.c` |
| SECURITY-04 | `ipc_receive` ownership gate absent — any module can drain any mailbox | `box->owner_id != caller_id` check before dequeue |
| BLOCKER-18 | EL0 `.text` mapped RW — self-modifying code vector | Two-stage loader: write as `AP_RW_EL1`, lock to `AP_RO_BOTH` after copy |
| BLOCKER-19 | `sender_id` from EL0 register at SVC boundary | Kernel injects identity from `module_region_t`, ignores `x0` |
| BLOCKER-20 | `ptr + size` integer overflow bypasses pointer range check | Explicit overflow guard before range comparison |
| BLOCKER-33 | Capability matrix syntax error — `{SYS_MOD_SHELL}` row initialiser | Explicit 2D initialiser with all elements named |

### Architecture Regressions Caught

| Regression | Phase Introduced | Phase Caught | Impact |
|---|---|---|---|
| `VBAR_EL1` cleared during smartcard profile pivot | 10 | 10 | Silent hang on first SVC/IRQ |
| `MAILBOX_CAPACITY` reduced from 8 to 1 | 10 | 10 | Dropped messages on any burst input |
| `mmu_init_tables` using `_bss_start` instead of `_erodata` | 10 | 10 | `.data` section locked read-only, kernel panic on first write |
| Module code mapped `AP_RW_BOTH` in `mmu_map_module_region` | 7 | 9 | Self-modifying code vulnerability |

### Hardware Interaction Findings

| Finding | Resolution |
|---|---|
| `CNTHCTL_EL2` traps physical timer at EL2 boot | EL detection in `boot.S`, `EL1PCEN` bit set before drop |
| PL011 FIFO stranding: bytes arrive after drain loop exits | Timer-driven FIFO poll fallback at 10ms interval |
| UART RX interrupt storm: `UART0_ICR` not cleared | Explicit `*UART0_ICR = 0x7FF` after FIFO drain |
| `CNTP_TVAL_EL0` reload causes timer drift | Switched to `CNTP_CVAL_EL0` absolute deadline (DEFERRED-03 closed) |

---

## 5. Deferred Items — Open at Release

| ID | Description | Trigger Condition |
|---|---|---|
| DEFERRED-02 | SMP memory barriers around mailbox access | Any multi-core unlock |
| DEFERRED-06 | Yield primitive / context scheduler | Computationally intensive module required |
| DEFERRED-07 | Kernel UART access migration to UART cartridge | Resolves with DEFERRED-06 |
| DEFERRED-08 | Promote syscall numbers to `os_syscall.h` | Before third out-of-tree repository |
| DEFERRED-09 | Physical UART1 / ISO 7816 hardware integration | Hardware deployment phase |
| DEFERRED-10 | Concurrent cartridge loading memory barriers | Multi-core loading model |

---

## 6. What Went Right

**MMU and W^X stability.** Page tables correctly isolated all five cartridges. Permission faults were correctly raised when EL0 attempted to write read-only pages, providing precise fault addresses that guided debugging.

**Capability matrix security.** The matrix correctly blocked all unauthorised routes throughout testing. The `SHELL → KERNEL = 0` invariant held under all test conditions. The matrix was never modified at runtime.

**Diagnostic discipline.** When execution failures were ambiguous, `FAR_EL1` dumps, direct UART tracers, and `aarch64-elf-objdump` section analysis consistently provided the information needed to identify root causes without a hardware debugger.

**Exception handling.** The kernel correctly caught EL0 faults, printed `ESR` and `FAR` values, and halted cleanly without corrupting kernel state. This allowed iterative debugging of userspace issues without requiring reboots.

---

## 7. What Went Wrong

**Linker script complexity.** The interaction between `objcopy` section extraction, GNU linker empty-section behaviour, and MMU page alignment produced multiple cascading failures. The `QUAD(0)` + `ALIGN(4096)` combination was required to force deterministic section boundaries.

**Build tool chain propagation.** Stale cartridges in the `ai-os` directory from earlier builds caused multiple debugging sessions to target the wrong binary. The `make clean` discipline was inconsistently applied.

**Text editor bracket stripping.** A persistent environment issue caused array indices to be silently deleted during copy-paste operations, reproducing BLOCKER-09 and BLOCKER-25 class bugs across multiple phases. Spacing inside brackets (`[ 0 ]`) was adopted as a workaround.

**Silent Python failures.** `subprocess.run` with `shell=False` failed to locate the toolchain when invoked from `make`, with the `try/except` block returning `0` silently. This caused `BSS=0b` in cartridge headers, which prevented correct memory allocation by the loader. The fix required `shutil.which` with explicit hard failure.

---

## 8. AI Orchestration Methodology

This project was developed entirely through a structured multi-agent AI pipeline over the course of multiple sessions:

**Gemini Pro** — Lead Developer. Generated architecture proposals, implementation code, and build scripts based on project requirements and QA feedback.

**Claude Sonnet 4.6** — Senior QA Architect. Issued structured audit reports with blocker/advisory/deferred classification. Reviewed all files before compilation authorisation. Maintained the audit trail across sessions.

**Kevin Davies** — Hardware Interface. Executed approved builds, provided terminal output and screenshots, and relayed results between the two AI systems.

No code was manually modified without explicit AI instruction. All architectural decisions were documented in QA reports before implementation. The pipeline successfully produced a security-auditable, capability-based operating system through AI collaboration.

---

## 9. Conclusion

The AI-OS-Smartcard POS terminal simulation demonstrates that a capability-based microkernel with hardware-enforced memory isolation can be built entirely through structured AI orchestration with a rigorous audit discipline. The system handles real hardware interrupts, enforces privilege boundaries, and routes application protocol messages through a verifiable security layer — all within a 256KB memory constraint matching real smartcard silicon.

The foundation is complete. 

---

*Full audit trail: 53 blockers, 23 advisories, 10 deferred items across 10 engineering phases.*


