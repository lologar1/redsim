#ifndef RENDERER_H
#define RENDERER_H

#include <stdio.h>
#include <math.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>
#include "renderutils.h"
#include "client.h"
#include "rsmlayout.h"
#include "gui.h"

extern u64 screenWidth_;
extern u64 screenHeight_;

void renderer_initShaders(void);
void renderer_render(GLFWwindow *window); /* Render scene with calls to the client */
void renderer_initBuffers(void);
void renderer_freeBuffers(void);

#endif
