#include "ghosti.h"
#include <stdio.h>
#include "sprites/maze.h"
#include <stdlib.h>
#include <limits.h>  


// Variáveis globais dos fantasmas
ghost ghosts_state[4];
GhostAI ghost_ai[4];

uint8_t micros;

int ghost_mode = 0;
int energized_time = 0;
coords pacman_c;
direction_t direction;
bool clyde_is_scared = false;
int distance(int x1, int y1, int x2, int y2){
    return (x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2);
}

void init_ghost_ai() {
    ghost_ai[blinky_idx].home_corner = (coords){25 * 8, 1 * 8};
    ghost_ai[blinky_idx].aggression_level = 10;

    ghost_ai[pinky_idx].home_corner = (coords){2 * 8, 1 * 8};
    ghost_ai[pinky_idx].aggression_level = 8;

    ghost_ai[inky_idx].home_corner = (coords){27 * 8, 30 * 8};
    ghost_ai[inky_idx].aggression_level = 6;

    ghost_ai[clyde_idx].home_corner = (coords){0, 30 * 8};
    ghost_ai[clyde_idx].aggression_level = 4;
    
    for(int i = 0; i < 4; i++) {
        ghost_ai[i].behavior_timer = 0;
        ghost_ai[i].scatter_timer = 0;
        ghost_ai[i].is_scattering = false;
        ghost_ai[i].last_direction_change = 0;
    }
}

void update_ghost_ai(int ghost_idx) {
    ghost *g = &ghosts_state[ghost_idx];
    GhostAI *ai = &ghost_ai[ghost_idx];
    
    // Fantasmas mortos ou presos não usam IA
    if(g->status == dead || g->status == jailed) return;
    
    // Atualizar timers
    ai->behavior_timer++;
    ai->scatter_timer++;
    
    // Alternar entre scatter e chase
    if(ai->scatter_timer > 300) { // 5 segundos
        ai->is_scattering = !ai->is_scattering;
        ai->scatter_timer = 0;
    }
    
    // Determinar alvo
    coords target = get_ghost_target(ghost_idx);
    ai->target = target;
    
    // Encontrar caminho apenas ocasionalmente ou quando necessário
    if(ai->behavior_timer % 30 == 0 || ai->pathfinder.path_length == 0) {
        find_path(ghost_idx, g->c, target);
    }
    
    // Seguir caminho
    if(ai->pathfinder.path_length > 0 && ai->pathfinder.current_step < ai->pathfinder.path_length) {
        coords next_pos = ai->pathfinder.path[ai->pathfinder.current_step];
        
        // Determinar direção para o próximo passo
        int dx = next_pos.x - g->c.x;
        int dy = next_pos.y - g->c.y;
        
        if(abs(dx) > abs(dy)) {
            g->direction = (dx > 0) ? right : left;
        } else {
            g->direction = (dy > 0) ? down : up;
        }
        
        // Avançar no caminho quando próximo do próximo ponto
        if(manhattan_distance(pixel_to_grid(g->c), pixel_to_grid(next_pos)) < 2) {
            ai->pathfinder.current_step++;
        }
    }
}

