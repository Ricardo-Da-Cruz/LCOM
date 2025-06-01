/**
 * @file game.c
 * @brief Implementação da lógica principal do jogo Pac-Man.
 *
 * Este arquivo contém toda a lógica de gameplay do Pac-Man, incluindo:
 * - Movimentação do Pac-Man e controle de direções
 * - Comportamento e inteligência artificial dos fantasmas (Blinky, Pinky, Inky, Clyde)
 * - Sistema de estados dos fantasmas (normal, assustado, morto, preso)
 * - Detecção de colisões entre Pac-Man e fantasmas
 * - Sistema de pontuação e multiplicadores de bônus
 * - Carregamento e renderização de sprites e animações
 * - Controle do estado do jogo (jogando, perdeu, ganhou, respawn)
 * - Gerenciamento da matriz de pellets e power pellets
 * - Renderização completa do labirinto e elementos do jogo
 *
 * O jogo implementa um sistema de máquina de estados para controlar diferentes
 * fases do gameplay, desde o menu inicial até as animações de morte e vitória.
 * Os fantasmas possuem comportamentos únicos e algoritmos de pathfinding para
 * perseguir o jogador de forma inteligente.
 *
 * @note Complexidade de Tempo: O(1) por frame para a maioria das operações,
 * com algumas operações de pathfinding que podem ser O(n) onde n é o número
 * de posições válidas no labirinto.
 * @note Complexidade de Espaço: O(n*m) onde n e m são as dimensões da matriz
 * do labirinto (31x28 neste caso).
 * @version 1.0
 */

#include <lcom/lcf.h>
#include <limits.h>
#include <math.h>
#include "devices/mouse.h"
#include "sprites/score.h"
#include "devices/keyboard.h"
#include "devices/timer.h"
#include "devices/gpu.h"
#include "devices/i8042.h"
#include "devices/interrupts.h"
#include <stdio.h>
#include "game.h"
#include <unistd.h>

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

typedef enum {
    normal,
    jailed, // the ghost is in the ghost house and is not free to move
    dead, // the ghost is dead and has to go back to the ghost house
    go_jail,
}ghost_status;

typedef enum {
    playing,
    lost,
    respawn,
    won,
    game_lost,
}game_state;

typedef struct{
    coords c;
    int direction;
    int time;
    ghost_status status;
}ghost;

game_state state;

int current_pellet_matrix[31][28];
int num_pellets;

uint32_t micros;

int energized_time;
bool clyde_is_scared = false;
int ghost_mode;

int next_direction;
int next_direction_time;

int maze_x;
int maze_y;

ghost ghosts_state[4];
coords pacman_c;
direction_t direction;
int pacman_lives;

bool game_paused;

int display_score_timer;
// LUGAR DA PONTUAÇÂO !!!
int score_display_x, score_display_y;
int score_display_index;

int score;
int bonus_multiplier; 
int score_saved;

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


/**
 * @brief Attempts to move Pac-Man in a given direction.
 * 
 * @param direction The intended direction to move.
 * @return 1 if the move is possible, 0 otherwise.
 */

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


/**
 * @brief Moves a ghost based on its current direction.
 *
 * @param g Pointer to the ghost to move.
 */
void move_ghost(ghost *g){
    switch (g->direction){
        case right:
            g->c.x++;
            if (g->c.x >= 27 * 8) g->c.x = 0;
            break;
        case left:
            g->c.x--;
            if (g->c.x < 0) g->c.x = 27 * 8;
            break;
        case up:
            g->c.y--;
            break;
        case down:
            g->c.y++;
            break;
    }
}



/**
 * @brief Calculates the Manhattan distance between two tiles.
 *
 * @param x1 X coordinate of the first point.
 * @param y1 Y coordinate of the first point.
 * @param x2 X coordinate of the second point.
 * @param y2 Y coordinate of the second point.
 * @return The distance.
 */
int distance(int x1, int y1, int x2, int y2){
    return (x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2);
}

/**
 * @brief Basic pathfinding logic to direct a ghost toward a target position.
 *
 * @param g Pointer to the ghost.
 * @param target_x Target X position.
 * @param target_y Target Y position.
 */
