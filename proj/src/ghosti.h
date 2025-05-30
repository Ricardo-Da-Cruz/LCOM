#ifndef GHOSTIS_H
#define GHOSTIS_H
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    normal,
    jailed, // the ghost is in the ghost house and is not free to move
    dead, // the ghost is dead and has to go back to the ghost house
    go_jail,
} ghost_status;

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

typedef struct {
    int x, y;
    int f, g, h;
    int parent_x, parent_y;
    bool in_open_list;
    bool in_closed_list;
} PathNode;

typedef struct {
    PathNode nodes[31][28];
    coords path[100];
    int path_length;
    int current_step;
} PathFinder;

typedef struct {
    coords c;
    int direction;
    int time;
    ghost_status status;
} ghost;

typedef struct {
    PathFinder pathfinder;
    coords target;
    int behavior_timer;
    int scatter_timer;
    bool is_scattering;
    coords home_corner;  
    int aggression_level;
    int last_direction_change;
} GhostAI;
extern uint8_t micros;
extern int ghost_mode;
extern int energized_time;
extern coords pacman_c;
extern direction_t direction;
extern bool clyde_is_scared;

extern ghost ghosts_state[4];
extern GhostAI ghost_ai[4];

int distance(int x1, int y1, int x2, int y2);
void init_ghost_ai(); //
void update_ghost_ai(int ghost_idx); //
void move_ghost(ghost *g); //
int find_path(int ghost_idx, coords start, coords target); // 
coords get_pinky_target(coords pacman_c, direction_t direction); //
coords get_inky_target(coords pacman_c, direction_t direction); // 
coords get_blinky_target(coords pacman_c, direction_t direction); // 
coords get_clyde_target(coords pacman_c, direction_t direction); //
coords get_ghost_target(int ghost_idx); //
int manhattan_distance(coords a, coords b); //
bool is_valid_position(int x, int y); //
coords grid_to_pixel(coords grid_coords); //
coords pixel_to_grid(coords pixel_coords); // 
void initialize_game_ai(); // 
void update_ghost(); //
void chase(ghost *g, int idx);
void scatter(ghost *g, int idx);
void pathfind(ghost *g, int target_x, int target_y);

















extern const int pellet_color;
extern const int pellet_count;
extern const int maze_matrix[31][28];
extern const int pellet_matrix[31][28];
extern const char *maze[];


#endif // GHOSTS_H

