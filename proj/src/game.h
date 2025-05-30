#ifndef _GAME_H_
#define _GAME_H_

int game();
void draw_text(const char *text, int x, int y, uint32_t color);
int loadAssets();
void display_ghost_score(int x, int y, int score);
void clear_text_area(int x, int y, const char *text);

#endif
