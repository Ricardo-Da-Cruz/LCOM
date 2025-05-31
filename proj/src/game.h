#ifndef _GAME_H_
#define _GAME_H_

/**
 * @brief Starts and runs the Pac-Man game loop.
 *
 * @return 0 on success, non-zero on error.
 */
int game();

/**
 * @brief Draws a string of characters using letter and number sprites.
 *
 * @param text The string to draw.
 * @param x X-coordinate for the text.
 * @param y Y-coordinate for the text.
 * @param color Color to apply to the drawn text.
 */
void draw_text(const char *text, int x, int y, uint32_t color);

/**
 * @brief Loads all sprite assets (letters, numbers, characters).
 *
 * @return 0 on success, non-zero on failure.
 */
int loadAssets();

/**
 * @brief Displays the score value that appears when a ghost is eaten.
 *
 * @param x X-coordinate where the score should appear.
 * @param y Y-coordinate for the score.
 * @param score Value to display.
 */
void display_ghost_score(int x, int y, int score);


/**
 * @brief Clears the area where a given text string was drawn.
 *
 * @param x X-coordinate where the text started.
 * @param y Y-coordinate of the text.
 * @param text The same string that was drawn.
 */
void clear_text_area(int x, int y, const char *text);

#endif
