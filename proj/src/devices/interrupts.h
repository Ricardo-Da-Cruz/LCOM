#ifndef _LCOM_INTERRUPTS_H_
#define _LCOM_INTERRUPTS_H_

#include <stdint.h>

uint8_t timer_irq = 0;
uint8_t kbd_irq = 1;


uint64_t await_interrupt(uint64_t mask);

#endif
