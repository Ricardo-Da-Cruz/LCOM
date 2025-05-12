
#include <lcom/lcf.h>
#include <limits.h>
#include <math.h>

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

xpm_image_t pacman_xpm[8];
xpm_image_t death_anim_xpm[12];

xpm_image_t energized_ghost_xpm[4];
xpm_image_t eyes_xpm[8];
xpm_image_t normal_ghost_xpm[4][8];

xpm_image_t maze_xpm;
xpm_image_t letters_xpm[26];
xpm_image_t numbers_xpm[10];

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

enum ghost_xpm_idx{
    blinky_idx,
    clyde_idx,
    inky_idx,
    pinky_idx,
};

typedef enum {
    right,
    left,
    up,
    down,
}direction_t;

typedef struct{
    int x;
    int y;
}coords;

enum ghost_status{
    normal,
    jailed, // the ghost is in the ghost house and is not free to move
    dead, // the ghost is dead and has to go back to the ghost house
};

typedef struct{
    coords c;
    int direction;
    int time;
    enum ghost_status status;
}ghost;

int current_pellet_matrix[31][28];

uint8_t micros;
direction_t direction;
int ghost_state;
int pacman_state;
int energized_time;
int num_pellets;

// for input buffering
int next_direction;
int next_direction_time;

int maze_x;
int maze_y;

ghost ghosts_state[4] = {
    { {13 * 8 + 4, 11 * 8},    up, 0, normal},
    { {11 * 8 + 4, 14 * 8 + 4},up, 100, jailed},
    { {13 * 8 + 4, 14 * 8 + 4},up, 200, jailed},
    { {15 * 8 + 4, 14 * 8 + 4},up, 300, jailed},
};

coords pacman_c = {13 * 8, 23 * 8};

