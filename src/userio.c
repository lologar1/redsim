#include "userio.h"

int RSM_FORWARD;
int RSM_BACKWARD;
int RSM_RIGHT;
int RSM_LEFT;
int RSM_DOWN;
int RSM_UP;

void processKey(GLFWwindow *window, int key, int scancode, int action, int mods) {
	/* Set appropriate user input variables for keypresses */

	(void) scancode;
	(void) mods;

	if (action == GLFW_REPEAT) return; /* Do not handle repeat events */

	switch (key) {
		case RSM_KEY_FORWARD: RSM_FORWARD = action == GLFW_PRESS ? 1 : 0; break;
		case RSM_KEY_BACKWARD: RSM_BACKWARD = action == GLFW_PRESS ? 1 : 0; break;
		case RSM_KEY_RIGHT: RSM_RIGHT = action == GLFW_PRESS ? 1 : 0; break;
		case RSM_KEY_LEFT: RSM_LEFT = action == GLFW_PRESS ? 1 : 0; break;
		case RSM_KEY_DOWN: RSM_DOWN = action == GLFW_PRESS ? 1 : 0; break;
		case RSM_KEY_UP: RSM_UP = action == GLFW_PRESS ? 1 : 0; break;
	}

	/* Now handle only first time key press */
	if (action == GLFW_RELEASE) return;

	switch (key) {
		case RSM_KEY_MENU:
			if (gamestate == NORMAL) gamestate = MENU;
			else gamestate = NORMAL;
			break;
		case RSM_KEY_INVENTORY:
			if (gamestate == NORMAL) gamestate = INVENTORY;
			break;
		case RSM_KEY_COMMAND:
			if (gamestate == NORMAL) gamestate = COMMAND;
			break;
	}

	/* Handle cursor status after a potential gamestate change */
	if (gamestate == NORMAL) glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	else glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}