int find_path(int ghost_idx, coords start, coords target) {
    PathFinder *pf = &ghost_ai[ghost_idx].pathfinder;
    
    // Converter para coordenadas de grid
    coords start_grid = pixel_to_grid(start);
    coords target_grid = pixel_to_grid(target);
    
    // Limpar listas
    for(int y = 0; y < 31; y++) {
        for(int x = 0; x < 28; x++) {
            pf->nodes[y][x].in_open_list = false;
            pf->nodes[y][x].in_closed_list = false;
            pf->nodes[y][x].f = pf->nodes[y][x].g = pf->nodes[y][x].h = 0;
        }
    }
    
    // Nó inicial
    PathNode *start_node = &pf->nodes[start_grid.y][start_grid.x];
    start_node->g = 0;
    start_node->h = manhattan_distance(start_grid, target_grid);
    start_node->f = start_node->g + start_node->h;
    start_node->in_open_list = true;
    
    while(true) {
        PathNode *current = NULL;
        int min_f = INT_MAX;
        coords current_pos;
        
        for(int y = 0; y < 31; y++) {
            for(int x = 0; x < 28; x++) {
                if(pf->nodes[y][x].in_open_list && pf->nodes[y][x].f < min_f) {
                    min_f = pf->nodes[y][x].f;
                    current = &pf->nodes[y][x];
                    current_pos = (coords){x, y};
                }
            }
        }
        
        if(current == NULL) return 0; 
        
        if(current_pos.x == target_grid.x && current_pos.y == target_grid.y) {
            pf->path_length = 0;
            coords pos = current_pos;
            
            while(!(pos.x == start_grid.x && pos.y == start_grid.y)) {
                pf->path[pf->path_length++] = grid_to_pixel(pos);
                PathNode *node = &pf->nodes[pos.y][pos.x];
                pos = (coords){node->parent_x, node->parent_y};
            }
            for(int i = 0; i < pf->path_length / 2; i++) {
                coords temp = pf->path[i];
                pf->path[i] = pf->path[pf->path_length - 1 - i];
                pf->path[pf->path_length - 1 - i] = temp;
            }
            
            pf->current_step = 0;
            return pf->path_length;
        }
        current->in_open_list = false;
        current->in_closed_list = true;
        int dx[] = {0, 1, 0, -1};
        int dy[] = {-1, 0, 1, 0};
        
        for(int i = 0; i < 4; i++) {
            int nx = current_pos.x + dx[i];
            int ny = current_pos.y + dy[i];
            if(!is_valid_position(nx, ny)) continue;
            PathNode *neighbor = &pf->nodes[ny][nx];
            if(neighbor->in_closed_list) continue;
            
            int tentative_g = current->g + 1;
            
            if(!neighbor->in_open_list || tentative_g < neighbor->g) {
                neighbor->parent_x = current_pos.x;
                neighbor->parent_y = current_pos.y;
                neighbor->g = tentative_g;
                neighbor->h = manhattan_distance((coords){nx, ny}, target_grid);
                neighbor->f = neighbor->g + neighbor->h;
                neighbor->in_open_list = true;
            }
        }
    }
    return 0;
}

coords get_pinky_target(coords pacman_c, direction_t direction) {
    coords target = pacman_c;
    
    switch(direction) {
        case up:
            target.y -= 4 * 8;
            target.x -= 4 * 8; 
            break;
        case down:
            target.y += 4 * 8;
            break;
        case left:
            target.x -= 4 * 8;
            break;
        case right:
            target.x += 4 * 8;
            break;
    }
    
    return target;
}

void pathfind(ghost *g, int target_x, int target_y){
    int ghost_d = INT_MAX;
    int dir = g->direction;
    
    // right
    if (dir != left){
        if (maze_matrix[g->c.y / 8][(g->c.x + 8) / 8] == 0
        && maze_matrix[(g->c.y + 7) / 8][(g->c.x + 8) / 8] == 0){
            int d = distance(g->c.x + 8, g->c.y, target_x, target_y);
            if (d < ghost_d){
                ghost_d = d;
                g->direction = right;
            }
        }
    }
    // left
    if (dir != right){
        if (maze_matrix[g->c.y / 8][(g->c.x - 1) / 8] == 0
        && maze_matrix[(g->c.y + 7) / 8][(g->c.x - 1) / 8] == 0){
            int d = distance(g->c.x - 1, g->c.y, target_x, target_y);
            if (d < ghost_d){
                ghost_d = d;
                g->direction = left;
            }
        }
    }
    // up
    if (dir != down){
        if (maze_matrix[(g->c.y - 1) / 8][(g->c.x + 7) / 8] == 0
        && maze_matrix[(g->c.y - 1) / 8][g->c.x / 8] == 0){
            int d = distance(g->c.x, g->c.y - 1, target_x, target_y);
            if (d < ghost_d){
                ghost_d = d;
                g->direction = up;
            }
        }
    }
    // down
    if (dir != up){
        if (maze_matrix[(g->c.y + 8) / 8][(g->c.x + 7) / 8] == 0
        && maze_matrix[(g->c.y + 8) / 8][g->c.x / 8] == 0){
            int d = distance(g->c.x, g->c.y + 8, target_x, target_y);
            if (d < ghost_d){
                ghost_d = d;
                g->direction = down;
            }
        } 
    }
}

coords get_inky_target(coords pacman_c, direction_t direction) {
    coords pacman_ahead = pacman_c;
    switch(direction) {
        case up:
            pacman_ahead.y -= 2 * 8;
            break;
        case down:
            pacman_ahead.y += 2 * 8;
            break;
        case left:
            pacman_ahead.x -= 2 * 8;
            break;
        case right:
            pacman_ahead.x += 2 * 8;
            break;
    }
    coords blinky_pos = ghosts_state[blinky_idx].c;
    coords target = {
        pacman_ahead.x + (pacman_ahead.x - blinky_pos.x),
        pacman_ahead.y + (pacman_ahead.y - blinky_pos.y)
    };
    return target;
}