int loadAssets(){
    for(int i = 0; i < 4; i++){
        xpm_load(pacman[i], XPM_8_8_8, &pacman_xpm[2 * i]);
        xpm_load(pacman2[i], XPM_8_8_8, &pacman_xpm[2 * i + 1]);
        xpm_load(eyes[i], XPM_8_8_8, &eyes_xpm[i]);
        xpm_load(ghosts[i], XPM_8_8_8, &energized_ghost_xpm[i]);
    }

    for(int i = 0; i < 8; i++){
        xpm_load(blinky[i], XPM_8_8_8, &normal_ghost_xpm[blinky_idx][i]);
        xpm_load(clyde[i], XPM_8_8_8, &normal_ghost_xpm[clyde_idx][i]);
        xpm_load(inky[i], XPM_8_8_8, &normal_ghost_xpm[inky_idx][i]);
        xpm_load(pinky[i], XPM_8_8_8, &normal_ghost_xpm[pinky_idx][i]);
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

int try_move(int direction){
    switch (direction){
        case right:
            if (maze_matrix[pacman_c.y / 8][(pacman_c.x + 8) / 8] == 0
                && maze_matrix[(pacman_c.y + 7) / 8][(pacman_c.x + 8) / 8] == 0){
                pacman_c.x++;
                return 1;
            }
            break;
        case left:
            if (maze_matrix[pacman_c.y / 8][(pacman_c.x - 1) / 8] == 0
                && maze_matrix[(pacman_c.y + 7) / 8][(pacman_c.x - 1) / 8] == 0){
                pacman_c.x--;
                return 1;
            } 
            break;
        case up:
            if (maze_matrix[(pacman_c.y - 1) / 8][(pacman_c.x + 7) / 8] == 0
                && maze_matrix[(pacman_c.y - 1) / 8][pacman_c.x / 8] == 0){
                pacman_c.y--;
                return 1;
            }
            break;
        case down:
            if (maze_matrix[(pacman_c.y + 8) / 8][(pacman_c.x + 7) / 8] == 0
                && maze_matrix[(pacman_c.y + 8) / 8][pacman_c.x / 8] == 0){
                pacman_c.y++;
                return 1;
            }
            break;
    }

    return 0;
}

void move_ghost(ghost *g){
    switch (g->direction){
        case right:
            g->c.x++;
            break;
        case left:
            g->c.x--;
            break;
        case up:
            g->c.y--;
            break;
        case down:
            g->c.y++;
            break;
    }
}

int distance(int x1, int y1, int x2, int y2){
    return (x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2);
}

// only works when the coords mod 8 == 0
void pathfind(ghost *g, int target_x, int target_y){
    //check all directions except the one the ghost came from
    int ghost_d = INT_MAX;

    // right
    if (g->direction != left){
        if (maze_matrix[g->c.y / 8][(g->c.x + 8) / 8] == 0){
            int d = distance(g->c.x + 8, g->c.y, target_x, target_y);
            if (d < distance){
                ghost_d = d;
                g->direction = right;
            }
        }
    }
    // left
    if (g->direction != right){
        if (maze_matrix[g->c.y / 8][(g->c.x - 1) / 8] == 0){
            int d = distance(g->c.x - 1, g->c.y, target_x, target_y);
            if (d < distance){
                ghost_d = d;
                g->direction = left;
            }
        }
    }
    // up
    if (g->direction != down){
        if (maze_matrix[(g->c.y - 1) / 8][(g->c.x + 7) / 8] == 0){
            int d = distance(g->c.x, g->c.y - 1, target_x, target_y);
            if (d < distance){
                ghost_d = d;
                g->direction = up;

            }
        }
    }
    // down
    if (g->direction != up){
        if (maze_matrix[(g->c.y + 8) / 8][(g->c.x + 7) / 8] == 0){
                int d = distance(g->c.x, g->c.y + 8, target_x, target_y);
                if (d < distance){
                    ghost_d = d;
                    g->direction = down;
                }
        }
    }
}

void scatter(ghost *g, int idx){
    switch (idx){
        case blinky_idx:
            //top right corner
            pathfind(g, 27 * 8 + 4, 4);

            break;
        case clyde_idx:
            //bottom left corner
            pathfind(g, 4, 30 * 8 + 4);
            break;
        case inky_idx:
            //bottom right corner
            pathfind(g, 27 * 8 + 4, 30 * 8 + 4);
            break;
        case pinky_idx:
            //top left corner
            pathfind(g, 4, 4);
            break;
    }
    move_ghost(g);
}

void chase(ghost *g, int idx){
    switch (idx){
        case blinky_idx:
            //Blinky wants to move to pacman
            pathfind(&ghosts_state[idx], pacman_c.x, pacman_c.y);
            break;
        case clyde_idx:
            //Clyde wants to move to pacman if he is far away, otherwise he wants to move to the ghost house
            if (distance(ghosts_state[idx].c.x, ghosts_state[idx].c.y, pacman_c.x, pacman_c.y) > 64)
                pathfind(&ghosts_state[idx], pacman_c.x, pacman_c.y);
            else
                pathfind(&ghosts_state[idx], 13 * 8 + 4, 14 * 8 + 4);
            break;
        case inky_idx:
            //Inky wants to move to the reflection of pacman through blinky
            coords target = {pacman_c.x - (ghosts_state[blinky_idx].c.x - pacman_c.x), pacman_c.y - (ghosts_state[blinky_idx].c.y - pacman_c.y)};
            pathfind(&ghosts_state[idx], target.x, target.y);
            break;
        case pinky_idx:
            //Pinky wants to move to a position 4 blocks in front of pacman
            coords target = {pacman_c.x, pacman_c.y};
            if (direction == right) target.x += 32;
            else if (direction == left) target.x -= 32;
            else if (direction == up) target.y -= 32;
            else if (direction == down) target.y += 32;
            pathfind(&ghosts_state[idx], target.x, target.y);
            break;
    }
}



void (game_logic)(){
    //move pacman
    //check for collisions
    //needs input buffering to prevent the need of pixel perfect movement
    if(next_direction_time != 0){
        if(try_move(next_direction)){
            next_direction_time = 0;
            direction = next_direction;
        }else{
            try_move(direction);
            next_direction_time--;
        }
    }else{
        try_move(direction);
    }

    //check for pellet pickup and energizer pickup
    if(current_pellet_matrix[pacman_c.y / 8][pacman_c.x / 8] == 1){
        num_pellets--;
        //remove pellet from maze xpm
        vg_draw_rectangle_xpm(pacman_c.x - pacman_c.x % 8, pacman_c.y - pacman_c.y % 8, 8, 8, 0, maze_xpm);
    }else if(current_pellet_matrix[pacman_c.y / 8][pacman_c.x / 8] == 2){
        num_pellets--;
        //remove pellet from maze xpm
        vg_draw_rectangle_xpm(pacman_c.x - pacman_c.x % 8 , pacman_c.y - pacman_c.y % 8, 8, 8, 0xFFFFFF, maze_xpm);
        energized_time = 200;
    }
    current_pellet_matrix[pacman_c.y / 8][pacman_c.x / 8] = 0;

    //ghosts have two modes scatter and chase
    //in scatter mode they move to a predefined position
    //pathfind doesn't need to be optimal, just minimize the linear distance to the target
    //only run pathfinding when the ghost reaches a crossroad that is marked by the target attribute
    //ghost can't make 180 degree turns unless they get energized

    /*
        | Cycle # | Scatter Duration | Chase Duration                   |
        | ------- | ---------------- | -------------------------------- |
        | 1       | 7 seconds        | 20 seconds                       |
        | 2       | 7 seconds        | 20 seconds                       |
        | 3       | 5 seconds        | 20 seconds                       |
        | 4       | 5 seconds        | until Pac-Man dies or level ends |
    */
    for(int i = 0; i < 4; i++){
        if(ghosts_state[i].status == normal){
            //ghost pathfinding
            //move ghost
            if (energized_time == 0){
                energized_time--;
                if(ghosts_state[i].c.x % 8 == 0 && ghosts_state[i].c.y % 8 == 0){
                    chase(&ghosts_state[i], i);
                }
                move_ghost(&ghosts_state[i]);
                
            }else{
                //move ghost away from pacman
            }
        }else if(ghosts_state[i].status == dead){
            //move ghost to ghost house
            if(ghosts_state[i].c.x == 13 * 8 + 4 && ghosts_state[i].c.y == 11 * 8){
                ghosts_state[i].status = jailed;
            }else{
                if(ghosts_state[i].c.x % 8 == 0 && ghosts_state[i].c.y % 8 == 0){
                    pathfind(&ghosts_state[i], 13 * 8 + 4, 11 * 8);
                }
                move_ghost(&ghosts_state[i]);
            }
        }else if(ghosts_state[i].status == jailed){
            //keep ghost in ghost house
            if(ghosts_state[i].time != 0){
                ghosts_state[i].time--;
            }else{
                ghosts_state[i].status = normal;
            }
        }
    }

    //check for ghost death
    //check for pacman death

    //check for win condition
    if (energized_time != 0) energized_time--; 
    if(num_pellets == 0){
        //win
    }
}

void draw(){
    draw_xpm(maze_xpm, maze_x, maze_y);
    draw_xpm(pacman_xpm[direction * 2 + pacman_state], maze_x + pacman_c.x, maze_y + pacman_c.y);

    for(int i = 0; i < 4; i++){
        if(ghosts_state[i].status == normal){
            if (energized_time != 0){
                draw_xpm(energized_ghost_xpm[ghost_state], maze_x + ghosts_state[i].c.x, maze_y + ghosts_state[i].c.y);
            }else{
                draw_xpm(normal_ghost_xpm[i][ghost_state], maze_x + ghosts_state[i].c.x, maze_y + ghosts_state[i].c.y);
            }
        }else if(ghosts_state[i].status == dead){
            draw_xpm(eyes_xpm[ghost_state], maze_x + ghosts_state[i].c.x, maze_y + ghosts_state[i].c.y);
        } else if(ghosts_state[i].status == jailed){
            draw_xpm(normal_ghost_xpm[i][ghost_state], maze_x + ghosts_state[i].c.x, maze_y + ghosts_state[i].c.y);
        }
    }
}

int game(){
    printf("loading assets\n");

    loadAssets();

    maze_x = vmi.XResolution / 2 - maze_xpm.width / 2;
    maze_y = vmi.YResolution / 2 - maze_xpm.height / 2;
    num_pellets = pellet_count;
    micros = 0;
    direction = 0;
    next_direction_time = 0;
    ghost_state = 0;
    pacman_state = 0;

    printf("waiting for ESC key\n");

    while(1){
        uint64_t status = await_interrupt(1 << 0 | 1 << 1);

        if (status & 1 << 0) { /* subscribed interrupt */
            kbc_ih();
            if (verify_status()){
                switch (scancode){
                    case W_MAKE_CODE:
                        next_direction = up;
                        next_direction_time = 4;
                        break;
                    case A_MAKE_CODE:
                        next_direction = left;
                        next_direction_time = 4;
                        break;
                    case S_MAKE_CODE:
                        next_direction = down;
                        next_direction_time = 4;
                        break;
                    case D_MAKE_CODE:
                        next_direction = right;
                        next_direction_time = 4;
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

                draw();
                
                if(refresh_screen()){
                    printf("refresh_screen failed\n");
                    return 4;
                }
            }
        }
    }

    return 0;
}
