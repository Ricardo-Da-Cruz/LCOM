#ifndef _LCOM_KEYBOARD_
#define _LCOM_KEYBOARD_

#include <stdbool.h>
#include <stdint.h>

extern uint8_t scancode;
extern uint8_t status;

int (keyboard_subscribe_int)(uint8_t *bit_no);

int (keyboard_unsubscribe_int)();

int (enable_kbc_int)();

int (disable_kbc_int)();

void (kbc_ih)();

int (kbc_poll)();

int (verify_status)();

#endif
