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
direction_t direction;
int ghost_state;
int pacman_state;
int energized_time;
int num_pellets;

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

int abs(int x) {
    return x >= 0 ? x : -x;
}

void debug_ghost_movement(ghost *g, int idx) {
    printf("Ghost %d at (%d,%d) moving in direction %d\n",
           idx, g->c.x, g->c.y, g->direction);

    // Verificar os valores da matriz do labirinto nas quatro direções
    printf("  Maze values: Right=%d, Left=%d, Up=%d, Down=%d\n",
           maze_matrix[g->c.y / 8][(g->c.x + 8) / 8],
           maze_matrix[g->c.y / 8][(g->c.x - 1) / 8],
           maze_matrix[(g->c.y - 1) / 8][g->c.x / 8],
           maze_matrix[(g->c.y + 8) / 8][g->c.x / 8]);
}


int try_move(int direction){
    switch (direction){
        case right:
            // Verificar teletransporte pelo túnel direito
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
            // Verificar teletransporte pelo túnel esquerdo
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

void move_ghost(ghost *g) {
    int new_x = g->c.x;
    int new_y = g->c.y;

    // Calcular nova posição baseada na direção
    switch (g->direction) {
        case right:
            new_x++;
            break;
        case left:
            new_x--;
            break;
        case up:
            new_y--;
            break;
        case down:
            new_y++;
            break;
    }

    // Verificar teletransporte pelo túnel
    if (new_x >= 27 * 8 + 8) {
        new_x = 0;
    } else if (new_x < 0) {
        new_x = 27 * 8 + 4;
    }

    // Verificar colisão com paredes (somente se estiver alinhado com a grade)
    if (new_x % 8 == 0 && new_y % 8 == 0) {
        int grid_x = new_x / 8;
        int grid_y = new_y / 8;

        // Se há uma parede, não mover nesta direção
        if (maze_matrix[grid_y][grid_x] == 1) {
            // Escolher uma nova direção aleatória
            int dirs[4] = {right, left, up, down};
            g->direction = dirs[rand() % 4];
            return;
        }
    }

    // Atualizar a posição se for válida
    g->c.x = new_x;
    g->c.y = new_y;
}



int distance(int x1, int y1, int x2, int y2){
    return (x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2);
}

void pathfind(ghost *g, int target_x, int target_y){
    // Armazenar a melhor direção e a menor distância
    int ghost_d = INT_MAX;
    int best_dir = g->direction; // Manter a direção atual como padrão

    // Primeiro, garantir que estamos alinhados à grade (múltiplos de 8)
    if (g->c.x % 8 != 0 || g->c.y % 8 != 0) {
        return; // Não mudar de direção se não estivermos alinhados à grade
    }

    // Verificar se mover para a direita é válido (não é a direção oposta e não tem parede)
    if (g->direction != left && maze_matrix[g->c.y / 8][(g->c.x + 8) / 8] == 0) {
        int d = distance(g->c.x + 8, g->c.y, target_x, target_y);
        if (d < ghost_d) {
            ghost_d = d;
            best_dir = right;
        }
    }

    // Verificar se mover para a esquerda é válido
    if (g->direction != right && maze_matrix[g->c.y / 8][(g->c.x - 1) / 8] == 0) {
        int d = distance(g->c.x - 1, g->c.y, target_x, target_y);
        if (d < ghost_d) {
            ghost_d = d;
            best_dir = left;
        }
    }

    // Verificar se mover para cima é válido
    if (g->direction != down && maze_matrix[(g->c.y - 1) / 8][g->c.x / 8] == 0) {
        int d = distance(g->c.x, g->c.y - 1, target_x, target_y);
        if (d < ghost_d) {
            ghost_d = d;
            best_dir = up;
        }
    }

    // Verificar se mover para baixo é válido
    if (g->direction != up && maze_matrix[(g->c.y + 8) / 8][g->c.x / 8] == 0) {
        int d = distance(g->c.x, g->c.y + 8, target_x, target_y);
        if (d < ghost_d) {
            ghost_d = d;
            best_dir = down;
        }
    }

    // Definir a melhor direção encontrada
    g->direction = best_dir;
}



void scatter(ghost *g, int idx){
    switch (idx){
        case blinky_idx:
            pathfind(g, 27 * 8 + 4, 4);
            break;
        case clyde_idx:
            pathfind(g, 4, 30 * 8 + 4);
            break;
        case inky_idx:
            pathfind(g, 27 * 8 + 4, 30 * 8 + 4);
            break;
        case pinky_idx:
            pathfind(g, 4, 4);
            break;
    }
    move_ghost(g);
}

void chase(ghost *g, int idx){
    coords target = {0,0};
    switch (idx){
        case blinky_idx:
            pathfind(g, pacman_c.x, pacman_c.y);
            break;
        case clyde_idx:
            if (distance(g->c.x, g->c.y, pacman_c.x, pacman_c.y) > 64)
                pathfind(g, pacman_c.x, pacman_c.y);
            else
                pathfind(g, 13 * 8 + 4, 14 * 8 + 4);
            break;
        case inky_idx:
            target = (coords) {pacman_c.x - (ghosts_state[blinky_idx].c.x - pacman_c.x), pacman_c.y - (ghosts_state[blinky_idx].c.y - pacman_c.y)};
            pathfind(g, target.x, target.y);
            break;
        case pinky_idx:
            target = (coords) {pacman_c.x, pacman_c.y};
            if (direction == right) target.x += 32;
            else if (direction == left) target.x -= 32;
            else if (direction == up) target.y -= 32;
            else if (direction == down) target.y += 32;
            pathfind(g, target.x, target.y);
            break;
    }
}


void flee(ghost *g) {
    int available_dirs[4] = {0, 0, 0, 0};
    int count = 0;

    // Verificar apenas direções opostas à direção do Pac-Man para uma fuga mais realista
    if (g->direction != left && maze_matrix[g->c.y / 8][(g->c.x + 8) / 8] == 0) {
        available_dirs[count++] = right;
    }

    if (g->direction != right && maze_matrix[g->c.y / 8][(g->c.x - 1) / 8] == 0) {
        available_dirs[count++] = left;
    }

    if (g->direction != down && maze_matrix[(g->c.y - 1) / 8][(g->c.x + 7) / 8] == 0) {
        available_dirs[count++] = up;
    }

    if (g->direction != up && maze_matrix[(g->c.y + 8) / 8][(g->c.x + 7) / 8] == 0) {
        available_dirs[count++] = down;
    }

    if (count > 0) {
        // Escolher uma direção aleatória mas preferindo direção oposta ao pacman
        int farthest_dir = -1;
        int max_dist = -1;

        for (int i = 0; i < count; i++) {
            int test_x = g->c.x;
            int test_y = g->c.y;

            // Simular movimento na direção disponível
            switch (available_dirs[i]) {
                case right: test_x += 8; break;
                case left:  test_x -= 8; break;
                case up:    test_y -= 8; break;
                case down:  test_y += 8; break;
            }

            int dist = distance(test_x, test_y, pacman_c.x, pacman_c.y);
            if (dist > max_dist) {
                max_dist = dist;
                farthest_dir = available_dirs[i];
            }
        }

        g->direction = farthest_dir;
    }

    move_ghost(g);
}


void reset_game() {
    for(int i = 0; i < 31; i++){
        for(int j = 0; j < 28; j++){
            current_pellet_matrix[i][j] = pellet_matrix[i][j];
            if(pellet_matrix[i][j] == 1){ 
                vg_draw_rectangle_xpm(j * 8 + 6, i * 8 + 4, 2, 2, pellet_color, maze_xpm);
            }else if(pellet_matrix[i][j] == 2){
                vg_draw_rectangle_xpm(j * 8 + 5, i * 8 + 3, 4, 4, pellet_color, maze_xpm);
            }
        }
    }
    
    num_pellets = pellet_count;
    pacman_c.x = 13 * 8;
    pacman_c.y = 23 * 8;
    direction = right;
    next_direction_time = 0;
    energized_time = 0;
    
    ghosts_state[0] = (ghost){ {13 * 8 + 4, 11 * 8}, up, 0, normal};
    ghosts_state[1] = (ghost){ {11 * 8 + 4, 14 * 8 + 4}, up, 100, jailed};
    ghosts_state[2] = (ghost){ {13 * 8 + 4, 14 * 8 + 4}, up, 200, jailed};
    ghosts_state[3] = (ghost){ {15 * 8 + 4, 14 * 8 + 4}, up, 300, jailed};
}


void game_logic(){
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

    // Corrigido: Checagem de pellets
    if(current_pellet_matrix[pacman_c.y / 8][pacman_c.x / 8] == 1){
        num_pellets--;
        vg_draw_rectangle_xpm(pacman_c.x - pacman_c.x % 8, pacman_c.y - pacman_c.y % 8, 8, 8, 0, maze_xpm);
    }else if(current_pellet_matrix[pacman_c.y / 8][pacman_c.x / 8] == 2){
        num_pellets--;
        vg_draw_rectangle_xpm(pacman_c.x - pacman_c.x % 8, pacman_c.y - pacman_c.y % 8, 8, 8, 0, maze_xpm);
        energized_time = 200;
    }
    current_pellet_matrix[pacman_c.y / 8][pacman_c.x / 8] = 0;

    for(int i = 0; i < 4; i++){
        if(ghosts_state[i].status == normal){

            debug_ghost_movement(&ghosts_state[i], i);
            // Corrigido: Lógica para fantasmas em modo normal
            if(ghosts_state[i].c.x % 8 == 0 && ghosts_state[i].c.y % 8 == 0){
                if (energized_time > 0){
                    flee(&ghosts_state[i]);
                } else {
                    // Alternar entre chase e scatter
                    if ((micros / 300) % 2 == 0) {
                        chase(&ghosts_state[i], i);
                    } else {
                        scatter(&ghosts_state[i], i);
                    }
                }
            } else {
                move_ghost(&ghosts_state[i]);
            }
        }else if(ghosts_state[i].status == dead){
            if(ghosts_state[i].c.x == 13 * 8 + 4 && ghosts_state[i].c.y == 11 * 8){
                ghosts_state[i].status = jailed;
                ghosts_state[i].time = 50; // Adicionar tempo na prisão após morrer
            }else{
                if(ghosts_state[i].c.x % 8 == 0 && ghosts_state[i].c.y % 8 == 0){
                    pathfind(&ghosts_state[i], 13 * 8 + 4, 11 * 8);
                }
                move_ghost(&ghosts_state[i]);
            }
        }else if(ghosts_state[i].status == jailed){
            if(ghosts_state[i].time > 0){
                ghosts_state[i].time--;
            }else{
                if(ghosts_state[i].c.x == 13 * 8 + 4 && ghosts_state[i].c.y == 11 * 8){
                    ghosts_state[i].status = normal;
                } else {
                    if(ghosts_state[i].c.x % 8 == 0 && ghosts_state[i].c.y % 8 == 0){
                        pathfind(&ghosts_state[i], 13 * 8 + 4, 11 * 8);
                    }
                    move_ghost(&ghosts_state[i]);
                }
            }
        }
    }

    // Bug 3: Corrigido - colisão entre Pac-Man e fantasmas
    for(int i = 0; i < 4; i++) {
        if(ghosts_state[i].status == normal) {
            // Verificar colisão entre

            if(abs(pacman_c.x - ghosts_state[i].c.x) < 8 && abs(pacman_c.y - ghosts_state[i].c.y) < 8) {
                if(energized_time > 0) {
                    ghosts_state[i].status = dead;
                } else {
                    // Reset do jogo quando o Pac-Man é pego
                    pacman_c.x = 13 * 8;
                    pacman_c.y = 23 * 8;
                    direction = right;
                    next_direction_time = 0;

                    ghosts_state[0] = (ghost){ {13 * 8 + 4, 11 * 8}, up, 0, normal};
                    ghosts_state[1] = (ghost){ {11 * 8 + 4, 14 * 8 + 4}, up, 100, jailed};
                    ghosts_state[2] = (ghost){ {13 * 8 + 4, 14 * 8 + 4}, up, 200, jailed};
                    ghosts_state[3] = (ghost){ {15 * 8 + 4, 14 * 8 + 4}, up, 300, jailed};

                    return;
                }
            }
        }
    }

    // Bug 4: Adicionando condição de vitória
    if(num_pellets == 0){
        // Reiniciar jogo ou mostrar tela de vitória
        reset_game();
    }

    if (energized_time > 0) energized_time--;
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

        if (status & 1 << 0) { 
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
        if (status & 1 << 1) { 
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
