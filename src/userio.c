#include "userio.h"

i32 rsm_forward_;
i32 rsm_backward_;
i32 rsm_right_;
i32 rsm_left_;
i32 rsm_down_;
i32 rsm_up_;
i32 rsm_leftclick_;
i32 rsm_rightclick_;
i32 rsm_middleclick_;
i32 rsm_forceplace_;
f64 mouseX_, mouseY_;

void io_processChar(GLFWwindow *window, u32 codepoint) {
	/* Handle char inputs for command mode */
	char c;
	c = (char) codepoint;

	if (gamestate_ != COMMAND) {
		/* If RSM_KEY_COMMAND is a char, the swap logic is here. If not, it is in key callback.
		 * Pending a better system, the user must uncomment the proper code sections for this transition. */
		if (c == RSM_KEY_COMMAND && gamestate_ == NORMAL) {
			gamestate_ = COMMAND;
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			gui_updateGUI();
		}
		return;
	}

	cmd_parseChar(c);
	gui_updateGUI();
}

void io_processKey(GLFWwindow *window, i32 key, i32 scancode, i32 action, i32 mods) {
	/* Set appropriate user input variables for keypresses */

	(void) scancode;
	(void) mods;

	/* Priority keypress (escape) to switch back to NORMAL mode */
	if (action == GLFW_PRESS && key == RSM_KEY_MENU) {
		if (gamestate_ == NORMAL) gamestate_ = MENU;
		else gamestate_ = NORMAL;
		gui_updateGUI();
	}

	/* Non-characters that affect the command prompt */
	if (gamestate_ == COMMAND && action != GLFW_RELEASE) {
		switch (key) {
			case GLFW_KEY_BACKSPACE: cmd_parseChar('\b'); gui_updateGUI(); break;
			case GLFW_KEY_ENTER: cmd_parseChar('\n'); gamestate_ = NORMAL; gui_updateGUI(); break;
			case GLFW_KEY_TAB: cmd_parseChar('\t'); gui_updateGUI(); break;
		}

		goto ctrl;
	}

	if (action == GLFW_REPEAT) return; /* Don't handle repeat events from now on */

	/* Movement */
	switch (key) {
		case RSM_KEY_FORWARD: rsm_forward_ = action == GLFW_PRESS ? 1 : 0; break;
		case RSM_KEY_BACKWARD: rsm_backward_ = action == GLFW_PRESS ? 1 : 0; break;
		case RSM_KEY_RIGHT: rsm_right_ = action == GLFW_PRESS ? 1 : 0; break;
		case RSM_KEY_LEFT: rsm_left_ = action == GLFW_PRESS ? 1 : 0; break;
		case RSM_KEY_DOWN: rsm_down_ = action == GLFW_PRESS ? 1 : 0; break;
		case RSM_KEY_UP: rsm_up_ = action == GLFW_PRESS ? 1 : 0; break;
		case RSM_KEY_FORCEPLACE: rsm_forceplace_ = action == GLFW_PRESS ? 1 : 0; break;
	}

	if (action != GLFW_PRESS) return; /* Only handle first key presses */

	switch (key) {
		case RSM_KEY_INVENTORY:
			if (gamestate_ == NORMAL) gamestate_ = INVENTORY;
			gui_updateGUI();
			break;
		case RSM_KEY_COMMAND:
			/* If the key is also a character, then the switch logic is in the char callback!
			 * Pending a better system, the user must uncomment the proper code sections for this transition. */
			/* UNCOMMENT THIS! if (gamestate_ == NORMAL) gamestate_ = COMMAND; */
			break;
		case RSM_KEY_HOTSLOT0: hotslotIndex_ = 0; gui_updateGUI(); break;
		case RSM_KEY_HOTSLOT1: hotslotIndex_ = 1; gui_updateGUI(); break;
		case RSM_KEY_HOTSLOT2: hotslotIndex_ = 2; gui_updateGUI(); break;
		case RSM_KEY_HOTSLOT3: hotslotIndex_ = 3; gui_updateGUI(); break;
		case RSM_KEY_HOTSLOT4: hotslotIndex_ = 4; gui_updateGUI(); break;
		case RSM_KEY_HOTSLOT5: hotslotIndex_ = 5; gui_updateGUI(); break;
		case RSM_KEY_HOTSLOT6: hotslotIndex_ = 6; gui_updateGUI(); break;
		case RSM_KEY_HOTSLOT7: hotslotIndex_ = 7; gui_updateGUI(); break;
		case RSM_KEY_HOTSLOT8: hotslotIndex_ = 8; gui_updateGUI(); break;
		case RSM_KEY_HOTSLOT9: hotslotIndex_ = 9; gui_updateGUI(); break;
		case RSM_KEY_HOTBAR_INCREMENT: hotbarIndex_ = (hotbarIndex_+1)%RSM_HOTBAR_COUNT; gui_updateGUI(); break;
		case RSM_KEY_HOTBAR_DECREMENT: hotbarIndex_ = (hotbarIndex_-1)%RSM_HOTBAR_COUNT; gui_updateGUI(); break;
	}

	/* CTRL keys */
ctrl:
	if (mods & GLFW_MOD_CONTROL) switch (key) {
		case RSM_KEY_CTRL_CLEARLOG:
			if (gamestate_ == COMMAND) {
				memset(cmdbuffer_, 0, sizeof(cmdbuffer_));
				cmdptr_ = cmdbuffer_;
			} else memset(cmdlog_, 0, sizeof(cmdlog_));
			gui_updateGUI();
			break;
		case RSM_KEY_CTRL_EXIT:
			glfwSetWindowShouldClose(window, GLFW_TRUE);
			break;
	}

	/* Handle cursor status after a potential gamestate change */
	if (gamestate_ == NORMAL) glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	else glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

	glfwGetCursorPos(window, &mouseX_, &mouseY_); /* Prevent mouse teleporting after cursor capture change */
	mouseY_ = screenHeight_ - mouseY_; /* Y axis swap */
}

