struct menu;
struct menu_entry;

// handy typedefs
typedef struct menu Menu;
typedef struct menu_entry MenuEntry;

struct menu {
    char *title; // menu title
    MenuEntry **entries; // pointer to array of menu entries
    int num; // number of menu entries
    int size; // array capacity
};

struct menu_entry {
    char *desc; // menu entry descriptive text
    Menu *subMenu; // non-NULL if entry is submenu
    void (*func)(); // non-NULL if entry selection calls a
};

Menu * newMenu(char *title); // the "constructor"
void menuDelete(Menu *m); // destructor

// Other "methods"
void menuAddFunction(Menu *m, char *desc, void (*f)(void));
void menuAddMenu(Menu *m, char *desc, Menu *sm);
void menuPost(Menu *m); // activate the menu