void pathfind(ghost *g, int target_x, int target_y){
    //check all directions except the one the ghost came from
    int ghost_d = INT_MAX;

    int dir = g->direction;
    // right
    if (dir != left){
        if (maze_matrix[g->c.y / 8][(g->c.x + 8) / 8] != 1
        && maze_matrix[(g->c.y + 7) / 8][(g->c.x + 8) / 8] != 1){
            int d = distance(g->c.x + 8, g->c.y, target_x, target_y);
            if (d < ghost_d){
                ghost_d = d;
                g->direction = right;
            }
        }
    }
    // left
    if (dir != right){
        if (maze_matrix[g->c.y / 8][(g->c.x - 1) / 8] != 1
        && maze_matrix[(g->c.y + 7) / 8][(g->c.x - 1) / 8] != 1){
            int d = distance(g->c.x - 1, g->c.y, target_x, target_y);
            if (d < ghost_d){
                ghost_d = d;
                g->direction = left;
            }
        }
    }
    // up
    if (dir != down){
        if (maze_matrix[(g->c.y - 1) / 8][(g->c.x + 7) / 8] != 1
        && maze_matrix[(g->c.y - 1) / 8][g->c.x / 8] != 1){
            int d = distance(g->c.x, g->c.y - 1, target_x, target_y);
            if (d < ghost_d){
                ghost_d = d;
                g->direction = up;
            }
        }
    }
    // down
    if (dir != up){
        if (maze_matrix[(g->c.y + 8) / 8][(g->c.x + 7) / 8] != 1
        && maze_matrix[(g->c.y + 8) / 8][g->c.x / 8] != 1){
            int d = distance(g->c.x, g->c.y + 8, target_x, target_y);
            if (d < ghost_d){
                ghost_d = d;
                g->direction = down;
            }
        } 
    }
}

/**
 * @brief Updates ghost movement using scatter behavior.
 *
 * @param g Pointer to the ghost.
 * @param idx Index of the ghost.
 */

 // MUDANÇA AQUI!!!!!!!!
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
}

/**
 * @brief Updates ghost movement using chase behavior.
 *
 * @param g Pointer to the ghost.
 * @param idx Index of the ghost.
 */

 // MUDANÇA AQUI!!!!!!!!
void chase(ghost *g, int idx){
    coords target = {0,0};
    int x = 14 * 8;
    int y = 11 * 8;
    switch (idx){
        case blinky_idx:
            //Blinky wants to move to pacman
            pathfind(&ghosts_state[idx], pacman_c.x, pacman_c.y);
            break;
        case clyde_idx:
            //Clyde wants to move to pacman if he is far away, otherwise he wants to move to the ghost house
            if (clyde_is_scared && ghosts_state[idx].c.x == x && ghosts_state[idx].c.y == y)
                    clyde_is_scared = false;
            else if (distance(ghosts_state[idx].c.x, ghosts_state[idx].c.y, pacman_c.x, pacman_c.y) < 16 * 3 * 16 * 3)
                    clyde_is_scared = true;
            //for performance distance is squared
            
            if (clyde_is_scared) pathfind(&ghosts_state[idx], x, y);                
            else pathfind(&ghosts_state[idx], pacman_c.x, pacman_c.y);

            break;
        case inky_idx:
            target = (coords) {pacman_c.x - (ghosts_state[blinky_idx].c.x - pacman_c.x), pacman_c.y - (ghosts_state[blinky_idx].c.y - pacman_c.y)};
            //Inky wants to move to the reflection of pacman through blinky
            pathfind(&ghosts_state[idx], target.x, target.y);
            break;
        case pinky_idx:
            //Pinky wants to move to a position 4 blocks in front of pacman
            target = (coords) {pacman_c.x, pacman_c.y};
            if (direction == right) target.x += 32;
            else if (direction == left) target.x -= 32;
            else if (direction == up) target.y -= 32;
            else if (direction == down) target.y += 32;
            pathfind(&ghosts_state[idx], target.x, target.y);
            
            break;
    }
}

int seconds1 = 0;

/**
 * @brief Updates all ghosts' states and behaviors.
 */
