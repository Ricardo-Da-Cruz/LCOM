#include "menu.h"
#include <stdio.h>

void e1() {printf("-e1-\n");}     
void se1() {printf("-se1-\n");}    
void sse1() {printf("-sse1-\n");}  
    
void e2() {printf("-e2-\n");}
void se2() {printf("-se2-\n");}
void sse2() {printf("-sse2-\n");}

int main() {
    Menu *ssm1 = newMenu("Sub Sub Menu 1");
    menuAddFunction(ssm1, "Sub Sub Entry 1", sse1);
    menuAddFunction(ssm1, "Sub Sub Entry 2", sse2);

    Menu *sm1 = newMenu("Sub Menu 1");
    menuAddFunction(sm1, "Sub Entry 1", se1);
    menuAddFunction(sm1, "Sub Entry 2", se2);
    menuAddMenu(sm1, "Sub Sub Menu 1", ssm1);

    Menu *m1 = newMenu("Main Menu");
    menuAddFunction(m1, "Entry 1", e1);
    menuAddFunction(m1, "Entry 2", e2);
    menuAddMenu(m1, "Sub Menu 1", sm1);

    menuPost(m1);
    menuDelete(m1); menuDelete(sm1); menuDelete(ssm1);

    return 0;
}

Menu *newMenu(char *title) {
    struct menu *m = malloc(sizeof(Menu)); 
    if (m == NULL) {
        printf("Error: Out of memory\n");
        exit(1);
    }
    m->title = strdup(title); // Corrigido: Copiando o título corretamente
    m->num = 0;
    m->size = 10; // Tamanho inicial razoável
    m->entries = malloc(m->size * sizeof(MenuEntry*));
    if (m->entries == NULL) {
        printf("Error: Out of memory\n");
        free(m);
        exit(1);
    }
    return m;
}

void menuAddFunction(Menu *m, char desc, void (*func)()) {
    MenuEntry *me = malloc(sizeof(MenuEntry));
    me->desc = desc;
    me->func = func; me->subMenu = NULL;
    m->entries[m->num++] = me;
    menuAdjust(m);
}

void menuAddMenu(Menu *m, char *desc, Menu *sm) {
    MenuEntry *me = malloc(sizeof(MenuEntry));
    me->desc = desc;
    me->subMenu = sm ; me->func = NULL;
    m->entries[m->num++] = me;
    menuAdjust(m);
}


void menuPost(Menu *m) {
    int choice;
    char *su = saveUnder(m); // save area under new menu
    while(1) {
    // draw menu and accept user choice
    choice = selectEntry(m);
    if( choice == 0 ) {
    restoreUnder(m, su);
    return;
    }
    if( m->entries[choice-1]->func != NULL )
    (*(m->entries[choice-1]->func))(); // call handler
    else // if( m->entries[choice-1]->subMenu != NULL )
    menuPost(m->entries[choice-1]->subMenu); // activate
    }
}
    //draw menu, accept user choice
    // return index of selected entry (
static int selectEntry(Menu *m) {}