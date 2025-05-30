#include <lcom/lcf.h>
#include <stdint.h>

#include "keyboard.h"
#include "timer.h"
#include "i8042.h"

uint64_t await_interrupt(uint64_t mask){
    int ipc_status;
    message msg;
    int r;

    while(1) {
        if ( (r = driver_receive(ANY, &msg, &ipc_status)) != 0) { 
            printf("driver_receive failed with: %d", r);
            continue;
        }
    
        if (is_ipc_notify(ipc_status)) { /* received notification */
        switch (_ENDPOINT_P(msg.m_source)) {
            case HARDWARE: /* hardware interrupt notification */				
                if (msg.m_notify.interrupts & mask) { /* subscribed interrupt */
                    return msg.m_notify.interrupts;
                }
                break;
            default:
                break; /* no other notifications expected: do nothing */
        }
        }
    }
}