void (update_ghost)(){
    //ghosts have two modes scatter and chase
    //in scatter mode they move to a predefined position
    //pathfind doesn't need to be optimal, just minimize the linear distance to the target
    //ghost can't make 180 degree turns unless they get energized

    /*
        | Cycle # | Scatter Duration | Chase Duration                   |
        | ------- | ---------------- | -------------------------------- |
        | 1       | 7 seconds        | 20 seconds                       |
        | 2       | 7 seconds        | 20 seconds                       |
        | 3       | 5 seconds        | 20 seconds                       |
        | 4       | 5 seconds        | until Pac-Man dies or level ends |
    */
   int seconds = micros / 60;

    if (seconds < 7) ghost_mode = 0; // Scatter
    else if (seconds < 27) ghost_mode = 1; // Chase
    else if (seconds < 34) ghost_mode = 0;
    else if (seconds < 54) ghost_mode = 1;
    else if (seconds < 59) ghost_mode = 0;
    else ghost_mode = 1;

    for(int i = 0; i < 4; i++){
        if(ghosts_state[i].status == normal){
            //ghost pathfinding
            //move ghost
            if (energized_time == 0){
                if (ghost_mode == 0) 
                    scatter(&ghosts_state[i], i);
                else 
                    chase(&ghosts_state[i], i);
                move_ghost(&ghosts_state[i]);
            }else{
                //move ghost away from pacman
                scatter(&ghosts_state[i], i);
                move_ghost(&ghosts_state[i]);
            }
        }else if(ghosts_state[i].status == dead){
            //move ghost to ghost house
            if(ghosts_state[i].c.x == 13 * 8 + 4 && ghosts_state[i].c.y == 11 * 8){
                ghosts_state[i].status = go_jail;
                ghosts_state[i].time = 120; // tempo da prisão do nengue
            }else{
                pathfind(&ghosts_state[i], 13 * 8 + 4, 11 * 8);
                move_ghost(&ghosts_state[i]);
                if(!(ghosts_state[i].c.x == 13 * 8 + 4 && ghosts_state[i].c.y == 11 * 8)){
                    pathfind(&ghosts_state[i], 13 * 8 + 4, 11 * 8);
                    move_ghost(&ghosts_state[i]);
                }
            }
        }else if(ghosts_state[i].status == jailed){
            //keep ghost in ghost house
            if(ghosts_state[i].time > 0){
                ghosts_state[i].time--;
            }else{
                if (ghosts_state[i].c.x > 11*8 && ghosts_state[i].c.x < 16*8
                        && ghosts_state[i].c.y  > 12*8 && ghosts_state[i].c.y < 15*8){
                    if (ghosts_state[i].c.x == 13 * 8 + 4){
                        ghosts_state[i].direction = up;
                    }else if (ghosts_state[i].c.x > 13 * 8 + 4){
                        ghosts_state[i].direction = left;
                    }else if (ghosts_state[i].c.x < 13 * 8 + 4){
                        ghosts_state[i].direction = right;
                    }
                    move_ghost(&ghosts_state[i]);
                }else{
                    ghosts_state[i].status = normal;
                }
            }
        }
        else if(ghosts_state[i].status == go_jail){
            ghosts_state[i].direction = down;
            move_ghost(&ghosts_state[i]);

            if((ghosts_state[i].c.x == 13 * 8 + 4 && ghosts_state[i].c.y == 14 * 8 + 4)){
                ghosts_state[i].status = jailed;
                ghosts_state[i].time = 150; // tempo da prisão do nengue
            }
        }
    }
}

/**
 * @brief Updates Pac-Man's state based on input and movement.
 */
void (update_pacman)(){
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
}

/**
 * @brief Checks for collisions between Pac-Man and ghosts.
 */
