/**
 * @file menu.c
 * @brief Sistema de menus interativos e interface de usuário.
 *
 * Este arquivo implementa um sistema completo de menus para o jogo Pac-Man:
 * - Estrutura de dados dinâmica para criação de menus flexíveis
 * - Sistema de renderização de texto usando sprites de letras e números
 * - Navegação por teclado com suporte a setas e teclas WASD
 * - Diferentes tipos de entradas de menu (funções, submenus, labels)
 * - Interface para tela de pontuações com leitura de arquivo
 * - Suporte a menus aninhados e hierárquicos
 * - Renderização com destaque visual para opção selecionada
 *
 * O sistema de menus é implementado usando uma estrutura de dados dinâmica
 * que permite adicionar diferentes tipos de entradas: funções executáveis,
 * submenus para navegação hierárquica, e labels informativos não-selecionáveis.
 * 
 * A renderização utiliza sprites XPM para caracteres, permitindo texto
 * customizado que mantém a estética pixel art do jogo. O sistema suporta
 * navegação intuitiva e feedback visual para melhorar a experiência do usuário.
 *
 * @note A estrutura Menu utiliza realocação dinâmica para crescer conforme
 * necessário, começando com capacidade inicial de 4 entradas.
 * @note Complexidade de Tempo: O(n) para renderização onde n é o número de
 * entradas do menu; O(1) para navegação.
 * @note Complexidade de Espaço: O(n) onde n é o número total de entradas
 * em todos os menus carregados.
 * @version 1.0
 */

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

int loadTextAssets() {
    for (int i = 0; i < 28; i++) {
        xpm_load(letters[i], XPM_8_8_8, &letters_xpm[i]);
    }
    for (int i = 0; i < 10; i++) {
        xpm_load(numbers[i], XPM_8_8_8, &numbers_xpm[i]);
    }
    return 0;
}

Menu* newMenu(char* title) {
    Menu* m = malloc(sizeof(Menu));
    if (!m) return NULL;
    m->title = strdup(title);
    m->num = 0;
    m->size = INITIAL_MENU_CAPACITY;
    m->entries = malloc(m->size * sizeof(MenuEntry*));
    return m;
}

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
    me->selectable = (func != NULL);
    menuAdjust(m);
    m->entries[m->num++] = me;
}

void menuAddMenu(Menu* m, char* desc, Menu* sm) {
    if (!m) return;
    MenuEntry* me = malloc(sizeof(MenuEntry));
    me->desc = strdup(desc);
    me->subMenu = sm;
    me->func = NULL;
    me->selectable = 1;
    menuAdjust(m);
    m->entries[m->num++] = me;
}

void menuAddLabel(Menu* m, char* desc) {
    if (!m) return;
    MenuEntry* me = malloc(sizeof(MenuEntry));
    me->desc = strdup(desc);
    me->func = NULL;
    me->subMenu = NULL;
    me->selectable = 0;
    menuAdjust(m);
    m->entries[m->num++] = me;
}

void vg_draw_text(const char* str, int x, int y, uint32_t color) {
    for (int i = 0; str[i] != '\0'; i++) {
        char ch = str[i];
        if (ch >= 'A' && ch <= 'Z') {
            draw_xpm_colored(letters_xpm[ch - 'A'], x + i * 8, y, color);
        }
        else if (ch >= 'a' && ch <= 'z') {
            draw_xpm_colored(letters_xpm[ch - 'a'], x + i * 8, y, color);
        }
        else if (ch >= '0' && ch <= '9') {
            draw_xpm_colored(numbers_xpm[ch - '0'], x + i * 8, y, color);
        }
        else if (ch == ' ') {
            // space
        }
        else if (ch == '(') {
            draw_xpm_colored(letters_xpm[26], x + i * 8, y, color);
        }
        else if (ch == ')') {
            draw_xpm_colored(letters_xpm[27], x + i * 8, y, color);
        }
        else if (ch == '.') {
            // Optional: add a dot sprite or just skip space
        }
    }
}

void menuPost(Menu* m) {
    if (!m || m->num == 0) return;

    loadTextAssets();

    int selected = 0;
    while (!m->entries[selected]->selectable) {
        selected = (selected + 1) % m->num;
    }

    int running = 1;
    int ipc_status;
    message msg;
    int r;

    while (running) {
        // Clear screen
        vg_draw_rectangle(0, 0, vmi.XResolution, vmi.YResolution, 0x000000);

        // Draw title
        int title_width = strlen(m->title) * 8;
        int title_x = vmi.XResolution / 2 - title_width / 2;
        int title_y = 40;
        vg_draw_text(m->title, title_x, title_y, 0xFFFFFC);

        // Draw entries
        for (int i = 0; i < m->num; i++) {
            uint32_t color = (i == selected && m->entries[i]->selectable) ? 0xFF0000 : 0xFFFFFF;
            vg_draw_rectangle(vmi.XResolution / 2 - 100, 100 + i * 40, 200, 30, color);
            vg_draw_text(m->entries[i]->desc, vmi.XResolution / 2 - 90, 100 + i * 40 + 6, 0x000000);
        }

        refresh_screen();

        if ((r = driver_receive(ANY, &msg, &ipc_status)) != 0) continue;

        if (is_ipc_notify(ipc_status)) {
            switch (_ENDPOINT_P(msg.m_source)) {
                case HARDWARE:
                    if (msg.m_notify.interrupts & kbd_arq_set) {
                        kbc_ih();
                        if (verify_status()) {
                            if (scancode == ESC_MAKE_CODE) return;

                            if (scancode == W_MAKE_CODE || scancode == UP_MAKE_CODE) {
                                do {
                                    selected = (selected - 1 + m->num) % m->num;
                                } while (!m->entries[selected]->selectable);
                            }
                            if (scancode == S_MAKE_CODE || scancode == DOWN_MAKE_CODE) {
                                do {
                                    selected = (selected + 1) % m->num;
                                } while (!m->entries[selected]->selectable);
                            }
                            if (scancode == ENTER_MAKE_CODE && m->entries[selected]->selectable) {
                                MenuEntry* entry = m->entries[selected];
                                if (entry->func) {
                                    entry->func();
                                } else if (entry->subMenu) {
                                    menuPost(entry->subMenu);
                                }
                                return;
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

void menuActivateOption(Menu* menu, int index) {
    if (menu->entries[index]->func) {
        menu->entries[index]->func();
    }
}
