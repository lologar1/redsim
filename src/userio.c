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

void processChar(GLFWwindow *window, unsigned int codepoint) {
	/* Handle char inputs for command mode */
	char c = (char) codepoint;

	if (gamestate != COMMAND) {
		/* If RSM_KEY_COMMAND is a char, the swap logic is here. If not, it is in key callback */
		if (c == RSM_KEY_COMMAND && gamestate == NORMAL) {
			gamestate = COMMAND;
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			renderGUI();
		}
		return;
	}

	cmd_parseChar(c); /* Invalid characters rejected */
	renderGUI();
}

void processKey(GLFWwindow *window, int key, int scancode, int action, int mods) {
	/* Set appropriate user input variables for keypresses */

	(void) scancode;
	(void) mods;

	/* Priority keypress (escape) to switch back to NORMAL mode */
	if (action == GLFW_PRESS && key == RSM_KEY_MENU) {
		if (gamestate == NORMAL) gamestate = MENU;
		else gamestate = NORMAL;
	}

	/* Non-characters that affect the command prompt */
	if (gamestate == COMMAND && action != GLFW_RELEASE) {
		switch (key) {
			case GLFW_KEY_BACKSPACE: cmd_parseChar('\b'); break;
			case GLFW_KEY_ENTER: cmd_parseChar('\n'); break;
			case GLFW_KEY_TAB: cmd_parseChar('\t'); break;
		}

		goto ctrl; /* All further keypresses handled by char callback */
	}

	if (action == GLFW_REPEAT) return; /* Don't handle repeat events */

	/* Movement */
	switch (key) {
		case RSM_KEY_FORWARD: RSM_FORWARD = action == GLFW_PRESS ? 1 : 0; break;
		case RSM_KEY_BACKWARD: RSM_BACKWARD = action == GLFW_PRESS ? 1 : 0; break;
		case RSM_KEY_RIGHT: RSM_RIGHT = action == GLFW_PRESS ? 1 : 0; break;
		case RSM_KEY_LEFT: RSM_LEFT = action == GLFW_PRESS ? 1 : 0; break;
		case RSM_KEY_DOWN: RSM_DOWN = action == GLFW_PRESS ? 1 : 0; break;
		case RSM_KEY_UP: RSM_UP = action == GLFW_PRESS ? 1 : 0; break;
	}

	if (action != GLFW_PRESS) return; /* Only handle first key presses */

	switch (key) {
		case RSM_KEY_INVENTORY:
			if (gamestate == NORMAL) gamestate = INVENTORY;
			break;
		case RSM_KEY_COMMAND:
			/* If the key is also a character, then the switch logic is in the char callback! */
			// if (gamestate == NORMAL) gamestate = COMMAND;
			break;
		case RSM_KEY_HOTSLOT0: hotslotIndex = 0; break;
		case RSM_KEY_HOTSLOT1: hotslotIndex = 1; break;
		case RSM_KEY_HOTSLOT2: hotslotIndex = 2; break;
		case RSM_KEY_HOTSLOT3: hotslotIndex = 3; break;
		case RSM_KEY_HOTSLOT4: hotslotIndex = 4; break;
		case RSM_KEY_HOTSLOT5: hotslotIndex = 5; break;
		case RSM_KEY_HOTSLOT6: hotslotIndex = 6; break;
		case RSM_KEY_HOTSLOT7: hotslotIndex = 7; break;
		case RSM_KEY_HOTSLOT8: hotslotIndex = 8; break;
		case RSM_KEY_HOTSLOT9: hotslotIndex = 9; break;
		case RSM_KEY_HOTBAR_INCREMENT: hotbarIndex = (hotbarIndex + 1) % RSM_HOTBAR_COUNT; break;
		case RSM_KEY_HOTBAR_DECREMENT: hotbarIndex = (hotbarIndex - 1) % RSM_HOTBAR_COUNT; break;
	}

ctrl:
	/* CTRL keys */
	if (!(mods & GLFW_MOD_CONTROL)) goto skip;
	switch (key) {
		case RSM_KEY_CTRL_CLEARLOG:
			if (gamestate == COMMAND) {
				memset(cmdbuffer, 0, sizeof(cmdbuffer));
				cmdchar = cmdbuffer;
			} else memset(cmdlog, 0, sizeof(cmdlog));
			break;
		case RSM_KEY_CTRL_EXIT:
			glfwSetWindowShouldClose(window, GLFW_TRUE);
			break;
	}

skip:
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

			rsm_interact();
			break;

		case INVENTORY:
#define INV_BASE_X (WINDOW_WIDTH/2-RSM_INVENTORY_SLOT_SIZE_PIXELS * ((float) RSM_INVENTORY_SLOTS_HORIZONTAL/2))
#define INV_ICONS_Y (WINDOW_HEIGHT/2+RSM_INVENTORY_SLOT_SIZE_PIXELS * ((float) RSM_INVENTORY_SLOTS_VERTICAL/2))
#define INV_SLOTS_Y (WINDOW_HEIGHT/2-RSM_INVENTORY_SLOT_SIZE_PIXELS * ((float) RSM_INVENTORY_SLOTS_VERTICAL/2))
#define INV_SLOTS_WIDTH (RSM_INVENTORY_SLOT_SIZE_PIXELS * RSM_INVENTORY_SLOTS_HORIZONTAL)

			/* Assuming icons do not protrude from inventory slot bounds */
			if (x < INV_BASE_X || x > INV_BASE_X + INV_SLOTS_WIDTH) return;
			if (y < INV_SLOTS_Y || y > INV_ICONS_Y + RSM_INVENTORY_ICON_SIZE_PIXELS) return;

			if (y >= INV_ICONS_Y) { /* Change submenu */
				inventoryIndex = (unsigned int) (x - INV_BASE_X) / RSM_INVENTORY_ICON_SIZE_PIXELS;
			} else { /* Change hotbar configuration */
				uid = submenus[inventoryIndex]
					[(unsigned int) ((x - INV_BASE_X) / RSM_INVENTORY_SLOT_SIZE_PIXELS)]
					[(unsigned int) ((y - INV_SLOTS_Y) / RSM_INVENTORY_SLOT_SIZE_PIXELS)];
				hotbar[hotbarIndex][hotslotIndex][0] = GETID(uid);
				hotbar[hotbarIndex][hotslotIndex][1] = GETVARIANT(uid);
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

	hotslotIndex = (hotslotIndex + (yoffset < 0 ? 1 : -1) + RSM_HOTBAR_SLOTS) % RSM_HOTBAR_SLOTS;

	renderGUI();
}
