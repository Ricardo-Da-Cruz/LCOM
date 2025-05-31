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

#define MAX_SCORES 999

uint8_t kbd_arq_set = 0;
uint8_t timer_arq_set = 1;

static game_state menu_return_state = MENU;// menu handling

void start_game(void);
void score_board(void);
void exit_game(void);
void back_to_menu(void);
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
    

    packet_index = 0;
    read_error_flag = false;

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

//score board
int proj_score_board() {
    Menu *m = newMenu("Score Board");
    // Step 1: Read scores from file
    FILE *file = fopen("/home/lcom/labs/proj/src/score.txt", "r");  // Adjust path as needed
    if (!file) {
        menuAddFunction(m, "Failed to open score.txt", NULL);
    } else {
        int scores[MAX_SCORES];
        int count = 0;

        while (count < MAX_SCORES && fscanf(file, "%d", &scores[count]) == 1) {
            count++;
            printf("opened %i", count);
        }
        fclose(file);

        // Step 2: Sort scores descending
        for (int i = 0; i < count - 1; i++) {
            for (int j = i + 1; j < count; j++) {
                if (scores[j] > scores[i]) {
                    int temp = scores[i];
                    scores[i] = scores[j];
                    scores[j] = temp;
                }
            }
        }

        // Step 3: Show top 3
        char buffer[50];
        for (int i = 0; i < count && i < 3; i++) {
            snprintf(buffer, sizeof(buffer), "Top %d: %d", i + 1, scores[i]);
            menuAddFunction(m, buffer, NULL);  // No function on selection
        }
    }

    // Step 4: Add a return or exit option
    menuAddFunction(m, "Back", back_to_menu);

    menu_return_state = MENU;
    
    menuPost(m);
    menuDelete(m);

    return menu_return_state;
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
                state = proj_score_board();
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
    menu_return_state = SCORE;
}

void exit_game() {
    menu_return_state = EXIT;
}

void back_to_menu(){
    menu_return_state = MENU;
}

