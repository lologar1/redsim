#ifndef USERIO_H
#define USERIO_H

#include "redsim.h"
#include "gui.h"
#include "rsmlayout.h"
#include "renderer.h"
#include "command.h"
#include "client.h"

extern i32 rsm_forward_;
extern i32 rsm_backward_;
extern i32 rsm_right_;
extern i32 rsm_left_;
extern i32 rsm_down_;
extern i32 rsm_up_;
extern i32 rsm_leftclick_;
extern i32 rsm_rightclick_;
extern i32 rsm_middleclick_;
extern i32 rsm_forceplace_;
extern f64 mouseX_, mouseY_;

void io_processChar(GLFWwindow *window, u32 codepoint);
void io_processKey(GLFWwindow *window, i32 key, i32 scancode, i32 action, i32 mods);
void io_processMouseMovement(GLFWwindow *window, f64 xpos, f64 ypos);
void io_processMouseInput(GLFWwindow *window, i32 button, i32 action, i32 mods);
void io_processMouseScroll(GLFWwindow *window, f64 xoffset, f64 yoffset);

#endif
