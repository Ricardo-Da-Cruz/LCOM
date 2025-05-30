
#include "stdint.h"
#include "utils.h"
#include "lcom/lcf.h"
#include "i8042.h"

int kbd_hook_id = 1;
uint8_t status;
uint8_t scancode;

int (keyboard_subscribe_int)(uint8_t *bit_no) {
    kbd_hook_id = *bit_no;
    *bit_no = BIT(*bit_no);
    return sys_irqsetpolicy(KEYBOARD_IRQ, IRQ_REENABLE | IRQ_EXCLUSIVE, &kbd_hook_id);
}

int (keyboard_unsubscribe_int)() {
    return sys_irqrmpolicy(&kbd_hook_id);
}

void (kbc_ih)(){
    util_sys_inb(KBC_ST_REG, &status);
    util_sys_inb(KBC_OUT_BUF,&scancode);
}

int (write_cmd)(uint8_t cmd) {
    uint8_t stat;

    int count = 0;

    while(count++ < 10) {

        if (util_sys_inb(KBC_ST_REG, &stat))
            continue;

        /* loop while 8042 input buffer is not empty */
        if((stat & KBC_IBF) == 0) {
           sys_outb(KBC_CMD_REG, (uint32_t) cmd); /* no args command */
           return 0;
        }
        tickdelay((WAIT_KBC));
        //delay(WAIT_KBC); // e.g. tickdelay()
    }

    return 1;
}

int (enable_kbc_int)() {
    return write_cmd(KBC_ENABLE_INT);
}

int (disable_kbc_int)() {
    return write_cmd(KBC_DISABLE_INT);
}

int (kbc_poll)(){
    while(1) {
        if(util_sys_inb(KBC_ST_REG, &status))
            continue;

        /* loop while 8042 output buffer is empty */
        if((status & KBC_OBF) && ((status & KBC_AUX) == 0)) {
            if(util_sys_inb(KBC_OUT_BUF, &scancode))
                continue;
            if ((status & (KBC_PARITY_ERR | KBC_TIMEOUT_ERR)) == 0)
                return scancode;
            else
                return -1;
        }
        tickdelay(WAIT_KBC);
    }
    return 0;
}


int (verify_status)() {
    return !(status & KBC_PARITY_ERR || status & KBC_TIMEOUT_ERR);
}

