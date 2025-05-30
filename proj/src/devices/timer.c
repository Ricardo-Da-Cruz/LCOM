#include <lcom/lcf.h>
#include <lcom/timer.h>
#include <stdint.h>

#include "i8254.h"
int timer_hook_id = 1;
int counter = 0;


int (timer_set_frequency)(uint8_t timer, uint32_t freq) {
  uint8_t ctrl;
  if (timer_get_conf(timer, &ctrl))
    return 1;

  uint16_t count = TIMER_FREQ / freq;

  //to maintain the 4 LSBs I read the values and remove the 4 MSBs
  ctrl &= 0x0f;
  ctrl |= timer << 6;
  ctrl |= TIMER_LSB_MSB;

  if (sys_outb(TIMER_CTRL, ctrl))
    return 1;

  if (sys_outb(TIMER_0 + timer, count & 0xff))
    return 1;

  if (sys_outb(TIMER_0 + timer, count >> 8))
    return 1;

  return 0;
}

int (timer_subscribe_int)(uint8_t *bit_no) {
  *bit_no = 1 << timer_hook_id;
  return sys_irqsetpolicy(TIMER0_IRQ, IRQ_REENABLE, &timer_hook_id);
}


int (timer_unsubscribe_int)() {
  return sys_irqrmpolicy(&timer_hook_id);
}

void (timer_int_handler)() {
  counter++;
}

int (timer_get_conf)(uint8_t timer, uint8_t *st) {
  // TIMER_RB_COUNT_ is inactive and TIMER_RB_STATUS_ is active in the command
  uint8_t cmd = TIMER_RB_CMD | TIMER_RB_COUNT_ | TIMER_RB_SEL(timer);

  if (sys_outb(TIMER_CTRL, cmd))
    return 1;

  if (util_sys_inb(TIMER_0 + timer, st))
    return 1;

  return 0;
}

int (timer_display_conf)(uint8_t timer, uint8_t st,
                        enum timer_status_field field) {
  union timer_status_field_val conf;

  switch (field) {
    case tsf_all:
      conf.byte = st;
      break;
    case tsf_initial:
      //enums start at 0
      conf.in_mode = (st & TIMER_LSB_MSB) >> 4;
      break;
    case tsf_mode:
      if ((st & (BIT(3) | BIT(2))) == (BIT(3) | BIT(2)))
        st -= BIT(3);

      conf.count_mode = (st & (BIT(3) | BIT(2) | BIT(1))) >> 1;
      
      break;
    case tsf_base:
      conf.bcd = (st & TIMER_BCD);
      break;
    default:
      return 1;
  }


  if (!timer_print_config(timer, field, conf))
    return 1;

  return 0;
}

