#include "userio.h"

int RSM_FORWARD;
int RSM_BACKWARD;
int RSM_RIGHT;
int RSM_LEFT;
int RSM_DOWN;
int RSM_UP;
int RSM_LEFTCLICK;
int RSM_RIGHTCLICK;
int RSM_MIDDLECLICK;

float mouseX, mouseY;

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
		case RSM_KEY_HOTSLOT0: hotbarIndex = 0; break;
		case RSM_KEY_HOTSLOT1: hotbarIndex = 1; break;
		case RSM_KEY_HOTSLOT2: hotbarIndex = 2; break;
		case RSM_KEY_HOTSLOT3: hotbarIndex = 3; break;
		case RSM_KEY_HOTSLOT4: hotbarIndex = 4; break;
		case RSM_KEY_HOTSLOT5: hotbarIndex = 5; break;
		case RSM_KEY_HOTSLOT6: hotbarIndex = 6; break;
		case RSM_KEY_HOTSLOT7: hotbarIndex = 7; break;
		case RSM_KEY_HOTSLOT8: hotbarIndex = 8; break;
		case RSM_KEY_HOTSLOT9: hotbarIndex = 9; break;
	}

	/* Handle cursor status after a potential gamestate change */
	if (gamestate == NORMAL) glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	else glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

	renderGUI(); /* GUI may have changed */
}

void processMouseMovement(GLFWwindow *window, double xpos, double ypos) {
	/* Handle mouse movements and adjust pitch/yaw accordingly */

	(void) window;

	float xoffset = xpos - mouseX;
	float yoffset = mouseY - ypos; /* Reversed to account for Y axis swap */

	mouseX = xpos; mouseY = ypos;

	/* Don't update game view if not in normal mode */
	if (gamestate != NORMAL) return;

	/* Convert to degrees of movement by multiplying with sensitivity */
	xoffset *= RSM_MOUSE_SENSITIVITY;
	yoffset *= RSM_MOUSE_SENSITIVITY;

	yaw += xoffset;
	pitch += yoffset;

	/* Prevent weird angle swaps when cranking the neck too far */
	if (pitch > 89.0f) pitch = 89.0f;
	if (pitch < -89.0f) pitch = -89.0f;
}

void processMouseInput(GLFWwindow *window, int button, int action, int mods) {
	/* Handle mouse clicks */

	(void) window;
	(void) mods;

	/* Convert from real screen xy (and inverted y) to WINDOW_WIDTH by WINDOW_HEIGHT (and right y) positions */
	float x, y;
	x = mouseX * ((float) WINDOW_WIDTH/screenWidth);
	y = (screenHeight - mouseY) * ((float) WINDOW_HEIGHT/screenHeight);

	uint64_t uid;
	switch (gamestate) {
		case NORMAL:
			switch (button) {
				case RSM_BUTTON_LEFTCLICK: RSM_LEFTCLICK = action == GLFW_PRESS ? 1 : 0; break;
				case RSM_BUTTON_RIGHTCLICK: RSM_RIGHTCLICK = action == GLFW_PRESS ? 1 : 0; break;
				case RSM_BUTTON_MIDDLECLICK: RSM_MIDDLECLICK = action == GLFW_PRESS ? 1 : 0; break;
			}
			break;

		case INVENTORY:
#define INV_BASE_X (WINDOW_WIDTH/2-RSM_INVENTORY_SLOT_SIZE_PIXELS * ((float) RSM_INVENTORY_SLOTS_HORIZONTAL/2))
#define INV_ICONS_Y (WINDOW_HEIGHT/2+RSM_INVENTORY_SLOT_SIZE_PIXELS * ((float) RSM_INVENTORY_SLOTS_VERTICAL/2))
#define INV_SLOTS_Y (WINDOW_HEIGHT/2-RSM_INVENTORY_SLOT_SIZE_PIXELS * ((float) RSM_INVENTORY_SLOTS_VERTICAL/2))
#define INV_SLOTS_WIDTH (RSM_INVENTORY_SLOT_SIZE_PIXELS * RSM_INVENTORY_SLOTS_HORIZONTAL)
#define INV_SLOTS_HEIGHT (RSM_INVENTORY_SLOT_SIZE_PIXELS * RSM_INVENTORY_SLOTS_VERTICAL)

			/* Assuming icons do not protrude from inventory slot bounds */
			if (x < INV_BASE_X || x > INV_BASE_X + INV_SLOTS_WIDTH) return;
			if (y < INV_SLOTS_Y || y > INV_ICONS_Y + RSM_INVENTORY_ICON_SIZE_PIXELS) return;

			if (y >= INV_ICONS_Y) { /* Change submenu */
				inventoryIndex = (unsigned int) (x - INV_BASE_X) / RSM_INVENTORY_ICON_SIZE_PIXELS;
			} else { /* Change hotbar configuration */
				uid = submenus[inventoryIndex]
					[(unsigned int) ((x - INV_BASE_X) / RSM_INVENTORY_SLOT_SIZE_PIXELS)]
					[(unsigned int) ((y - INV_SLOTS_Y) / RSM_INVENTORY_SLOT_SIZE_PIXELS)];
				hotbar[hotbarIndex][0] = uid >> 32;
				hotbar[hotbarIndex][1] = uid & 0xFFFFFFFF;
			}
			break;

		case MENU: /*TODO*/
			break;

		case COMMAND:
			return;
	}

	renderGUI();
}

void processMouseScroll(GLFWwindow *window, double xoffset, double yoffset) {
	/* Handle mouse scroll */

	(void) window;
	(void) xoffset;

	hotbarIndex = (hotbarIndex + (yoffset < 0 ? 1 : -1) + RSM_HOTBAR_SLOTS) % RSM_HOTBAR_SLOTS;

	renderGUI();
}
