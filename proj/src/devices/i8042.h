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

#endif /* _LCOM_I8254_H */
