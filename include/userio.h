#ifndef USERIO_H
#define USERIO_H

#include "redsim.h"
#include "gui.h"
#include "rsmlayout.h"
#include "renderer.h"
#include "command.h"

extern int32_t RSM_FORWARD;
extern int32_t RSM_BACKWARD;
extern int32_t RSM_RIGHT;
extern int32_t RSM_LEFT;
extern int32_t RSM_DOWN;
extern int32_t RSM_UP;
extern int32_t RSM_LEFTCLICK;
extern int32_t RSM_RIGHTCLICK;
extern int32_t RSM_MIDDLECLICK;
extern double mouseX, mouseY;

/* For mouse movements, lives in client */
extern float pitch, yaw;

void io_processChar(GLFWwindow *window, uint32_t codepoint);
void io_processKey(GLFWwindow *window, int32_t key, int32_t scancode, int32_t action, int32_t mods);
void io_processMouseMovement(GLFWwindow *window, double xpos, double ypos);
void io_processMouseInput(GLFWwindow *window, int32_t button, int32_t action, int32_t mods);
void io_processMouseScroll(GLFWwindow *window, double xoffset, double yoffset);

#endif
