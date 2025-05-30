#ifndef _LCOM_MOUSE_H
#define _LCOM_MOUSE_H

#include <lcom/lcf.h>
#include "i8042.h"
#include <stdbool.h>
#include <stdint.h>
enum STATE {
  INITIAL,
  FIRST,
  SECOND,
  THIRD,
  FOURTH
};

enum DIRECTION {
  UP,
  DOWN,
  VERTEX
};

int (kbc_write_register)(uint8_t port,uint8_t message);
int (kbc_read_register)(uint8_t port, uint8_t *message);
int (mouse_write_register)(uint8_t command);
int (mouse_read_command_byte)(uint8_t *value);
void (mouse_build_packet)();
int (mouse_subscribe_int)(uint8_t *bit_no);
int (mouse_unsubscribe_int)();
void (mouse_ih)();
void (mouse_synch_packet)();
int (mouse_reset)();
void (draw_mouse_cursor)(int x, int y);
bool (is_point_in_rect)(int px, int py, int rx, int ry, int width, int height);
int (next_state)(struct packet pp, int tolerance);
bool (check_inbound)(int *x,int *y, int x_offset, int y_offset, int tolerance);

extern int mouse_x;
extern int mouse_y;
extern struct packet packet_struct;
extern uint8_t packet_bytes[3];
extern int packet_index;
extern bool read_error_flag;

#endif
