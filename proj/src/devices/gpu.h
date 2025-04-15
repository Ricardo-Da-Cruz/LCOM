#ifndef _GPU_H_
#define _GPU_H_

extern vbe_mode_info_t vmi;

int (set_graphics_mode)(uint16_t mode);

int (vg_draw_hline)(uint16_t x, uint16_t y, uint16_t len, uint32_t color);

int (vg_draw_rectangle)(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint32_t color);

int (draw_xpm)(xpm_image_t xpm, uint16_t x, uint16_t y);

int (exit_graphics_mode)();

#endif
