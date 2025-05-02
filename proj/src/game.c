
#include <lcom/lcf.h>

#include "devices/keyboard.h"
#include "devices/timer.h"
#include "devices/gpu.h"
#include "devices/i8042.h"
#include "devices/interrupts.h"

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
#include "sprites/letters.h"
#include "sprites/numbers.h"
#include "sprites/maze.h"

xpm_image_t ghost_xpm[4];
xpm_image_t pacman_xpm[8];
xpm_image_t blinky_xpm[8];
xpm_image_t clyde_xpm[8];
xpm_image_t inky_xpm[8];
xpm_image_t pinky_xpm[8];
xpm_image_t eyes_xpm[8];
xpm_image_t death_anim_xpm[12];
xpm_image_t letters_xpm[26];
xpm_image_t numbers_xpm[10];
xpm_image_t maze_xpm;

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

typedef enum {
    pacman_right,
    pacman_left,
    pacman_up,
    pacman_down,
}pacman_indexes;

typedef struct{
    int x;
    int y;
}coords;


enum ghost_status{
    normal,
    jailed,
    dead,
};

typedef struct{
    coords c;
    int direction;
    int time;
    enum ghost_status status;
}ghost;

int current_pellet_matrix[31][28];

uint8_t micros;
pacman_indexes direction;
int ghost_state;
int pacman_state;
bool energized;
int energized_time;
int num_pellets;

ghost pinky_g  = { {13, 12}, 0, 100, jailed};
ghost blinky_g = { {13, 12}, 0, 200, jailed};
ghost inky_g   = { {13, 12}, 0, 300, jailed};
ghost clyde_g  = { {13, 12}, 0, 400, jailed};

int maze_x;
int maze_y;

coords pacman_c = {13 * 8, 23 * 8};

int loadAssets(){
    for(int i = 0; i < 4; i++){
        xpm_load(pacman[i], XPM_8_8_8, &pacman_xpm[2 * i]);
        xpm_load(pacman2[i], XPM_8_8_8, &pacman_xpm[2 * i + 1]);
        xpm_load(eyes[i], XPM_8_8_8, &eyes_xpm[i]);
        xpm_load(ghosts[i], XPM_8_8_8, &ghost_xpm[i]);
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
    
    for(int i = 0; i < 26; i++){
        xpm_load(letters[i], XPM_8_8_8, &letters_xpm[i]);
    }

    for(int i = 0; i < 10; i++){
        xpm_load(numbers[i], XPM_8_8_8, &numbers_xpm[i]);
    }

    xpm_load(maze, XPM_8_8_8, &maze_xpm);

    for(int i = 0; i < 30; i++){
        for(int j = 0; j < 28; j++){
            if(pellet_matrix[i][j] == 1){ 
                vg_draw_rectangle_xpm(j * 8 + 6, i * 8 + 4, 2, 2, pellet_color, maze_xpm);
            }else if(pellet_matrix[i][j] == 2){
                vg_draw_rectangle_xpm(j * 8 + 5, i * 8 + 3, 4, 4, pellet_color, maze_xpm);
                printf("pellet at %d %d\n", j, i);
            }
        }
    }

    memcpy(current_pellet_matrix, pellet_matrix, sizeof(current_pellet_matrix));

    return 0;
}

void (game_logic)(){
    //move pacman and ghosts
    //check for collisions
    switch (direction){
        case pacman_right:
            //printf("right %d %d\n", pacman_c.y / 8, (pacman_c.x + 1) / 8);
            if (maze_matrix[pacman_c.y / 8][(pacman_c.x + 4 + 1) / 8] == 0) pacman_c.x++;
            break;
        case pacman_left:
            //printf("left %d %d\n", pacman_c.y / 8, (pacman_c.x - 1) / 8);
            //printf("left %d %d\n", pacman_c.y, pacman_c.x);
            if (maze_matrix[pacman_c.y / 8][(pacman_c.x - 4 - 1) / 8] == 0) pacman_c.x--;
            break;
        case pacman_up:
            //printf("up %d %d\n", (pacman_c.y - 1) / 8, pacman_c.x / 8);
            if (maze_matrix[(pacman_c.y - 4 - 1) / 8][pacman_c.x / 8] == 0) pacman_c.y--;
            break;
        case pacman_down:
            //printf("down %d %d\n", (pacman_c.y + 1) / 8, pacman_c.x / 8);
            if (maze_matrix[(pacman_c.y + 4 + 1) / 8][pacman_c.x / 8] == 0) pacman_c.y++;
            break;
    }

    //ghost pathfinding


    //check for pellet pickup
    //check for energizer pickup
    if(current_pellet_matrix[pacman_c.y / 8][pacman_c.x / 8] == 1){
        num_pellets--;
        vg_draw_rectangle_xpm(pacman_c.x, pacman_c.y, 8, 8, 0, maze_xpm);
    }else if(current_pellet_matrix[pacman_c.y / 8][pacman_c.x / 8] == 2){
        num_pellets--;
        vg_draw_rectangle_xpm(pacman_c.x, pacman_c.y, 8, 8, 0, maze_xpm);
        energized = true;
        energized_time = 200;
    }
    current_pellet_matrix[pacman_c.y / 8][pacman_c.x / 8] = 0;

    //check for ghost death
    //check for pacman death
    //check for energizer time out
    //check for ghost release

    if(energized){
        //ghosts are vulnerable
    }else{
        //pacman is vulnerable
    }

    //check for win condition
    if(num_pellets == 0){
        //win
    }

    //drawing
    draw_xpm(maze_xpm, maze_x, maze_y);
    draw_xpm(pacman_xpm[direction * 2 + pacman_state], maze_x + pacman_c.x, maze_y + pacman_c.y);

    if(pinky_g.status == dead){
        
    }

    if(energized){
    }

}

int game(){
    printf("loading assets\n");

    loadAssets();

    maze_x = vmi.XResolution / 2 - maze_xpm.width / 2;
    maze_y = vmi.YResolution / 2 - maze_xpm.height / 2;
    num_pellets = pellet_count;
    energized = false;
    micros = 0;
    direction = 0;
    ghost_state = 0;
    pacman_state = 0;

    printf("waiting for ESC key\n");

    while(1){
        uint64_t status = await_interrupt(1 << 0 | 1 << 1);

        if (status & 1 << 0) { /* subscribed interrupt */
            kbc_ih();
            if (verify_status()){
                printf("scancode: %02x\n", scancode);

                switch (scancode){
                    case W_MAKE_CODE:
                        direction = pacman_up;
                        break;
                    case A_MAKE_CODE:
                        direction = pacman_left;
                        break;
                    case S_MAKE_CODE:
                        direction = pacman_down;
                        break;
                    case D_MAKE_CODE:
                        direction = pacman_right;
                        break;
                    case ESC_MAKE_CODE:
                        return 7;
                }
            }
        }
        if (status & 1 << 1) { /* subscribed interrupt */
            micros++;
            if(micros % 7 == 0){
                ghost_state = (ghost_state + 1) % 4;
                pacman_state = (pacman_state + 1) % 2;

                game_logic();
                
                if(refresh_screen()){
                    printf("refresh_screen failed\n");
                    return 4;
                }
            }
        }
    }

    return 0;
}
