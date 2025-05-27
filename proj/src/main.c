#include <lcom/lcf.h>
#include <lcom/proj.h>
#include <stdint.h>
#include <stdio.h>

#include "devices/keyboard.h"
#include "devices/timer.h"
#include "devices/gpu.h"
#include "devices/i8042.h"

#include "game.h"

typedef enum {
    MENU,
    PLAYING,
    LOST,
    WON,
    EXIT,
}game_state;

uint8_t kbd_arq_set = 0;
uint8_t timer_arq_set = 1;

int main(int argc, char *argv[]) {
    // sets the language of LCF messages (can be either EN-US or PT-PT)
    lcf_set_language("EN-US");
  
    // enables to log function invocations that are being "wrapped" by LCF
    // [comment this out if you don't want/need it]
    lcf_trace_calls("/home/lcom/labs/proj/src/trace.txt");
  
    // enables to save the output of printf function calls on a file
    // [comment this out if you don't want/need it]
    lcf_log_output("/home/lcom/labs/proj/src/output.txt");
  
    // handles control over to LCF
    // [LCF handles command line arguments and invokes the right function]
    if (lcf_start(argc, argv))
      return 1;
  
    // LCF clean up tasks
    // [must be the last statement before return]
    lcf_cleanup();
  
    return 0;
}

int (proj_init)(){

    if(timer_set_frequency(0, 60))return 1;
    printf("subscribing keyboard interrupts\n");
    if(keyboard_subscribe_int(&kbd_arq_set))return 1;
    printf("subscribing timer interrupts\n");
    if(timer_subscribe_int(&timer_arq_set))return 1;
    printf("setting graphics mode\n");
    if(set_graphics_mode(0x115)) return 1;

    printf("loading assets for menu...\n");
    if(loadAssets()) return 1;  


    return 0;
}

int (proj_end)(){
    if(keyboard_unsubscribe_int()) return 1;
    if(timer_unsubscribe_int()) return 1;
    if(exit_graphics_mode()) return 1;

    return 0;
}

int (proj_menu)(){
    vg_draw_rectangle(vmi.XResolution / 2 - 75, vmi.YResolution / 2, 150, 25, 0xFF0000);
    vg_draw_rectangle(vmi.XResolution / 2 - 75, vmi.YResolution / 2 + 40, 150, 25, 0xFFFFFF);

    if(refresh_screen()){
        return 4;
    }

    int ipc_status;
    message msg;
    int r;
    int selected = 0;

    printf("waiting for ESC key\n");

    while (1) {
        // draw menu UI
        vg_draw_rectangle(vmi.XResolution / 2 - 75, vmi.YResolution / 2, 150, 25, selected == 0 ? 0xFF0000 : 0xFFFFFF); 
        vg_draw_rectangle(vmi.XResolution / 2 - 75, vmi.YResolution / 2 + 40, 150, 25, selected == 1 ? 0xFF0000 : 0xFFFFFF); 

        draw_text("PACKMAN", vmi.XResolution / 2 - 30, vmi.YResolution / 2 - 30, 0xFFFF0);

        draw_text("PLAY (ENTER)", vmi.XResolution / 2 - 40, vmi.YResolution / 2 + 6, 0xFFFFF);
        draw_text("EXIT (ESC)", vmi.XResolution / 2 - 40, vmi.YResolution / 2 + 46, 0x00000);

        if (refresh_screen()) {
            printf("refresh_screen failed\n");
            return 4;
        }

        if ((r = driver_receive(ANY, &msg, &ipc_status)) != 0) { 
            printf("driver_receive failed with: %d\n", r);
            continue;
        }

        scancode = 0;

        if (is_ipc_notify(ipc_status)) {
            switch (_ENDPOINT_P(msg.m_source)) {
                case HARDWARE:
                    if (msg.m_notify.interrupts & kbd_arq_set) {
                        kbc_ih();
                        if (verify_status()) {
                            if (scancode == ESC_MAKE_CODE) return EXIT;
                            if (scancode == W_MAKE_CODE || scancode == S_MAKE_CODE)
                                selected = (selected + 1) % 2;
                            if (scancode == ENTER_MAKE_CODE)
                                return selected == 0 ? PLAYING : EXIT;
                            printf("scancode: %02x\n", scancode);
                        }
                    }
                    break;
                default:
                    break;
            }
        }
    }


    return EXIT;
}

int (proj_play)(){
    return 0;
}

int(proj_main_loop)(int argc, char* argv[]) { 

    game_state state = MENU;

    if (proj_init()) return 1;

    printf("Starting Pacman Game\n");

    while(state != EXIT) {
        printf("state: %d\n", state);
        switch (state) {
            case MENU:
                state = proj_menu();
                break;
            case PLAYING:
                state = game();
                break;
            case EXIT:
                break;
            default:
                printf("invalid state\n");
                proj_end();
                return 1;
        }
    }  

    printf("exiting graphics mode\n");

    if (proj_end()) return 1;

    return 0;
} 
