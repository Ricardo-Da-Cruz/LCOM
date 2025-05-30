struct menu;
struct menu_entry;

// handy typedefs
typedef struct menu Menu;
typedef struct menu_entry MenuEntry;

/**
 * @struct menu
 * @brief Represents a menu with a title and a dynamic list of entries.
 */
struct menu {
    char *title; // menu title
    MenuEntry **entries; // pointer to array of menu entries
    int num; // number of menu entries
    int size; // array capacity
};

/**
 * @struct menu_entry
 * @brief Represents a single entry in a menu.
 */
struct menu_entry {
    char *desc; // menu entry descriptive text
    Menu *subMenu; // non-NULL if entry is submenu
    void (*func)(); // non-NULL if entry selection calls a
};

/**
 * @brief Creates a new menu with a specified title.
 * @param title The title of the menu.
 * @return A pointer to the created Menu structure.
 */
Menu * newMenu(char *title); // the "constructor"

/**
 * @brief Frees the memory associated with a menu and its entries.
 * @param m Pointer to the menu to delete.
 */
void menuDelete(Menu *m); // destructor


/**
 * @brief Adds a selectable entry to the menu which triggers a function.
 * @param m Pointer to the menu.
 * @param desc Description of the entry.
 * @param f Function to call when the entry is selected.
 */
void menuAddFunction(Menu *m, char *desc, void (*f)(void));

/**
 * @brief Adds a submenu entry to the menu.
 * @param m Pointer to the parent menu.
 * @param desc Description of the submenu entry.
 * @param sm Pointer to the submenu.
 */
void menuAddMenu(Menu *m, char *desc, Menu *sm);


/**
 * @brief Displays and manages user interaction with the menu.
 * @param m Pointer to the menu to activate.
 */
void menuPost(Menu *m); // activate the menu