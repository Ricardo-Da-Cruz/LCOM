#include "stdint.h"
#include "lcom/lcf.h"
#include "i8042.h"
#include "utils.h"
#include "mouse.h"

uint8_t status;
uint8_t data_byte;
uint8_t packet_bytes[3];
uint8_t packet_size;
uint32_t packet_count;
int mouse_interrupt_id = MOUSE_IRQ;
int packet_index = 0;
struct packet packet_struct;
bool read_error_flag;
int mouse_x = 320;
int mouse_y = 240;

// State machine states
enum STATE current_state = INITIAL;
int line_slope = 2;
int x_position = 0;
int y_position = 0;

/**
 * State Machine Logic:
 * 1. Initial State
 * 1-> 2 via click left
 * 2 -> 2 while abs(expected - current) <= tolerance & left is pressed
 * 2 -> 1 if abs(expected - current) > tolerance
 * 2 -> 3 if left released
 * 3 -> 3 while right is not clicked
 * 3 -> 1 if button other than right is clicked or abs(expected - current) > tolerance
 * 3 -> 4 if right is clicked and only right
 * 4 -> 4 while abs(expected - current) <= tolerance & right is pressed
 * 4 -> 1 if right release, if abs(expected - current) <= tolerance or another button is clicked
**/
void draw_mouse_cursor(int x, int y) {
    vg_draw_rectangle(x, y, 2, 10, 0xFFFFFF);    
    vg_draw_rectangle(x, y, 6, 2, 0xFFFFFF);     
    vg_draw_rectangle(x, y + 3, 4, 2, 0xFFFFFF); 
    vg_draw_rectangle(x, y + 6, 3, 2, 0xFFFFFF); 
}

bool is_point_in_rect(int px, int py, int rx, int ry, int width, int height) {
    return (px >= rx && px <= rx + width && py >= ry && py <= ry + height);
}

bool (check_inbound)(int *x, int *y, int x_offset, int y_offset, int tolerance) {
  int new_x_position, new_y_position;

  if(x_offset == 0 && y_offset == 0) return true;

  new_x_position = *x + x_offset;
  new_y_position = *y + y_offset;

  int lower_bound = abs(line_slope * new_x_position) - tolerance;
  int upper_bound = abs(line_slope * new_x_position) + tolerance;

  if((new_y_position >= lower_bound) && (new_y_position <= upper_bound)) {
    *x = new_x_position;  // CORREÇÃO: usar = em vez de +=
    *y = new_y_position;  // CORREÇÃO: usar = em vez de +=
    return true;
  } else {
    *x = 0;
    *y = 0;
    return false;
  }
}


int (next_state)(struct packet pp, int tolerance) {
  switch (current_state) {
  case INITIAL:
    if(pp.lb && !pp.mb && !pp.rb) {
      x_position += pp.delta_x;
      y_position += pp.delta_y;
      current_state = FIRST;
    }
    break;
  case FIRST: // release left button
    if(check_inbound(&x_position, &y_position, pp.delta_x, pp.delta_y, tolerance)) {
      if(pp.mb) {
        current_state = INITIAL;
      } else if(!pp.lb) {
        current_state = SECOND;
      } else {
        current_state = FIRST;  // just for clarity
      }
    } else {
      current_state = INITIAL;
    }
    break;
  case SECOND:
    if(check_inbound(&x_position, &y_position, pp.delta_x, pp.delta_y, tolerance)) {
      if(!pp.rb && !pp.mb && !pp.lb) current_state = SECOND;    // just for clarity
      if(pp.mb || pp.lb) current_state = INITIAL;
      if(pp.rb) current_state = THIRD;
    } else {
      current_state = INITIAL;
    }
    break;
  case THIRD:
    if(check_inbound(&x_position, &y_position, pp.delta_x, pp.delta_y, tolerance)) {
      if(pp.mb || pp.lb) {
        current_state = INITIAL;
      } else if (!pp.rb) {
        current_state = INITIAL;
      }
      break;
    } else {
      current_state = INITIAL;
      break;
    }
    current_state = THIRD;  // just for clarity
  default:
    break;
  }
  return 0;
}

int (kbc_write_register)(uint8_t port, uint8_t message) {
  int attempts = MAX_NUM_TRIES;
  while(attempts) {
    util_sys_inb(KBC_STATUS_PORT, &status);
    if(((status & KBC_IBF) == 0)) {
      sys_outb(port, (uint32_t) message);
      return 0;
    }
    attempts--;
    // tickdelay(micros_to_ticks(DELAY_US));
  }
  return 1;
}

