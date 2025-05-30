#include "sprites/maze.h"
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
#include "sprites/score.h"

#include "ghosti.h"

xpm_image_t pacman_xpm[8];
xpm_image_t death_anim_xpm[12];

xpm_image_t energized_ghost_xpm[4];
xpm_image_t eyes_xpm[8];
xpm_image_t normal_ghost_xpm[4][8];

xpm_image_t scoree[4]; 

xpm_image_t maze_xpm;
xpm_image_t letters_xpm[29];
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

typedef enum {
    playing,
    lost,
    respawn,
    won,
}game_state;

game_state state;

int current_pellet_matrix[31][28];
int num_pellets;

int next_direction;
int next_direction_time;

int maze_x;
int maze_y;


int pacman_lives;

bool game_paused;

int display_score_timer;
// LUGAR DA PONTUAÇÂO !!!
int score_display_x, score_display_y;
int score_display_index;
int score;
int bonus_multiplier; 



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
    
    for(int i = 0; i < 29; i++){
        xpm_load(letters[i], XPM_8_8_8, &letters_xpm[i]);
    }

    for(int i = 0; i < 10; i++){
        xpm_load(numbers[i], XPM_8_8_8, &numbers_xpm[i]);
    }

    
    for(int i = 0; i < 4; i++){
        xpm_load(scori[i], XPM_8_8_8, &scoree[i]);
    }
    
    xpm_load(maze, XPM_8_8_8, &maze_xpm);

    for(int i = 0; i < 30; i++){
        for(int j = 0; j < 29; j++){
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
            if (pacman_c.x >= 27 * 8) {
                pacman_c.x = 0;
                return 1;
            }

            if (maze_matrix[pacman_c.y / 8][(pacman_c.x + 8) / 8] == 0
                && maze_matrix[(pacman_c.y + 7) / 8][(pacman_c.x + 8) / 8] == 0){
                pacman_c.x++;
                return 1;
            }
            break;
        case left:
            if (pacman_c.x <= 0) {
                pacman_c.x = 27 * 8;
                return 1;
            }
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

void (update_pacman)(){
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
}

void (check_collisions)(){
    for(int i = 0; i < 4; i++){
        if(ghosts_state[i].status == normal){
            if(distance(ghosts_state[i].c.x, ghosts_state[i].c.y, pacman_c.x, pacman_c.y) < 8 * 8){
                if (energized_time == 0){
                    state = lost;
                    pacman_lives--;
                    micros = 0;
                }else{
                    int ghost_score = 200 * bonus_multiplier;
                    score += ghost_score;
                    bonus_multiplier *= 2;
                    if(bonus_multiplier > 8) bonus_multiplier = 8; 

                    ghosts_state[i].status = dead;
                    int ghost_eaten_x = ghosts_state[i].c.x;
                    int ghost_eaten_y = ghosts_state[i].c.y;

                    printf("Antes de display_ghost_score: game_paused = %d\n", game_paused);
                    display_ghost_score(ghost_eaten_x, ghost_eaten_y, ghost_score);
                    printf("Depois de display_ghost_score: game_paused = %d\n", game_paused);

                }
            }
        }
    }
}

void display_ghost_score(int x, int y, int score) {
    game_paused = true;
    display_score_timer = 60; 
    
    score_display_x = x;
    score_display_y = y;

    switch(score) {
        case 200:
            score_display_index = 0;
            break;
        case 400:
            score_display_index = 1;
            break;
        case 800:
            score_display_index = 2;
            break;
        case 1600:
            score_display_index = 3;
            break;
        default:
            score_display_index = 0; 
            break;
    }
}

void (check_pellets)(){
    if (energized_time != 0) energized_time--;

    if(current_pellet_matrix[pacman_c.y / 8][pacman_c.x / 8] == 1){
        num_pellets--;
        score += 10; 
        vg_draw_rectangle_xpm(pacman_c.x - pacman_c.x % 8, pacman_c.y - pacman_c.y % 8, 8, 8, 0, maze_xpm);
    }else if(current_pellet_matrix[pacman_c.y / 8][pacman_c.x / 8] == 2){
        num_pellets--;
        score += 50; 
        energized_time = 700;
        bonus_multiplier = 1;
        vg_draw_rectangle_xpm(pacman_c.x - pacman_c.x % 8 , pacman_c.y - pacman_c.y % 8, 16, 16, 0, maze_xpm);

        for(int i = 0; i < 4; i++){
            if(ghosts_state[i].direction == right) ghosts_state[i].direction = left;
            else if(ghosts_state[i].direction == left) ghosts_state[i].direction = right;
            else if(ghosts_state[i].direction == up) ghosts_state[i].direction = down;
            else if(ghosts_state[i].direction == down) ghosts_state[i].direction = up;
        }
    }
    
    current_pellet_matrix[pacman_c.y / 8][pacman_c.x / 8] = 0;
}


void game_logic() {
    if (micros % 2 == 0)
        update_pacman();
    if (micros % 3 == 0)
        update_ghost();
    check_pellets();
    check_collisions();
    if(num_pellets == 0)
        state = won;
    if (display_score_timer > 0) {
        display_score_timer--;
        if (display_score_timer == 0) {
            game_paused = false;
        }
    }
}

void draw_ui(){
    draw_xpm(maze_xpm, maze_x, maze_y);

    draw_text("LIVES", 100, 100, 0xFFFFFF);
    for(int i = 0; i < pacman_lives; i++){
        draw_xpm(death_anim_xpm[0], 100 + i * 16, 110);
    }

    draw_text("SCORE", 100, 130, 0xFFFFFF);
    
    char score_str[20];
    sprintf(score_str, "%d", score);
    draw_text(score_str, 100, 140, 0xFFFF00); 
}

void draw_game(){
    draw_xpm(pacman_xpm[direction * 2 + micros / 8 % 2], maze_x + pacman_c.x, maze_y + pacman_c.y);
    for(int i = 0; i < 4; i++){
        if(ghosts_state[i].status == normal){
            if (energized_time != 0){
                if (energized_time < 50)
                    draw_xpm(energized_ghost_xpm[micros / 8 % 4], maze_x + ghosts_state[i].c.x, maze_y + ghosts_state[i].c.y);
                else
                    draw_xpm(energized_ghost_xpm[micros / 8 % 2], maze_x + ghosts_state[i].c.x, maze_y + ghosts_state[i].c.y);
            }else{
                draw_xpm(normal_ghost_xpm[i][micros / 8 % 2 + 2 *ghosts_state[i].direction], maze_x + ghosts_state[i].c.x, maze_y + ghosts_state[i].c.y);
            }
        }else if(ghosts_state[i].status == dead){
            draw_xpm(eyes_xpm[ghosts_state[i].direction], maze_x + ghosts_state[i].c.x, maze_y + ghosts_state[i].c.y);
        } else if(ghosts_state[i].status == jailed){
            draw_xpm(normal_ghost_xpm[i][micros / 8 % 2], maze_x + ghosts_state[i].c.x, maze_y + ghosts_state[i].c.y);
        } else if(ghosts_state[i].status == go_jail){
            draw_xpm(eyes_xpm[ghosts_state[i].direction], maze_x + ghosts_state[i].c.x, maze_y + ghosts_state[i].c.y);
        }
    }
    if (game_paused && display_score_timer > 0) {
        draw_xpm(scoree[score_display_index], maze_x + score_display_x, maze_y + score_display_y);
    }    
}

void draw_respawn(){
    if(micros / 6 < 12) 
        draw_xpm(death_anim_xpm[micros / 6], maze_x + pacman_c.x, maze_y + pacman_c.y);
    else
        state = respawn;
}

int game(){
    printf("loading assets\n");

    game_paused = false;

    display_score_timer = 0;

    score_display_x = 0;
    score_display_y = 0;
    score_display_index = 0;

    loadAssets();

    maze_x = vmi.XResolution / 2 - maze_xpm.width / 2;
    maze_y = vmi.YResolution / 2 - maze_xpm.height / 2;

    num_pellets = pellet_count;
    micros = 0;
    next_direction_time = 0;
    energized_time = 0;
    ghost_mode = 0;
    pacman_lives = 3;

    score = 0;
    bonus_multiplier = 1; 

    state = playing;

    pacman_c = (coords) {13 * 8, 23 * 8};
    ghosts_state[blinky_idx] = (ghost) { {13 * 8 + 4, 11 * 8},  left, 0, normal};
    ghosts_state[clyde_idx] =  (ghost) { {11 * 8 + 4, 14 * 8 + 4},up, 120, jailed};
    ghosts_state[inky_idx] =   (ghost) { {13 * 8 + 4, 14 * 8 + 4},up, 60, jailed};
    ghosts_state[pinky_idx] =  (ghost) { {15 * 8 + 4, 14 * 8 + 4},up, 180, jailed};

    printf("waiting for ESC key\n");

    while(1){
        uint64_t status = await_interrupt(1 << 0 | 1 << 1);

        if (status & 1 << 0) { /* subscribed interrupt */
            kbc_ih();
            if (verify_status()){
                switch (scancode){
                    case W_MAKE_CODE:
                        next_direction = up;
                        next_direction_time = 7;
                        break;
                    case A_MAKE_CODE:
                        next_direction = left;
                        next_direction_time = 7;
                        break;
                    case S_MAKE_CODE:
                        next_direction = down;
                        next_direction_time = 7;
                        break;
                    case D_MAKE_CODE:
                        next_direction = right;
                        next_direction_time = 7;
                        break;
                    case ESC_MAKE_CODE:
                        return 7;
                }
            }
        }
        if (status & 1 << 1) { /* subscribed interrupt */
            // Primeiro, sempre processe o timer de pontuação (mesmo quando pausado)
            if (display_score_timer > 0) {
                display_score_timer--;
                printf("display_score_timer: %d\n", display_score_timer);
                if (display_score_timer == 0) {
                    game_paused = false;
                    printf("game_paused redefinido para false\n");
                }
            }
            
            // Só executa a lógica do jogo se não estiver pausado
            if (!game_paused) {
                micros++;
                draw_ui();
                switch (state){
                    case playing:
                        game_logic();
                        draw_game();
                        break;
                    case lost:
                        if(pacman_lives == 0) {
                            vg_draw_rectangle(0, 0, vmi.XResolution, vmi.YResolution, 0x000000);

                            draw_text("GAME OVER!", vmi.XResolution / 2 - (9 * 8) / 2 - 8, vmi.YResolution / 2, 0xFFFF00);
                            draw_text("PRESS ESC", vmi.XResolution / 2 - 5 * 8, vmi.YResolution / 2 + 20, 0xFFFFFF);
                            break;
                        }

                        else draw_respawn();
                        break;
                    case respawn:
                        micros = 0;
                        next_direction_time = 0;
                        energized_time = 0;
                        ghost_mode = 0;
                        state = playing;

                        bonus_multiplier = 1;

                        pacman_c = (coords) {13 * 8, 23 * 8};

                        //reset ghosts
                        ghosts_state[blinky_idx] = (ghost) { {13 * 8 + 4, 11 * 8},  left, 0, normal};
                        ghosts_state[clyde_idx] =  (ghost) { {11 * 8 + 4, 14 * 8 + 4},up, 60, jailed};
                        ghosts_state[inky_idx] =   (ghost) { {13 * 8 + 4, 14 * 8 + 4},up, 120, jailed};
                        ghosts_state[pinky_idx] =  (ghost) { {15 * 8 + 4, 14 * 8 + 4},up, 180, jailed};

                        break;
                    case won:
                        vg_draw_rectangle(0, 0, vmi.XResolution, vmi.YResolution, 0x000000);

                        draw_text("YOU WON!", vmi.XResolution / 2 - 4 * 8, vmi.YResolution / 2, 0xFFFF00);
                        draw_text("PRESS ESC", vmi.XResolution / 2 - 5 * 8, vmi.YResolution / 2 + 20, 0xFFFFFF);

                        break;
                }

                if(refresh_screen()){
                    printf("refresh_screen failed\n");
                    return 4;
                }
            }
        }
    }
    return 0;
}

void draw_text(const char *text, int x, int y, uint32_t color) {
    for (int i = 0; text[i] != '\0'; i++) {
        char ch = text[i];
        if (ch >= 'A' && ch <= 'Z') {
            draw_xpm_colored(letters_xpm[ch - 'A'], x + i * 8, y, color);
        }
        else if (ch >= 'a' && ch <= 'z') {
            draw_xpm_colored(letters_xpm[ch - 'a'], x + i * 8, y, color);
        }
        else if (ch >= '0' && ch <= '9') {
            draw_xpm_colored(numbers_xpm[ch - '0'], x + i * 8, y, color );
        }
        else if (ch == '(') {
            draw_xpm_colored(letters_xpm[26], x + i * 8, y, color);
        }
        else if (ch == ')') {
            draw_xpm_colored(letters_xpm[27], x + i * 8, y, color);
        }
        else if (ch == '!') {
            draw_xpm_colored(letters_xpm[28], x + i * 8, y, color);
        }

            
    }
}
