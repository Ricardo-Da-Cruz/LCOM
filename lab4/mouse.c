#include <lcom/lcf.h>

#include <stdint.h>
#include "i8042.h"
#include "utils.h"

int mouse_hook_id = 2;
uint8_t mouse_status;
uint8_t mouse_scancode;

int (mouse_subscribe_int)(uint8_t *bit_no) {
    mouse_hook_id = *bit_no;
    *bit_no = BIT(*bit_no);
    return sys_irqsetpolicy(MOUSE_IRQ, IRQ_REENABLE | IRQ_EXCLUSIVE, &mouse_hook_id);
}

int (mouse_unsubscribe_int)() {
    return sys_irqrmpolicy(&mouse_hook_id);
}

void (mouse_ih)(){
    util_sys_inb(KBC_ST_REG, &mouse_status);
    util_sys_inb(KBC_OUT_BUF,&mouse_scancode);
}

int (avoid_ibf)() {
    int tries = 3;
    uint8_t stat;

    while(tries-- > 0) {
        if (util_sys_inb(KBC_ST_REG, &stat))
            continue;

        if ((stat & KBC_IBF) == 0)
            return 0;

        tickdelay(micros_to_ticks(WAIT_KBC));
    }

    return 1;
}

int (write_mouse_cmd)(uint8_t cmd) {
    int tries = 10;
    uint8_t stat;

    while(--tries > 0) {

        if (avoid_ibf())return 1;
        if (sys_outb(KBC_CMD_REG, KBC_WRITE_TO_MOUSE))return 1;
        if (avoid_ibf())return 1;
        if (sys_outb(0x60, (uint32_t)cmd))return 1;

        tickdelay(micros_to_ticks(WAIT_KBC));

        if (util_sys_inb(KBC_OUT_BUF, &stat))return 1;
        if (stat != MOUSE_ACK)return 1;

        return 0;
    }

    return 1;
}

int construct_packet(struct packet *packet, int p, uint8_t scancode) {
    packet->bytes[p] = mouse_scancode;
    switch(p){
        case 0:
            packet->lb = (mouse_scancode & MOUSE_LEFT_BUTTON) != 0;
            packet->rb = (mouse_scancode & MOUSE_RIGHT_BUTTON) != 0;
            packet->mb = (mouse_scancode & MOUSE_MIDDLE_BUTTON) != 0;
            packet->x_ov = (mouse_scancode & MOUSE_X_OV) != 0;
            packet->y_ov = (mouse_scancode & MOUSE_Y_OV) != 0;
            packet->delta_x = ((mouse_scancode & MOUSE_X_MSB) != 0 ? -(1 << 8) : 0);
            packet->delta_y = ((mouse_scancode & MOUSE_Y_MSB) != 0 ? -(1 << 8) : 0);
            break;
        case 1:
            packet->delta_x += mouse_scancode;
            break;
        case 2:
            packet->delta_y += mouse_scancode;
            mouse_print_packet(packet);
            break;
    }
    return 0;
}