int (kbc_read_register)(uint8_t port, uint8_t *message) {
  int attempts = MAX_NUM_TRIES;
  while(attempts) {
    if(util_sys_inb(KBC_STATUS_PORT, &status) != 0) return 1;
    if((status & (KBC_PAR_ERR | KBC_TO_ERR)) != 0) return 1;
    if((status & KBC_OBF) && (status & (KBC_PAR_ERR | KBC_TO_ERR)) == 0 && (status & KBC_AUX_BYTE)) {
      if(util_sys_inb(port, message) != 0) return 1;
      return 0;
    }
    attempts--;
    // tickdelay(micros_to_ticks(DELAY_US));
  }
  return 1;
}

int (mouse_write_register)(uint8_t command) {
  int attempts = MAX_NUM_TRIES;
  uint8_t ack;
  while(attempts) {
    if(kbc_write_register(KBC_COMMAND_PORT, KBC_MOUSE_COMMAND) != 0) return 1;
    if(kbc_write_register(KBC_IN_BUF, command) != 0) return 1;
    kbc_read_register(KBC_OUT_BUF, &ack);
    if(ack == MOUSE_ACK) return 0;
    attempts--;
  }
  return 1;
}

int (mouse_read_command_byte)(uint8_t *value) {
  while(mouse_write_register(MOUSE_READ_COMMAND_BYTE) != 0) {
    if(kbc_read_register(KBC_OUT_BUF, value) != 0) return 1;
  }
  return 0;
}

void (mouse_build_packet)() {
  packet_struct.bytes[0] = packet_bytes[0];
  packet_struct.bytes[1] = packet_bytes[1];
  packet_struct.bytes[2] = packet_bytes[2];
  packet_struct.rb = (packet_bytes[0] & MOUSE_RIGHT_BUTTON) != 0;
  packet_struct.mb = (packet_bytes[0] & MOUSE_MIDDLE_BUTTON) != 0;
  packet_struct.lb = (packet_bytes[0] & MOUSE_LEFT_BUTTON) != 0;

  if (packet_bytes[0] & MOUSE_MSB_X_DELTA) {
    packet_struct.delta_x = (int16_t)(packet_bytes[1] | 0xFF00);
  } else {
    packet_struct.delta_x = (int16_t)packet_bytes[1];
  }

  if (packet_bytes[0] & MOUSE_MSB_Y_DELTA) {
    packet_struct.delta_y = (int16_t)(packet_bytes[2] | 0xFF00);
  } else {
    packet_struct.delta_y = (int16_t)packet_bytes[2];
  }
  packet_struct.x_ov = (packet_bytes[0] & MOUSE_X_OVERFLOW) != 0;
  packet_struct.y_ov = (packet_bytes[0] & MOUSE_Y_OVERFLOW) != 0;
}

int (mouse_subscribe_int)(uint8_t *bit_no) {
  *bit_no = mouse_interrupt_id;
  if(sys_irqsetpolicy(MOUSE_IRQ, IRQ_REENABLE | IRQ_EXCLUSIVE, &mouse_interrupt_id) != 0) return 1;
  return 0;
}

int (mouse_unsubscribe_int)() {
  if(sys_irqrmpolicy(&mouse_interrupt_id) != 0) return 1;
  return 0;
}

void (mouse_ih)() {
  if (kbc_read_register(KBC_OUT_BUF, &data_byte) != 0) read_error_flag = 1;
}

int (mouse_reset)() {
    uint8_t response;
    int attempts = 3;
    printf("Attempting mouse reset...\n");
    while(attempts--) {
        if(mouse_write_register(0xFF) == 0) {
            printf("Reset command sent, waiting for responses...\n");
            if(kbc_read_register(KBC_OUT_BUF, &response) == 0 && response == MOUSE_ACK) {
                printf("ACK received\n");
                if(kbc_read_register(KBC_OUT_BUF, &response) == 0 && response == 0xAA) {
                  printf("BAT passed (0xAA)\n");
                  if(kbc_read_register(KBC_OUT_BUF, &response) == 0 && response == 0x00) {
                      printf("Device ID received (0x00)\n");
                      printf("Mouse reset successful\n");
                      return 0;
                  } else {
                      printf("Invalid Device ID: 0x%02X\n", response);
                    }
                } else {
                    printf("BAT failed or invalid response: 0x%02X\n", response);
                  }
            } else {
                printf("No ACK received or invalid response: 0x%02X\n", response);
                }   
        } else {
            printf("Failed to send reset command\n");
          }
        printf("Reset attempt %d failed, retrying...\n", 3 - attempts);
        tickdelay(micros_to_ticks(5000)); // 5ms delay entre tentativas
    }
    printf("Mouse reset failed after all attempts\n");
    return 1;
}

void (mouse_synch_packet)() {
    if (packet_index == 0) {
        if (data_byte & BIT(3)) {
            packet_bytes[packet_index] = data_byte;
            packet_index++;
        }
    } else if (packet_index < 3) {
        packet_bytes[packet_index] = data_byte;
        packet_index++;
        if (packet_index == 3) {
            packet_index = 0;
        }
    }
}

