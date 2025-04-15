#ifndef _VBE_H_
#define _VBE_H_

#define VBE_AH 0x4F
#define BIOS_AH 0x00
#define BIOS_TEXT_MODE 0x03
#define INT_VIDEO_CARD 0x10

#define GET_VBE_INFO_AL 0x01
#define SET_VBE_MODE_AL 0x02
#define GET_VBE_CONTR_INFO_AL 0x00

#define R_1024_X_768_I 0x105
#define R_640_X_480_D 0x110
#define R_800_X_600_D 0x115
#define R_1280_X_1024_D 0x11A
#define R_1152_X_864_D 0x11B

#define LINEAR_FRAMEBUFFER 1 << 14

#endif
