#ifndef _LCOM_MOUSE_H_
#define _LCOM_MOUSE_H_
#include <stdint.h>

extern uint8_t scancode;
extern uint8_t mouse_status;
extern uint8_t mouse_scancode;

int (mouse_subscribe_int)(uint8_t *bit_no);

int (mouse_unsubscribe_int)();

void (mouse_ih)();

int (write_mouse_cmd)(uint8_t cmd);

int (construct_packet)(struct packet *packet, int p, uint8_t scancode);

int (my_mouse_enable_data_reporting)(void);

int (mouse_disable_data_reporting)(void);

int (avoid_ibf)();


#endif

