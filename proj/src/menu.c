#include "menu.h"
#include <lcom/lcf.h>
#include <lcom/proj.h>

#include "devices/gpu.h"
#include "devices/i8042.h"
#include "devices/keyboard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sprites/letters.h"
#include "sprites/numbers.h"

#define UP_MAKE_CODE    0x48
#define DOWN_MAKE_CODE  0x50

// External definitions
extern uint8_t kbd_arq_set;
extern uint8_t scancode;
extern vbe_mode_info_t vmi;
xpm_image_t letters_xpm[28];
xpm_image_t numbers_xpm[10];


#define INITIAL_MENU_CAPACITY 4

int loadTextAssets(){
     for(int i = 0; i < 28; i++){
        xpm_load(letters[i], XPM_8_8_8, &letters_xpm[i]);
    }

        for(int i = 0; i < 10; i++){
        xpm_load(numbers[i], XPM_8_8_8, &numbers_xpm[i]);
    }


    return 0;
}

// Allocate and initialize a new menu
Menu* newMenu(char* title) {
    Menu* m = malloc(sizeof(Menu));
    if (!m) return NULL;

    m->title = strdup(title); // dynamically copy title
    m->num = 0;
    m->size = INITIAL_MENU_CAPACITY;
    m->entries = malloc(m->size * sizeof(MenuEntry*));
    return m;
}

// Free memory used by menu and its entries
void menuDelete(Menu* m) {
    if (!m) return;
    for (int i = 0; i < m->num; i++) {
        free(m->entries[i]->desc);
        free(m->entries[i]);
    }
    free(m->entries);
    free(m->title);
    free(m);
}

// Dynamically grow entries array if needed
static void menuAdjust(Menu* m) {
    if (m->num >= m->size) {
        m->size *= 2;
        m->entries = realloc(m->entries, m->size * sizeof(MenuEntry*));
    }
}

void menuAddFunction(Menu* m, char* desc, void (*func)(void)) {
    if (!m) return;
    MenuEntry* me = malloc(sizeof(MenuEntry));
    me->desc = strdup(desc);
    me->func = func;
    me->subMenu = NULL;
    menuAdjust(m);
    m->entries[m->num++] = me;
}

void menuAddMenu(Menu* m, char* desc, Menu* sm) {
    if (!m) return;
    MenuEntry* me = malloc(sizeof(MenuEntry));
    me->desc = strdup(desc);
    me->subMenu = sm;
    me->func = NULL;
    menuAdjust(m);
    m->entries[m->num++] = me;
}

// Optional text rendering stub (or use bitmap font drawing if implemented)
void vg_draw_text(const char* str, int x, int y, uint32_t color) {
   for (int i = 0; str[i] != '\0'; i++) {
        char ch = str[i];
        if (ch >= 'A' && ch <= 'Z') {
            draw_xpm(letters_xpm[ch - 'A'], x + i * 8, y);
            draw_xpm_colored(letters_xpm[ch - 'A'], x + i * 8, y, color);
        }
        else if (ch >= 'a' && ch <= 'z') {
            draw_xpm(letters_xpm[ch - 'a'], x + i * 8, y);
            draw_xpm_colored(letters_xpm[ch - 'a'], x + i * 8, y, color);
        }
        else if (ch >= '0' && ch <= '9') {
            draw_xpm(numbers_xpm[ch - '0'], x + i * 8, y);
            draw_xpm_colored(numbers_xpm[ch - '0'], x + i * 8, y, color );
        }
        else if (ch == ' ') {
             // Leave space between words
        }
        else if (ch == '(') {
            draw_xpm_colored(letters_xpm[26], x + i * 8, y, color);
        }
        else if (ch == ')') {
            draw_xpm_colored(letters_xpm[27], x + i * 8, y, color);
        }
    }
}

// Core menu function — draws and handles input
void menuPost(Menu* m) {
    if (!m || m->num == 0) return;

    loadTextAssets();

    int selected = 0;
    int running = 1;
    int ipc_status;
    message msg;
    int r;

    while (running) {
        // Clear screen
        vg_draw_rectangle(0, 0, vmi.XResolution, vmi.YResolution, 0x000000);

        // Draw title
        vg_draw_text(m->title, vmi.XResolution / 2 - 100, 50, 0xFFFFFF);

        // Draw entries
        for (int i = 0; i < m->num; i++) {
            uint32_t color = (i == selected) ? 0xFF0000 : 0xFFFFFF;
            vg_draw_rectangle(vmi.XResolution / 2 - 100, 100 + i * 40, 200, 30, color);
            vg_draw_text(m->entries[i]->desc, vmi.XResolution / 2 - 20, vmi.YResolution / 2 + 6, 0x000000);
        }

        refresh_screen();

        // Wait for interrupt
        if ((r = driver_receive(ANY, &msg, &ipc_status)) != 0) continue;

        if (is_ipc_notify(ipc_status)) {
            switch (_ENDPOINT_P(msg.m_source)) {
                case HARDWARE:
                    if (msg.m_notify.interrupts & kbd_arq_set) {
                        kbc_ih();
                        if (verify_status()) {
                            if (scancode == ESC_MAKE_CODE) return;

                            if (scancode == W_MAKE_CODE || scancode == UP_MAKE_CODE) {
                                selected = (selected - 1 + m->num) % m->num;
                            }
                            if (scancode == S_MAKE_CODE || scancode == DOWN_MAKE_CODE) {
                                selected = (selected + 1) % m->num;
                            }
                            if (scancode == ENTER_MAKE_CODE) {
                                MenuEntry* entry = m->entries[selected];
                                if (entry->func) {
                                    entry->func(); // Call function
                                } else if (entry->subMenu) {
                                    menuPost(entry->subMenu); // Open submenu
                                }
                                return; // Exit after selection
                            }
                        }
                    }
                    break;
                default:
                    break;
            }
        }
    }
}

void menuActivateOption(Menu *menu, int index) {
    if (menu->entries[index]->func) {
        menu->entries[index]->func(); // Call function
    }
}
