#ifndef _LCOM_I8042_H_
#define _LCOM_I8042_H_

#include <lcom/lcf.h>

#define KEYBOARD_IRQ 1

#define KBC_OUT_BUF 0x60
#define KBC_ST_REG 0x64
#define KBC_CMD_REG 0x64

#define ESC_MAKE_CODE 0x81

#define BREAK_CODE_BIT 1 << 7
#define MULTIBYTE_CODE 0xE0

#define KBC_OBF 1 << 0
#define KBC_IBF 1 << 1
#define KBC_AUX 1 << 5
#define KBC_TIMEOUT_ERR 1 << 6
#define KBC_PARITY_ERR 1 << 7

#define KBC_ENABLE_INT 0xAE
#define KBC_DISABLE_INT 0xAD

#define WAIT_KBC 100

#define W_MAKE_CODE 0x11
#define A_MAKE_CODE 0x1E
#define S_MAKE_CODE 0x1F
#define D_MAKE_CODE 0x20

#define ENTER_MAKE_CODE 0x1C


#define MOUSE_Y_OV 1 << 7
#define MOUSE_X_OV 1 << 6
#define MOUSE_MLB 1 << 0
#define MOUSE_Y_MSB 1 << 5
#define MOUSE_X_MSB 1 << 4
#define MOUSE_FIRST_BYTE_BIT 1 << 3
#define MOUSE_MIDDLE_BUTTON 1 << 2
#define MOUSE_RIGHT_BUTTON 1 << 1
#define MOUSE_LEFT_BUTTON 1 << 0

#define MOUSE_ACK 0xFA
#define MOUSE_NACK 0xFE
#define MOUSE_ERROR 0xFC

#define MOUSE_ENABLE_DATA_REPORTING 0xF4
#define MOUSE_DISABLE_DATA_REPORTING 0xF5

#define KBC_WRITE_TO_MOUSE 0xD4

#define MOUSE_IRQ 12





#endif /* _LCOM_I8254_H */