coords get_blinky_target(coords pacman_c, direction_t direction) {
    return pacman_c;
}

coords get_clyde_target(coords pacman_c, direction_t direction) {
    int distance = manhattan_distance(pixel_to_grid(ghosts_state[clyde_idx].c), 
                                    pixel_to_grid(pacman_c));
    
    if(distance < 8) {
        return ghost_ai[clyde_idx].home_corner; 
    } else {
        return pacman_c;
    }
}


coords get_ghost_target(int ghost_idx) {
    GhostAI *ai = &ghost_ai[ghost_idx];
    if(ai->is_scattering || ghost_mode == 0) {
        return ai->home_corner;
    }
    
    if(energized_time > 0) {
        coords flee_target = ai->home_corner;
        flee_target.x += (rand() % 5 - 2) * 8;
        flee_target.y += (rand() % 5 - 2) * 8;
        return flee_target;
    }
    
    switch(ghost_idx) {
        case blinky_idx:
            return get_blinky_target(pacman_c,direction);
        case pinky_idx:
            return get_pinky_target(pacman_c,direction);
        case inky_idx:
            return get_inky_target(pacman_c,direction);
        case clyde_idx:
            return get_clyde_target(pacman_c,direction);
        default:
            return pacman_c;
    }
}

void move_ghost(ghost *g){
    int new_x = g->c.x;
    int new_y = g->c.y;
    
    switch (g->direction){
        case right:
            new_x++;
            if (new_x >= 27 * 8) new_x = 0; // tunnel da direita
            break;
        case left:
            new_x--;
            if (new_x < 0) new_x = 27 * 8; // tunnel da esquerda
            break;
        case up:
            new_y--;
            break;
        case down:
            new_y++;
            break;
    }
    
    // Verificar se a nova posição é válida (mesma lógica do pacman)
    if (new_x >= 27 * 8 || new_x < 0) {
        // Tunnel horizontal - sempre permitido
        g->c.x = new_x;
    } else {
        // Verificar colisão com paredes (usando a mesma lógica do try_move do pacman)
        bool can_move = false;
        
        switch (g->direction) {
            case right:
                if (maze_matrix[new_y / 8][(new_x + 8) / 8] == 0
                    && maze_matrix[(new_y + 7) / 8][(new_x + 8) / 8] == 0) {
                    can_move = true;
                }
                break;
            case left:
                if (maze_matrix[new_y / 8][(new_x - 1) / 8] == 0
                    && maze_matrix[(new_y + 7) / 8][(new_x - 1) / 8] == 0) {
                    can_move = true;
                }
                break;
            case up:
                if (maze_matrix[(new_y - 1) / 8][(new_x + 7) / 8] == 0
                    && maze_matrix[(new_y - 1) / 8][new_x / 8] == 0) {
                    can_move = true;
                }
                break;
            case down:
                if (maze_matrix[(new_y + 8) / 8][(new_x + 7) / 8] == 0
                    && maze_matrix[(new_y + 8) / 8][new_x / 8] == 0) {
                    can_move = true;
                }
                break;
        }
        
        if (can_move) {
            g->c.x = new_x;
            g->c.y = new_y;
        }
    }
}


void initialize_game_ai() {
    init_ghost_ai();
}

coords pixel_to_grid(coords pixel_coords) {
    return (coords){pixel_coords.x / 8, pixel_coords.y / 8};
}

coords grid_to_pixel(coords grid_coords) {
    return (coords){grid_coords.x * 8, grid_coords.y * 8};
}

bool is_valid_position(int x, int y) {
    if (x < 0 || x >= 28 || y < 0 || y >= 31) return false;
    return maze_matrix[y][x] == 0;  // 0 = caminho livre, 1 = parede
}

int manhattan_distance(coords a, coords b) {
    return abs(a.x - b.x) + abs(a.y - b.y);
}