void io_processMouseMovement(GLFWwindow *window, f64 xpos, f64 ypos) {
	/* Handle mouse movements and adjust pitch/yaw accordingly */

	(void) window;
	ypos = screenHeight_ - ypos; /* Y axis swap */

	f64 xoffset = xpos - mouseX_;
	f64 yoffset = ypos - mouseY_;

	mouseX_ = xpos;
	mouseY_ = ypos; /* Reversed to account for Y axis swap */

	/* Don't update game view if not in normal mode */
	if (gamestate_ != NORMAL) return;

	/* Convert to degrees of movement by multiplying with sensitivity */
	xoffset *= RSM_MOUSE_SENSITIVITY;
	yoffset *= RSM_MOUSE_SENSITIVITY;

	yaw_ += xoffset;
	pitch_ += yoffset;

	/* Prevent weird angle swaps when cranking the neck too far */
	if (pitch_ > 89.0f) pitch_ = 89.0f;
	if (pitch_ < -89.0f) pitch_ = -89.0f;
}

void io_processMouseInput(GLFWwindow *window, i32 button, i32 action, i32 mods) {
	/* Handle mouse clicks */

	(void) window;
	(void) mods;

	uint64_t uid;
	switch (gamestate_) {
		case NORMAL:
			switch (button) {
				case RSM_BUTTON_LEFTCLICK: rsm_leftclick_ = action == GLFW_PRESS ? 1 : 0; break;
				case RSM_BUTTON_RIGHTCLICK: rsm_rightclick_ = action == GLFW_PRESS ? 1 : 0; break;
				case RSM_BUTTON_MIDDLECLICK: rsm_middleclick_ = action == GLFW_PRESS ? 1 : 0; break;
			}
			rsm_interact();
			break;

		case INVENTORY:
#define INV_BASE_X (screenWidth_/2-RSM_INVENTORY_SLOT_SIZE_PIXELS * ((f32) RSM_INVENTORY_SLOTS_HORIZONTAL/2))
#define INV_ICONS_Y (screenHeight_/2+RSM_INVENTORY_SLOT_SIZE_PIXELS * ((f32) RSM_INVENTORY_SLOTS_VERTICAL/2))
#define INV_SLOTS_Y (screenHeight_/2-RSM_INVENTORY_SLOT_SIZE_PIXELS * ((f32) RSM_INVENTORY_SLOTS_VERTICAL/2))
#define INV_SLOTS_WIDTH (RSM_INVENTORY_SLOT_SIZE_PIXELS * RSM_INVENTORY_SLOTS_HORIZONTAL)
			if (mouseX_ < INV_BASE_X || mouseX_ > INV_BASE_X + INV_SLOTS_WIDTH) return;
			if (mouseY_ < INV_SLOTS_Y || mouseY_ > INV_ICONS_Y + RSM_INVENTORY_ICON_SIZE_PIXELS) return;

			if (mouseY_ >= INV_ICONS_Y) { /* Change submenu */
				submenuIndex_ = (u64) (mouseX_ - INV_BASE_X) / RSM_INVENTORY_ICON_SIZE_PIXELS;
			} else { /* Change hotbar configuration */
				uid = SUBMENUS[submenuIndex_]
					[(u64) ((mouseX_ - INV_BASE_X) / RSM_INVENTORY_SLOT_SIZE_PIXELS)]
					[(u64) ((mouseY_ - INV_SLOTS_Y) / RSM_INVENTORY_SLOT_SIZE_PIXELS)];
				hotbar_[hotbarIndex_][hotslotIndex_][0] = GETID(uid);
				hotbar_[hotbarIndex_][hotslotIndex_][1] = GETVARIANT(uid);
			}
			gui_updateGUI();
			break;
#undef INV_BASE_X
#undef INV_ICONS_Y
#undef INV_SLOTS_Y
#undef INV_SLOTS_WIDTH

		case MENU: /*TODO*/
			break;

		case COMMAND:
			return;
	}
}

void io_processMouseScroll(GLFWwindow *window, f64 xoffset, f64 yoffset) {
	/* Handle mouse scroll */

	(void) window;
	(void) xoffset;

	hotslotIndex_ = (u64) ((i64) hotslotIndex_ + (yoffset < 0 ? 1 : -1) + RSM_HOTBAR_SLOTS) % RSM_HOTBAR_SLOTS;
	gui_updateGUI();
}
