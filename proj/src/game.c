
#include <lcom/lcf.h>

#include "devices/keyboard.h"
#include "devices/timer.h"
#include "devices/gpu.h"
#include "devices/i8042.h"

#include "game.h"

#include "sprites/blinky.h"
#include "sprites/clyde.h"
#include "sprites/eyes.h"
#include "sprites/inky.h"
#include "sprites/pacman.h"
#include "sprites/pacman2.h"
#include "sprites/pinky.h"
#include "sprites/pacman_death_anim.h"
#include "sprites/ghost.h"

xpm_image_t pacman_xpm[4];
xpm_image_t pacman2_xpm[4];
xpm_image_t blinky_xpm[8];
xpm_image_t clyde_xpm[8];
xpm_image_t inky_xpm[8];
xpm_image_t pinky_xpm[8];
xpm_image_t eyes_xpm[8];
xpm_image_t death_anim_xpm[12];
xpm_image_t ghost_xpm[4];

enum ghost_indexes {
    right_1,
    right_2,
    left_1,
    left_2,
    up_1,
    up_2,
    down_1,
    down_2,
};

enum pacman_indexes {
    pacman_right,
    pacman_left,
    pacman_up,
    pacman_down,
};

int loadAssets(){
    for(int i = 0; i < 4; i++){
        xpm_load(pacman[i], XPM_8_8_8, &pacman_xpm[i]);
        xpm_load(pacman2[i], XPM_8_8_8, &pacman2_xpm[i]);
        xpm_load(eyes[i], XPM_8_8_8, &eyes_xpm[i]);
        xpm_load(ghost[i], XPM_8_8_8, &ghost_xpm[i]);
    }

    for(int i = 0; i < 8; i++){
        xpm_load(blinky[i], XPM_8_8_8, &blinky_xpm[i]);
        xpm_load(clyde[i], XPM_8_8_8, &clyde_xpm[i]);
        xpm_load(inky[i], XPM_8_8_8, &inky_xpm[i]);
        xpm_load(pinky[i], XPM_8_8_8, &pinky_xpm[i]);
    }

    for(int i = 0; i < 12; i++){
        xpm_load(death_anim[i], XPM_8_8_8, &death_anim_xpm[i]);
    }

    return 0;
}

void draw_maze(){
    printf("drawing maze\n");
}

int game(){
    printf("loading assets\n");

    loadAssets();

    printf("drawing assets\n");

    for (int i = 0; i < 4; i++)
    {
        vg_draw_rectangle(0, 0, 16, 16, 0x000000);
        draw_xpm(pacman_xpm[i], i * 16, 0);
        draw_xpm(pacman2_xpm[i], i * 16,16);
        draw_xpm(eyes_xpm[i], i * 16, 32);
        draw_xpm(ghost_xpm[i], i * 16, 48);
    }

    for (int i = 0; i < 8; i++)
    {
        draw_xpm(blinky_xpm[i], i * 16, 64);
        draw_xpm(clyde_xpm[i], i * 16, 80);
        draw_xpm(inky_xpm[i], i * 16, 96);
        draw_xpm(pinky_xpm[i], i * 16, 112);
    }

    for (int i = 0; i < 12; i++)
    {
        draw_xpm(death_anim_xpm[i], i * 16, 128);
    }

    printf("waiting for ESC key\n");

    int ipc_status;
    message msg;
    int r;
    int micros = 0;
    int state = 0;


    while(1) {
        if ( (r = driver_receive(ANY, &msg, &ipc_status)) != 0) { 
            printf("driver_receive failed with: %d", r);
            continue;
        }

          scancode = 0;
      
          if (is_ipc_notify(ipc_status)) { /* received notification */
            switch (_ENDPOINT_P(msg.m_source)) {
                case HARDWARE: /* hardware interrupt notification */				
                    if (msg.m_notify.interrupts & 1 << 0) { /* subscribed interrupt */
                        kbc_ih();
                        if (verify_status()){
                            printf("scancode: %02x\n", scancode);
                            if(scancode == ESC_MAKE_CODE) return 7;
                        }
                    }
                    if (msg.m_notify.interrupts & 1 << 1) { /* subscribed interrupt */
                        micros = (micros + 1) % 15;
                        if(micros == 0){
                            state = (state + 1) % 4;
                            vg_draw_rectangle(0, 144, 16, 16, 0x000000);
                            draw_xpm(ghost_xpm[state],0 , 144);
                            if(refresh_screen()){
                                printf("refresh_screen failed\n");
                                return 4;
                            }
                        }
                    }
                    break;
                default:
                    break; /* no other notifications expected: do nothing */	
            }
        }
    }

    return 0;
}
