#include <lcom/lcf.h>
#include <lcom/proj.h>
#include <stdint.h>
#include <stdio.h>

#include "devices/keyboard.h"
#include "devices/timer.h"
#include "devices/gpu.h"
#include "devices/i8042.h"


#include "menu.h"
#include "game.h"

typedef enum {
    MENU,
    PLAYING,
    LOST,
    WON,
    EXIT,
    SCORE,
}game_state;

uint8_t kbd_arq_set = 0;
uint8_t timer_arq_set = 1;

static game_state menu_return_state = MENU;// menu handling

void start_game(void);
void score_board(void);
void exit_game(void);

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
    Menu *m = newMenu("Main Menu");
    menuAddFunction(m, "Start Game", start_game); 
    menuAddFunction(m, "Score Board", score_board); 
    menuAddFunction(m, "Exit", exit_game);       

    menu_return_state = MENU;

    // This handles drawing + input + selection
    menuPost(m);

    menuDelete(m);

    return menu_return_state;
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
                state = proj_menu();
                break;
            case SCORE:
                state = proj_menu();
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

// menue functions
void start_game() {
    menu_return_state = PLAYING;
}

void score_board() {
    printf("Showing scoreboard (TODO: implement)\n");
    tickdelay(micros_to_ticks(1000000)); // 1 second pause
}

void exit_game() {
    menu_return_state = EXIT;
}

