#ifndef USERIO_H
#define USERIO_H

#include "rsmlayout.h"
#include <GLFW/glfw3.h>

typedef enum {
	NORMAL, /* Normal gamestate (mouse captured) */
	MENU, /* Escape menu (mouse free) */
	INVENTORY, /* Inventory menu (mouse free) */
	COMMAND /* Text prompt (mouse free) */
} Gamestate;

extern Gamestate gamestate;

extern int RSM_FORWARD;
extern int RSM_BACKWARD;
extern int RSM_RIGHT;
extern int RSM_LEFT;
extern int RSM_DOWN;
extern int RSM_UP;

void processKey(GLFWwindow *window, int key, int scancode, int action, int mods);

#endif
