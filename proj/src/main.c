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

extern struct packet packet_struct; // From mouse.c
extern uint8_t packet_bytes[3];
extern int packet_index;
extern bool read_error_flag;

/**
 * @brief Entry point of the program. Initializes LCF and delegates control to the appropriate function.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return 0 on success, 1 on failure.
 */
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

/**
 * @brief Initializes hardware and graphics for the game.
 *
 * Sets up timer, keyboard, and mouse interrupts, resets and enables the mouse, and sets the video mode.
 *
 * @return 0 on success, 1 on failure.
 */
int (proj_init)(){
    if(timer_set_frequency(0, 60))return 1;
    printf("subscribing keyboard interrupts\n");
    if(keyboard_subscribe_int(&kbd_arq_set))return 1;
    printf("subscribing timer interrupts\n");
    if(timer_subscribe_int(&timer_arq_set))return 1;
    printf("subscribing mouse interrupts\n");
    if(mouse_subscribe_int(&mouse_arq_set))return 1;
    
    printf("resetting mouse\n");
    mouse_reset(); 
    printf("enabling mouse data reporting\n");
    if (mouse_write_register(MOUSE_ENABLE_DATA_REPORTING) != 0) {
        printf("Failed to enable mouse data reporting\n");
        return 1;
    }
    packet_index = 0;
    read_error_flag = false;
    mouse_x = vmi.XResolution / 2; 
    mouse_y = vmi.YResolution / 2;

    printf("setting graphics mode\n");
    if(set_graphics_mode(0x115)) return 1;

    printf("loading assets for menu...\n");
    if(loadAssets()) return 1;  

    return 0;
}

/**
 * @brief Displays the main menu of the game and handles user interaction.
 *
 * Handles mouse and keyboard input to allow the user to select between starting the game or exiting. 
 * Draws UI elements and updates the screen.
 *
 * @return PLAYING if the user starts the game, EXIT if the user exits.
 */
int (proj_menu)(){
    int selected = 0;
    printf("Starting menu with mouse support\n");
    int ipc_status;
    message msg;
    int r;
    bool need_redraw = true;
    int play_button_x = vmi.XResolution / 2 - 75;
    int play_button_y = vmi.YResolution / 2;
    int exit_button_x = vmi.XResolution / 2 - 75;
    int exit_button_y = vmi.YResolution / 2 + 40;

    while (1) {
        if (need_redraw) {
            bool mouse_over_play = is_point_in_rect(mouse_x, mouse_y, play_button_x, play_button_y, 150, 25);
            bool mouse_over_exit = is_point_in_rect(mouse_x, mouse_y, exit_button_x, exit_button_y, 150, 25);
            if (mouse_over_play) selected = 0;
            else if (mouse_over_exit) selected = 1;
            vg_draw_rectangle(play_button_x, play_button_y, 150, 25, 
                             (selected == 0 || mouse_over_play) ? 0xFF0000 : 0xFFFFFF); 
            vg_draw_rectangle(exit_button_x, exit_button_y, 150, 25, 
                             (selected == 1 || mouse_over_exit) ? 0xFF0000 : 0xFFFFFF); 

            draw_text("PACKMAN", vmi.XResolution / 2 - 30, vmi.YResolution / 2 - 30, 0xFFFF00);
            draw_text("PLAY (ENTER)", play_button_x + 10, play_button_y + 6, 0x000000);
            draw_text("EXIT (ESC)", exit_button_x + 10, exit_button_y + 6, 0x000000);
            draw_mouse_cursor(mouse_x, mouse_y);
            if (refresh_screen()) {
                printf("refresh_screen failed\n");
                return 4;
            }
            
            need_redraw = false; 
        }
        if ((r = driver_receive(ANY, &msg, &ipc_status)) != 0) { 
            printf("driver_receive failed with: %d\n", r);
            continue;
        }

        scancode = 0;

        if (is_ipc_notify(ipc_status)) {
            switch (_ENDPOINT_P(msg.m_source)) {
                case HARDWARE:
                    if (msg.m_notify.interrupts & BIT(kbd_arq_set)) {
                        kbc_ih();
                        if (verify_status()) {
                            if (scancode == ESC_MAKE_CODE) return EXIT;
                            if (scancode == W_MAKE_CODE || scancode == S_MAKE_CODE) {
                                selected = (selected + 1) % 2;
                                need_redraw = true;
                            }
                            if (scancode == ENTER_MAKE_CODE)
                                return selected == 0 ? PLAYING : EXIT;
                            printf("scancode: %02x\n", scancode);
                        }
                    }
                    if (msg.m_notify.interrupts & BIT(mouse_arq_set)) {
                        mouse_ih(); 
                        if (!read_error_flag) {
                            mouse_synch_packet();
                            if (packet_index == 0) {
                                mouse_build_packet();
                                if (!packet_struct.x_ov && !packet_struct.y_ov) {
                                    int old_mouse_x = mouse_x;
                                    int old_mouse_y = mouse_y;
                                    int delta_x = packet_struct.delta_x / 6; 
                                    int delta_y = packet_struct.delta_y / 6;
                                    if (abs(delta_x) > 0 || abs(delta_y) > 0) {
                                        mouse_x += delta_x;
                                        mouse_y -= delta_y; 
                                        if (mouse_x < 0) mouse_x = 0;
                                        if (mouse_y < 0) mouse_y = 0;
                                        if (mouse_x >= vmi.XResolution - 10) mouse_x = vmi.XResolution - 11;
                                        if (mouse_y >= vmi.YResolution - 10) mouse_y = vmi.YResolution - 11;
                                        if (old_mouse_x != mouse_x || old_mouse_y != mouse_y) {
                                            need_redraw = true;
                                        }
                                    }
                                    if (packet_struct.lb) {
                                        printf("Mouse left click at (%d, %d)\n", mouse_x, mouse_y);
                                        if (is_point_in_rect(mouse_x, mouse_y, play_button_x, play_button_y, 150, 25)) {
                                            printf("Play button clicked!\n");
                                            return PLAYING;
                                        }
                                        else if (is_point_in_rect(mouse_x, mouse_y, exit_button_x, exit_button_y, 150, 25)) {
                                            printf("Exit button clicked!\n");
                                            return EXIT;
                                        }
                                    }
                                }
                            }
                        } else {
                            read_error_flag = false;
                            packet_index = 0;
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

/**
 * @brief Placeholder for the main game loop.
 *
 * @return Game state to transition to after playing.
 */
int (proj_play)(){
    return 0;
}

/**
 * @brief Cleans up resources before exiting the program.
 *
 * Disables mouse data reporting, unsubscribes interrupts, and exits graphics mode.
 *
 * @return 0 on success, 1 on failure.
 */
int (proj_end)(){
    printf("disabling mouse data reporting\n");
    mouse_write_register(MOUSE_DISABLE_DATA_REPORTING);
    
    if(mouse_unsubscribe_int()) return 1;
    if(keyboard_unsubscribe_int()) return 1;
    if(timer_unsubscribe_int()) return 1;
    if(exit_graphics_mode()) return 1;

    return 0;
}

/**
 * @brief Controls the flow of the game using a state machine.
 *
 * Initializes the game, then loops through game states such as MENU, PLAYING, and EXIT, calling the appropriate function for each.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return 0 on success, 1 on failure.
 */
int(proj_main_loop)(int argc, char* argv[]) { 

    game_state state = MENU;

    if (proj_init()) return 1;

    printf("Starting Pacman Game with Mouse Support\n");

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
