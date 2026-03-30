#include "os_types.h"
#include "os_dispatcher.h"
#include "os_ipc.h"

// SYS_MOD_CARDSIM is now inherited from os_ipc.h
// WARNING: SYS_MOD_CARDSIM (5) and SYS_HANDLER_DONE (5) share the same value.
// No collision occurs as they occupy different registers (x0 vs x8).
#define SYS_MODULE_REGISTER  3
#define SYS_INIT_DONE        4
#define SYS_HANDLER_DONE     5

void cardsim_handler(os_message_t *msg);

void _start(void) {
    __asm__ volatile("mov x0, %0\n\t" "adr x1, cardsim_handler\n\t" "mov x8, %1\n\t" "svc 0\n\t" 
                     :: "i"(SYS_MOD_CARDSIM), "i"(SYS_MODULE_REGISTER) : "x0", "x1", "x8");
                     
    __asm__ volatile("mov x8, %0\n\t" "svc 0\n\t" :: "i"(SYS_INIT_DONE) : "x8");
    while(1) { __asm__ volatile("wfi"); }
}

void cardsim_handler(os_message_t *msg) {
    if (msg->type == IPC_TYPE_APDU_COMMAND) {
        
        if (msg->length >= 5) {
            if (msg->payload [ 0 ] == 0x00 &&
                msg->payload [ 1 ] == 0xA4 &&
                msg->payload [ 2 ] == 0x04 &&
                msg->payload [ 3 ] == 0x00 &&
                msg->payload [ 4 ] == 0x00) {

                os_message_t resp = { 0 };
                resp.target_id = msg->sender_id;
                resp.type = IPC_TYPE_APDU_RESPONSE;
                resp.length = 2;
                resp.payload [ 0 ] = 0x90;
                resp.payload [ 1 ] = 0x00;

                uint64_t ret_code;
                __asm__ volatile(
                    "mov x1, %1\n\t" "mov x8, %2\n\t" "svc 0\n\t" "mov %0, x0\n\t"
                    : "=r"(ret_code) : "r"(&resp), "i"(1) : "x1", "x8", "x0"
                );

                if (ret_code == IPC_ERR_FULL) {
                    /* Gracefully drop response if the target mailbox is full */
                }
            }
        }
    }
    
    __asm__ volatile("mov x8, %0\n\t" "svc 0\n\t" :: "i"(SYS_HANDLER_DONE));
}