void (check_collisions)(){
    for(int i = 0; i < 4; i++){
        if(ghosts_state[i].status == normal){
            if(distance(ghosts_state[i].c.x, ghosts_state[i].c.y, pacman_c.x, pacman_c.y) < 8 * 8){
                if (energized_time == 0){
                    state = lost;
                    pacman_lives--;
                    micros = 0;
                }else{
                    // Jackpot do jantar dos fantasmas
                    int ghost_score = 200 * bonus_multiplier;
                    score += ghost_score; // No pacman original é "200, 400, 800, 1600 pontos"
                    bonus_multiplier *= 2;
                    if(bonus_multiplier > 8) bonus_multiplier = 8; // Máximo 1600 pontos

                    ghosts_state[i].status = dead;

                    // Capture the position where the ghost was eaten
                    int ghost_eaten_x = ghosts_state[i].c.x;
                    int ghost_eaten_y = ghosts_state[i].c.y;

                    // Pause the game and display the score
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
    display_score_timer = 60; // Aumentei para 1 segundo (60 frames a 60fps)
    
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
    // Se quiser exibir o score visualmente, adicione aqui
    // draw_text com a pontuação na posição (x, y)
}

/**
 * @brief Checks and updates the state of collected pellets.
 */
void (check_pellets)(){
    if (energized_time != 0) energized_time--;

    if(current_pellet_matrix[pacman_c.y / 8][pacman_c.x / 8] == 1){
        num_pellets--;
        score += 10; // 10 pontos de nhambane
        //remove pellet from maze xpm
        vg_draw_rectangle_xpm(pacman_c.x - pacman_c.x % 8, pacman_c.y - pacman_c.y % 8, 8, 8, 0, maze_xpm);
    }else if(current_pellet_matrix[pacman_c.y / 8][pacman_c.x / 8] == 2){
        num_pellets--;
        score += 50; // 50 pontos de luanda
        energized_time = 700;
        bonus_multiplier = 1; // debuff de ruanda
        //remove pellet from maze xpm
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

/**
 * @brief Runs one logic step of the game loop.
 */
void (game_logic)(){
    if (micros % 2 == 0)
        update_pacman();

    if (micros % 3 == 0)
        update_ghost();

    check_pellets();

    check_collisions();

    if(num_pellets == 0)
        state = won;

    // Decrementa o temporizador de exibição da pontuação
    if (display_score_timer > 0) {
        display_score_timer--;
        printf("display_score_timer: %d\n", display_score_timer); // Adicione esta linha para depuração
        if (display_score_timer == 0) {
            game_paused = false;
            printf("game_paused redefinido para false\n"); // Adicione esta linha para depuração
        }
    }
}


/**
 * @brief Draws UI elements such as the score and lives.
 */
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

/**
 * @brief Draws the entire game scene including characters and the map.
 */
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



/**
 * @brief Draws the animation or visuals for Pac-Man respawning.
 */
void draw_respawn(){
    if(micros / 6 < 12) 
        draw_xpm(death_anim_xpm[micros / 6], maze_x + pacman_c.x, maze_y + pacman_c.y);
    else
        state = respawn;
}


/**
 * @brief Main game loop.
 * 
 * @return 0 on success.
 */
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
                        if (state == won || state == game_lost) {
                           if (!score_saved) {
                                FILE *file = fopen("/home/lcom/labs/proj/src/score.txt", "a+"); 
                                if (file) {
                                    // Move to the end and check last char
                                    fseek(file, -1, SEEK_END);
                                    int last = fgetc(file);
                                    if (last != '\n') fputc('\n', file); // Add newline if not present

                                    fprintf(file, "%d\n", score);
                                    fclose(file);
                                    score_saved = true;
                                    printf("Score %d saved to file\n", score);
                                } else {
                                    printf("Erro ao abrir score.txt\n");
                                }
                            }
                            return 5; // Return to scoreboard
                        } else {
                            printf("Exiting to MENU\n");
                            return 0;
                    }
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
                        if(pacman_lives <= 0) {
                            state = game_lost;
                            break;
                        }

                        else draw_respawn();
                        break;
                    case game_lost:
                            vg_draw_rectangle(0, 0, vmi.XResolution, vmi.YResolution, 0x000000);

                            draw_text("GAME OVER!", vmi.XResolution / 2 - (9 * 8) / 2 - 8, vmi.YResolution / 2, 0xFFFF00);
                            draw_text("PRESS ESC", vmi.XResolution / 2 - 5 * 8, vmi.YResolution / 2 + 20, 0xFFFFFF);

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
