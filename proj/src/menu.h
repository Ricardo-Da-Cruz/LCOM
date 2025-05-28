#ifndef MENU_H
#define MENU_H

struct menu;
struct menu_entry;

typedef struct menu Menu;
typedef struct menu_entry MenuEntry;

struct menu {
    char *title;
    MenuEntry **entries;
    int num;
    int size;
};

struct menu_entry {
    char *desc;
    Menu *subMenu;
    void (*func)();
    int selectable; 
};

Menu * newMenu(char *title);
void menuDelete(Menu *m);
void menuAddFunction(Menu *m, char *desc, void (*f)(void));
void menuAddMenu(Menu *m, char *desc, Menu *sm);
void menuPost(Menu *m);
void menuActivateOption(Menu *menu, int index);

#endif /* MENU_H */
