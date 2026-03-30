#include "os_types.h"
#include "os_dispatcher.h"
#include "os_ipc.h" /* Added per QA's hint to resolve IPC types natively */

// WARNING: SYS_MOD_ISO7816 and SYS_INIT_DONE are both 4 by coincidence.
// They occupy different registers (x0 vs x8) so no runtime collision occurs.
// DEFERRED-08: Promote syscall numbers to os_syscall.h to prevent future collisions.
#define SYS_MOD_ISO7816      4
#define SYS_MODULE_REGISTER  3
#define SYS_INIT_DONE        4
#define SYS_HANDLER_DONE     5

/* Hardcoded Target ID for UART Cartridge */
#define SYS_MOD_UART         1

/* Hardware Pointers: Emulating the Smartcard Reader on UART1 */
#define SC_UART_DR  ((volatile uint32_t *)0x09040000)
#define SC_UART_FR  ((volatile uint32_t *)0x09040018)

void iso7816_handler(os_message_t *msg);
void debug_print(const char *str);
void uart1_send(uint8_t byte);
uint8_t read_uart1_byte(uint8_t *out);

void _start(void) {
    /* 1. Register the module */
    __asm__ volatile("mov x0, %0\n\t" "adr x1, iso7816_handler\n\t" "mov x8, %1\n\t" "svc 0\n\t" 
                     :: "i"(SYS_MOD_ISO7816), "i"(SYS_MODULE_REGISTER) : "x0", "x1", "x8");
    
    /* 2. Signal init is complete immediately (No I/O blocking!) */
    __asm__ volatile("mov x8, %0\n\t" "svc 0\n\t" :: "i"(SYS_INIT_DONE) : "x8");
    
    /* 3. Sleep and wait for IPC messages */
    while(1) { __asm__ volatile("wfi"); }
}

void iso7816_handler(os_message_t *msg) {
    /* Only execute handshake when the kernel acknowledges boot completion */
    if (msg->type == IPC_TYPE_SYS_ACK) {
        debug_print("\r\n[ISO7816] Received SYS_ACK. Waking up smartcard on UART1...\r\n");
        
        /* Send standard SELECT FILE APDU (00 A4 04 00 00) */
        uart1_send(0x00); 
        uart1_send(0xA4); 
        uart1_send(0x04); 
        uart1_send(0x00); 
        uart1_send(0x00); 
        
        uint8_t sw1 = 0, sw2 = 0;
        
        /* Read with Timeout Guards */
        if (read_uart1_byte(&sw1) && read_uart1_byte(&sw2)) {
            if (sw1 == 0x90 && sw2 == 0x00) {
                debug_print("[ISO7816] Secure Channel Established (90 00)!\r\n");
            } else {
                debug_print("[ISO7816] Card Error: Unexpected Status Word.\r\n");
            }
        } else {
            debug_print("[ISO7816] TIMEOUT: Smartcard did not respond.\r\n");
        }
    }
    
    /* Yield CPU back to kernel */
    __asm__ volatile("mov x8, %0\n\t" "svc 0\n\t" :: "i"(SYS_HANDLER_DONE));
}

/* Guarded TX */
void uart1_send(uint8_t byte) {
    uint32_t timeout = 1000000;
    while ((*SC_UART_FR & (1 << 5)) != 0) { 
        if (--timeout == 0) return; 
    }
    *SC_UART_DR = byte;
}

/* Guarded RX (QA Requirement) */
uint8_t read_uart1_byte(uint8_t *out) {
    uint32_t timeout = 1000000; 
    while ((*SC_UART_FR & (1 << 4)) != 0) { 
        if (--timeout == 0) return 0; /* Timeout — return failure */
    }
    *out = (uint8_t)*SC_UART_DR;
    return 1; /* Success */
}

/* Route through IPC — do not access UART0 hardware directly from EL0 */
void debug_print(const char *str) {
    while (*str) {
        os_message_t out = { 0 };
        out.target_id = SYS_MOD_UART;
        out.type = IPC_TYPE_CHAR_OUT;
        out.length = 1;
        out.payload [ 0 ] = (uint8_t)*str++;
        
        /* syscall 1 is IPC_SEND */
        __asm__ volatile(
            "mov x1, %0\n\t" "mov x8, %1\n\t" "svc 0\n\t"
            :: "r"(&out), "i"(1) : "x1", "x8"
        );
    }
}