void update_ghost() {
    if (micros / 60 == 7) ghost_mode = 1;
    else if (micros / 60 == 27) ghost_mode = 0;
    else if (micros / 60 == 34) ghost_mode = 1;
    else if (micros / 60 == 54) ghost_mode = 0;
    else if (micros / 60 == 59) ghost_mode = 1;

    for (int i = 0; i < 4; i++) {
        if (ghosts_state[i].status == normal) {
            if (energized_time == 0) {
                if (ghost_mode == 0)
                    scatter(&ghosts_state[i], i);
                else
                    chase(&ghosts_state[i], i);
                move_ghost(&ghosts_state[i]);
            } else {
                scatter(&ghosts_state[i], i);
                move_ghost(&ghosts_state[i]);
            }
        } else if (ghosts_state[i].status == dead) {
            if (ghosts_state[i].c.x == 13 * 8 + 4 && ghosts_state[i].c.y == 11 * 8) {
                ghosts_state[i].status = go_jail;
                ghosts_state[i].time = 120;
            } else {
                pathfind(&ghosts_state[i], 13 * 8 + 4, 11 * 8);
                move_ghost(&ghosts_state[i]);
            }
        } else if (ghosts_state[i].status == jailed) {
            if (ghosts_state[i].time > 0) {
                ghosts_state[i].time--;
            } else {
                if (ghosts_state[i].c.x > 11 * 8 && ghosts_state[i].c.x < 16 * 8
                    && ghosts_state[i].c.y > 12 * 8 && ghosts_state[i].c.y < 15 * 8) {
                    if (ghosts_state[i].c.x == 13 * 8 + 4) {
                        ghosts_state[i].direction = up;
                    } else if (ghosts_state[i].c.x > 13 * 8 + 4) {
                        ghosts_state[i].direction = left;
                    } else if (ghosts_state[i].c.x < 13 * 8 + 4) {
                        ghosts_state[i].direction = right;
                    }
                    move_ghost(&ghosts_state[i]);
                } else {
                    ghosts_state[i].status = normal;
                }
            }
        } else if (ghosts_state[i].status == go_jail) {
            ghosts_state[i].direction = down;
            move_ghost(&ghosts_state[i]);

            if (ghosts_state[i].c.x == 13 * 8 + 4 && ghosts_state[i].c.y == 14 * 8 + 4) {
                ghosts_state[i].status = jailed;
                ghosts_state[i].time = 150;
            }
        }

    }
}

void chase(ghost *g, int idx){
    coords target = {0,0};
    int x = 14 * 8;
    int y = 11 * 8;
    switch (idx){
        case blinky_idx:
            pathfind(&ghosts_state[idx], pacman_c.x, pacman_c.y);
            break;
        case clyde_idx:
            if (clyde_is_scared && ghosts_state[idx].c.x == x && ghosts_state[idx].c.y == y)
                    clyde_is_scared = false;
            else if (distance(ghosts_state[idx].c.x, ghosts_state[idx].c.y, pacman_c.x, pacman_c.y) < 16 * 3 * 16 * 3)
                    clyde_is_scared = true;
            
            if (clyde_is_scared) pathfind(&ghosts_state[idx], x, y);                
            else pathfind(&ghosts_state[idx], pacman_c.x, pacman_c.y);
            break;
        case inky_idx:
            target = (coords) {pacman_c.x - (ghosts_state[blinky_idx].c.x - pacman_c.x), pacman_c.y - (ghosts_state[blinky_idx].c.y - pacman_c.y)};
            pathfind(&ghosts_state[idx], target.x, target.y);
            break;
        case pinky_idx:
            target = (coords) {pacman_c.x, pacman_c.y};
            if (direction == right) target.x += 32;
            else if (direction == left) target.x -= 32;
            else if (direction == up) target.y -= 32;
            else if (direction == down) target.y += 32;
            pathfind(&ghosts_state[idx], target.x, target.y);
            
            break;
    }
}

void scatter(ghost *g, int idx) {
    coords target;

    switch (idx) {
        case blinky_idx:
            target = (coords){27 * 8 + 4, 4};
            break;
        case clyde_idx:
            target = (coords){4, 30 * 8 + 4};
            break;
        case inky_idx:
            target = (coords){27 * 8 + 4, 30 * 8 + 4};
            break;
        case pinky_idx:
            target = (coords){4, 4};
            break;
        default:
            return;
    }

    find_path(idx, g->c, target);
}

void debug_ghost_position(int ghost_idx) {
    ghost *g = &ghosts_state[ghost_idx];
    int grid_x = g->c.x / 8;
    int grid_y = g->c.y / 8;
    
    printf("Ghost %d: pos(%d,%d) grid(%d,%d) maze_val=%d\n", 
           ghost_idx, g->c.x, g->c.y, grid_x, grid_y, 
           (grid_x >= 0 && grid_x < 28 && grid_y >= 0 && grid_y < 31) ? maze_matrix[grid_y][grid_x] : -1);
}
