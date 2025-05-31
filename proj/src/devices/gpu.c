#include <lcom/lcf.h>
#include <stdint.h>
#include <machine/int86.h>
#include "vbe.h"

vbe_mode_info_t vmi;

void* video_mem;
void* comp_video_mem;

int (map_vram)(unsigned int vram_base, unsigned int vram_size){
    int r;
    struct minix_mem_range mr;
    mr.mr_base = (phys_bytes) vram_base;
    mr.mr_limit = mr.mr_base + vram_size * 2;

    if((r = sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr))){
        printf("sys_privctl (ADD_MEM) failed: %d\n", r);    
        return 1;
    }
    
    video_mem = vm_map_phys(SELF, (void *)mr.mr_base, vram_size * 2);
    comp_video_mem = video_mem;
    memset(video_mem, 0, vram_size * 2);

    if(video_mem == MAP_FAILED){
        printf("couldn’t map video memory");
        return 1;
    }

    return 0;
}

int (set_graphics_mode)(uint16_t mode){
    reg86_t r;
    memset(&r, 0, sizeof(r));

    r.intno = INT_VIDEO_CARD;
    r.ah = VBE_AH;
    r.al = SET_VBE_MODE_AL;
    r.bx = LINEAR_FRAMEBUFFER | mode;

    if (vbe_get_mode_info(mode, &vmi))
        return 1;

    if ((map_vram(vmi.PhysBasePtr, vmi.XResolution * vmi.YResolution * ((vmi.BitsPerPixel + 7) / 8))))
        return 1;

    if (sys_int86(&r))
        return 1;

    if (r.al != 0x4f)
        return 1;

    if (r.ah != 0x00)
        return 1;

    return 0;
}

int refresh_screen(){
    reg86_t r;
    memset(&r, 0, sizeof(r));

    r.intno = INT_VIDEO_CARD;
    r.ah = VBE_AH;
    r.al = SET_DISPLAY_START_AL;
    r.bx = 0x00;
    r.cx = 0;
    r.dx = video_mem == comp_video_mem? 0 : vmi.YResolution;

    if (sys_int86(&r)){
        printf("sys_int86 failed\n");
        return 1;
    }

    if (r.al != 0x4f || r.ah != 0x00){
        printf("al: %x ah: %x\n", r.al, r.ah);
        return 1;
    }

    if (video_mem == comp_video_mem)
        video_mem = ((char*)comp_video_mem) + vmi.XResolution * vmi.YResolution * ((vmi.BitsPerPixel + 7) / 8);
    else
        video_mem = comp_video_mem;
    
    memset(video_mem, 0, vmi.XResolution * vmi.YResolution * ((vmi.BitsPerPixel + 7) / 8));

    return 0;
}

int (exit_graphics_mode)(void){
    reg86_t r;
    memset(&r, 0, sizeof(r));
    r.al = BIOS_TEXT_MODE;
    r.ah = BIOS_AH;
    r.intno = INT_VIDEO_CARD;

    free(video_mem);

    if (sys_int86(&r))
        return 1;

    return 0;
}

int (vg_draw_hline)(uint16_t x, uint16_t y, uint16_t len, uint32_t color){
    // I think that 0x110 with 15 bits per pixel is aligned to 16 bits
    // so I can iterate through the bytes of the video memory because all modes are divided by 8
    uint8_t bytes_per_pixel = (vmi.BitsPerPixel + 7) / 8; // + 7 to round up
    uint8_t *coord = ((uint8_t*)video_mem) + (y * vmi.XResolution + x) * bytes_per_pixel;

    for (int i = 0; i < len; i++){
        for (int j = 0; j < bytes_per_pixel ; j++){
            *coord = (color >> ((bytes_per_pixel - 1 - j) * 8));
            coord++;
        }
    }
    
    return 0;
}

int (vg_draw_rectangle)(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint32_t color){
    int bytes_per_pixel = (vmi.BitsPerPixel + 7) / 8;
    uint8_t *coords = ((uint8_t*)video_mem) + (y * vmi.XResolution + x) * bytes_per_pixel;
    uint8_t bytes[] = {color >> 24, color >> 16, color >> 8, color};
    for (int i = 0; i < height; i++){
        for (int j = 0; j < width; j++){
            for (int k = 4 - bytes_per_pixel; k < 4; k++){
                *coords = bytes[k];
                coords++;
            }
        }
        coords += (vmi.XResolution - width) * bytes_per_pixel;
    }
    return 0;
}

int (vg_draw_rectangle_xpm)(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint32_t color, xpm_image_t img){
    int bytes_per_pixel = (vmi.BitsPerPixel + 7) / 8;
    uint8_t *coords = ((uint8_t*)img.bytes) + (y * img.width + x) * bytes_per_pixel;
    uint8_t bytes[] = {color >> 24, color >> 16, color >> 8, color};
    for (int i = 0; i < height; i++){
        for (int j = 0; j < width; j++){
            for (int k = 4 - bytes_per_pixel; k < 4; k++){
                *coords = bytes[k];
                coords++;
            }
        }
        coords += (img.width - width) * bytes_per_pixel;
    }
    return 0;
}

//only works for 8:8:8 probably
int (draw_xpm)(xpm_image_t xpm, uint16_t x, uint16_t y){
    int a = 0;
    int num_bytes = ((vmi.BitsPerPixel + 7) / 8);
    uint8_t *coord = ((uint8_t*)video_mem) + (y * vmi.XResolution + x) * num_bytes;

    for (int i = 0; i < xpm.height; i++){
        for (int j = 0; j < xpm.width; j++){
            //xpm_load uses 0x00b140 as chroma key green for transparency
            //for some reason the color is inverted
            if (xpm.bytes[a] == 0x40 && xpm.bytes[a + 1] == 0xb1 && xpm.bytes[a + 2] == 0x00){
                a += num_bytes;
                coord += num_bytes;
                continue;
            }

            for (int k = 0; k < num_bytes; k++){
                *coord = xpm.bytes[a++];
                coord++;
            }
            
        }
        coord += (vmi.XResolution - xpm.width) * num_bytes;
    }
    
    return 0;
}

int (draw_xpm_colored)(xpm_image_t xpm, uint16_t x, uint16_t y, uint32_t color) {
    int num_bytes = ((vmi.BitsPerPixel + 7) / 8);
    uint8_t *coord = ((uint8_t*)video_mem) + (y * vmi.XResolution + x) * num_bytes;

    uint8_t red = (color >> 16) & 0xFF;
    uint8_t green = (color >> 8) & 0xFF;
    uint8_t blue = color & 0xFF;

    int a = 0;
    for (int i=0; i < xpm.height; i++) {
        for (int j=0; j < xpm.width; j++) {
            if (xpm.bytes[a] == 0x40 && xpm.bytes[a + 1] == 0xb1 && xpm.bytes[a + 2] == 0x00) {
                a += num_bytes;
                coord += num_bytes;
                continue;
            }

            coord[0] = red;
            coord[1] = green;
            coord[2] = blue;

            a += num_bytes;
            coord += num_bytes;
        }
        coord += (vmi.XResolution - xpm.width) * num_bytes;
    }

    return 0;
}


