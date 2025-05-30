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

// Mouse variables
int mouse_x = 320, mouse_y = 240; // Initial cursor position
extern struct packet packet_struct; // From mouse.c
extern uint8_t packet_bytes[3];
extern int packet_index;
extern bool read_error_flag;

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

    // Subscribe mouse interrupts
    printf("subscribing mouse interrupts\n");
    if(mouse_subscribe_int(&mouse_arq_set))return 1;
    
    // Enable mouse data reporting
    printf("enabling mouse data reporting\n");
    if(mouse_write_register(MOUSE_ENABLE_DATA_REPORTING) != 0) {
        printf("Failed to enable mouse data reporting\n");
        return 1;
    }

    printf("setting graphics mode\n");
    if(set_graphics_mode(0x115)) return 1;

    printf("loading assets for menu...\n");
    if(loadAssets()) return 1;  

    return 0;
}

int (proj_end)(){

    // Disable mouse data reporting before cleanup
    printf("disabling mouse data reporting\n");
    mouse_write_register(MOUSE_DISABLE_DATA_REPORTING);
    
    if(mouse_unsubscribe_int()) return 1;
    if(keyboard_unsubscribe_int()) return 1;
    if(timer_unsubscribe_int()) return 1;
    if(exit_graphics_mode()) return 1;

    return 0;
}

void draw_mouse_cursor(int x, int y) {
    // Draw a simple arrow cursor
    vg_draw_rectangle(x, y, 2, 10, 0xFFFFFF);     // Vertical line
    vg_draw_rectangle(x, y, 6, 2, 0xFFFFFF);      // Horizontal top
    vg_draw_rectangle(x, y + 3, 4, 2, 0xFFFFFF);  // Middle line
    vg_draw_rectangle(x, y + 6, 3, 2, 0xFFFFFF);  // Lower line
}

bool is_point_in_rect(int px, int py, int rx, int ry, int width, int height) {
    return (px >= rx && px <= rx + width && py >= ry && py <= ry + height);
}

int (proj_menu)(){
    int selected = 0;
    
    printf("Starting menu with mouse support\n");

    int ipc_status;
    message msg;
    int r;

    while (1) {
        // Clear screen (optional, depends on your implementation)
        // vg_clear_screen(); // if you have this function
        
        // Draw menu UI - highlight based on mouse position or keyboard selection
        int play_button_x = vmi.XResolution / 2 - 75;
        int play_button_y = vmi.YResolution / 2;
        int exit_button_x = vmi.XResolution / 2 - 75;
        int exit_button_y = vmi.YResolution / 2 + 40;
        
        // Check if mouse is over buttons
        bool mouse_over_play = is_point_in_rect(mouse_x, mouse_y, play_button_x, play_button_y, 150, 25);
        bool mouse_over_exit = is_point_in_rect(mouse_x, mouse_y, exit_button_x, exit_button_y, 150, 25);
        
        // Update selected based on mouse position
        if (mouse_over_play) selected = 0;
        else if (mouse_over_exit) selected = 1;
        
        // Draw buttons
        vg_draw_rectangle(play_button_x, play_button_y, 150, 25, 
                         (selected == 0 || mouse_over_play) ? 0xFF0000 : 0xFFFFFF); 
        vg_draw_rectangle(exit_button_x, exit_button_y, 150, 25, 
                         (selected == 1 || mouse_over_exit) ? 0xFF0000 : 0xFFFFFF); 

        draw_text("PACKMAN", vmi.XResolution / 2 - 30, vmi.YResolution / 2 - 30, 0xFFFF00);

        draw_text("PLAY (ENTER)", play_button_x + 10, play_button_y + 6, 0x000000);
        draw_text("EXIT (ESC)", exit_button_x + 10, exit_button_y + 6, 0x000000);

        // Draw mouse cursor
        draw_mouse_cursor(mouse_x, mouse_y);

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
                    // Handle keyboard interrupts
                    if (msg.m_notify.interrupts & BIT(kbd_arq_set)) {
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
                    
                    // Handle mouse interrupts
                    if (msg.m_notify.interrupts & BIT(mouse_arq_set)) {
                        mouse_ih(); // Call mouse interrupt handler from mouse.c
                        
                        if (!read_error_flag) {
                            mouse_synch_packet(); // Synchronize packet from mouse.c
                            
                            // Check if we have a complete packet (3 bytes)
                            if (packet_index == 3) {
                                packet_index = 0; // Reset for next packet
                                
                                // Build the packet structure
                                mouse_build_packet();
                                
                                // Update mouse position
                                mouse_x += packet_struct.delta_x;
                                mouse_y -= packet_struct.delta_y; // Invert Y (screen coordinates)
                                
                                // Keep mouse within screen bounds
                                if (mouse_x < 0) mouse_x = 0;
                                if (mouse_y < 0) mouse_y = 0;
                                if (mouse_x >= vmi.XResolution - 10) mouse_x = vmi.XResolution - 11;
                                if (mouse_y >= vmi.YResolution - 10) mouse_y = vmi.YResolution - 11;
                                
                                // Handle mouse clicks
                                if (packet_struct.lb) { // Left button clicked
                                    printf("Mouse left click at (%d, %d)\n", mouse_x, mouse_y);
                                    
                                    // Check if click is on play button
                                    if (is_point_in_rect(mouse_x, mouse_y, play_button_x, play_button_y, 150, 25)) {
                                        printf("Play button clicked!\n");
                                        return PLAYING;
                                    }
                                    // Check if click is on exit button
                                    else if (is_point_in_rect(mouse_x, mouse_y, exit_button_x, exit_button_y, 150, 25)) {
                                        printf("Exit button clicked!\n");
                                        return EXIT;
                                    }
                                }
                                
                                if (packet_struct.rb) { // Right button clicked
                                    printf("Mouse right click at (%d, %d)\n", mouse_x, mouse_y);
                                }
                                
                                if (packet_struct.mb) { // Middle button clicked
                                    printf("Mouse middle click at (%d, %d)\n", mouse_x, mouse_y);
                                }
                                
                                // Debug: print packet info
                                printf("Mouse: pos(%d,%d) delta(%d,%d) buttons(L:%d M:%d R:%d)\n", 
                                       mouse_x, mouse_y, packet_struct.delta_x, packet_struct.delta_y,
                                       packet_struct.lb, packet_struct.mb, packet_struct.rb);
                            }
                        } else {
                            read_error_flag = false; // Reset error flag
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
    // You can add mouse support to the game here as well
    // Similar to the menu implementation
    return 0;
}

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
