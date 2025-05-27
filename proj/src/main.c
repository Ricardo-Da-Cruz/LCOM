#include <lcom/lcf.h>
#include <lcom/proj.h>
#include <stdint.h>
#include <stdio.h>

#include "devices/keyboard.h"
#include "devices/timer.h"
#include "devices/gpu.h"
#include "devices/i8042.h"
#include "devices/mouse.h"

#include "game.h"


uint8_t scancode;

typedef enum {
    MENU,
    PLAYING,
    LOST,
    WON,
    EXIT,
}game_state;

uint8_t kbd_arq_set = 0;
uint8_t timer_arq_set = 1;
uint8_t mouse_arq_set = 2;
struct packet mouse_packet;
uint8_t mouse_bytes[3];
int mouse_byte_index = 0;

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

    printf("subscribing mouse interrupts\n");
    if(mouse_subscribe_int(&mouse_arq_set)) return 1;
    if(my_mouse_enable_data_reporting()) return 1;

    printf("setting graphics mode\n");
    if(set_graphics_mode(0x115)) return 1;

    printf("loading assets for menu...\n");
    if(loadAssets()) return 1;  


    return 0;
}

int (proj_end)(){
    if(mouse_disable_data_reporting()) return 1;
    if(mouse_unsubscribe_int()) return 1;

    if(keyboard_unsubscribe_int()) return 1;
    if(timer_unsubscribe_int()) return 1;
    if(exit_graphics_mode()) return 1;

    return 0;
}

void draw_mouse_cursor(int x, int y) {
    vg_draw_rectangle(x, y, 10, 10, 0x00FF00); // quadrado verde como cursor
}

int verify_status() {
    // Verificar o status do teclado e retornar um valor indicando se o status é válido
    // Exemplo: verificar se o bit de erro de paridade está definido
    if (mouse_status & KBC_PARITY_ERR) {
        return 0; // Status inválido
    }
    return 1; // Status válido
}

int (proj_menu)(){
    vg_draw_rectangle(vmi.XResolution / 2 - 75, vmi.YResolution / 2, 150, 25, 0xFF0000);
    vg_draw_rectangle(vmi.XResolution / 2 - 75, vmi.YResolution / 2 + 40, 150, 25, 0xFFFFFF);

    int mouse_x = 320, mouse_y = 240;

    draw_mouse_cursor(mouse_x, mouse_y);

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
                        
                        printf("scancode: 0x%02X\n", scancode);

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
                    if (msg.m_notify.interrupts & mouse_arq_set) {
                        mouse_ih();

                        if (mouse_byte_index == 0 && (mouse_scancode & BIT(3)) == 0) break;


                        mouse_bytes[mouse_byte_index] = mouse_scancode;
                        mouse_byte_index++;

                        if (mouse_byte_index == 3) {
                            mouse_byte_index = 0;
                            construct_packet(&mouse_packet, 0, mouse_bytes[0]);
                            construct_packet(&mouse_packet, 1, mouse_bytes[1]);
                            construct_packet(&mouse_packet, 2, mouse_bytes[2]);

                            // Aqui você pode usar mouse_packet.delta_x, delta_y, lb, etc
                            // Exemplo: desenhar o cursor (fictício)
                            static int mouse_x = 320, mouse_y = 240;
                            mouse_x += mouse_packet.delta_x;
                            mouse_y -= mouse_packet.delta_y;

                            if (mouse_x < 0) mouse_x = 0;
                            if (mouse_y < 0) mouse_y = 0;
                            if (mouse_x > vmi.XResolution - 10) mouse_x = vmi.XResolution - 10;
                            if (mouse_y > vmi.YResolution - 10) mouse_y = vmi.YResolution - 10;

                            draw_mouse_cursor(mouse_x, mouse_y); // função que desenha um cursor com XPM ou algo simples
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
